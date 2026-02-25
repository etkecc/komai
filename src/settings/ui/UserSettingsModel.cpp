// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMessageBox>
#include <QMetaEnum>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <array>
#include <type_traits>

#include "Cache.h"
#include "JdenticonProvider.h"
#include "Logging.h"
#include "MainWindow.h"
#include "Utils.h"
#include "config/nheko.h"
#include "encryption/Olm.h"
#include "settings/SettingKeys.h"
#include "settings/core/StartupConfig.h"
#include "settings/ui/SettingDescriptor.h"
#include "settings/ui/SettingInputValidation.h"
#include "settings/ui/UserSettingsModel.h"
#include "settings/ui/facade/UserSettingsPage.h"

/**
 * UserSettingsModel is a UI adapter: it exposes settings metadata through roles,
 * groups by tab, and translates UI edits into `UserSettings` mutations.
 *
 * Storage/load semantics are implemented in `UserSettings` and `settings::*`
 * modules; this file intentionally contains list-model and delegate-facing
 * behavior only.
 */
#include "voip/CallDevices.h"

QHash<int, QByteArray>
UserSettingsModel::roleNames() const
{
    static QHash<int, QByteArray> roles{
      {Name, "name"},
      {Description, "description"},
      {Value, "value"},
      {Type, "type"},
      {ValueLowerBound, "valueLowerBound"},
      {ValueUpperBound, "valueUpperBound"},
      {ValueStep, "valueStep"},
      {Values, "values"},
      {Good, "good"},
      {Enabled, "enabled"},
      {ThemeVariantValue, "themeVariantValue"},
      {ThemeVariantValues, "themeVariantValues"},
      {SettingImage, "settingImage"},
      {Tab, "tab"},
    };

    return roles;
}

int
UserSettingsModel::rowCount(const QModelIndex &parent) const
{
    (void)parent;
    return settings::ui::settingsTableRowCount();
}

QObject *
UserSettingsModel::modelForTab(int tab) const
{
    auto it = filteredModels_.find(tab);
    if (it != filteredModels_.end())
        return it.value();

    auto *proxyModel = new QSortFilterProxyModel(const_cast<UserSettingsModel *>(this));
    proxyModel->setSourceModel(const_cast<UserSettingsModel *>(this));
    proxyModel->setFilterRole(Tab);
    proxyModel->setFilterRegularExpression(QStringLiteral("^%1$").arg(tab));
    filteredModels_.insert(tab, proxyModel);

    return proxyModel;
}

using settings::ui::rowForSettingId;
using settings::ui::settingsTable;
using settings::ui::settingsTableRowCount;

namespace {
bool
hasSettingId(const settings::ui::SettingMeta &meta, settings::core::SettingId id)
{
    return meta.settingId == id;
}
} // namespace

QVariant
UserSettingsModel::data(const QModelIndex &index, int role) const
{
    if (index.row() >= settingsTableRowCount())
        return {};

    auto i = UserSettings::instance();
    if (!i)
        return {};

    const auto &m = settingsTable[index.row()];

    switch (role) {
    case Name:
        return m.name ? tr(m.name) : QVariant{};
    case Description:
        if (!m.description)
            return QVariant{};

        if (hasSettingId(m, settings::core::SettingId::NetworkPresenceStatusPolicy)) {
            return tr(m.description)
              .arg(QStringLiteral("https://spec.matrix.org/v1.17/client-server-api/#presence"));
        }

        return tr(m.description);
    case Type:
        return m.type;
    case Tab:
        return m.tab;
    case Value:
        return m.getValue ? m.getValue() : QVariant{};
    case Enabled:
        return m.isEnabled ? m.isEnabled() : true;
    case ValueLowerBound:
        return m.lowerBound;
    case ValueUpperBound:
        return m.upperBound;
    case ValueStep:
        return m.step;
    case Values:
        return m.getValues ? m.getValues() : QVariant{};
    case Good:
    case ThemeVariantValue:
    case ThemeVariantValues:
        if (m.getRoleData) {
            const auto value = m.getRoleData(role);
            if (value.isValid())
                return value;
        }
        if (role == ThemeVariantValue)
            return -1;
        if (role == ThemeVariantValues)
            return QStringList{};
        return false;
    case SettingImage:
        return QString();
    }

    return {};
}

bool
UserSettingsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.row() >= settingsTableRowCount())
        return false;

    auto i = UserSettings::instance();
    if (!i)
        return false;

    const auto &m = settingsTable[index.row()];
    if (role == Value) {
        if (!m.setValue)
            return false;
        if (!validateSettingInput(m, value)) {
            nhlog::ui()->warn("Ignoring invalid settings input (setting_id={}, type={}, role={})",
                              static_cast<int>(m.settingId),
                              m.type,
                              role);
            return false;
        }
        return m.setValue(value);
    }

    if (m.setRoleData)
        return m.setRoleData(role, value);

    return false;
}

void
UserSettingsModel::importSessionKeys()
{
    const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString fileName   = QFileDialog::getOpenFileName(
      nullptr, tr("Open Sessions File"), homeFolder, QLatin1String(""));

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(nullptr, tr("Error"), file.errorString());
        return;
    }

    auto bin     = file.peek(file.size());
    auto payload = std::string(bin.data(), bin.size());

    bool ok;
    auto password = QInputDialog::getText(nullptr,
                                          tr("File Password"),
                                          tr("Enter the passphrase to decrypt the file:"),
                                          QLineEdit::Password,
                                          QLatin1String(""),
                                          &ok);
    if (!ok)
        return;

    if (password.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Error"), tr("The password cannot be empty"));
        return;
    }

    try {
        auto sessions = mtx::crypto::decrypt_exported_sessions(payload, password.toStdString());
        cache::importSessionKeys(std::move(sessions));
    } catch (const std::exception &e) {
        QMessageBox::warning(nullptr, tr("Error"), e.what());
    }
}
void
UserSettingsModel::exportSessionKeys()
{
    // Open password dialog.
    bool ok;
    auto password = QInputDialog::getText(nullptr,
                                          tr("File Password"),
                                          tr("Enter passphrase to encrypt your session keys:"),
                                          QLineEdit::Password,
                                          QLatin1String(""),
                                          &ok);
    if (!ok)
        return;

    if (password.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Error"), tr("The password cannot be empty"));
        return;
    }

    auto repeatedPassword = QInputDialog::getText(nullptr,
                                                  tr("Repeat File Password"),
                                                  tr("Repeat the passphrase:"),
                                                  QLineEdit::Password,
                                                  QLatin1String(""),
                                                  &ok);
    if (!ok)
        return;

    if (password != repeatedPassword) {
        QMessageBox::warning(nullptr, tr("Error"), tr("Passwords don't match"));
        return;
    }

    // Open file dialog to save the file.
    const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString fileName   = QFileDialog::getSaveFileName(
      nullptr, tr("File to save the exported session keys"), homeFolder);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(nullptr, tr("Error"), file.errorString());
        return;
    }

    // Export sessions & save to file.
    try {
        auto encrypted_blob = mtx::crypto::encrypt_exported_sessions(cache::exportSessionKeys(),
                                                                     password.toStdString());

        QString b64 = QString::fromStdString(mtx::crypto::bin2base64(encrypted_blob));

        QString prefix(QStringLiteral("-----BEGIN MEGOLM SESSION DATA-----"));
        QString suffix(QStringLiteral("-----END MEGOLM SESSION DATA-----"));
        QString newline(QStringLiteral("\n"));
        QTextStream out(&file);
        out << prefix << newline << b64 << newline << suffix << newline;
        file.close();
    } catch (const std::exception &e) {
        QMessageBox::warning(nullptr, tr("Error"), e.what());
    }
}
void
UserSettingsModel::requestCrossSigningSecrets()
{
    olm::request_cross_signing_keys();
}
void
UserSettingsModel::downloadCrossSigningSecrets()
{
    olm::download_cross_signing_keys();
}

UserSettingsModel::UserSettingsModel(QObject *p)
  : QAbstractListModel(p)
{
    auto s = UserSettings::instance();

#define CONNECT_SETTING_ID(id, sig, ...)                                                           \
    if (const int idx = rowForSettingId(settings::core::SettingId::id); idx >= 0) {                \
        connect(s.get(), &UserSettings::sig, this, [this, idx]() {                                 \
            emit dataChanged(index(idx), index(idx), {__VA_ARGS__});                               \
        });                                                                                        \
    }

#include "settings/ui/connections/UserSettingsModelConnectionsCalls.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsComposer.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsEncryption.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsIntegrations.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsLookFeel.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsNetwork.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsNotifications.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsPrivacy.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsTimeline.inc"

#undef CONNECT_SETTING_ID
}

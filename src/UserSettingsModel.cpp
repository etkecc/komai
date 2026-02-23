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
#include "MainWindow.h"
#include "UserSettingsPage.h"
#include "Utils.h"
#include "config/nheko.h"
#include "encryption/Olm.h"
#include "settings/SettingDescriptor.h"
#include "settings/SettingKeys.h"
#include "settings/core/StartupConfig.h"

/**
 * UserSettingsModel is a UI adapter: it exposes settings metadata through roles,
 * groups by tab, and translates UI edits into `UserSettings` mutations.
 *
 * Storage/load semantics are implemented in `UserSettings` and `settings::*`
 * modules; this file intentionally contains list-model and delegate-facing
 * behavior only.
 */
#include "ui/Theme.h"
#include "ui/ThemeRegistry.h"
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

using settings::ui::readSettingValue;
using settings::ui::sectionTitleForRow;
using settings::ui::settingsTable;
using settings::ui::settingsTableRowCount;

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
        if (m.type == UserSettingsModel::SectionTitle) {
            auto title = sectionTitleForRow(index.row());
            return title ? tr(title) : QVariant{};
        }
        return m.name ? tr(m.name) : QVariant{};
    case Description:
        if (!m.description)
            return QVariant{};

        if (index.row() == PresenceDefault) {
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

    // Special roles with only a few entries — keep as small switches
    case Good:
        switch (index.row()) {
        case OnlineBackupKey:
            return cache::secret(mtx::secret_storage::secrets::megolm_backup_v1).has_value();
        case SelfSigningKey:
            return cache::secret(mtx::secret_storage::secrets::cross_signing_self_signing)
              .has_value();
        case UserSigningKey:
            return cache::secret(mtx::secret_storage::secrets::cross_signing_user_signing)
              .has_value();
        case MasterKey:
            return true;
        }
        break;
    case ThemeVariantValue:
        if (index.row() == Theme) {
            auto variant = ThemeRegistry::instance().themeVariant(i->theme());
            if (variant == u"light")
                return 0;
            if (variant == u"dark")
                return 1;
            return 2;
        }
        return -1;
    case ThemeVariantValues:
        if (index.row() == Theme)
            return QStringList{
              QStringLiteral("Light"), QStringLiteral("Dark"), QStringLiteral("System")};
        return QStringList{};
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

    if (role == Value) {
        const auto &m = settingsTable[index.row()];
        return m.setValue ? m.setValue(value) : false;
    } else if (role == ThemeVariantValue) {
        if (index.row() == Theme) {
            int variantIdx = 0;
            if (!readSettingValue(value, variantIdx))
                return false;
            QString newVariant;
            if (variantIdx == 0)
                newVariant = QStringLiteral("light");
            else if (variantIdx == 1)
                newVariant = QStringLiteral("dark");
            else
                newVariant = QStringLiteral("system");
            auto currentVariant = ThemeRegistry::instance().themeVariant(i->theme());
            if (newVariant == currentVariant)
                return false;
            i->setTheme(ThemeRegistry::instance().defaultThemeSlug(newVariant));
            return true;
        }
        return false;
    }
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

#define CONNECT_SETTING(idx, sig, ...)                                                             \
    connect(s.get(), &UserSettings::sig, this, [this]() {                                          \
        emit dataChanged(index(idx), index(idx), {__VA_ARGS__});                                   \
    })

#include "UserSettingsModelConnectionsCalls.inc"
#include "UserSettingsModelConnectionsComposer.inc"
#include "UserSettingsModelConnectionsEncryption.inc"
#include "UserSettingsModelConnectionsIntegrations.inc"
#include "UserSettingsModelConnectionsLookFeel.inc"
#include "UserSettingsModelConnectionsNotifications.inc"
#include "UserSettingsModelConnectionsPrivacy.inc"
#include "UserSettingsModelConnectionsTimeline.inc"

#undef CONNECT_SETTING
}

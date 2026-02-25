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
using settings::ui::rowForSettingId;
using settings::ui::settingsTable;
using settings::ui::settingsTableRowCount;

namespace {
bool
hasSettingId(const settings::ui::SettingMeta &meta, settings::core::SettingId id)
{
    return meta.settingId == id;
}

bool
hasIntBound(const QVariant &bound, int &out)
{
    if (!bound.isValid())
        return false;
    return readSettingValue(bound, out);
}

bool
hasDoubleBound(const QVariant &bound, double &out)
{
    if (!bound.isValid())
        return false;
    return readSettingValue(bound, out);
}

bool
validateIntInput(const settings::ui::SettingMeta &meta, const QVariant &value)
{
    int rawValue = 0;
    if (!readSettingValue(value, rawValue))
        return false;

    int minValue = 0;
    if (hasIntBound(meta.lowerBound, minValue) && rawValue < minValue)
        return false;

    int maxValue = 0;
    if (hasIntBound(meta.upperBound, maxValue) && rawValue > maxValue)
        return false;

    return true;
}

bool
validateOptionInput(const settings::ui::SettingMeta &meta, const QVariant &value)
{
    int rawIndex = 0;
    if (!readSettingValue(value, rawIndex))
        return false;

    if (!validateIntInput(meta, value))
        return false;

    if (meta.getValues) {
        const auto optionValues = meta.getValues();
        if (optionValues.canConvert<QStringList>()) {
            const auto values = optionValues.toStringList();
            return rawIndex >= 0 && rawIndex < values.size();
        }
        if (optionValues.canConvert<QVariantList>()) {
            const auto values = optionValues.toList();
            return rawIndex >= 0 && rawIndex < values.size();
        }
    }

    return rawIndex >= 0;
}

bool
validateDoubleInput(const settings::ui::SettingMeta &meta, const QVariant &value)
{
    double rawValue = 0.0;
    if (!readSettingValue(value, rawValue))
        return false;

    double minValue = 0.0;
    if (hasDoubleBound(meta.lowerBound, minValue) && rawValue < minValue)
        return false;

    double maxValue = 0.0;
    if (hasDoubleBound(meta.upperBound, maxValue) && rawValue > maxValue)
        return false;

    return true;
}

bool
validateSettingInput(const settings::ui::SettingMeta &meta, const QVariant &value)
{
    switch (meta.type) {
    case UserSettingsModel::Toggle:
    case UserSettingsModel::ToggleWithDescription:
        return value.canConvert<bool>();
    case UserSettingsModel::Options:
    case UserSettingsModel::OptionsWithDescription:
    case UserSettingsModel::ThemeSelector:
        return validateOptionInput(meta, value);
    case UserSettingsModel::Integer:
    case UserSettingsModel::IntegerWithDescription:
        return validateIntInput(meta, value);
    case UserSettingsModel::Double:
        return validateDoubleInput(meta, value);
    case UserSettingsModel::TextInput:
        return value.canConvert<QString>();
    default:
        return true;
    }
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

    // Special roles with only a few entries — keep as small switches
    case Good:
        switch (m.settingId) {
        case settings::core::SettingId::EncryptionOnlineBackupKeyStatus:
            return cache::secret(mtx::secret_storage::secrets::megolm_backup_v1).has_value();
        case settings::core::SettingId::EncryptionSelfSigningKeyStatus:
            return cache::secret(mtx::secret_storage::secrets::cross_signing_self_signing)
              .has_value();
        case settings::core::SettingId::EncryptionUserSigningKeyStatus:
            return cache::secret(mtx::secret_storage::secrets::cross_signing_user_signing)
              .has_value();
        case settings::core::SettingId::EncryptionMasterSigningKeyStatus:
            return true;
        default:
            break;
        }
        break;
    case ThemeVariantValue:
        if (hasSettingId(m, settings::core::SettingId::UiThemeSlug)) {
            auto variant = ThemeRegistry::instance().themeVariant(i->theme());
            if (variant == u"light")
                return 0;
            if (variant == u"dark")
                return 1;
            return 2;
        }
        return -1;
    case ThemeVariantValues:
        if (hasSettingId(m, settings::core::SettingId::UiThemeSlug))
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
    if (!i)
        return false;

    if (role == Value) {
        const auto &m = settingsTable[index.row()];
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
    } else if (role == ThemeVariantValue) {
        const auto &m = settingsTable[index.row()];
        if (hasSettingId(m, settings::core::SettingId::UiThemeSlug)) {
            int variantIdx = 0;
            if (!readSettingValue(value, variantIdx))
                return false;
            if (variantIdx < 0 || variantIdx > 2)
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

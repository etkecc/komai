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
#include <array>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>

#include "Cache.h"
#include "JdenticonProvider.h"
#include "MainWindow.h"
#include "UserSettingsPage.h"
#include "Utils.h"
#include "config/nheko.h"
#include "encryption/Olm.h"
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

// ── Metadata table for settings model ──────────────────────────────────────────

namespace {

struct SectionDescriptor
{
    int row;
    int tab;
    const char *title;
};

constexpr auto sectionDescriptors = std::to_array<SectionDescriptor>({
  SectionDescriptor{UserSettingsModel::LookFeelThemeSection, UserSettingsModel::TabLookFeel,
                   QT_TR_NOOP("THEME")},
  SectionDescriptor{UserSettingsModel::LookFeelFontsSection, UserSettingsModel::TabLookFeel,
                   QT_TR_NOOP("FONTS")},
  SectionDescriptor{UserSettingsModel::LookFeelBehaviorSection, UserSettingsModel::TabLookFeel,
                   QT_TR_NOOP("BEHAVIOR")},
  SectionDescriptor{UserSettingsModel::LookFeelRoomListSection, UserSettingsModel::TabSidebars,
                   QT_TR_NOOP("ROOM LIST")},
  SectionDescriptor{UserSettingsModel::LookFeelCommunitiesSidebarSection,
                   UserSettingsModel::TabSidebars,
                   QT_TR_NOOP("COMMUNITIES SIDEBAR")},
  SectionDescriptor{UserSettingsModel::IntegrationsSystemTraySection, UserSettingsModel::TabIntegrations,
                   QT_TR_NOOP("SYSTEM TRAY")},
#ifdef NHEKO_DBUS_SYS
  SectionDescriptor{UserSettingsModel::IntegrationsDbusSection, UserSettingsModel::TabIntegrations,
                   QT_TR_NOOP("D-BUS")},
#endif
  SectionDescriptor{UserSettingsModel::IntegrationsBrowserSection, UserSettingsModel::TabIntegrations,
                   QT_TR_NOOP("BROWSER")},
  SectionDescriptor{UserSettingsModel::TimelineMessagesSection, UserSettingsModel::TabTimeline,
                   QT_TR_NOOP("MESSAGES")},
  SectionDescriptor{UserSettingsModel::TimelineMediaSection, UserSettingsModel::TabTimeline,
                   QT_TR_NOOP("MEDIA")},
  SectionDescriptor{UserSettingsModel::ComposerInputSection, UserSettingsModel::TabComposer,
                   QT_TR_NOOP("INPUT")},
  SectionDescriptor{UserSettingsModel::ComposerFeedbackSection, UserSettingsModel::TabComposer,
                   QT_TR_NOOP("FEEDBACK")},
  SectionDescriptor{UserSettingsModel::ComposerExtrasSection, UserSettingsModel::TabComposer,
                   QT_TR_NOOP("EXTRAS")},
  SectionDescriptor{UserSettingsModel::NotificationsDesktopSection, UserSettingsModel::TabNotifications,
                   QT_TR_NOOP("DESKTOP")},
  SectionDescriptor{UserSettingsModel::CallsGeneralSection, UserSettingsModel::TabCalls,
                   QT_TR_NOOP("GENERAL")},
  SectionDescriptor{UserSettingsModel::CallsDevicesSection, UserSettingsModel::TabCalls,
                   QT_TR_NOOP("DEVICES")},
  SectionDescriptor{UserSettingsModel::PrivacyScreenLockSection, UserSettingsModel::TabPrivacy,
                   QT_TR_NOOP("SCREEN LOCK")},
  SectionDescriptor{UserSettingsModel::PrivacyDataSection, UserSettingsModel::TabPrivacy,
                   QT_TR_NOOP("DATA & MAINTENANCE")},
  SectionDescriptor{UserSettingsModel::PrivacyUsersSection, UserSettingsModel::TabPrivacy,
                   QT_TR_NOOP("USERS")},
  SectionDescriptor{UserSettingsModel::EncryptionKeySharingSection, UserSettingsModel::TabEncryption,
                   QT_TR_NOOP("KEY SHARING")},
  SectionDescriptor{UserSettingsModel::EncryptionBackupSection, UserSettingsModel::TabEncryption,
                   QT_TR_NOOP("BACKUP")},
  SectionDescriptor{UserSettingsModel::EncryptionCrossSigningSection, UserSettingsModel::TabEncryption,
                   QT_TR_NOOP("CROSS-SIGNING")},
  SectionDescriptor{UserSettingsModel::SessionAccountSection, UserSettingsModel::TabSession,
                   QT_TR_NOOP("ACCOUNT")},
  SectionDescriptor{UserSettingsModel::SessionDeviceSection, UserSettingsModel::TabSession,
                   QT_TR_NOOP("DEVICE")},
  SectionDescriptor{UserSettingsModel::SessionActionsSection, UserSettingsModel::TabSession,
                   QT_TR_NOOP("ACTIONS")},
  SectionDescriptor{UserSettingsModel::AboutApplicationSection, UserSettingsModel::TabAbout,
                   QT_TR_NOOP("APPLICATION")},
});

const char *
sectionTitleForRow(int row)
{
    for (const auto &section : sectionDescriptors) {
        if (section.row == row)
            return section.title;
    }
    return {};
}

struct SettingMeta
{
    const char *name;                   // tr() key (nullptr = skip)
    const char *description;            // tr() key (nullptr = no description)
    int type;                           // Types enum
    int tab;                            // SettingsTab enum
    QVariant (*getValue)();             // getter (nullptr for sections)
    bool (*setValue)(const QVariant &); // setter (nullptr for read-only/sections)
    QVariant lowerBound, upperBound, step;
    QVariant (*getValues)(); // for Options type (nullptr if N/A)
    bool (*isEnabled)();     // nullptr = always enabled
};

#define I UserSettings::instance()
#define SM UserSettingsModel

template<typename T>
bool
readSettingValue(const QVariant &value, T &out)
{
    if (!value.canConvert<T>())
        return false;
    out = value.value<T>();
    return true;
}

template<typename Enum>
bool
readEnumSettingValue(const QVariant &value, Enum &out)
{
    int raw = 0;
    if (!readSettingValue(value, raw))
        return false;

    const auto meta = QMetaEnum::fromType<Enum>();
    if (raw < 0 || meta.keyCount() <= raw)
        return false;

    out = static_cast<Enum>(raw);
    return true;
}

// Helper: convert std::vector<std::string> to QStringList
static QStringList
vecToList(const std::vector<std::string> &vec)
{
    QStringList l;
    for (const auto &d : vec)
        l.push_back(QString::fromStdString(d));
    return l;
}

// clang-format off
static const SettingMeta settingsTable[] = {
    #include "UserSettingsModelLookFeel.inc"
    #include "UserSettingsModelTimeline.inc"
    #include "UserSettingsModelComposer.inc"
    #include "UserSettingsModelNotifications.inc"
    #include "UserSettingsModelCalls.inc"
    #include "UserSettingsModelPrivacy.inc"
    #include "UserSettingsModelEncryption.inc"
    #include "UserSettingsModelSession.inc"
    #include "UserSettingsModelAbout.inc"
};
// clang-format on

// Note: settingsTable must have exactly COUNT entries (one per Indices enum value before COUNT).
// The Indices enum puts ScaleFactor, IntegrationsDbusSection, and the D-Bus rows
// after COUNT when platform flags exclude them.
constexpr int settingsTableCount = sizeof(settingsTable) / sizeof(settingsTable[0]);
static_assert(settingsTableCount == UserSettingsModel::kSettingRowCount,
              "settingsTable size must match the number of visible settings indices");

#undef I
#undef SM

} // anonymous namespace

QVariant
UserSettingsModel::data(const QModelIndex &index, int role) const
{
    if (index.row() >= settingsTableCount)
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
        return m.description ? tr(m.description) : QVariant{};
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
    if (index.row() >= settingsTableCount)
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

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/SettingDescriptor.h"

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
#include <QStringList>
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
#include "settings/SettingKeys.h"
#include "settings/core/StartupConfig.h"
#include "ui/Theme.h"
#include "ui/ThemeRegistry.h"
#include "voip/CallDevices.h"

namespace settings::descriptor {

struct SectionDescriptor
{
    int row;
    int tab;
    const char *title;
};

constexpr auto sectionDescriptors = std::to_array<SectionDescriptor>({
  SectionDescriptor{UserSettingsModel::LookFeelThemeSection,
                    UserSettingsModel::TabLookFeel,
                    QT_TR_NOOP("Theme")},
  SectionDescriptor{UserSettingsModel::LookFeelFontsSection,
                    UserSettingsModel::TabLookFeel,
                    QT_TR_NOOP("Fonts")},
  SectionDescriptor{UserSettingsModel::LookFeelBehaviorSection,
                    UserSettingsModel::TabLookFeel,
                    QT_TR_NOOP("Behavior")},
  SectionDescriptor{UserSettingsModel::LookFeelRoomListSection,
                    UserSettingsModel::TabSidebars,
                    QT_TR_NOOP("Room list")},
  SectionDescriptor{UserSettingsModel::LookFeelCommunitiesSidebarSection,
                    UserSettingsModel::TabSidebars,
                    QT_TR_NOOP("Communities sidebar")},
  SectionDescriptor{UserSettingsModel::IntegrationsSystemTraySection,
                    UserSettingsModel::TabIntegrations,
                    QT_TR_NOOP("System tray")},
#ifdef KOMAI_DBUS_SYS
  SectionDescriptor{UserSettingsModel::IntegrationsDbusSection,
                    UserSettingsModel::TabIntegrations,
                    QT_TR_NOOP("D-Bus")},
#endif
  SectionDescriptor{UserSettingsModel::IntegrationsBrowserSection,
                    UserSettingsModel::TabIntegrations,
                    QT_TR_NOOP("Browser")},
  SectionDescriptor{UserSettingsModel::TimelineMessagesSection,
                    UserSettingsModel::TabTimeline,
                    QT_TR_NOOP("Messages")},
  SectionDescriptor{UserSettingsModel::TimelineMediaSection,
                    UserSettingsModel::TabTimeline,
                    QT_TR_NOOP("Media")},
  SectionDescriptor{UserSettingsModel::ComposerInputSection,
                    UserSettingsModel::TabComposer,
                    QT_TR_NOOP("Input")},
  SectionDescriptor{UserSettingsModel::ComposerFeedbackSection,
                    UserSettingsModel::TabComposer,
                    QT_TR_NOOP("Feedback")},
  SectionDescriptor{UserSettingsModel::ComposerExtrasSection,
                    UserSettingsModel::TabComposer,
                    QT_TR_NOOP("Extras")},
  SectionDescriptor{UserSettingsModel::NotificationsDesktopSection,
                    UserSettingsModel::TabNotifications,
                    QT_TR_NOOP("Desktop")},
  SectionDescriptor{UserSettingsModel::CallsGeneralSection,
                    UserSettingsModel::TabCalls,
                    QT_TR_NOOP("General")},
  SectionDescriptor{UserSettingsModel::CallsDevicesSection,
                    UserSettingsModel::TabCalls,
                    QT_TR_NOOP("Devices")},
  SectionDescriptor{UserSettingsModel::PrivacyPresenceSection,
                    UserSettingsModel::TabPrivacy,
                    QT_TR_NOOP("Presence")},
  SectionDescriptor{UserSettingsModel::PrivacyScreenLockSection,
                    UserSettingsModel::TabPrivacy,
                    QT_TR_NOOP("Screen lock")},
  SectionDescriptor{UserSettingsModel::PrivacyDataSection,
                    UserSettingsModel::TabPrivacy,
                    QT_TR_NOOP("Data & maintenance")},
  SectionDescriptor{UserSettingsModel::PrivacyUsersSection,
                    UserSettingsModel::TabPrivacy,
                    QT_TR_NOOP("Users")},
  SectionDescriptor{UserSettingsModel::EncryptionKeySharingSection,
                    UserSettingsModel::TabEncryption,
                    QT_TR_NOOP("Key sharing")},
  SectionDescriptor{UserSettingsModel::EncryptionBackupSection,
                    UserSettingsModel::TabEncryption,
                    QT_TR_NOOP("Backup")},
  SectionDescriptor{UserSettingsModel::EncryptionCrossSigningSection,
                    UserSettingsModel::TabEncryption,
                    QT_TR_NOOP("Cross-signing")},
  SectionDescriptor{UserSettingsModel::SessionAccountSection,
                    UserSettingsModel::TabSession,
                    QT_TR_NOOP("Account")},
  SectionDescriptor{UserSettingsModel::SessionDeviceSection,
                    UserSettingsModel::TabSession,
                    QT_TR_NOOP("Device")},
  SectionDescriptor{UserSettingsModel::SessionActionsSection,
                    UserSettingsModel::TabSession,
                    QT_TR_NOOP("Actions")},
  SectionDescriptor{UserSettingsModel::AboutApplicationSection,
                    UserSettingsModel::TabAbout,
                    QT_TR_NOOP("Application")},
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

#define I UserSettings::instance()
#define SM UserSettingsModel

template<auto Get>
QVariant
getSettingValue()
{
    auto i = I;
    if (!i)
        return {};
    return (i.get()->*Get)();
}

template<auto Get>
QVariant
getSettingEnumValue()
{
    auto i = I;
    if (!i)
        return {};
    return static_cast<int>((i.get()->*Get)());
}

template<auto Set, typename T>
using SetValueResult = decltype((std::declval<UserSettings *>()->*Set)(std::declval<T>()));

template<auto Set, typename T>
bool
setSettingValueImpl(UserSettings *settings, const T &castValue, std::true_type /*is_void*/)
{
    (settings->*Set)(castValue);
    return true;
}

template<auto Set, typename T>
bool
setSettingValueImpl(UserSettings *settings, const T &castValue, std::false_type /*is_void*/)
{
    return (settings->*Set)(castValue);
}

template<auto Set, typename T>
bool
setSettingValue(const QVariant &value)
{
    auto i = I;
    if (!i)
        return false;

    T castValue{};
    if (!readSettingValue(value, castValue))
        return false;

    return setSettingValueImpl<Set, T>(i.get(), castValue, std::is_void<SetValueResult<Set, T>>{});
}

template<auto Set, typename Enum>
using SetEnumResult = decltype((std::declval<UserSettings *>()->*Set)(std::declval<Enum>()));

template<auto Set, typename Enum>
bool
setSettingEnumValueImpl(UserSettings *settings, Enum enumValue, std::true_type /*is_void*/)
{
    (settings->*Set)(enumValue);
    return true;
}

template<auto Set, typename Enum>
bool
setSettingEnumValueImpl(UserSettings *settings, Enum enumValue, std::false_type /*is_void*/)
{
    return (settings->*Set)(enumValue);
}

template<auto Set, typename Enum>
bool
setSettingEnumValue(const QVariant &value)
{
    auto i = I;
    if (!i)
        return false;

    int rawValue = 0;
    if (!readSettingValue(value, rawValue))
        return false;

    const auto meta = QMetaEnum::fromType<Enum>();
    if (rawValue < 0 || rawValue >= meta.keyCount())
        return false;

    return setSettingEnumValueImpl<Set, Enum>(
      i.get(), static_cast<Enum>(rawValue), std::is_void<SetEnumResult<Set, Enum>>{});
}

#define SIMPLE_BOOL_SETTING(name, desc, tab, getter, setter, enabled_cb)                           \
    {QT_TR_NOOP(name),                                                                             \
     desc,                                                                                         \
     SM::Toggle,                                                                                   \
     tab,                                                                                          \
     getSettingValue<&UserSettings::getter>,                                                       \
     setSettingValue<&UserSettings::setter, bool>,                                                 \
     {},                                                                                           \
     {},                                                                                           \
     {},                                                                                           \
     nullptr,                                                                                      \
     enabled_cb}

#define SIMPLE_INVERTED_BOOL_SETTING(name, desc, tab, getter, setter, enabled_cb)                  \
    {QT_TR_NOOP(name),                                                                             \
     desc,                                                                                         \
     SM::Toggle,                                                                                   \
     tab,                                                                                          \
     []() -> QVariant { return !static_cast<bool>(I->getter()); },                                 \
     [](const QVariant &value) -> bool {                                                           \
         bool enabled{};                                                                           \
         if (!readSettingValue(value, enabled))                                                    \
             return false;                                                                         \
         I->setter(!enabled);                                                                      \
         return true;                                                                              \
     },                                                                                            \
     {},                                                                                           \
     {},                                                                                           \
     {},                                                                                           \
     nullptr,                                                                                      \
     enabled_cb}

#define SIMPLE_READ_ONLY_TEXT(name, desc, tab, getter)                                             \
    {QT_TR_NOOP(name),                                                                             \
     desc,                                                                                         \
     SM::ReadOnlyText,                                                                             \
     tab,                                                                                          \
     getSettingValue<&UserSettings::getter>,                                                       \
     nullptr,                                                                                      \
     {},                                                                                           \
     {},                                                                                           \
     {},                                                                                           \
     nullptr,                                                                                      \
     nullptr}

#define SIMPLE_DOUBLE_SETTING(name, desc, tab, getter, setter, min_v, max_v, step_v)               \
    {QT_TR_NOOP(name),                                                                             \
     desc,                                                                                         \
     SM::Double,                                                                                   \
     tab,                                                                                          \
     getSettingValue<&UserSettings::getter>,                                                       \
     setSettingValue<&UserSettings::setter, double>,                                               \
     min_v,                                                                                        \
     max_v,                                                                                        \
     step_v,                                                                                       \
     nullptr,                                                                                      \
     nullptr}

#define SIMPLE_OPTIONS_ENUM_SETTING(                                                               \
  name, desc, tab, getter, setter, enum_type, values_expr, enabled_cb)                             \
    {QT_TR_NOOP(name),                                                                             \
     desc,                                                                                         \
     SM::Options,                                                                                  \
     tab,                                                                                          \
     getSettingEnumValue<&UserSettings::getter>,                                                   \
     setSettingEnumValue<&UserSettings::setter, enum_type>,                                        \
     {},                                                                                           \
     {},                                                                                           \
     {},                                                                                           \
     values_expr,                                                                                  \
     enabled_cb}

#define SIMPLE_TEXT_SETTING(name, desc, tab, getter, setter, enabled_cb)                           \
    {QT_TR_NOOP(name),                                                                             \
     desc,                                                                                         \
     SM::TextInput,                                                                                \
     tab,                                                                                          \
     getSettingValue<&UserSettings::getter>,                                                       \
     setSettingValue<&UserSettings::setter, QString>,                                              \
     {},                                                                                           \
     {},                                                                                           \
     {},                                                                                           \
     nullptr,                                                                                      \
     enabled_cb}

#define SIMPLE_OPTIONS_INT_SETTING(                                                                \
  name, desc, tab, getter, setter, min_v, max_v, step_v, values_expr, enabled_cb)                  \
    {QT_TR_NOOP(name),                                                                             \
     desc,                                                                                         \
     SM::Integer,                                                                                  \
     tab,                                                                                          \
     getSettingValue<&UserSettings::getter>,                                                       \
     setSettingValue<&UserSettings::setter, int>,                                                  \
     min_v,                                                                                        \
     max_v,                                                                                        \
     step_v,                                                                                       \
     values_expr,                                                                                  \
     enabled_cb}

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
const SettingMeta settingsTable[] = {
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

static_assert(std::size(settingsTable) == UserSettingsModel::kSettingRowCount,
              "settingsTable size must match the number of visible settings indices");

#undef I
#undef SM

} // namespace settings::descriptor

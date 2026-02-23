// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/SettingDescriptor.h"

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
#include <iterator>
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

namespace settings::ui {

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

#define CORE_SETTING(scope, key, restart)                                                          \
    settings::core::SettingDefinition                                                              \
    {                                                                                              \
        settings::core::SettingId::Unknown, settings::core::SettingScope::scope, key, restart      \
    }

#define CORE_CONFIG(key) CORE_SETTING(Config, key, false)
#define CORE_CONFIG_RESTART(key) CORE_SETTING(Config, key, true)

#define SIMPLE_BOOL_SETTING_CORE(name, desc, tab, getter, setter, enabled_cb, core_def)            \
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
     enabled_cb,                                                                                   \
     core_def}

#define SIMPLE_BOOL_SETTING(name, desc, tab, getter, setter, enabled_cb)                           \
    SIMPLE_BOOL_SETTING_CORE(                                                                      \
      name, desc, tab, getter, setter, enabled_cb, settings::core::SettingDefinition{})

#define SIMPLE_BOOL_CONFIG_SETTING(name, desc, tab, getter, setter, enabled_cb, key)               \
    SIMPLE_BOOL_SETTING_CORE(name, desc, tab, getter, setter, enabled_cb, CORE_CONFIG(key))

#define SIMPLE_INVERTED_BOOL_SETTING_CORE(name, desc, tab, getter, setter, enabled_cb, core_def)   \
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
     enabled_cb,                                                                                   \
     core_def}

#define SIMPLE_INVERTED_BOOL_SETTING(name, desc, tab, getter, setter, enabled_cb)                  \
    SIMPLE_INVERTED_BOOL_SETTING_CORE(                                                             \
      name, desc, tab, getter, setter, enabled_cb, settings::core::SettingDefinition{})

#define SIMPLE_INVERTED_BOOL_CONFIG_SETTING(name, desc, tab, getter, setter, enabled_cb, key)      \
    SIMPLE_INVERTED_BOOL_SETTING_CORE(name, desc, tab, getter, setter, enabled_cb, CORE_CONFIG(key))

#define SIMPLE_READ_ONLY_TEXT(name, desc, tab, getter)                                             \
    {                                                                                              \
        QT_TR_NOOP(name), desc, SM::ReadOnlyText, tab, getSettingValue<&UserSettings::getter>,     \
          nullptr, {}, {}, {}, nullptr, nullptr, settings::core::SettingDefinition                 \
        {                                                                                          \
        }                                                                                          \
    }

#define SIMPLE_DOUBLE_SETTING_CORE(                                                                \
  name, desc, tab, getter, setter, min_v, max_v, step_v, core_def)                                 \
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
     nullptr,                                                                                      \
     core_def}

#define SIMPLE_DOUBLE_SETTING(name, desc, tab, getter, setter, min_v, max_v, step_v)               \
    SIMPLE_DOUBLE_SETTING_CORE(                                                                    \
      name, desc, tab, getter, setter, min_v, max_v, step_v, settings::core::SettingDefinition{})

#define SIMPLE_DOUBLE_CONFIG_SETTING(name, desc, tab, getter, setter, min_v, max_v, step_v, key)   \
    SIMPLE_DOUBLE_SETTING_CORE(                                                                    \
      name, desc, tab, getter, setter, min_v, max_v, step_v, CORE_CONFIG(key))

#define SIMPLE_DOUBLE_CONFIG_RESTART_SETTING(                                                      \
  name, desc, tab, getter, setter, min_v, max_v, step_v, key)                                      \
    SIMPLE_DOUBLE_SETTING_CORE(                                                                    \
      name, desc, tab, getter, setter, min_v, max_v, step_v, CORE_CONFIG_RESTART(key))

#define SIMPLE_OPTIONS_ENUM_SETTING_CORE(                                                          \
  name, desc, tab, getter, setter, enum_type, values_expr, enabled_cb, core_def)                   \
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
     enabled_cb,                                                                                   \
     core_def}

#define SIMPLE_OPTIONS_ENUM_SETTING(                                                               \
  name, desc, tab, getter, setter, enum_type, values_expr, enabled_cb)                             \
    SIMPLE_OPTIONS_ENUM_SETTING_CORE(name,                                                         \
                                     desc,                                                         \
                                     tab,                                                          \
                                     getter,                                                       \
                                     setter,                                                       \
                                     enum_type,                                                    \
                                     values_expr,                                                  \
                                     enabled_cb,                                                   \
                                     settings::core::SettingDefinition{})

#define SIMPLE_OPTIONS_ENUM_CONFIG_SETTING(                                                        \
  name, desc, tab, getter, setter, enum_type, values_expr, enabled_cb, key)                        \
    SIMPLE_OPTIONS_ENUM_SETTING_CORE(                                                              \
      name, desc, tab, getter, setter, enum_type, values_expr, enabled_cb, CORE_CONFIG(key))

#define SIMPLE_TEXT_SETTING_CORE(name, desc, tab, getter, setter, enabled_cb, core_def)            \
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
     enabled_cb,                                                                                   \
     core_def}

#define SIMPLE_TEXT_SETTING(name, desc, tab, getter, setter, enabled_cb)                           \
    SIMPLE_TEXT_SETTING_CORE(                                                                      \
      name, desc, tab, getter, setter, enabled_cb, settings::core::SettingDefinition{})

#define SIMPLE_TEXT_CONFIG_SETTING(name, desc, tab, getter, setter, enabled_cb, key)               \
    SIMPLE_TEXT_SETTING_CORE(name, desc, tab, getter, setter, enabled_cb, CORE_CONFIG(key))

#define SIMPLE_OPTIONS_INT_SETTING_CORE(                                                           \
  name, desc, tab, getter, setter, min_v, max_v, step_v, values_expr, enabled_cb, core_def)        \
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
     enabled_cb,                                                                                   \
     core_def}

#define SIMPLE_OPTIONS_INT_SETTING(                                                                \
  name, desc, tab, getter, setter, min_v, max_v, step_v, values_expr, enabled_cb)                  \
    SIMPLE_OPTIONS_INT_SETTING_CORE(name,                                                          \
                                    desc,                                                          \
                                    tab,                                                           \
                                    getter,                                                        \
                                    setter,                                                        \
                                    min_v,                                                         \
                                    max_v,                                                         \
                                    step_v,                                                        \
                                    values_expr,                                                   \
                                    enabled_cb,                                                    \
                                    settings::core::SettingDefinition{})

#define SIMPLE_OPTIONS_INT_CONFIG_SETTING(                                                         \
  name, desc, tab, getter, setter, min_v, max_v, step_v, values_expr, enabled_cb, key)             \
    SIMPLE_OPTIONS_INT_SETTING_CORE(name,                                                          \
                                    desc,                                                          \
                                    tab,                                                           \
                                    getter,                                                        \
                                    setter,                                                        \
                                    min_v,                                                         \
                                    max_v,                                                         \
                                    step_v,                                                        \
                                    values_expr,                                                   \
                                    enabled_cb,                                                    \
                                    CORE_CONFIG(key))

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
    #include "rows/UserSettingsModelLookFeel.inc"
    #include "rows/UserSettingsModelTimeline.inc"
    #include "rows/UserSettingsModelComposer.inc"
    #include "rows/UserSettingsModelNotifications.inc"
    #include "rows/UserSettingsModelCalls.inc"
    #include "rows/UserSettingsModelPrivacy.inc"
    #include "rows/UserSettingsModelEncryption.inc"
    #include "rows/UserSettingsModelSession.inc"
    #include "rows/UserSettingsModelAbout.inc"
};
// clang-format on

static_assert(std::size(settingsTable) == UserSettingsModel::COUNT,
              "settingsTable size must match the number of visible settings indices");

int
settingsTableRowCount()
{
    return static_cast<int>(std::size(settingsTable));
}

#undef I
#undef SM

} // namespace settings::ui

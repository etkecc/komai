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

#define CORE_SETTING_WITH_ID(id, scope, key, restart)                                              \
    settings::core::SettingDefinition                                                              \
    {                                                                                              \
        settings::core::SettingId::id, settings::core::SettingScope::scope, key, restart           \
    }

#define CORE_SETTING(scope, key, restart) CORE_SETTING_WITH_ID(Unknown, scope, key, restart)
#define CORE_CONFIG(key) CORE_SETTING(Config, key, false)
#define CORE_CONFIG_RESTART(key) CORE_SETTING(Config, key, true)
#define CORE_CONFIG_ID(id, key) CORE_SETTING_WITH_ID(id, Config, key, false)
#define CORE_RUNTIME_ID(id) CORE_SETTING_WITH_ID(id, Runtime, nullptr, false)

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

#define SIMPLE_BOOL_CONFIG_ID_SETTING(name, desc, tab, getter, setter, enabled_cb, id, key)        \
    SIMPLE_BOOL_SETTING_CORE(name, desc, tab, getter, setter, enabled_cb, CORE_CONFIG_ID(id, key))

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

#define SIMPLE_INVERTED_BOOL_CONFIG_ID_SETTING(                                                    \
  name, desc, tab, getter, setter, enabled_cb, id, key)                                            \
    SIMPLE_INVERTED_BOOL_SETTING_CORE(                                                             \
      name, desc, tab, getter, setter, enabled_cb, CORE_CONFIG_ID(id, key))

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

#define SIMPLE_DOUBLE_CONFIG_ID_SETTING(                                                           \
  name, desc, tab, getter, setter, min_v, max_v, step_v, id, key)                                  \
    SIMPLE_DOUBLE_SETTING_CORE(                                                                    \
      name, desc, tab, getter, setter, min_v, max_v, step_v, CORE_CONFIG_ID(id, key))

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

#define SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(                                                     \
  name, desc, tab, getter, setter, enum_type, values_expr, enabled_cb, id, key)                    \
    SIMPLE_OPTIONS_ENUM_SETTING_CORE(name,                                                         \
                                     desc,                                                         \
                                     tab,                                                          \
                                     getter,                                                       \
                                     setter,                                                       \
                                     enum_type,                                                    \
                                     values_expr,                                                  \
                                     enabled_cb,                                                   \
                                     CORE_CONFIG_ID(id, key))

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

#define SIMPLE_TEXT_CONFIG_ID_SETTING(name, desc, tab, getter, setter, enabled_cb, id, key)        \
    SIMPLE_TEXT_SETTING_CORE(name, desc, tab, getter, setter, enabled_cb, CORE_CONFIG_ID(id, key))

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

#define SIMPLE_OPTIONS_INT_CONFIG_ID_SETTING(                                                      \
  name, desc, tab, getter, setter, min_v, max_v, step_v, values_expr, enabled_cb, id, key)         \
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
                                    CORE_CONFIG_ID(id, key))

// Helper: convert std::vector<std::string> to QStringList
static QStringList
vecToList(const std::vector<std::string> &vec)
{
    QStringList l;
    for (const auto &d : vec)
        l.push_back(QString::fromStdString(d));
    return l;
}

static QVariant
lookFeelThemeValue()
{
    auto i = I;
    if (!i)
        return -1;

    auto variant = ThemeRegistry::instance().themeVariant(i->theme());
    if (variant == u"system")
        return -1;

    return ThemeRegistry::instance().themeSlugs(variant).indexOf(i->theme());
}

static bool
setLookFeelThemeValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    auto variant = ThemeRegistry::instance().themeVariant(i->theme());
    if (variant == u"system")
        return false;

    int idx = 0;
    if (!readSettingValue(v, idx))
        return false;

    auto slugs = ThemeRegistry::instance().themeSlugs(variant);
    if (idx < 0 || idx >= slugs.size())
        return false;

    i->setTheme(slugs.at(idx));
    return true;
}

static QVariant
lookFeelThemeValues()
{
    auto i = I;
    if (!i)
        return QStringList{};

    auto variant = ThemeRegistry::instance().themeVariant(i->theme());
    if (variant == u"system")
        return QStringList{};

    return ThemeRegistry::instance().themeNames(variant);
}

static QVariant
lookFeelFontFamilyValue()
{
    auto i = I;
    if (!i)
        return 0;

    if (i->font().isEmpty())
        return 0;

    auto fonts = QFontDatabase::families();
    fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System font"));
    return fonts.indexOf(i->font());
}

static bool
setLookFeelFontFamilyValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int idx = 0;
    if (!readSettingValue(v, idx))
        return false;

    auto fonts = QFontDatabase::families();
    if (idx < 0 || idx > fonts.size())
        return false;

    i->setFontFamily(idx == 0 ? QString{} : fonts.at(idx - 1));
    return true;
}

static QVariant
lookFeelFontFamilyValues()
{
    auto fonts = QFontDatabase::families();
    fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System font"));
    return fonts;
}

static QVariant
lookFeelEmojiFontFamilyValue()
{
    auto i = I;
    if (!i)
        return 0;

    if (i->emojiFontFamily().isEmpty())
        return 0;

    auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
    fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System emoji font"));
    return fonts.indexOf(i->emojiFontFamily());
}

static bool
setLookFeelEmojiFontFamilyValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int idx = 0;
    if (!readSettingValue(v, idx))
        return false;

    auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
    if (idx < 0 || idx > fonts.size())
        return false;

    i->setEmojiFontFamily(idx <= 0 ? QString{} : fonts.at(idx - 1));
    return true;
}

static QVariant
lookFeelEmojiFontFamilyValues()
{
    auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
    fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System emoji font"));
    return fonts;
}

static bool
identiconSettingEnabled()
{
    return JdenticonProvider::isAvailable();
}

static QVariant
integrationsDbusApiAccessValue()
{
    auto i = I;
    if (!i)
        return IntegrationsDbusAccessNone;
    return i->integrationsDbusApiAccess();
}

static bool
setIntegrationsDbusApiAccessValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int access = 0;
    if (!readSettingValue(v, access))
        return false;
    if (access < IntegrationsDbusAccessNone || access > IntegrationsDbusAccessReadWrite)
        return false;

    i->setIntegrationsDbusApiAccess(access);
    return true;
}

static QVariant
integrationsDbusApiAccessValues()
{
    return QStringList{
      QCoreApplication::translate("UserSettingsModel", "None"),
      QCoreApplication::translate("UserSettingsModel", "Read-only"),
      QCoreApplication::translate("UserSettingsModel", "Read & write"),
    };
}

static QVariant
privacyPresenceStatusPolicyValue()
{
    auto i = I;
    if (!i)
        return 3;

    switch (i->presence()) {
    case UserSettings::Presence::Online:
        return 0;
    case UserSettings::Presence::Unavailable:
        return 1;
    case UserSettings::Presence::Offline:
        return 2;
    case UserSettings::Presence::AutomaticPresence:
        return 3;
    }

    return 3;
}

static bool
setPrivacyPresenceStatusPolicyValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int option = 0;
    if (!readSettingValue(v, option))
        return false;

    switch (option) {
    case 0:
        i->setPresence(UserSettings::Presence::Online);
        return true;
    case 1:
        i->setPresence(UserSettings::Presence::Unavailable);
        return true;
    case 2:
        i->setPresence(UserSettings::Presence::Offline);
        return true;
    case 3:
        i->setPresence(UserSettings::Presence::AutomaticPresence);
        return true;
    default:
        return false;
    }
}

static QVariant
privacyPresenceStatusPolicyValues()
{
    return QStringList{
      QCoreApplication::translate("UserSettingsModel", "Always online"),
      QCoreApplication::translate("UserSettingsModel", "Always unavailable"),
      QCoreApplication::translate("UserSettingsModel", "Always offline"),
      QCoreApplication::translate("UserSettingsModel", "Automatic"),
    };
}

static bool
privacyScreenTimeoutEnabled()
{
    auto i = I;
    return i && i->privacyScreen();
}

static QStringList
callMicrophoneValuesList()
{
    auto i = I;
    if (!i)
        return {};
    return vecToList(CallDevices::instance().names(false, i->microphone().toStdString()));
}

static QVariant
callMicrophoneValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callMicrophoneValuesList().indexOf(i->microphone());
}

static bool
setCallMicrophoneValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int idx = 0;
    if (!readSettingValue(v, idx))
        return false;

    const auto list = callMicrophoneValuesList();
    if (idx < 0 || idx >= list.size())
        return false;

    i->setMicrophone(list.at(idx));
    return true;
}

static QVariant
callMicrophoneValues()
{
    return callMicrophoneValuesList();
}

static QStringList
callCameraValuesList()
{
    auto i = I;
    if (!i)
        return {};
    return vecToList(CallDevices::instance().names(true, i->camera().toStdString()));
}

static QVariant
callCameraValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callCameraValuesList().indexOf(i->camera());
}

static bool
setCallCameraValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int idx = 0;
    if (!readSettingValue(v, idx))
        return false;

    const auto list = callCameraValuesList();
    if (idx < 0 || idx >= list.size())
        return false;

    i->setCamera(list.at(idx));
    return true;
}

static QVariant
callCameraValues()
{
    return callCameraValuesList();
}

static QStringList
callCameraResolutionValuesList()
{
    auto i = I;
    if (!i)
        return {};
    return vecToList(CallDevices::instance().resolutions(i->camera().toStdString()));
}

static QVariant
callCameraResolutionValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callCameraResolutionValuesList().indexOf(i->cameraResolution());
}

static bool
setCallCameraResolutionValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int idx = 0;
    if (!readSettingValue(v, idx))
        return false;

    const auto list = callCameraResolutionValuesList();
    if (idx < 0 || idx >= list.size())
        return false;

    i->setCameraResolution(list.at(idx));
    return true;
}

static QVariant
callCameraResolutionValues()
{
    return callCameraResolutionValuesList();
}

static QStringList
callCameraFrameRateValuesList()
{
    auto i = I;
    if (!i)
        return {};

    return vecToList(CallDevices::instance().frameRates(i->camera().toStdString(),
                                                        i->cameraResolution().toStdString()));
}

static QVariant
callCameraFrameRateValue()
{
    auto i = I;
    if (!i)
        return -1;
    return callCameraFrameRateValuesList().indexOf(i->cameraFrameRate());
}

static bool
setCallCameraFrameRateValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int idx = 0;
    if (!readSettingValue(v, idx))
        return false;

    const auto list = callCameraFrameRateValuesList();
    if (idx < 0 || idx >= list.size())
        return false;

    i->setCameraFrameRate(list.at(idx));
    return true;
}

static QVariant
callCameraFrameRateValues()
{
    return callCameraFrameRateValuesList();
}

static QVariant
callRingtoneValue()
{
    auto i = I;
    if (!i)
        return 1;

    const auto v = i->ringtone();
    if (v == QStringView(u"Mute"))
        return 0;
    if (v == QStringView(u"Default"))
        return 1;
    if (v == QStringView(u"Other"))
        return 2;
    return 3;
}

static bool
setCallRingtoneValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int ringtone = 0;
    if (!readSettingValue(v, ringtone))
        return false;

    if (ringtone == 2) {
        const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        auto filepath            = QFileDialog::getOpenFileName(
          nullptr,
          QCoreApplication::translate("UserSettingsModel", "Select a file"),
          homeFolder,
          QCoreApplication::translate("UserSettingsModel", "All Files (*)"));
        if (!filepath.isEmpty()) {
            i->setRingtone(filepath);
            i->setRingtone(filepath);
        }
    } else if (ringtone == 0) {
        i->setRingtone(QStringLiteral("Mute"));
        i->setRingtone(QStringLiteral("Mute"));
    } else if (ringtone == 1) {
        i->setRingtone(QStringLiteral("Default"));
        i->setRingtone(QStringLiteral("Default"));
    }

    return true;
}

static QVariant
callRingtoneValues()
{
    QStringList list{
      QStringLiteral("Mute"),
      QStringLiteral("Default"),
      QStringLiteral("Other"),
    };

    auto i = I;
    if (i && !list.contains(i->ringtone()))
        list.push_back(i->ringtone());

    return list;
}

static QVariant
sidebarsLastMessagePreviewValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Only in unencrypted rooms")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static QVariant
sidebarsRoomSortValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Unread first, then recent")
           << QCoreApplication::translate("UserSettingsModel", "Unread first, then A-Z")
           << QCoreApplication::translate("UserSettingsModel", "Recent activity")
           << QCoreApplication::translate("UserSettingsModel", "Alphabetical");
    return values;
}

static QVariant
timelineSenderUsernameValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Only in large rooms (> 16 members)")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static QVariant
timelineShowImageValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Only in private rooms")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static QVariant
composerSendMessageKeyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Enter")
           << QCoreApplication::translate("UserSettingsModel", "Shift+Enter")
           << QCoreApplication::translate("UserSettingsModel", "Ctrl+Enter");
    return values;
}

static QVariant
composerAutoReplaceEmojiValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Only at the end of messages")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static QVariant
encryptionOnlineBackupKeyStatusValue()
{
    return cache::secret(mtx::secret_storage::secrets::megolm_backup_v1).has_value();
}

static QVariant
encryptionSelfSigningKeyStatusValue()
{
    return cache::secret(mtx::secret_storage::secrets::cross_signing_self_signing).has_value();
}

static QVariant
encryptionUserSigningKeyStatusValue()
{
    return cache::secret(mtx::secret_storage::secrets::cross_signing_user_signing).has_value();
}

static QVariant
encryptionMasterSigningKeyStatusValue()
{
    return cache::secret(mtx::secret_storage::secrets::cross_signing_master).has_value();
}

static QVariant
sessionProfileValue()
{
    auto i = I;
    if (!i)
        return QCoreApplication::translate("UserSettingsModel", "Default");

    return i->profile().isEmpty() ? QCoreApplication::translate("UserSettingsModel", "Default")
                                  : i->profile();
}

static QVariant
sessionDeviceFingerprintValue()
{
    auto fingerprint = utils::humanReadableFingerprint(olm::client()->identity_keys().ed25519);
    fingerprint.replace(u'\n', u' ');
    return fingerprint.simplified();
}

static QVariant
aboutAppNameValue()
{
    return QStringLiteral("<a href=\"https://github.com/etkecc/komai\">Komai</a> @ ") +
           QString::fromStdString(nheko::version) +
           QStringLiteral(" (<a href=\"https://github.com/etkecc/komai/commit/") +
           QString::fromStdString(nheko::commit_hash) + QStringLiteral("\">") +
           QString::fromStdString(nheko::commit_hash) + QStringLiteral("</a>)");
}

static QVariant
aboutPlatformValue()
{
    return QString::fromStdString(nheko::build_os);
}

static QVariant
aboutBasedOnValue()
{
    return QStringLiteral("<a href=\"https://nheko.im/nheko-reborn/nheko\">nheko</a> @ ~v0.12.1 "
                          "(<a href=\"https://nheko.im/nheko-reborn/nheko/-/commit/"
                          "abb2325a995f936081219f402339fc8e0a661ac1\">abb2325a</a>)");
}

static QVariant
aboutMaintainedByValue()
{
    return QStringLiteral("<a href=\"https://etke.cc\">etke.cc</a>");
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

int
settingsTableRowCount()
{
    return static_cast<int>(std::size(settingsTable));
}

int
rowForSettingId(settings::core::SettingId id)
{
    for (int i = 0; i < settingsTableRowCount(); ++i) {
        if (settingsTable[i].core.id == id)
            return i;
    }
    return -1;
}

#undef I
#undef SM

} // namespace settings::ui

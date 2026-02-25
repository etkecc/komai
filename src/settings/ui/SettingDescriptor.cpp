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
#include <QHash>
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
#include "Utils.h"
#include "config/nheko.h"
#include "encryption/Olm.h"
#include "settings/SettingKeys.h"
#include "settings/core/StartupConfig.h"
#include "settings/ui/facade/UserSettingsPage.h"
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

template<settings::core::SettingId Id, auto Get>
QVariant
getCoreBoolValue()
{
    auto i = I;
    if (!i)
        return {};

    if (const auto value = i->coreStore().valueAs<bool>(Id); value.has_value())
        return *value;

    return (i.get()->*Get)();
}

template<settings::core::SettingId Id, auto Get>
QVariant
getCoreIntValue()
{
    auto i = I;
    if (!i)
        return {};

    if (const auto value = i->coreStore().valueAs<int>(Id); value.has_value())
        return *value;

    return (i.get()->*Get)();
}

template<settings::core::SettingId Id, auto Get>
QVariant
getCoreDoubleValue()
{
    auto i = I;
    if (!i)
        return {};

    if (const auto value = i->coreStore().valueAs<double>(Id); value.has_value())
        return *value;

    return (i.get()->*Get)();
}

template<settings::core::SettingId Id, auto Get>
QVariant
getCoreStringValue()
{
    auto i = I;
    if (!i)
        return {};

    if (const auto value = i->coreStore().valueAs<std::string>(Id); value.has_value())
        return QString::fromStdString(*value);

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

template<settings::core::SettingId Id, auto Get>
QVariant
getCoreEnumValue()
{
    auto i = I;
    if (!i)
        return {};

    if (const auto value = i->coreStore().valueAs<int>(Id); value.has_value())
        return *value;

    return static_cast<int>((i.get()->*Get)());
}

template<typename Enum>
int
enumMaxValue()
{
    const auto meta = QMetaEnum::fromType<Enum>();
    return meta.isValid() ? (meta.keyCount() - 1) : -1;
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

#define CORE_SETTING_WITH_ID(id, scope, key, restart) settings::core::SettingId::id

#define CORE_CONFIG_RESTART(key) CORE_SETTING_WITH_ID(Unknown, Config, key, true)
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
      name, desc, tab, getter, setter, enabled_cb, settings::core::SettingId::Unknown)

#define SIMPLE_BOOL_CONFIG_ID_SETTING(name, desc, tab, getter, setter, enabled_cb, id, key)        \
    {QT_TR_NOOP(name),                                                                             \
     desc,                                                                                         \
     SM::Toggle,                                                                                   \
     tab,                                                                                          \
     getCoreBoolValue<settings::core::SettingId::id, &UserSettings::getter>,                       \
     setSettingValue<&UserSettings::setter, bool>,                                                 \
     {},                                                                                           \
     {},                                                                                           \
     {},                                                                                           \
     nullptr,                                                                                      \
     enabled_cb,                                                                                   \
     CORE_CONFIG_ID(id, key)}

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
     nullptr,                                                                                      \
     settings::core::SettingId::Unknown}

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
      name, desc, tab, getter, setter, min_v, max_v, step_v, settings::core::SettingId::Unknown)

#define SIMPLE_DOUBLE_CONFIG_ID_SETTING(                                                           \
  name, desc, tab, getter, setter, min_v, max_v, step_v, id, key)                                  \
    {QT_TR_NOOP(name),                                                                             \
     desc,                                                                                         \
     SM::Double,                                                                                   \
     tab,                                                                                          \
     getCoreDoubleValue<settings::core::SettingId::id, &UserSettings::getter>,                     \
     setSettingValue<&UserSettings::setter, double>,                                               \
     min_v,                                                                                        \
     max_v,                                                                                        \
     step_v,                                                                                       \
     nullptr,                                                                                      \
     nullptr,                                                                                      \
     CORE_CONFIG_ID(id, key)}

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
     0,                                                                                            \
     enumMaxValue<enum_type>(),                                                                    \
     1,                                                                                            \
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
                                     settings::core::SettingId::Unknown)

#define SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(                                                     \
  name, desc, tab, getter, setter, enum_type, values_expr, enabled_cb, id, key)                    \
    {QT_TR_NOOP(name),                                                                             \
     desc,                                                                                         \
     SM::Options,                                                                                  \
     tab,                                                                                          \
     getCoreEnumValue<settings::core::SettingId::id, &UserSettings::getter>,                       \
     setSettingEnumValue<&UserSettings::setter, enum_type>,                                        \
     0,                                                                                            \
     enumMaxValue<enum_type>(),                                                                    \
     1,                                                                                            \
     values_expr,                                                                                  \
     enabled_cb,                                                                                   \
     CORE_CONFIG_ID(id, key)}

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
      name, desc, tab, getter, setter, enabled_cb, settings::core::SettingId::Unknown)

#define SIMPLE_TEXT_CONFIG_ID_SETTING(name, desc, tab, getter, setter, enabled_cb, id, key)        \
    {QT_TR_NOOP(name),                                                                             \
     desc,                                                                                         \
     SM::TextInput,                                                                                \
     tab,                                                                                          \
     getCoreStringValue<settings::core::SettingId::id, &UserSettings::getter>,                     \
     setSettingValue<&UserSettings::setter, QString>,                                              \
     {},                                                                                           \
     {},                                                                                           \
     {},                                                                                           \
     nullptr,                                                                                      \
     enabled_cb,                                                                                   \
     CORE_CONFIG_ID(id, key)}

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
                                    settings::core::SettingId::Unknown)

#define SIMPLE_OPTIONS_INT_CONFIG_ID_SETTING(                                                      \
  name, desc, tab, getter, setter, min_v, max_v, step_v, values_expr, enabled_cb, id, key)         \
    {QT_TR_NOOP(name),                                                                             \
     desc,                                                                                         \
     SM::Integer,                                                                                  \
     tab,                                                                                          \
     getCoreIntValue<settings::core::SettingId::id, &UserSettings::getter>,                        \
     setSettingValue<&UserSettings::setter, int>,                                                  \
     min_v,                                                                                        \
     max_v,                                                                                        \
     step_v,                                                                                       \
     values_expr,                                                                                  \
     enabled_cb,                                                                                   \
     CORE_CONFIG_ID(id, key)}

#include "settings/ui/SettingDescriptorCallbacks.inc"

// clang-format off
const SettingMeta settingsTable[] = {
    #include "rows/UserSettingsModelLookFeel.inc"
    #include "rows/UserSettingsModelTimeline.inc"
    #include "rows/UserSettingsModelComposer.inc"
    #include "rows/UserSettingsModelNotifications.inc"
    #include "rows/UserSettingsModelCalls.inc"
    #include "rows/UserSettingsModelNetwork.inc"
    #include "rows/UserSettingsModelPrivacy.inc"
    #include "rows/UserSettingsModelEncryption.inc"
    #include "rows/UserSettingsModelAccount.inc"
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
    if (id == settings::core::SettingId::Unknown)
        return -1;

    static const auto lookup = [] {
        QHash<int, int> idToRow;
        idToRow.reserve(settingsTableRowCount());

        for (int i = 0; i < settingsTableRowCount(); ++i) {
            const auto settingId = settingsTable[i].settingId;
            if (settingId == settings::core::SettingId::Unknown)
                continue;

            const int key = static_cast<int>(settingId);
            Q_ASSERT_X(!idToRow.contains(key),
                       "settings::ui::rowForSettingId",
                       "Duplicate non-Unknown SettingId found in settingsTable.");

            if (!idToRow.contains(key))
                idToRow.insert(key, i);
        }

        return idToRow;
    }();

    return lookup.value(static_cast<int>(id), -1);
}

#undef I
#undef SM

} // namespace settings::ui

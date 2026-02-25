// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Keep macro argument shape compatible with existing row includes while we only
// consume SettingId here. Scope/key/restart metadata comes from core definitions.
#define CORE_SETTING_WITH_ID(id, scope, key, restart) settings::core::SettingId::id

#define CORE_CONFIG_RESTART(key) CORE_SETTING_WITH_ID(Unknown, Config, key, true)
#define CORE_CONFIG_ID(id, key) CORE_SETTING_WITH_ID(id, Config, key, false)
#define CORE_CONFIG_RESTART_ID(id, key) CORE_SETTING_WITH_ID(id, Config, key, true)
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

#define SIMPLE_DOUBLE_CONFIG_RESTART_ID_SETTING(                                                   \
  name, desc, tab, getter, setter, min_v, max_v, step_v, id, key)                                  \
    SIMPLE_DOUBLE_SETTING_CORE(                                                                    \
      name, desc, tab, getter, setter, min_v, max_v, step_v, CORE_CONFIG_RESTART_ID(id, key))

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

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

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

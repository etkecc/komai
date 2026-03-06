// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

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

#define SIMPLE_BOOL_CONFIG_ID_SETTING_WITH_ICON(                                                    \
  name, desc, tab, getter, setter, enabled_cb, id, key, icon_path)                                 \
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
     CORE_CONFIG_ID(id, key),                                                                      \
     icon_path}

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

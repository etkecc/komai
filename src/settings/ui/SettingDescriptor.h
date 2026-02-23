// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QVariant>

#include "settings/core/SettingDefinition.h"

namespace settings::ui {

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
    settings::core::SettingDefinition core{};
};

template<typename T>
bool
readSettingValue(const QVariant &value, T &out)
{
    if (!value.canConvert<T>())
        return false;
    out = value.value<T>();
    return true;
}

extern const SettingMeta settingsTable[];

int
settingsTableRowCount();

int
rowForSettingId(settings::core::SettingId id);

} // namespace settings::ui

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QVariant>

#include "UserSettingsPage.h"

namespace settings::descriptor {

enum class SettingScope
{
    Runtime,
    Config,
    State,
    Session,
    Secrets,
};

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
    SettingScope scope       = SettingScope::Runtime;
    const char *persistedKey = nullptr;
    bool requiresRestart     = false;
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

const char *
sectionTitleForRow(int row);

extern const SettingMeta settingsTable[UserSettingsModel::kSettingRowCount];

} // namespace settings::descriptor

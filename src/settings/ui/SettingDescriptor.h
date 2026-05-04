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
    settings::core::SettingId settingId{settings::core::SettingId::Unknown};
    const char *icon    = nullptr; // optional qrc icon path for label rendering
    const char *tagId   = nullptr; // community filter tag ID (e.g. "people", "tag:m.favourite")
    bool syncedToMatrix = false;   // true when this setting is stored on the Matrix homeserver
    // Optional extra search terms (synonyms, alternate spellings) — comma-separated.
    // Searched in both source-English and tr() forms, so wrap with QT_TRANSLATE_NOOP
    // to make translators aware. Used to surface settings whose label/description
    // doesn't contain the obvious term users type.
    const char *searchKeywords = nullptr;
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

void
validateSettingsTable();

int
rowForSettingId(settings::core::SettingId id);

} // namespace settings::ui

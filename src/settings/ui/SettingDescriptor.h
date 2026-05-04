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

// Attaches `searchKeywords` to a row that's been built by one of the macro
// helpers (SIMPLE_BOOL_CONFIG_ID_SETTING etc.). The macros only fill the
// first 12 positional fields of SettingMeta, leaving the trailing
// icon/tagId/syncedToMatrix/searchKeywords at their defaults. Wrapping the
// macro call with `withKeywords(..., "...")` overrides searchKeywords
// without requiring every macro to grow a keywords parameter.
//
// Used at static-init time to construct settingsTable[]; not constexpr
// because SettingMeta carries QVariant members (lowerBound, upperBound,
// step) whose copy ctors are not constexpr.
inline SettingMeta
withKeywords(SettingMeta m, const char *keywords)
{
    m.searchKeywords = keywords;
    return m;
}

extern const SettingMeta settingsTable[];

int
settingsTableRowCount();

void
validateSettingsTable();

int
rowForSettingId(settings::core::SettingId id);

// For enum-style settings whose `getValues()` helper has a static set of
// source-English options (Density's "Spacious"/"Compact"/"Dense", etc.),
// returns a null-terminated array of those source strings. Used by the
// settings search proxy to support cross-locale enum option matching: a
// user typing "compact" in any UI language can still find Density.
//
// Returns nullptr for dynamic helpers (font lists, languages, audio
// devices, themes) where source-English doesn't carry meaning.
const char *const *valuesEnglishFor(QVariant (*helper)());

// Source-English keywords for tabs whose content is partly or wholly
// custom QML (Account, Application Profiles, Integrations Transcription
// + Browser, Timeline state events). Lets the search proxy report a
// tab as a match for terms the model rows don't carry, and lets the
// custom QML hide non-matching sections.
//
// Keywords are grouped per section so each section can gate its own
// visibility. `sectionId` is the stable string the QML matches on
// (e.g. "profile", "thisDevice", "transcription"). The returned array
// is null-terminated by `{nullptr, nullptr}`. Returns nullptr for tabs
// that are pure-model (LookFeel, Navigation, etc.) — for those, the
// SettingMeta rows themselves cover search.
struct TabSearchSection
{
    const char *sectionId;
    const char *const *keywordsEnglish;
};

const TabSearchSection *
customSectionsForTab(int tab);

} // namespace settings::ui

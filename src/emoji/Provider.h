// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <cstddef>
#include <vector>

#include "Emoji.h"

namespace emoji {
class Provider
{
public:
    struct Query
    {
        QString keyword;
        QString preferredSkinToneClass;
        QString preferredGender;
        bool includeSkinToneVariants = true;
        bool applyKeywordMatch       = true;
    };

    struct QueryData
    {
        QString searchText;
        QString skinToneClass;
        QString genderClass;
        QString baseId;
        bool hasSkinToneVariants = false;
    };

    // Returns the lazily loaded emoji catalog from embedded runtime JSON resources.
    static const std::vector<Emoji> &emoji();

    // Returns query-relevant metadata for emoji entry at index. Empty/default data when out of
    // bounds.
    static const QueryData &queryData(std::size_t index);

    // Returns whether emoji entry at index matches query instructions. This is designed to be
    // extensible for future user preferences (for example skin tone and gender preferences).
    static bool matchesQuery(std::size_t index, const Query &query);

    // Runtime user preference values consumed by query filtering.
    static void setPreferredSkinToneClass(const QString &preferredSkinToneClass);
    static void setPreferredGender(const QString &preferredGender);
    static QString preferredSkinToneClass();
    static QString preferredGender();

    // Backward-compatible helper for existing callers. Prefer queryData(index).searchText.
    static QString searchText(std::size_t index);
};
} // namespace emoji

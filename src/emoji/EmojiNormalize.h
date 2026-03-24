// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <string>

namespace emoji {

/// Strips Unicode variation selectors (U+FE0F and U+FE0E) from an emoji string.
///
/// This normalizes emojis for deduplication: for example, 👍️ (U+1F44D U+FE0F)
/// and 👍 (U+1F44D) are visually identical and should be treated as the same
/// reaction. Different Matrix clients disagree on whether to include the
/// variation selector, so reactions from room history may use either form.
///
/// Only presentation-style variation selectors are removed. Skin-tone modifiers
/// (U+1F3FB–U+1F3FF), ZWJ sequences, and all other combining characters are
/// preserved — those produce visually distinct emojis.
QString
normalizeForComparison(const QString &emoji);
std::string
normalizeForComparison(const std::string &utf8Emoji);

} // namespace emoji

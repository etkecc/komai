// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include "settings/ui/facade/UserSettingsPage.h"

namespace emoji {

/// Replace common text emoticons (:) :D ;) etc.) with their Unicode emoji equivalents.
///
/// The replacement mode controls behaviour:
///   - Always:    replace all boundary-safe emoticons in the entire string
///   - OnlyAtEnd: replace a single emoticon at the very end of the string
///   - Never:     return the input unchanged
QString
replaceEmoticons(const QString &input, UserSettings::AutoReplaceEmoji mode);

/// Returns true when `input` is exactly one of the known emoticon shortcuts
/// (e.g. `:)`, `:D`, `:'(`). Compared case-insensitively to match
/// replaceEmoticons. Used by the composer to skip the inline emoji picker when
/// the user has typed a complete emoticon that auto-conversion will replace.
bool
isEmoticonShortcut(const QString &input);

} // namespace emoji

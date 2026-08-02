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

/// Returns the emoji for `input` if it is exactly one of the known text
/// emoticon shortcuts (case-insensitive), or an empty string otherwise.
/// Unlike replaceEmoticons(), this does a single whole-string lookup with
/// no scanning or boundary logic. Used by isEmoticonShortcut()'s exact-match
/// check; for live-replacement use replaceLeadingEmoticon() instead, which
/// tolerates trailing punctuation.
QString
emoticonForShortcut(const QString &input);

/// If `input` begins with one of the known emoticon shortcuts (case-
/// insensitive), returns `input` with that leading shortcut replaced by its
/// emoji, preserving anything after it unchanged (e.g. `:)?` -> `🙂?`, a
/// bare `:)` -> `🙂`). The match is rejected -- returns an empty string --
/// if the character immediately following the shortcut is a letter or
/// digit, since that means the shortcut is actually a prefix of a longer
/// word (e.g. `:Dog` must not become `😀og`). Returns an empty string when
/// no known shortcut prefixes `input` at all. Longest patterns are tried
/// first so `</3` isn't shadowed by `<3`. Used by the composer to convert a
/// completed shortcut to its emoji right after the user types the space
/// that follows it, even when punctuation sits between the shortcut and
/// that space.
QString
replaceLeadingEmoticon(const QString &input);

/// Returns true when `input` is exactly one of the known emoticon shortcuts
/// (e.g. `:)`, `:D`, `:'(`). Compared case-insensitively to match
/// replaceEmoticons. Used by the composer to skip the inline emoji picker when
/// the user has typed a complete emoticon that auto-conversion will replace.
bool
isEmoticonShortcut(const QString &input);

} // namespace emoji

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

} // namespace emoji

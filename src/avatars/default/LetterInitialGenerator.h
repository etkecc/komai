// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace avatars {

/// Generates an SVG with a single letter on a colored background.
/// @param key       userid or roomid (fallback when displayName is empty)
/// @param displayName  display name to extract initial from
/// @param color     hex color without '#' — used for text color
/// @return SVG markup string
QString
generateLetterInitial(const QString &key, const QString &displayName, const QString &color);

} // namespace avatars

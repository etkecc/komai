// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFont>
#include <QPalette>
#include <QString>

namespace timeline::litehtml {

/// Scale factor for emoji glyphs relative to the surrounding text.
/// Used in the CSS stylesheet (font-size) and in the font metrics override.
constexpr double emojiScaleFactor = 1.4;

QString
generateMasterStylesheet(const QPalette &palette,
                         const QFont &font,
                         bool compact,
                         const QString &errorColor,
                         const QString &attentionColor,
                         const QString &successColor,
                         const QString &searchHighlightBgColor,
                         const QString &searchHighlightTextColor,
                         const QString &codeBackgroundColor);

} // namespace timeline::litehtml

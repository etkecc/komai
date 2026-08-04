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

/// The surface `code` and `pre` are painted on, as `#rrggbb`.
///
/// Syntax highlighting bakes token colors into the message HTML and picks them
/// for legibility against this exact color, so producers of that HTML and the
/// stylesheet that paints it have to agree on one answer.
QString
codeBackgroundColor(const QPalette &palette);

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

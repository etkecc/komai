// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LetterInitialGenerator.h"

#include <QColor>

namespace avatars {

/// Extract a single display character from either the displayName or the key.
static QString
extractInitial(const QString &key, const QString &displayName)
{
    // Prefer displayName if available
    const QString &source = displayName.isEmpty() ? key : displayName;
    if (source.isEmpty())
        return QStringLiteral("#");

    // For keys: strip leading sigil (@, #, !)
    int start = 0;
    if (&source == &key) {
        const QChar first = source.at(0);
        if (first == u'@' || first == u'#') {
            start = 1;
        } else if (first == u'!') {
            // Room IDs are opaque — use generic fallback
            return QStringLiteral("#");
        }
    }

    if (start >= source.length())
        return QStringLiteral("#");

    // Take the first codepoint (handles surrogate pairs)
    if (source.at(start).isHighSurrogate() && start + 1 < source.length() &&
        source.at(start + 1).isLowSurrogate()) {
        return source.mid(start, 2);
    }
    return source.mid(start, 1);
}

/// Derive a background color from the text color: desaturate and lighten significantly.
static QString
deriveBackground(const QString &hexColor)
{
    QColor c(u'#' + hexColor);
    if (!c.isValid())
        c = QColor(128, 128, 128);

    // Create a light tint: reduce saturation, increase lightness
    float h, s, l, a;
    c.getHslF(&h, &s, &l, &a);
    s = s * 0.3f;
    l = 0.85f + (1.0f - 0.85f) * (1.0f - s); // very light
    c.setHslF(h, s, qBound(0.0f, l, 1.0f), a);
    return c.name(); // returns "#rrggbb"
}

QString
generateLetterInitial(const QString &key, const QString &displayName, const QString &color)
{
    constexpr int SIZE = 80;

    const QString initial    = extractInitial(key, displayName);
    const QString textColor  = u'#' + color;
    const QString background = deriveBackground(color);

    // XML-escape the initial in case it contains '&' or '<' (unlikely but safe)
    QString escapedInitial = initial;
    escapedInitial.replace(u'&', QStringLiteral("&amp;"));
    escapedInitial.replace(u'<', QStringLiteral("&lt;"));

    return QStringLiteral(
             R"(<svg viewBox="0 0 %1 %1" xmlns="http://www.w3.org/2000/svg">)"
             R"(<rect width="%1" height="%1" fill="%2"/>)"
             R"(<text x="%3" y="%3" text-anchor="middle" dominant-baseline="central" )"
             R"(font-family="sans-serif" font-size="%4" font-weight="600" fill="%5">%6</text>)"
             R"(</svg>)")
      .arg(SIZE)            // %1
      .arg(background)      // %2
      .arg(SIZE / 2)        // %3
      .arg(SIZE * 48 / 100) // %4  font-size ~48% of viewbox
      .arg(textColor)       // %5
      .arg(escapedInitial); // %6
}

} // namespace avatars

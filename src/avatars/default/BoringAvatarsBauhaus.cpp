// SPDX-FileCopyrightText: Komai Contributors
// SPDX-FileCopyrightText: Boring Avatars Contributors (MIT License)
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Bauhaus style ported from Boring Avatars (avatar-bauhaus.tsx).
// https://github.com/boringdesigners/boring-avatars — MIT License.

#include "BoringAvatars.h"

namespace boring_avatars {

QString
generateBauhaus(const QString &key)
{
    constexpr int SIZE     = 80;
    constexpr int ELEMENTS = 4;
    constexpr int range    = 5;

    const uint32_t num = hashCode(key);

    struct Element
    {
        const char *color;
        int translateX;
        int translateY;
        int rotate;
        bool isSquare;
    };

    Element elems[ELEMENTS];
    for (int i = 0; i < ELEMENTS; ++i) {
        const uint32_t n    = num * static_cast<uint32_t>(i + 1);
        elems[i].color      = getRandomColor(num + static_cast<uint32_t>(i), range);
        elems[i].translateX = getUnit(n, SIZE / 2 - (i + 17), 1);
        elems[i].translateY = getUnit(n, SIZE / 2 - (i + 17), 2);
        elems[i].rotate     = getUnit(n, 360);
        elems[i].isSquare   = getBoolean(num, 2);
    }

    const int rectHeight = elems[1].isSquare ? SIZE : SIZE / 8;

    return QStringLiteral(
             R"SVG(<svg viewBox="0 0 %1 %1" fill="none" xmlns="http://www.w3.org/2000/svg">)SVG"
             R"SVG(<mask id="m" maskUnits="userSpaceOnUse" x="0" y="0" width="%1" height="%1">)SVG"
             R"SVG(<rect width="%1" height="%1" fill="#FFFFFF"/>)SVG"
             R"SVG(</mask>)SVG"
             R"SVG(<g mask="url(#m)">)SVG"
             R"SVG(<rect width="%1" height="%1" fill="%2"/>)SVG"
             R"SVG(<rect x="%3" y="%4" width="%1" height="%5" fill="%6" )SVG"
             R"SVG(transform="translate(%7 %8) rotate(%9 %10 %10)"/>)SVG"
             R"SVG(<circle cx="%10" cy="%10" fill="%11" r="%12" )SVG"
             R"SVG(transform="translate(%13 %14)"/>)SVG"
             R"SVG(<line x1="0" y1="%10" x2="%1" y2="%10" stroke-width="2" stroke="%15" )SVG"
             R"SVG(transform="translate(%16 %17) rotate(%18 %10 %10)"/>)SVG"
             R"SVG(</g>)SVG"
             R"SVG(</svg>)SVG")
      .arg(SIZE)                          // %1
      .arg(QLatin1String(elems[0].color)) // %2
      .arg((SIZE - 60) / 2)               // %3  rect x
      .arg((SIZE - 20) / 2)               // %4  rect y
      .arg(rectHeight)                    // %5
      .arg(QLatin1String(elems[1].color)) // %6
      .arg(elems[1].translateX)           // %7
      .arg(elems[1].translateY)           // %8
      .arg(elems[1].rotate)               // %9
      .arg(SIZE / 2)                      // %10
      .arg(QLatin1String(elems[2].color)) // %11
      .arg(SIZE / 5)                      // %12 circle r
      .arg(elems[2].translateX)           // %13
      .arg(elems[2].translateY)           // %14
      .arg(QLatin1String(elems[3].color)) // %15
      .arg(elems[3].translateX)           // %16
      .arg(elems[3].translateY)           // %17
      .arg(elems[3].rotate);              // %18
}

} // namespace boring_avatars

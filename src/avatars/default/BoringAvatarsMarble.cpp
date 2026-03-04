// SPDX-FileCopyrightText: Komai Contributors
// SPDX-FileCopyrightText: Boring Avatars Contributors (MIT License)
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Marble style ported from Boring Avatars (avatar-marble.tsx).
// https://github.com/boringdesigners/boring-avatars — MIT License.
// Uses SVG feBlend instead of CSS mix-blend-mode for Qt compatibility.
// Uses expanded color palette for greater avatar variety.

#include "BoringAvatars.h"

namespace boring_avatars {

QString
generateMarble(const QString &key)
{
    constexpr int SIZE     = 80;
    constexpr int ELEMENTS = 3;
    constexpr int range    = kDefaultColors.size();

    const uint32_t num = hashCode(key);

    struct Element
    {
        const char *color;
        int translateX;
        int translateY;
        double scale;
        int rotate;
    };

    Element elems[ELEMENTS];
    for (int i = 0; i < ELEMENTS; ++i) {
        const uint32_t n    = num * static_cast<uint32_t>(i + 1);
        elems[i].color      = getRandomColor(num + static_cast<uint32_t>(i), range);
        elems[i].translateX = getUnit(n, SIZE / 10, 1);
        elems[i].translateY = getUnit(n, SIZE / 10, 2);
        elems[i].scale      = 1.2 + getUnit(n, SIZE / 20) / 10.0;
        elems[i].rotate     = getUnit(n, 360, 1);
    }

    return QStringLiteral(
             R"SVG(<svg viewBox="0 0 %1 %1" fill="none" xmlns="http://www.w3.org/2000/svg">)SVG"
             R"SVG(<mask id="m" maskUnits="userSpaceOnUse" x="0" y="0" width="%1" height="%1">)SVG"
             R"SVG(<rect width="%1" height="%1" fill="#FFFFFF"/>)SVG"
             R"SVG(</mask>)SVG"
             R"SVG(<g mask="url(#m)">)SVG"
             R"SVG(<rect width="%1" height="%1" fill="%2"/>)SVG"
             R"SVG(<path filter="url(#f)" )SVG"
             R"SVG(d="M32.414 59.35L50.376 70.5H72.5v-71H33.728L26.5 13.381l19.057 27.08L32.414 59.35z" )SVG"
             R"SVG(fill="%3" )SVG"
             R"SVG(transform="translate(%4 %5) rotate(%6 %7 %7) scale(%8)"/>)SVG"
             R"SVG(<path filter="url(#f)" )SVG"
             R"SVG(d="M22.216 24L0 46.75l14.108 38.129L78 86l-3.081-59.276-22.378 4.005 12.972 20.186-23.35 27.395L22.215 24z" )SVG"
             R"SVG(fill="%9" )SVG"
             R"SVG(transform="translate(%10 %11) rotate(%12 %7 %7) scale(%13)"/>)SVG"
             R"SVG(</g>)SVG"
             R"SVG(<defs>)SVG"
             R"SVG(<filter id="f" filterUnits="userSpaceOnUse" color-interpolation-filters="sRGB">)SVG"
             R"SVG(<feFlood flood-opacity="0" result="bg"/>)SVG"
             R"SVG(<feBlend in="SourceGraphic" in2="bg" result="shape"/>)SVG"
             R"SVG(<feGaussianBlur stdDeviation="7" result="blur"/>)SVG"
             R"SVG(</filter>)SVG"
             R"SVG(</defs>)SVG"
             R"SVG(</svg>)SVG")
      .arg(SIZE)                          // %1
      .arg(QLatin1String(elems[0].color)) // %2
      .arg(QLatin1String(elems[1].color)) // %3
      .arg(elems[1].translateX)           // %4
      .arg(elems[1].translateY)           // %5
      .arg(elems[1].rotate)               // %6
      .arg(SIZE / 2)                      // %7
      .arg(elems[2].scale, 0, 'f', 1)     // %8
      .arg(QLatin1String(elems[2].color)) // %9
      .arg(elems[2].translateX)           // %10
      .arg(elems[2].translateY)           // %11
      .arg(elems[2].rotate)               // %12
      .arg(elems[2].scale, 0, 'f', 1);    // %13
}

} // namespace boring_avatars

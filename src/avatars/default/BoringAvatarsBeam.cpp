// SPDX-FileCopyrightText: Komai Contributors
// SPDX-FileCopyrightText: Boring Avatars Contributors (MIT License)
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Beam style ported from Boring Avatars (avatar-beam.tsx).
// https://github.com/boringdesigners/boring-avatars — MIT License.
// Uses expanded color palette for greater avatar variety.

#include "BoringAvatars.h"

namespace boring_avatars {

QString
generateBeam(const QString &key)
{
    constexpr int SIZE  = 36;
    constexpr int range = kDefaultColors.size();

    const uint32_t num          = hashCode(key);
    const char *wrapperColor    = getRandomColor(num, range);
    const char *backgroundColor = getRandomColor(num + 13, range);
    const char *faceColor       = getContrast(wrapperColor);

    int preTranslateX           = getUnit(num, 10, 1);
    const int wrapperTranslateX = preTranslateX < 5 ? preTranslateX + SIZE / 9 : preTranslateX;
    int preTranslateY           = getUnit(num, 10, 2);
    const int wrapperTranslateY = preTranslateY < 5 ? preTranslateY + SIZE / 9 : preTranslateY;
    const int wrapperRotate     = getUnit(num, 360);
    const double wrapperScale   = 1.0 + getUnit(num, SIZE / 12) / 10.0;
    const bool isMouthOpen      = getBoolean(num, 2);
    const bool isCircle         = getBoolean(num, 1);
    const int eyeSpread         = getUnit(num, 5);
    const int mouthSpread       = getUnit(num, 3);
    const int faceRotate        = getUnit(num, 10, 3);
    const int faceTranslateX =
      wrapperTranslateX > SIZE / 6 ? wrapperTranslateX / 2 : getUnit(num, 8, 1);
    const int faceTranslateY =
      wrapperTranslateY > SIZE / 6 ? wrapperTranslateY / 2 : getUnit(num, 7, 2);

    const int rx = isCircle ? SIZE : SIZE / 6;

    // Pre-stringify values to build SVG via concatenation, avoiding
    // Qt's chained .arg() which can confuse %1 with %10-%16.
    const auto s      = QString::number(SIZE);
    const auto half   = QString::number(SIZE / 2);
    const auto bg     = QLatin1String(backgroundColor);
    const auto wrap   = QLatin1String(wrapperColor);
    const auto face   = QLatin1String(faceColor);
    const auto scale  = QString::number(wrapperScale, 'f', 1);
    const auto mouthY = QString::number(19 + mouthSpread);

    QString mouth;
    if (isMouthOpen) {
        mouth = R"(<path d="M15 )" + mouthY + R"(c2 1 4 1 6 0" stroke=")" + face +
                R"(" fill="none" stroke-linecap="round"/>)";
    } else {
        mouth = R"(<path d="M13,)" + mouthY + R"( a1,0.75 0 0,0 10,0" fill=")" + face + R"("/>)";
    }

    // clang-format off
    return QStringLiteral(R"SVG(<svg viewBox="0 0 )SVG") + s + u' ' + s +
           QStringLiteral(R"SVG(" fill="none" xmlns="http://www.w3.org/2000/svg">)SVG"
                          R"SVG(<mask id="m" maskUnits="userSpaceOnUse" x="0" y="0" width=")SVG") + s +
           QStringLiteral(R"SVG(" height=")SVG") + s +
           QStringLiteral(R"SVG("><rect width=")SVG") + s +
           QStringLiteral(R"SVG(" height=")SVG") + s +
           QStringLiteral(R"SVG(" fill="#FFFFFF"/></mask><g mask="url(#m)"><rect width=")SVG") + s +
           QStringLiteral(R"SVG(" height=")SVG") + s +
           QStringLiteral(R"SVG(" fill=")SVG") + bg +
           QStringLiteral(R"SVG("/><rect x="0" y="0" width=")SVG") + s +
           QStringLiteral(R"SVG(" height=")SVG") + s +
           QStringLiteral(R"SVG(" transform="translate()SVG") +
           QString::number(wrapperTranslateX) + u' ' + QString::number(wrapperTranslateY) +
           QStringLiteral(R"SVG() rotate()SVG") +
           QString::number(wrapperRotate) + u' ' + half + u' ' + half +
           QStringLiteral(R"SVG() scale()SVG") + scale +
           QStringLiteral(R"SVG()" fill=")SVG") + wrap +
           QStringLiteral(R"SVG(" rx=")SVG") + QString::number(rx) +
           QStringLiteral(R"SVG("/><g transform="translate()SVG") +
           QString::number(faceTranslateX) + u' ' + QString::number(faceTranslateY) +
           QStringLiteral(R"SVG() rotate()SVG") +
           QString::number(faceRotate) + u' ' + half + u' ' + half +
           QStringLiteral(R"SVG()">)SVG") + mouth +
           QStringLiteral(R"SVG(<rect x=")SVG") + QString::number(14 - eyeSpread) +
           QStringLiteral(R"SVG(" y="14" width="1.5" height="2" rx="1" stroke="none" fill=")SVG") + face +
           QStringLiteral(R"SVG("/><rect x=")SVG") + QString::number(20 + eyeSpread) +
           QStringLiteral(R"SVG(" y="14" width="1.5" height="2" rx="1" stroke="none" fill=")SVG") + face +
           QStringLiteral(R"SVG("/></g></g></svg>)SVG");
    // clang-format on
}

} // namespace boring_avatars

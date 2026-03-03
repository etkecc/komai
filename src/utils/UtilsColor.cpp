// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Utils.h"

#include <QCryptographicHash>
#include <QPalette>

#include <limits>

#include "settings/ui/facade/UserSettingsPage.h"

QString
utils::linkColor()
{
    const auto theme = UserSettings::instance()->uiThemeSlug();

    if (theme == QLatin1String("light")) {
        return QStringLiteral("#0077b5");
    } else if (theme == QLatin1String("dark")) {
        return QStringLiteral("#38A3D8");
    } else {
        return QPalette().color(QPalette::Link).name();
    }
}

uint32_t
utils::hashQString(const QString &input)
{
    auto h = QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha1);

    return (static_cast<uint32_t>(h[0]) << 24) ^ (static_cast<uint32_t>(h[1]) << 16) ^
           (static_cast<uint32_t>(h[2]) << 8) ^ static_cast<uint32_t>(h[3]);
}

QColor
utils::generateContrastingHexColor(const QString &input, const QColor &backgroundCol)
{
    const qreal backgroundLum = luminance(backgroundCol);

    // Create a color for the input
    auto hash = hashQString(input);
    // create a hue value based on the hash of the input.
    // Adapted to make Nico blue
    auto userHue = static_cast<double>(hash - static_cast<uint32_t>(0x60'00'00'00)) /
                   std::numeric_limits<uint32_t>::max() * 360.;
    // start with moderate saturation and lightness values.
    auto sat       = 230.;
    auto lightness = 125.;

    // converting to a QColor makes the luminance calc easier.
    QColor inputColor = QColor::fromHsl(
      static_cast<int>(userHue), static_cast<int>(sat), static_cast<int>(lightness));

    // calculate the initial luminance and contrast of the
    // generated color.  It's possible that no additional
    // work will be necessary.
    auto lum      = luminance(inputColor);
    auto contrast = computeContrast(lum, backgroundLum);

    // If the contrast doesn't meet our criteria,
    // try again and again until they do by modifying first
    // the lightness and then the saturation of the color.
    int iterationCount = 9;
    while (contrast < 4.5) {
        // if our lightness is at it's bounds, try changing
        // saturation instead.
        if (lightness >= 242 || lightness <= 13) {
            qreal newSat = qBound(26.0, sat * 1.25, 242.0);

            inputColor.setHsl(static_cast<int>(userHue),
                              static_cast<int>(qFloor(newSat)),
                              static_cast<int>(lightness));
            auto tmpLum         = luminance(inputColor);
            auto higherContrast = computeContrast(tmpLum, backgroundLum);
            if (higherContrast > contrast) {
                contrast = higherContrast;
                sat      = newSat;
            } else {
                newSat = qBound(26.0, sat / 1.25, 242.0);
                inputColor.setHsl(static_cast<int>(userHue),
                                  static_cast<int>(qFloor(newSat)),
                                  static_cast<int>(lightness));
                tmpLum             = luminance(inputColor);
                auto lowerContrast = computeContrast(tmpLum, backgroundLum);
                if (lowerContrast > contrast) {
                    contrast = lowerContrast;
                    sat      = newSat;
                }
            }
        } else {
            qreal newLightness = qBound(13.0, lightness * 1.25, 242.0);

            inputColor.setHsl(static_cast<int>(userHue),
                              static_cast<int>(sat),
                              static_cast<int>(qFloor(newLightness)));

            auto tmpLum         = luminance(inputColor);
            auto higherContrast = computeContrast(tmpLum, backgroundLum);

            // Check to make sure we have actually improved contrast
            if (higherContrast > contrast) {
                contrast  = higherContrast;
                lightness = newLightness;
                // otherwise, try going the other way instead.
            } else {
                newLightness = qBound(13.0, lightness / 1.25, 242.0);
                inputColor.setHsl(static_cast<int>(userHue),
                                  static_cast<int>(sat),
                                  static_cast<int>(qFloor(newLightness)));
                tmpLum             = luminance(inputColor);
                auto lowerContrast = computeContrast(tmpLum, backgroundLum);
                if (lowerContrast > contrast) {
                    contrast  = lowerContrast;
                    lightness = newLightness;
                }
            }
        }

        // don't loop forever, just give up at some point!
        // Someone smart may find a better solution
        if (--iterationCount < 0)
            break;
    }

    // get the hex value of the generated color.
    auto colorHex = inputColor.name();

    return colorHex;
}

qreal
utils::computeContrast(const qreal &one, const qreal &two)
{
    auto ratio = (one + 0.05) / (two + 0.05);

    if (two > one) {
        ratio = 1 / ratio;
    }

    return ratio;
}

qreal
utils::luminance(const QColor &col)
{
    int colRgb[3] = {col.red(), col.green(), col.blue()};
    qreal lumRgb[3];

    for (int i = 0; i < 3; i++) {
        qreal v   = colRgb[i] / 255.0;
        lumRgb[i] = v <= 0.03928 ? v / 12.92 : qPow((v + 0.055) / 1.055, 2.4);
    }

    auto lum = lumRgb[0] * 0.2126 + lumRgb[1] * 0.7152 + lumRgb[2] * 0.0722;

    return lum;
}

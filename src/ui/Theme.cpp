// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Theme.h"

QPalette
Theme::paletteFromTheme(QStringView theme)
{
    static QPalette original;

    const auto *def = ThemeRegistry::instance().findTheme(theme);
    if (def)
        return def->toPalette();

    return original;
}

Theme::Theme(QStringView theme)
{
    auto p     = paletteFromTheme(theme);
    separator_ = p.mid().color();

    // Hardcoded fallback user colors for unknown themes
    auto applyDefaultUserColors = [this, &p]() {
        userColorSelf_ = p.color(QPalette::Highlight);
        // Golden-angle-spaced hues at S=0.7, L=0.5 (neutral defaults)
        userColorOthers_ = {
          QColor::fromHslF(0.0 / 360, 0.7, 0.5),
          QColor::fromHslF(137.5 / 360, 0.7, 0.5),
          QColor::fromHslF(275.0 / 360, 0.7, 0.5),
          QColor::fromHslF(52.5 / 360, 0.7, 0.5),
          QColor::fromHslF(190.0 / 360, 0.7, 0.5),
          QColor::fromHslF(327.5 / 360, 0.7, 0.5),
          QColor::fromHslF(95.0 / 360, 0.7, 0.5),
          QColor::fromHslF(232.5 / 360, 0.7, 0.5),
          QColor::fromHslF(22.5 / 360, 0.7, 0.5),
          QColor::fromHslF(160.0 / 360, 0.7, 0.5),
          QColor::fromHslF(297.5 / 360, 0.7, 0.5),
          QColor::fromHslF(75.0 / 360, 0.7, 0.5),
          QColor::fromHslF(212.5 / 360, 0.7, 0.5),
          QColor::fromHslF(350.0 / 360, 0.7, 0.5),
        };
    };

    const auto *def = ThemeRegistry::instance().findTheme(theme);
    if (def) {
        sidebarBackground_ = p.color(QPalette::AlternateBase);
        attention_         = def->attention;
        success_           = def->success;
        warning_           = def->warning;
        error_             = def->error;
        userColorSelf_     = def->userColorSelf.background;
        userColorOthers_.clear();
        userColorOthers_.reserve(static_cast<qsizetype>(def->userColorOthers.size()));
        for (const auto &slot : def->userColorOthers)
            userColorOthers_.append(slot.background);
    } else {
        // Unknown theme — fall back to palette-derived defaults
        sidebarBackground_ = p.color(QPalette::AlternateBase);
        attention_         = QColor(QColorConstants::Svg::red);
        success_           = QColor(QColorConstants::Svg::green);
        warning_           = QColor(QColorConstants::Svg::orange);
        error_             = QColor(0xdd, 0x3d, 0x3d);
        applyDefaultUserColors();
    }
}

#include "moc_Theme.cpp"

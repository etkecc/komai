// SPDX-FileCopyrightText: Nheko Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Theme.h"

QPalette
Theme::paletteFromTheme(QStringView theme)
{
    static QPalette original;

    if (theme == u"system")
        return original;

    const auto *def = findTheme(theme);
    if (def)
        return def->toPalette();

    return original;
}

Theme::Theme(QStringView theme)
{
    auto p     = paletteFromTheme(theme);
    separator_ = p.mid().color();

    if (theme == u"system") {
        sidebarBackground_ = p.color(QPalette::AlternateBase);
        red_               = QColor(QColorConstants::Svg::red);
        green_             = QColor(QColorConstants::Svg::green);
        orange_            = QColor(QColorConstants::Svg::orange);
        error_             = QColor(0xdd, 0x3d, 0x3d);
        return;
    }

    const auto *def = findTheme(theme);
    if (def) {
        sidebarBackground_ = p.color(QPalette::AlternateBase);
        red_               = def->red;
        green_             = def->green;
        orange_            = def->orange;
        error_             = def->error;
    } else {
        // Unknown theme — fall back to system-like defaults
        sidebarBackground_ = p.color(QPalette::AlternateBase);
        red_               = QColor(QColorConstants::Svg::red);
        green_             = QColor(QColorConstants::Svg::green);
        orange_            = QColor(QColorConstants::Svg::orange);
        error_             = QColor(0xdd, 0x3d, 0x3d);
    }
}

#include "moc_Theme.cpp"

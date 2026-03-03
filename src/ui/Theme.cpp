// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Theme.h"

QPalette
Theme::paletteFromTheme(QStringView theme)
{
    static QPalette original;

    if (theme == u"system")
        return original;

    const auto *def = ThemeRegistry::instance().findTheme(theme);
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
        attention_         = QColor(QColorConstants::Svg::red);
        success_           = QColor(QColorConstants::Svg::green);
        warning_           = QColor(QColorConstants::Svg::orange);
        error_             = QColor(0xdd, 0x3d, 0x3d);
        return;
    }

    const auto *def = ThemeRegistry::instance().findTheme(theme);
    if (def) {
        sidebarBackground_ = p.color(QPalette::AlternateBase);
        attention_         = def->attention;
        success_           = def->success;
        warning_           = def->warning;
        error_             = def->error;
    } else {
        // Unknown theme — fall back to system-like defaults
        sidebarBackground_ = p.color(QPalette::AlternateBase);
        attention_         = QColor(QColorConstants::Svg::red);
        success_           = QColor(QColorConstants::Svg::green);
        warning_           = QColor(QColorConstants::Svg::orange);
        error_             = QColor(0xdd, 0x3d, 0x3d);
    }
}

#include "moc_Theme.cpp"

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QColor>
#include <QPalette>
#include <QString>

struct ThemeUserColorSlot
{
    QColor background;
    QColor text;
    QColor secondaryText;
    QColor link;
};

struct ThemeDef
{
    QString slug;
    QString name;
    QString variant; // "light" or "dark"
    int sortOrder;

    // QPalette colors
    QColor window, windowText, base, alternateBase, text, brightText;
    QColor button, buttonText, light, mid, dark;
    QColor highlight, highlightedText, link;
    QColor toolTipBase, toolTipText;

    // Semantic accent colors
    QColor attention, attentionText, success, warning, error;

    // User colors for sender/member color coding
    ThemeUserColorSlot userColorSelf;
    std::vector<ThemeUserColorSlot> userColorOthers;

    QString source; // "builtin" or "/full/path/to/theme.yml"

    QPalette toPalette() const
    {
        QPalette p(windowText, button, light, dark, mid, text, brightText, base, window);
        p.setColor(QPalette::AlternateBase, alternateBase);
        p.setColor(QPalette::Highlight, highlight);
        p.setColor(QPalette::HighlightedText, highlightedText);
        p.setColor(QPalette::ToolTipBase, toolTipBase);
        p.setColor(QPalette::ToolTipText, toolTipText);
        p.setColor(QPalette::Link, link);
        p.setColor(QPalette::ButtonText, buttonText);

        // Theme YAML provides one colour per role. QPalette's 9-arg ctor and
        // the setColor(role, color) overload write the same value into the
        // Active, Inactive and Disabled groups, so disabled controls look
        // identical to enabled ones (the Basic Quick-Controls MenuItem in
        // particular has no built-in disabled treatment and relies entirely
        // on palette.windowText resolving differently when enabled is false).
        // Derive a dimmer text variant for the Disabled group by blending
        // each text role halfway toward its natural background. Works the
        // same way for light and dark themes since the dim direction
        // follows the theme's background role rather than a fixed colour.
        auto mix = [](const QColor &a, const QColor &b, qreal t) {
            return QColor::fromRgbF(a.redF() * (1 - t) + b.redF() * t,
                                    a.greenF() * (1 - t) + b.greenF() * t,
                                    a.blueF() * (1 - t) + b.blueF() * t,
                                    a.alphaF() * (1 - t) + b.alphaF() * t);
        };
        constexpr qreal disabledBlend = 0.5;
        p.setColor(
          QPalette::Disabled, QPalette::WindowText, mix(windowText, window, disabledBlend));
        p.setColor(QPalette::Disabled, QPalette::Text, mix(text, base, disabledBlend));
        p.setColor(
          QPalette::Disabled, QPalette::ButtonText, mix(buttonText, button, disabledBlend));
        p.setColor(
          QPalette::Disabled, QPalette::BrightText, mix(brightText, window, disabledBlend));
        p.setColor(QPalette::Disabled,
                   QPalette::HighlightedText,
                   mix(highlightedText, highlight, disabledBlend));
        p.setColor(QPalette::Disabled, QPalette::Link, mix(link, window, disabledBlend));

        return p;
    }
};

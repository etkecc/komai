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
    QColor attention, success, warning, error;

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
        return p;
    }
};

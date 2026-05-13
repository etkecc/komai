// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton

import QtQml

// Force a Menu to be wide enough for its longest item. Komai uses the
// QtQuick Controls Basic style, whose Menu background has a fixed
// default implicitWidth (~200 px). Menu.contentWidth isn't always
// recomputed from the items' implicit widths -- especially for menus
// built dynamically (createObject + addItem/addMenu) and for menus
// whose visible items depend on translatable strings that can be much
// wider than the English. The fix: set Menu.contentWidth from the
// widest item's required width.
//
// We compute `implicitContentWidth + leftPadding + rightPadding` rather
// than reading `implicitWidth` directly: the Basic-style MenuItem also
// has a 200 px background floor, so items whose content is just under
// 200 px wide report `implicitWidth == 200` and lie about how much room
// they actually need (the checkable indicator alone adds 30 px on top
// of icon + spacing + text). Going through the contentItem side
// bypasses that floor.
QtObject {
    // Install a binding on Menu.contentWidth that recomputes the widest
    // item's required width whenever any item's text/icon/padding (and
    // therefore implicitContentWidth) changes. This is the right tool
    // for declaratively-declared menus: at construction time the
    // contentItem (IconLabel) hasn't been laid out yet, so a one-shot
    // read at popup time would see implicitContentWidth = 0 and floor
    // the menu at the background's 200 px default. A binding will
    // re-evaluate once Qt measures the items and reports real widths.
    function applyAutoWidth(m) {
        m.contentWidth = Qt.binding(function () {
            let w = 0;
            for (let i = 0; i < m.count; ++i) {
                const it = m.itemAt(i);
                if (!it)
                    continue;
                const need = it.implicitContentWidth + it.leftPadding + it.rightPadding;
                if (need > w)
                    w = need;
            }
            return w;
        });
    }

    // One-shot variant: read item widths once and set contentWidth.
    // Use this for menus built dynamically (createObject + addItem) at
    // the moment of popup, where items are fully realised before they
    // get added and a binding would be overkill. Reads via
    // implicitContentWidth + padding for the same reason as above.
    function sizeMenuToContents(m) {
        let w = 0;
        for (let i = 0; i < m.count; ++i) {
            const it = m.itemAt(i);
            if (!it)
                continue;
            const need = it.implicitContentWidth + it.leftPadding + it.rightPadding;
            if (need > w)
                w = need;
        }
        if (w > 0)
            m.contentWidth = w;
    }
}

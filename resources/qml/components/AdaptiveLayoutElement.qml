// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

Item {
    clip: true

    property int minimumWidth: 100
    property int maximumWidth: 400
    property int collapsedWidth: 40
    // When > collapsedWidth, the range [collapsedWidth, snapUpperWidth] is
    // treated as a "dead zone" during splitter drag: on release, the splitter
    // snaps to whichever endpoint is closer. Useful when an element has two
    // discrete layout modes (e.g. icon-only vs. full) and any in-between
    // width would just waste space.
    property int snapUpperWidth: 0
    property bool collapsible: true
    property bool collapsed: width <= collapsedWidth
    property int splitterWidth: 1
    property bool splitterOnLeft: false
    property int preferredWidth: 100

    Component.onCompleted: {
        children[0].x = Qt.binding(() => {
            return splitterOnLeft ? splitterWidth : 0;
        });
        children[0].width = Qt.binding(() => {
            return width - splitterWidth;
        });
        children[0].height = Qt.binding(() => {
            return parent.height;
        });
    }
}

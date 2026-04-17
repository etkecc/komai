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
    property bool collapsible: true
    property bool collapsed: width <= collapsedWidth
    property int splitterWidth: 1
    property int preferredWidth: 100

    Component.onCompleted: {
        children[0].width = Qt.binding(() => {
            return width - splitterWidth;
        });
        children[0].height = Qt.binding(() => {
            return parent.height;
        });
    }
}

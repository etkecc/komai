// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import cc.etke.komai

TextEdit {
    id: root

    required property var model
    property bool hovered: false

    color: root.hovered ? palette.brightText : palette.text
    // Guard against `model` going null during search-filter teardown.
    text: model?.value ?? ""
    textFormat: Text.RichText
    readOnly: true
    selectByMouse: true
    onLinkActivated: function(link) {
        Qt.openUrlExternally(link);
    }

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: root.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}

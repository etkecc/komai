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
    property bool leftAligned: false
    property bool hovered: false

    x: 0
    width: parent ? parent.width : implicitWidth
    height: Math.ceil(contentHeight)
    clip: true
    color: root.hovered ? palette.brightText : palette.text
    font.pointSize: Settings.uiFontSizePt
    // Guard against `model` going null during search-filter teardown.
    text: model?.value ?? ""
    horizontalAlignment: root.leftAligned ? Text.AlignLeft : Text.AlignRight
    wrapMode: root.leftAligned ? TextEdit.WrapAnywhere : TextEdit.NoWrap
    readOnly: true
    selectByMouse: true
}

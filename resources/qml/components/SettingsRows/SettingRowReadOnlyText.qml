// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import im.nheko

TextEdit {
    id: root

    required property var model
    property bool leftAligned: false

    x: 0
    width: parent ? parent.width : implicitWidth
    height: Math.ceil(contentHeight)
    clip: true
    color: palette.text
    font.pointSize: Settings.uiFontSizePt
    text: model.value
    horizontalAlignment: root.leftAligned ? Text.AlignLeft : Text.AlignRight
    wrapMode: root.leftAligned ? TextEdit.WrapAnywhere : TextEdit.NoWrap
    readOnly: true
    selectByMouse: true
}

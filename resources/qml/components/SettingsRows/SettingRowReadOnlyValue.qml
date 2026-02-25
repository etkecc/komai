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

    x: 0
    width: parent ? parent.width : implicitWidth
    clip: true
    color: palette.text
    font.pointSize: Settings.uiFontSizePt
    horizontalAlignment: Text.AlignRight
    wrapMode: TextEdit.NoWrap
    readOnly: true
    selectByMouse: true
    text: model.value
}

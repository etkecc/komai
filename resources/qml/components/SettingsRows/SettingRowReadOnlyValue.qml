// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick

TextEdit {
    id: root

    required property var model

    x: 0
    width: parent ? parent.width : implicitWidth
    clip: true
    color: palette.text
    horizontalAlignment: Text.AlignRight
    wrapMode: TextEdit.NoWrap
    readOnly: true
    selectByMouse: true
    text: model.value
}

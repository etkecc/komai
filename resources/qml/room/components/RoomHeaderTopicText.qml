// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../delegates"
import QtQuick 2.15
import QtQuick.Layouts 1.2

MatrixText {
    id: root

    property string roomTopic: ""
    property bool compactMode: false
    property real lineSpacing: 0

    Layout.column: 1
    Layout.columnSpan: 8
    Layout.fillWidth: true
    Layout.maximumHeight: lineSpacing * 2 // show 2 lines
    Layout.row: 2
    clip: true
    color: palette.text
    selectByMouse: true
    text: roomTopic
    visible: roomTopic.length > 0 && !compactMode
}

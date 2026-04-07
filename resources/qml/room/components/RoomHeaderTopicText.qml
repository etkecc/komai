// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts

MatrixText {
    id: root

    property string roomTopic: ""
    property bool compactMode: false
    property real lineSpacing: 0

    Layout.maximumHeight: lineSpacing * 2 // show 2 lines
    clip: true
    color: palette.text
    selectByMouse: true
    text: roomTopic
    visible: roomTopic.length > 0 && !compactMode
}

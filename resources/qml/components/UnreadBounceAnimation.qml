// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

SequentialAnimation {
    id: animation

    property bool active: false
    property real offset: 0

    running: active && Settings.uiMotionAnimationsEnabled
    loops: Animation.Infinite

    NumberAnimation { target: animation; property: "offset"; from: 0; to: -4; duration: 200; easing.type: Easing.OutQuad }
    NumberAnimation { target: animation; property: "offset"; from: -4; to: 0; duration: 200; easing.type: Easing.InQuad }
    PauseAnimation { duration: 1800 }
}

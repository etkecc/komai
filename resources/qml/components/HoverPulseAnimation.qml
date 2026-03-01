// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

SequentialAnimation {
    id: animation

    required property Item targetItem
    property bool motionEnabled: Settings.uiMotionAnimationsEnabled

    NumberAnimation {
        target: animation.targetItem
        property: "scale"
        from: 1.0
        to: 1.2
        duration: 150
        easing.type: Easing.OutQuad
    }
    NumberAnimation {
        target: animation.targetItem
        property: "scale"
        from: 1.2
        to: 1.0
        duration: 150
        easing.type: Easing.InQuad
    }

    function pulse() {
        if (motionEnabled)
            start();
    }
}

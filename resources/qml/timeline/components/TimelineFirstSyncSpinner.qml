// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../ui"
import QtQuick
import im.nheko

Spinner {
    id: root

    required property bool waitingForFirstSync

    anchors.centerIn: parent
    foreground: palette.mid
    height: Nheko.timelineLogoSize
    opacity: hoverHandler.hovered ? 0.3 : 1
    running: waitingForFirstSync
    visible: waitingForFirstSync
    z: 3

    Behavior on opacity  {
        NumberAnimation {
            duration: 100
        }
    }

    HoverHandler {
        id: hoverHandler

    }
}

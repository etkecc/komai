// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls

Label {
    property bool isStateEvent
    color: palette.text
    horizontalAlignment: Text.AlignHCenter
    height: Math.round(fontMetrics.height * 1.4)
    width: contentWidth * 1.2

    background: Rectangle {
        radius: parent.height / 2
        color: palette.alternateBase
    }

}

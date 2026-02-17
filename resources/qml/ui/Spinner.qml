// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

Item {
    id: spinner

    property bool running: true
    property var foreground: "#333"

    height: 40
    width: height

    Image {
        id: logo

        anchors.centerIn: parent
        height: spinner.height
        width: spinner.height
        source: "qrc:/logos/komai.svg"
        sourceSize.height: spinner.height * 2
        sourceSize.width: spinner.height * 2
        fillMode: Image.PreserveAspectFit
        smooth: true

        RotationAnimator {
            target: logo
            from: 0
            to: 360
            duration: 3000
            loops: Animation.Infinite
            running: spinner.running
        }
    }
}

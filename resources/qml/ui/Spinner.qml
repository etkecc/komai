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

        SequentialAnimation {
            loops: Animation.Infinite
            running: spinner.running && !Settings.reducedMotion

            NumberAnimation {
                target: logo
                property: "scale"
                from: 1.0
                to: 1.2
                duration: 400
                easing.type: Easing.OutQuad
            }
            NumberAnimation {
                target: logo
                property: "scale"
                from: 1.2
                to: 1.0
                duration: 400
                easing.type: Easing.InQuad
            }

            onRunningChanged: {
                if (!running)
                    logo.scale = 1.0;
            }
        }
    }
}

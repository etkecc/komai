// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts

ComposerToolbarButton {
    id: root

    required property var room
    required property bool showAllButtons
    readonly property bool uploadInProgress: !!(root.room && root.room.input && root.room.input.uploading === true)

    Layout.alignment: Qt.AlignBottom
    toolTipText: uploadInProgress ? "" : qsTr("Attach an image or file")
    image: uploadInProgress ? "" : ":/icons/icons/ui/attach.svg"
    visible: showAllButtons
    opacity: enabled ? 1.0 : 0.3

    onClicked: {
        if (!uploadInProgress && room && room.input)
            room.input.openFileSelection();
    }

    Rectangle {
        anchors.fill: parent
        color: palette.window
        visible: root.uploadInProgress

        Item {
            id: uploadProgressAnimation

            property real progress: 0
            readonly property real logoSize: Math.max(12, parent.height * 0.6)
            readonly property real travelDistance: parent.height + logoSize

            anchors.fill: parent
            clip: true

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                height: uploadProgressAnimation.logoSize
                source: "qrc:/logos/komai.svg"
                width: uploadProgressAnimation.logoSize
                y: parent.height - uploadProgressAnimation.progress * uploadProgressAnimation.travelDistance
            }
            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                height: uploadProgressAnimation.logoSize
                source: "qrc:/logos/komai.svg"
                width: uploadProgressAnimation.logoSize
                y: parent.height - ((uploadProgressAnimation.progress + 0.5) % 1) * uploadProgressAnimation.travelDistance
            }

            NumberAnimation on progress {
                duration: 1200
                from: 0
                loops: Animation.Infinite
                running: root.uploadInProgress
                to: 1
            }
        }
    }
}

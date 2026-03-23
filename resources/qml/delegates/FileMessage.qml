// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import cc.etke.komai

Control {
    id: evRoot

    required property QtObject styleProfile
    required property string eventId
    required property string filename
    required property string filesize
    required property string fileTypeIconSource

    padding: styleProfile.fileMessagePadding
    property int metadataWidth: 0
    property bool fitsMetadata: false

    Layout.maximumWidth: rowa.Layout.maximumWidth + padding * 2

    contentItem: RowLayout {
        id: rowa

        spacing: Komai.paddingMedium * 2

        Rectangle {
            id: iconCircle

            readonly property int circleSize: Komai.listIconSize + 2 * Komai.paddingMedium

            color: palette.light
            radius: circleSize / 2
            Layout.preferredHeight: circleSize
            Layout.preferredWidth: circleSize

            Image {
                id: img

                height: Komai.listIconSize
                width: Komai.listIconSize
                sourceSize.height: Komai.listIconSize * Screen.devicePixelRatio
                sourceSize.width: Komai.listIconSize * Screen.devicePixelRatio

                anchors.centerIn: parent
                source: "image://colorimage/" + evRoot.fileTypeIconSource + "?" + palette.buttonText
                fillMode: Image.PreserveAspectFit
            }
        }

        ColumnLayout {
            id: col

            Layout.fillWidth: true

            Text {
                id: filename_

                Layout.fillWidth: true
                Layout.maximumWidth: implicitWidth + 1
                text: evRoot.filename
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: palette.text
            }

            Text {
                id: filesize_

                Layout.fillWidth: true
                Layout.maximumWidth: implicitWidth + 1
                text: evRoot.filesize
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: palette.text
            }
        }

        KomaiButton {
            text: qsTr("Save")
            icon.source: "qrc:/icons/icons/ui/download.svg"
            toolTipText: qsTr("Save file")

            onClicked: room.saveMedia(eventId)
        }

    }

    background: Rectangle {
        color: palette.alternateBase
        radius: fontMetrics.lineSpacing / 2 + 2 * Komai.paddingSmall
        visible: styleProfile.showFileMessageBackground
    }

}

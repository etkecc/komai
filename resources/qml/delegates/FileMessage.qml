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

    property var roomAdapter: null
    required property QtObject styleProfile
    required property string body
    required property string eventId
    required property string filename
    required property string filesize
    required property string fileTypeIconSource

    readonly property bool hasCaption: body.length > 0 && body !== filename

    padding: styleProfile.fileMessagePadding
    property int metadataWidth: 0
    property bool fitsMetadata: false
    readonly property var effectiveDelegateRoom: roomAdapter
        ? roomAdapter
        : (typeof effectiveRoomContext !== "undefined" && effectiveRoomContext)
        ? effectiveRoomContext
        : ((typeof room !== "undefined" && room) ? room : null)

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

            spacing: 0
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

                visible: evRoot.filesize.length > 0
                Layout.fillWidth: true
                Layout.maximumWidth: implicitWidth + 1
                text: evRoot.filesize
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: palette.text
            }

            Text {
                id: caption_

                visible: evRoot.hasCaption
                Layout.fillWidth: true
                text: evRoot.body
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: palette.buttonText
            }
        }

        KomaiButton {
            text: qsTr("Save")
            icon.source: "qrc:/icons/icons/ui/download.svg"
            toolTipText: qsTr("Save file")

            onClicked: {
                if (evRoot.effectiveDelegateRoom)
                    evRoot.effectiveDelegateRoom.saveMedia(eventId);
            }
        }

    }

    background: Rectangle {
        color: palette.alternateBase
        radius: fontMetrics.lineSpacing / 2 + 2 * Komai.paddingSmall
        visible: styleProfile.showFileMessageBackground
    }

}

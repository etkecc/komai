// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../ui"
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3
import im.nheko 1.0

Page {
    id: uploadPopup

    Layout.fillWidth: true
    Layout.minimumHeight: 0
    Layout.maximumHeight: visible ? 200 : 0
    Layout.preferredHeight: visible ? 200 : 0
    clip: true
    padding: Nheko.paddingMedium
    visible: room && room.input.uploads.length > 0

    background: Rectangle {
        color: palette.base
    }
    contentItem: ListView {
        id: uploadsList

        anchors.horizontalCenter: parent.horizontalCenter
        boundsBehavior: Flickable.StopAtBounds
        model: room ? room.input.uploads : undefined
        orientation: ListView.Horizontal
        spacing: Nheko.paddingMedium
        width: Math.min(contentWidth, parent.availableWidth)

        ScrollBar.horizontal: ScrollBar {
            id: scr

        }
        delegate: Pane {
            id: pane

            height: uploadPopup.availableHeight - buttons.height - (scr.visible ? scr.height : 0)
            padding: Nheko.paddingSmall
            width: uploadPopup.availableHeight - buttons.height

            background: Rectangle {
                color: palette.window
                radius: Nheko.paddingMedium
            }
            contentItem: ColumnLayout {
                Image {
                    property string fallbackIconSource: switch (modelData.mediaType) {
                    case MediaUpload.Video:
                        return "image://colorimage/:/icons/icons/ui/video-file.svg?" + palette.buttonText;
                    case MediaUpload.Audio:
                        return "image://colorimage/:/icons/icons/ui/music.svg?" + palette.buttonText;
                    case MediaUpload.Image:
                        return "image://colorimage/:/icons/icons/ui/image.svg?" + palette.buttonText;
                    default:
                        return "image://colorimage/:/icons/icons/ui/zip.svg?" + palette.buttonText;
                    }

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                    smooth: true
                    source: (modelData.thumbnail != "") ? modelData.thumbnail : fallbackIconSource
                    sourceSize.height: pane.availableHeight - namefield.height
                    sourceSize.width: pane.availableWidth
                }
                MatrixTextField {
                    id: namefield

                    Layout.fillWidth: true
                    text: modelData.filename

                    onTextEdited: modelData.filename = text
                }
            }
        }
    }
    footer: DialogButtonBox {
        id: buttons

        standardButtons: DialogButtonBox.Cancel

        onAccepted: room.input.acceptUploads()
        onRejected: room.input.declineUploads()

        Button {
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            text: qsTr("Upload %n file(s)", "", (room ? room.input.uploads.length : 0))
        }
    }
}

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import "../ui"
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3
import QtQuick.Window 2.15
import Qt5Compat.GraphicalEffects
import cc.etke.komai 1.0

Rectangle {
    id: uploadPopup

    readonly property bool layoutVisible: room && room.input.uploads.length > 0
    readonly property int uploadCount: room ? room.input.uploads.length : 0
    property int headerTextHeight: Math.round(Komai.fontPixelSize * 2.4)
    property int headerIconSize: Math.ceil(headerTextHeight * 0.5)
    property int headerFontSize: Math.ceil(headerTextHeight * 0.45)
    readonly property int previewSize: Komai.listIconSize
    // Show "Remove" label when the row is wide enough.
    readonly property bool showRemoveLabel: uploadsList.width > previewSize * 6

    Layout.fillWidth: true
    Layout.minimumHeight: 0
    Layout.maximumHeight: layoutVisible ? implicitHeight : 0
    Layout.preferredHeight: layoutVisible ? implicitHeight : 0
    clip: true
    visible: layoutVisible
    implicitHeight: layoutVisible ? contentColumn.implicitHeight + Komai.paddingMedium * 2 : 0
    color: palette.alternateBase
    radius: 8

    // Mask the bottom rounded corners so it sits flush against the input below.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: parent.radius
        color: parent.color
    }

    Column {
        id: contentColumn

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Komai.paddingMedium
        spacing: Komai.paddingSmall

        // ── Attachments header ──
        RowLayout {
            spacing: Komai.paddingSmall
            width: parent.width

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: uploadPopup.headerIconSize
                Layout.preferredWidth: uploadPopup.headerIconSize
                source: "image://colorimage/:/icons/icons/ui/attach.svg?" + palette.text
            }

            Label {
                color: palette.text
                font.pixelSize: uploadPopup.headerFontSize
                font.bold: true
                text: qsTr("Attachments")
            }

            Item {
                Layout.fillWidth: true
            }

            ImageButton {
                ToolTip.delay: Komai.tooltipDelay
                ToolTip.text: qsTr("Remove all attachments")
                ToolTip.visible: hovered
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: uploadPopup.headerIconSize
                Layout.preferredWidth: uploadPopup.headerIconSize
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"

                onClicked: room.input.declineUploads()
            }
        }

        // ── File list (vertical) ──
        ListView {
            id: uploadsList

            width: parent.width
            height: Math.min(contentHeight, uploadPopup.previewSize * 5)
            boundsBehavior: Flickable.StopAtBounds
            model: room ? room.input.uploads : undefined
            orientation: ListView.Vertical
            spacing: Komai.paddingSmall
            clip: true

            ScrollBar.vertical: ScrollBar {}

            delegate: RowLayout {
                width: uploadsList.width
                height: uploadPopup.previewSize + Komai.paddingSmall * 2
                spacing: Komai.paddingSmall

                // ── Preview thumbnail ──
                Item {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: uploadPopup.previewSize
                    Layout.preferredHeight: uploadPopup.previewSize
                    layer.enabled: true
                    layer.effect: OpacityMask {
                        maskSource: Rectangle {
                            width: uploadPopup.previewSize
                            height: uploadPopup.previewSize
                            radius: Komai.paddingSmall
                        }
                    }

                    Image {
                        id: previewImage

                        function fileTypeIcon(mediaType, mime) {
                            var icon;
                            switch (mediaType) {
                            case MediaUpload.Video:
                                icon = "video-file"; break;
                            case MediaUpload.Audio:
                                icon = "music"; break;
                            case MediaUpload.Image:
                                icon = "image"; break;
                            default:
                                if (mime === "application/pdf")
                                    icon = "document-pdf";
                                else if (mime.startsWith("text/plain"))
                                    icon = "document-text";
                                else if (mime.startsWith("text/") || mime === "application/json"
                                         || mime === "application/xml" || mime === "application/javascript")
                                    icon = "code";
                                else if (mime.indexOf("spreadsheet") >= 0 || mime === "text/csv")
                                    icon = "table-simple";
                                else if (mime.indexOf("presentation") >= 0)
                                    icon = "slide-content";
                                else
                                    icon = "document-data";
                            }
                            return "image://colorimage/:/icons/icons/ui/" + icon + ".svg?" + palette.buttonText;
                        }
                        property string fallbackIconSource: fileTypeIcon(modelData.mediaType, modelData.mimetype)

                        anchors.fill: parent
                        fillMode: Image.PreserveAspectFit
                        mipmap: true
                        smooth: true
                        source: (modelData.thumbnail != "") ? modelData.thumbnail : fallbackIconSource
                        sourceSize.height: parent.height * Screen.devicePixelRatio
                        sourceSize.width: parent.width * Screen.devicePixelRatio
                    }
                }

                // ── Filename input ──
                MatrixTextField {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillWidth: true
                    Layout.leftMargin: Komai.paddingMedium
                    Layout.rightMargin: Komai.paddingMedium
                    text: modelData.filename

                    onTextEdited: modelData.filename = text
                }

                // ── Remove button ──
                KomaiButton {
                    Layout.alignment: Qt.AlignVCenter
                    text: uploadPopup.showRemoveLabel ? qsTr("Remove") : ""
                    icon.source: "qrc:/icons/icons/ui/delete.svg"
                    ToolTip.delay: Komai.tooltipDelay
                    ToolTip.text: qsTr("Remove")
                    ToolTip.visible: hovered && !uploadPopup.showRemoveLabel

                    onClicked: room.input.removeUpload(index)
                }
            }
        }

        // ── Multi-file warning ──
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Note: each file is sent as a separate message.")
            color: Komai.theme.warning
            visible: uploadPopup.uploadCount > 1
            font.pointSize: Settings.uiFontSizePt * 0.9
        }
    }
}

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import cc.etke.komai

ColumnLayout {
    id: evRoot

    property var roomAdapter: null
    required property QtObject styleProfile
    required property string body
    required property string formattedBody
    required property string eventId
    required property string filename
    required property string filesize
    required property string fileTypeIconSource

    readonly property bool hasCaption: body.length > 0 && body !== filename
    readonly property bool useFormattedCaption: hasCaption && formattedBody.length > 0

    property int metadataWidth: 0
    property bool fitsMetadata: false
    readonly property var effectiveDelegateRoom: roomAdapter
        ? roomAdapter
        : (typeof effectiveRoomContext !== "undefined" && effectiveRoomContext)
        ? effectiveRoomContext
        : ((typeof room !== "undefined" && room) ? room : null)

    spacing: Komai.paddingSmall

    Control {
        id: fileWidget

        Layout.alignment: Qt.AlignLeft
        Layout.maximumWidth: rowa.Layout.maximumWidth + padding * 2

        padding: evRoot.styleProfile.fileMessagePadding

        contentItem: RowLayout {
            id: rowa

            spacing: Komai.paddingMedium * 2

            Rectangle {
                id: iconCircle

                readonly property int circleSize: Komai.iconSize + 2 * Komai.paddingMedium

                color: palette.light
                radius: circleSize / 2
                Layout.preferredHeight: circleSize
                Layout.preferredWidth: circleSize

                Image {
                    id: img

                    height: Komai.iconSize
                    width: Komai.iconSize
                    sourceSize.height: Komai.iconSize * Screen.devicePixelRatio
                    sourceSize.width: Komai.iconSize * Screen.devicePixelRatio

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
            }

            KomaiButton {
                text: qsTr("Save")
                icon.source: "qrc:/icons/icons/ui/download.svg"
                toolTipText: qsTr("Save file")

                onClicked: {
                    if (evRoot.effectiveDelegateRoom)
                        evRoot.effectiveDelegateRoom.saveMedia(evRoot.eventId);
                }
            }
        }

        background: Rectangle {
            color: palette.alternateBase
            radius: fontMetrics.lineSpacing / 2 + 2 * Komai.paddingSmall
            visible: evRoot.styleProfile.showFileMessageBackground
        }
    }

    TextEdit {
        id: caption_

        property point hoverPoint: Qt.point(0, 0)

        visible: evRoot.hasCaption
        Layout.fillWidth: true
        readOnly: true
        selectByMouse: true
        selectionColor: palette.highlight
        selectedTextColor: palette.highlightedText
        wrapMode: TextEdit.Wrap
        textFormat: evRoot.useFormattedCaption ? TextEdit.RichText : TextEdit.PlainText
        text: evRoot.useFormattedCaption ? evRoot.formattedBody : evRoot.body
        color: palette.text

        onLinkActivated: (link) => Komai.openLink(link)

        HoverHandler {
            cursorShape: caption_.hoveredLink.length > 0
                ? Qt.PointingHandCursor
                : Qt.IBeamCursor
            onPointChanged: if (hovered)
                caption_.hoverPoint = Qt.point(point.position.x, point.position.y)
        }

        Loader {
            active: caption_.hoveredLink.length > 0
            sourceComponent: Component {
                Item {
                    TextMetrics {
                        id: linkMetrics
                        text: Komai.punyLink(caption_.hoveredLink)
                    }
                    KomaiToolTip {
                        anchorItem: caption_
                        anchorX: caption_.hoverPoint.x
                        anchorY: caption_.hoverPoint.y
                        gapX: Komai.paddingMedium
                        gapY: Komai.paddingMedium
                        text: linkMetrics.text
                        requestedVisible: caption_.hoveredLink.length > 0
                        width: Math.min(linkMetrics.advanceWidth + leftPadding + rightPadding, 500)
                    }
                }
            }
        }
    }
}

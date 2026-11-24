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
        // Shrink to the available width (eliding the filename) instead of
        // overflowing the bubble and pushing the Save button out of view.
        Layout.fillWidth: true
        Layout.maximumWidth: implicitWidth

        // The thresholds and the implicit width below must not depend on the
        // layout they select, or the delegate would keep re-laying itself out.
        readonly property int regularPadding: evRoot.styleProfile.fileMessagePadding
        readonly property int compactPadding: Math.min(regularPadding, Komai.paddingMedium)
        readonly property int regularSpacing: Komai.paddingMedium * 2
        readonly property real saveIconWidth: saveButton.leftPadding + saveButton.effectiveIconSize + saveButton.rightPadding
        readonly property real saveFullWidth: saveIconWidth + saveButton.spacing + Math.ceil(saveMetrics.advanceWidth)
        readonly property real labelsWidth: Math.ceil(Math.max(filename_.implicitWidth, filesize_.visible ? filesize_.implicitWidth : 0)) + 1
        readonly property real regularWidthWithoutLabels: 2 * regularPadding + iconCircle.circleSize + saveFullWidth + 2 * regularSpacing
        readonly property real compactInnerWidth: evRoot.width - 2 * compactPadding

        // Too narrow for a readable file name between the icon and the button:
        // move the name to a row of its own.
        readonly property bool compact: evRoot.width < regularWidthWithoutLabels
            + Math.min(labelsWidth, 12 * fontMetrics.averageCharacterWidth)
        readonly property bool showSaveLabel: !compact
            || compactInnerWidth >= iconCircle.circleSize + Komai.paddingSmall + saveFullWidth
        readonly property bool showIcon: !compact
            || compactInnerWidth >= iconCircle.circleSize + Komai.paddingSmall + saveIconWidth

        implicitWidth: regularWidthWithoutLabels + labelsWidth
        padding: compact ? compactPadding : regularPadding

        TextMetrics {
            id: saveMetrics

            font: saveButton.font
            text: saveButton.text
        }

        contentItem: GridLayout {
            id: grid

            columnSpacing: fileWidget.compact ? Komai.paddingSmall : fileWidget.regularSpacing
            rowSpacing: Komai.paddingSmall

            Rectangle {
                id: iconCircle

                readonly property int circleSize: Komai.iconSize + 2 * Komai.paddingMedium

                visible: fileWidget.showIcon
                color: palette.light
                radius: circleSize / 2
                Layout.row: 0
                Layout.column: 0
                Layout.preferredHeight: circleSize
                Layout.preferredWidth: circleSize

                Image {
                    id: img

                    height: Komai.iconSize
                    width: Komai.iconSize
                    sourceSize.height: Komai.iconSize * Komai.devicePixelRatio
                    sourceSize.width: Komai.iconSize * Komai.devicePixelRatio

                    anchors.centerIn: parent
                    source: "image://colorimage/" + evRoot.fileTypeIconSource + "?" + palette.buttonText
                    fillMode: Image.PreserveAspectFit
                }
            }

            ColumnLayout {
                id: col

                spacing: 0
                Layout.row: fileWidget.compact ? 1 : 0
                Layout.column: fileWidget.compact ? 0 : 1
                Layout.columnSpan: fileWidget.compact ? 2 : 1
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
                id: saveButton

                Layout.row: 0
                Layout.column: fileWidget.compact ? 1 : 2
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                display: fileWidget.showSaveLabel ? AbstractButton.TextBesideIcon : AbstractButton.IconOnly
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

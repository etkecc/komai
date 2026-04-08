// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai 1.0

// This class is for showing Reactions in the timeline row, not for
// adding new reactions via the emoji picker
Flow {
    id: reactionFlow

    property string eventId
    property var roomModel: null
    readonly property var effectiveRoomModel: roomModel ? roomModel : (typeof room !== "undefined" ? room : null)

    // lower-contrast colors to avoid distracting from text & to enhance hover effect
    property color gentleHighlight: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.35)
    property color gentleText: Qt.hsla(palette.text.hslHue, palette.text.hslSaturation, palette.text.hslLightness, 0.6)
    property alias reactions: repeater.model

    spacing: 4

    Repeater {
        id: repeater

        delegate: AbstractButton {
            id: reaction

            readonly property bool reactionDisplayKeyElided: textMetrics.advanceWidth > textMetrics.elideWidth
            readonly property string reactionToolTipText: reactionDisplayKeyElided
                ? modelData.users
                : (modelData.displayKey + "\n" + modelData.users)
            hoverEnabled: true
            leftPadding: textMetrics.height / 2
            rightPadding: textMetrics.height / 2

            KomaiToolTip {
                anchorItem: reaction
                anchorX: reaction.width / 2
                anchorY: 0
                maxWidth: 300
                text: reaction.reactionToolTipText
                delay: Komai.tooltipDelay
                requestedVisible: reaction.hovered && reaction.reactionToolTipText.length > 0
            }

            background: Rectangle {
                anchors.centerIn: parent
                border.color: reaction.hovered ? palette.dark : (modelData.selfReactedEvent !== '' ? palette.highlight : gentleText)
                border.width: 1
                color: reaction.hovered ? palette.dark : (modelData.selfReactedEvent !== '' ? gentleHighlight : palette.window)
                implicitHeight: reaction.implicitHeight
                implicitWidth: reaction.implicitWidth
                radius: Komai.paddingMedium
            }
            contentItem: Row {
                spacing: textMetrics.height / 4

                TextMetrics {
                    id: textMetrics

                    elide: Text.ElideRight
                    elideWidth: 150
                    font.family: Settings.uiFontEmojiFamily
                    text: modelData.displayKey
                }
                Text {
                    id: reactionText

                    anchors.baseline: reactionCounter.baseline
                    color: reaction.hovered ? palette.brightText : palette.text
                    font.family: Settings.uiFontEmojiFamily
                    font.pixelSize: 24
                    textFormat: Text.StyledText
                    maximumLineCount: 1
                    text: {
                        // When an emoji font is selected that doesn't have …, it is dropped from elidedText. So we add it back.
                        if (textMetrics.elidedText !== modelData.displayKey) {
                            if (!textMetrics.elidedText.endsWith("…")) {
                                return textMetrics.elidedText + "…";
                            }
                        }
                        return textMetrics.elidedText;
                    }
                    visible: !modelData.key.startsWith("mxc://")
                }
                Image {
                    anchors.verticalCenter: divider.verticalCenter
                    fillMode: Image.PreserveAspectFit
                    height: textMetrics.height
                    mipmap: true
                    source: modelData.key.startsWith("mxc://") ? (modelData.key.replace("mxc://", "image://MxcImage/") + "?scale") : ""
                    visible: modelData.key.startsWith("mxc://")
                    width: textMetrics.height
                }
                Rectangle {
                    id: divider

                    color: reaction.hovered ? palette.brightText : gentleText
                    height: Math.floor(reactionCounter.implicitHeight * 1.4)
                    width: 1
                }
                Text {
                    id: reactionCounter

                    anchors.verticalCenter: divider.verticalCenter
                    color: reaction.hovered ? palette.brightText : palette.windowText
                    font: reaction.font
                    text: modelData.count
                }
            }

            onClicked: {
                if (reactionFlow.effectiveRoomModel && reactionFlow.effectiveRoomModel.input)
                    reactionFlow.effectiveRoomModel.input.reaction(reactionFlow.eventId, modelData.key);
                else
                    TimelineManager.toggleActiveMatrixTimelineReaction(String(reactionFlow.eventId || ""), String(modelData.key || ""));
            }

            KomaiCursorShape {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
            }
        }
    }
}

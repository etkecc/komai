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

    // Beyond this many distinct reactions we collapse the rest behind a "+N"
    // pill that opens the reaction-details dialog. Picked empirically: enough
    // to cover the vast majority of messages without letting reactions take
    // over the bubble.
    property int visibleCap: 10

    // Emitted when the user clicks the "+N" pill. The host wires this to
    // chatRoot.openReactionDetailsDialog(eventId).
    signal reactionDetailsRequested(string eventId)

    // lower-contrast colors to avoid distracting from text & to enhance hover effect
    property color gentleHighlight: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.35)
    property color gentleText: Qt.hsla(palette.text.hslHue, palette.text.hslSaturation, palette.text.hslLightness, 0.6)
    property alias reactions: repeater.model

    readonly property int reactionCount: reactions ? reactions.length : 0
    readonly property int hiddenReactionCount: Math.max(0, reactionCount - visibleCap)

    // Width Reactions would need to render every visible pill side-by-side in
    // a single row. Exposed so callers that size this Flow without `fillWidth`
    // (e.g. the right-aligned bubble path in TimelineBubbleMessageStyle) have
    // a stable preferred-size hint: Qt's Flow.implicitWidth is computed from
    // the laid-out rows (max row width) and recursively collapses to a single
    // pill's width when the Flow is given less space than its natural width,
    // which makes it useless as a Layout.preferredWidth fallback.
    readonly property real naturalContentWidth: {
        // Touch reactive inputs so the binding re-runs when the model or font
        // changes; itemAt(i).implicitWidth itself isn't tracked, but items
        // are recreated when the model changes (count flips), and font
        // changes propagate through Settings.
        const _count = repeater.count;
        const _font = Settings.uiFontSizePt;
        const _morePillVisible = morePill.visible;
        let total = 0;
        let visibleItems = 0;
        for (let i = 0; i < repeater.count; i++) {
            const item = repeater.itemAt(i);
            if (item && item.visible) {
                if (visibleItems > 0)
                    total += spacing;
                total += item.implicitWidth;
                visibleItems += 1;
            }
        }
        if (_morePillVisible) {
            if (visibleItems > 0)
                total += spacing;
            total += morePill.implicitWidth;
        }
        return total;
    }

    spacing: 4

    Repeater {
        id: repeater

        delegate: AbstractButton {
            id: reaction

            readonly property bool reactionDisplayKeyElided: textMetrics.advanceWidth > textMetrics.elideWidth
            readonly property string reactionToolTipText: reactionDisplayKeyElided
                ? modelData.users
                : (modelData.displayKey + "\n" + modelData.users)
            // Hide reactions past the visible cap; the "+N" pill below covers them.
            // Invisible items are excluded from Flow layout, so this just trims
            // the row without leaving gaps.
            visible: index < reactionFlow.visibleCap
            hoverEnabled: true
            leftPadding: textMetrics.height / 2
            rightPadding: textMetrics.height / 2
            topPadding: Komai.paddingSmall / 2
            bottomPadding: Komai.paddingSmall / 2

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
                anchors.fill: parent
                border.color: reaction.hovered ? palette.dark : (modelData.selfReactedEvent !== '' ? palette.highlight : gentleText)
                border.width: 1
                color: reaction.hovered ? palette.dark : (modelData.selfReactedEvent !== '' ? gentleHighlight : palette.window)
                radius: Komai.paddingMedium
            }
            contentItem: Row {
                spacing: textMetrics.height / 4

                TextMetrics {
                    id: textMetrics

                    elide: Text.ElideRight
                    elideWidth: 150
                    font.family: Settings.uiFontEmojiFamily
                    font.pointSize: Settings.uiFontSizePt
                    text: modelData.displayKey
                }
                Text {
                    id: reactionText

                    anchors.verticalCenter: parent.verticalCenter
                    color: reaction.hovered ? palette.brightText : palette.text
                    font.family: Settings.uiFontEmojiFamily
                    font.pointSize: Settings.uiFontSizePt * 1.5
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
                    font.pointSize: Settings.uiFontSizePt
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

    // "+N" overflow pill. Intentionally shaped like neither a reaction nor a
    // self-reacted reaction (no emoji, no "emoji | counter" divider, neutral
    // background) so it reads as a control, not as a reaction.
    //
    // Padding/font sizing mirrors the regular reaction pill so the heights and
    // line-up match: side padding scales off the same 1x emoji-font metric;
    // content font matches the dominant `reactionText` size (1.5x).
    AbstractButton {
        id: morePill

        visible: reactionFlow.hiddenReactionCount > 0
        hoverEnabled: true
        leftPadding: morePillSizeMetrics.height / 2
        rightPadding: morePillSizeMetrics.height / 2
        topPadding: Komai.paddingSmall / 2
        bottomPadding: Komai.paddingSmall / 2

        TextMetrics {
            id: morePillSizeMetrics

            font.family: Settings.uiFontEmojiFamily
            font.pointSize: Settings.uiFontSizePt
            text: "+"
        }

        KomaiToolTip {
            anchorItem: morePill
            anchorX: morePill.width / 2
            anchorY: 0
            maxWidth: 300
            text: qsTr("Show all reactions")
            delay: Komai.tooltipDelay
            requestedVisible: morePill.hovered
        }

        background: Rectangle {
            anchors.fill: parent
            border.color: morePill.hovered ? palette.dark : reactionFlow.gentleText
            border.width: 1
            color: morePill.hovered ? palette.dark : palette.window
            radius: Komai.paddingMedium
        }
        contentItem: Text {
            color: morePill.hovered ? palette.brightText : palette.windowText
            font.family: Settings.uiFontEmojiFamily
            font.pointSize: Settings.uiFontSizePt * 1.5
            text: "+" + reactionFlow.hiddenReactionCount
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }

        onClicked: reactionFlow.reactionDetailsRequested(String(reactionFlow.eventId || ""))

        KomaiCursorShape {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
        }
    }
}

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import ".."
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Control {
    id: root

    property Item attached: null
    property Item anchorItem: null
    property var model: null
    property bool pinned: false
    property bool positioned: false

    function dismiss() {
        pinned = false;
        attached = null;
        anchorItem = null;
        positioned = false;
    }

    hoverEnabled: true
    padding: Komai.paddingSmall
    visible: Settings.timelineMessageActionsActivationPolicy !== Settings.TimelineMessageActionsActivationPolicy.Never && !!attached && (pinned || Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.OnHover)
    z: 10

    background: Rectangle {
        border.color: palette.buttonText
        border.width: 1
        color: palette.window
        radius: root.padding
    }

    contentItem: RowLayout {
        id: actionRow

        property int itemPadding: Math.round(root.padding / 2)
        property var pinnedReactions: Settings.timelineMessageActionsPinnedReactions
            .split(",")
            .map(function (s) { return s.trim(); })
            .filter(function (s) { return s.length > 0; })
            .slice(0, 10)

        spacing: 0

        Repeater {
            model: actionRow.pinnedReactions

            delegate: ToolButton {
                id: pinnedReactionButton

                required property string modelData

                Layout.preferredHeight: 32
                Layout.preferredWidth: Math.max(32, emojiLabel.implicitWidth + 2 * actionRow.itemPadding)
                focusPolicy: Qt.NoFocus
                hoverEnabled: true
                leftPadding: actionRow.itemPadding
                rightPadding: actionRow.itemPadding

                onClicked: root.dismiss()
                onHoveredChanged: {
                    if (hovered)
                        pinnedReactionPulse.pulse();
                }

                contentItem: Label {
                    id: emojiLabel

                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: modelData
                    font.pixelSize: 24
                    font.family: Settings.uiFontEmojiFamily
                }

                HoverPulseAnimation {
                    id: pinnedReactionPulse

                    targetItem: pinnedReactionButton
                }
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            Layout.leftMargin: Komai.paddingSmall
            Layout.rightMargin: Komai.paddingSmall
            color: palette.mid
        }

        ToolButton {
            id: reactButton

            ToolTip.delay: Komai.tooltipDelay
            ToolTip.text: qsTr("React")
            ToolTip.visible: hovered
            focusPolicy: Qt.NoFocus
            hoverEnabled: true
            text: "\u263A"
            onClicked: root.dismiss()
            onHoveredChanged: {
                if (hovered)
                    reactPulse.pulse();
            }

            HoverPulseAnimation {
                id: reactPulse

                targetItem: reactButton
            }
        }

        ToolButton {
            id: editButton

            ToolTip.delay: Komai.tooltipDelay
            ToolTip.text: qsTr("Edit")
            ToolTip.visible: hovered
            focusPolicy: Qt.NoFocus
            hoverEnabled: true
            text: "\u270E"
            visible: !!root.model && root.model.isEditable
            onClicked: root.dismiss()
            onHoveredChanged: {
                if (hovered)
                    editPulse.pulse();
            }

            HoverPulseAnimation {
                id: editPulse

                targetItem: editButton
            }
        }

        ToolButton {
            id: replyButton

            ToolTip.delay: Komai.tooltipDelay
            ToolTip.text: qsTr("Reply")
            ToolTip.visible: hovered
            focusPolicy: Qt.NoFocus
            hoverEnabled: true
            text: "\u21A9"
            onClicked: root.dismiss()
            onHoveredChanged: {
                if (hovered)
                    replyPulse.pulse();
            }

            HoverPulseAnimation {
                id: replyPulse

                targetItem: replyButton
            }
        }

        ToolButton {
            id: optionsButton

            ToolTip.delay: Komai.tooltipDelay
            ToolTip.text: qsTr("Options")
            ToolTip.visible: hovered
            focusPolicy: Qt.NoFocus
            hoverEnabled: true
            text: "\u22EF"
            onClicked: root.dismiss()
            onHoveredChanged: {
                if (hovered)
                    optionsPulse.pulse();
            }

            HoverPulseAnimation {
                id: optionsPulse

                targetItem: optionsButton
            }
        }
    }
}

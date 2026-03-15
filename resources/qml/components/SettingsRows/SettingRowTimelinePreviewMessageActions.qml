// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../timeline/components" as TimelineComponents
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
    readonly property bool canEdit: !!root.model && root.model.isEditable
    property int itemHorizontalPadding: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
    property int itemVerticalPadding: 0
    property int actionButtonHeight: Komai.listIconSize
    property int actionButtonIconSize: Math.max(14, actionButtonHeight - 2 * itemHorizontalPadding)
    property int labelBreakpointWidth: 600
    readonly property real actionHostWidth: (root.parent && root.parent.width > 0) ? root.parent.width : width
    readonly property int separatorSlotWidth: Komai.paddingMedium * 2 + 1
    readonly property real reactionButtonsWidth: repeaterItemsWidth(pinnedReactionsRepeater)
    readonly property real requiredLabeledWidth: reactionButtonsWidth
        + iconOnlyButtonWidth()
        + separatorSlotWidth
        + (canEdit ? labeledButtonWidth(editLabelMetrics.advanceWidth) : 0)
        + labeledButtonWidth(replyLabelMetrics.advanceWidth)
        + iconOnlyButtonWidth()
    readonly property bool showActionLabels: actionHostWidth >= labelBreakpointWidth
        && actionHostWidth >= requiredLabeledWidth
    readonly property color actionButtonColor: palette.buttonText
    readonly property color actionButtonActiveColor: palette.brightText
    readonly property color actionButtonHoverBackgroundColor: palette.dark

    function repeaterItemsWidth(repeater) {
        if (!repeater || repeater.count <= 0)
            return 0;

        let total = 0;
        for (let i = 0; i < repeater.count; i++) {
            const item = repeater.itemAt(i);
            if (!item || item.visible === false)
                continue;
            total += Math.max(item.implicitWidth || 0, item.width || 0);
        }
        return total;
    }

    function iconOnlyButtonWidth() {
        return actionButtonIconSize + itemHorizontalPadding * 2;
    }

    function labeledButtonWidth(labelAdvanceWidth) {
        return iconOnlyButtonWidth() + Komai.paddingSmall + labelAdvanceWidth;
    }

    function dismiss() {
        pinned = false;
        attached = null;
        anchorItem = null;
        positioned = false;
    }

    hoverEnabled: true
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    leftInset: 0
    rightInset: 0
    topInset: 0
    bottomInset: 0
    visible: Settings.timelineMessageActionsActivationPolicy !== Settings.TimelineMessageActionsActivationPolicy.Never && !!attached && (pinned || Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.OnHover)
    z: 10

    background: TimelineComponents.TimelineFloatingActionBarBackground {
        barColor: palette.alternateBase
        barRadius: Komai.paddingSmall
        barBorderColor: Komai.theme.separator
        barBorderWidth: 1
    }

    TextMetrics {
        id: editLabelMetrics

        font: Qt.font({
            "bold": true
        })
        text: qsTr("Edit")
    }

    TextMetrics {
        id: replyLabelMetrics

        font: Qt.font({
            "bold": true
        })
        text: qsTr("Reply")
    }

    contentItem: RowLayout {
        id: actionRow

        property int itemPadding: root.itemHorizontalPadding
        property var pinnedReactions: Settings.timelineMessageActionsPinnedReactions
            .split(",")
            .map(function (s) { return s.trim(); })
            .filter(function (s) { return s.length > 0; })
            .slice(0, 10)

        spacing: 0

        Repeater {
            id: pinnedReactionsRepeater

            model: actionRow.pinnedReactions

            delegate: ToolButton {
                id: pinnedReactionButton

                required property string modelData

                Layout.preferredHeight: root.actionButtonHeight
                Layout.preferredWidth: Math.max(root.actionButtonIconSize + 2 * actionRow.itemPadding, emojiLabel.implicitWidth + 2 * actionRow.itemPadding)
                focusPolicy: Qt.NoFocus
                hoverEnabled: true
                leftPadding: actionRow.itemPadding
                rightPadding: actionRow.itemPadding
                topPadding: root.itemVerticalPadding
                bottomPadding: root.itemVerticalPadding

                onClicked: root.dismiss()

                contentItem: Label {
                    id: emojiLabel

                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: modelData
                    color: pinnedReactionButton.hovered || pinnedReactionButton.pressed || pinnedReactionButton.visualFocus
                        ? root.actionButtonActiveColor
                        : root.actionButtonColor
                    font.pixelSize: root.actionButtonIconSize
                    font.family: Settings.uiFontEmojiFamily
                }
                background: Rectangle {
                    color: pinnedReactionButton.hovered || pinnedReactionButton.pressed || pinnedReactionButton.visualFocus
                        ? root.actionButtonHoverBackgroundColor
                        : "transparent"
                    radius: Komai.paddingSmall
                }
            }
        }

        TimelineComponents.MessageActionsLabeledButton {
            id: reactButton

            buttonTextColor: root.actionButtonColor
            hoverIconColor: root.actionButtonActiveColor
            hoverTextColor: root.actionButtonActiveColor
            contentHorizontalPadding: root.itemHorizontalPadding
            contentVerticalPadding: root.itemVerticalPadding
            hoverBackgroundColor: root.actionButtonHoverBackgroundColor
            buttonHeight: root.actionButtonHeight
            iconSize: root.actionButtonIconSize
            image: ":/icons/icons/ui/smile-add.svg"
            labelText: ""
            toolTipText: qsTr("React")
            onClicked: root.dismiss()
        }

        Item {
            Layout.preferredWidth: root.separatorSlotWidth
            Layout.preferredHeight: root.actionButtonHeight

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                color: Qt.rgba(root.actionButtonColor.r, root.actionButtonColor.g, root.actionButtonColor.b, 0.35)
                height: Math.max(1, parent.height - Komai.paddingMedium)
                width: 1
            }
        }

        TimelineComponents.MessageActionsLabeledButton {
            id: editButton

            buttonTextColor: root.actionButtonColor
            hoverIconColor: root.actionButtonActiveColor
            hoverTextColor: root.actionButtonActiveColor
            contentHorizontalPadding: root.itemHorizontalPadding
            contentVerticalPadding: root.itemVerticalPadding
            hoverBackgroundColor: root.actionButtonHoverBackgroundColor
            buttonHeight: root.actionButtonHeight
            iconSize: root.actionButtonIconSize
            image: ":/icons/icons/ui/edit.svg"
            labelText: root.showActionLabels ? qsTr("Edit") : ""
            toolTipText: qsTr("Edit")
            visible: root.canEdit
            onClicked: root.dismiss()
        }

        TimelineComponents.MessageActionsLabeledButton {
            id: replyButton

            buttonTextColor: root.actionButtonColor
            hoverIconColor: root.actionButtonActiveColor
            hoverTextColor: root.actionButtonActiveColor
            contentHorizontalPadding: root.itemHorizontalPadding
            contentVerticalPadding: root.itemVerticalPadding
            hoverBackgroundColor: root.actionButtonHoverBackgroundColor
            buttonHeight: root.actionButtonHeight
            iconSize: root.actionButtonIconSize
            image: ":/icons/icons/ui/reply.svg"
            labelText: root.showActionLabels ? qsTr("Reply") : ""
            toolTipText: qsTr("Reply")
            onClicked: root.dismiss()
        }

        TimelineComponents.MessageActionsLabeledButton {
            id: optionsButton

            buttonTextColor: root.actionButtonColor
            hoverIconColor: root.actionButtonActiveColor
            hoverTextColor: root.actionButtonActiveColor
            contentHorizontalPadding: root.itemHorizontalPadding
            contentVerticalPadding: root.itemVerticalPadding
            hoverBackgroundColor: root.actionButtonHoverBackgroundColor
            buttonHeight: root.actionButtonHeight
            iconSize: root.actionButtonIconSize
            image: ":/icons/icons/ui/options-circle.svg"
            labelText: ""
            toolTipText: qsTr("Options")
            onClicked: root.dismiss()
        }
    }
}

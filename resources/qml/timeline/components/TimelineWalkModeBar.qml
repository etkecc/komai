// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Rectangle {
    id: walkBar

    required property var chatRoot
    required property var roomModel
    property int minimumHeight: Math.max(48, Komai.navigationRowHeight)
    readonly property var primaryMessage: chatRoot.primaryActionMessageInfo()
    readonly property bool showActionLabels: width >= 1180
    readonly property bool hasPrimaryMessage: !!primaryMessage
    readonly property bool hasSingleActionTarget: chatRoot.selectedCount <= 1 && hasPrimaryMessage
    readonly property bool canReply: hasSingleActionTarget && messageActionSupport.canReply(primaryMessage, roomModel)
    readonly property bool canThread: hasSingleActionTarget && messageActionSupport.canThread(primaryMessage, roomModel)
    readonly property bool canEdit: hasSingleActionTarget && messageActionSupport.canEdit(primaryMessage, roomModel)
    readonly property bool canForward: hasSingleActionTarget && messageActionSupport.canForward(primaryMessage)
    readonly property bool canRemove: hasSingleActionTarget && messageActionSupport.canRemove(primaryMessage, roomModel)
    readonly property bool canOpenOptions: hasSingleActionTarget
    readonly property bool canClearSelection: chatRoot.selectedCount > 0
    readonly property int headerButtonHeight: Komai.listIconSize
    readonly property int verticalMargin: Math.max(0, Math.floor((minimumHeight - headerButtonHeight) / 2))

    function statusText() {
        if (chatRoot.selectedCount > 1)
            return qsTr("%n selected messages", "", chatRoot.selectedCount);
        if (chatRoot.selectedCount === 1)
            return qsTr("1 selected message");
        return qsTr("Selection mode");
    }

    implicitHeight: minimumHeight
    color: palette.alternateBase

    MessageActionSupport {
        id: messageActionSupport
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Komai.paddingMedium
        anchors.rightMargin: Komai.paddingMedium
        anchors.topMargin: walkBar.verticalMargin
        anchors.bottomMargin: walkBar.verticalMargin
        spacing: Komai.paddingSmall

        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            color: palette.text
            elide: Text.ElideRight
            font.bold: true
            text: walkBar.statusText()
            verticalAlignment: Text.AlignVCenter
        }
        RowLayout {
            spacing: 0

            TimelineWalkModeActionButton {
                buttonHeight: walkBar.headerButtonHeight
                chatRoot: walkBar.chatRoot
                alwaysShowToolTip: true
                image: ":/icons/icons/ui/keyboard-shortcut.svg"
                labelText: qsTr("Help")
                showLabel: true
                toolTipText: qsTr("Show selection mode help [?]")

                onClicked: walkBar.chatRoot.openWalkModeHelpDialog()
            }
            TimelineWalkModeActionButton {
                buttonHeight: walkBar.headerButtonHeight
                chatRoot: walkBar.chatRoot
                enabled: walkBar.canReply
                image: ":/icons/icons/ui/reply.svg"
                labelText: qsTr("Reply")
                showLabel: walkBar.showActionLabels
                toolTipText: qsTr("Reply to message [R]")

                onClicked: walkBar.chatRoot.performWalkModeAction("reply")
            }
            TimelineWalkModeActionButton {
                buttonHeight: walkBar.headerButtonHeight
                chatRoot: walkBar.chatRoot
                enabled: walkBar.canThread
                image: ":/icons/icons/ui/thread.svg"
                labelText: qsTr("Thread")
                showLabel: walkBar.showActionLabels
                toolTipText: qsTr("Open or continue a thread [T]")

                onClicked: walkBar.chatRoot.performWalkModeAction("thread")
            }
            TimelineWalkModeActionButton {
                buttonHeight: walkBar.headerButtonHeight
                chatRoot: walkBar.chatRoot
                enabled: walkBar.canEdit
                image: ":/icons/icons/ui/edit.svg"
                labelText: qsTr("Edit")
                showLabel: walkBar.showActionLabels
                toolTipText: qsTr("Edit message [E]")

                onClicked: walkBar.chatRoot.performWalkModeAction("edit")
            }
            TimelineWalkModeActionButton {
                buttonHeight: walkBar.headerButtonHeight
                chatRoot: walkBar.chatRoot
                enabled: walkBar.canForward
                image: ":/icons/icons/ui/reply.svg"
                labelText: qsTr("Forward")
                showLabel: walkBar.showActionLabels
                mirrorIcon: true
                toolTipText: qsTr("Forward message [F]")

                onClicked: walkBar.chatRoot.performWalkModeAction("forward")
            }
            TimelineWalkModeActionButton {
                buttonHeight: walkBar.headerButtonHeight
                chatRoot: walkBar.chatRoot
                enabled: walkBar.canRemove
                image: ":/icons/icons/ui/delete.svg"
                labelText: qsTr("Delete message")
                showLabel: walkBar.showActionLabels
                toolTipText: qsTr("Delete message [D]")

                onClicked: walkBar.chatRoot.performWalkModeAction("remove")
            }
            TimelineWalkModeActionButton {
                buttonHeight: walkBar.headerButtonHeight
                chatRoot: walkBar.chatRoot
                enabled: walkBar.canOpenOptions
                image: ":/icons/icons/ui/options-circle.svg"
                labelText: qsTr("Options")
                showLabel: walkBar.showActionLabels
                toolTipText: qsTr("More message actions [O]")

                onClicked: walkBar.chatRoot.openPrimaryMessageActionsDialog()
            }
            TimelineWalkModeActionButton {
                buttonHeight: walkBar.headerButtonHeight
                chatRoot: walkBar.chatRoot
                enabled: walkBar.canClearSelection
                image: ":/icons/icons/ui/round-remove-button.svg"
                labelText: qsTr("Clear")
                showLabel: walkBar.showActionLabels
                toolTipText: qsTr("Clear explicit selection [Escape]")

                onClicked: {
                    walkBar.chatRoot.clearSelectedEvents();
                    walkBar.chatRoot.focusTimelineSelection();
                }
            }
            TimelineWalkModeActionButton {
                buttonHeight: walkBar.headerButtonHeight
                chatRoot: walkBar.chatRoot
                alwaysShowToolTip: true
                image: ":/icons/icons/ui/dismiss.svg"
                labelText: qsTr("Close")
                showLabel: walkBar.showActionLabels
                toolTipText: qsTr("Exit selection mode [I or Escape]")

                onClicked: walkBar.chatRoot.exitWalkMode({
                    "focusComposer": true
                })
            }
        }
    }
}

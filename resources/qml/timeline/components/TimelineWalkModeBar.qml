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
    property int minimumHeight: Math.max(48, Komai.navigationRowHeight)
    readonly property bool showActionLabels: width >= 1180
    // QML's binding engine does not reliably track property reads
    // inside deep forwarding chains.  These explicit dependencies
    // ensure re-evaluation when the focused event changes or a
    // delegate registers/unregisters after scroll.
    readonly property string _eid: chatRoot.primaryActionEventId
    readonly property int _rev: chatRoot.delegateRegistrationRevision
    readonly property bool canReply: _rev >= 0 && _eid.length > 0 && chatRoot.canPerformWalkModeAction("reply")
    readonly property bool canThread: _rev >= 0 && _eid.length > 0 && chatRoot.canPerformWalkModeAction("thread")
    readonly property bool canEdit: _rev >= 0 && _eid.length > 0 && chatRoot.canPerformWalkModeAction("edit")
    readonly property bool canForward: _rev >= 0 && (_eid.length > 0 || chatRoot.selectedCount > 1) && chatRoot.canPerformWalkModeAction("forward")
    readonly property bool canRemove: _rev >= 0 && (_eid.length > 0 || chatRoot.selectedCount > 1) && chatRoot.canPerformWalkModeAction("remove")
    readonly property bool canOpenOptions: _rev >= 0 && _eid.length > 0 && chatRoot.canPerformWalkModeAction("options")
    readonly property bool canClearSelection: chatRoot.selectedCount > 0
    readonly property int headerButtonHeight: Komai.listIconSize
    readonly property int separatorSlotWidth: Komai.paddingMedium * 2 + 1
    readonly property int verticalMargin: Math.max(0, Math.floor((minimumHeight - headerButtonHeight) / 2))

    function allButtons() {
        return [
            shortcutsButton,
            replyButton,
            threadButton,
            editButton,
            forwardButton,
            deleteButton,
            optionsButton,
            clearButton,
            closeButton
        ];
    }

    function addVisibleButton(buttons, button) {
        if (button && button.visible !== false && button.enabled !== false)
            buttons.push(button);
    }

    function visibleButtons() {
        const buttons = [];

        addVisibleButton(buttons, shortcutsButton);
        addVisibleButton(buttons, replyButton);
        addVisibleButton(buttons, threadButton);
        addVisibleButton(buttons, editButton);
        addVisibleButton(buttons, forwardButton);
        addVisibleButton(buttons, deleteButton);
        addVisibleButton(buttons, optionsButton);
        addVisibleButton(buttons, clearButton);
        addVisibleButton(buttons, closeButton);

        return buttons;
    }

    function activeButtonIndex() {
        const buttons = visibleButtons();
        let activeItem = walkBar.Window.activeFocusItem;

        for (let index = 0; index < buttons.length; index++) {
            let current = activeItem;
            while (current) {
                if (current === buttons[index])
                    return index;
                current = current.parent;
            }
        }

        return -1;
    }

    function focusFirstVisibleButton() {
        const buttons = visibleButtons();
        if (buttons.length === 0)
            return false;

        buttons[0].forceActiveFocus();
        return true;
    }

    function firstVisibleButtonItem() {
        const buttons = visibleButtons();
        return buttons.length > 0 ? buttons[0] : null;
    }

    function focusLastVisibleButton() {
        const buttons = visibleButtons();
        if (buttons.length === 0)
            return false;

        buttons[buttons.length - 1].forceActiveFocus();
        return true;
    }

    function lastVisibleButtonItem() {
        const buttons = visibleButtons();
        return buttons.length > 0 ? buttons[buttons.length - 1] : null;
    }

    function nextVisibleButton(button) {
        const buttons = visibleButtons();
        const index = buttons.indexOf(button);
        if (index < 0 || index >= buttons.length - 1)
            return null;

        return buttons[index + 1];
    }

    function previousVisibleButton(button) {
        const buttons = visibleButtons();
        const index = buttons.indexOf(button);
        if (index <= 0)
            return null;

        return buttons[index - 1];
    }

    function moveFocus(step) {
        const buttons = visibleButtons();
        if (buttons.length === 0)
            return false;

        const currentIndex = activeButtonIndex();
        if (currentIndex < 0) {
            if (step >= 0)
                return focusFirstVisibleButton();

            return focusLastVisibleButton();
        }

        const nextIndex = currentIndex + step;
        if (nextIndex < 0 || nextIndex >= buttons.length)
            return false;

        buttons[nextIndex].forceActiveFocus();
        return true;
    }

    function refreshButtonNavigationTargets() {
        const buttons = visibleButtons();
        const all = allButtons();

        for (let index = 0; index < all.length; index++) {
            const button = all[index];
            if (!button)
                continue;

            button.previousTabTarget = null;
            button.nextTabTarget = null;
        }

        for (let index = 0; index < buttons.length; index++) {
            buttons[index].previousTabTarget = index > 0
                ? buttons[index - 1]
                : (walkBar.chatRoot ? walkBar.chatRoot.timelineSelectionFocusTarget() : null);
            buttons[index].nextTabTarget = index < buttons.length - 1 ? buttons[index + 1] : null;
        }
    }

    function scheduleButtonNavigationTargetsRefresh() {
        Qt.callLater(refreshButtonNavigationTargets);
    }

    function statusText() {
        if (chatRoot.selectedCount > 1)
            return qsTr("%n selected messages", "", chatRoot.selectedCount);
        if (chatRoot.selectedCount === 1)
            return qsTr("1 selected message");
        return qsTr("Selection mode");
    }

    implicitHeight: minimumHeight
    color: palette.alternateBase

    Component.onCompleted: scheduleButtonNavigationTargetsRefresh()

    component GroupDivider: Item {
        Layout.alignment: Qt.AlignVCenter
        Layout.preferredWidth: walkBar.separatorSlotWidth
        Layout.preferredHeight: walkBar.headerButtonHeight
        implicitWidth: walkBar.separatorSlotWidth
        implicitHeight: walkBar.headerButtonHeight

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
            width: 1
            height: Math.max(1, parent.height - Komai.paddingMedium)
            color: Komai.theme.separator
        }
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

            RowLayout {
                spacing: 0

                TimelineWalkModeActionButton {
                    id: shortcutsButton

                    buttonHeight: walkBar.headerButtonHeight
                    chatRoot: walkBar.chatRoot
                    navigationHost: walkBar
                    alwaysShowToolTip: true
                    image: ":/icons/icons/ui/keyboard-shortcut.svg"
                    labelText: qsTr("Shortcuts")
                    nextTabTarget: walkBar.nextVisibleButton(shortcutsButton)
                    previousTabTarget: walkBar.previousVisibleButton(shortcutsButton)
                        || (walkBar.chatRoot ? walkBar.chatRoot.timelineSelectionFocusTarget() : null)
                    showLabel: true
                    toolTipText: qsTr("Show keyboard shortcuts [?]")

                    onClicked: walkBar.chatRoot.openWalkModeHelpDialog()
                }
            }

            GroupDivider {
            }

            RowLayout {
                spacing: 0

                TimelineWalkModeActionButton {
                    id: replyButton

                    buttonHeight: walkBar.headerButtonHeight
                    chatRoot: walkBar.chatRoot
                    navigationHost: walkBar
                    enabled: walkBar.canReply
                    image: ":/icons/icons/ui/reply.svg"
                    labelText: qsTr("Reply")
                    nextTabTarget: walkBar.nextVisibleButton(replyButton)
                    previousTabTarget: walkBar.previousVisibleButton(replyButton)
                        || (walkBar.chatRoot ? walkBar.chatRoot.timelineSelectionFocusTarget() : null)
                    showLabel: walkBar.showActionLabels
                    toolTipText: qsTr("Reply to message [R]")

                    onClicked: walkBar.chatRoot.performWalkModeAction("reply")
                }
                TimelineWalkModeActionButton {
                    id: threadButton

                    buttonHeight: walkBar.headerButtonHeight
                    chatRoot: walkBar.chatRoot
                    navigationHost: walkBar
                    enabled: walkBar.canThread
                    image: ":/icons/icons/ui/thread.svg"
                    labelText: qsTr("Thread")
                    nextTabTarget: walkBar.nextVisibleButton(threadButton)
                    previousTabTarget: walkBar.previousVisibleButton(threadButton)
                        || (walkBar.chatRoot ? walkBar.chatRoot.timelineSelectionFocusTarget() : null)
                    showLabel: walkBar.showActionLabels
                    toolTipText: qsTr("Open or continue a thread [T]")

                    onClicked: walkBar.chatRoot.performWalkModeAction("thread")
                }
                TimelineWalkModeActionButton {
                    id: editButton

                    buttonHeight: walkBar.headerButtonHeight
                    chatRoot: walkBar.chatRoot
                    navigationHost: walkBar
                    enabled: walkBar.canEdit
                    image: ":/icons/icons/ui/edit.svg"
                    labelText: qsTr("Edit")
                    nextTabTarget: walkBar.nextVisibleButton(editButton)
                    previousTabTarget: walkBar.previousVisibleButton(editButton)
                        || (walkBar.chatRoot ? walkBar.chatRoot.timelineSelectionFocusTarget() : null)
                    showLabel: walkBar.showActionLabels
                    toolTipText: qsTr("Edit message [E]")

                    onClicked: walkBar.chatRoot.performWalkModeAction("edit")
                }
                TimelineWalkModeActionButton {
                    id: forwardButton

                    buttonHeight: walkBar.headerButtonHeight
                    chatRoot: walkBar.chatRoot
                    navigationHost: walkBar
                    enabled: walkBar.canForward
                    image: ":/icons/icons/ui/reply.svg"
                    labelText: qsTr("Forward")
                    nextTabTarget: walkBar.nextVisibleButton(forwardButton)
                    previousTabTarget: walkBar.previousVisibleButton(forwardButton)
                        || (walkBar.chatRoot ? walkBar.chatRoot.timelineSelectionFocusTarget() : null)
                    showLabel: walkBar.showActionLabels
                    mirrorIcon: true
                    toolTipText: chatRoot.selectedCount > 1
                        ? qsTr("Forward selected messages [F]")
                        : qsTr("Forward message [F]")

                    onClicked: walkBar.chatRoot.performWalkModeAction("forward")
                }
                TimelineWalkModeActionButton {
                    id: deleteButton

                    buttonHeight: walkBar.headerButtonHeight
                    chatRoot: walkBar.chatRoot
                    navigationHost: walkBar
                    enabled: walkBar.canRemove
                    image: ":/icons/icons/ui/delete.svg"
                    labelText: chatRoot.selectedCount > 1
                        ? qsTr("Delete messages")
                        : qsTr("Delete message")
                    nextTabTarget: walkBar.nextVisibleButton(deleteButton)
                    previousTabTarget: walkBar.previousVisibleButton(deleteButton)
                        || (walkBar.chatRoot ? walkBar.chatRoot.timelineSelectionFocusTarget() : null)
                    showLabel: walkBar.showActionLabels
                    toolTipText: chatRoot.selectedCount > 1
                        ? qsTr("Delete selected messages [D]")
                        : qsTr("Delete message [D]")

                    onClicked: walkBar.chatRoot.performWalkModeAction("remove")
                }
                TimelineWalkModeActionButton {
                    id: optionsButton

                    buttonHeight: walkBar.headerButtonHeight
                    chatRoot: walkBar.chatRoot
                    navigationHost: walkBar
                    enabled: walkBar.canOpenOptions
                    image: ":/icons/icons/ui/options-circle.svg"
                    labelText: qsTr("Options")
                    nextTabTarget: walkBar.nextVisibleButton(optionsButton)
                    previousTabTarget: walkBar.previousVisibleButton(optionsButton)
                        || (walkBar.chatRoot ? walkBar.chatRoot.timelineSelectionFocusTarget() : null)
                    showLabel: walkBar.showActionLabels
                    toolTipText: qsTr("More message actions [O]")

                    onClicked: walkBar.chatRoot.openPrimaryMessageActionsDialog()
                }
            }

            GroupDivider {
            }

            RowLayout {
                spacing: 0

                TimelineWalkModeActionButton {
                    id: clearButton

                    buttonHeight: walkBar.headerButtonHeight
                    chatRoot: walkBar.chatRoot
                    navigationHost: walkBar
                    enabled: walkBar.canClearSelection
                    image: ":/icons/icons/ui/round-remove-button.svg"
                    labelText: qsTr("Clear")
                    nextTabTarget: walkBar.nextVisibleButton(clearButton)
                    previousTabTarget: walkBar.previousVisibleButton(clearButton)
                        || (walkBar.chatRoot ? walkBar.chatRoot.timelineSelectionFocusTarget() : null)
                    showLabel: walkBar.showActionLabels
                    toolTipText: qsTr("Clear selection [Escape]")

                    onClicked: {
                        walkBar.chatRoot.clearSelectedEvents();
                        walkBar.chatRoot.focusTimelineSelection();
                    }
                }
            }

            GroupDivider {
            }

            RowLayout {
                spacing: 0

                TimelineWalkModeActionButton {
                    id: closeButton

                    buttonHeight: walkBar.headerButtonHeight
                    chatRoot: walkBar.chatRoot
                    navigationHost: walkBar
                    alwaysShowToolTip: true
                    image: ":/icons/icons/ui/dismiss.svg"
                    labelText: qsTr("Close")
                    nextTabTarget: walkBar.nextVisibleButton(closeButton)
                    previousTabTarget: walkBar.previousVisibleButton(closeButton)
                        || (walkBar.chatRoot ? walkBar.chatRoot.timelineSelectionFocusTarget() : null)
                    showLabel: walkBar.showActionLabels
                    toolTipText: qsTr("Exit Selection mode and return to the composer [I or Escape]")

                    onClicked: walkBar.chatRoot.exitWalkMode({
                        "focusComposer": true
                    })
                }
            }
        }
    }
}

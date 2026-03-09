// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Pane {
    id: roomActionsBar

    required property int avatarSize
    required property var componentCatalog
    required property var profileContextMenu
    required property var timelineRoot
    property int buttonSize: Komai.barIconSize
    readonly property int actionButtonCount: 2
    readonly property string newActionLabel: qsTr("New")
    readonly property string switchActionLabel: qsTr("Switch")
    readonly property real requiredIconOnlyActionWidth: actionButtonCount * buttonSize
    readonly property real requiredLabeledActionWidth: requiredIconOnlyActionWidth
        + (Komai.paddingSmall + newLabelMetrics.advanceWidth)
        + (Komai.paddingSmall + switchLabelMetrics.advanceWidth)
    readonly property real minimumActionsVisibleWidth: horizontalPadding * 2
        + userSettingsButton.effectiveButtonSize
        + Komai.paddingMedium * 2
        + requiredIconOnlyActionWidth
    readonly property real availableActionWidth: Math.max(0, width
        - horizontalPadding * 2
        - userSettingsButton.effectiveButtonSize
        - Komai.paddingMedium * 2)
    property bool showActionButtons: roomActionsBar.width > minimumActionsVisibleWidth
    property bool showActionLabels: false
    property int actionLabelsHysteresisPx: 24
    property bool actionLabelStateSeeded: false

    horizontalPadding: Komai.paddingMedium
    verticalPadding: 0

    function updateActionLabelVisibility() {
        if (!showActionButtons) {
            showActionLabels = false;
            actionLabelStateSeeded = false;
            return;
        }

        if (!actionLabelStateSeeded) {
            showActionLabels = availableActionWidth >= requiredLabeledActionWidth;
            actionLabelStateSeeded = true;
            return;
        }

        const showThreshold = requiredLabeledActionWidth + actionLabelsHysteresisPx;
        const hideThreshold = Math.max(requiredIconOnlyActionWidth, requiredLabeledActionWidth - actionLabelsHysteresisPx);

        if (showActionLabels) {
            if (availableActionWidth < hideThreshold)
                showActionLabels = false;
        } else if (availableActionWidth >= showThreshold) {
            showActionLabels = true;
        }
    }

    background: Rectangle {
        color: palette.alternateBase
    }
    Shortcut {
        // Some focused text inputs can steal Ctrl+N; keep a fallback.
        sequences: ["Ctrl+N", "Ctrl+Shift+N"]
        context: Qt.ApplicationShortcut

        onActivated: roomJoinCreateDialog.open()
        onActivatedAmbiguously: roomJoinCreateDialog.open()
    }
    TextMetrics {
        id: newLabelMetrics

        font: Qt.font({
            "bold": true
        })
        text: roomActionsBar.newActionLabel
    }
    TextMetrics {
        id: switchLabelMetrics

        font: Qt.font({
            "bold": true
        })
        text: roomActionsBar.switchActionLabel
    }
    contentItem: RowLayout {
        spacing: Komai.paddingMedium

        UserSettingsFlipButton {
            id: userSettingsButton

            profile: Komai.currentUser
            avatarButtonSize: roomActionsBar.avatarSize

            Layout.preferredHeight: Komai.navigationRowHeight
            Layout.preferredWidth: effectiveButtonSize
            onLeftClicked: {
                if (!roomActionsBar.showActionButtons)
                    profileContextMenu.popup(userSettingsButton);
                else
                    MainWindow.showUserSettingsPage();
            }

            onRightClicked: profileContextMenu.popup(userSettingsButton)
        }
        Item {
            Layout.fillWidth: true
            visible: roomActionsBar.showActionButtons
        }
        RowLayout {
            spacing: 0
            visible: roomActionsBar.showActionButtons

            RoomListActionButton {
                id: startChatButton

                buttonSize: roomActionsBar.buttonSize
                toolTipText: qsTr("Join or create a new chat or space [Ctrl+N]")
                iconSource: ":/icons/icons/ui/plus-circle.svg"
                labelText: roomActionsBar.newActionLabel
                showLabel: roomActionsBar.showActionLabels

                onClicked: roomJoinCreateDialog.open()
            }
            RoomListActionButton {
                buttonSize: roomActionsBar.buttonSize
                toolTipText: qsTr("Find & switch room or space [Ctrl+K]")
                iconSource: ":/icons/icons/ui/globe-search.svg"
                labelText: roomActionsBar.switchActionLabel
                showLabel: roomActionsBar.showActionLabels

                onClicked: timelineRoot.openCatalogDialog(componentCatalog.navigationQuickSwitcherDialog)
            }
        }
        RoomJoinCreateDialog {
            id: roomJoinCreateDialog

            profileContextMenu: roomActionsBar.profileContextMenu
        }
    }

    onAvailableActionWidthChanged: updateActionLabelVisibility()
    onRequiredLabeledActionWidthChanged: updateActionLabelVisibility()
    onShowActionButtonsChanged: updateActionLabelVisibility()
    Component.onCompleted: updateActionLabelVisibility()
}

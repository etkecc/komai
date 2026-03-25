// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import cc.etke.komai 1.0

Pane {
    id: topBar

    property var room: null
    property string avatarUrl: room ? room.roomAvatarUrl : ""
    property string directChatOtherUserId: room ? room.directChatOtherUserId : ""
    property bool isDirect: room ? room.isDirect : false
    property bool isEncrypted: room ? room.isEncrypted : false
    property var roomModel: room
    property string roomId: room ? room.roomId : ""
    property string roomName: room ? room.roomName : qsTr("No room selected")
    property string avatarDisplayName: room ? room.plainRoomName : roomName
    property string roomTopic: room ? room.roomTopic : ""
    property bool searchHasFocus: roomSearchRow.searchHasFocus
    property string searchString: ""
    property bool showBackButton: false
    property bool filteringInProgress: false
    property bool filterNotifications: false
    property int trustlevel: room ? room.trustlevel : Crypto.Unverified
    property int topBarAvatarSize: Komai.listIconSize
    property int buttonPaddingH: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
    property int buttonPaddingV: 0
    property bool isPublic: room ? room.isPublic : true
    property bool showActionLabels: false
    property int actionLabelsHysteresisPx: 8
    readonly property string membersActionLabel: qsTr("%n member(s)", "", roomModel ? roomModel.roomMemberCount : 0)
    readonly property string encryptionActionLabel: shortEncryptionLabel()
    readonly property string visibilityActionLabel: isPublic ? qsTr("Public") : qsTr("Private")
    readonly property real actionButtonWidth: topBarAvatarSize
    readonly property real actionButtonLabelGap: Komai.paddingSmall
    readonly property int visibleActionButtonCount:
        (pinButton.visible ? 1 : 0)
        + (searchButton.visible ? 1 : 0)
        + (memberButton.visible ? 1 : 0)
        + (encryptionButton.visible ? 1 : 0)
        + (leaveRoomButton.visible ? 1 : 0)
    readonly property real requiredIconOnlyActionWidth: visibleActionButtonCount * actionButtonWidth
    readonly property real requiredLabeledActionWidth: requiredIconOnlyActionWidth
        + (pinButton.visible ? (actionButtonLabelGap + pinLabelMetrics.advanceWidth) : 0)
        + (searchButton.visible ? (actionButtonLabelGap + searchLabelMetrics.advanceWidth) : 0)
        + (memberButton.visible ? (actionButtonLabelGap + membersLabelMetrics.advanceWidth) : 0)
        + (encryptionButton.visible ? (actionButtonLabelGap + encryptionLabelMetrics.advanceWidth) : 0)
        + (leaveRoomButton.visible ? (actionButtonLabelGap + leaveLabelMetrics.advanceWidth) : 0)
    readonly property real fixedOverhead:
        Komai.paddingMedium * 2
        + (showBackButton ? actionButtonWidth : 0)
        + topBarAvatarSize + buttonPaddingH
    readonly property real nameSpaceNeeded: roomNameLabel.nameImplicitWidth + roomNameLabel.visibilityFullWidth
    readonly property real minWidthForLabels: fixedOverhead + nameSpaceNeeded + requiredLabeledActionWidth

    function shortEncryptionLabel() {
        if (!isEncrypted)
            return qsTr("Unencrypted");
        switch (trustlevel) {
        case Crypto.Verified:
            return qsTr("Verified");
        case Crypto.TOFU:
            return qsTr("Trusted");
        default:
            return qsTr("Warning");
        }
    }

    function updateActionLabelVisibility() {
        if (showActionLabels) {
            if (topBar.width < minWidthForLabels - actionLabelsHysteresisPx)
                showActionLabels = false;
        } else {
            if (topBar.width >= minWidthForLabels + actionLabelsHysteresisPx)
                showActionLabels = true;
        }
    }

    function addVisibleActionButton(buttons, button) {
        if (button && button.visible !== false)
            buttons.push(button);
    }

    function visibleActionButtons() {
        const buttons = [];

        addVisibleActionButton(buttons, backToRoomsButton);
        addVisibleActionButton(buttons, pinButton);
        addVisibleActionButton(buttons, searchButton);
        addVisibleActionButton(buttons, memberButton);
        addVisibleActionButton(buttons, encryptionButton);
        addVisibleActionButton(buttons, leaveRoomButton);

        return buttons;
    }

    function focusLastVisibleActionButton() {
        const buttons = visibleActionButtons();
        if (buttons.length === 0)
            return false;

        buttons[buttons.length - 1].forceActiveFocus();
        return true;
    }

    function lastVisibleActionButtonItem() {
        const buttons = visibleActionButtons();
        return buttons.length > 0 ? buttons[buttons.length - 1] : null;
    }

    Layout.fillWidth: true
    Layout.minimumHeight: Komai.uiLayoutCompactMode ? Komai.navigationRowHeight : 0
    implicitHeight: Math.max(topLayout.height + (Komai.uiLayoutCompactMode ? Komai.paddingSmall / 2 : Komai.paddingMedium) * 2, Komai.navigationRowHeight)
    padding: 0
    z: 3

    background: Rectangle {
        color: palette.alternateBase
    }
    TextMetrics {
        id: pinLabelMetrics

        font: Qt.font({
            "bold": true,
            "pointSize": Settings.uiFontSizePt
        })
        text: qsTr("Pins")
    }
    TextMetrics {
        id: searchLabelMetrics

        font: Qt.font({
            "bold": true,
            "pointSize": Settings.uiFontSizePt
        })
        text: qsTr("Search")
    }
    TextMetrics {
        id: membersLabelMetrics

        font: Qt.font({
            "bold": true,
            "pointSize": Settings.uiFontSizePt
        })
        text: topBar.membersActionLabel
    }
    TextMetrics {
        id: encryptionLabelMetrics

        font: Qt.font({
            "bold": true,
            "pointSize": Settings.uiFontSizePt
        })
        text: topBar.encryptionActionLabel
    }
    TextMetrics {
        id: leaveLabelMetrics

        font: Qt.font({
            "bold": true,
            "pointSize": Settings.uiFontSizePt
        })
        text: qsTr("Leave")
    }
    contentItem: Item {
        ColumnLayout {
            id: topLayout

            anchors.left: parent.left
            anchors.leftMargin: Komai.paddingMedium
            anchors.right: parent.right
            anchors.rightMargin: Komai.paddingMedium
            anchors.top: parent.top
            anchors.topMargin: Komai.uiLayoutCompactMode ? Komai.paddingSmall / 2 : Komai.paddingMedium
            spacing: Komai.uiLayoutCompactMode ? 0 : Komai.paddingSmall

            GridLayout {
                id: topGrid

                Layout.fillWidth: true
                columnSpacing: 0
                rowSpacing: Komai.uiLayoutCompactMode ? 0 : Komai.paddingSmall

                RoomHeaderCommunitySection {
                    room: topBar.roomModel
                    lineSpacing: fontMetrics.lineSpacing
                }
                RoomHeaderBackButton {
                    id: backToRoomsButton

                    topBarRef: topBar
                    column: 0
                    KeyNavigation.tab: roomSettingsButton
                    showBackButton: topBar.showBackButton
                }
                RoomHeaderRoomAvatar {
                    id: roomSettingsButton

                    KeyNavigation.backtab: backToRoomsButton.visible ? backToRoomsButton : null
                    KeyNavigation.tab: pinButton.visible ? pinButton
                        : searchButton.visible ? searchButton
                        : memberButton.visible ? memberButton
                        : encryptionButton.visible ? encryptionButton
                        : leaveRoomButton.visible ? leaveRoomButton
                        : null
                    room: topBar.roomModel
                    roomId: topBar.roomId
                    avatarDisplayName: topBar.avatarDisplayName
                    roomAvatarUrl: topBar.avatarUrl
                    isDirect: topBar.isDirect
                    directChatOtherUserId: topBar.directChatOtherUserId
                    topBarAvatarSize: topBar.topBarAvatarSize
                    buttonPaddingH: topBar.buttonPaddingH
                }
                RoomHeaderRoomNameLabel {
                    id: roomNameLabel

                    roomName: topBar.roomName
                    room: topBar.roomModel
                    showVisibilityLabel: topBar.showActionLabels
                }
                // BROKEN: "Show only notifications" filter doesn't work properly.
                // It only filters messages already loaded in QML, not the full timeline.
                // The virtual timeline window (commit 5b47f5c6) makes this worse by capping
                // exposed messages to 200, but the feature was broken even before that.
                // Fixing would likely require scanning the database for highlighted messages.
                // Hiding for now until we can revisit this feature.
                // ImageButton {
                //     id: notificationsButton
                //
                //     Layout.alignment: Qt.AlignRight
                //     Layout.column: 3
                //     Layout.preferredHeight: Komai.listIconSize - Komai.paddingMedium
                //     Layout.preferredWidth: Komai.listIconSize - Komai.paddingMedium
                //     Layout.row: 1
                //     ToolTip.text: qsTr("Show only notifications")
                //     ToolTip.visible: hovered
                //     image: ":/icons/icons/ui/alert.svg"
                //
                //     onClicked: {
                //         topBar.filterNotifications = !topBar.filterNotifications
                //     }
                // }
                RoomHeaderPinButton {
                    id: pinButton

                    topBarRef: topBar
                    column: 4
                    KeyNavigation.backtab: roomSettingsButton
                    room: topBar.roomModel
                    roomId: topBar.roomId
                    showTextLabel: topBar.showActionLabels
                }
                RoomHeaderSearchButton {
                    id: searchButton

                    topBarRef: topBar
                    column: 5
                    KeyNavigation.backtab: pinButton.visible ? pinButton : roomSettingsButton
                    room: topBar.roomModel
                    showTextLabel: topBar.showActionLabels

                    onSearchActiveChanged: {
                        if (searchActive) {
                            roomSearchRow.focusInput();
                        } else {
                            roomSearchRow.clearInput();
                            topBar.searchString = "";
                        }
                    }
                }
                RoomHeaderMembersButton {
                    id: memberButton

                    topBarRef: topBar
                    column: 6
                    room: topBar.roomModel
                    showTextLabel: topBar.showActionLabels
                }
                RoomEncryptionStatusButton {
                    id: encryptionButton

                    roomId: topBar.roomModel ? topBar.roomModel.roomId : ""
                    isEncrypted: topBar.isEncrypted
                    roomAvailable: !!topBar.roomModel
                    trustlevel: topBar.trustlevel
                    topBarAvatarSize: topBar.topBarAvatarSize
                    buttonPaddingH: topBar.buttonPaddingH
                    buttonPaddingV: topBar.buttonPaddingV
                    showLabel: topBar.showActionLabels
                }
                RoomOptionsButton {
                    id: leaveRoomButton

                    topBarRef: topBar
                    column: 8
                    roomAvailable: !!topBar.roomModel
                    roomId: topBar.roomId
                    showTextLabel: topBar.showActionLabels
                }
            }

            RoomHeaderTopicText {
                Layout.fillWidth: true
                roomTopic: topBar.roomTopic
                compactMode: Komai.uiLayoutCompactMode
                lineSpacing: fontMetrics.lineSpacing
            }
            RoomPinnedMessagesSection {
                Layout.fillWidth: true
                room: topBar.roomModel
                roomId: topBar.roomId
            }
            RoomWidgetsSection {
                Layout.fillWidth: true
                room: topBar.roomModel
                roomId: topBar.roomId
            }
            RoomHeaderSearchRow {
                id: roomSearchRow

                Layout.fillWidth: true
                room: topBar.roomModel
                filteringInProgress: topBar.filteringInProgress
                topBarAvatarSize: topBar.topBarAvatarSize
                searchActive: searchButton.searchActive

                onSearchStringCommitted: function (value) {
                    topBar.searchString = value;
                }
                onRequestClose: searchButton.searchActive = false
            }
        }
    }

    onRoomIdChanged: {
        searchString = "";
        searchButton.searchActive = false;
        filterNotifications = false;
    }
    onWidthChanged: updateActionLabelVisibility()
    onMinWidthForLabelsChanged: updateActionLabelVisibility()
    Component.onCompleted: updateActionLabelVisibility()

    Shortcut {
        sequence: StandardKey.Find

        onActivated: searchButton.searchActive = !searchButton.searchActive
    }
}

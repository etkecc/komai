// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
    property Item roomListLastActionButton: null
    property bool showBackButton: false
    property bool filteringInProgress: false
    property int trustlevel: room ? room.trustlevel : Crypto.Unverified
    property int topBarAvatarSize: Komai.iconSize
    property int buttonPaddingH: (Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall : Komai.paddingMedium
    property int buttonPaddingV: 0
    property bool isPublic: room ? room.isPublic : true
    property bool showActionLabels: false
    property int actionLabelsHysteresisPx: 8
    readonly property string membersActionLabel: qsTr("Members (%1)").arg(roomModel ? roomModel.roomMemberCount : 0)
    readonly property string encryptionActionLabel: shortEncryptionLabel()
    readonly property string visibilityActionLabel: isPublic ? qsTr("Public") : qsTr("Private")
    readonly property bool roomModelSupportsSearch: !!roomModel
        && (roomModel.supportsSearch === undefined || !!roomModel.supportsSearch)
    readonly property bool roomModelSupportsPinnedMessagesUi: !!roomModel
        && (roomModel.supportsPinnedMessagesUi === undefined || !!roomModel.supportsPinnedMessagesUi)
    readonly property bool roomModelSupportsVisibilityInfo: !!roomModel
        && (roomModel.supportsVisibilityInfo === undefined || !!roomModel.supportsVisibilityInfo)
    readonly property real actionButtonWidth: topBarAvatarSize
    readonly property real actionButtonLabelGap: Komai.paddingSmall
    readonly property int visibleActionButtonCount:
        (searchButton.visible ? 1 : 0)
        + (pinButton.visible ? 1 : 0)
        + (threadsButton.visible ? 1 : 0)
        + (memberButton.visible ? 1 : 0)
        + (encryptionButton.visible ? 1 : 0)
        + (leaveRoomButton.visible ? 1 : 0)
    readonly property real requiredIconOnlyActionWidth: visibleActionButtonCount * actionButtonWidth
    readonly property real requiredLabeledActionWidth: requiredIconOnlyActionWidth
        + (searchButton.visible ? (actionButtonLabelGap + searchLabelMetrics.advanceWidth) : 0)
        + (pinButton.visible ? (actionButtonLabelGap + pinLabelMetrics.advanceWidth) : 0)
        + (threadsButton.visible ? (actionButtonLabelGap + threadsLabelMetrics.advanceWidth) : 0)
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
        addVisibleActionButton(buttons, searchButton);
        addVisibleActionButton(buttons, pinButton);
        addVisibleActionButton(buttons, threadsButton);
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
    Layout.minimumHeight: (Komai.density !== Settings.Density.Spacious) ? Komai.navigationRowHeight : 0
    implicitHeight: Math.max(topLayout.height + ((Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall / 2 : Komai.paddingMedium) * 2, Komai.navigationRowHeight)
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
        text: pinButton.labelText
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
        id: threadsLabelMetrics

        font: Qt.font({
            "bold": true,
            "pointSize": Settings.uiFontSizePt
        })
        text: threadsButton.labelText
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
            anchors.topMargin: (Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall / 2 : Komai.paddingMedium
            spacing: (Komai.density !== Settings.Density.Spacious) ? 0 : Komai.paddingSmall

            GridLayout {
                id: topGrid

                Layout.fillWidth: true
                columnSpacing: 0
                rowSpacing: (Komai.density !== Settings.Density.Spacious) ? 0 : Komai.paddingSmall

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

                    KeyNavigation.backtab: backToRoomsButton.visible ? backToRoomsButton
                        : topBar.roomListLastActionButton ? topBar.roomListLastActionButton
                        : null
                    KeyNavigation.tab: searchButton.visible ? searchButton
                        : pinButton.visible ? pinButton
                        : threadsButton.visible ? threadsButton
                        : memberButton.visible ? memberButton
                        : encryptionButton.visible ? encryptionButton
                        : leaveRoomButton.visible ? leaveRoomButton
                        : null
                    room: topBar.roomModel
                    roomId: topBar.roomId
                    roomDisplayName: topBar.avatarDisplayName
                    roomAvatarUrl: topBar.avatarUrl
                    isDirect: topBar.isDirect
                    directChatOtherUserId: topBar.directChatOtherUserId
                    topBarAvatarSize: topBar.topBarAvatarSize
                    buttonPaddingH: topBar.buttonPaddingH
                }
                RoomHeaderRoomNameLabel {
                    id: roomNameLabel

                    roomName: topBar.roomName
                    room: topBar.roomModelSupportsVisibilityInfo ? topBar.roomModel : null
                    showVisibilityLabel: topBar.showActionLabels && topBar.roomModelSupportsVisibilityInfo
                }
                RoomHeaderSearchButton {
                    id: searchButton

                    topBarRef: topBar
                    column: 4
                    KeyNavigation.backtab: roomSettingsButton
                    KeyNavigation.tab: pinButton.visible ? pinButton
                        : threadsButton.visible ? threadsButton
                        : memberButton.visible ? memberButton
                        : encryptionButton.visible ? encryptionButton
                        : leaveRoomButton.visible ? leaveRoomButton
                        : null
                    room: topBar.roomModelSupportsSearch ? topBar.roomModel : null
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
                RoomHeaderPinButton {
                    id: pinButton

                    topBarRef: topBar
                    column: 5
                    KeyNavigation.backtab: searchButton.visible ? searchButton : roomSettingsButton
                    KeyNavigation.tab: threadsButton.visible ? threadsButton
                        : memberButton.visible ? memberButton
                        : encryptionButton.visible ? encryptionButton
                        : leaveRoomButton.visible ? leaveRoomButton
                        : null
                    room: topBar.roomModelSupportsPinnedMessagesUi ? topBar.roomModel : null
                    roomId: topBar.roomId
                    showTextLabel: topBar.showActionLabels
                }
                RoomHeaderThreadsButton {
                    id: threadsButton

                    topBarRef: topBar
                    column: 6
                    KeyNavigation.backtab: pinButton.visible ? pinButton
                        : searchButton.visible ? searchButton
                        : roomSettingsButton
                    KeyNavigation.tab: memberButton.visible ? memberButton
                        : encryptionButton.visible ? encryptionButton
                        : leaveRoomButton.visible ? leaveRoomButton
                        : null
                    room: topBar.roomModel
                    roomId: topBar.roomId
                    showTextLabel: topBar.showActionLabels
                }
                RoomHeaderMembersButton {
                    id: memberButton

                    topBarRef: topBar
                    column: 7
                    KeyNavigation.backtab: threadsButton.visible ? threadsButton
                        : pinButton.visible ? pinButton
                        : searchButton.visible ? searchButton
                        : roomSettingsButton
                    KeyNavigation.tab: encryptionButton.visible ? encryptionButton
                        : leaveRoomButton.visible ? leaveRoomButton
                        : null
                    room: topBar.roomModel
                    showTextLabel: topBar.showActionLabels
                }
                RoomEncryptionStatusButton {
                    id: encryptionButton

                    KeyNavigation.backtab: memberButton.visible ? memberButton
                        : threadsButton.visible ? threadsButton
                        : pinButton.visible ? pinButton
                        : searchButton.visible ? searchButton
                        : roomSettingsButton
                    KeyNavigation.tab: leaveRoomButton.visible ? leaveRoomButton : null
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
                    column: 9
                    KeyNavigation.backtab: encryptionButton.visible ? encryptionButton
                        : memberButton.visible ? memberButton
                        : threadsButton.visible ? threadsButton
                        : pinButton.visible ? pinButton
                        : searchButton.visible ? searchButton
                        : roomSettingsButton
                    roomAvailable: !!topBar.roomModel
                    roomId: topBar.roomId
                    showTextLabel: topBar.showActionLabels
                }
            }

            RoomHeaderTopicText {
                Layout.fillWidth: true
                roomTopic: topBar.roomTopic
                compactMode: (Komai.density !== Settings.Density.Spacious)
                lineSpacing: fontMetrics.lineSpacing
            }
            RoomWidgetsSection {
                Layout.fillWidth: true
                room: topBar.roomModel
                roomId: topBar.roomId
            }
            RoomHeaderSearchRow {
                id: roomSearchRow

                Layout.fillWidth: true
                room: topBar.roomModelSupportsSearch ? topBar.roomModel : null
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
    }
    onWidthChanged: updateActionLabelVisibility()
    onMinWidthForLabelsChanged: updateActionLabelVisibility()
    Component.onCompleted: updateActionLabelVisibility()

    Shortcut {
        sequences: [StandardKey.Find]
        enabled: searchButton.visible

        onActivated: searchButton.searchActive = !searchButton.searchActive
    }
}

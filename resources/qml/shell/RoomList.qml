// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import ".."
import "../components"
import "../dialogs"
import "../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

Page {
    //leftPadding: Nheko.paddingSmall
    //rightPadding: Nheko.paddingSmall
    property bool compactMode: Nheko.uiLayoutCompactMode
    property int avatarSize: Nheko.listIconSize
    property bool collapsed: false

    ComponentCatalog {
        id: componentCatalog
    }

    background: Rectangle {
        color: Nheko.theme.sidebarBackground
    }
    header: ColumnLayout {
        spacing: 0

        Pane {
            id: userInfoPanel
            Layout.maximumHeight: Settings.sidebarsCommunitiesVisible ? 0 : -1
            clip: true

            function openUserProfile() {
                profileContextMenu.openCurrentUserProfile();
            }

            Layout.alignment: Qt.AlignBottom
            Layout.fillWidth: true
            Layout.minimumHeight: 40
            //Layout.preferredHeight: userInfoGrid.implicitHeight + 2 * Nheko.paddingMedium
            padding: Nheko.paddingMedium

            background: Rectangle {
                color: palette.window
            }
            contentItem: RowLayout {
                id: userInfoGrid

                property var profile: Nheko.currentUser

                spacing: Nheko.paddingMedium

                Avatar {
                    id: headerAvatar

                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredHeight: fontMetrics.lineSpacing * 2
                    Layout.preferredWidth: fontMetrics.lineSpacing * 2
                    displayName: userInfoGrid.profile ? userInfoGrid.profile.displayName : ""
                    enabled: false
                    url: (userInfoGrid.profile ? userInfoGrid.profile.avatarUrl : "").replace("mxc://", "image://MxcImage/")
                    userid: userInfoGrid.profile ? userInfoGrid.profile.userid : ""
                }
                ColumnLayout {
                    id: col

                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    Layout.preferredWidth: parent.width - headerAvatar.width - logoutButton.width - Nheko.paddingMedium * 2
                    spacing: 0
                    visible: !collapsed

                    ElidedLabel {
                        Layout.alignment: Qt.AlignBottom
                        elideWidth: col.width
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        font.weight: Font.DemiBold
                        fullText: userInfoGrid.profile ? userInfoGrid.profile.displayName : ""
                    }
                    ElidedLabel {
                        Layout.alignment: Qt.AlignTop
                        color: palette.buttonText
                        elideWidth: col.width
                        font.pointSize: Settings.uiFontSizePt * 0.9
                        fullText: userInfoGrid.profile ? userInfoGrid.profile.userid : ""
                    }
                }
                Item {
                }
                ImageButton {
                    id: logoutButton

                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredHeight: fontMetrics.lineSpacing * 2
                    Layout.preferredWidth: fontMetrics.lineSpacing * 2
                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Logout")
                    ToolTip.visible: hovered
                    image: ":/icons/icons/ui/power-off.svg"
                    visible: !collapsed

                    onClicked: Nheko.openLogoutDialog()
                }
            }

            InputDialog {
                id: statusDialog

                prompt: qsTr("Enter your status message:")
                title: qsTr("Status Message")

                text: userInfoGrid.profile ? Presence.userStatus(userInfoGrid.profile.userid) : ""

                onAccepted: function (text) {
                    Nheko.setStatusMessage(text);
                }
            }
            Menu {
                id: userInfoMenu

                MenuItem {
                    text: qsTr("Profile settings")

                    onTriggered: userInfoPanel.openUserProfile()
                }
                MenuItem {
                    text: qsTr("Set status message")

                    onTriggered: statusDialog.show()
                }
                MenuSeparator {
                }

                ButtonGroup {
                    id: onlineStateGroup
                }
                MenuItem {
                    text: qsTr("Automatic online status")
                    ButtonGroup.group: onlineStateGroup
                    checkable: true
                    checked: Settings.networkPresenceStatusPolicy == Settings.Presence.AutomaticPresence
                    onTriggered: if (checked) Settings.networkPresenceStatusPolicy = Settings.Presence.AutomaticPresence
                }
                MenuItem {
                    text: qsTr("Online")
                    ButtonGroup.group: onlineStateGroup
                    checkable: true
                    checked: Settings.networkPresenceStatusPolicy == Settings.Presence.Online
                    onTriggered: if (checked) Settings.networkPresenceStatusPolicy = Settings.Presence.Online
                }
                MenuItem {
                    text: qsTr("Unavailable")
                    ButtonGroup.group: onlineStateGroup
                    checkable: true
                    checked: Settings.networkPresenceStatusPolicy == Settings.Presence.Unavailable
                    onTriggered: if (checked) Settings.networkPresenceStatusPolicy = Settings.Presence.Unavailable
                }
                MenuItem {
                    text: qsTr("Offline")
                    ButtonGroup.group: onlineStateGroup
                    checkable: true
                    checked: Settings.networkPresenceStatusPolicy == Settings.Presence.Offline
                    onTriggered: if (checked) Settings.networkPresenceStatusPolicy = Settings.Presence.Offline
                }
            }
            TapHandler {
                acceptedButtons: Qt.LeftButton
                gesturePolicy: TapHandler.ReleaseWithinBounds
                margin: -Nheko.paddingSmall

                onLongPressed: userInfoMenu.popup(userInfoPanel)
                onSingleTapped: userInfoPanel.openUserProfile()
            }
            TapHandler {
                acceptedButtons: Qt.RightButton
                gesturePolicy: TapHandler.ReleaseWithinBounds
                margin: -Nheko.paddingSmall

                onSingleTapped: userInfoMenu.popup(userInfoPanel)
            }
        }
        Rectangle {
            Layout.fillWidth: true
            color: Nheko.theme.separator
            Layout.preferredHeight: Settings.sidebarsCommunitiesVisible ? 0 : 2
        }
        Pane {
            id: roomActionsBar

            property int buttonSize: Math.min(30, avatarSize)
            property bool showActionButtons: roomActionsBar.width > 160

            Layout.fillWidth: true
            Layout.preferredHeight: Nheko.navigationRowHeight
            horizontalPadding: Nheko.paddingMedium
            verticalPadding: 0

            background: Rectangle {
                color: palette.alternateBase
            }
            contentItem: RowLayout {
                id: buttonRow

                spacing: Nheko.paddingMedium

                UserSettingsFlipButton {
                    id: userSettingsButton

                    profile: Nheko.currentUser
                    avatarButtonSize: Nheko.barIconSize

                    Layout.preferredHeight: Nheko.navigationRowHeight
                    Layout.preferredWidth: effectiveButtonSize
                    onLeftClicked: {
                        if (!roomActionsBar.showActionButtons)
                            profileContextMenu.popup(userSettingsButton)
                        else
                            MainWindow.showUserSettingsPage()
                    }

                    onRightClicked: profileContextMenu.popup(userSettingsButton)
                }
                Item {
                    Layout.fillWidth: true
                    visible: roomActionsBar.showActionButtons
                }
                ImageButton {
                    id: startChatButton

                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Start a new chat")
                    ToolTip.visible: hovered
                    Layout.preferredHeight: roomActionsBar.buttonSize
                    Layout.preferredWidth: roomActionsBar.buttonSize
                    hoverEnabled: true
                    image: ":/icons/icons/ui/plus-circle.svg"
                    visible: roomActionsBar.showActionButtons

                    onClicked: roomJoinCreateMenu.popup(startChatButton)

                    Menu {
                        id: roomJoinCreateMenu

                        MenuItem {
                            text: qsTr("Join a room")

                            onTriggered: Nheko.openJoinRoomDialog()
                        }
                        MenuItem {
                            text: qsTr("Create a new room")

                            onTriggered: {
                                var createRoom = createRoomComponent.createObject(timelineRoot);
                                createRoom.show();
                                timelineRoot.destroyOnClose(createRoom);
                            }
                        }
                        MenuItem {
                            text: qsTr("Start a direct chat")

                            onTriggered: {
                                var createDirect = createDirectComponent.createObject(timelineRoot);
                                createDirect.show();
                                timelineRoot.destroyOnClose(createDirect);
                            }
                        }
                        MenuItem {
                            text: qsTr("Create a new community")

                            onTriggered: {
                                var createRoom = createRoomComponent.createObject(timelineRoot, {
                                        "space": true
                                    });
                                createRoom.show();
                                timelineRoot.destroyOnClose(createRoom);
                            }
                        }
                    }
                }
                ImageButton {
                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Room directory")
                    ToolTip.visible: hovered
                    Layout.preferredHeight: roomActionsBar.buttonSize
                    Layout.preferredWidth: roomActionsBar.buttonSize
                    hoverEnabled: true
                    image: ":/icons/icons/ui/room-directory.svg"
                    visible: roomActionsBar.showActionButtons

                    onClicked: {
                        var win = roomDirectoryComponent.createObject(timelineRoot);
                        win.show();
                        timelineRoot.destroyOnClose(win);
                    }
                }
                ImageButton {
                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Find & switch room (Ctrl+K)")
                    ToolTip.visible: hovered
                    Layout.preferredHeight: roomActionsBar.buttonSize
                    Layout.preferredWidth: roomActionsBar.buttonSize
                    hoverEnabled: true
                    image: ":/icons/icons/ui/search.svg"
                    ripple: false
                    visible: roomActionsBar.showActionButtons

                    onClicked: {
                        var component = Qt.createComponent(componentCatalog.quickSwitcher);
                        if (component.status == Component.Ready) {
                            var quickSwitch = component.createObject(timelineRoot);
                            quickSwitch.open();
                            destroyOnClosed(quickSwitch);
                        } else {
                            console.error("Failed to create component: " + component.errorString());
                        }
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            color: Nheko.theme.separator
            Layout.preferredHeight: 1
        }
    }

    // HACK: https://bugreports.qt.io/browse/QTBUG-83972, qtwayland cannot auto hide menu
    Connections {
        function onHideMenu() {
            userInfoMenu.close();
            roomContextMenu.close();
        }

        target: MainWindow
    }
    RoomListProfileMenu {
        id: profileContextMenu

        timelineRoot: timelineRoot
        componentCatalog: componentCatalog
        createRoomComponent: createRoomComponent
        createDirectComponent: createDirectComponent
        roomDirectoryComponent: roomDirectoryComponent
    }

    Component {
        id: roomDirectoryComponent

        RoomDirectory {
        }
    }
    Component {
        id: createRoomComponent

        CreateRoom {
        }
    }
    Component {
        id: createDirectComponent

        CreateDirect {
        }
    }
    ListView {
        id: roomlist

        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height
        model: Rooms
        boundsBehavior: Flickable.StopAtBounds

        //reuseItems: true
        ScrollBar.vertical: ScrollBar {
            id: scrollbar

            parent: roomlist
            policy: !collapsed && Settings.sidebarsRoomListScrollbarsEnabled ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            palette.dark: Qt.darker(parent.palette.alternateBase, 1.5)
            palette.mid: Qt.darker(parent.palette.alternateBase, 1.3)

            Rectangle {
                anchors.fill: parent
                color: palette.window
                z: -1
            }
        }
        delegate: ItemDelegate {
            id: roomItem

            required property string avatarUrl
            property color backgroundColor: palette.window
            property color bubbleBackground: palette.highlight
            property color bubbleText: palette.highlightedText
            required property string directChatOtherUserId
            required property bool hasLoudNotification
            required property bool hasUnreadMessages
            property color importantText: palette.text
            required property bool isDirect
            required property bool isInvite
            required property bool isSpace
            required property string lastMessage
            required property int notificationCount
            required property string roomId
            required property string roomName
            required property var tags
            required property string time
            required property bool isEncrypted
            property color unimportantText: palette.buttonText
            ToolTip.delay: Nheko.tooltipDelay
            ToolTip.text: roomName
            ToolTip.visible: hovered && collapsed
            height: Nheko.navigationRowHeight
            state: "normal"
            width: ListView.view.width - ((scrollbar.interactive && scrollbar.visible && scrollbar.parent) ? scrollbar.width : 0)

            topInset: 0
            bottomInset: 0
            leftInset: 0
            rightInset: 0

            background: Rectangle {
                color: backgroundColor

                Rectangle {
                    anchors.fill: parent
                    color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.15)
                    visible: hasUnreadMessages && roomItem.state !== "selected"
                }
            }
            states: [
                State {
                    name: "highlight"
                    when: roomItem.hovered && !((Rooms.currentRoom && roomId == Rooms.currentRoom.roomId) || Rooms.currentRoomPreview.roomid == roomId)

                    PropertyChanges {
                        roomItem {
                            backgroundColor: palette.dark
                            bubbleBackground: palette.highlight
                            bubbleText: palette.highlightedText
                            importantText: palette.brightText
                            unimportantText: palette.brightText
                        }
                    }
                },
                State {
                    name: "selected"
                    when: (Rooms.currentRoom && roomId == Rooms.currentRoom.roomId) || Rooms.currentRoomPreview.roomid == roomId

                    PropertyChanges {
                        roomItem {
                            backgroundColor: palette.highlight
                            bubbleBackground: palette.highlightedText
                            bubbleText: palette.highlight
                            importantText: palette.highlightedText
                            unimportantText: palette.highlightedText
                        }
                    }
                }
            ]

            onClicked: {
                console.log("tapped " + roomId);
                if (!Rooms.currentRoom || Rooms.currentRoom.roomId !== roomId)
                    Rooms.setCurrentRoom(roomId);
                else
                    Rooms.resetCurrentRoom();
            }
            onPressAndHold: {
                if (!isInvite)
                    roomContextMenu.show(roomItem, roomId, tags);
            }

            Ripple {
                color: Qt.rgba(palette.dark.r, palette.dark.g, palette.dark.b, 0.5)
            }

            // NOTE(Nico): We want to prevent the touch areas from overlapping. For some reason we need to add 1px of padding for that...
            Item {
                anchors.fill: parent
                anchors.margins: 1

                TapHandler {
                    id: roomItemTh

                    acceptedButtons: Qt.RightButton
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
                    gesturePolicy: TapHandler.ReleaseWithinBounds

                    onSingleTapped: {
                        if (!TimelineManager.isInvite)
                            roomContextMenu.show(roomItemTh.parent, roomId, tags);
                    }
                }
            }
            RowLayout {
                anchors.fill: parent
                anchors.margins: Nheko.paddingMedium
                spacing: Nheko.paddingMedium

                Avatar {
                    id: avatar

                    Layout.alignment: Qt.AlignVCenter
                    displayName: roomName
                    enabled: false
                    roomid: roomId
                    url: avatarUrl.replace("mxc://", "image://MxcImage/")
                    userid: isDirect ? directChatOtherUserId : ""
                    Layout.preferredWidth: avatarSize
                    Layout.preferredHeight: avatarSize

                    NotificationBubble {
                        id: collapsedNotificationBubble

                        anchors.bottom: parent.bottom
                        anchors.margins: -Nheko.paddingSmall
                        anchors.right: parent.right
                        bubbleBackgroundColor: roomItem.bubbleBackground
                        bubbleTextColor: roomItem.bubbleText
                        hasLoudNotification: roomItem.hasLoudNotification
                        mayBeVisible: collapsed && (isSpace ? Settings.sidebarsRoomListShowCommunityCounts : true)
                        notificationCount: roomItem.notificationCount
                    }
                }
                ColumnLayout {
                    id: textContent

                    Layout.alignment: compactMode ? Qt.AlignVCenter : Qt.AlignLeft
                    Layout.minimumWidth: 100
                    Layout.preferredWidth: roomItem.width - avatar.width
                    Layout.preferredHeight: compactMode ? -1 : avatar.height
                    spacing: compactMode ? 0 : Nheko.paddingSmall
                    visible: !collapsed

                    Item {
                        id: titleRow

                        property bool previewsEnabled: !isSpace && (Settings.sidebarsRoomListLastMessagePreview === Settings.LastMessagePreview.Always || (Settings.sidebarsRoomListLastMessagePreview === Settings.LastMessagePreview.OnlyUnencrypted && !isEncrypted))

                        Layout.alignment: Qt.AlignTop
                        Layout.fillWidth: true
                        Layout.preferredHeight: compactMode ? titleText.implicitHeight : subtitleText.implicitHeight

                        ElidedLabel {
                            id: titleText

                            anchors.left: parent.left
                            anchors.verticalCenter: compactMode ? parent.verticalCenter : undefined
                            color: roomItem.importantText
                            elideWidth: parent.width - (timestamp.visible ? timestamp.implicitWidth + Nheko.paddingSmall : 0) - (spaceNotificationBubble.visible ? spaceNotificationBubble.implicitWidth + Nheko.paddingSmall : 0) - (inlinePreview.visible ? Nheko.paddingSmall : 0)
                            font.bold: hasUnreadMessages
                            fullText: TimelineManager.htmlEscape(roomName)
                            textFormat: Text.RichText
                        }
                        ElidedLabel {
                            id: inlinePreview

                            anchors.left: titleText.right
                            anchors.leftMargin: Nheko.paddingSmall
                            anchors.baseline: titleText.baseline
                            anchors.right: timestamp.visible ? timestamp.left : (spaceNotificationBubble.visible ? spaceNotificationBubble.left : parent.right)
                            anchors.rightMargin: (timestamp.visible || spaceNotificationBubble.visible) ? Nheko.paddingSmall : 0
                            color: roomItem.unimportantText
                            elideWidth: Math.max(0, parent.width - titleText.implicitWidth - Nheko.paddingSmall - (timestamp.visible ? timestamp.implicitWidth + Nheko.paddingSmall : (spaceNotificationBubble.visible ? spaceNotificationBubble.implicitWidth + Nheko.paddingSmall : 0)))
                            font.pixelSize: fontMetrics.font.pixelSize * 0.95
                            fullText: TimelineManager.htmlEscape(lastMessage)
                            textFormat: Text.RichText
                            visible: compactMode && titleRow.previewsEnabled
                        }
                        Label {
                            id: timestamp

                            anchors.baseline: titleText.baseline
                            anchors.right: parent.right
                            color: roomItem.unimportantText
                            font.pixelSize: fontMetrics.font.pixelSize * 0.95
                            text: time
                            visible: !isInvite && !isSpace && Nheko.sidebarsRoomListShowLastMessageTime
                        }
                        NotificationBubble {
                            id: spaceNotificationBubble

                            anchors.right: parent.right
                            bubbleBackgroundColor: roomItem.bubbleBackground
                            bubbleTextColor: roomItem.bubbleText
                            hasLoudNotification: roomItem.hasLoudNotification
                            mayBeVisible: !collapsed && (isSpace ? Settings.sidebarsRoomListShowCommunityCounts : compactMode)
                            notificationCount: roomItem.notificationCount
                            parent: (isSpace || compactMode) ? titleRow : subtextRow
                        }
                    }
                    Item {
                        id: subtextRow

                        Layout.alignment: Qt.AlignBottom
                        Layout.fillWidth: true
                        Layout.preferredHeight: subtitleText.implicitHeight
                        visible: !compactMode && !isSpace && (Settings.sidebarsRoomListLastMessagePreview === Settings.LastMessagePreview.Always || (Settings.sidebarsRoomListLastMessagePreview === Settings.LastMessagePreview.OnlyUnencrypted && !isEncrypted))

                        ElidedLabel {
                            id: subtitleText

                            anchors.left: parent.left
                            color: roomItem.unimportantText
                            elideWidth: subtextRow.width - (subtextNotificationBubble.visible ? subtextNotificationBubble.implicitWidth : 0)
                            font.pixelSize: fontMetrics.font.pixelSize * 0.95
                            fullText: TimelineManager.htmlEscape(lastMessage)
                            textFormat: Text.RichText
                        }
                        NotificationBubble {
                            id: subtextNotificationBubble

                            anchors.baseline: subtitleText.baseline
                            anchors.right: parent.right
                            bubbleBackgroundColor: roomItem.bubbleBackground
                            bubbleTextColor: roomItem.bubbleText
                            hasLoudNotification: roomItem.hasLoudNotification
                            mayBeVisible: !collapsed
                            notificationCount: roomItem.notificationCount
                        }
                    }
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                color: Nheko.theme.separator
                height: 1
            }
            Rectangle {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                color: palette.highlight
                height: parent.height - Nheko.paddingSmall * 2
                visible: hasUnreadMessages
                width: 6
                radius: 3
            }
        }

        Connections {
            function onCurrentRoomChanged() {
                if (Rooms.currentRoom)
                    roomlist.positionViewAtIndex(Rooms.roomidToIndex(Rooms.currentRoom.roomId), ListView.Contain);
            }

            target: Rooms
        }
        Component {
            id: roomWindowComponent

            DetachedRoomWindow {
            }
        }
        Menu {
            id: roomContextMenu

            property string roomid
            property var tags

            function show(parent, roomid_, tags_) {
                roomid = roomid_;
                tags = tags_;
                popup(parent);
            }

            Component.onCompleted: {
                if (roomContextMenu.popupType != undefined) {
                    roomContextMenu.popupType = 2; // Popup.Native with fallback on older Qt (<6.8.0)
                }
            }

            InputDialog {
                id: newTag

                prompt: qsTr("Enter the tag you want to use:")
                title: qsTr("New tag")

                onAccepted: function (text) {
                    Rooms.toggleTag(roomContextMenu.roomid, "u." + text, true);
                }
            }
            MenuItem {
                text: qsTr("Open separately")

                onTriggered: {
                    var roomWindow = roomWindowComponent.createObject(null, {
                            "room": Rooms.getRoomById(roomContextMenu.roomid),
                            "roomPreview": Rooms.getRoomPreviewById(roomContextMenu.roomid)
                        });
                    roomWindow.showNormal();
                    destroyOnClose(roomWindow);
                }
            }
            MenuItem {
                text: qsTr("Mark as read")

                onTriggered: Rooms.getRoomById(roomContextMenu.roomid).markRoomAsRead()
            }
            MenuItem {
                text: qsTr("Room settings")

                onTriggered: TimelineManager.openRoomSettings(roomContextMenu.roomid)
            }
            MenuItem {
                text: qsTr("Leave room")

                onTriggered: TimelineManager.openLeaveRoomDialog(roomContextMenu.roomid)
            }
            MenuItem {
                text: qsTr("Copy room link")

                onTriggered: Rooms.copyLink(roomContextMenu.roomid)
            }
            Menu {
                id: tagsMenu

                title: qsTr("Tag room as:")

                Instantiator {
                    model: Communities.tagsWithDefault

                    delegate: MenuItem {
                        property string t: modelData

                        checkable: true
                        checked: roomContextMenu.tags !== undefined && roomContextMenu.tags.includes(t)
                        text: {
                            switch (t) {
                            case "m.favourite":
                                return qsTr("Favourite");
                            case "m.lowpriority":
                                return qsTr("Low priority");
                            case "m.server_notice":
                                return qsTr("Server notice");
                            default:
                                return t.substring(2);
                            }
                        }

                        onTriggered: Rooms.toggleTag(roomContextMenu.roomid, t, checked)
                    }

                    onObjectAdded: (index, object) => tagsMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => tagsMenu.removeItem(object)
                }
                MenuItem {
                    text: qsTr("Create new tag...")

                    onTriggered: newTag.show()
                }
            }
            SpaceMenu {
                id: rootSpaceMenu

                roomid: roomContextMenu.roomid
            }
        }
    }
}

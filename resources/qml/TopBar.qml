// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import QtQuick.Window 2.15
import im.nheko 1.0
import "./delegates"

Pane {
    id: topBar

    property string avatarUrl: room ? room.roomAvatarUrl : ""
    property string directChatOtherUserId: room ? room.directChatOtherUserId : ""
    property bool isDirect: room ? room.isDirect : false
    property bool isEncrypted: room ? room.isEncrypted : false
    property string roomId: room ? room.roomId : ""
    property string roomName: room ? room.roomName : qsTr("No room selected")
    property string roomTopic: room ? room.roomTopic : ""
    property bool searchHasFocus: searchField.focus && searchField.enabled
    property string searchString: ""
    property bool showBackButton: false
    property bool filterNotifications: false
    property int trustlevel: room ? room.trustlevel : Crypto.Unverified
    property int roomListAvatarSize: Math.ceil(fontMetrics.lineSpacing * Nheko.sidebarAvatarMultiplier)
    property int roomListEntryHeight: roomListAvatarSize + 2 * Nheko.paddingMedium
    property int topBarAvatarSize: Nheko.compactRoomList ? roomListAvatarSize : Nheko.avatarSize

    Layout.fillWidth: true
    Layout.minimumHeight: Nheko.compactRoomList ? roomListEntryHeight : 0
    implicitHeight: Math.max(topLayout.height + Nheko.paddingMedium * 2, roomListEntryHeight)
    padding: 0
    z: 3

    background: Rectangle {
        color: palette.alternateBase
    }
    contentItem: Item {
        GridLayout {
            id: topLayout

            anchors.left: parent.left
            anchors.margins: Nheko.paddingMedium
            anchors.right: parent.right
            anchors.top: parent.top
            columnSpacing: Nheko.paddingSmall
            rowSpacing: Nheko.compactRoomList ? 0 : Nheko.paddingSmall

            Avatar {
                id: communityAvatar

                property string avatarUrl: (Settings.groupView && room && room.parentSpace && room.parentSpace.roomAvatarUrl) || ""
                property string communityId: (Settings.groupView && room && room.parentSpace && room.parentSpace.roomid) || ""
                property string communityName: (Settings.groupView && room && room.parentSpace && room.parentSpace.roomName) || ""

                Layout.alignment: Qt.AlignHCenter
                Layout.column: 1
                Layout.row: 0
                displayName: communityName
                enabled: false
                implicitHeight: fontMetrics.lineSpacing
                implicitWidth: fontMetrics.lineSpacing
                roomid: communityId
                url: avatarUrl.replace("mxc://", "image://MxcImage/")
                visible: !Nheko.compactRoomList && roomid && room.parentSpace.isLoaded && ("space:" + room.parentSpace.roomid != Communities.currentTagId)
            }
            Label {
                id: communityLabel

                Layout.column: 2
                Layout.fillWidth: true
                Layout.row: 0
                color: palette.text
                elide: Text.ElideRight
                maximumLineCount: 1
                text: qsTr("In %1").arg(communityAvatar.displayName)
                textFormat: Text.RichText
                visible: communityAvatar.visible

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        if (!Communities.trySwitchToSpace(room.parentSpace.roomid))
                            room.parentSpace.promptJoin();
                    }
                }
            }
            ImageButton {
                id: backToRoomsButton

                Layout.alignment: Qt.AlignVCenter
                Layout.column: 0
                Layout.preferredHeight: topBarAvatarSize - Nheko.paddingMedium
                Layout.preferredWidth: topBarAvatarSize - Nheko.paddingMedium
                Layout.row: 1
                ToolTip.text: qsTr("Back to room list")
                ToolTip.visible: hovered
                image: ":/icons/icons/ui/angle-arrow-left.svg"
                visible: showBackButton

                onClicked: Rooms.resetCurrentRoom()
            }
            Avatar {
                Layout.alignment: Qt.AlignVCenter
                Layout.column: 1
                Layout.row: 1
                displayName: room ? room.plainRoomName : roomName
                enabled: false
                implicitHeight: topBarAvatarSize
                implicitWidth: topBarAvatarSize
                roomid: roomId
                url: avatarUrl.replace("mxc://", "image://MxcImage/")
                userid: isDirect ? directChatOtherUserId : ""

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        if (room)
                            TimelineManager.openRoomSettings(room.roomId);
                    }
                }
            }
            Label {
                Layout.column: 2
                Layout.fillWidth: true
                Layout.row: 1
                color: palette.text
                elide: Text.ElideRight
                font.bold: true
                font.pointSize: fontMetrics.font.pointSize * 1.1
                maximumLineCount: 1
                text: roomName
                textFormat: Text.RichText
            }
            MatrixText {
                id: roomTopicC

                Layout.column: 1
                Layout.columnSpan: 9
                Layout.fillWidth: true
                Layout.maximumHeight: fontMetrics.lineSpacing * 2 // show 2 lines
                Layout.row: 2
                clip: true
                visible: roomTopic.length > 0 && !Nheko.compactRoomList
                color: topBar.palette.text
                selectByMouse: true
                text: roomTopic
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
            //     Layout.preferredHeight: Nheko.avatarSize - Nheko.paddingMedium
            //     Layout.preferredWidth: Nheko.avatarSize - Nheko.paddingMedium
            //     Layout.row: 1
            //     ToolTip.text: qsTr("Show only notifications")
            //     ToolTip.visible: hovered
            //     image: ":/icons/icons/ui/alert.svg"
            //
            //     onClicked: {
            //         topBar.filterNotifications = !topBar.filterNotifications
            //     }
            // }
            ImageButton {
                id: pinButton

                property bool pinsShown: !Settings.hiddenPins.includes(roomId)

                Layout.alignment: Qt.AlignVCenter
                Layout.column: 4
                Layout.preferredHeight: topBarAvatarSize - Nheko.paddingMedium
                Layout.preferredWidth: topBarAvatarSize - Nheko.paddingMedium
                Layout.row: 1
                ToolTip.text: qsTr("Show or hide pinned messages")
                ToolTip.visible: hovered
                image: pinsShown ? ":/icons/icons/ui/pin.svg" : ":/icons/icons/ui/pin-off.svg"
                visible: !!room && room.pinnedMessages.length > 0

                onClicked: {
                    var ps = Settings.hiddenPins;
                    if (pinsShown) {
                        ps.push(roomId);
                    } else {
                        const index = ps.indexOf(roomId);
                        if (index > -1) {
                            ps.splice(index, 1);
                        }
                    }
                    Settings.hiddenPins = ps;
                }
            }
            ImageButton {
                id: memberButton

                Layout.alignment: Qt.AlignVCenter
                Layout.column: 5
                Layout.preferredHeight: topBarAvatarSize - Nheko.paddingMedium
                Layout.preferredWidth: topBarAvatarSize - Nheko.paddingMedium
                Layout.row: 1
                visible: !!room

                ToolTip.text: qsTr("Show room members.")
                ToolTip.visible: hovered
                image: ":/icons/icons/ui/people.svg"

                onClicked: TimelineManager.openRoomMembers(room)
            }
            AbstractButton {
                id: encryptionButton

                Layout.alignment: Qt.AlignVCenter
                Layout.column: 6
                Layout.preferredHeight: topBarAvatarSize - Nheko.paddingMedium
                Layout.preferredWidth: topBarAvatarSize - Nheko.paddingMedium
                Layout.row: 1
                background: null
                visible: !!room

                ToolTip.delay: Nheko.tooltipDelay
                ToolTip.text: encryptionDialogTitle()
                ToolTip.visible: hovered

                function encryptionDialogTitle() {
                    if (!isEncrypted)
                        return qsTr("This room is not encrypted.");
                    switch (trustlevel) {
                    case Crypto.Verified:
                        return qsTr("This room contains only verified devices.");
                    case Crypto.TOFU:
                        return qsTr("This room contains verified devices and devices which have never changed their master key.");
                    default:
                        return qsTr("This room contains unverified devices!");
                    }
                }

                function encryptionDialogBody() {
                    if (!isEncrypted)
                        return qsTr("Messages in this room are not end-to-end encrypted. See the Members list for device details.");
                    switch (trustlevel) {
                    case Crypto.Verified:
                        return qsTr("Messages are end-to-end encrypted and all devices are verified. See the Members list for device details.");
                    case Crypto.TOFU:
                        return qsTr("Messages are end-to-end encrypted. Some devices are verified, others are trusted by first use. See the Members list for device details.");
                    case Crypto.MessageUnverified:
                        return qsTr("Messages are end-to-end encrypted, but the key is from an untrusted source. See the Members list for device details.");
                    default:
                        return qsTr("Messages are end-to-end encrypted, but some devices are unverified. See the Members list for device details.");
                    }
                }

                contentItem: EncryptionIndicator {
                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: encryptionButton.encryptionDialogTitle()
                    ToolTip.visible: encryptionButton.hovered
                    enabled: false
                    encrypted: isEncrypted
                    hovered: parent.hovered
                    trust: trustlevel
                    unencryptedColor: palette.buttonText
                    unencryptedHoverColor: palette.highlight
                    sourceSize.height: encryptionButton.Layout.preferredHeight
                    sourceSize.width: encryptionButton.Layout.preferredWidth
                }

                onClicked: encryptionDialog.open()

                NhekoCursorShape {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                }
            }
            Dialog {
                id: encryptionDialog

                parent: Overlay.overlay
                anchors.centerIn: parent
                modal: true
                standardButtons: Dialog.Ok
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                title: qsTr("Encryption status")
                width: Math.min(520, parent.width - Nheko.paddingLarge * 2)
                padding: Nheko.paddingLarge

                contentItem: ColumnLayout {
                    spacing: Nheko.paddingMedium

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: encryptionButton.encryptionDialogTitle()
                        color: palette.text
                        font.bold: true
                    }
                    MatrixText {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: encryptionButton.encryptionDialogBody()
                        color: palette.buttonText
                    }
                }
            }
            ImageButton {
                id: searchButton

                property bool searchActive: false

                Layout.alignment: Qt.AlignVCenter
                Layout.column: 7
                Layout.preferredHeight: topBarAvatarSize - Nheko.paddingMedium
                Layout.preferredWidth: topBarAvatarSize - Nheko.paddingMedium
                Layout.row: 1
                ToolTip.text: qsTr("Search this room")
                ToolTip.visible: hovered
                image: ":/icons/icons/ui/search.svg"
                visible: !!room

                onClicked: searchActive = !searchActive
                onSearchActiveChanged: {
                    if (searchActive) {
                        searchField.forceActiveFocus();
                    } else {
                        searchField.clear();
                        topBar.searchString = "";
                    }
                }
            }
            ImageButton {
                id: roomSettingsButton

                Layout.alignment: Qt.AlignVCenter
                Layout.column: 8
                Layout.preferredHeight: topBarAvatarSize - Nheko.paddingMedium
                Layout.preferredWidth: topBarAvatarSize - Nheko.paddingMedium
                Layout.row: 1
                ToolTip.text: qsTr("Room settings")
                ToolTip.visible: hovered
                image: ":/icons/icons/ui/toggles.svg"
                visible: !!room

                onClicked: TimelineManager.openRoomSettings(roomId)
            }
            ImageButton {
                id: roomOptionsButton

                Layout.alignment: Qt.AlignVCenter
                Layout.column: 9
                Layout.preferredHeight: topBarAvatarSize - Nheko.paddingMedium
                Layout.preferredWidth: topBarAvatarSize - Nheko.paddingMedium
                Layout.row: 1
                ToolTip.text: qsTr("Room options")
                ToolTip.visible: hovered
                image: ":/icons/icons/ui/options.svg"
                visible: !!room

                onClicked: roomOptionsMenu.popup(roomOptionsButton)

                Menu {
                    id: roomOptionsMenu

                    Component.onCompleted: {
                        if (roomOptionsMenu.popupType != undefined) {
                            roomOptionsMenu.popupType = 2; // Popup.Native with fallback on older Qt (<6.8.0)
                        }
                    }


                    MenuItem {
                        text: qsTr("Leave room")
                        icon.source: "qrc:/icons/icons/ui/power-off.svg"

                        onTriggered: TimelineManager.openLeaveRoomDialog(roomId)
                    }
                }
            }
            ScrollView {
                id: pinnedMessages

                Layout.column: 1
                Layout.columnSpan: 9
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, Nheko.avatarSize * 4)
                Layout.row: 3
                ScrollBar.horizontal.visible: false
                clip: true
                visible: !!room && room.pinnedMessages.length > 0 && !Settings.hiddenPins.includes(roomId)
                contentWidth: availableWidth

                ListView {
                    model: room ? room.pinnedMessages : undefined
                    spacing: Nheko.paddingSmall

                    delegate: RowLayout {
                        required property string modelData

                        height: implicitHeight
                        width: ListView.view.width

                        Reply {
                            id: reply

                            property var e: room ? room.getDump(modelData, "pins") : {}

                            maxWidth: pinnedMessages.width - 16
                            eventId: e.eventId ?? ""
                            userColor: room ? TimelineManager.roomUserColor(room.roomId, e.userId, palette.window, palette.highlight) : TimelineManager.userColor(e.userId, palette.window)
                            roomColor: room ? TimelineManager.roomUserColor(room.roomId, e.userId, palette.base, palette.highlight) : TimelineManager.userColor(e.userId, palette.base)

                            Connections {
                                function onPinnedMessagesChanged() {
                                    reply.e = room.getDump(modelData, "pins");
                                }

                                target: room
                            }
                        }
                        ImageButton {
                            id: deletePinButton

                            Layout.alignment: Qt.AlignTop | Qt.AlignRight
                            Layout.preferredHeight: 16
                            Layout.preferredWidth: 16
                            ToolTip.text: qsTr("Unpin")
                            ToolTip.visible: hovered
                            hoverEnabled: true
                            image: ":/icons/icons/ui/dismiss.svg"
                            visible: room.permissions.canChange(MtxEvent.PinnedEvents)

                            onClicked: room.unpin(modelData)
                        }
                    }
                }
            }
            ScrollView {
                id: widgets

                Layout.column: 1
                Layout.columnSpan: 9
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, Nheko.avatarSize * 1.5)
                Layout.row: 4
                ScrollBar.horizontal.visible: false
                clip: true
                visible: !!room && room.widgetLinks.length > 0 && !Settings.hiddenWidgets.includes(roomId)
                contentWidth: availableWidth

                ListView {
                    model: room ? room.widgetLinks : undefined
                    spacing: Nheko.paddingSmall

                    delegate: MatrixText {
                        width: widgets.width
                        required property var modelData

                        color: palette.text
                        text: modelData
                    }
                }
            }
            MatrixTextField {
                id: searchField

                Layout.column: 1
                Layout.columnSpan: 9
                Layout.fillWidth: true
                Layout.row: 5
                enabled: visible
                hasClear: true
                placeholderText: qsTr("Enter search query")
                visible: searchButton.searchActive

                onAccepted: topBar.searchString = text
            }
        }
    }

    onRoomIdChanged: {
        searchString = "";
        searchButton.searchActive = false;
        searchField.text = "";
        filterNotifications = false;
    }

    // HACK: https://bugreports.qt.io/browse/QTBUG-83972, qtwayland cannot auto hide menu
    Connections {
        function onHideMenu() {
            roomOptionsMenu.close();
        }

        target: MainWindow
    }
    Shortcut {
        sequence: StandardKey.Find

        onActivated: searchButton.searchActive = !searchButton.searchActive
    }
}

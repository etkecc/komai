// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import QtQuick.Window 2.15
import QtQuick.Effects
import im.nheko 1.0
import "../../delegates"
import "../../ui"

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
    property bool filteringInProgress: false
    property bool filterNotifications: false
    property int trustlevel: room ? room.trustlevel : Crypto.Unverified
    property int topBarAvatarSize: Nheko.barIconSize
    property int buttonPaddingH: Nheko.uiLayoutCompactMode ? Nheko.paddingSmall : Nheko.paddingMedium
    property int buttonPaddingV: 0

    Layout.fillWidth: true
    Layout.minimumHeight: Nheko.uiLayoutCompactMode ? Nheko.navigationRowHeight : 0
    implicitHeight: Math.max(topLayout.height + Nheko.paddingMedium * 2, Nheko.navigationRowHeight)
    padding: 0
    z: 3

    background: Rectangle {
        color: palette.alternateBase
    }
    contentItem: Item {
    GridLayout {
        id: topLayout

        anchors.left: parent.left
        anchors.leftMargin: Nheko.paddingMedium
        anchors.right: parent.right
        anchors.rightMargin: Nheko.paddingMedium
        anchors.top: parent.top
        columnSpacing: 0
            rowSpacing: Nheko.uiLayoutCompactMode ? 0 : Nheko.paddingSmall

            Avatar {
                id: communityAvatar

                property string avatarUrl: (Settings.sidebarsCommunitiesVisible && room && room.parentSpace && room.parentSpace.roomAvatarUrl) || ""
                property string communityId: (Settings.sidebarsCommunitiesVisible && room && room.parentSpace && room.parentSpace.roomid) || ""
                property string communityName: (Settings.sidebarsCommunitiesVisible && room && room.parentSpace && room.parentSpace.roomName) || ""

                Layout.alignment: Qt.AlignHCenter
                Layout.column: 1
                Layout.row: 0
                displayName: communityName
                enabled: false
                implicitHeight: fontMetrics.lineSpacing
                implicitWidth: fontMetrics.lineSpacing
                roomid: communityId
                url: avatarUrl.replace("mxc://", "image://MxcImage/")
                visible: !Nheko.uiLayoutCompactMode && roomid && room.parentSpace.isLoaded && ("space:" + room.parentSpace.roomid != Communities.currentTagId)
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
                Layout.preferredHeight: topBarAvatarSize
                Layout.preferredWidth: topBarAvatarSize
                Layout.row: 1
                leftPadding: buttonPaddingH
                rightPadding: buttonPaddingH
                topPadding: buttonPaddingV
                bottomPadding: buttonPaddingV
                ToolTip.text: qsTr("Back to room list")
                ToolTip.visible: hovered
                image: ":/icons/icons/ui/angle-arrow-left.svg"
                visible: showBackButton

                onClicked: Rooms.resetCurrentRoom()
            }
            Avatar {
                Layout.alignment: Qt.AlignVCenter
                Layout.column: 1
                Layout.rightMargin: buttonPaddingH
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
                font.pointSize: Settings.uiFontSizePt * 1.1
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
                visible: roomTopic.length > 0 && !Nheko.uiLayoutCompactMode
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
                Layout.preferredHeight: topBarAvatarSize
                Layout.preferredWidth: topBarAvatarSize
                Layout.row: 1
                leftPadding: buttonPaddingH
                rightPadding: buttonPaddingH
                topPadding: buttonPaddingV
                bottomPadding: buttonPaddingV
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
                id: searchButton

                property bool searchActive: false

                Layout.alignment: Qt.AlignVCenter
                Layout.column: 5
                Layout.preferredHeight: topBarAvatarSize
                Layout.preferredWidth: topBarAvatarSize
                Layout.row: 1
                leftPadding: buttonPaddingH
                rightPadding: buttonPaddingH
                topPadding: buttonPaddingV
                bottomPadding: buttonPaddingV
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
                id: memberButton

                Layout.alignment: Qt.AlignVCenter
                Layout.column: 6
                Layout.preferredHeight: topBarAvatarSize
                Layout.preferredWidth: topBarAvatarSize
                Layout.row: 1
                leftPadding: buttonPaddingH
                rightPadding: buttonPaddingH
                topPadding: buttonPaddingV
                bottomPadding: buttonPaddingV
                visible: !!room

                ToolTip.text: qsTr("Show room members.")
                ToolTip.visible: hovered
                image: ":/icons/icons/ui/people.svg"

                onClicked: TimelineManager.openRoomMembers(room)
            }
            RoomEncryptionStatusButton {
                isEncrypted: topBar.isEncrypted
                roomAvailable: !!room
                trustlevel: topBar.trustlevel
                topBarAvatarSize: topBar.topBarAvatarSize
                buttonPaddingH: topBar.buttonPaddingH
                buttonPaddingV: topBar.buttonPaddingV
            }
            ImageButton {
                id: roomSettingsButton

                Layout.alignment: Qt.AlignVCenter
                Layout.column: 8
                Layout.preferredHeight: topBarAvatarSize
                Layout.preferredWidth: topBarAvatarSize
                Layout.row: 1
                leftPadding: buttonPaddingH
                rightPadding: buttonPaddingH
                topPadding: buttonPaddingV
                bottomPadding: buttonPaddingV
                ToolTip.text: qsTr("Room settings")
                ToolTip.visible: hovered
                image: ":/icons/icons/ui/toggles.svg"
                visible: !!room

                onClicked: TimelineManager.openRoomSettings(roomId)
            }
            RoomOptionsButton {
                roomAvailable: !!room
                roomId: topBar.roomId
                topBarAvatarSize: topBar.topBarAvatarSize
                buttonPaddingH: topBar.buttonPaddingH
                buttonPaddingV: topBar.buttonPaddingV
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
                            property string replyUserId: (e && e.userId) ? String(e.userId) : ""
                            property bool isReplyFromCurrentUser: {
                                const currentUser = Nheko.currentUser;
                                const currentUserId = (currentUser && currentUser.userid)
                                        ? String(currentUser.userid)
                                        : "";
                                return currentUserId.length > 0 && replyUserId === currentUserId;
                            }

                            maxWidth: pinnedMessages.width - 16
                            eventId: e.eventId ?? ""
                            userColor: isReplyFromCurrentUser
                                ? palette.highlight
                                : room ? TimelineManager.roomUserColor(room.roomId, replyUserId, palette.window, palette.highlight, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyUserId, palette.window)
                            roomColor: isReplyFromCurrentUser
                                ? palette.highlight
                                : room ? TimelineManager.roomUserColor(room.roomId, replyUserId, palette.base, palette.highlight, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyUserId, palette.base)

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
            RowLayout {
                Layout.column: 1
                Layout.columnSpan: 9
                Layout.fillWidth: true
                Layout.row: 5
                Layout.topMargin: Nheko.paddingSmall
                spacing: Nheko.paddingSmall
                visible: searchButton.searchActive

                Item {
                    id: searchIcon

                    property bool _rawLoading: (room && room.paginationInProgress) || topBar.filteringInProgress
                    property bool isLoading: _rawLoading || searchLoadingHoldTimer.running

                    on_RawLoadingChanged: {
                        if (_rawLoading)
                            searchLoadingHoldTimer.stop();
                        else
                            searchLoadingHoldTimer.start();
                    }

                    Timer {
                        id: searchLoadingHoldTimer
                        interval: 200
                    }

                    Layout.preferredHeight: topBarAvatarSize
                    Layout.preferredWidth: topBarAvatarSize

                    // Composite rendered as one unit for the desaturation effect
                    Item {
                        id: searchIconContent

                        anchors.fill: parent
                        visible: false
                        layer.enabled: true

                        Image {
                            anchors.fill: parent
                            source: "qrc:/logos/komai.svg"
                            sourceSize.width: width * 2
                            sourceSize.height: height * 2
                            fillMode: Image.PreserveAspectFit
                        }
                        Rectangle {
                            id: searchBadge

                            property int badgeSize: Math.round(topBarAvatarSize * 0.55)
                            property int iconSize: Math.round(badgeSize * 0.69)

                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            anchors.bottomMargin: -2
                            anchors.rightMargin: -2
                            width: badgeSize
                            height: badgeSize
                            radius: Math.round(badgeSize * 0.25)
                            color: palette.alternateBase

                            transform: Translate { id: badgeTranslate; x: 0 }

                            SequentialAnimation {
                                loops: Animation.Infinite
                                running: searchIcon.isLoading && Settings.uiMotionAnimationsEnabled

                                ParallelAnimation {
                                    NumberAnimation {
                                        target: badgeTranslate; property: "x"
                                        from: 0; to: 3
                                        duration: 300; easing.type: Easing.InOutQuad
                                    }
                                    NumberAnimation {
                                        target: searchBadge; property: "scale"
                                        from: 1.0; to: 1.3
                                        duration: 300; easing.type: Easing.InOutQuad
                                    }
                                }
                                ParallelAnimation {
                                    NumberAnimation {
                                        target: badgeTranslate; property: "x"
                                        from: 3; to: -3
                                        duration: 600; easing.type: Easing.InOutQuad
                                    }
                                    NumberAnimation {
                                        target: searchBadge; property: "scale"
                                        from: 1.3; to: 1.3
                                        duration: 600
                                    }
                                }
                                ParallelAnimation {
                                    NumberAnimation {
                                        target: badgeTranslate; property: "x"
                                        from: -3; to: 0
                                        duration: 300; easing.type: Easing.InOutQuad
                                    }
                                    NumberAnimation {
                                        target: searchBadge; property: "scale"
                                        from: 1.3; to: 1.0
                                        duration: 300; easing.type: Easing.InOutQuad
                                    }
                                }

                                onRunningChanged: {
                                    if (!running) {
                                        badgeTranslate.x = 0;
                                        searchBadge.scale = 1.0;
                                    }
                                }
                            }

                            Image {
                                anchors.centerIn: parent
                                source: "image://colorimage/:/icons/icons/ui/search.svg?" + (searchIcon.isLoading ? palette.highlight : palette.text)
                                sourceSize.width: searchBadge.iconSize
                                sourceSize.height: searchBadge.iconSize
                                width: searchBadge.iconSize
                                height: searchBadge.iconSize
                            }
                        }
                    }
                    MultiEffect {
                        id: searchEffect

                        anchors.fill: parent
                        source: searchIconContent
                        saturation: searchIcon.isLoading && !Settings.uiMotionAnimationsEnabled ? 0.0 : -1.0

                        SequentialAnimation {
                            loops: Animation.Infinite
                            running: searchIcon.isLoading && Settings.uiMotionAnimationsEnabled

                            NumberAnimation {
                                target: searchEffect
                                property: "saturation"
                                from: -1.0
                                to: 0.0
                                duration: 800
                                easing.type: Easing.InOutQuad
                            }
                            NumberAnimation {
                                target: searchEffect
                                property: "saturation"
                                from: 0.0
                                to: -1.0
                                duration: 800
                                easing.type: Easing.InOutQuad
                            }

                            onRunningChanged: {
                                if (!running) {
                                    searchEffect.saturation = Qt.binding(function() {
                                        return (searchIcon.isLoading && !Settings.uiMotionAnimationsEnabled) ? 0.0 : -1.0;
                                    });
                                }
                            }
                        }
                    }
                }
                MatrixTextField {
                    id: searchField

                    Layout.fillWidth: true
                    enabled: searchButton.searchActive
                    hasClear: false
                    placeholderText: qsTr("Type to search in this room's messages")
                    radius: Nheko.paddingSmall

                    onEditingFinished: topBar.searchString = text
                }
                ImageButton {
                    Layout.preferredHeight: topBarAvatarSize
                    Layout.preferredWidth: topBarAvatarSize
                    ToolTip.text: qsTr("Close search")
                    ToolTip.visible: hovered
                    image: ":/icons/icons/ui/dismiss.svg"

                    onClicked: searchButton.searchActive = false
                }
            }
        }
    }

    onRoomIdChanged: {
        searchString = "";
        searchButton.searchActive = false;
        searchField.text = "";
        filterNotifications = false;
    }

    Shortcut {
        sequence: StandardKey.Find

        onActivated: searchButton.searchActive = !searchButton.searchActive
    }
}

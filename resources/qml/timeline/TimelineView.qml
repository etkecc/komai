// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import "../composer" as Composer
import "../emoji"
import "../room/components"
import "../ui"
import "../voip"
import "./components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

Item {
    id: timelineView

    required property var windowFocusBlurOverlay
    property var room: null
    property var roomPreview: null
    property bool shouldEffectsRun: false
    property bool showBackButton: false

    ComponentCatalog {
        id: componentCatalog
    }

    clip: true

    // focus message input on key press, but not on Ctrl-C and such.
    Keys.onPressed: event => {
        if (event.text && event.key !== Qt.Key_Enter && event.key !== Qt.Key_Return && !topBar.searchHasFocus) {
            TimelineManager.focusMessageInput();
            if (event.modifiers != Qt.ControlModifier) {
                room.input.setText(room.input.text + event.text);
            }
        }
    }
    onRoomChanged: if (room != null)
        room.triggerSpecialEffects()

    StickerPicker {
        id: timelineEmojiPopup

        emoji: true
    }
    Shortcut {
        sequences: [StandardKey.Close]

        onActivated: Rooms.resetCurrentRoom()
    }
    TimelineEmptyState {
        anchors.centerIn: parent
        visible: !room && !TimelineManager.waitingForFirstSync && (!roomPreview || !roomPreview.roomid)
    }
    TimelineFirstSyncSpinner {
        waitingForFirstSync: TimelineManager.waitingForFirstSync
    }
    ColumnLayout {
        id: timelineLayout

        anchors.fill: parent
        enabled: visible
        spacing: 0
        visible: room != null && !room.isSpace

        RoomHeader {
            id: topBar

            showBackButton: timelineView.showBackButton
            filteringInProgress: messageView.filteringInProgress
        }
        Rectangle {
            Layout.fillWidth: true
            color: Nheko.theme.separator
            implicitHeight: 1
            z: 3
        }
        Rectangle {
            id: msgView

            Layout.fillHeight: true
            Layout.fillWidth: true
            color: palette.base

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                StackLayout {
                    id: stackLayout

                    currentIndex: 0

                    Connections {
                        function onRoomChanged() {
                            stackLayout.currentIndex = 0;
                        }

                        target: timelineView
                    }
                    MessageView {
                        id: messageView
                        Layout.fillWidth: true
                        implicitHeight: msgView.height - typingIndicator.height
                        emojiPopup: timelineEmojiPopup
                        searchString: topBar.searchString
                        filterByNotifications: topBar.filterNotifications
                    }
                    Loader {
                        source: CallManager.isOnCall && CallManager.callType != Voip.VOICE && Settings.callsLegacyEnabled ? (Qt.platform.os != "windows" ? "../voip/VideoCall.qml" : "../voip/VideoCallD3D11.qml") : ""

                        onLoaded: TimelineManager.setVideoCallItem()
                    }
                }
                Composer.TypingIndicator {
                    id: typingIndicator
                    room: timelineView.room
                }
            }
        }
        CallInviteBar {
            id: callInviteBar

            Layout.fillWidth: true
            z: 3
        }
        ActiveCallBar {
            Layout.fillWidth: true
            z: 3
        }
        Composer.UploadBox {
        }
        Composer.ReplyPopup {
            id: replyPopup

            roundTopCorners: true
        }
        Repeater {
            model: room ? room.input.mentions : null

            delegate: TimelineMentionWarningBar {
                mention: modelData
                mentionIndex: index
                replyPopupVisible: replyPopup.visible
                room: timelineView.room
            }
        }
        Composer.MessageInputWarning {
            roundTopCorners: !replyPopup.visible && (room ? room.input.mentions.length : 0) == 0
            text: qsTr("The command /%1 is not recognized and will be sent as part of your message").arg(room ? room.input.currentCommand : "")
            visible: room ? room.input.containsInvalidCommand && !room.input.containsIncompleteCommand : false
        }
        Composer.MessageInputWarning {
            roundTopCorners: !replyPopup.visible && (room ? room.input.mentions.length : 0) == 0
            bubbleColor: Nheko.theme.orange
            text: qsTr("/%1 looks like an incomplete command. To send it anyway, add a space to the end of your message.").arg(room ? room.input.currentCommand : "")
            visible: room ? room.input.containsIncompleteCommand : false
        }
        Rectangle {
            Layout.fillWidth: true
            color: Nheko.theme.separator
            implicitHeight: 1
            z: 3
        }
        Composer.MessageInput {
        }
    }
    TimelinePreviewPane {
        room: timelineView.room
        roomPreview: timelineView.roomPreview
    }

    TimelineBackButton {
        roomModel: timelineView.room
        showBackButton: timelineView.showBackButton
    }
    TimelineEffects {
        id: timelineEffects

        anchors.fill: parent
        shouldEffectsRun: timelineView.shouldEffectsRun
    }
    NhekoDropArea {
        anchors.fill: parent
        roomid: room ? room.roomId : ""
    }
    Timer {
        id: effectsTimer

        interval: timelineEffects.maxLifespan
        repeat: false
        running: false

        onTriggered: {
            timelineEffects.removeParticles()
            shouldEffectsRun = false
        }
    }
    TimelineRoomEventConnections {
        room: timelineView.room
        timelineView: timelineView
        timelineEffects: timelineEffects
        effectsTimer: effectsTimer
        readReceiptsDialog: readReceiptsDialog
        timelineRoot: timelineRoot
        componentCatalog: componentCatalog
    }
}

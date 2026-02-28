// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import "../composer" as Composer
import "../emoji"
import "../room/components"
import "../ui"
import "./components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

Item {
    id: timelineView

    required property var windowFocusBlurOverlay
    property var dialogHost: null
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
        TimelineSeparator {
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
                        dialogHost: timelineView.dialogHost
                        componentCatalog: componentCatalog
                        searchString: topBar.searchString
                        filterByNotifications: topBar.filterNotifications
                    }
                    TimelineVideoCallLoader {
                        componentCatalog: componentCatalog
                    }
                }
                Composer.TypingIndicator {
                    id: typingIndicator
                    room: timelineView.room
                }
            }
        }
        TimelineCallStatusBars {
        }
        Composer.UploadBox {
        }
        Composer.ReplyPopup {
            id: replyPopup

            roundTopCorners: true
        }
        TimelineComposerWarnings {
            roomModel: timelineView.room
            replyPopupVisible: replyPopup.visible
        }
        TimelineSeparator {
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
        dialogHost: timelineView.dialogHost
        componentCatalog: componentCatalog
    }
}

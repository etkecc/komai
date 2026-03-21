// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../composer" as Composer
import QtQuick
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: timelineView

    required property var windowFocusBlurOverlay
    property var dialogHost: null
    property var room: null
    property var roomPreview: null
    property bool shouldEffectsRun: false
    property bool showBackButton: false
    readonly property bool perfDisableComposer: TimelineManager.perfUiFlagEnabled("disable_composer")
    readonly property bool perfDisableRoomHeader: TimelineManager.perfUiFlagEnabled("disable_room_header")
    readonly property bool perfDisableTimelineEffects: TimelineManager.perfUiFlagEnabled("disable_timeline_effects")
    readonly property bool perfDisableTimelineList: TimelineManager.perfUiFlagEnabled("disable_timeline_list")
    readonly property int composerBaselineHeight: Math.max(48, Komai.navigationRowHeight)

    ComponentCatalog {
        id: componentCatalog
    }

    clip: true

    // focus message input on key press, but not on Ctrl-C and such.
    Keys.onPressed: event => {
        if (event.text
                && event.key !== Qt.Key_Enter
                && event.key !== Qt.Key_Return
                && !topBar.searchHasFocus
                && !messageView.walkModeActive) {
            TimelineManager.focusMessageInput();
            if (event.modifiers != Qt.ControlModifier) {
                room.input.setText(room.input.text + event.text);
            }
        }
    }
    onRoomChanged: {
        if (room == null)
            return;

        const roomId = room.roomId;
        TimelineManager.markRoomSwitchPhase(roomId, "qml.timeline_view.room_changed");
        room.triggerSpecialEffects();
        Qt.callLater(function () {
            TimelineManager.markRoomSwitchPhase(roomId, "qml.timeline_view.next_tick");
        });
    }

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

            Layout.minimumHeight: visible ? implicitHeight : 0
            Layout.preferredHeight: visible ? implicitHeight : 0
            Layout.maximumHeight: visible ? implicitHeight : 0
            showBackButton: timelineView.showBackButton
            filteringInProgress: messageView.filteringInProgress
            visible: !timelineView.perfDisableRoomHeader
        }
        TimelineSeparator {
            Layout.minimumHeight: visible ? implicitHeight : 0
            Layout.preferredHeight: visible ? implicitHeight : 0
            Layout.maximumHeight: visible ? implicitHeight : 0
            visible: !timelineView.perfDisableRoomHeader
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
                        suppressRoomSwitchSpinner: TimelineManager.waitingForFirstSync
                        disableTimelineList: timelineView.perfDisableTimelineList
                        dialogHost: timelineView.dialogHost
                        componentCatalog: componentCatalog
                        composerAvailable: !timelineView.perfDisableComposer
                        selectionModeBar: walkModeBar
                        roomHeader: topBar
                        roomSearchHasFocus: topBar.searchHasFocus
                        searchString: topBar.searchString
                        filterByNotifications: topBar.filterNotifications
                    }
                    TimelineVideoCallLoader {
                        componentCatalog: componentCatalog
                    }
                }
                Composer.TypingIndicator {
                    id: typingIndicator
                    Layout.minimumHeight: visible ? implicitHeight : 0
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    Layout.maximumHeight: visible ? implicitHeight : 0
                    room: timelineView.room
                    visible: !timelineView.perfDisableComposer
                }
            }
        }
        // Keep collapsible sections collapsed at the integration boundary too,
        // so nested component regressions do not steal timeline height.
        TimelineCallStatusBars {
            id: callStatusBars

            Layout.minimumHeight: 0
            Layout.preferredHeight: !timelineView.perfDisableComposer && layoutVisible ? implicitHeight : 0
            Layout.maximumHeight: !timelineView.perfDisableComposer && layoutVisible ? implicitHeight : 0
        }
        Composer.UploadBox {
            id: uploadBox

            Layout.minimumHeight: 0
            Layout.preferredHeight: !timelineView.perfDisableComposer && layoutVisible ? implicitHeight : 0
            Layout.maximumHeight: !timelineView.perfDisableComposer && layoutVisible ? implicitHeight : 0
        }
        Composer.ReplyPopup {
            id: replyPopup

            Layout.minimumHeight: 0
            Layout.preferredHeight: !timelineView.perfDisableComposer && layoutVisible ? implicitHeight : 0
            Layout.maximumHeight: !timelineView.perfDisableComposer && layoutVisible ? implicitHeight : 0
            roundTopCorners: true
        }
        TimelineComposerWarnings {
            id: composerWarnings

            Layout.minimumHeight: 0
            Layout.preferredHeight: !timelineView.perfDisableComposer && layoutVisible && !messageView.walkModeActive ? implicitHeight : 0
            Layout.maximumHeight: !timelineView.perfDisableComposer && layoutVisible && !messageView.walkModeActive ? implicitHeight : 0
            roomModel: timelineView.room
            replyPopupVisible: replyPopup.visible
        }
        Item {
            id: bottomInputShell

            readonly property int contentHeight: messageView.walkModeActive
                ? timelineView.composerBaselineHeight
                : Math.max(timelineView.composerBaselineHeight, messageInput.implicitHeight)
            Layout.fillWidth: true
            Layout.minimumHeight: visible ? implicitHeight : 0
            Layout.preferredHeight: visible ? implicitHeight : 0
            Layout.maximumHeight: visible ? implicitHeight : 0
            implicitHeight: inputShellSeparator.implicitHeight + contentHeight
            visible: !timelineView.perfDisableComposer

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                TimelineSeparator {
                    id: inputShellSeparator

                    Layout.minimumHeight: implicitHeight
                    Layout.preferredHeight: implicitHeight
                    Layout.maximumHeight: implicitHeight
                }
                Composer.MessageInput {
                    id: messageInput

                    Layout.fillWidth: true
                    Layout.minimumHeight: visible ? timelineView.composerBaselineHeight : 0
                    Layout.preferredHeight: visible ? Math.max(timelineView.composerBaselineHeight, implicitHeight) : 0
                    Layout.maximumHeight: visible ? Math.max(timelineView.composerBaselineHeight, implicitHeight) : 0
                    room: timelineView.room
                    timelineRoot: timelineView.dialogHost
                    visible: !messageView.walkModeActive
                    walkModeActive: messageView.walkModeActive
                }
                TimelineWalkModeBar {
                    id: walkModeBar

                    Layout.fillWidth: true
                    Layout.minimumHeight: visible ? timelineView.composerBaselineHeight : 0
                    Layout.preferredHeight: visible ? timelineView.composerBaselineHeight : 0
                    Layout.maximumHeight: visible ? timelineView.composerBaselineHeight : 0
                    minimumHeight: timelineView.composerBaselineHeight
                    chatRoot: messageView
                    roomModel: timelineView.room
                    visible: messageView.walkModeActive
                }
            }
        }
    }
    Connections {
        function onComposerInteractionRequested() {
            if (messageView.walkModeActive) {
                messageView.exitWalkMode({
                    "focusComposer": false
                });
                Qt.callLater(function () {
                    messageInput.focusTextInput();
                });
            }
        }

        target: messageInput
    }
    Connections {
        function onWalkModeOlderChunkRequested() {
            messageView.enterWalkModeAndMoveTowardOlderEventsByChunk();
        }

        target: messageInput
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
        visible: !timelineView.perfDisableTimelineEffects
    }
    KomaiDropArea {
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

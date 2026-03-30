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
    readonly property bool useMatrixRoomView: !room && roomPreview && roomPreview.isMatrixSummary
    readonly property int composerBaselineHeight: Math.max(48, Komai.navigationRowHeight)
    readonly property var legacyTimeline: legacyTimelineLoader.item
    readonly property var legacyRoomHeader: legacyTimeline ? legacyTimeline.roomHeader : null
    readonly property var legacyMessageView: legacyTimeline ? legacyTimeline.messageViewItem : null
    readonly property var legacyMessageInput: legacyTimeline ? legacyTimeline.messageInputItem : null
    readonly property var matrixTimelineShell: matrixRoomLoader.item
    readonly property var matrixTimeline: matrixTimelineShell ? matrixTimelineShell.roomView : null
    readonly property bool activeSearchHasFocus: useMatrixRoomView
        ? (!!matrixHeaderPane && !!matrixHeaderPane.searchHasFocus)
        : (!!legacyRoomHeader && !!legacyRoomHeader.searchHasFocus)
    readonly property bool activeWalkModeActive: useMatrixRoomView
        ? (!!matrixTimeline && !!matrixTimeline.walkModeActive)
        : (!!legacyMessageView && !!legacyMessageView.walkModeActive)
    readonly property var notificationAreaItem: matrixRoomLoader.active && matrixTimeline
        ? (matrixTimeline.notificationAreaItem ? matrixTimeline.notificationAreaItem : matrixTimeline)
        : (legacyTimeline ? legacyTimeline.notificationAreaItem : null)
    readonly property var notificationAvoidBottomItem: matrixRoomLoader.active && matrixTimeline
        ? matrixTimeline.composerShell
        : (legacyTimeline ? legacyTimeline.notificationAvoidBottomItem : null)

    ComponentCatalog {
        id: componentCatalog
    }

    QtObject {
        id: matrixTimelineHost

        readonly property var dialogHost: timelineView.dialogHost
        readonly property real listViewDisplayMargin: timelineView.matrixTimeline
            && timelineView.matrixTimeline.listViewDisplayMargin !== undefined
            ? Number(timelineView.matrixTimeline.listViewDisplayMargin)
            : 0
        readonly property real listViewCacheBuffer: timelineView.matrixTimeline
            && timelineView.matrixTimeline.listViewCacheBuffer !== undefined
            ? Number(timelineView.matrixTimeline.listViewCacheBuffer)
            : 320
        readonly property bool roomSwitchInProgress: timelineView.matrixTimeline
            ? !!timelineView.matrixTimeline.roomSwitchInProgress
            : false

        function clearSearch() {
            if (timelineView.matrixTimeline && typeof timelineView.matrixTimeline.clearSearch === "function")
                timelineView.matrixTimeline.clearSearch();
        }

        function openWalkModeHelpDialog() {
            return timelineView.matrixTimeline
                && typeof timelineView.matrixTimeline.openWalkModeHelpDialog === "function"
                ? timelineView.matrixTimeline.openWalkModeHelpDialog()
                : false;
        }
    }

    clip: true

    // focus message input on key press, but not on Ctrl-C and such.
    Keys.onPressed: event => {
        if (room
                && event.text
                && event.key !== Qt.Key_Enter
                && event.key !== Qt.Key_Return
                && !activeSearchHasFocus
                && !activeWalkModeActive) {
            TimelineManager.focusMessageInput();
            if (event.modifiers != Qt.ControlModifier) {
                room.input.setText(room.input.text + event.text);
            }
        } else if (useMatrixRoomView
                && event.text
                && event.key !== Qt.Key_Enter
                && event.key !== Qt.Key_Return
                && !activeSearchHasFocus
                && !activeWalkModeActive) {
            TimelineManager.focusMessageInput();
            if (event.modifiers !== Qt.ControlModifier && matrixTimeline)
                matrixTimeline.appendText(event.text);
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
        visible: !room && !useMatrixRoomView && !TimelineManager.waitingForFirstSync && (!roomPreview || !roomPreview.roomid)
    }
    TimelineFirstSyncSpinner {
        waitingForFirstSync: TimelineManager.waitingForFirstSync
    }
    MatrixRoomHeaderPane {
        id: matrixHeaderPane

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        roomPreview: timelineView.roomPreview
        showBackButton: timelineView.showBackButton
        perfDisableRoomHeader: timelineView.perfDisableRoomHeader
        headerRoomModel: matrixTimeline ? matrixHeaderRoomModel : null
        visible: timelineView.useMatrixRoomView
    }
    MatrixRoomRouteModels {
        id: matrixRoomRouteModels

        roomPreview: timelineView.roomPreview
    }
    MatrixRoomHeaderModel {
        id: matrixHeaderRoomModel

        rootItem: matrixTimeline
        roomPreview: timelineView.roomPreview
        dialogRoomModel: matrixRoomRouteModels.dialogRoomModel
    }
    MatrixRoomComposerSupport {
        id: matrixRoomComposerSupport

        rootItem: matrixTimeline
        roomPreview: timelineView.roomPreview
        dialogRoomModel: matrixRoomRouteModels.dialogRoomModel
    }
    MatrixRoomMessageActionsModel {
        id: matrixRoomMessageActionsModel

        rootItem: matrixTimeline
        roomPreview: timelineView.roomPreview
        dialogRoomModel: matrixRoomRouteModels.dialogRoomModel
        headerRoomModel: matrixHeaderRoomModel
    }
    MatrixRoomDialogSupport {
        id: matrixRoomDialogSupport

        rootItem: matrixTimeline
        roomPreview: timelineView.roomPreview
        chatRoot: matrixTimelineHost
        timelineRoot: timelineView.dialogHost
        emojiPopup: timelineEmojiPopup
        filteredTimeline: null
        timelineList: matrixTimeline ? matrixTimeline.timelineListItem : null
        messageActionsDefaultRoomModel: matrixRoomMessageActionsModel
        dialogRoomModel: matrixRoomRouteModels.dialogRoomModel
        forwardRoomModel: matrixRoomRouteModels.forwardRoomModel
    }
    Component {
        id: legacyTimelineComponent

        ColumnLayout {
            property alias roomHeader: topBar
            property alias messageViewItem: messageView
            property alias messageInputItem: messageInput
            readonly property var notificationAreaItem: msgView
            readonly property var notificationAvoidBottomItem: bottomInputShell.visible ? bottomInputShell : null

            anchors.fill: parent
            enabled: visible
            spacing: 0
            visible: true

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
                commandPickerVisible: messageInput.commandPickerVisible
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
                        selectionModeRoot: messageView
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
                        visible: messageView.walkModeActive
                    }
                }
            }
        }
    }
    Component {
        id: matrixTimelineComponent

        ColumnLayout {
            property alias roomView: matrixRoomView

            anchors.fill: parent
            spacing: 0

            MatrixRoomView {
                id: matrixRoomView

                Layout.fillHeight: true
                Layout.fillWidth: true
                roomPreview: timelineView.roomPreview
                dialogSupport: matrixRoomDialogSupport
                messageActionsRoomModel: matrixRoomMessageActionsModel
                composerInputController: matrixRoomComposerSupport.composerInputController
                externalDialogHost: timelineView.dialogHost
                externalHeaderPane: matrixHeaderPane
                externalComposerPane: matrixComposerPane
            }

            MatrixRoomComposerPane {
                id: matrixComposerPane

                Layout.fillWidth: true
                rootItem: matrixRoomView
                uploadsController: matrixRoomComposerSupport.uploadsController
                composerRoom: matrixRoomComposerSupport.composerRoom
                composerInputController: matrixRoomComposerSupport.composerInputController
                timelineRoot: timelineView.dialogHost
            }
        }
    }
    Loader {
        id: legacyTimelineLoader

        anchors.fill: parent
        active: timelineView.room != null && !timelineView.room.isSpace
        sourceComponent: legacyTimelineComponent
        visible: active
    }
    Loader {
        id: matrixRoomLoader

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: matrixHeaderPane.visible ? matrixHeaderPane.bottom : parent.top
        active: timelineView.useMatrixRoomView
        sourceComponent: matrixTimelineComponent
        visible: active
    }
    Connections {
        function onComposerInteractionRequested() {
            if (legacyMessageView && legacyMessageView.walkModeActive) {
                legacyMessageView.exitWalkMode({
                    "focusComposer": false
                });
                Qt.callLater(function () {
                    if (legacyMessageInput)
                        legacyMessageInput.focusTextInput();
                });
            }
        }

        function onTextInputActiveFocusChanged() {
            if (legacyMessageInput
                    && legacyMessageView
                    && legacyMessageInput.textInputActiveFocus
                    && legacyMessageView.buttonActionsOpen) {
                legacyMessageView.dismissButtonActions();
            }
        }

        target: legacyMessageInput
    }
    Connections {
        function onButtonActionsOpenChanged() {
            if (legacyMessageView
                    && legacyMessageInput
                    && legacyMessageView.buttonActionsOpen
                    && legacyMessageInput.textInputActiveFocus) {
                legacyMessageView.forceActiveFocus();
            }
        }

        target: legacyMessageView
    }
    TimelinePreviewPane {
        room: timelineView.room
        roomPreview: timelineView.useMatrixRoomView ? null : timelineView.roomPreview
    }

    TimelineBackButton {
        roomModel: timelineView.useMatrixRoomView ? null : timelineView.room
        showBackButton: timelineView.showBackButton && !timelineView.useMatrixRoomView
    }
    TimelineKeyboardShortcuts {
        chatList: timelineView.useMatrixRoomView
            ? (matrixTimeline ? matrixTimeline.timelineListItem : null)
            : null
        chatRoot: timelineView.useMatrixRoomView ? matrixTimeline : null
        roomModel: null
        allowEscape: timelineView.useMatrixRoomView
            && matrixTimeline
            && !matrixTimeline.hasOpenOverlayDialog
            && (matrixTimeline.walkModeActive
                || matrixTimeline.hasSelectedEvents
                || matrixTimeline.hasFocusedEvent)
    }
    TimelineEffects {
        id: timelineEffects

        anchors.fill: parent
        shouldEffectsRun: timelineView.shouldEffectsRun
        animationsEnabled: Settings.uiMotionAnimationsEnabled
        visible: !timelineView.perfDisableTimelineEffects
    }
    KomaiDropArea {
        anchors.fill: parent
        roomid: room ? room.roomId : ""
    }
    Timer {
        id: effectsTimer

        interval: timelineEffects.maxEffectDuration
        repeat: false
        running: false

        onTriggered: {
            shouldEffectsRun = false
            timelineEffects.removeParticles()
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

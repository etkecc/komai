// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: timelineView

    required property var windowFocusBlurOverlay
    property var tabController: null
    property var dialogHost: null
    property Item roomListLastActionButton: null
    property var roomPreview: null
    property bool shouldEffectsRun: false
    property bool showBackButton: false
    readonly property bool perfDisableComposer: TimelineManager.perfUiFlagEnabled("disable_composer")
    readonly property bool perfDisableRoomHeader: TimelineManager.perfUiFlagEnabled("disable_room_header")
    readonly property bool perfDisableTimelineEffects: TimelineManager.perfUiFlagEnabled("disable_timeline_effects")
    readonly property bool perfDisableTimelineList: TimelineManager.perfUiFlagEnabled("disable_timeline_list")
    readonly property bool useMatrixRoomView: roomPreview && roomPreview.isMatrixSummary
    readonly property int composerBaselineHeight: Math.max(48, Komai.navigationRowHeight)
    readonly property var matrixTimelineShell: _activePoolEntry
    readonly property var matrixTimeline: matrixTimelineShell ? matrixTimelineShell.roomView : null
    readonly property var notificationAreaItem: _activePoolEntry && matrixTimeline
        ? (matrixTimeline.notificationAreaItem ? matrixTimeline.notificationAreaItem : matrixTimeline)
        : null
    readonly property var notificationAvoidBottomItem: _activePoolEntry && matrixTimeline
        ? matrixTimeline.composerShell
        : null

    // Timeline cache pool: keeps recently-visited room timelines alive (hidden)
    // so switching back is fast (no QML component recreation).
    property var _activePoolEntry: null
    property var _poolEntries: ({})
    property var _poolLru: []
    property int _poolMaxSize: 20
    property string _poolCurrentRoomId: useMatrixRoomView && roomPreview
        ? String(roomPreview.roomid || "") : ""
    on_PoolCurrentRoomIdChanged: _poolSwitchTo(_poolCurrentRoomId)

    function _poolSwitchTo(roomId) {
        _poolDeactivateCurrent();

        if (!roomId)
            return;

        var entry = _poolGetOrCreate(roomId);
        var isReactivation = (entry.roomView.activeRoomId === roomId);
        var poolSize = Object.keys(_poolEntries).length;

        entry.poolSlotActive = true;
        _activePoolEntry = entry;

        if (isReactivation) {
            console.info("[timeline-pool] hit room=" + roomId
                         + " poolSize=" + poolSize);
            entry.roomView.handlePoolReactivation();
        } else {
            console.info("[timeline-pool] miss room=" + roomId
                         + " poolSize=" + poolSize);
            entry.roomView.activeRoomId = roomId;
        }
    }

    function _poolDeactivateCurrent() {
        if (_activePoolEntry) {
            _activePoolEntry.poolSlotActive = false;
            _activePoolEntry = null;
        }
    }

    function _poolGetOrCreate(roomId) {
        var entry = _poolEntries[roomId];
        if (entry) {
            _poolTouchLru(roomId);
            return entry;
        }

        _poolEvictIfNeeded();

        entry = matrixTimelineComponent.createObject(timelinePoolContainer, {});
        _poolEntries[roomId] = entry;
        _poolTouchLru(roomId);
        return entry;
    }

    function _poolTouchLru(roomId) {
        var idx = _poolLru.indexOf(roomId);
        if (idx !== -1)
            _poolLru.splice(idx, 1);
        _poolLru.unshift(roomId);
    }

    function _poolEvictIfNeeded() {
        while (Object.keys(_poolEntries).length >= _poolMaxSize) {
            var evicted = false;
            for (var i = _poolLru.length - 1; i >= 0; i--) {
                var candidate = _poolLru[i];
                if (tabController && tabController.findTab(candidate) !== -1)
                    continue;
                console.info("[timeline-pool] evict room=" + candidate
                             + " poolSize=" + Object.keys(_poolEntries).length);
                _poolRemove(candidate);
                evicted = true;
                break;
            }
            if (!evicted)
                break;
        }
    }

    function _poolRemove(roomId) {
        var entry = _poolEntries[roomId];
        if (entry) {
            entry.destroy();
            delete _poolEntries[roomId];
        }
        var idx = _poolLru.indexOf(roomId);
        if (idx !== -1)
            _poolLru.splice(idx, 1);
    }

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

    // Route printable text into the composer when focus is elsewhere in the room view.
    Keys.onPressed: event => {
        if (useMatrixRoomView && matrixTimeline)
            matrixTimeline.handleComposerTextKey(event);
    }

    StickerPicker {
        id: timelineEmojiPopup

        emoji: true
    }
    Shortcut {
        sequences: [StandardKey.Close]
        // When tabs are open, Ctrl+W is handled by ChatPage's tab shortcut instead.
        enabled: !tabController || tabController.tabs.count === 0

        onActivated: Rooms.resetCurrentRoom()
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: !useMatrixRoomView && !TimelineManager.waitingForFirstSync && (!roomPreview || !roomPreview.roomid)

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            TimelineEmptyState {
                anchors.centerIn: parent
                dialogHost: timelineView.dialogHost
            }
        }

        AttributionFooter {}
    }
    TimelineFirstSyncSpinner {
        waitingForFirstSync: TimelineManager.waitingForFirstSync
    }
    MatrixRoomHeaderPane {
        id: matrixHeaderPane

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        roomListLastActionButton: timelineView.roomListLastActionButton
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
        permissions: matrixRoomPermissions
    }
    MatrixRoomPermissions {
        id: matrixRoomPermissions

        roomId: timelineView.useMatrixRoomView && timelineView.roomPreview
            ? String(timelineView.roomPreview.roomid || "")
            : ""
    }
    MatrixRoomComposerSupport {
        id: matrixRoomComposerSupport

        rootItem: matrixTimeline
        roomPreview: timelineView.roomPreview
        dialogRoomModel: matrixRoomRouteModels.dialogRoomModel
        headerRoomModel: matrixHeaderRoomModel
        permissions: matrixRoomPermissions
    }
    MatrixRoomMessageActionsModel {
        id: matrixRoomMessageActionsModel

        rootItem: matrixTimeline
        roomPreview: timelineView.roomPreview
        dialogRoomModel: matrixRoomRouteModels.dialogRoomModel
        headerRoomModel: matrixHeaderRoomModel
        permissions: matrixRoomPermissions
    }
    MatrixRoomDialogSupport {
        id: matrixRoomDialogSupport

        rootItem: matrixTimeline
        roomPreview: timelineView.roomPreview
        chatRoot: matrixTimelineHost
        timelineRoot: timelineView.dialogHost
        emojiPopup: timelineEmojiPopup
        filteredTimeline: matrixTimeline ? matrixTimeline.filteredTimeline : null
        timelineList: matrixTimeline ? matrixTimeline.timelineListItem : null
        messageActionsDefaultRoomModel: matrixRoomMessageActionsModel
        dialogRoomModel: matrixRoomRouteModels.dialogRoomModel
        forwardRoomModel: matrixRoomRouteModels.forwardRoomModel
    }
    Component {
        id: matrixTimelineComponent

        ColumnLayout {
            property alias roomView: matrixRoomView
            property bool poolSlotActive: false

            anchors.fill: parent
            spacing: 0
            visible: poolSlotActive

            MatrixRoomView {
                id: matrixRoomView

                Layout.fillHeight: true
                Layout.fillWidth: true
                poolActive: parent.poolSlotActive
                roomPreview: parent.poolSlotActive ? timelineView.roomPreview : null
                dialogSupport: matrixRoomDialogSupport
                messageActionsRoomModel: matrixRoomMessageActionsModel
                composerInputController: matrixRoomComposerSupport.composerInputController
                externalDialogHost: timelineView.dialogHost
                externalHeaderPane: matrixHeaderPane
                externalComposerPane: matrixComposerPane
                composerRoom: matrixRoomComposerSupport.composerRoom
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
    Item {
        id: timelinePoolContainer

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: matrixHeaderPane.visible ? matrixHeaderPane.bottom : parent.top
        visible: timelineView.useMatrixRoomView
    }

    Connections {
        target: tabController

        function onAboutToSwitchRoom() {
            timelineView._poolDeactivateCurrent();
        }
    }
    TimelinePreviewPane {
        roomPreview: timelineView.useMatrixRoomView ? null : timelineView.roomPreview
    }

    TimelineBackButton {
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
            && matrixTimeline.canHandleEscape()
            && mainWindow.depth <= 1
            && !timelineRoot.activeMediaOverlay
    }
    TimelineEffects {
        id: timelineEffects

        anchors.fill: parent
        shouldEffectsRun: timelineView.shouldEffectsRun
        animationsEnabled: Settings.uiMotionAnimationsEnabled
        visible: timelineView.shouldEffectsRun && !timelineView.perfDisableTimelineEffects
    }
    Connections {
        target: TimelineManager.matrixTimelineModel

        function onSpecialEffectsTriggered(effectNames) {
            if (timelineView.perfDisableTimelineEffects || !effectNames || effectNames.length === 0)
                return;

            timelineEffects.pulseEffects(effectNames);
            timelineView.shouldEffectsRun = true;
            effectsTimer.interval = timelineEffects.durationForEffects(effectNames);
            effectsTimer.restart();
        }
    }
    KomaiDropArea {
        anchors.fill: parent
        roomid: roomPreview && roomPreview.roomid ? roomPreview.roomid : ""
    }
    Timer {
        id: effectsTimer

        interval: timelineEffects.maxEffectDuration
        repeat: false
        running: false

        onTriggered: {
            timelineEffects.removeParticles();
            timelineView.shouldEffectsRun = false;
        }
    }
}

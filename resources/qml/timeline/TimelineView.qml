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
    readonly property var notificationAvoidBottomItem: matrixComposerPane.visible
        ? matrixComposerPane.composerShell
        : null

    // Timeline cache pool: keeps recently-visited room timelines alive (hidden)
    // so switching back is fast (no QML component recreation).
    property var _activePoolEntry: null
    property var _poolEntries: ({})
    property var _poolLru: []
    property int _poolMaxSize: Settings.navigationTabsMaxRecentlyClosedTimelines
    property string _poolCurrentRoomId: useMatrixRoomView && roomPreview
        ? String(roomPreview.roomid || "") : ""
    on_PoolMaxSizeChanged: _poolTrimExcess()
    on_PoolCurrentRoomIdChanged: {
        // Skip if the aboutToSwitchRoom handler already activated this room.
        if (_activePoolEntry && _poolEntries[_poolCurrentRoomId] === _activePoolEntry) {
            console.info("[timeline-pool] _poolCurrentRoomIdChanged SKIPPED (already active) newId="
                         + _poolCurrentRoomId);
            return;
        }
        console.info("[timeline-pool] _poolCurrentRoomIdChanged PROCEEDING newId="
                     + _poolCurrentRoomId);
        _poolSwitchTo(_poolCurrentRoomId);
    }

    function _poolSwitchTo(roomId) {
        // Already showing this room — nothing to do.  This guard prevents
        // redundant deactivation/reactivation when both the roomPreview
        // binding and the roomSwitched signal resolve to the same room.
        if (roomId && _activePoolEntry && _poolEntries[roomId] === _activePoolEntry)
            return;

        console.info("[timeline-pool] switchTo room=" + roomId
                     + " useMatrixRoomView=" + useMatrixRoomView
                     + " hasActiveEntry=" + !!_activePoolEntry
                     + " hasEntry=" + !!_poolEntries[roomId]);
        _poolDeactivateCurrent();

        if (!roomId) {
            _poolTrimExcess();
            return;
        }

        var entry = _poolGetOrCreate(roomId);
        var isReactivation = (entry.roomView.activeRoomId === roomId);
        var poolSize = Object.keys(_poolEntries).length;

        // On the cold path the entry's `MatrixRoomView` is brand new and its
        // `activeRoomId` is still empty. Set it *before* `poolSlotActive`
        // flips true: the layout cascade triggered by becoming visible can
        // fire stray signals (e.g. a transient `atYEnd=true` on the empty
        // ListView) whose slots query `root.activeRoomId`. Setting the room
        // id last leaves a window where those slots see "" and route requests
        // to whichever target the runtime currently has selected — typically
        // the previous room's thread, leaking pagination on tab switch.
        if (!isReactivation)
            entry.roomView.activeRoomId = roomId;

        // Mirror the synchronous-assignment pattern in `_poolDeactivateCurrent`:
        // set `poolActive` directly so timeline-maintenance gates flip on
        // before any further QML work. The binding from
        // `matrixTimelineComponent` was removed; we own this property's value
        // explicitly here.
        entry.roomView.poolActive = true;
        entry.poolSlotActive = true;
        _activePoolEntry = entry;

        if (isReactivation) {
            var preserveScroll = entry.preserveScrollOnReactivation;
            entry.preserveScrollOnReactivation = false;
            var modelCount = entry.roomView.perRoomModel ? entry.roomView.perRoomModel.count : 0;
            console.info("[timeline-pool] hit room=" + roomId
                         + " poolSize=" + poolSize
                         + " modelItems=" + modelCount
                         + " preserveScroll=" + preserveScroll);
            entry.roomView.handlePoolReactivation(preserveScroll);
        } else {
            console.info("[timeline-pool] miss room=" + roomId
                         + " poolSize=" + poolSize);
        }
    }

    function _poolDeactivateCurrent() {
        if (_activePoolEntry) {
            // If the room still has an open tab, preserve scroll on next
            // reactivation (tab switch).  If the tab was closed or reused,
            // findTab returns -1 and scroll resets to bottom.
            var roomId = _activePoolEntry.roomView.activeRoomId;
            _activePoolEntry.preserveScrollOnReactivation = tabController
                && tabController.findTab(roomId) !== -1;
            // Synchronously kill timeline maintenance on the outgoing view
            // *before* flipping `poolSlotActive`. The visibility/layout
            // teardown that follows can fire stray geometry signals
            // (notably `atYEndChanged` on the inner ListView) before Qt
            // finishes propagating the bound `poolActive` change to gates
            // that depend on it. Direct assignment makes the property
            // observable to those gates immediately.
            _activePoolEntry.roomView.poolActive = false;
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
        entry.roomView.perRoomModel = TimelineManager.ensureModelForRoom(roomId);
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
        TimelineManager.releaseModelForRoom(roomId);
    }

    function _poolTrimExcess() {
        var poolSize = Object.keys(_poolEntries).length;
        while (Object.keys(_poolEntries).length > _poolMaxSize) {
            var evicted = false;
            for (var i = _poolLru.length - 1; i >= 0; i--) {
                var candidate = _poolLru[i];
                if (_poolEntries[candidate] === _activePoolEntry)
                    continue;
                if (tabController && tabController.findTab(candidate) !== -1)
                    continue;
                _poolRemove(candidate);
                evicted = true;
                break;
            }
            if (!evicted)
                break;
        }
        if (poolSize !== Object.keys(_poolEntries).length)
            _poolMemoryTrimTimer.restart();
    }

    Timer {
        id: _poolMemoryTrimTimer

        interval: 500
        onTriggered: {
            gc();
            TimelineManager.trimProcessMemory();
        }
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
        id: newTabLayout

        anchors.fill: parent
        spacing: 0
        visible: !_activePoolEntry && !useMatrixRoomView && !TimelineManager.waitingForFirstSync && (!roomPreview || !roomPreview.roomid)

        NewTabPage {
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: newTabLayout.visible
            dialogHost: timelineView.dialogHost
            tabController: timelineView.tabController
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
        visible: !!timelineView._activePoolEntry || timelineView.useMatrixRoomView

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
            property bool preserveScrollOnReactivation: false

            anchors.fill: parent
            spacing: 0
            visible: poolSlotActive

            MatrixRoomView {
                id: matrixRoomView

                Layout.fillHeight: true
                Layout.fillWidth: true
                // `poolActive` is set directly in JS by the pool functions
                // (see `_poolSwitchTo` / `_poolDeactivateCurrent`) so its
                // value flips synchronously, ahead of the layout signals
                // that follow `poolSlotActive` changes. A QML binding here
                // would re-evaluate too lazily on the cold path.
                roomPreview: parent.poolSlotActive ? timelineView.roomPreview : null
                dialogSupport: matrixRoomDialogSupport
                messageActionsRoomModel: matrixRoomMessageActionsModel
                composerInputController: matrixRoomComposerSupport.composerInputController
                externalDialogHost: timelineView.dialogHost
                externalHeaderPane: matrixHeaderPane
                externalComposerPane: matrixComposerPane
                composerRoom: matrixRoomComposerSupport.composerRoom
            }
        }
    }
    ThreadViewBar {
        id: threadViewBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: matrixHeaderPane.visible ? matrixHeaderPane.bottom : parent.top
        roomId: timelineView.useMatrixRoomView && timelineView.roomPreview
            ? String(timelineView.roomPreview.roomid || "") : ""
        visible: active && (!!timelineView._activePoolEntry || timelineView.useMatrixRoomView)
    }
    // Legacy (1:1 GStreamer) call bar. Sits at the top of the timeline region
    // (below the room header / thread bar, above the timeline) to match the
    // Element Call panel placement; the timeline reflows below it. Persists
    // across room switches via the global CallManager state inside.
    TimelineCallStatusBars {
        id: legacyCallBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: threadViewBar.visible ? threadViewBar.bottom
            : (matrixHeaderPane.visible ? matrixHeaderPane.bottom : parent.top)
        height: visible ? implicitHeight : 0
        z: 3
    }
    // Element Call "active call" bar — shown when an EC call is live in a
    // DIFFERENT room than the one on screen, so the call is reachable from any
    // room (click to jump back; End call hangs up from here). Mutually
    // exclusive with the in-room EC panel below, which shows when you ARE in
    // the call's room. Sits just below the legacy call bar.
    ElementCallActiveBar {
        id: elementCallActiveBar

        tabController: timelineView.tabController
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: legacyCallBar.visible ? legacyCallBar.bottom
            : (threadViewBar.visible ? threadViewBar.bottom
                : (matrixHeaderPane.visible ? matrixHeaderPane.bottom : parent.top))
        height: visible ? implicitHeight : 0
        visible: ElementCall.supported && ElementCall.active
            && Rooms.currentRoomId !== ElementCall.activeRoomId
        z: 3
    }
    // Element Call "incoming call" ring bar — shown from any room while a 1:1 EC
    // call is ringing us (not yet joined/declined). Sits just below the active
    // call bar; Join opens the call, Decline silences it.
    ElementCallRingBar {
        id: elementCallRingBar

        tabController: timelineView.tabController
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: elementCallActiveBar.visible ? elementCallActiveBar.bottom
            : (legacyCallBar.visible ? legacyCallBar.bottom
                : (threadViewBar.visible ? threadViewBar.bottom
                    : (matrixHeaderPane.visible ? matrixHeaderPane.bottom : parent.top)))
        height: visible ? implicitHeight : 0
        visible: ElementCall.supported && ElementCall.incomingRingActive
        z: 3
    }
    Item {
        id: timelinePoolContainer

        anchors.bottom: matrixComposerPane.visible ? matrixComposerPane.top : parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        // Sits below the Element Call panel (when a call shows here) and the
        // legacy call bar, so the timeline (and its scrollbar) reflow into the
        // remaining space rather than being overlaid; otherwise it spans the
        // usual region.
        anchors.top: elementCallPanelLoader.inCallRoom
            ? elementCallPanelLoader.bottom
            : (elementCallRingBar.visible ? elementCallRingBar.bottom
                : (elementCallActiveBar.visible ? elementCallActiveBar.bottom
                    : (legacyCallBar.visible ? legacyCallBar.bottom
                        : (threadViewBar.visible ? threadViewBar.bottom
                            : (matrixHeaderPane.visible ? matrixHeaderPane.bottom : parent.top)))))
        visible: !!timelineView._activePoolEntry || timelineView.useMatrixRoomView
    }
    TimelineVideoCallLoader {
        anchors.fill: timelinePoolContainer
        componentCatalog: componentCatalog
        z: 2
    }
    // In-room Element Call surface. Sits at the top of the timeline region (below
    // the room header / thread bar, above the timeline) so the timeline reflows
    // below it as a column instead of being overlaid. Kept loaded for the whole
    // call (so the live WebRTC session survives room switches) but only shown
    // while the call's room is the one on screen; otherwise it is hidden with the
    // call still running, the same way ActiveCallBar persists across switches.
    // Driven by the always-compiled ElementCall singleton; the panel itself only
    // exists on ELEMENT_CALL builds, so this Loader points at it by path (no hard
    // type reference) and stays inactive when support is absent.
    Loader {
        id: elementCallPanelLoader

        readonly property bool inCallRoom: ElementCall.activeRoomId.length > 0
            && Rooms.currentRoomId === ElementCall.activeRoomId
        // Height of the whole timeline region (header/thread bar bottom to the
        // composer top), independent of our own height so the panel's fractional
        // expanded height does not feed back into the layout.
        readonly property real regionHeight:
            (matrixComposerPane.visible ? matrixComposerPane.y : timelineView.height)
            - elementCallPanelLoader.y

        active: ElementCall.supported && ElementCall.active
        source: "qrc:/resources/qml/voip/ElementCallPanel.qml"

        anchors.top: legacyCallBar.visible ? legacyCallBar.bottom
            : (threadViewBar.visible ? threadViewBar.bottom
                : (matrixHeaderPane.visible ? matrixHeaderPane.bottom : parent.top))
        anchors.left: parent.left
        anchors.right: parent.right
        height: (item && inCallRoom) ? item.panelHeight : 0
        visible: inCallRoom

        onLoaded: item.availableHeight = Qt.binding(function () {
            return elementCallPanelLoader.regionHeight;
        })
    }
    MatrixRoomComposerPane {
        id: matrixComposerPane

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        rootItem: timelineView.matrixTimeline
        uploadsController: matrixRoomComposerSupport.uploadsController
        composerRoom: matrixRoomComposerSupport.composerRoom
        composerInputController: matrixRoomComposerSupport.composerInputController
        timelineRoot: timelineView.dialogHost
        visible: !!timelineView._activePoolEntry || timelineView.useMatrixRoomView
    }

    Connections {
        target: tabController

        function onAboutToSwitchRoom() {
            timelineView._poolDeactivateCurrent();
        }

        function onRoomSwitched(newRoomId) {
            if (newRoomId)
                timelineView._poolSwitchTo(newRoomId);
        }

        function onTabClosed(roomId) {
            var entry = timelineView._poolEntries[roomId];
            if (entry)
                entry.preserveScrollOnReactivation = false;
            timelineView._poolTrimExcess();
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
        allowEscape: timelineView.useMatrixRoomView
            && matrixTimeline
            && matrixTimeline.canHandleEscape()
            && mainWindow.depth <= 1
            && !timelineRoot.activeMediaOverlay
            // While the Element Call panel is fullscreen, Escape must leave
            // fullscreen, not run the timeline's escape cascade. This window-wide
            // shortcut would otherwise swallow the key before the panel's
            // key-catcher sees it.
            && !(elementCallPanelLoader.item && elementCallPanelLoader.item.fullscreen)
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
            if (timelineView.perfDisableTimelineEffects
                || !Settings.timelineMediaEffectsEnabled
                || !effectNames || effectNames.length === 0)
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

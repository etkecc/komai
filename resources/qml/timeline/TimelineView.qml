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
    property var dialogHost: null
    property var roomPreview: null
    property bool shouldEffectsRun: false
    property bool showBackButton: false
    readonly property bool perfDisableComposer: TimelineManager.perfUiFlagEnabled("disable_composer")
    readonly property bool perfDisableRoomHeader: TimelineManager.perfUiFlagEnabled("disable_room_header")
    readonly property bool perfDisableTimelineEffects: TimelineManager.perfUiFlagEnabled("disable_timeline_effects")
    readonly property bool perfDisableTimelineList: TimelineManager.perfUiFlagEnabled("disable_timeline_list")
    readonly property bool useMatrixRoomView: roomPreview && roomPreview.isMatrixSummary
    readonly property int composerBaselineHeight: Math.max(48, Komai.navigationRowHeight)
    readonly property var matrixTimelineShell: matrixRoomLoader.item
    readonly property var matrixTimeline: matrixTimelineShell ? matrixTimelineShell.roomView : null
    readonly property bool activeSearchHasFocus: !!matrixHeaderPane && !!matrixHeaderPane.searchHasFocus
    readonly property bool activeWalkModeActive: !!matrixTimeline && !!matrixTimeline.walkModeActive
    readonly property var notificationAreaItem: matrixRoomLoader.active && matrixTimeline
        ? (matrixTimeline.notificationAreaItem ? matrixTimeline.notificationAreaItem : matrixTimeline)
        : null
    readonly property var notificationAvoidBottomItem: matrixRoomLoader.active && matrixTimeline
        ? matrixTimeline.composerShell
        : null

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
        if (useMatrixRoomView
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
        visible: !useMatrixRoomView && !TimelineManager.waitingForFirstSync && (!roomPreview || !roomPreview.roomid)
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
        id: matrixRoomLoader

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: matrixHeaderPane.visible ? matrixHeaderPane.bottom : parent.top
        active: timelineView.useMatrixRoomView
        sourceComponent: matrixTimelineComponent
        visible: active
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
            timelineView.shouldEffectsRun = false;
            timelineEffects.removeParticles();
        }
    }
}

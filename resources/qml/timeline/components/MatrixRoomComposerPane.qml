// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../composer" as Composer
import QtQuick
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: root

    property var rootItem: null
    required property var uploadsController
    required property var composerRoom
    required property var composerInputController
    required property var timelineRoot

    readonly property alias composerInput: composerInput
    readonly property alias composerShell: composerContainer

    // Thread-tint expression shared by the composer container and the
    // backdrop below. Matches MatrixRoomView's timeline tint so the bars,
    // composer, and timeline read as one continuous coloured surface.
    readonly property string _threadEventId: TimelineManager.matrixTimelineThreadEventId
    readonly property bool _threadActive: _threadEventId.length > 0
    readonly property color _threadTintColor: _threadActive
        ? TimelineManager.userColor(_threadEventId, palette.base)
        : palette.buttonText
    readonly property color _threadTintedSurface: _threadActive
        ? Qt.tint(palette.window, Qt.hsla(_threadTintColor.hslHue, 0.7,
                                          _threadTintColor.hslLightness, 0.1))
        : palette.window

    implicitHeight: paneLayout.implicitHeight
    implicitWidth: paneLayout.implicitWidth

    // Backdrop behind the bars (ReplyPopup, UploadBox, transcription banner)
    // whose rounded TOP corners would otherwise cut through to palette.window.
    // In thread view we paint the same tint composerContainer uses, so the
    // corner triangles read as a continuous coloured surface instead of a
    // white notch (#185). Outside a thread we leave it transparent — the
    // existing background remains in place.
    Rectangle {
        anchors.fill: parent
        z: -1
        color: root._threadActive ? root._threadTintedSurface : "transparent"
    }

    ColumnLayout {
        id: paneLayout

        anchors.fill: parent
        spacing: 0

        TimelineCallStatusBars {}

        Composer.ReplyPopup {
        Layout.minimumHeight: 0
        Layout.preferredHeight: composerContainer.visible
            && layoutVisible
            && !composerContainer._walkMode ? implicitHeight : 0
        Layout.maximumHeight: composerContainer.visible
            && layoutVisible
            && !composerContainer._walkMode ? implicitHeight : 0
        matrixReplyEventId: TimelineManager.matrixTimelineReplyEventId
        matrixReplySenderId: TimelineManager.matrixTimelineReplySenderId
        matrixReplyDisplayName: TimelineManager.matrixTimelineReplySenderDisplayName
        matrixReplyBody: TimelineManager.matrixTimelineReplyBody
        matrixEditEventId: TimelineManager.matrixTimelineEditEventId
        matrixThreadEventId: ""
        roomModel: root.composerRoom
        roundTopCorners: true
    }

    Composer.UploadBox {
        Layout.minimumHeight: 0
        Layout.preferredHeight: composerContainer.visible
            && layoutVisible
            && !composerContainer._walkMode ? implicitHeight : 0
        Layout.maximumHeight: composerContainer.visible
            && layoutVisible
            && !composerContainer._walkMode ? implicitHeight : 0
        uploadsController: root.uploadsController
        uploadsSending: TimelineManager.matrixTimelineAttachmentSending
    }

    Composer.ComposerTranscriptionBanner {
        Layout.minimumHeight: 0
        Layout.preferredHeight: composerContainer.visible
            && layoutVisible
            && !composerContainer._walkMode ? implicitHeight : 0
        Layout.maximumHeight: composerContainer.visible
            && layoutVisible
            && !composerContainer._walkMode ? implicitHeight : 0
        inputBar: composerInput
    }

    Rectangle {
        id: composerContainer

        readonly property bool _hasRootItem: !!root.rootItem
        readonly property bool _walkMode: _hasRootItem && root.rootItem.walkModeActive
        readonly property int _baselineHeight: _hasRootItem ? root.rootItem.composerBaselineHeight : Math.max(48, Komai.navigationRowHeight)
        readonly property int contentHeight: _walkMode
            ? _baselineHeight
            : Math.max(_baselineHeight, composerInput.implicitHeight)
        Layout.fillWidth: true
        Layout.minimumHeight: visible ? implicitHeight : 0
        Layout.preferredHeight: visible ? implicitHeight : 0
        Layout.maximumHeight: visible ? implicitHeight : 0
        // Thread-tinted in thread view, so the bars, composer, and timeline
        // form one continuous coloured surface; expression shared with the
        // backdrop above.
        color: root._threadTintedSurface
        implicitHeight: inputShellSeparator.implicitHeight + contentHeight
        visible: !_hasRootItem || !root.rootItem.perfDisableComposer

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            TimelineSeparator {
                id: inputShellSeparator

                Layout.minimumHeight: implicitHeight
                Layout.preferredHeight: implicitHeight
                Layout.maximumHeight: implicitHeight
                color: composerInput.textInputActiveFocus ? palette.highlight : Komai.theme.separator
                implicitHeight: 2

                Behavior on color { ColorAnimation { duration: 150 } }
            }

            Composer.MessageInput {
                id: composerInput

                Layout.fillWidth: true
                Layout.minimumHeight: visible ? composerContainer._baselineHeight : 0
                Layout.preferredHeight: visible
                    ? Math.max(composerContainer._baselineHeight, implicitHeight)
                    : 0
                Layout.maximumHeight: visible
                    ? Math.max(composerContainer._baselineHeight, implicitHeight)
                    : 0
                room: root.composerRoom
                timelineRoot: root.timelineRoot ? root.timelineRoot : root.rootItem
                selectionModeRoot: root.rootItem
                walkModeActive: composerContainer._walkMode
                inputController: root.composerInputController
                allowStickers: false
                allowCommandCompleter: composerContainer._hasRootItem && !root.rootItem.editing
                attachmentsEnabled: composerContainer._hasRootItem && !root.rootItem.editing
                showAllButtons: true
                visible: !composerContainer._walkMode
            }

            TimelineWalkModeBar {
                Layout.fillWidth: true
                Layout.minimumHeight: visible ? composerContainer._baselineHeight : 0
                Layout.preferredHeight: visible ? composerContainer._baselineHeight : 0
                Layout.maximumHeight: visible ? composerContainer._baselineHeight : 0
                minimumHeight: composerContainer._baselineHeight
                chatRoot: root.rootItem
                visible: composerContainer._walkMode
            }
        }
    }
    }
}

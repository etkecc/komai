// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../composer" as Composer
import QtQuick
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    id: root

    required property var rootItem
    required property var uploadsController
    required property var composerRoom
    required property var composerInputController
    required property var timelineRoot

    readonly property alias composerInput: composerInput
    readonly property alias composerShell: composerContainer

    Layout.fillWidth: true
    Layout.minimumHeight: visible ? implicitHeight : 0
    Layout.preferredHeight: visible ? implicitHeight : 0
    Layout.maximumHeight: visible ? implicitHeight : 0
    spacing: 0

    Composer.UploadBox {
        Layout.minimumHeight: 0
        Layout.preferredHeight: !root.rootItem.perfDisableComposer
            && layoutVisible
            && !root.rootItem.walkModeActive ? implicitHeight : 0
        Layout.maximumHeight: !root.rootItem.perfDisableComposer
            && layoutVisible
            && !root.rootItem.walkModeActive ? implicitHeight : 0
        uploadsController: root.uploadsController
        uploadsSending: TimelineManager.matrixTimelineAttachmentSending
    }

    Composer.ReplyPopup {
        Layout.minimumHeight: 0
        Layout.preferredHeight: !root.rootItem.perfDisableComposer
            && layoutVisible
            && !root.rootItem.walkModeActive ? implicitHeight : 0
        Layout.maximumHeight: !root.rootItem.perfDisableComposer
            && layoutVisible
            && !root.rootItem.walkModeActive ? implicitHeight : 0
        matrixReplyEventId: TimelineManager.matrixTimelineReplyEventId
        matrixReplySenderId: TimelineManager.matrixTimelineReplySenderId
        matrixReplyDisplayName: TimelineManager.matrixTimelineReplySenderDisplayName
        matrixReplyBody: TimelineManager.matrixTimelineReplyBody
        matrixEditEventId: TimelineManager.matrixTimelineEditEventId
        roomModel: root.composerRoom
        roundTopCorners: true
    }

    Rectangle {
        id: composerContainer

        readonly property int contentHeight: root.rootItem.walkModeActive
            ? root.rootItem.composerBaselineHeight
            : Math.max(root.rootItem.composerBaselineHeight, composerInput.implicitHeight)
        Layout.fillWidth: true
        Layout.minimumHeight: visible ? implicitHeight : 0
        Layout.preferredHeight: visible ? implicitHeight : 0
        Layout.maximumHeight: visible ? implicitHeight : 0
        color: palette.window
        implicitHeight: inputShellSeparator.implicitHeight + contentHeight
        visible: !root.rootItem.perfDisableComposer

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
                Layout.minimumHeight: visible ? root.rootItem.composerBaselineHeight : 0
                Layout.preferredHeight: visible
                    ? Math.max(root.rootItem.composerBaselineHeight, implicitHeight)
                    : 0
                Layout.maximumHeight: visible
                    ? Math.max(root.rootItem.composerBaselineHeight, implicitHeight)
                    : 0
                room: root.composerRoom
                timelineRoot: root.timelineRoot ? root.timelineRoot : root.rootItem
                selectionModeRoot: root.rootItem
                walkModeActive: root.rootItem.walkModeActive
                inputController: root.composerInputController
                allowCalls: false
                allowStickers: false
                allowCommandCompleter: !root.rootItem.editing
                attachmentsEnabled: !root.rootItem.editing
                showAllButtons: true
                visible: !root.rootItem.walkModeActive
            }

            TimelineWalkModeBar {
                Layout.fillWidth: true
                Layout.minimumHeight: visible ? root.rootItem.composerBaselineHeight : 0
                Layout.preferredHeight: visible ? root.rootItem.composerBaselineHeight : 0
                Layout.maximumHeight: visible ? root.rootItem.composerBaselineHeight : 0
                minimumHeight: root.rootItem.composerBaselineHeight
                chatRoot: root.rootItem
                visible: root.rootItem.walkModeActive
            }
        }
    }
}

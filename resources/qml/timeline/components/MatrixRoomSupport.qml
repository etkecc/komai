// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Item {
    id: support

    required property var rootItem
    required property var roomPreview
    required property var chatRoot
    required property var timelineRoot
    required property var emojiPopup
    required property var filteredTimeline
    required property var timelineList

    width: 0
    height: 0

    readonly property var uploadsController: composerSupport.uploadsController
    readonly property var composerInputController: composerSupport.composerInputController
    readonly property var composerRoom: composerSupport.composerRoom
    readonly property var messageActionsDefaultRoomModel: messageActionsModel
    readonly property var messageContextMenu: dialogSupport.messageContextMenu
    readonly property var replyContextMenu: dialogSupport.replyContextMenu
    readonly property var messageActionsHost: dialogSupport.messageActionsHost
    readonly property var dialogRoomModel: matrixDialogRoomModel
    readonly property var forwardRoomModel: matrixForwardRoomModel
    readonly property var headerRoomModel: matrixHeaderRoomModel

    MatrixRoomComposerSupport {
        id: composerSupport

        rootItem: support.rootItem
        roomPreview: support.roomPreview
        dialogRoomModel: matrixDialogRoomModel
    }

    MatrixRoomMessageActionsModel {
        id: messageActionsModel

        rootItem: support.rootItem
        roomPreview: support.roomPreview
        dialogRoomModel: matrixDialogRoomModel
        headerRoomModel: matrixHeaderRoomModel
        openForwardDialogFn: support.openMatrixForwardDialog
    }

    MatrixRoomDialogSupport {
        id: dialogSupport

        rootItem: support.rootItem
        roomPreview: support.roomPreview
        chatRoot: support.chatRoot
        timelineRoot: support.timelineRoot
        emojiPopup: support.emojiPopup
        filteredTimeline: support.filteredTimeline
        timelineList: support.timelineList
        messageActionsDefaultRoomModel: matrixMessageActionsDefaultRoomModel
        dialogRoomModel: matrixDialogRoomModel
        forwardRoomModel: matrixForwardRoomModel
    }

    QtObject {
        id: matrixDialogRoomModel

        property string roomId: roomPreview ? roomPreview.roomid : ""

        function openUserProfile(userId) {
            const trimmedUserId = String(userId || "").trim();
            if (trimmedUserId.length === 0)
                return;

            TimelineManager.openGlobalUserProfile(trimmedUserId);
        }
    }

    QtObject {
        id: matrixForwardRoomModel

        property string roomId: roomPreview ? roomPreview.roomid : ""

        function forwardMessage(eventId, targetRoomId) {
            TimelineManager.forwardActiveMatrixTimelineEvent(String(eventId || ""),
                                                            String(targetRoomId || ""));
        }
    }

    function openRemoveMessageDialog(eventId) {
        return dialogSupport.openRemoveMessageDialog(eventId);
    }

    function destroyOnClose(dialog) {
        return dialogSupport.destroyOnClose(dialog);
    }

    function openRawMessageDialog(eventId) {
        return dialogSupport.openRawMessageDialog(eventId);
    }

    function openReadReceiptsDialog(eventId) {
        return dialogSupport.openReadReceiptsDialog(eventId);
    }

    function openMatrixForwardDialog(eventId) {
        return dialogSupport.openMatrixForwardDialog(eventId);
    }

    function openReportMessageDialog(eventId) {
        return dialogSupport.openReportMessageDialog(eventId);
    }

    function openMessageActionsDialog(eventId,
                                      threadId,
                                      eventType,
                                      isSender,
                                      isEncrypted,
                                      isEditable,
                                      link,
                                      text,
                                      messageModelOverride,
                                      roomModelOverride) {
        return dialogSupport.openMessageActionsDialog(eventId,
                                                      threadId,
                                                      eventType,
                                                      isSender,
                                                      isEncrypted,
                                                      isEditable,
                                                      link,
                                                      text,
                                                      messageModelOverride,
                                                      roomModelOverride);
    }

    MatrixRoomHeaderModel {
        id: matrixHeaderRoomModel

        rootItem: support.rootItem
        roomPreview: support.roomPreview
        dialogRoomModel: matrixDialogRoomModel
    }
}

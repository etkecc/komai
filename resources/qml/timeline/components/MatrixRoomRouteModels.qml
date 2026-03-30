// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Item {
    id: root

    required property var roomPreview

    width: 0
    height: 0

    readonly property var dialogRoomModel: matrixDialogRoomModel
    readonly property var forwardRoomModel: matrixForwardRoomModel

    QtObject {
        id: matrixDialogRoomModel

        property string roomId: roomPreview ? roomPreview.roomid : ""

        function openUserProfile(userId) {
            const trimmedUserId = String(userId || "").trim();
            if (trimmedUserId.length === 0)
                return;

            TimelineManager.openRoomUserProfile(roomId, trimmedUserId);
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
}

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import "../voip"
import cc.etke.komai 1.0

ComposerToolbarButton {
    id: root

    required property var room
    required property var timelineRoot
    required property bool showAllButtons

    // Mirrors the CallManager::sendInvite guard: legacy 1:1 calls are
    // allowed in any room flagged direct, plus any room with exactly two
    // active members.
    readonly property bool roomCallable: !!root.room
        && (root.room.isDirect === true || Number(root.room.roomMemberCount) === 2)
    readonly property bool roomBlocksCall: !roomCallable && !CallManager.isOnCall

    Layout.alignment: Qt.AlignBottom
    enabled: !roomBlocksCall
    toolTipText: roomBlocksCall
        ? qsTr("Calls are currently supported only in direct chats.")
        : (CallManager.isOnCall
            ? qsTr("Hang up")
            : (CallManager.isOnCallOnOtherDevice
                ? qsTr("Already on a call")
                : qsTr("Place a call")))
    buttonTextColor: CallManager.isOnCall ? Komai.theme.error : palette.buttonText
    image: CallManager.isOnCall ? ":/icons/icons/ui/end-call.svg" : ":/icons/icons/ui/place-call.svg"
    opacity: (roomBlocksCall || CallManager.haveCallInvite || CallManager.isOnCallOnOtherDevice) ? 0.3 : 1
    visible: CallManager.callsSupported && showAllButtons && CallManager.preMatrixRtcCallsEnabled

    onClicked: {
        if (root.room) {
            if (CallManager.haveCallInvite) {
                return;
            } else if (CallManager.isOnCall) {
                CallManager.hangUp();
            } else if (CallManager.isOnCallOnOtherDevice) {
                return;
            } else {
                var dialog = placeCallDialog.createObject(root.timelineRoot, {
                    "roomId": String(root.room.roomId || ""),
                    "roomName": String(root.room.roomName || ""),
                    "timelineRoot": root.timelineRoot
                });
                dialog.open();
                root.timelineRoot.destroyOnClose(dialog);
            }
        }
    }

    Component {
        id: placeCallDialog

        PlaceCall {
        }
    }
}

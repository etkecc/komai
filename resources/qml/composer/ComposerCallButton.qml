// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../voip"
import cc.etke.komai 1.0

// The composer call button. When both call systems are available it opens a
// small menu to choose between Element Call (MatrixRTC group calls) and a legacy
// 1:1 call; when only one is available it acts directly. While a call is up it
// becomes a hang-up button.
ComposerToolbarButton {
    id: root

    required property var room
    required property var timelineRoot
    required property bool showAllButtons

    readonly property string roomId: root.room ? String(root.room.roomId || "") : ""

    // ── Legacy (pre-MatrixRTC) 1:1 calls ────────────────────────────────────
    readonly property bool legacyShown: CallManager.callsSupported
        && CallManager.preMatrixRtcCallsEnabled
    // Mirrors the CallManager::sendInvite guard: legacy 1:1 calls are allowed in
    // any room flagged direct, plus any room with exactly two active members.
    readonly property bool roomCallable: !!root.room
        && (root.room.isDirect === true || Number(root.room.roomMemberCount) === 2)
    readonly property bool legacyUsable: legacyShown && roomCallable
    readonly property bool legacyOnCall: CallManager.isOnCall

    // ── Element Call (MatrixRTC) ────────────────────────────────────────────
    // Works in any room, 1:1 or group (subject to the user setting and a
    // homeserver with a MatrixRTC backend).
    readonly property bool elementAvailable: ElementCall.supported
        && Settings.callsElementEnabled && !!root.room
    readonly property bool elementOnCallHere: ElementCall.active
        && root.roomId.length > 0 && ElementCall.activeRoomId === root.roomId

    readonly property bool onAnyCall: legacyOnCall || elementOnCallHere
    readonly property bool anyStartable: legacyUsable || elementAvailable

    Layout.alignment: Qt.AlignBottom
    visible: showAllButtons && (legacyShown || elementAvailable)
    enabled: anyStartable || onAnyCall
    toolTipText: onAnyCall
        ? qsTr("Hang up")
        : (CallManager.isOnCallOnOtherDevice
            ? qsTr("Already on a call")
            : (anyStartable
                ? qsTr("Place a call")
                : qsTr("Calls are not available in this room.")))
    buttonTextColor: onAnyCall ? Komai.theme.error : palette.buttonText
    image: onAnyCall ? ":/icons/icons/ui/end-call.svg" : ":/icons/icons/ui/place-call.svg"
    opacity: (CallManager.haveCallInvite || CallManager.isOnCallOnOtherDevice) ? 0.3 : 1

    onClicked: {
        if (!root.room)
            return;

        // Already on a call -> hang up.
        if (root.elementOnCallHere) {
            ElementCall.hangup();
            return;
        }
        if (root.legacyOnCall) {
            CallManager.hangUp();
            return;
        }
        if (CallManager.haveCallInvite || CallManager.isOnCallOnOtherDevice)
            return;

        // Otherwise start one. Offer a choice only when both are possible.
        if (root.legacyUsable && root.elementAvailable) {
            callMenu.popup();
        } else if (root.elementAvailable) {
            ElementCall.startCall(root.roomId);
        } else if (root.legacyUsable) {
            root.placeLegacyCall();
        }
    }

    function placeLegacyCall() {
        var dialog = placeCallDialog.createObject(root.timelineRoot, {
            "roomId": root.roomId,
            "roomName": String(root.room.roomName || ""),
            "timelineRoot": root.timelineRoot
        });
        dialog.open();
        root.timelineRoot.destroyOnClose(dialog);
    }

    Menu {
        id: callMenu

        Component.onCompleted: {
            if (callMenu.popupType !== undefined)
                callMenu.popupType = 2;
        }

        MenuItem {
            text: qsTr("Element Call")
            onTriggered: ElementCall.startCall(root.roomId)
        }
        MenuItem {
            text: qsTr("Legacy call")
            onTriggered: root.placeLegacyCall()
        }
    }

    Component {
        id: placeCallDialog

        PlaceCall {
        }
    }
}

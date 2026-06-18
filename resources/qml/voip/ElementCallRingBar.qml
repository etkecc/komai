// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Global "incoming call" ring bar for Element Call, shown at the top of the
// timeline region whenever a MatrixRTC `ring` notification is ringing us (a 1:1
// call we have been invited to but have not joined or declined). Visible from
// any room, so an incoming call is reachable wherever you are: Join opens the
// call, Decline stops the ring (here and on your other devices). Warning-hued
// to read as "incoming, your attention" — distinct from the green active-call
// bar (a call you are already on) and matching the warning-hued avatar glow
// used for calls you have not joined. ElementCall is always compiled, so this
// builds even when Element Call support is off (it never becomes visible).

import "../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Rectangle {
    id: bar

    // The room-tab controller, so Join opens the call room (switches to its tab)
    // and the in-room call panel becomes visible right away.
    property var tabController: null

    readonly property string callRoomId: ElementCall.incomingRingRoomId
    readonly property string displayName: callRoomId.length > 0
        ? (Rooms.unfilteredRoomData(callRoomId, Rooms.roleId("roomName")) || callRoomId)
        : ""
    readonly property string avatarUrl: callRoomId.length > 0
        ? (Rooms.unfilteredRoomData(callRoomId, Rooms.roleId("avatarUrl")) || "")
        : ""

    // Bright, saturated warning hue (same transform the avatar call glow uses):
    // the theme's raw warning colour is tuned for text and can be a dark, muddy
    // amber on light themes, so we keep its hue but force a vivid value that
    // reads with black text across themes.
    readonly property color warningColor: {
        const w = Komai.theme.warning;
        return Qt.hsla(w.hslHue < 0 ? 0 : w.hslHue, Math.max(w.hslSaturation, 0.6), 0.55, 1.0);
    }

    color: warningColor
    implicitHeight: Komai.navigationRowHeight

    // Answer the call: open the call room AND start the Element Call surface so
    // the in-room webview panel is visible (it only shows in the call's room).
    // Capture the room id first: startCall() clears the ring, which empties
    // callRoomId (it is bound to ElementCall.incomingRingRoomId).
    function joinCall() {
        const roomId = callRoomId;
        if (roomId.length === 0)
            return;
        if (bar.tabController)
            bar.tabController.handleRoomClick(roomId, false, false);
        else
            Rooms.setCurrentRoom(roomId);
        ElementCall.startCall(roomId);
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Komai.paddingMedium
        anchors.rightMargin: Komai.paddingSmall
        spacing: Komai.paddingSmall

        Avatar {
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: Komai.iconSize
            implicitHeight: Komai.iconSize
            roomid: bar.callRoomId
            displayName: bar.displayName
            url: bar.avatarUrl.replace("mxc://", "image://MxcImage/")
            enabled: false
        }

        Label {
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            text: qsTr("Incoming call from %1").arg(bar.displayName)
            color: "#000000"
            font.pointSize: Settings.uiFontSizePt
            font.bold: true
            elide: Text.ElideRight
        }

        ElementCallBarButton {
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("Join")
            image: ":/icons/icons/ui/place-call.svg"
            accept: true
            onClicked: bar.joinCall()
        }

        ElementCallBarButton {
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("Decline")
            image: ":/icons/icons/ui/end-call.svg"
            danger: true
            onClicked: ElementCall.declineIncomingRing()
        }
    }
}

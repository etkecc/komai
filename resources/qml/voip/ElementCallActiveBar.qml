// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Thin "active call" bar for Element Call, shown at the top of the timeline
// region when a call is live in a DIFFERENT room than the one on screen (in
// the call's own room the full ElementCallPanel is shown instead). Click it to
// jump back to the call room; End call hangs up gracefully from anywhere. Sits
// in the same place as the legacy ActiveCallBar so both call stacks read
// consistently. ElementCall is always compiled, so this builds even when
// Element Call support is off (it just never becomes visible).

import "../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Rectangle {
    id: bar

    // The room-tab controller, so a click reuses the normal room-open path
    // (switches to the call room's existing tab).
    property var tabController: null

    readonly property string callRoomId: ElementCall.activeRoomId
    readonly property string displayName: callRoomId.length > 0
        ? (Rooms.unfilteredRoomData(callRoomId, Rooms.roleId("roomName")) || callRoomId)
        : ""
    readonly property string avatarUrl: callRoomId.length > 0
        ? (Rooms.unfilteredRoomData(callRoomId, Rooms.roleId("avatarUrl")) || "")
        : ""

    // True during the graceful hangup drain, so End call can't be hit twice.
    property bool leaving: false

    // Matches the legacy ActiveCallBar's call green so both bars read alike.
    color: "#2ECC71"
    implicitHeight: Komai.navigationRowHeight

    function returnToCall() {
        if (callRoomId.length === 0)
            return;
        if (bar.tabController)
            bar.tabController.handleRoomClick(callRoomId, false, false);
        else
            Rooms.setCurrentRoom(callRoomId);
    }

    // Reset the drain guard when the active call changes/clears, so a later
    // call doesn't inherit a disabled End-call button.
    Connections {
        target: ElementCall
        function onActiveChanged() {
            if (!ElementCall.active)
                bar.leaving = false;
        }
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
            text: qsTr("Element Call in %1").arg(bar.displayName)
            color: "#000000"
            font.pointSize: Settings.uiFontSizePt
            font.bold: true
            elide: Text.ElideRight
        }

        ElementCallBarButton {
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("Back to call")
            image: ":/icons/icons/ui/place-call.svg"
            style: ElementCallBarButton.Style.OnAccent
            onClicked: bar.returnToCall()
        }

        ElementCallBarButton {
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("End call")
            image: ":/icons/icons/ui/end-call.svg"
            style: ElementCallBarButton.Style.Danger
            enabled: !bar.leaving
            onClicked: {
                bar.leaving = true;
                ElementCall.hangup();
            }
        }
    }
}

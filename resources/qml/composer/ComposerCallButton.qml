// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../voip"
import cc.etke.komai 1.0

ComposerToolbarButton {
    id: root

    required property var room
    required property var timelineRoot
    required property bool showAllButtons

    Layout.alignment: Qt.AlignBottom
    ToolTip.text: CallManager.isOnCall ? qsTr("Hang up") : (CallManager.isOnCallOnOtherDevice ? qsTr("Already on a call") : qsTr("Place a call"))
    image: CallManager.isOnCall ? ":/icons/icons/ui/end-call.svg" : ":/icons/icons/ui/place-call.svg"
    opacity: (CallManager.haveCallInvite || CallManager.isOnCallOnOtherDevice) ? 0.3 : 1
    visible: CallManager.callsSupported && showAllButtons && Settings.callsLegacyEnabled

    onClicked: {
        if (root.room) {
            if (CallManager.haveCallInvite) {
                return;
            } else if (CallManager.isOnCall) {
                CallManager.hangUp();
            } else if (CallManager.isOnCallOnOtherDevice) {
                return;
            } else {
                var dialog = placeCallDialog.createObject(root.timelineRoot);
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

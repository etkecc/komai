// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai 1.0

ImageButton {
    id: indicator

    property string eventId: ""
    property int status: MtxEvent.Empty

    toolTipText: {
        switch (status) {
        case MtxEvent.Failed:
            return qsTr("Failed");
        case MtxEvent.Pending:
            return qsTr("Sending");
        case MtxEvent.Sent:
            return qsTr("Sent");
        case MtxEvent.Received:
            return qsTr("Received");
        case MtxEvent.Read:
            return qsTr("Read");
        default:
            return "";
        }
    }
    toolTipVisible: hovered && status != MtxEvent.Empty
    changeColorOnHover: (status == MtxEvent.Read)
    cursor: (status == MtxEvent.Read) ? Qt.PointingHandCursor : Qt.ArrowCursor
    height: 16
    hoverEnabled: true
    image: {
        switch (status) {
        case MtxEvent.Failed:
            return ":/icons/icons/ui/dismiss.svg";
        case MtxEvent.Pending:
            return ":/icons/icons/ui/clock.svg";
        case MtxEvent.Sent:
            return ":/icons/icons/ui/clock.svg";
        case MtxEvent.Received:
            return ":/icons/icons/ui/checkmark.svg";
        case MtxEvent.Read:
            return ":/icons/icons/ui/double-checkmark.svg";
        default:
            return "";
        }
    }
    width: 16

    signal readReceiptsRequested(string eventId)

    onClicked: {
        if (status == MtxEvent.Read)
            readReceiptsRequested(eventId);
    }
}

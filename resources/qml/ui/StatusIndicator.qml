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
    // Human-readable SDK error string from `EventSendState::SendingFailed.error`.
    // When present on a Failed row, it's appended to the tooltip so the user
    // can see *why* the send was rejected without digging through logs.
    property string sendError: ""

    toolTipText: {
        switch (status) {
        case MtxEvent.Failed:
            return indicator.sendError.length > 0
                ? qsTr("Failed: %1").arg(indicator.sendError)
                : qsTr("Failed");
        case MtxEvent.Pending:
            return qsTr("Sending");
        case MtxEvent.Sent:
            return qsTr("Sent");
        case MtxEvent.Read:
            return qsTr("Read");
        default:
            return "";
        }
    }
    toolTipVisible: hovered && status != MtxEvent.Empty
    changeColorOnHover: false
    cursor: (status == MtxEvent.Read) ? Qt.PointingHandCursor : Qt.ArrowCursor
    // Tint the Failed X with the theme's error color so it reads as a problem
    // at a glance, and the Read double-check with the brand highlight so it's
    // unmistakably distinct from the single Sent check at a glance.
    buttonTextColor: {
        if (status == MtxEvent.Failed)
            return Komai.theme.error;
        if (status == MtxEvent.Read)
            return palette.highlight;
        return palette.buttonText;
    }
    height: 16
    hoverEnabled: true
    image: {
        switch (status) {
        case MtxEvent.Failed:
            return ":/icons/icons/ui/dismiss.svg";
        case MtxEvent.Pending:
            return ":/icons/icons/ui/clock.svg";
        case MtxEvent.Sent:
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

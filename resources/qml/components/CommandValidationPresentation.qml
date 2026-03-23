// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai 1.0

QtObject {
    id: root

    property bool selectionActive: false
    property string validationMessage: ""
    property string validationState: "none"

    readonly property bool messageVisible: validationMessage.length > 0
    readonly property bool successVisible: !messageVisible && validationState === "valid"
    readonly property bool warningVisible: validationState === "incomplete"
        || validationState === "unrecognized"
    readonly property color validationColor: warningVisible ? Komai.theme.warning : Komai.theme.error
    readonly property color footerAccentColor: messageVisible ? validationColor : Komai.theme.success
    readonly property bool footerAccentVisible: messageVisible || successVisible
    readonly property string footerText: messageVisible
        ? validationMessage
        : successVisible
        ? (selectionActive
            ? qsTr("Hit Enter to insert it.")
            : qsTr("Looks good! Hit Enter to send it."))
        : qsTr("Select a command first. Enter inserts if selected; otherwise it sends.")
}

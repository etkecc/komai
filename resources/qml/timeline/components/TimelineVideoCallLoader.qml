// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Loader {
    required property var componentCatalog

    readonly property bool showCallVideo: CallManager.isOnCall
        && CallManager.callType != Voip.VOICE
        && Settings.callsLegacyEnabled

    source: showCallVideo
        ? (Qt.platform.os != "windows"
            ? componentCatalog.voipVideoCallComponent
            : componentCatalog.voipVideoCallD3D11Component)
        : ""

    onLoaded: TimelineManager.setVideoCallItem()
}

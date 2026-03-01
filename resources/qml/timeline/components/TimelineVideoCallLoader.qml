// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import im.nheko

Loader {
    required property var componentCatalog

    readonly property bool showLegacyVideoCall: CallManager.isOnCall
        && CallManager.callType != Voip.VOICE
        && Settings.callsLegacyEnabled

    source: showLegacyVideoCall
        ? (Qt.platform.os != "windows"
            ? componentCatalog.voipVideoCallComponent
            : componentCatalog.voipVideoCallD3D11Component)
        : ""

    onLoaded: TimelineManager.setVideoCallItem()
}

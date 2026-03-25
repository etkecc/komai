// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../ui/media"
import QtQuick

Item {
    id: content

    required property int duration
    required property string eventId
    required property string body
    required property string filename
    required property string filesize
    readonly property var roomContext: (typeof effectiveRoomContext !== "undefined" && effectiveRoomContext)
        ? effectiveRoomContext
        : ((typeof room !== "undefined" && room) ? room : null)

    implicitWidth: 500
    width: Math.min(parent?.width ?? implicitWidth, implicitWidth)
    height: audioPlayer.implicitHeight

    property int metadataWidth
    property bool fitsMetadata: parent != null ? ((parent.width - width) > metadataWidth + 4) : false

    InlineAudioPlayer {
        id: audioPlayer

        width: parent.width
        room: content.roomContext
        eventId: content.eventId
        duration: content.duration
        body: content.body
        filename: content.filename
        filesize: content.filesize
    }
}

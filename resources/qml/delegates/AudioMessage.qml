// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import "../ui/media"
import QtQuick
import cc.etke.komai

Item {
    id: content

    property var roomAdapter: null
    required property int duration
    required property string eventId
    required property string body
    required property string formattedBody
    required property string filename
    required property string filesize
    required property string mimetype
    property bool isVoiceMessage: false
    property var waveform: []
    readonly property var roomContext: roomAdapter
        ? roomAdapter
        : (typeof effectiveRoomContext !== "undefined" && effectiveRoomContext)
        ? effectiveRoomContext
        : ((typeof room !== "undefined" && room) ? room : null)

    // Treat the body as a real caption only when it differs from the filename
    // — voice messages and bare uploads otherwise show the auto-generated
    // file name twice (once inside the player, once as a "caption").
    readonly property bool hasCaption: body.length > 0 && body !== filename

    implicitWidth: audioPlayer.implicitWidth
    implicitHeight: contentColumn.implicitHeight
    width: Math.min(parent?.width ?? implicitWidth, implicitWidth)
    height: implicitHeight

    property int metadataWidth
    property bool fitsMetadata: parent != null ? ((parent.width - width) > metadataWidth + 4) : false

    Column {
        id: contentColumn

        width: parent.width
        spacing: content.hasCaption ? Komai.paddingSmall : 0

        InlineAudioPlayer {
            id: audioPlayer

            width: parent.width
            room: content.roomContext
            eventId: content.eventId
            duration: content.duration
            body: content.body
            filename: content.filename
            filesize: content.filesize
            mimetype: content.mimetype
            isVoiceMessage: content.isVoiceMessage
            waveform: content.waveform
        }

        MediaCaption {
            width: parent.width
            visible: content.hasCaption
            body: content.hasCaption ? content.body : ""
            formattedBody: content.hasCaption ? content.formattedBody : ""
        }
    }
}

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import QtQuick
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    id: content

    property var roomAdapter: null
    required property double proportionalHeight
    required property int originalWidth
    required property int duration
    required property string thumbnailUrl
    required property string eventId
    required property string url
    required property string body
    required property string formattedBody
    required property string filename
    required property string filesize
    required property int filesizeBytes
    required property string mimetype

    readonly property bool isGifVideo: Settings.timelineMediaAutoplayGifVideos
        && content.filesizeBytes > 0
        && content.filesizeBytes <= Komai.gifVideoMaxSizeBytes
        && (body.toLowerCase().startsWith("gif-")
            || content.duration === 0
            || (content.duration > 0 && content.duration <= Komai.gifVideoMaxDurationMs))

    // Real caption iff body differs from filename — otherwise the in-frame
    // video info overlay already shows the body as the only available label.
    readonly property bool hasCaption: body.length > 0 && body !== filename

    property int metadataWidth
    property bool fitsMetadata: parent != null ? ((parent.width - width) > metadataWidth + 4) : false

    spacing: Komai.paddingSmall

    readonly property Loader activeLoader: isGifVideo ? gifLoader : regularLoader

    Loader {
        id: regularLoader

        Layout.alignment: Qt.AlignLeft
        active: !content.isGifVideo
        visible: active
        sourceComponent: Component {
            RegularVideoMessage {
                roomAdapter: content.roomAdapter
                proportionalHeight: content.proportionalHeight
                originalWidth: content.originalWidth
                duration: content.duration
                thumbnailUrl: content.thumbnailUrl
                eventId: content.eventId
                url: content.url
                body: content.body
                filename: content.filename
                filesize: content.filesize
                mimetype: content.mimetype
            }
        }
    }

    Loader {
        id: gifLoader

        Layout.alignment: Qt.AlignLeft
        active: content.isGifVideo
        visible: active
        sourceComponent: Component {
            GifVideoMessage {
                roomAdapter: content.roomAdapter
                proportionalHeight: content.proportionalHeight
                originalWidth: content.originalWidth
                duration: content.duration
                eventId: content.eventId
                url: content.url
                thumbnailUrl: content.thumbnailUrl
                mimetype: content.mimetype
            }
        }
    }

    MediaCaption {
        Layout.fillWidth: true
        body: content.hasCaption ? content.body : ""
        formattedBody: content.hasCaption ? content.formattedBody : ""
    }
}

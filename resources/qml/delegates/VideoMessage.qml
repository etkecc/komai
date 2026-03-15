// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Item {
    id: content

    required property double proportionalHeight
    required property int originalWidth
    required property int duration
    required property string thumbnailUrl
    required property string eventId
    required property string url
    required property string body
    required property string filename
    required property string filesize
    required property int filesizeBytes

    readonly property bool isGifVideo: Settings.timelineMediaAutoplayGifVideos
        && content.filesizeBytes > 0
        && content.filesizeBytes <= Komai.gifVideoMaxSizeBytes
        && (body.toLowerCase().startsWith("gif-")
            || content.duration === 0
            || (content.duration > 0 && content.duration <= Komai.gifVideoMaxDurationMs))

    property int metadataWidth
    property bool fitsMetadata: parent != null ? ((parent.width - width) > metadataWidth + 4) : false

    implicitWidth: activeLoader.item ? activeLoader.item.implicitWidth : 0
    width: Math.min(parent?.width ?? implicitWidth, implicitWidth)
    height: activeLoader.item ? activeLoader.item.height : 0

    readonly property Loader activeLoader: isGifVideo ? gifLoader : regularLoader

    Loader {
        id: regularLoader

        active: !content.isGifVideo
        width: content.width
        sourceComponent: Component {
            RegularVideoMessage {
                proportionalHeight: content.proportionalHeight
                originalWidth: content.originalWidth
                duration: content.duration
                thumbnailUrl: content.thumbnailUrl
                eventId: content.eventId
                url: content.url
                body: content.body
                filename: content.filename
                filesize: content.filesize
            }
        }
    }

    Loader {
        id: gifLoader

        active: content.isGifVideo
        width: content.width
        sourceComponent: Component {
            GifVideoMessage {
                proportionalHeight: content.proportionalHeight
                originalWidth: content.originalWidth
                duration: content.duration
                eventId: content.eventId
                url: content.url
                thumbnailUrl: content.thumbnailUrl
            }
        }
    }
}

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import QtQuick
import cc.etke.komai

Item {
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

    readonly property Loader activeLoader: isGifVideo ? gifLoader : regularLoader
    readonly property int captionSpacing: hasCaption ? Komai.paddingSmall : 0

    implicitWidth: activeLoader.item ? activeLoader.item.implicitWidth : 0
    width: Math.min(parent?.width ?? implicitWidth, implicitWidth)
    implicitHeight: (activeLoader.item ? activeLoader.item.height : 0)
        + (mediaCaption.visible ? captionSpacing + mediaCaption.implicitHeight : 0)
    height: implicitHeight

    Loader {
        id: regularLoader

        active: !content.isGifVideo
        visible: active
        width: content.width
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

        active: content.isGifVideo
        visible: active
        width: content.width
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
        id: mediaCaption

        anchors.top: parent.top
        anchors.topMargin: (content.activeLoader.item ? content.activeLoader.item.height : 0)
            + content.captionSpacing
        width: parent.width
        visible: content.hasCaption
        body: content.hasCaption ? content.body : ""
        formattedBody: content.hasCaption ? content.formattedBody : ""
    }
}

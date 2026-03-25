// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtMultimedia
import QtQuick
import cc.etke.komai
import Qt5Compat.GraphicalEffects

Item {
    id: content

    required property double proportionalHeight
    required property int originalWidth
    required property int duration
    required property string eventId
    required property string url
    required property string thumbnailUrl
    required property string mimetype

    property double divisor: EventDelegateChooser.isReply ? 10 : 4
    property int tempWidth: originalWidth < 1 ? 400 : originalWidth
    readonly property var roomContext: (typeof effectiveRoomContext !== "undefined" && effectiveRoomContext)
        ? effectiveRoomContext
        : ((typeof room !== "undefined" && room) ? room : null)

    implicitWidth: Math.round(tempWidth * Math.min((timelineView.height / divisor) / (tempWidth * proportionalHeight), 1))
    width: Math.min(parent?.width ?? implicitWidth, implicitWidth)
    height: width * proportionalHeight

    MxcMedia {
        id: gifMedia

        roomm: roomContext
        eventId: content.eventId
        mimeTypeHint: content.mimetype
        videoOutput: gifVideoOutput
        skipAudioOutput: true
        muted: true
        volume: 0.0

        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia
                || mediaStatus === MediaPlayer.BufferedMedia) {
                gifMedia.loops = MediaPlayer.Infinite;
                gifMedia.play();
            }
        }
    }

    Rectangle {
        id: videoContainer

        color: palette.window
        width: parent.width
        height: parent.height
        radius: 8
        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: Rectangle {
                width: videoContainer.width
                height: videoContainer.height
                radius: 8
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (!roomContext)
                    return;

                if (Settings.timelineMediaOpenVideosExternal
                        || roomContext.isActiveMatrixTimelineRoom === true) {
                    roomContext.openMedia(content.eventId);
                } else {
                    TimelineManager.openMediaOverlayWithContext(
                        roomContext, content.url, content.eventId,
                        content.originalWidth, content.proportionalHeight,
                        MtxEvent.VideoMessage, content.duration, content.thumbnailUrl,
                        timeline, timelineView);
                }
            }
        }

        Image {
            id: gifThumb
            anchors.fill: parent
            source: content.thumbnailUrl
                ? ((roomContext && roomContext.isActiveMatrixTimelineRoom === true)
                    ? ("image://MxcImage/matrix-timeline:" + content.eventId + "?scale")
                    : (thumbnailUrl.replace("mxc://", "image://MxcImage/") + "?scale" + (roomContext ? "&room=" + roomContext.roomId : "")))
                : "image://colorimage/:/icons/icons/ui/video-file.svg?" + palette.windowText
            asynchronous: true
            fillMode: Image.PreserveAspectFit

            VideoOutput {
                id: gifVideoOutput

                visible: true
                clip: true
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectFit
                orientation: gifMedia.orientation
            }
        }
    }

    Component.onCompleted: gifMedia.startDownload()
    Component.onDestruction: gifMedia.killPlayback()
}

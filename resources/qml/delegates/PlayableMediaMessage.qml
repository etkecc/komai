// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../ui/media"
import QtMultimedia
import QtQuick
import QtQuick.Controls
import cc.etke.komai
import Qt5Compat.GraphicalEffects

Item {
    id: content

    required property double proportionalHeight
    required property int type
    required property int originalWidth
    required property int duration
    required property string thumbnailUrl
    required property string eventId
    required property string url
    required property string body
    required property string filename
    required property string filesize
    property double divisor: EventDelegateChooser.isReply ? 10 : 4
    property int tempWidth: originalWidth < 1? 400: originalWidth
    readonly property string mediaLabel: body.length > 0 && filename.length > 0 && body !== filename
        ? body + " (" + filename + ")"
        : (filename.length > 0 ? filename : body)
    implicitWidth: type == MtxEvent.VideoMessage ? Math.round(tempWidth*Math.min((timelineView.height/divisor)/(tempWidth*proportionalHeight), 1)) : 500
    width: Math.min(parent?.width ?? implicitWidth, implicitWidth)
    height: type == MtxEvent.VideoMessage ? width*proportionalHeight : (mediaControls.height + audioInfoLabel.height)
    //implicitHeight: height

    property int metadataWidth
    property bool fitsMetadata: parent != null ? ((parent.width - (type == MtxEvent.VideoMessage ? width : audioInfoLabel.width)) > metadataWidth+4) : false

    // Only preload cached audio — videos play in the media overlay instead.
    Component.onCompleted: {
        if (content.type !== MtxEvent.VideoMessage)
            mxcmedia.startDownload(true);
    }

    MxcMedia {
        id: mxcmedia

        // TODO: Show error in overlay or so?
        roomm: room
        eventId: content.eventId
        videoOutput: videoOutput

        muted: mediaControls.muted
        volume: mediaControls.desiredVolume
    }

    Rectangle {
        id: videoContainer

        color: content.type == MtxEvent.VideoMessage ? palette.window : "transparent"
        width: parent.width
        height: content.type == MtxEvent.VideoMessage ? parent.height : (parent.height - audioInfoLabel.height)
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
            id: videoMouseArea
            anchors.fill: parent
            visible: content.type == MtxEvent.VideoMessage
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (Settings.timelineMediaOpenVideosExternal) {
                    room.openMedia(content.eventId);
                } else {
                    TimelineManager.openMediaOverlayWithContext(
                        room, content.url, content.eventId,
                        content.originalWidth, content.proportionalHeight,
                        content.type, content.duration, content.thumbnailUrl,
                        timeline, timelineView);
                }
            }
        }

        TapHandler {
            enabled: content.type != MtxEvent.VideoMessage
            onTapped: {
                // Audio: keep existing behavior
                Settings.timelineMediaOpenVideosExternal ? room.openMedia(eventId) : mediaControls.showControls();
            }
        }

        Image {
            id: videoThumb
            anchors.fill: parent
            visible: content.type == MtxEvent.VideoMessage
            source: content.thumbnailUrl ? thumbnailUrl.replace("mxc://", "image://MxcImage/") + "?scale" : "image://colorimage/:/icons/icons/ui/video-file.svg?" + palette.windowText
            asynchronous: true
            fillMode: Image.PreserveAspectFit

            VideoOutput {
                id: videoOutput

                visible: content.type == MtxEvent.VideoMessage
                clip: true
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectFit
                orientation: mxcmedia.orientation
            }
        }

        // Play button overlay for video — always visible since tap opens the overlay
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.3)
            visible: content.type == MtxEvent.VideoMessage

            Rectangle {
                id: playButton
                anchors.centerIn: parent
                width: Math.max(56, Math.min(parent.width, parent.height) * 0.3)
                height: width
                radius: width / 2
                color: videoMouseArea.containsMouse ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(0, 0, 0, 0.6)
                scale: videoMouseArea.containsMouse ? 1.08 : 1.0

                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on scale { NumberAnimation { duration: 150 } }

                Image {
                    anchors.centerIn: parent
                    width: parent.width * 0.45
                    height: width
                    source: "image://colorimage/:/icons/icons/ui/play-sign.svg?white"
                    sourceSize.width: width * Screen.devicePixelRatio
                    sourceSize.height: height * Screen.devicePixelRatio
                }
            }
        }

        // Caption/filename overlay — matches image style (bottom overlay inside the media)
        Item {
            id: videoInfoOverlay
            anchors.fill: parent
            visible: content.type == MtxEvent.VideoMessage

            Rectangle {
                width: parent.width
                height: videoInfoColumn.height
                anchors.bottom: parent.bottom
                color: palette.window
                opacity: 0.75
            }

            Column {
                id: videoInfoColumn
                width: parent.width
                anchors.bottom: parent.bottom

                Text {
                    width: parent.width
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignHCenter
                    text: content.mediaLabel
                    color: palette.text
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: content.filesize
                    color: palette.text
                    opacity: 0.7
                    font.pointSize: Qt.application.font.pointSize * 0.9
                }
            }
        }

        MediaControls {
            id: mediaControls

            // Hide for video — videos open in the media overlay instead
            visible: content.type !== MtxEvent.VideoMessage
            anchors.left: videoContainer.left
            anchors.right: videoContainer.right
            anchors.bottom: videoContainer.bottom
            playingVideo: content.type == MtxEvent.VideoMessage
            positionValue: mxcmedia.position
            duration: mediaLoaded ? mxcmedia.duration : content.duration
            mediaLoaded: mxcmedia.loaded
            mediaState: mxcmedia.playbackState
            onPositionChanged: mxcmedia.position = position
            onPlayPauseActivated: mxcmedia.playbackState == MediaPlayer.PlayingState ? mxcmedia.pause() : mxcmedia.play()
            onLoadActivated: mxcmedia.startDownload()
        }
    }

    // File info label for audio messages (below the controls)
    Label {
        id: audioInfoLabel

        visible: content.type !== MtxEvent.VideoMessage
        anchors.top: videoContainer.bottom
        text: content.mediaLabel + " [" + content.filesize + "]"
        textFormat: Text.RichText
        elide: Text.ElideRight
        color: palette.text

        background: Rectangle {
            color: palette.base
        }
    }

}

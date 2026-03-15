// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtMultimedia
import Qt5Compat.GraphicalEffects

import "../../../ui"
import "../../../ui/media"

import cc.etke.komai 1.0

// Video display content for the media overlay.
// Owns its own input: click on video toggles playback,
// hover shows play button effect, MediaControls at the bottom.
Item {
    id: videoContent

    required property Room room
    required property string eventId
    required property string thumbnailUrl
    required property int mediaDuration
    required property int cornerRadius

    // Expose player state for the shell.
    readonly property bool loaded: mediaPlayer.loaded
    readonly property bool encrypted: mediaPlayer.encrypted
    readonly property int playbackState: mediaPlayer.playbackState

    function togglePlayback() {
        if (!mediaPlayer.loaded)
            mediaPlayer.startDownload();
        else if (mediaPlayer.playbackState === MediaPlayer.PlayingState)
            mediaPlayer.pause();
        else {
            mediaPlayer.ensureAudioReady();
            mediaPlayer.play();
        }
    }

    function stopPlayback() {
        if (mediaPlayer.loaded)
            mediaPlayer.killPlayback();
    }

    function startDownload() {
        mediaPlayer.ensureAudioReady();
        // Restore muted state from controls — killPlayback() force-mutes
        // the audio output, so we need to sync it back.
        mediaPlayer.muted = controls.muted;
        mediaPlayer.startDownload();
    }

    // Full-area click handler: click anywhere on the video toggles playback.
    // Declared first so play button and MediaControls (declared later) sit on top.
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: videoContent.togglePlayback()
    }

    // Video display with rounded corners via OpacityMask.
    Item {
        id: videoClipper

        anchors.fill: parent
        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: Rectangle {
                width: videoClipper.width
                height: videoClipper.height
                radius: videoContent.cornerRadius
            }
        }

        Image {
            id: videoThumbnail

            anchors.fill: parent
            source: videoContent.thumbnailUrl
                ? videoContent.thumbnailUrl.replace("mxc://", "image://MxcImage/") + "?scale&room=" + videoContent.room.roomId
                : ""
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            visible: !videoOutput.visible
        }

        VideoOutput {
            id: videoOutput

            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectFit
            // Qt6 FFmpeg backend auto-applies rotation from video metadata,
            // so we do NOT set orientation here to avoid double-rotation.
            visible: mediaPlayer.loaded
                     && mediaPlayer.playbackState !== MediaPlayer.StoppedState
        }

        // Dark scrim over thumbnail for play button contrast
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.3)
            visible: videoThumbnail.visible && !videoOutput.visible
        }
    }

    // Large centered play/pause button.
    // Reacts to hover anywhere on the video area.
    Rectangle {
        id: playButton

        readonly property bool hovered: videoHover.hovered

        visible: !videoOutput.visible
                 || mediaPlayer.playbackState !== MediaPlayer.PlayingState
        anchors.centerIn: parent
        width: Math.max(56, Math.min(parent.width, parent.height) * 0.15)
        height: width
        radius: width / 2
        color: hovered ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(0, 0, 0, 0.6)
        scale: hovered ? 1.08 : 1.0

        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on scale { NumberAnimation { duration: 150 } }

        Image {
            anchors.centerIn: parent
            width: parent.width * 0.4
            height: width
            source: "image://colorimage/:/icons/icons/ui/play-sign.svg?white"
            sourceSize.width: width * Screen.devicePixelRatio
            sourceSize.height: height * Screen.devicePixelRatio
        }
    }

    MxcMedia {
        id: mediaPlayer

        roomm: videoContent.room
        eventId: videoContent.eventId
        videoOutput: videoOutput
        loops: MediaPlayer.Infinite
        muted: controls.muted
        volume: controls.desiredVolume

        // Wait for the player to finish parsing the media before auto-playing.
        // Playing immediately on loadedChanged is too early — the FFmpeg backend
        // hasn't found the first keyframe yet, so the internal clock advances
        // while no frames are decoded, causing the video to start late.
        onMediaStatusChanged: {
            if ((mediaStatus === MediaPlayer.LoadedMedia
                 || mediaStatus === MediaPlayer.BufferedMedia)
                && playbackState === MediaPlayer.StoppedState
                && videoContent.visible) {
                mediaPlayer.position = 0;
                mediaPlayer.ensureAudioReady();
                mediaPlayer.play();
            }
        }
    }

    MediaControls {
        id: controls

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        playingVideo: true
        positionValue: mediaPlayer.position
        duration: controls.mediaLoaded ? mediaPlayer.duration : videoContent.mediaDuration
        mediaLoaded: mediaPlayer.loaded
        mediaState: mediaPlayer.playbackState
        onPositionChanged: mediaPlayer.position = position
        onPlayPauseActivated: {
            if (mediaPlayer.playbackState == MediaPlayer.PlayingState) {
                mediaPlayer.pause();
            } else {
                mediaPlayer.ensureAudioReady();
                mediaPlayer.play();
            }
        }
        onLoadActivated: mediaPlayer.startDownload()
    }

    // HoverHandler for play button hover effect. Pointer handlers are not
    // blocked by sibling Items or MouseAreas, so this works reliably.
    HoverHandler {
        id: videoHover
    }
}

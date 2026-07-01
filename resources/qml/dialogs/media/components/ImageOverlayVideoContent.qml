// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtMultimedia
import QtQuick.Effects

import "../../../ui"
import "../../../ui/media"

import cc.etke.komai 1.0

// Video display content for the media overlay.
// Owns its own input: click on video toggles playback,
// hover shows play button effect, MediaControls at the bottom.
Item {
    id: videoContent

    property var room
    required property string eventId
    required property string thumbnailUrl
    required property int mediaDuration
    required property int cornerRadius
    readonly property bool useActiveMatrixTimelineSource: !!room
        && room.isActiveMatrixTimelineRoom === true

    // Expose player state for the shell.
    readonly property bool loaded: mediaPlayer.loaded
    readonly property bool encrypted: mediaPlayer.encrypted
    readonly property int playbackState: mediaPlayer.playbackState

    function togglePlayback() {
        // A load/download is already in flight — ignore clicks so we don't kick
        // off duplicate downloads while the spinner is showing.
        if (mediaPlayer.buffering)
            return;
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

    // Video display with rounded corners via MultiEffect mask.
    Rectangle {
        id: videoMask

        anchors.fill: videoClipper
        radius: videoContent.cornerRadius
        layer.enabled: true
        visible: false
    }

    Item {
        id: videoClipper

        anchors.fill: parent
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: videoMask
        }

        Image {
            id: videoThumbnail

            anchors.fill: parent
            source: (videoContent.thumbnailUrl && videoContent.eventId !== "")
                ? (videoContent.useActiveMatrixTimelineSource
                    ? ("image://MxcImage/matrix-timeline:" + videoContent.eventId + "?scale")
                    : (videoContent.thumbnailUrl.replace("mxc://", "image://MxcImage/") + "?scale" + (videoContent.room ? "&room=" + videoContent.room.roomId : "")))
                : ""
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            // Provide sourceSize so the matrix-timeline: path knows a
            // thumbnail (not the full video) is wanted.
            sourceSize.width: videoContent.useActiveMatrixTimelineSource
                ? Screen.desktopAvailableWidth * Screen.devicePixelRatio : 0
            sourceSize.height: videoContent.useActiveMatrixTimelineSource
                ? Screen.desktopAvailableHeight * Screen.devicePixelRatio : 0
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

        visible: !mediaPlayer.buffering
                 && (!videoOutput.visible
                     || mediaPlayer.playbackState !== MediaPlayer.PlayingState)
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

    // Loading indicator shown while the video is being fetched/buffered, so a
    // click that triggers a download doesn't look like nothing happened.
    Spinner {
        anchors.centerIn: parent
        height: Math.max(48, Math.min(parent.width, parent.height) * 0.12)
        visible: mediaPlayer.buffering
        running: visible
        z: 10
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
            if (mediaPlayer.buffering)
                return;
            if (mediaPlayer.playbackState == MediaPlayer.PlayingState) {
                mediaPlayer.pause();
            } else {
                mediaPlayer.ensureAudioReady();
                mediaPlayer.play();
            }
        }
        onLoadActivated: {
            if (!mediaPlayer.buffering)
                mediaPlayer.startDownload();
        }
    }

    // HoverHandler for play button hover effect. Pointer handlers are not
    // blocked by sibling Items or MouseAreas, so this works reliably.
    HoverHandler {
        id: videoHover
    }
}

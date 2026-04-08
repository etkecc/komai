// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtMultimedia
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../ui"
import "../ui/media"
import cc.etke.komai

Item {
    id: root

    readonly property bool active: VoiceRecorder.recording || VoiceRecorder.paused || VoiceRecorder.hasRecording
    readonly property var focusableButton: VoiceRecorder.hasRecording ? playButton : (VoiceRecorder.paused ? finalizeButton : null)
    readonly property alias deleteButton: trashButton

    implicitHeight: contentRow.implicitHeight

    function formatDuration(durationMs) {
        const safeDuration = Math.max(0, Math.floor(durationMs / 1000));
        const seconds = safeDuration % 60;
        const minutes = Math.floor(safeDuration / 60) % 60;

        function withLeadingZero(value) {
            return value < 10 ? "0" + value : value.toString();
        }

        return minutes.toString() + ":" + withLeadingZero(seconds);
    }

    MediaPlayer {
        id: previewPlayer

        property bool autoPlayPending: false

        source: VoiceRecorder.hasRecording ? Qt.url("file://" + VoiceRecorder.filePath) : ""
        audioOutput: AudioOutput {}
        onPlaybackStateChanged: {
            if (playbackState === MediaPlayer.StoppedState && previewPlayer.position > 0)
                previewPlayer.position = 0;
        }
        onMediaStatusChanged: {
            if (autoPlayPending
                && (mediaStatus === MediaPlayer.LoadedMedia || mediaStatus === MediaPlayer.BufferedMedia)) {
                autoPlayPending = false;
                previewPlayer.play();
            }
        }
        onSourceChanged: {
            if (source.toString() !== "")
                autoPlayPending = true;
        }
    }

    RowLayout {
        id: contentRow

        anchors.fill: parent
        spacing: Komai.paddingSmall

        // Recording indicator (visible while actively recording)
        RowLayout {
            visible: VoiceRecorder.recording
            Layout.fillWidth: true
            spacing: Komai.paddingSmall

            Rectangle {
                id: recordingDot

                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 8
                Layout.preferredHeight: 8
                radius: 4
                color: Komai.theme.error

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    running: VoiceRecorder.recording
                    NumberAnimation { to: 0.3; duration: 600 }
                    NumberAnimation { to: 1.0; duration: 600 }
                }
            }

            LiveWaveformBars {
                Layout.fillWidth: true
                Layout.preferredHeight: 24
                Layout.alignment: Qt.AlignVCenter
                samples: VoiceRecorder.waveformSamples
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                text: root.formatDuration(VoiceRecorder.durationMs)
                color: palette.text
            }
        }

        // Paused indicator (visible when recording is paused but not stopped)
        RowLayout {
            visible: VoiceRecorder.paused
            spacing: Komai.paddingSmall

            ComposerToolbarButton {
                id: finalizeButton

                Layout.alignment: Qt.AlignVCenter
                image: ":/icons/icons/ui/play-sign.svg"
                toolTipText: qsTr("Finalize recording for preview")

                onClicked: VoiceRecorder.stopRecording()
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("Paused")
                color: palette.buttonText
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
                text: root.formatDuration(VoiceRecorder.durationMs)
                color: palette.text
            }
        }

        // Playback preview (visible only when recording is stopped / file is finalized)
        RowLayout {
            visible: VoiceRecorder.hasRecording
            Layout.fillWidth: true
            spacing: Komai.paddingSmall

            ComposerToolbarButton {
                id: playButton

                Layout.alignment: Qt.AlignVCenter
                image: previewPlayer.playbackState === MediaPlayer.PlayingState
                    ? ":/icons/icons/ui/pause-symbol.svg"
                    : ":/icons/icons/ui/play-sign.svg"

                onClicked: {
                    if (previewPlayer.playbackState === MediaPlayer.PlayingState) {
                        previewPlayer.pause();
                    } else {
                        if (previewPlayer.duration > 0
                            && previewPlayer.position >= Math.max(0, previewPlayer.duration - 250)) {
                            previewPlayer.position = 0;
                        }
                        previewPlayer.play();
                    }
                }
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                text: root.formatDuration(previewPlayer.position)
                color: palette.buttonText
            }

            WaveformBars {
                Layout.fillWidth: true
                Layout.minimumWidth: 80
                Layout.preferredHeight: 28
                Layout.alignment: Qt.AlignVCenter
                samples: VoiceRecorder.waveformSamples
                progress: previewPlayer.duration > 0
                    ? previewPlayer.position / previewPlayer.duration
                    : 0
                interactive: previewPlayer.duration > 0
                onSeekRequested: position => {
                    const maxDuration = Math.max(VoiceRecorder.durationMs, previewPlayer.duration, 1);
                    previewPlayer.position = position * maxDuration;
                }
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                text: root.formatDuration(Math.max(VoiceRecorder.durationMs, previewPlayer.duration))
                color: palette.buttonText
            }
        }

        // Trash button
        ComposerToolbarButton {
            id: trashButton

            Layout.alignment: Qt.AlignVCenter
            image: ":/icons/icons/ui/delete.svg"
            toolTipText: qsTr("Discard recording")

            onClicked: {
                if (previewPlayer.playbackState !== MediaPlayer.StoppedState)
                    previewPlayer.stop();
                VoiceRecorder.discardRecording();
                TimelineManager.clearActiveMatrixAttachments();
            }
        }
    }
}

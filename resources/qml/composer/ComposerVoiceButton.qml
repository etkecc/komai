// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai 1.0

ComposerToolbarButton {
    id: root

    required property bool showAllButtons
    required property bool composerHasText

    Layout.alignment: Qt.AlignBottom
    toolTipText: VoiceRecorder.recording ? qsTr("Pause recording [Ctrl+R]")
        : VoiceRecorder.paused ? qsTr("Resume recording [Ctrl+R]")
        : qsTr("Record a voice message [Ctrl+R]")
    image: VoiceRecorder.recording ? ":/icons/icons/ui/pause-symbol.svg"
        : ":/icons/icons/ui/mic-record.svg"
    visible: showAllButtons
    enabled: !composerHasText || VoiceRecorder.recording || VoiceRecorder.paused || VoiceRecorder.hasRecording
    opacity: enabled ? 1.0 : 0.3

    onClicked: {
        if (VoiceRecorder.recording) {
            VoiceRecorder.pauseRecording();
        } else if (VoiceRecorder.paused) {
            VoiceRecorder.resumeRecording();
        } else if (!VoiceRecorder.hasRecording) {
            VoiceRecorder.startRecording();
            TimelineManager.stageVoiceRecording(VoiceRecorder.filePath);
        }
    }
}

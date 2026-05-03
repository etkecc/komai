// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai 1.0

ComposerToolbarButton {
    id: root

    required property bool showAllButtons
    required property var inputBar

    readonly property bool _recordingViaButton:
        inputBar
        && inputBar.transcriptionState === "recording"
        && (inputBar.transcriptionTriggerKind === "button-hold"
            || inputBar.transcriptionTriggerKind === "button-toggle")
    readonly property bool _eligible:
        inputBar
        && inputBar.transcriptionGestureEligible
        && (inputBar.transcriptionState === "idle"
            || inputBar.transcriptionState === "error"
            || inputBar.transcriptionState === "armed"
            || _recordingViaButton)

    Layout.alignment: Qt.AlignBottom
    visible: showAllButtons && Settings.composerInputTranscriptionEnabled === true
    enabled: _eligible
    opacity: enabled ? 1.0 : 0.3
    image: ":/icons/icons/ui/transcription.svg"
    mirrorImage: LayoutMirroring.enabled || Qt.application.layoutDirection === Qt.RightToLeft
    buttonTextColor: _recordingViaButton ? Komai.theme.error : palette.buttonText
    toolTipText: _recordingViaButton
        ? qsTr("Click to transcribe speech to text (or release if held)")
        : qsTr("Hold or click to transcribe speech to text [long-press Space]")

    // Track press state ourselves: AbstractButton's `clicked` would fire
    // after every release without distinguishing hold from click. The
    // long-press timer fires while still pressed → enters hold mode and
    // starts recording immediately. Releasing before the timer fires =
    // click → toggle mode (recording continues until next click).
    property bool _pressing: false

    Timer {
        id: holdThresholdTimer
        interval: 350
        repeat: false
        onTriggered: {
            if (!root._pressing || !root.inputBar)
                return;
            // Promote the armed gesture to recording in hold mode.
            if (root.inputBar.transcriptionState === "armed"
                && root.inputBar.transcriptionTriggerKind === "button-hold") {
                root.inputBar._beginTranscriptionRecording();
            }
        }
    }

    onPressed: {
        if (!inputBar)
            return;

        // If we're already mid-toggle-recording, the press is the "stop"
        // click. Treat it on `released` so `clicked` semantics still work
        // for keyboard activation.
        if (inputBar.transcriptionState === "recording"
            && inputBar.transcriptionTriggerKind === "button-toggle") {
            return;
        }

        if (!inputBar.transcriptionGestureEligible)
            return;
        if (inputBar.transcriptionState !== "idle"
            && inputBar.transcriptionState !== "error") {
            return;
        }

        _pressing = true;
        inputBar._armTranscriptionGesture("button-hold");
        holdThresholdTimer.restart();
    }

    onReleased: {
        if (!inputBar)
            return;

        const wasPressing = _pressing;
        _pressing = false;
        holdThresholdTimer.stop();

        // Toggle-mode click-again: stop recording and dispatch.
        if (inputBar.transcriptionState === "recording"
            && inputBar.transcriptionTriggerKind === "button-toggle") {
            inputBar._commitTranscriptionGesture();
            return;
        }

        if (!wasPressing)
            return;

        if (inputBar.transcriptionTriggerKind !== "button-hold")
            return;

        if (inputBar.transcriptionState === "armed") {
            // Released before the threshold = click. Enter toggle mode and
            // start recording right now; user clicks again to stop.
            inputBar._beginTranscriptionInToggleMode();
        } else if (inputBar.transcriptionState === "recording") {
            // Held past threshold and now released = commit.
            inputBar._commitTranscriptionGesture();
        }
    }

    onCanceled: {
        // Pointer left the button bounds before release. Treat as cancel
        // so we don't leave the gesture armed.
        if (!inputBar)
            return;
        const wasPressing = _pressing;
        _pressing = false;
        holdThresholdTimer.stop();
        if (wasPressing
            && inputBar.transcriptionTriggerKind === "button-hold"
            && inputBar.transcriptionState === "armed") {
            inputBar._cancelTranscriptionGesture();
        }
    }
}

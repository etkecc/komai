// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai 1.0

ComposerToolbarButton {
    id: root

    required property bool showAllButtons
    required property var composerInput

    readonly property bool _recordingViaButton:
        composerInput
        && composerInput.transcriptionState === "recording"
        && (composerInput.transcriptionTriggerKind === "button-hold"
            || composerInput.transcriptionTriggerKind === "button-toggle")
    readonly property bool _eligible:
        composerInput
        && composerInput.transcriptionGestureEligible
        && (composerInput.transcriptionState === "idle"
            || composerInput.transcriptionState === "error"
            || composerInput.transcriptionState === "armed"
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
            if (!root._pressing || !root.composerInput)
                return;
            // Promote the armed gesture to recording in hold mode.
            if (root.composerInput.transcriptionState === "armed"
                && root.composerInput.transcriptionTriggerKind === "button-hold") {
                root.composerInput._beginTranscriptionRecording();
            }
        }
    }

    onPressed: {
        if (!composerInput)
            return;

        // If we're already mid-toggle-recording, the press is the "stop"
        // click. Treat it on `released` so `clicked` semantics still work
        // for keyboard activation.
        if (composerInput.transcriptionState === "recording"
            && composerInput.transcriptionTriggerKind === "button-toggle") {
            return;
        }

        if (!composerInput.transcriptionGestureEligible)
            return;
        if (composerInput.transcriptionState !== "idle"
            && composerInput.transcriptionState !== "error") {
            return;
        }

        _pressing = true;
        composerInput._armTranscriptionGesture("button-hold");
        holdThresholdTimer.restart();
    }

    onReleased: {
        if (!composerInput)
            return;

        const wasPressing = _pressing;
        _pressing = false;
        holdThresholdTimer.stop();

        // Toggle-mode click-again: stop recording and dispatch.
        if (composerInput.transcriptionState === "recording"
            && composerInput.transcriptionTriggerKind === "button-toggle") {
            composerInput._commitTranscriptionGesture();
            return;
        }

        if (!wasPressing)
            return;

        if (composerInput.transcriptionTriggerKind !== "button-hold")
            return;

        if (composerInput.transcriptionState === "armed") {
            // Released before the threshold = click. Enter toggle mode and
            // start recording right now; user clicks again to stop.
            composerInput._beginTranscriptionInToggleMode();
        } else if (composerInput.transcriptionState === "recording") {
            // Held past threshold and now released = commit.
            composerInput._commitTranscriptionGesture();
        }
    }

    onCanceled: {
        // Pointer left the button bounds before release. Treat as cancel
        // so we don't leave the gesture armed.
        if (!composerInput)
            return;
        const wasPressing = _pressing;
        _pressing = false;
        holdThresholdTimer.stop();
        if (wasPressing
            && composerInput.transcriptionTriggerKind === "button-hold"
            && composerInput.transcriptionState === "armed") {
            composerInput._cancelTranscriptionGesture();
        }
    }
}

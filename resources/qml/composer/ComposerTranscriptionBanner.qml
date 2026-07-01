// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import "../shell/components" as ShellComponents
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

// Status banner that sits ABOVE the composer toolbar, in the same visual
// idiom as ReplyPopup ("Replying to …" / "Editing a message"): rounded top
// corners, palette.alternateBase background, a small icon + bold label, a
// dismiss button on the right. The composer textarea remains visible
// underneath so the user keeps their existing draft while transcribing.
Rectangle {
    id: root

    // The MessageInput inputBar — drives state, button actions, and copy.
    required property var inputBar

    readonly property string transcriptionState: inputBar ? inputBar.transcriptionState : "idle"
    readonly property string triggerKind: inputBar ? inputBar.transcriptionTriggerKind : ""
    readonly property string effectiveProvider: inputBar ? inputBar.transcriptionEffectiveProvider : ""
    readonly property string lastError: inputBar ? inputBar.transcriptionLastError : ""
    readonly property bool isRealtimeProvider: effectiveProvider === "openai_realtime"
    // Banner stays hidden during the brief "armed" confirmation window
    // (350ms long-press wait) so a normal short Space tap never flashes it.
    readonly property bool layoutVisible: transcriptionState !== "idle"
        && transcriptionState !== "armed"

    property int headerTextHeight: Math.round(Komai.fontPixelSize * 2.4)
    property int headerIconSize: Math.ceil(headerTextHeight * 0.5)
    property int headerFontSize: Math.ceil(headerTextHeight * 0.45)

    Layout.fillWidth: true
    Layout.minimumHeight: 0
    Layout.preferredHeight: layoutVisible ? implicitHeight : 0
    Layout.maximumHeight: layoutVisible ? implicitHeight : 0
    color: palette.alternateBase
    radius: 8
    implicitHeight: layoutVisible ? row.implicitHeight + Komai.paddingMedium * 2 : 0
    visible: layoutVisible
    z: 3

    // Mask the bottom rounded corners so the banner sits flush against the
    // composer below — same trick ReplyPopup uses.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: parent.radius
        color: parent.color
        visible: parent.visible
    }

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Komai.paddingMedium
        spacing: Komai.paddingSmall

        // Status icon: pulsing red dot while recording, spinner while
        // transcribing, alert while error, slide-microphone otherwise.
        Item {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: root.headerIconSize
            Layout.preferredHeight: root.headerIconSize

            Rectangle {
                id: recordingDot
                anchors.centerIn: parent
                width: parent.width * 0.6
                height: width
                radius: width / 2
                color: Komai.theme.error
                visible: root.transcriptionState === "recording"

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    running: recordingDot.visible
                    NumberAnimation { to: 0.3; duration: 600 }
                    NumberAnimation { to: 1.0; duration: 600 }
                }
            }

            Image {
                id: transcribingSpinner
                anchors.fill: parent
                visible: root.transcriptionState === "transcribing"
                source: "image://colorimage/:/icons/icons/ui/spinner.svg?" + palette.text
                sourceSize.width: root.headerIconSize
                sourceSize.height: root.headerIconSize
                fillMode: Image.PreserveAspectFit
                smooth: true

                RotationAnimation on rotation {
                    loops: Animation.Infinite
                    running: transcribingSpinner.visible && Settings.uiMotionAnimationsEnabled
                    from: 0
                    to: 360
                    duration: 1200
                }
            }

            Image {
                anchors.fill: parent
                visible: root.transcriptionState === "error"
                source: "image://colorimage/:/icons/icons/ui/alert.svg?" + Komai.theme.error
                sourceSize.width: root.headerIconSize
                sourceSize.height: root.headerIconSize
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Image {
                anchors.fill: parent
                visible: root.transcriptionState === "not-configured"
                source: "image://colorimage/:/icons/icons/ui/transcription.svg?" + palette.text
                sourceSize.width: root.headerIconSize
                sourceSize.height: root.headerIconSize
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
        }

        Label {
            id: statusLabel

            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            color: root.transcriptionState === "error" ? Komai.theme.error : palette.text
            font.pixelSize: root.headerFontSize
            font.bold: true
            elide: Text.ElideRight

            // Reveal the full status text on hover when it does not fit. The
            // error message in particular ("Transcription failed: …") is often
            // long enough to elide to "…"; the tooltip keeps it readable.
            HoverHandler {
                id: statusHover
            }

            // Full-width label: follow the cursor (like the toolbar-button
            // tooltips) rather than parking at a fixed corner of the banner.
            KomaiToolTip {
                anchorItem: statusLabel
                followMouse: true
                delay: 300
                maxWidth: Math.round(root.width * 0.9)
                text: statusLabel.truncated ? statusLabel.text : ""
                requestedVisible: statusHover.hovered && statusLabel.truncated
            }

            text: {
                switch (root.transcriptionState) {
                case "not-configured":
                    return qsTr("Voice transcription is enabled but not configured.");
                case "recording":
                    if (root.isRealtimeProvider) {
                        // Realtime: text is being injected as you speak.
                        if (root.triggerKind === "button-toggle")
                            return qsTr("Recording & transcribing… Click Stop to finish, Esc to cancel.");
                        if (root.triggerKind === "button-hold")
                            return qsTr("Recording & transcribing… Release the button to stop.");
                        return qsTr("Recording & transcribing… Release Space to stop.");
                    }
                    // Batch: only recording right now; transcription happens on stop.
                    if (root.triggerKind === "button-toggle")
                        return qsTr("Recording… Click Stop to transcribe, Esc to cancel.");
                    if (root.triggerKind === "button-hold")
                        return qsTr("Recording… Release the button to transcribe.");
                    return qsTr("Recording… Release Space to transcribe.");
                case "transcribing":
                    return qsTr("Transcribing…");
                case "error":
                    return root.lastError && root.lastError.length > 0
                        ? qsTr("Transcription failed: %1.").arg(root.lastError)
                        : qsTr("Transcription failed.");
                }
                return "";
            }
        }

        // Audio level meter while recording.
        Item {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 80
            Layout.preferredHeight: Math.max(4, root.headerIconSize / 2)
            visible: root.transcriptionState === "recording"

            Rectangle {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                height: parent.height
                width: parent.width * Math.max(0.05, TranscriptionAudioCapture.audioLevel)
                radius: 2
                color: Komai.theme.success
                Behavior on width { NumberAnimation { duration: 60 } }
            }
        }

        // Stop button — visible in toggle-mode recording where there is
        // nothing for the user to "release". Same effect as a second
        // click on the microphone button.
        ImageButton {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: root.headerIconSize
            Layout.preferredWidth: root.headerIconSize
            visible: root.transcriptionState === "recording" && root.triggerKind === "button-toggle"
            hoverEnabled: true
            image: ":/icons/icons/ui/stop.svg"
            toolTipText: qsTr("Stop and transcribe")
            toolTipVisible: hovered
            onClicked: {
                if (root.inputBar)
                    root.inputBar._commitTranscriptionGesture();
            }
        }

        // Open-Settings button — visible in the not-configured state.
        // Drops the user on Settings → Integrations → Voice transcription
        // via the existing in-app deeplink dispatcher. Styled like the
        // "Leave" / "New" action buttons on the room header / tab bar.
        ShellComponents.RoomListActionButton {
            Layout.alignment: Qt.AlignVCenter
            visible: root.transcriptionState === "not-configured"
            buttonSize: Komai.iconSize
            iconSource: ":/icons/icons/ui/settings.svg"
            labelText: qsTr("Settings")
            showLabel: true
            toolTipText: qsTr("Open Settings → Integrations → Voice transcription")
            onClicked: {
                if (root.inputBar)
                    root.inputBar.openTranscriptionSettings();
            }
        }

        // Dismiss button — close the banner. Always present so the user
        // can recover from any non-idle state, including a stuck
        // "Transcribing…" wait. We can't actually cancel the in-flight
        // Rust job, but `_cancelTranscriptionGesture` zeros the tracked
        // jobId, so when the late `batchFinished` / `batchFailed` signal
        // arrives the result is silently dropped. During recording,
        // dismiss is equivalent to Esc.
        ImageButton {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: root.headerIconSize
            Layout.preferredWidth: root.headerIconSize
            hoverEnabled: true
            image: ":/icons/icons/ui/dismiss.svg"
            toolTipText: qsTr("Dismiss")
            toolTipVisible: hovered
            onClicked: {
                if (!root.inputBar)
                    return;
                if (root.transcriptionState === "error")
                    root.inputBar.dismissTranscriptionError();
                else if (root.transcriptionState === "not-configured")
                    root.inputBar.dismissTranscriptionNotConfigured();
                else
                    root.inputBar._cancelTranscriptionGesture();
            }
        }
    }
}

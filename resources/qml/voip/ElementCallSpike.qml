// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Throwaway QtWebEngine build spike for the Element Call work. As of M3a it
// loads a hand-written getUserMedia test page over the SECURE komai-ec://
// scheme (ElementCallWebProfile) to prove that Chromium grants a secure
// context and that camera/microphone acquisition works from native. Reachable
// via the Ctrl+Alt+E shortcut wired in shell/Root.qml; replaced by the real
// call surface in a later milestone.

import QtQuick
import QtQuick.Window
import QtWebEngine
import cc.etke.komai

Window {
    id: spikeWindow

    width: 1024
    height: 768
    title: qsTr("Element Call build spike")

    WebEngineView {
        id: webView

        anchors.fill: parent
        // Dedicated profile that serves the secure komai-ec:// origin.
        profile: ElementCallWebProfile.profile
        url: "komai-ec://app/index.html"

        settings.playbackRequiresUserGesture: false
        settings.screenCaptureEnabled: true

        // Temporary M3a diagnostics (warn level so they surface in Komai's log).
        onLoadingChanged: function (loadRequest) {
            console.warn("[EC] load status=" + loadRequest.status +
                         " url=" + loadRequest.url +
                         " err=" + loadRequest.errorString);
        }
        onJavaScriptConsoleMessage: function (level, message, lineNumber, sourceID) {
            console.warn("[EC] js(" + level + ") " + sourceID + ":" + lineNumber + " " + message);
        }
        onRenderProcessTerminated: function (terminationStatus, exitCode) {
            console.warn("[EC] render process terminated status=" + terminationStatus +
                         " exit=" + exitCode);
        }

        // The page is trusted (we serve it ourselves), so auto-grant the
        // camera/microphone permission requests Chromium raises for
        // getUserMedia. This is the native side of the M3a proof.
        onFeaturePermissionRequested: function (securityOrigin, feature) {
            if (feature === WebEngineView.MediaAudioCapture ||
                feature === WebEngineView.MediaVideoCapture ||
                feature === WebEngineView.MediaAudioVideoCapture) {
                webView.grantFeaturePermission(securityOrigin, feature, true);
            }
        }
    }
}

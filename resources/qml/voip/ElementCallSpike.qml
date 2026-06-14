// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Throwaway QtWebEngine spike for the Element Call work. As of M4 it drives the
// embedded Element Call bundle as a real Matrix widget: ElementCallWidgetSession
// starts a native matrix-sdk WidgetDriver for the currently-open room, hands
// back the widget URL, and we relay the Widget API postMessage traffic both ways
// (page->host over a QWebChannel transport, host->page via runJavaScript). The
// goal at this milestone is that Element Call completes the handshake and reaches
// its lobby. Reachable via Ctrl+Alt+E (wired in shell/Root.qml); replaced by the
// real call surface in a later milestone.

import QtQuick
import QtQuick.Window
import QtWebEngine
import QtWebChannel
import cc.etke.komai

Window {
    id: spikeWindow

    width: 1024
    height: 768
    title: qsTr("Element Call build spike")

    // Drives the widget session for the active room. The QWebChannel transport
    // exposes it to the page as `komaiBridge`; the injected bridge script calls
    // its postMessageFromWidget() slot for every widget->host message.
    ElementCallWidgetSession {
        id: ecSession

        WebChannel.id: "komaiBridge"

        // Host -> widget: inject the driver's message into the page. JSON.parse
        // of a stringified literal keeps the payload intact and safely escaped.
        onMessageToWidget: function (json) {
            webView.runJavaScript("window.postMessage(JSON.parse(" +
                                  JSON.stringify(json) + "), '*')");
        }
        onUrlReady: function (url) {
            console.warn("[EC] widget URL ready: " + url);
        }
        onStopped: function (reason) {
            console.warn("[EC] widget session stopped" +
                         (reason.length ? (": " + reason) : ""));
        }
    }

    WebChannel {
        id: ecChannel
        registeredObjects: [ecSession]
    }

    WebEngineView {
        id: webView

        anchors.fill: parent
        // Dedicated profile that serves the secure komai-ec:// origin.
        profile: ElementCallWebProfile.profile
        webChannel: ecChannel
        // qwebchannel.js + the window.postMessage bridge, built in C++ because
        // Qt 6's WebEngineScript is a value type that can't be created from QML.
        // They inject at DocumentCreation in the main world, so the bridge's
        // message listener is attached before Element Call posts anything.
        userScripts.collection: ElementCallWebProfile.bridgeUserScripts()
        // Empty until ElementCallWidgetSession reports the generated widget URL;
        // then the view navigates to Element Call in widget mode.
        url: ecSession.url

        settings.playbackRequiresUserGesture: false
        settings.screenCaptureEnabled: true

        // Temporary diagnostics (warn level so they surface in Komai's log).
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
        // camera/microphone permission requests Chromium raises for getUserMedia.
        onFeaturePermissionRequested: function (securityOrigin, feature) {
            if (feature === WebEngineView.MediaAudioCapture ||
                feature === WebEngineView.MediaVideoCapture ||
                feature === WebEngineView.MediaAudioVideoCapture) {
                webView.grantFeaturePermission(securityOrigin, feature, true);
            }
        }
    }

    // Shown until a session is started / while there is no room to call.
    Text {
        anchors.centerIn: parent
        visible: !ecSession.active
        text: Rooms.currentRoomId.length
              ? qsTr("Starting Element Call…")
              : qsTr("Open a room first, then press Ctrl+Alt+E.")
    }

    // Start a widget session for the currently-open room as soon as the spike
    // opens. (M4 testing entry point; the real UX gets a proper call button.)
    Component.onCompleted: {
        if (Rooms.currentRoomId.length)
            ecSession.start(Rooms.currentRoomId, "");
        else
            console.warn("[EC] no current room; open a room before launching the spike");
    }
}

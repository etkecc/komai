// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// In-room Element Call surface: a collapsible panel that hosts the Element Call
// QtWebEngine widget for the room with the active call. It is the real-call
// replacement for the throwaway ElementCallSpike window.
//
// Lifecycle is coordinated by the always-compiled ElementCall singleton: the
// composer call button (or a dev shortcut) calls ElementCall.startCall(roomId),
// which flips ElementCall.active and makes this panel's Loader (in TimelineView)
// instantiate us. We start a native matrix-sdk widget session for the room and
// relay the Widget API postMessage traffic both ways, exactly like the spike.
//
// Komai owns only the collapsed <-> expanded axis (a thin header bar that
// expands to host the widget); Element Call owns its own in-call layout
// (spotlight/grid/...). The panel shows only while the call's room is the one on
// screen; when another room is open it hides but stays alive (the WebRTC session
// keeps running), the same way the legacy ActiveCallBar persists across switches.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine
import QtWebChannel
import "../ui"
import cc.etke.komai

Item {
    id: panel

    // The room this panel was created for (captured once at load; ElementCall
    // only runs one call at a time).
    readonly property string callRoomId: ElementCall.activeRoomId

    // Total height the timeline area offers us; set by the hosting Loader so our
    // expanded height does not depend on our own height (avoids a binding loop).
    property real availableHeight: 0

    // Komai's one view-mode axis. Defaults to expanded while in a call.
    property bool collapsed: false

    // True from the moment the user asks to leave until the session is torn down.
    // Leaving runs Element Call's graceful drain (disconnect LiveKit, publish the
    // leave membership) which takes a moment, so we show a busy state instead of
    // letting the call view sit there looking frozen.
    property bool leaving: false

    // Height the panel wants to occupy. The Loader reads this.
    readonly property real panelHeight: collapsed
        ? headerBar.implicitHeight
        : Math.max(headerBar.implicitHeight + 200,
                   Math.min(720, Math.round(availableHeight * 0.62)))

    // Set once we are tearing down so the close handlers stop re-entering.
    property bool closing: false

    implicitHeight: panelHeight

    // Tear the session down and tell the controller, which clears ElementCall
    // state and unloads us. Idempotent.
    function teardown() {
        if (panel.closing)
            return;
        panel.closing = true;
        hangupFallbackTimer.stop();
        ecSession.stop();
        ElementCall.notifyStopped();
    }

    // Drives the widget session for the call room. Exposed to the page as
    // `komaiBridge` over the QWebChannel transport.
    ElementCallWidgetSession {
        id: ecSession

        WebChannel.id: "komaiBridge"

        onMessageToWidget: function (json) {
            webView.runJavaScript("window.postMessage(JSON.parse(" +
                                  JSON.stringify(json) + "), '*')");
        }
        // Element Call asked the host to dismiss the surface (posted after the
        // user hangs up, once it has run its own leave flow).
        onCloseRequested: panel.teardown();
        onStopped: function (reason) {
            if (reason.length)
                console.warn("[EC] widget session stopped: " + reason);
            panel.teardown();
        }
    }

    // The composer "leave call" path: ElementCall.hangup() asks us to leave
    // gracefully. Element Call replies, runs its leave flow, then posts
    // io.element.close which routes through onCloseRequested -> teardown.
    Connections {
        target: ElementCall
        function onHangupRequested() {
            if (panel.closing)
                return;
            if (ecSession.active) {
                panel.leaving = true;
                ecSession.hangup();
                hangupFallbackTimer.restart();
            } else {
                panel.teardown();
            }
        }
    }

    // Close anyway if Element Call never posts io.element.close after we ask it
    // to leave (e.g. it failed to join), rather than hanging.
    Timer {
        id: hangupFallbackTimer
        interval: 2500
        onTriggered: panel.teardown();
    }

    WebChannel {
        id: ecChannel
        registeredObjects: [ecSession]
    }

    // Absorbs stray mouse/hover events over the panel so they do not fall
    // through to the timeline behind us (the panel is an overlay on top of the
    // timeline; without this, hovering the bar surfaced tooltips from the
    // messages underneath). Declared first so the bar, its buttons and the
    // webview (all later siblings) sit on top and get their events normally.
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
    }

    // ── Komai header bar (collapse/expand + end call) ───────────────────────
    // Explicit navigationRowHeight so it lines up with the room header and the
    // other in-room bars.
    Rectangle {
        id: headerBar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        implicitHeight: Komai.navigationRowHeight
        // Call green, matching the legacy ActiveCallBar and the Element Call
        // "active call" bar shown in other rooms, so call surfaces read alike.
        color: "#2ECC71"
        z: 1

        RowLayout {
            id: headerRow
            anchors.fill: parent
            anchors.leftMargin: Komai.paddingMedium
            anchors.rightMargin: Komai.paddingSmall
            spacing: Komai.paddingSmall

            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: qsTr("Element Call")
                color: "#000000"
                font.bold: true
            }

            // Native mic / camera toggles mirroring Element Call's own state
            // (io.element.device_mute). Most useful while the panel is collapsed,
            // when Element Call's in-call controls are hidden; they appear only
            // once Element Call has reported its mute state. Icon shows the
            // current state; clicking toggles it.
            ElementCallBarButton {
                visible: ecSession.deviceControlsAvailable
                style: ElementCallBarButton.Style.OnAccent
                image: ecSession.micEnabled
                    ? ":/icons/icons/ui/microphone-unmute.svg"
                    : ":/icons/icons/ui/microphone-mute.svg"
                onClicked: ecSession.setMicEnabled(!ecSession.micEnabled)
                toolTipText: ecSession.micEnabled
                    ? qsTr("Mute microphone")
                    : qsTr("Unmute microphone")
            }

            ElementCallBarButton {
                visible: ecSession.deviceControlsAvailable
                style: ElementCallBarButton.Style.OnAccent
                image: ecSession.cameraEnabled
                    ? ":/icons/icons/ui/video.svg"
                    : ":/icons/icons/ui/video-off.svg"
                onClicked: ecSession.setCameraEnabled(!ecSession.cameraEnabled)
                toolTipText: ecSession.cameraEnabled
                    ? qsTr("Turn camera off")
                    : qsTr("Turn camera on")
            }

            ElementCallBarButton {
                text: panel.collapsed ? qsTr("Expand") : qsTr("Collapse")
                image: panel.collapsed
                    ? ":/icons/icons/ui/chevron-down.svg"
                    : ":/icons/icons/ui/chevron-up.svg"
                style: ElementCallBarButton.Style.OnAccent
                onClicked: panel.collapsed = !panel.collapsed
            }

            // Destructive red "End call", matching the in-call control in the
            // Element Call web UI. The label stays put while leaving (only the
            // disabled state changes) so nothing shifts around; the busy overlay
            // carries the "Leaving call…" message.
            ElementCallBarButton {
                text: qsTr("End call")
                image: ":/icons/icons/ui/end-call.svg"
                style: ElementCallBarButton.Style.Danger
                enabled: !panel.leaving
                onClicked: ElementCall.hangup()
            }
        }
    }

    // Separator under the bar, matching the 1px line under the room header and
    // the room tab bar (Komai.theme.separator is the canonical separator colour).
    Rectangle {
        id: barSeparator
        anchors.top: headerBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Komai.theme.separator
        z: 2
    }

    // ── Element Call widget ─────────────────────────────────────────────────
    WebEngineView {
        id: webView

        anchors.top: barSeparator.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: !panel.collapsed

        // Dedicated profile that serves the secure komai-ec:// origin.
        profile: ElementCallWebProfile.profile
        webChannel: ecChannel
        // qwebchannel.js + the window.postMessage bridge, built in C++ because
        // Qt 6's WebEngineScript is a value type that can't be created from QML.
        userScripts.collection: ElementCallWebProfile.bridgeUserScripts()
        // Empty until ElementCallWidgetSession reports the generated widget URL.
        url: ecSession.url

        settings.playbackRequiresUserGesture: false
        settings.screenCaptureEnabled: true

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

    // Bottom border so it is clear where the call surface ends (when expanded).
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Komai.theme.separator
        visible: !panel.collapsed
        z: 3
    }

    // Busy overlay shown while a session is starting (no URL yet) or while the
    // user is leaving the call (Element Call's graceful drain takes a moment).
    Rectangle {
        anchors.fill: webView
        visible: !panel.collapsed && (panel.leaving || !ecSession.url.length)
        color: palette.window
        z: 4

        // Absorb events so the draining webview behind us is not interacted with.
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: Komai.paddingMedium

            Spinner {
                Layout.alignment: Qt.AlignHCenter
                running: true
                foreground: palette.text
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: panel.leaving ? qsTr("Leaving call…") : qsTr("Starting Element Call…")
            }
        }
    }

    // Start the widget session for the call room as soon as we are instantiated.
    Component.onCompleted: {
        if (panel.callRoomId.length)
            ecSession.start(panel.callRoomId);
        else
            console.warn("[EC] panel loaded without an active room");
    }
}

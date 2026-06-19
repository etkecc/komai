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
import QtQuick.Window
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

    // True while the call is shown fullscreen: the webview is reparented to the
    // window-level overlay (covering the whole window, room list included) and
    // the OS window is switched to fullscreen. Both our own fullscreen button and
    // Element Call's in-page fullscreen button (onFullScreenRequested) drive this.
    property bool fullscreen: false

    // The OS window visibility before we went fullscreen, so we restore exactly
    // what the user had (windowed vs maximized) on exit.
    property int _savedVisibility: Window.Windowed

    onFullscreenChanged: {
        const win = panel.Window.window;
        if (!win)
            return;
        if (panel.fullscreen) {
            panel._savedVisibility = win.visibility;
            win.visibility = Window.FullScreen;
            // Take keyboard focus onto the key-catcher so Escape reaches its
            // Keys.onPressed. The timeline's window-wide Escape shortcut is
            // disabled while we are fullscreen (see TimelineView), so the key now
            // propagates to whatever item holds focus -- which must be the
            // key-catcher, not the webview (Chromium would eat it).
            escapeCatcher.forceActiveFocus();
        } else if (win.visibility === Window.FullScreen) {
            win.visibility = panel._savedVisibility !== Window.FullScreen
                ? panel._savedVisibility
                : Window.Windowed;
        }
    }

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
        // Restore the OS window before the surface goes away.
        panel.fullscreen = false;
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
        // Double-clicking the call view (away from Element Call's own controls)
        // toggles fullscreen.
        onFullscreenToggleRequested: panel.fullscreen = !panel.fullscreen
        // Escape inside the webview leaves fullscreen.
        onExitFullscreenRequested: panel.fullscreen = false
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
                text: ecSession.micEnabled ? qsTr("Mute") : qsTr("Unmute")
                altText: ecSession.micEnabled ? qsTr("Unmute") : qsTr("Mute")
                onClicked: ecSession.setMicEnabled(!ecSession.micEnabled)
            }

            ElementCallBarButton {
                visible: ecSession.deviceControlsAvailable
                style: ElementCallBarButton.Style.OnAccent
                image: ecSession.cameraEnabled
                    ? ":/icons/icons/ui/video.svg"
                    : ":/icons/icons/ui/video-off.svg"
                text: ecSession.cameraEnabled ? qsTr("Stop camera") : qsTr("Start camera")
                altText: ecSession.cameraEnabled ? qsTr("Start camera") : qsTr("Stop camera")
                onClicked: ecSession.setCameraEnabled(!ecSession.cameraEnabled)
            }

            ElementCallBarButton {
                text: panel.collapsed ? qsTr("Expand") : qsTr("Collapse")
                altText: panel.collapsed ? qsTr("Collapse") : qsTr("Expand")
                image: panel.collapsed
                    ? ":/icons/icons/ui/chevron-down.svg"
                    : ":/icons/icons/ui/chevron-up.svg"
                style: ElementCallBarButton.Style.OnAccent
                onClicked: panel.collapsed = !panel.collapsed
            }

            ElementCallBarButton {
                style: ElementCallBarButton.Style.OnAccent
                image: ":/icons/icons/ui/fullscreen-maximize.svg"
                text: qsTr("Fullscreen")
                onClicked: panel.fullscreen = true
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
    // Stable in-panel slot the webview occupies normally. The webview is
    // reparented away (to fullscreenHost, below) for fullscreen, so the busy
    // overlay and the bottom border anchor to this slot rather than to the
    // webview itself (which would cross parents and break the anchors).
    Item {
        id: webSlot
        anchors.top: barSeparator.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }

    WebEngineView {
        id: webView

        // Lives in the in-panel slot normally; for fullscreen it reparents to the
        // window-level overlay so it covers the whole window. This is a same-
        // QQuickWindow reparent, so the render surface (and the live WebRTC
        // session) are not torn down. anchors.fill: parent follows the reparent.
        parent: panel.fullscreen ? fullscreenHost : webSlot
        anchors.fill: parent
        visible: panel.fullscreen || !panel.collapsed

        // Keep keyboard focus on the key-catcher while fullscreen so Escape always
        // reaches it: the webview grabs focus when it reparents/shows and on click,
        // so bounce focus back each time. The cost is no keyboard input to the call
        // while fullscreen, which is fine (mouse still drives Element Call's
        // controls; exit fullscreen to type).
        onActiveFocusChanged: {
            if (panel.fullscreen && webView.activeFocus)
                escapeCatcher.forceActiveFocus();
        }

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
        // Let Element Call's own in-page fullscreen button work: without this
        // QtWebEngine silently drops the page's requestFullscreen() call.
        settings.fullScreenSupportEnabled: true

        // Element Call's in-page fullscreen button calls requestFullscreen();
        // honour it by entering/leaving our own fullscreen (same path as our
        // header button) so the webview actually fills the screen.
        onFullScreenRequested: function (request) {
            request.accept();
            panel.fullscreen = request.toggleOn;
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

    // Bottom border so it is clear where the call surface ends (when expanded).
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Komai.theme.separator
        visible: !panel.collapsed && !panel.fullscreen
        z: 3
    }

    // Busy overlay shown while a session is starting (no URL yet) or while the
    // user is leaving the call (Element Call's graceful drain takes a moment).
    Rectangle {
        anchors.fill: webSlot
        visible: !panel.collapsed && !panel.fullscreen && (panel.leaving || !ecSession.url.length)
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

    // ── Fullscreen host ──────────────────────────────────────────────────────
    // Window-level layer (above all chrome, room list included) the webview
    // reparents into for fullscreen. Declared here for id scoping but parented to
    // the window overlay, like Snackbar/dialogs. Holds only our control pill; the
    // webview is added as a child by its `parent:` binding above.
    Item {
        id: fullscreenHost

        parent: Overlay.overlay
        anchors.fill: parent
        visible: panel.fullscreen
        z: 100000

        // Holds keyboard focus while fullscreen (see onFullscreenChanged) so that
        // Escape generates a Qt key event we can act on; the Chromium webview
        // otherwise consumes Escape natively. A focus-only Item: it has no visuals
        // and no MouseArea, so clicks still reach the webview and the OSD buttons.
        Item {
            id: escapeCatcher
            anchors.fill: parent
            focus: panel.fullscreen
            Keys.onPressed: function (event) {
                if (event.key === Qt.Key_Escape) {
                    panel.fullscreen = false;
                    event.accepted = true;
                }
            }
        }

        // Control bar flush in the top-right corner: Exit is the rightmost
        // button so it sits in the very corner (the easiest target to hit by
        // slamming the pointer there). A dark translucent bar with light icons
        // (rather than the in-room green bars) since it floats over the call
        // video; sits above the webview (higher z). Only the inner (bottom-left)
        // corner is rounded so the bar reaches the screen corner cleanly.
        Rectangle {
            id: osdBar
            anchors.top: parent.top
            anchors.right: parent.right
            // Wrap the buttons tightly (no extra bar padding) so the rightmost
            // button sits flush in the screen corner.
            implicitWidth: pillRow.implicitWidth
            implicitHeight: pillRow.implicitHeight
            bottomLeftRadius: Komai.paddingSmall
            // Solid enough to give the white icons contrast over arbitrary video
            // on its own (the media viewer gets this from a full-screen dim we do
            // not have here, so this bar carries it).
            color: Qt.rgba(0, 0, 0, 0.55)
            z: 10

            RowLayout {
                id: pillRow
                anchors.fill: parent
                spacing: 0

                ElementCallBarButton {
                    visible: ecSession.deviceControlsAvailable
                    style: ElementCallBarButton.Style.OnDark
                    image: ecSession.micEnabled
                        ? ":/icons/icons/ui/microphone-unmute.svg"
                        : ":/icons/icons/ui/microphone-mute.svg"
                    text: ecSession.micEnabled ? qsTr("Mute") : qsTr("Unmute")
                    altText: ecSession.micEnabled ? qsTr("Unmute") : qsTr("Mute")
                    onClicked: ecSession.setMicEnabled(!ecSession.micEnabled)
                }

                ElementCallBarButton {
                    visible: ecSession.deviceControlsAvailable
                    style: ElementCallBarButton.Style.OnDark
                    image: ecSession.cameraEnabled
                        ? ":/icons/icons/ui/video.svg"
                        : ":/icons/icons/ui/video-off.svg"
                    text: ecSession.cameraEnabled ? qsTr("Stop camera") : qsTr("Start camera")
                    altText: ecSession.cameraEnabled ? qsTr("Start camera") : qsTr("Stop camera")
                    onClicked: ecSession.setCameraEnabled(!ecSession.cameraEnabled)
                }

                ElementCallBarButton {
                    text: qsTr("End call")
                    image: ":/icons/icons/ui/end-call.svg"
                    style: ElementCallBarButton.Style.Danger
                    enabled: !panel.leaving
                    // Leave fullscreen first so the leaving spinner shows in the
                    // in-room panel as usual, then hang up.
                    onClicked: {
                        panel.fullscreen = false;
                        ElementCall.hangup();
                    }
                }

                ElementCallBarButton {
                    style: ElementCallBarButton.Style.OnDark
                    image: ":/icons/icons/ui/fullscreen-minimize.svg"
                    text: qsTr("Exit fullscreen")
                    onClicked: panel.fullscreen = false
                }
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

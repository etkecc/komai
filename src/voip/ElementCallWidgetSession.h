// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QString>

// Bridges one Element Call widget session between the QtWebEngine view (which
// speaks the Matrix Widget API over window.postMessage) and the native
// matrix-sdk widget driver running in Rust.
//
// The flow, per session:
//   1. QML calls start(roomId, theme). We ask Rust to spin up a WidgetDriver
//      for that room; it answers asynchronously with the generated webview URL
//      (urlReady) which QML then loads.
//   2. The injected JS bridge in the page calls postMessageFromWidget() (via
//      QWebChannel) for every widget->host message; we forward it to the driver.
//   3. Driver->widget messages arrive from Rust (routed here by session id) and
//      are emitted as messageToWidget() for QML to inject via runJavaScript.
//
// This type deliberately has NO QtWebEngine dependency: the WebEngineView, the
// QWebChannel transport and the runJavaScript injection all live in QML. That
// keeps it cheap to include from the always-compiled Matrix backend bridge.
class ElementCallWidgetSession : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString url READ url NOTIFY urlChanged)
    Q_PROPERTY(qulonglong sessionId READ sessionId NOTIFY sessionIdChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

    // Live mirror of Element Call's own microphone / camera mute state, learned
    // from the io.element.device_mute messages it posts (on join and on every
    // internal toggle). `deviceControlsAvailable` flips true once we have seen at
    // least one such message, so the native toggles only appear with real state.
    Q_PROPERTY(bool micEnabled READ micEnabled NOTIFY deviceMuteStateChanged)
    Q_PROPERTY(bool cameraEnabled READ cameraEnabled NOTIFY deviceMuteStateChanged)
    Q_PROPERTY(
      bool deviceControlsAvailable READ deviceControlsAvailable NOTIFY deviceMuteStateChanged)

public:
    explicit ElementCallWidgetSession(QObject *parent = nullptr);
    ~ElementCallWidgetSession() override;

    QString url() const { return url_; }
    qulonglong sessionId() const { return sessionId_; }
    bool active() const { return sessionId_ != 0; }

    bool micEnabled() const { return micEnabled_; }
    bool cameraEnabled() const { return cameraEnabled_; }
    bool deviceControlsAvailable() const { return deviceMuteStateKnown_; }

    // Starts a widget session for roomId on the current backend handle. The
    // Element Call locale and theme are resolved from the current UI settings.
    // Returns false if the driver could not be started; otherwise the URL
    // arrives later via urlReady.
    Q_INVOKABLE bool start(const QString &roomId);

    // Tears the session down (hangup / call surface closed).
    Q_INVOKABLE void stop();

    // Asks Element Call to leave the call gracefully: sends the host->widget
    // im.vector.hangup action. Element Call runs its own leave flow and then
    // posts io.element.close back, which we intercept to tear the session down.
    // Used when the call surface is dismissed by the user (vs Element Call's own
    // in-call hangup button, which posts io.element.close directly).
    Q_INVOKABLE void hangup();

    // Toggle Element Call's microphone / camera from native chrome. Sends a
    // toWidget io.element.device_mute carrying only the changed field; Element
    // Call applies it and echoes the resulting state back via the fromWidget
    // device_mute we mirror, so micEnabled/cameraEnabled stay authoritative.
    Q_INVOKABLE void setMicEnabled(bool enabled);
    Q_INVOKABLE void setCameraEnabled(bool enabled);

    // Called from the injected page bridge when the user double-clicks the call
    // view (away from Element Call's own controls); the call surface toggles
    // fullscreen in response.
    Q_INVOKABLE void requestFullscreenToggle() { emit fullscreenToggleRequested(); }

    // Called from the injected page bridge when the user presses Escape while the
    // webview itself has keyboard focus (e.g. after clicking into the call video).
    // The QML side normally holds focus on a key-catcher while fullscreen, but
    // this covers the case where the page has it instead.
    Q_INVOKABLE void requestExitFullscreen() { emit exitFullscreenRequested(); }

    // QWebChannel-exposed slot the injected page bridge calls for every
    // widget->host Widget API message. Element Call-specific host actions
    // (io.element.close, io.element.join, set_always_on_screen,
    // io.element.device_mute) are answered locally; everything else is
    // forwarded to the Rust driver.
    Q_INVOKABLE void postMessageFromWidget(const QString &json);

    // Entry points used by the Matrix backend bridge to route driver callbacks
    // (which fire on tokio worker threads) to the right session on the GUI
    // thread. No-ops if the session id is unknown (already torn down).
    static void deliverUrlReady(quint64 sessionId, const QString &url);
    static void deliverMessage(quint64 sessionId, const QString &message);
    static void deliverStopped(quint64 sessionId, const QString &reason);

signals:
    void urlChanged();
    void sessionIdChanged();
    void activeChanged();
    // Emitted whenever the mirrored mic/camera state or its availability changes.
    void deviceMuteStateChanged();
    // Emitted when the user double-clicks the call view (see requestFullscreenToggle).
    void fullscreenToggleRequested();
    // Emitted when the user presses Escape inside the webview (see requestExitFullscreen).
    void exitFullscreenRequested();

    // The webview should load this URL (Element Call in widget mode).
    void urlReady(const QString &url);
    // A driver->widget message to inject into the page via window.postMessage.
    void messageToWidget(const QString &json);
    // The session ended (reason is empty for a normal stop).
    void stopped(const QString &reason);
    // Element Call asked the host to close the call surface (io.element.close,
    // posted after a hangup). The call surface should dismiss itself; tearing it
    // down destroys this object, which stops the driver.
    void closeRequested();

private:
    // Intercepts the Element Call-specific host actions the matrix-sdk widget
    // driver does not implement, answering them locally instead of letting the
    // driver reject them. Returns true if the message was handled here (and must
    // not be forwarded to the driver), false if it should go to the driver.
    bool interceptHostAction(const QString &json);
    // Sends a Widget API message to the page (host->widget). Reused for both
    // fromWidget responses (acks) and toWidget requests we originate.
    void sendToWidget(const class QJsonObject &message);
    // Builds and sends a host->widget (toWidget) request for `action` with `data`,
    // registering its requestId so Element Call's reply is swallowed (it would not
    // match a driver UUID). Used by hangup() and the device-mute toggles.
    void sendToWidgetRequest(const QString &action, const class QJsonObject &data);

    void clearSession();

    static QHash<quint64, ElementCallWidgetSession *> &registry();

    QString url_;
    quint64 sessionId_ = 0;
    // The widget id Element Call uses, learned from its first message. Needed to
    // address host->widget requests (e.g. im.vector.hangup) back at it.
    QString widgetId_;
    // Monotonic counter for the requestId of host->widget requests we originate.
    quint64 requestCounter_ = 0;
    // requestIds of host->widget requests we originated and are still awaiting a
    // reply for. Element Call's reply is a toWidget response that must NOT reach
    // the driver (it never sent the request, and would log an error trying to
    // match our non-UUID requestId).
    QSet<QString> pendingHostRequests_;

    // Mirror of Element Call's mic/camera state (see the Q_PROPERTYs). Default to
    // enabled; corrected by the first io.element.device_mute Element Call posts.
    bool micEnabled_           = true;
    bool cameraEnabled_        = true;
    bool deviceMuteStateKnown_ = false;
};

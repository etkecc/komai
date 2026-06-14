// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QObject>
#include <QQmlEngine>
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

public:
    explicit ElementCallWidgetSession(QObject *parent = nullptr);
    ~ElementCallWidgetSession() override;

    QString url() const { return url_; }
    qulonglong sessionId() const { return sessionId_; }
    bool active() const { return sessionId_ != 0; }

    // Starts a widget session for roomId on the current backend handle. theme is
    // "dark"/"light" (or empty to let Element Call default). Returns false if the
    // driver could not be started; otherwise the URL arrives later via urlReady.
    Q_INVOKABLE bool start(const QString &roomId, const QString &theme);

    // Tears the session down (hangup / call surface closed).
    Q_INVOKABLE void stop();

    // QWebChannel-exposed slot the injected page bridge calls for every
    // widget->host Widget API message. Forwarded verbatim to the Rust driver.
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

    // The webview should load this URL (Element Call in widget mode).
    void urlReady(const QString &url);
    // A driver->widget message to inject into the page via window.postMessage.
    void messageToWidget(const QString &json);
    // The session ended (reason is empty for a normal stop).
    void stopped(const QString &reason);

private:
    void clearSession();

    static QHash<quint64, ElementCallWidgetSession *> &registry();

    QString url_;
    quint64 sessionId_ = 0;
};

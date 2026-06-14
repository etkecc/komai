// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ElementCallWidgetSession.h"

#include <exception>

#include "komai-rust-cxxbridge/ffi.h"

#include "logging/Logging.h"
#include "ui/MainWindow.h"
#include "voip/ElementCallWebProfile.h"

QHash<quint64, ElementCallWidgetSession *> &
ElementCallWidgetSession::registry()
{
    // All access happens on the GUI thread (start/stop/dtor are called from QML;
    // the deliver*() callbacks are marshalled onto the GUI thread by the Matrix
    // backend bridge), so no locking is needed.
    static QHash<quint64, ElementCallWidgetSession *> instances;
    return instances;
}

ElementCallWidgetSession::ElementCallWidgetSession(QObject *parent)
  : QObject(parent)
{
}

ElementCallWidgetSession::~ElementCallWidgetSession()
{
    stop();
}

bool
ElementCallWidgetSession::start(const QString &roomId, const QString &theme)
{
    if (sessionId_ != 0) {
        komai::logging::ui()->warn("[EC] start() called on an already-active widget session");
        return false;
    }

    auto *mainWindow       = MainWindow::instance();
    const quint64 handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        komai::logging::ui()->warn("[EC] cannot start widget session: no Matrix backend handle");
        return false;
    }
    if (roomId.trimmed().isEmpty()) {
        komai::logging::ui()->warn("[EC] cannot start widget session: empty room id");
        return false;
    }

    // The secure origin the embedded bundle is served from (see
    // ElementCallWebProfile). Single source of truth for the scheme/host.
    const QString baseUrl =
      QStringLiteral("%1://%2/")
        .arg(QLatin1String(komai::elementcall::kScheme), QLatin1String(komai::elementcall::kHost));

    quint64 sessionId = 0;
    try {
        sessionId = ::komai::rust::matrix_element_call_start_session(
          handleId, roomId.toStdString(), baseUrl.toStdString(), theme.toStdString());
    } catch (const std::exception &e) {
        komai::logging::ui()->warn("[EC] failed to start widget session: {}", e.what());
        return false;
    }

    sessionId_ = sessionId;
    registry().insert(sessionId_, this);
    emit sessionIdChanged();
    emit activeChanged();

    komai::logging::ui()->warn(
      "[EC] started widget session {} for room {}", sessionId_, roomId.toStdString());
    return true;
}

void
ElementCallWidgetSession::stop()
{
    if (sessionId_ == 0)
        return;

    const quint64 sessionId = sessionId_;
    // Clear local state first so the resulting stopped() callback (which routes
    // back here by id) finds nothing and becomes a no-op.
    clearSession();

    try {
        ::komai::rust::matrix_element_call_stop_session(sessionId);
    } catch (const std::exception &e) {
        komai::logging::ui()->warn("[EC] failed to stop widget session: {}", e.what());
    }
    komai::logging::ui()->warn("[EC] stopped widget session {}", sessionId);
}

void
ElementCallWidgetSession::postMessageFromWidget(const QString &json)
{
    if (sessionId_ == 0)
        return;

    try {
        ::komai::rust::matrix_element_call_send_message(sessionId_, json.toStdString());
    } catch (const std::exception &e) {
        komai::logging::ui()->warn("[EC] failed to forward widget message: {}", e.what());
    }
}

void
ElementCallWidgetSession::clearSession()
{
    if (sessionId_ == 0)
        return;
    registry().remove(sessionId_);
    sessionId_ = 0;
    emit sessionIdChanged();
    emit activeChanged();
}

void
ElementCallWidgetSession::deliverUrlReady(quint64 sessionId, const QString &url)
{
    auto *session = registry().value(sessionId, nullptr);
    if (!session)
        return;
    session->url_ = url;
    emit session->urlChanged();
    emit session->urlReady(url);
}

void
ElementCallWidgetSession::deliverMessage(quint64 sessionId, const QString &message)
{
    auto *session = registry().value(sessionId, nullptr);
    if (!session)
        return;
    emit session->messageToWidget(message);
}

void
ElementCallWidgetSession::deliverStopped(quint64 sessionId, const QString &reason)
{
    auto *session = registry().value(sessionId, nullptr);
    if (!session)
        return;
    session->clearSession();
    emit session->stopped(reason);
}

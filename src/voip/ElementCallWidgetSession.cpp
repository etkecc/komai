// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ElementCallWidgetSession.h"

#include <exception>

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

#include "komai-rust-cxxbridge/ffi.h"

#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"
#include "ui/ThemeRegistry.h"
#include "voip/ElementCallWebProfile.h"

namespace {
// Resolves the effective Element Call theme ("dark"/"light") from the active UI
// theme slug. Element Call only distinguishes the two variants.
QString
currentElementCallTheme()
{
    const auto settings = UserSettings::instance();
    if (!settings)
        return QString();
    return ThemeRegistry::instance().themeVariant(settings->uiThemeSlug());
}

// Resolves the UI locale as a BCP-47 language tag Element Call understands
// ("en-US", "de", ...). Komai stores language codes with underscores ("pt_BR")
// and an empty value means "use the system locale"; normalise both.
QString
currentElementCallLocale()
{
    const auto settings = UserSettings::instance();
    QString code        = settings ? settings->uiLanguage() : QString();
    if (code.isEmpty())
        code = QLocale::system().name(); // e.g. "en_US"
    return code.replace(QLatin1Char('_'), QLatin1Char('-'));
}
}

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
ElementCallWidgetSession::start(const QString &roomId)
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

    const QString lang  = currentElementCallLocale();
    const QString theme = currentElementCallTheme();

    quint64 sessionId = 0;
    try {
        sessionId = ::komai::rust::matrix_element_call_start_session(handleId,
                                                                     roomId.toStdString(),
                                                                     baseUrl.toStdString(),
                                                                     lang.toStdString(),
                                                                     theme.toStdString());
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
ElementCallWidgetSession::hangup()
{
    if (sessionId_ == 0)
        return;

    komai::logging::ui()->warn("[EC] requesting hangup for widget session {}", sessionId_);
    sendToWidgetRequest(QStringLiteral("im.vector.hangup"), QJsonObject{});
}

void
ElementCallWidgetSession::setMicEnabled(bool enabled)
{
    if (sessionId_ == 0 || micEnabled_ == enabled)
        return;
    // Send only the changed field; Element Call copies its current state and
    // applies the provided keys, then reports the resulting state back via the
    // fromWidget device_mute we mirror (which is what actually updates our
    // properties), so we do not optimistically flip them here.
    sendToWidgetRequest(QStringLiteral("io.element.device_mute"),
                        QJsonObject{{QStringLiteral("audio_enabled"), enabled}});
}

void
ElementCallWidgetSession::setCameraEnabled(bool enabled)
{
    if (sessionId_ == 0 || cameraEnabled_ == enabled)
        return;
    sendToWidgetRequest(QStringLiteral("io.element.device_mute"),
                        QJsonObject{{QStringLiteral("video_enabled"), enabled}});
}

void
ElementCallWidgetSession::postMessageFromWidget(const QString &json)
{
    if (sessionId_ == 0)
        return;

    // Element Call posts a few host actions the matrix-sdk widget driver does
    // not implement; answer those here rather than forward them (the driver
    // would reject them as unknown actions, and io.element.close in particular
    // must tear the call surface down).
    if (interceptHostAction(json))
        return;

    try {
        ::komai::rust::matrix_element_call_send_message(sessionId_, json.toStdString());
    } catch (const std::exception &e) {
        komai::logging::ui()->warn("[EC] failed to forward widget message: {}", e.what());
    }
}

bool
ElementCallWidgetSession::interceptHostAction(const QString &json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject())
        return false;
    const QJsonObject obj = doc.object();

    const QString api     = obj.value(QStringLiteral("api")).toString();
    const bool isResponse = obj.contains(QStringLiteral("response"));

    // Swallow Element Call's reply to a host->widget request we originated (e.g.
    // im.vector.hangup): the driver never sent it and logs an error matching our
    // non-UUID requestId. Real driver responses (UUID ids) are not in the set.
    if (api == QLatin1String("toWidget") && isResponse) {
        const QString requestId = obj.value(QStringLiteral("requestId")).toString();
        return pendingHostRequests_.remove(requestId);
    }

    // Otherwise only widget->host requests are host actions. A fromWidget
    // message that already carries a "response" is itself a response, not a
    // request, so leave it for the driver.
    if (api != QLatin1String("fromWidget"))
        return false;
    if (isResponse)
        return false;

    // Learn Element Call's widget id so we can address requests back at it.
    const QString widgetId = obj.value(QStringLiteral("widgetId")).toString();
    if (!widgetId.isEmpty() && widgetId_ != widgetId)
        widgetId_ = widgetId;

    const QString action = obj.value(QStringLiteral("action")).toString();

    // Builds the Widget API response envelope for a request: the request object
    // with a "response" field appended (matrix-widget-api's reply() shape).
    const auto reply = [this, &obj](const QJsonValue &responseData) {
        QJsonObject response = obj;
        response.insert(QStringLiteral("response"), responseData);
        sendToWidget(response);
    };

    if (action == QLatin1String("io.element.close")) {
        // Element Call (or its hangup flow) asks the host to dismiss the widget.
        reply(QJsonObject{});
        komai::logging::ui()->warn("[EC] widget requested close for session {}", sessionId_);
        emit closeRequested();
        return true;
    }
    if (action == QLatin1String("io.element.join")) {
        // Element Call reports that it has joined the call. The widget driver
        // rejects the action as an unknown variant, which surfaces in the page
        // as a "Failed to send join action" error, so ack it here. The call
        // surface is already up by this point (we started the session), so there
        // is nothing further to do with it.
        reply(QJsonObject{});
        komai::logging::ui()->warn("[EC] widget joined the call for session {}", sessionId_);
        return true;
    }
    if (action == QLatin1String("set_always_on_screen")) {
        // Picture-in-picture stickiness. We acknowledge success; honouring it
        // (a floating call overlay) is a later UX milestone.
        reply(QJsonObject{{QStringLiteral("success"), true}});
        return true;
    }
    if (action == QLatin1String("io.element.device_mute")) {
        // Element Call reports its current mic/camera state to the host (on join
        // and whenever it changes, including after a toggle we requested). Mirror
        // it onto our properties so the native controls reflect reality, then ack
        // by echoing the data back (Element Call ignores the reply for its own
        // outgoing report; only the toWidget direction drives its state).
        const QJsonObject data = obj.value(QStringLiteral("data")).toObject();
        const bool wasKnown    = deviceMuteStateKnown_;
        const bool prevMic     = micEnabled_;
        const bool prevCamera  = cameraEnabled_;
        if (data.contains(QStringLiteral("audio_enabled")))
            micEnabled_ = data.value(QStringLiteral("audio_enabled")).toBool();
        if (data.contains(QStringLiteral("video_enabled")))
            cameraEnabled_ = data.value(QStringLiteral("video_enabled")).toBool();
        deviceMuteStateKnown_ = true;
        if (!wasKnown || micEnabled_ != prevMic || cameraEnabled_ != prevCamera)
            emit deviceMuteStateChanged();
        reply(obj.value(QStringLiteral("data")));
        return true;
    }

    return false;
}

void
ElementCallWidgetSession::sendToWidget(const QJsonObject &message)
{
    emit messageToWidget(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void
ElementCallWidgetSession::sendToWidgetRequest(const QString &action, const QJsonObject &data)
{
    // Element Call learns its own widget id from the URL; we learn it from the
    // first message it sends. Fall back to the id we asked the driver to use
    // (see runtime_element_call.rs) if we somehow haven't seen a message yet.
    const QString widgetId =
      widgetId_.isEmpty() ? QStringLiteral("komai-ec-%1").arg(sessionId_) : widgetId_;

    const QString requestId = QStringLiteral("komai-%1-%2").arg(sessionId_).arg(++requestCounter_);
    pendingHostRequests_.insert(requestId);
    sendToWidget(QJsonObject{
      {QStringLiteral("api"), QStringLiteral("toWidget")},
      {QStringLiteral("widgetId"), widgetId},
      {QStringLiteral("requestId"), requestId},
      {QStringLiteral("action"), action},
      {QStringLiteral("data"), data},
    });
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

    // Forget the mirrored device state so a fresh session starts with the native
    // controls hidden until Element Call reports its mute state again.
    if (deviceMuteStateKnown_) {
        deviceMuteStateKnown_ = false;
        micEnabled_           = true;
        cameraEnabled_        = true;
        emit deviceMuteStateChanged();
    }
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

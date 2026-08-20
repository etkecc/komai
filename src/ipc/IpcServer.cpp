// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "IpcServer.h"

#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QPointer>
#include <QTimer>

#include "SharedLogic.h"
#include "profile/ProfileId.h"

namespace komai::ipc {

IpcServer::IpcServer(const QString &profileId, QObject *parent)
  : QObject{parent}
  , m_socketName{socketName(profileId)}
{
}

IpcServer::~IpcServer() = default;

QString
IpcServer::socketName(const QString &profileId)
{
    return QStringLiteral("komai-cli-") + profile_id::normalized(profileId);
}

bool
IpcServer::start()
{
    m_server = new QLocalServer{this};

    // Remove any stale socket left by a previous crash.
    QLocalServer::removeServer(m_socketName);

    connect(m_server, &QLocalServer::newConnection, this, &IpcServer::onNewConnection);

    if (!m_server->listen(m_socketName))
        return false;

    return true;
}

void
IpcServer::onNewConnection()
{
    while (auto *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            if (!socket->canReadLine())
                return; // wait for the full line
            handleRequest(socket);
        });
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);

        // Guard against idle connections that never send a request.
        QTimer::singleShot(30000, socket, [socket]() {
            if (socket->state() == QLocalSocket::ConnectedState)
                socket->disconnectFromServer();
        });
    }
}

// ---------------------------------------------------------------------------
// Request dispatch
// ---------------------------------------------------------------------------

static void
writeResponse(QLocalSocket *socket, const QJsonObject &response)
{
    socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + "\n");
    socket->flush();
    socket->disconnectFromServer();
}

/// Writes a response from an async callback, which may fire on a background
/// thread while the socket belongs to the thread that accepted it.
static void
writeResponseFromCallback(const QPointer<QLocalSocket> &socket, const QJsonObject &response)
{
    if (!socket)
        return;

    QMetaObject::invokeMethod(
      socket.data(),
      [socket, response]() {
          if (socket)
              writeResponse(socket.data(), response);
      },
      Qt::QueuedConnection);
}

/// Builds the response body for an async call that either failed with `error`
/// or produced `result`.
static QJsonObject
resultOrErrorResponse(const QJsonValue &result, const QString &error)
{
    if (!error.isEmpty())
        return {{QStringLiteral("error"), error}};

    return {{QStringLiteral("result"), result}};
}

/// Relays an async send result, which every message-sending method shares.
static SendMessageCallback
sendResultResponder(const QPointer<QLocalSocket> &socket)
{
    return [socket](const QString &eventId, const QString &error) {
        writeResponseFromCallback(
          socket, resultOrErrorResponse(QJsonObject{{QStringLiteral("eventId"), eventId}}, error));
    };
}

/// Relays an async room action that carries no payload beyond success.
static RoomActionCallback
roomActionResponder(const QPointer<QLocalSocket> &socket)
{
    return [socket](const QString &error) {
        writeResponseFromCallback(socket, resultOrErrorResponse(true, error));
    };
}

static bool
requireNonEmptyString(QLocalSocket *socket,
                      const QJsonObject &params,
                      const QString &key,
                      QString *value)
{
    const auto normalized = params.value(key).toString().trimmed();
    if (normalized.isEmpty()) {
        writeResponse(
          socket,
          {{QStringLiteral("error"),
            QStringLiteral("Argument '") + key + QStringLiteral("' must not be empty.")}});
        return false;
    }

    *value = normalized;
    return true;
}

static bool
optionalStringParam(QLocalSocket *socket,
                    const QJsonObject &params,
                    const QString &key,
                    QString *value)
{
    if (!params.contains(key)) {
        value->clear();
        return true;
    }

    if (!params.value(key).isString()) {
        writeResponse(
          socket,
          {{QStringLiteral("error"),
            QStringLiteral("Argument '") + key + QStringLiteral("' must be a string.")}});
        return false;
    }

    *value = params.value(key).toString();
    return true;
}

static bool
optionalBoolParam(QLocalSocket *socket, const QJsonObject &params, const QString &key, bool *value)
{
    if (!params.contains(key)) {
        *value = false;
        return true;
    }

    if (!params.value(key).isBool()) {
        writeResponse(
          socket,
          {{QStringLiteral("error"),
            QStringLiteral("Argument '") + key + QStringLiteral("' must be a boolean.")}});
        return false;
    }

    *value = params.value(key).toBool();
    return true;
}

static bool
tristateBoolParam(QLocalSocket *socket,
                  const QJsonObject &params,
                  const QString &key,
                  std::optional<bool> *value)
{
    if (!params.contains(key)) {
        value->reset();
        return true;
    }

    if (!params.value(key).isBool()) {
        writeResponse(
          socket,
          {{QStringLiteral("error"),
            QStringLiteral("Argument '") + key + QStringLiteral("' must be a boolean.")}});
        return false;
    }

    *value = params.value(key).toBool();
    return true;
}

static bool
optionalIntParam(QLocalSocket *socket,
                 const QJsonObject &params,
                 const QString &key,
                 int defaultValue,
                 int *value)
{
    if (!params.contains(key)) {
        *value = defaultValue;
        return true;
    }

    if (!params.value(key).isDouble()) {
        writeResponse(
          socket,
          {{QStringLiteral("error"),
            QStringLiteral("Argument '") + key + QStringLiteral("' must be an integer.")}});
        return false;
    }

    *value = params.value(key).toInt();
    return true;
}

static bool
optionalObjectParam(QLocalSocket *socket,
                    const QJsonObject &params,
                    const QString &key,
                    QJsonObject *value)
{
    if (!params.contains(key)) {
        *value = {};
        return true;
    }

    if (!params.value(key).isObject()) {
        writeResponse(
          socket,
          {{QStringLiteral("error"),
            QStringLiteral("Argument '") + key + QStringLiteral("' must be an object.")}});
        return false;
    }

    *value = params.value(key).toObject();
    return true;
}

static bool
optionalArrayParam(QLocalSocket *socket,
                   const QJsonObject &params,
                   const QString &key,
                   QJsonArray *value)
{
    if (!params.contains(key)) {
        *value = {};
        return true;
    }

    if (!params.value(key).isArray()) {
        writeResponse(
          socket,
          {{QStringLiteral("error"),
            QStringLiteral("Argument '") + key + QStringLiteral("' must be an array.")}});
        return false;
    }

    *value = params.value(key).toArray();
    return true;
}

static bool
optionalStringListParam(QLocalSocket *socket,
                        const QJsonObject &params,
                        const QString &key,
                        QStringList *value)
{
    QJsonArray array;
    if (!optionalArrayParam(socket, params, key, &array))
        return false;

    value->clear();
    for (const auto entry : array) {
        if (!entry.isString()) {
            writeResponse(socket,
                          {{QStringLiteral("error"),
                            QStringLiteral("Argument '") + key +
                              QStringLiteral("' must contain only strings.")}});
            return false;
        }
        value->append(entry.toString());
    }

    return true;
}

void
IpcServer::handleRequest(QLocalSocket *socket)
{
    const auto line = socket->readLine().trimmed();
    const auto doc  = QJsonDocument::fromJson(line);
    if (!doc.isObject()) {
        writeResponse(socket, {{QStringLiteral("error"), QStringLiteral("invalid request")}});
        return;
    }

    const auto request = doc.object();
    const auto method  = request.value(QStringLiteral("method")).toString();
    const auto params  = request.value(QStringLiteral("params")).toObject();

    // -- app --

    if (method == QLatin1String("app.version")) {
        writeResponse(socket, {{QStringLiteral("result"), appVersion()}});
        return;
    }
    if (method == QLatin1String("app.apiVersion")) {
        writeResponse(socket, {{QStringLiteral("result"), apiVersion()}});
        return;
    }

    // -- rooms --

    if (method == QLatin1String("rooms.list")) {
        RoomListQuery query;

        if (!optionalStringListParam(socket, params, QStringLiteral("ids"), &query.ids) ||
            !optionalStringListParam(socket, params, QStringLiteral("fields"), &query.fields)) {
            return;
        }

        if (!optionalStringParam(socket, params, QStringLiteral("query"), &query.query) ||
            !optionalStringParam(
              socket, params, QStringLiteral("parentSpace"), &query.parentSpace) ||
            !optionalStringParam(socket, params, QStringLiteral("tag"), &query.tag)) {
            return;
        }

        if (!tristateBoolParam(socket, params, QStringLiteral("isDm"), &query.isDm) ||
            !tristateBoolParam(socket, params, QStringLiteral("encrypted"), &query.encrypted)) {
            return;
        }

        int minMemberCount = -1;
        if (!optionalIntParam(
              socket, params, QStringLiteral("minMemberCount"), -1, &minMemberCount)) {
            return;
        }
        if (minMemberCount >= 0)
            query.minMemberCount = minMemberCount;

        if (!optionalIntParam(socket, params, QStringLiteral("limit"), -1, &query.limit) ||
            !optionalIntParam(socket, params, QStringLiteral("offset"), 0, &query.offset)) {
            return;
        }

        const auto result = roomList(query);
        if (const auto *error = std::get_if<QString>(&result)) {
            writeResponse(socket, {{QStringLiteral("error"), *error}});
            return;
        }

        const auto &page = std::get<RoomListPage>(result);
        QJsonArray arr;
        for (const auto &room : page.rooms)
            arr.append(room.toJson(query.fields));

        writeResponse(socket,
                      {{QStringLiteral("result"),
                        QJsonObject{{QStringLiteral("rooms"), arr},
                                    {QStringLiteral("matchCount"), page.matchCount}}}});
        return;
    }
    if (method == QLatin1String("rooms.join")) {
        QString roomIdOrAlias;
        if (!requireNonEmptyString(
              socket, params, QStringLiteral("roomIdOrAlias"), &roomIdOrAlias)) {
            return;
        }
        joinRoom(roomIdOrAlias);
        writeResponse(socket, {{QStringLiteral("result"), true}});
        return;
    }

    // -- rooms (membership) --
    //
    // The four user-targeting operations share a parameter shape, so they
    // share a dispatch block; only the SharedLogic call differs.
    if (method == QLatin1String("rooms.invite") || method == QLatin1String("rooms.kick") ||
        method == QLatin1String("rooms.ban") || method == QLatin1String("rooms.unban")) {
        QString roomIdOrAlias;
        if (!requireNonEmptyString(
              socket, params, QStringLiteral("roomIdOrAlias"), &roomIdOrAlias)) {
            return;
        }

        QString userId;
        if (!requireNonEmptyString(socket, params, QStringLiteral("userId"), &userId))
            return;

        QString reason;
        if (!optionalStringParam(socket, params, QStringLiteral("reason"), &reason))
            return;

        QPointer<QLocalSocket> safeSocket = socket;
        auto responder                    = roomActionResponder(safeSocket);
        if (method == QLatin1String("rooms.invite"))
            inviteUser(roomIdOrAlias, userId, reason, std::move(responder));
        else if (method == QLatin1String("rooms.kick"))
            kickUser(roomIdOrAlias, userId, reason, std::move(responder));
        else if (method == QLatin1String("rooms.ban"))
            banUser(roomIdOrAlias, userId, reason, std::move(responder));
        else
            unbanUser(roomIdOrAlias, userId, reason, std::move(responder));
        return;
    }

    if (method == QLatin1String("rooms.leave")) {
        QString roomIdOrAlias;
        if (!requireNonEmptyString(
              socket, params, QStringLiteral("roomIdOrAlias"), &roomIdOrAlias)) {
            return;
        }

        QString reason;
        if (!optionalStringParam(socket, params, QStringLiteral("reason"), &reason))
            return;

        QPointer<QLocalSocket> safeSocket = socket;
        leaveRoom(roomIdOrAlias, reason, roomActionResponder(safeSocket));
        return;
    }

    if (method == QLatin1String("rooms.create")) {
        CreateRoomRequest request;

        if (!optionalStringParam(socket, params, QStringLiteral("name"), &request.name) ||
            !optionalStringParam(socket, params, QStringLiteral("topic"), &request.topic) ||
            !optionalStringParam(
              socket, params, QStringLiteral("aliasLocalpart"), &request.aliasLocalpart) ||
            !optionalStringParam(
              socket, params, QStringLiteral("roomVersion"), &request.roomVersion) ||
            !optionalStringParam(socket, params, QStringLiteral("preset"), &request.preset)) {
            return;
        }

        if (!optionalStringListParam(
              socket, params, QStringLiteral("invite"), &request.inviteUserIds)) {
            return;
        }

        if (!optionalBoolParam(socket, params, QStringLiteral("isDirect"), &request.isDirect) ||
            !optionalBoolParam(
              socket, params, QStringLiteral("isEncrypted"), &request.isEncrypted) ||
            !optionalBoolParam(socket, params, QStringLiteral("isSpace"), &request.isSpace) ||
            !optionalBoolParam(socket, params, QStringLiteral("isPublic"), &request.isPublic)) {
            return;
        }

        if (!optionalObjectParam(socket,
                                 params,
                                 QStringLiteral("powerLevelContentOverride"),
                                 &request.powerLevelContentOverride) ||
            !optionalObjectParam(
              socket, params, QStringLiteral("creationContent"), &request.creationContent) ||
            !optionalArrayParam(
              socket, params, QStringLiteral("initialState"), &request.initialState)) {
            return;
        }

        QPointer<QLocalSocket> safeSocket = socket;
        createRoom(request, [safeSocket](const QString &roomId, const QString &error) {
            writeResponseFromCallback(
              safeSocket,
              resultOrErrorResponse(QJsonObject{{QStringLiteral("roomId"), roomId}}, error));
        });
        return;
    }

    if (method == QLatin1String("rooms.newDirectChat")) {
        QString userId;
        if (!requireNonEmptyString(socket, params, QStringLiteral("userId"), &userId)) {
            return;
        }
        newDirectChat(userId);
        writeResponse(socket, {{QStringLiteral("result"), true}});
        return;
    }

    if (method == QLatin1String("rooms.timeline")) {
        QString roomIdOrAlias;
        if (!requireNonEmptyString(
              socket, params, QStringLiteral("roomIdOrAlias"), &roomIdOrAlias)) {
            return;
        }

        int limit = 50;
        if (!optionalIntParam(socket, params, QStringLiteral("limit"), 50, &limit))
            return;

        QString beforeEventId;
        if (!optionalStringParam(socket, params, QStringLiteral("beforeEventId"), &beforeEventId))
            return;

        bool includeUnsignedFields = false;
        if (!optionalBoolParam(
              socket, params, QStringLiteral("includeUnsignedFields"), &includeUnsignedFields)) {
            return;
        }

        QString fetchMode = QStringLiteral("cached_only");
        if (!optionalStringParam(socket, params, QStringLiteral("fetchMode"), &fetchMode))
            return;
        if (fetchMode.trimmed().isEmpty())
            fetchMode = QStringLiteral("cached_only");

        QPointer<QLocalSocket> safeSocket = socket;
        readTimeline(roomIdOrAlias,
                     limit,
                     beforeEventId,
                     includeUnsignedFields,
                     fetchMode,
                     [safeSocket](const QJsonObject &result, const QString &error) {
                         writeResponseFromCallback(safeSocket,
                                                   resultOrErrorResponse(result, error));
                     });
        return;
    }

    // -- user --

    if (method == QLatin1String("user.userId")) {
        writeResponse(socket, {{QStringLiteral("result"), userId()}});
        return;
    }
    if (method == QLatin1String("user.homeserverUrl")) {
        writeResponse(socket, {{QStringLiteral("result"), homeserverUrl()}});
        return;
    }
    if (method == QLatin1String("user.deviceId")) {
        writeResponse(socket, {{QStringLiteral("result"), deviceId()}});
        return;
    }
    if (method == QLatin1String("user.statusMessage")) {
        writeResponse(socket, {{QStringLiteral("result"), statusMessage()}});
        return;
    }
    if (method == QLatin1String("user.setStatusMessage")) {
        setStatusMessage(params.value(QStringLiteral("message")).toString());
        writeResponse(socket, {{QStringLiteral("result"), true}});
        return;
    }

    // -- settings.ui --

    if (method == QLatin1String("settings.ui.theme")) {
        writeResponse(socket, {{QStringLiteral("result"), uiTheme()}});
        return;
    }
    if (method == QLatin1String("settings.ui.setTheme")) {
        QString theme;
        if (!requireNonEmptyString(socket, params, QStringLiteral("theme"), &theme))
            return;

        setUiTheme(theme);
        if (uiTheme() != theme) {
            writeResponse(
              socket, {{QStringLiteral("error"), QStringLiteral("invalid theme slug: ") + theme}});
            return;
        }
        writeResponse(socket, {{QStringLiteral("result"), true}});
        return;
    }

    if (method == QLatin1String("rooms.send")) {
        QPointer<QLocalSocket> safeSocket = socket;
        sendMessage(params.value(QStringLiteral("roomIdOrAlias")).toString(),
                    params.value(QStringLiteral("body")).toString(),
                    params.value(QStringLiteral("msgtype")).toString(QStringLiteral("m.text")),
                    params.value(QStringLiteral("format")).toString(QStringLiteral("auto")),
                    sendResultResponder(safeSocket));
        return;
    }

    if (method == QLatin1String("rooms.sendImageFile")) {
        QPointer<QLocalSocket> safeSocket = socket;
        sendImageFromFile(params.value(QStringLiteral("roomIdOrAlias")).toString(),
                          params.value(QStringLiteral("path")).toString(),
                          params.value(QStringLiteral("body")).toString(),
                          sendResultResponder(safeSocket));
        return;
    }

    if (method == QLatin1String("rooms.sendImage")) {
        QPointer<QLocalSocket> safeSocket = socket;
        sendImage(params.value(QStringLiteral("roomIdOrAlias")).toString(),
                  params.value(QStringLiteral("mxcUri")).toString(),
                  params.value(QStringLiteral("body")).toString(),
                  params.value(QStringLiteral("filename")).toString(),
                  params.value(QStringLiteral("info")).toObject(),
                  sendResultResponder(safeSocket));
        return;
    }

    // -- media (async) --

    if (method == QLatin1String("media.fetch")) {
        QPointer<QLocalSocket> safeSocket = socket;
        QString mxcUri;
        if (!requireNonEmptyString(socket, params, QStringLiteral("mxcUri"), &mxcUri))
            return;
        mediaFetch(mxcUri, [safeSocket](const QImage &image) {
            // Build the response payload (safe on any thread).
            QJsonObject response;
            if (image.isNull()) {
                response.insert(QStringLiteral("error"), QStringLiteral("failed to fetch image"));
            } else {
                QByteArray pngData;
                QBuffer buffer(&pngData);
                buffer.open(QIODevice::WriteOnly);
                image.save(&buffer, "PNG");
                response.insert(QStringLiteral("result"), QString::fromLatin1(pngData.toBase64()));
            }

            writeResponseFromCallback(safeSocket, response);
        });
        return;
    }

    if (method == QLatin1String("media.upload")) {
        QPointer<QLocalSocket> safeSocket = socket;
        QString path;
        if (!requireNonEmptyString(socket, params, QStringLiteral("path"), &path))
            return;
        mediaUpload(path,
                    params.value(QStringLiteral("filename")).toString(),
                    params.value(QStringLiteral("contentType")).toString(),
                    [safeSocket](const UploadResult &result, const QString &error) {
                        writeResponseFromCallback(safeSocket,
                                                  resultOrErrorResponse(result.toJson(), error));
                    });
        return;
    }

    writeResponse(socket, {{QStringLiteral("error"), QStringLiteral("unknown method: ") + method}});
}

} // namespace komai::ipc

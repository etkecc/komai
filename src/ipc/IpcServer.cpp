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
        const auto rooms = roomList();
        QJsonArray arr;
        for (const auto &room : rooms)
            arr.append(room.toJson());
        writeResponse(socket, {{QStringLiteral("result"), arr}});
        return;
    }
    if (method == QLatin1String("rooms.activate")) {
        activateRoom(params.value(QStringLiteral("roomIdOrAlias")).toString());
        writeResponse(socket, {{QStringLiteral("result"), true}});
        return;
    }
    if (method == QLatin1String("rooms.join")) {
        joinRoom(params.value(QStringLiteral("roomIdOrAlias")).toString());
        writeResponse(socket, {{QStringLiteral("result"), true}});
        return;
    }
    if (method == QLatin1String("rooms.newDirectChat")) {
        newDirectChat(params.value(QStringLiteral("userId")).toString());
        writeResponse(socket, {{QStringLiteral("result"), true}});
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
        setUiTheme(params.value(QStringLiteral("theme")).toString());
        writeResponse(socket, {{QStringLiteral("result"), true}});
        return;
    }

    if (method == QLatin1String("rooms.send")) {
        QPointer<QLocalSocket> safeSocket = socket;
        sendMessage(params.value(QStringLiteral("roomIdOrAlias")).toString(),
                    params.value(QStringLiteral("body")).toString(),
                    params.value(QStringLiteral("msgtype")).toString(QStringLiteral("m.text")),
                    params.value(QStringLiteral("format")).toString(QStringLiteral("auto")),
                    [safeSocket](const QString &eventId, const QString &error) {
                        if (!safeSocket)
                            return;
                        QJsonObject response;
                        if (!error.isEmpty())
                            response.insert(QStringLiteral("error"), error);
                        else
                            response.insert(QStringLiteral("result"),
                                            QJsonObject{{QStringLiteral("eventId"), eventId}});
                        QMetaObject::invokeMethod(
                          safeSocket.data(),
                          [safeSocket, response]() {
                              if (safeSocket)
                                  writeResponse(safeSocket.data(), response);
                          },
                          Qt::QueuedConnection);
                    });
        return;
    }

    // -- media (async) --

    if (method == QLatin1String("media.fetch")) {
        QPointer<QLocalSocket> safeSocket = socket;
        mediaFetch(
          params.value(QStringLiteral("mxcUri")).toString(), [safeSocket](const QImage &image) {
              if (!safeSocket)
                  return;

              // Build the response payload (safe on any thread).
              QJsonObject response;
              if (image.isNull()) {
                  response.insert(QStringLiteral("error"), QStringLiteral("failed to fetch image"));
              } else {
                  QByteArray pngData;
                  QBuffer buffer(&pngData);
                  buffer.open(QIODevice::WriteOnly);
                  image.save(&buffer, "PNG");
                  response.insert(QStringLiteral("result"),
                                  QString::fromLatin1(pngData.toBase64()));
              }

              // The callback may fire on a background thread, but the socket
              // must be written to on the thread that owns it (main thread).
              QMetaObject::invokeMethod(
                safeSocket.data(),
                [safeSocket, response]() {
                    if (safeSocket)
                        writeResponse(safeSocket.data(), response);
                },
                Qt::QueuedConnection);
          });
        return;
    }

    if (method == QLatin1String("media.upload")) {
        QPointer<QLocalSocket> safeSocket = socket;
        mediaUpload(params.value(QStringLiteral("path")).toString(),
                    params.value(QStringLiteral("filename")).toString(),
                    params.value(QStringLiteral("contentType")).toString(),
                    [safeSocket](const UploadResult &result, const QString &error) {
                        if (!safeSocket)
                            return;
                        QJsonObject response;
                        if (!error.isEmpty())
                            response.insert(QStringLiteral("error"), error);
                        else
                            response.insert(QStringLiteral("result"), result.toJson());
                        QMetaObject::invokeMethod(
                          safeSocket.data(),
                          [safeSocket, response]() {
                              if (safeSocket)
                                  writeResponse(safeSocket.data(), response);
                          },
                          Qt::QueuedConnection);
                    });
        return;
    }

    writeResponse(socket, {{QStringLiteral("error"), QStringLiteral("unknown method: ") + method}});
}

} // namespace komai::ipc

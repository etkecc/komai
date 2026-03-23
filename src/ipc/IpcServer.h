// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>

class QLocalSocket;

namespace komai::ipc {

/// QLocalServer-based IPC server for CLI commands.
/// Listens on a per-profile Unix socket, receives JSON-line requests,
/// dispatches to shared business logic, and writes JSON-line responses.
/// Started unconditionally at app startup (does not depend on D-Bus settings).
class IpcServer final : public QObject
{
    Q_OBJECT

public:
    explicit IpcServer(const QString &profileId, QObject *parent = nullptr);
    ~IpcServer() override;

    bool start();

    /// Socket name derived from the profile ID (e.g. "komai-cli-default").
    static QString socketName(const QString &profileId);

private slots:
    void onNewConnection();

private:
    void handleRequest(QLocalSocket *socket);

    QLocalServer *m_server = nullptr;
    QString m_socketName;
};

} // namespace komai::ipc

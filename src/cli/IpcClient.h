// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <iostream>

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QString>
#include <QStringList>

#include "app/MainApplicationSupport.h"
#include "profile/ProfileId.h"

namespace cli_ipc {

/// Socket name derived from the profile ID (matches IpcServer::socketName).
inline QString
socketName(const QString &profileId)
{
    return QStringLiteral("komai-cli-") + profile_id::normalized(profileId);
}

/// Returns the profile ID from the -p / --profile flag, or empty for default.
inline QString
profileFromArgs(int argc, char *argv[])
{
    return app::support::selectedProfileFromArgs(argc, argv).value;
}

/// Checks that a running Komai instance is listening on the IPC socket for the
/// given profile.  Returns true on success; prints an error to stderr and
/// returns false otherwise.
inline bool
ensureConnected(const QString &profileId)
{
    QLocalSocket socket;
    socket.connectToServer(socketName(profileId));
    if (!socket.waitForConnected(1000)) {
        auto display = profileId.isEmpty() ? QStringLiteral("default") : profileId;
        std::cerr << "Error: no running Komai instance for profile '" << display.toStdString()
                  << "'\n"
                  << "Start Komai first: komai";
        if (!profileId.isEmpty() && profileId != QLatin1String("default"))
            std::cerr << " -p " << profileId.toStdString();
        std::cerr << "\n";
        return false;
    }
    socket.disconnectFromServer();
    return true;
}

/// Send a JSON-line request to the running instance and return the parsed
/// response object.  On transport failure the response contains an "error" key.
inline QJsonObject
call(const QString &profileId, const QString &method, const QJsonObject &params = {})
{
    QLocalSocket socket;
    socket.connectToServer(socketName(profileId));
    if (!socket.waitForConnected(5000))
        return {{QStringLiteral("error"), QStringLiteral("connection failed")}};

    QJsonObject request{{QStringLiteral("method"), method}};
    if (!params.isEmpty())
        request.insert(QStringLiteral("params"), params);

    const auto data = QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n";
    socket.write(data);
    if (!socket.waitForBytesWritten(5000))
        return {{QStringLiteral("error"), QStringLiteral("write failed")}};

    // Read until we get a complete JSON line.
    QByteArray response;
    while (!response.contains('\n')) {
        if (!socket.waitForReadyRead(30000))
            return {{QStringLiteral("error"), QStringLiteral("read timeout")}};
        response.append(socket.readAll());
    }

    socket.disconnectFromServer();

    const auto doc = QJsonDocument::fromJson(response.trimmed());
    if (!doc.isObject())
        return {{QStringLiteral("error"), QStringLiteral("invalid response")}};

    return doc.object();
}

/// Returns the value of a --flag from argv, or defaultValue if absent.
inline QString
flagValue(int argc, char *argv[], const QString &flag, const QString &defaultValue = {})
{
    for (int i = 1; i < argc - 1; ++i) {
        if (QString{argv[i]} == flag)
            return QString{argv[i + 1]};
    }
    return defaultValue;
}

/// Returns true if a boolean flag (no value) appears anywhere in argv.
inline bool
hasFlag(int argc, char *argv[], const QString &flag)
{
    for (int i = 1; i < argc; ++i) {
        if (QString{argv[i]} == flag)
            return true;
    }
    return false;
}

/// Collects positional arguments after the given keyword in argv,
/// skipping known option+value pairs and flags.
inline QStringList
positionalsAfter(int argc, char *argv[], const QString &keyword)
{
    QStringList result;
    bool pastKeyword = false;
    for (int i = 1; i < argc; ++i) {
        QString arg{argv[i]};

        // Skip known option+value pairs
        if (arg == QLatin1String("-p") || arg == QLatin1String("--profile") ||
            arg == QLatin1String("-l") || arg == QLatin1String("--log-level") ||
            arg == QLatin1String("-L") || arg == QLatin1String("--log-type") ||
            arg == QLatin1String("--msgtype") || arg == QLatin1String("--format") ||
            arg == QLatin1String("--caption") || arg == QLatin1String("--filename") ||
            arg == QLatin1String("--content-type")) {
            ++i;
            continue;
        }
        // Skip flags (including --profile=value, -pvalue, etc.)
        if (arg.startsWith(QLatin1Char('-')))
            continue;

        if (!pastKeyword) {
            if (arg == keyword)
                pastKeyword = true;
            continue;
        }

        result.append(arg);
    }
    return result;
}

/// Returns true if --help or -h appears anywhere in argv.
inline bool
hasHelpFlag(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        QString arg{argv[i]};
        if (arg == QLatin1String("--help") || arg == QLatin1String("-h"))
            return true;
    }
    return false;
}

} // namespace cli_ipc

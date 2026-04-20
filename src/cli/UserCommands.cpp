// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UserCommands.h"

#include <iostream>

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

#include "IpcClient.h"
#include "schema/Dispatcher.h"
#include "schema/SchemaTypes.h"

namespace {

int
emitPlainIpcString(const QString &profileId,
                   const QString &method,
                   const QString &resultKey,
                   const char *failLabel,
                   bool allowEmpty = false)
{
    auto response = cli_ipc::call(profileId, method);
    auto result   = response.value(QStringLiteral("result")).toString();
    if (result.isEmpty() && !allowEmpty) {
        std::cerr << "Error: failed to get " << failLabel << "\n";
        return 1;
    }
    std::cout << QJsonDocument(QJsonObject{{resultKey, result}})
                   .toJson(QJsonDocument::Compact)
                   .toStdString()
              << "\n";
    return 0;
}

int
handleId(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    return emitPlainIpcString(
      parsed.profileId, QStringLiteral("user.userId"), QStringLiteral("userId"), "user ID");
}

int
handleHomeserverUrl(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    return emitPlainIpcString(parsed.profileId,
                              QStringLiteral("user.homeserverUrl"),
                              QStringLiteral("homeserverUrl"),
                              "homeserver URL");
}

int
handleDeviceId(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    return emitPlainIpcString(
      parsed.profileId, QStringLiteral("user.deviceId"), QStringLiteral("deviceId"), "device ID");
}

int
handleStatus(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    return emitPlainIpcString(parsed.profileId,
                              QStringLiteral("user.statusMessage"),
                              QStringLiteral("statusMessage"),
                              "status message",
                              /*allowEmpty=*/true);
}

int
handleSetStatus(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    const auto message = parsed.positionals.join(QLatin1Char(' '));
    cli_ipc::call(parsed.profileId,
                  QStringLiteral("user.setStatusMessage"),
                  {{QStringLiteral("message"), message}});
    return 0;
}

cli_schema::GroupDef
userGroup()
{
    cli_schema::GroupDef group;
    group.name = QStringLiteral("user");
    group.help = QStringLiteral("Account and presence (JSON)");

    cli_schema::SubcommandDef id;
    id.name    = QStringLiteral("id");
    id.help    = QStringLiteral("Print the user's Matrix ID (JSON)");
    id.handler = handleId;
    group.subcommands.append(id);

    cli_schema::SubcommandDef homeserver;
    homeserver.name    = QStringLiteral("homeserver-url");
    homeserver.help    = QStringLiteral("Print the homeserver URL (JSON)");
    homeserver.handler = handleHomeserverUrl;
    group.subcommands.append(homeserver);

    cli_schema::SubcommandDef device;
    device.name    = QStringLiteral("device-id");
    device.help    = QStringLiteral("Print the device ID (JSON)");
    device.handler = handleDeviceId;
    group.subcommands.append(device);

    cli_schema::SubcommandDef status;
    status.name    = QStringLiteral("status");
    status.help    = QStringLiteral("Print the user's status message (JSON)");
    status.handler = handleStatus;
    group.subcommands.append(status);

    cli_schema::SubcommandDef setStatus;
    setStatus.name = QStringLiteral("set-status");
    setStatus.help = QStringLiteral("Set the user's status message");
    cli_schema::PositionalDef message;
    message.name     = QStringLiteral("message");
    message.help     = QStringLiteral("Status text; multiple words are joined with spaces.");
    message.variadic = true;
    setStatus.positionals.append(message);
    setStatus.handler = handleSetStatus;
    group.subcommands.append(setStatus);

    return group;
}

} // namespace

int
runUserCommand(int argc, char *argv[], QCoreApplication &app)
{
    return cli_schema::dispatchGroup(userGroup(), argc, argv, app);
}

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppCommands.h"

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
                   const char *failLabel)
{
    auto response = cli_ipc::call(profileId, method);
    auto result   = response.value(QStringLiteral("result")).toString();
    if (result.isEmpty()) {
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
handleVersion(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    return emitPlainIpcString(
      parsed.profileId, QStringLiteral("app.version"), QStringLiteral("version"), "app version");
}

int
handleApiVersion(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    return emitPlainIpcString(parsed.profileId,
                              QStringLiteral("app.apiVersion"),
                              QStringLiteral("apiVersion"),
                              "API version");
}

} // namespace

cli_schema::GroupDef
appGroupDef()
{
    cli_schema::GroupDef group;
    group.name = QStringLiteral("app");
    group.help = QStringLiteral("Instance metadata (JSON)");

    cli_schema::SubcommandDef version;
    version.name    = QStringLiteral("version");
    version.help    = QStringLiteral("Print the Komai version (JSON)");
    version.handler = handleVersion;
    group.subcommands.append(version);

    cli_schema::SubcommandDef apiVersion;
    apiVersion.name    = QStringLiteral("api-version");
    apiVersion.help    = QStringLiteral("Print the D-Bus API version (JSON)");
    apiVersion.handler = handleApiVersion;
    group.subcommands.append(apiVersion);

    return group;
}

int
runAppCommand(int argc, char *argv[], QCoreApplication &app)
{
    return cli_schema::dispatchGroup(appGroupDef(), argc, argv, app);
}

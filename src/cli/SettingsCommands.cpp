// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsCommands.h"

#include <iostream>

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

#include "IpcClient.h"
#include "schema/Dispatcher.h"
#include "schema/SchemaTypes.h"

namespace {

bool
handleIpcError(const QJsonObject &response)
{
    if (!response.contains(QStringLiteral("error")))
        return false;

    std::cerr << "Error: " << response.value(QStringLiteral("error")).toString().toStdString()
              << "\n";
    return true;
}

int
handleUiTheme(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    auto response = cli_ipc::call(parsed.profileId, QStringLiteral("settings.ui.theme"));
    if (handleIpcError(response))
        return 1;
    auto result = response.value(QStringLiteral("result")).toString();
    if (result.isEmpty()) {
        std::cerr << "Error: failed to get current theme\n";
        return 1;
    }
    std::cout << QJsonDocument(QJsonObject{{QStringLiteral("theme"), result}})
                   .toJson(QJsonDocument::Compact)
                   .toStdString()
              << "\n";
    return 0;
}

int
handleUiSetTheme(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    const auto theme = parsed.positionals.value(0).trimmed();
    if (theme.isEmpty()) {
        std::cerr << "Error: theme slug must not be empty\n";
        return 1;
    }
    auto response = cli_ipc::call(
      parsed.profileId, QStringLiteral("settings.ui.setTheme"), {{QStringLiteral("theme"), theme}});
    if (handleIpcError(response))
        return 1;
    return 0;
}

cli_schema::GroupDef
settingsGroup()
{
    cli_schema::GroupDef group;
    group.name = QStringLiteral("settings");
    group.help = QStringLiteral("Application settings (JSON)");

    // `settings ui` — subgroup (no direct handler, only nested subcommands).
    cli_schema::SubcommandDef ui;
    ui.name            = QStringLiteral("ui");
    ui.help            = QStringLiteral("Appearance settings (theme)");
    ui.requiresProfile = false; // child subcommands manage this themselves

    cli_schema::SubcommandDef uiTheme;
    uiTheme.name    = QStringLiteral("theme");
    uiTheme.help    = QStringLiteral("Print the current theme slug (JSON)");
    uiTheme.handler = handleUiTheme;
    ui.subcommands.append(uiTheme);

    cli_schema::SubcommandDef uiSetTheme;
    uiSetTheme.name = QStringLiteral("set-theme");
    uiSetTheme.help = QStringLiteral("Set the active theme");
    cli_schema::PositionalDef slug;
    slug.name = QStringLiteral("slug");
    slug.help = QStringLiteral("Theme slug (as printed by `settings ui theme`).");
    uiSetTheme.positionals.append(slug);
    uiSetTheme.handler = handleUiSetTheme;
    ui.subcommands.append(uiSetTheme);

    group.subcommands.append(ui);

    return group;
}

} // namespace

int
runSettingsCommand(int argc, char *argv[], QCoreApplication &app)
{
    return cli_schema::dispatchGroup(settingsGroup(), argc, argv, app);
}

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsCommands.h"

#include <iostream>

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

#include "IpcClient.h"

static int
runSettingsUiCommand(int argc, char *argv[], const QStringList &args)
{
    // args[0] = "ui", args[1..] = subcommand + its arguments
    auto subcmd = args.size() < 2 ? QString{} : args.at(1);

    if (subcmd.isEmpty()) {
        std::cout << "Usage: komai [-p <profile>] settings ui <subcommand> [args...]\n\n"
                  << "Subcommands:\n"
                  << "  theme                  Print the current theme slug (JSON)\n"
                  << "  set-theme <slug>       Set the active theme\n";
        return cli_ipc::hasHelpFlag(argc, argv) ? 0 : 1;
    }

    auto profileId = cli_ipc::profileFromArgs(argc, argv);
    if (!cli_ipc::ensureConnected(profileId))
        return 1;

    if (subcmd == QLatin1String("theme")) {
        auto response = cli_ipc::call(profileId, QStringLiteral("settings.ui.theme"));
        auto result   = response.value(QStringLiteral("result")).toString();
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

    if (subcmd == QLatin1String("set-theme")) {
        if (args.size() < 3) {
            std::cerr << "Usage: komai settings ui set-theme <slug>\n";
            return 1;
        }
        cli_ipc::call(profileId,
                      QStringLiteral("settings.ui.setTheme"),
                      {{QStringLiteral("theme"), args.at(2)}});
        return 0;
    }

    std::cerr << "Unknown subcommand: " << subcmd.toStdString() << "\n"
              << "Run 'komai settings ui --help' for a list of subcommands.\n";
    return 1;
}

int
runSettingsCommand(int argc, char *argv[], QCoreApplication & /*app*/)
{
    auto args     = cli_ipc::positionalsAfter(argc, argv, QStringLiteral("settings"));
    auto subgroup = args.isEmpty() ? QString{} : args.first();

    if (subgroup.isEmpty()) {
        std::cout << "Usage: komai [-p <profile>] settings <group> <subcommand> [args...]\n\n"
                  << "Groups:\n"
                  << "  ui    Appearance settings (theme)\n";
        return cli_ipc::hasHelpFlag(argc, argv) ? 0 : 1;
    }

    if (subgroup == QLatin1String("ui"))
        return runSettingsUiCommand(argc, argv, args);

    std::cerr << "Unknown settings group: " << subgroup.toStdString() << "\n"
              << "Run 'komai settings --help' for a list of groups.\n";
    return 1;
}

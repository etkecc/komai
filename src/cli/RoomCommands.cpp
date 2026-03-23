// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomCommands.h"

#include <iostream>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "IpcClient.h"

int
runRoomsCommand(int argc, char *argv[], QCoreApplication & /*app*/)
{
    auto args   = cli_ipc::positionalsAfter(argc, argv, QStringLiteral("rooms"));
    auto subcmd = args.isEmpty() ? QString{} : args.first();

    if (subcmd.isEmpty()) {
        std::cout << "Usage: komai [-p <profile>] rooms <subcommand> [args...]\n\n"
                  << "Subcommands:\n"
                  << "  list                         List all joined rooms (JSON)\n"
                  << "  activate <room-id-or-alias>  Activate (focus) a room\n"
                  << "  join <room-id-or-alias>      Join a room\n"
                  << "  new-direct-chat <user-id>    Start or open a direct chat\n";
        return cli_ipc::hasHelpFlag(argc, argv) ? 0 : 1;
    }

    auto profileId = cli_ipc::profileFromArgs(argc, argv);
    if (!cli_ipc::ensureConnected(profileId))
        return 1;

    if (subcmd == QLatin1String("list")) {
        auto response = cli_ipc::call(profileId, QStringLiteral("rooms.list"));
        auto arr      = response.value(QStringLiteral("result")).toArray();
        std::cout << QJsonDocument(arr).toJson(QJsonDocument::Compact).toStdString() << "\n";
        return 0;
    }

    if (subcmd == QLatin1String("activate")) {
        if (args.size() < 2) {
            std::cerr << "Usage: komai rooms activate <room-id-or-alias>\n";
            return 1;
        }
        cli_ipc::call(profileId,
                      QStringLiteral("rooms.activate"),
                      {{QStringLiteral("roomIdOrAlias"), args.at(1)}});
        return 0;
    }

    if (subcmd == QLatin1String("join")) {
        if (args.size() < 2) {
            std::cerr << "Usage: komai rooms join <room-id-or-alias>\n";
            return 1;
        }
        cli_ipc::call(
          profileId, QStringLiteral("rooms.join"), {{QStringLiteral("roomIdOrAlias"), args.at(1)}});
        return 0;
    }

    if (subcmd == QLatin1String("new-direct-chat")) {
        if (args.size() < 2) {
            std::cerr << "Usage: komai rooms new-direct-chat <user-id>\n";
            return 1;
        }
        cli_ipc::call(profileId,
                      QStringLiteral("rooms.newDirectChat"),
                      {{QStringLiteral("userId"), args.at(1)}});
        return 0;
    }

    std::cerr << "Unknown subcommand: " << subcmd.toStdString() << "\n"
              << "Run 'komai rooms --help' for a list of subcommands.\n";
    return 1;
}

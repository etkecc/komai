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
        std::cout
          << "Usage: komai [-p <profile>] rooms <subcommand> [args...]\n\n"
          << "Subcommands:\n"
          << "  list                         List all joined rooms (JSON)\n"
          << "  activate <room-id-or-alias>  Activate (focus) a room\n"
          << "  join <room-id-or-alias>      Join a room\n"
          << "  new-direct-chat <user-id>    Start or open a direct chat\n"
          << "  send <room-id-or-alias> <message>  Send a message to a room\n"
          << "    --msgtype text|notice            Message type (default: text)\n"
          << "    --format  auto|plain|html        Markdown handling (default: auto)\n"
          << "  send-image <room> <path>           Upload and send an image (encryption-aware)\n"
          << "  send-image <room> <mxc-uri>       Send a pre-uploaded image (unencrypted only)\n"
          << "    --caption <text>                 Image caption\n"
          << "    --filename <name>                Filename (required with mxc:// URI)\n";
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

    if (subcmd == QLatin1String("send")) {
        if (args.size() < 3) {
            std::cerr << "Usage: komai rooms send <room-id-or-alias> <message> "
                         "[--msgtype text|notice] [--format auto|plain|html]\n";
            return 1;
        }

        // All positionals after the room ID are joined as the message body.
        QStringList bodyParts;
        for (int i = 2; i < args.size(); ++i)
            bodyParts.append(args.at(i));
        const auto body = bodyParts.join(QLatin1Char(' '));

        auto msgtypeFlag =
          cli_ipc::flagValue(argc, argv, QStringLiteral("--msgtype"), QStringLiteral("text"));
        auto msgtype = (msgtypeFlag == QLatin1String("notice")) ? QStringLiteral("m.notice")
                                                                : QStringLiteral("m.text");
        auto format =
          cli_ipc::flagValue(argc, argv, QStringLiteral("--format"), QStringLiteral("auto"));

        auto response = cli_ipc::call(profileId,
                                      QStringLiteral("rooms.send"),
                                      {{QStringLiteral("roomIdOrAlias"), args.at(1)},
                                       {QStringLiteral("body"), body},
                                       {QStringLiteral("msgtype"), msgtype},
                                       {QStringLiteral("format"), format}});
        if (response.contains(QStringLiteral("error"))) {
            std::cerr << "Error: "
                      << response.value(QStringLiteral("error")).toString().toStdString() << "\n";
            return 1;
        }
        auto result = response.value(QStringLiteral("result")).toObject();
        std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << "\n";
        return 0;
    }

    if (subcmd == QLatin1String("send-image")) {
        if (args.size() < 3) {
            std::cerr << "Usage: komai rooms send-image <room-id-or-alias> <path-or-mxc>\n"
                      << "       [--caption <text>] [--filename <name>]\n";
            return 1;
        }

        const auto roomArg   = args.at(1);
        const auto pathOrMxc = args.at(2);
        auto caption         = cli_ipc::flagValue(argc, argv, QStringLiteral("--caption"));
        auto filenameFlag    = cli_ipc::flagValue(argc, argv, QStringLiteral("--filename"));

        QJsonObject response;
        if (pathOrMxc.startsWith(QLatin1String("mxc://"))) {
            QJsonObject params{
              {QStringLiteral("roomIdOrAlias"), roomArg},
              {QStringLiteral("mxcUri"), pathOrMxc},
            };
            if (!caption.isEmpty())
                params.insert(QStringLiteral("body"), caption);
            if (!filenameFlag.isEmpty())
                params.insert(QStringLiteral("filename"), filenameFlag);
            response = cli_ipc::call(profileId, QStringLiteral("rooms.sendImage"), params);
        } else {
            QJsonObject params{
              {QStringLiteral("roomIdOrAlias"), roomArg},
              {QStringLiteral("path"), pathOrMxc},
            };
            if (!caption.isEmpty())
                params.insert(QStringLiteral("body"), caption);
            response = cli_ipc::call(profileId, QStringLiteral("rooms.sendImageFile"), params);
        }

        if (response.contains(QStringLiteral("error"))) {
            std::cerr << "Error: "
                      << response.value(QStringLiteral("error")).toString().toStdString() << "\n";
            return 1;
        }
        auto result = response.value(QStringLiteral("result")).toObject();
        std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << "\n";
        return 0;
    }

    std::cerr << "Unknown subcommand: " << subcmd.toStdString() << "\n"
              << "Run 'komai rooms --help' for a list of subcommands.\n";
    return 1;
}

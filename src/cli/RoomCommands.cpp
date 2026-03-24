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

namespace {

bool
requireNonEmptyValue(const QString &value, const char *label)
{
    if (!value.trimmed().isEmpty())
        return true;

    std::cerr << "Error: " << label << " must not be empty\n";
    return false;
}

bool
handleIpcError(const QJsonObject &response)
{
    if (!response.contains(QStringLiteral("error")))
        return false;

    std::cerr << "Error: " << response.value(QStringLiteral("error")).toString().toStdString()
              << "\n";
    return true;
}

bool
parseIntFlag(const QString &value, const char *label, int *parsed)
{
    if (value.isEmpty())
        return true;

    bool ok         = false;
    const int asInt = value.toInt(&ok);
    if (!ok) {
        std::cerr << "Error: " << label << " must be an integer\n";
        return false;
    }

    *parsed = asInt;
    return true;
}

} // namespace

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
          << "  timeline <room-id-or-alias> Read visible timeline events (JSON)\n"
          << "    --limit <n>                     Max events to return (default: 50, max: 500)\n"
          << "    --before-event-id <id>         Return events older than this event ID\n"
          << "    --include-unsigned-fields      Include Matrix unsigned event fields\n"
          << "    --fetch-mode cached_only|server_fetch_if_needed\n"
          << "                                   Fetch older history from the server when needed\n"
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

    if (subcmd == QLatin1String("timeline")) {
        if (args.size() < 2) {
            std::cerr << "Usage: komai rooms timeline <room-id-or-alias> [--limit <n>] "
                         "[--before-event-id <id>] [--include-unsigned-fields] "
                         "[--fetch-mode cached_only|server_fetch_if_needed]\n";
            return 1;
        }
        if (!requireNonEmptyValue(args.at(1), "room-id-or-alias"))
            return 1;

        QJsonObject params{{QStringLiteral("roomIdOrAlias"), args.at(1)}};

        int limit            = 50;
        const auto limitFlag = cli_ipc::flagValue(argc, argv, QStringLiteral("--limit"));
        if (!parseIntFlag(limitFlag, "--limit", &limit))
            return 1;
        if (!limitFlag.isEmpty())
            params.insert(QStringLiteral("limit"), limit);

        const auto beforeEventId =
          cli_ipc::flagValue(argc, argv, QStringLiteral("--before-event-id"));
        if (!beforeEventId.isEmpty())
            params.insert(QStringLiteral("beforeEventId"), beforeEventId);

        if (cli_ipc::hasFlag(argc, argv, QStringLiteral("--include-unsigned-fields")))
            params.insert(QStringLiteral("includeUnsignedFields"), true);

        const auto fetchMode = cli_ipc::flagValue(argc, argv, QStringLiteral("--fetch-mode"));
        if (!fetchMode.isEmpty())
            params.insert(QStringLiteral("fetchMode"), fetchMode);

        const auto response = cli_ipc::call(profileId, QStringLiteral("rooms.timeline"), params);
        if (handleIpcError(response))
            return 1;

        const auto result = response.value(QStringLiteral("result")).toObject();
        std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << "\n";
        return 0;
    }

    if (subcmd == QLatin1String("activate")) {
        if (args.size() < 2) {
            std::cerr << "Usage: komai rooms activate <room-id-or-alias>\n";
            return 1;
        }
        if (!requireNonEmptyValue(args.at(1), "room-id-or-alias"))
            return 1;
        auto response = cli_ipc::call(profileId,
                                      QStringLiteral("rooms.activate"),
                                      {{QStringLiteral("roomIdOrAlias"), args.at(1)}});
        if (handleIpcError(response))
            return 1;
        return 0;
    }

    if (subcmd == QLatin1String("join")) {
        if (args.size() < 2) {
            std::cerr << "Usage: komai rooms join <room-id-or-alias>\n";
            return 1;
        }
        if (!requireNonEmptyValue(args.at(1), "room-id-or-alias"))
            return 1;
        auto response = cli_ipc::call(
          profileId, QStringLiteral("rooms.join"), {{QStringLiteral("roomIdOrAlias"), args.at(1)}});
        if (handleIpcError(response))
            return 1;
        return 0;
    }

    if (subcmd == QLatin1String("new-direct-chat")) {
        if (args.size() < 2) {
            std::cerr << "Usage: komai rooms new-direct-chat <user-id>\n";
            return 1;
        }
        if (!requireNonEmptyValue(args.at(1), "user-id"))
            return 1;
        auto response = cli_ipc::call(profileId,
                                      QStringLiteral("rooms.newDirectChat"),
                                      {{QStringLiteral("userId"), args.at(1)}});
        if (handleIpcError(response))
            return 1;
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
        if (handleIpcError(response)) {
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

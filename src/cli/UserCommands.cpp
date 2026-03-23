// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UserCommands.h"

#include <iostream>

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

#include "CliDbusHelper.h"

int
runUserCommand(int argc, char *argv[], QCoreApplication & /*app*/)
{
    auto args   = cli_dbus::positionalsAfter(argc, argv, QStringLiteral("user"));
    auto subcmd = args.isEmpty() ? QString{} : args.first();

    if (subcmd.isEmpty()) {
        std::cout << "Usage: komai [-p <profile>] user <subcommand> [args...]\n\n"
                  << "Subcommands:\n"
                  << "  id                     Print the user's Matrix ID (JSON)\n"
                  << "  homeserver-url         Print the homeserver URL (JSON)\n"
                  << "  device-id              Print the device ID (JSON)\n"
                  << "  status                 Print the user's status message (JSON)\n"
                  << "  set-status <message>   Set the user's status message\n";
        return cli_dbus::hasHelpFlag(argc, argv) ? 0 : 1;
    }

    auto profileId = cli_dbus::profileFromArgs(argc, argv);
    if (!cli_dbus::ensureConnected(profileId))
        return 1;

    if (subcmd == QLatin1String("id")) {
        auto result = komai::dbus::userId(profileId);
        if (result.isEmpty()) {
            std::cerr << "Error: failed to get user ID\n";
            return 1;
        }
        std::cout << QJsonDocument(QJsonObject{{QStringLiteral("userId"), result}})
                       .toJson(QJsonDocument::Compact)
                       .toStdString()
                  << "\n";
        return 0;
    }

    if (subcmd == QLatin1String("homeserver-url")) {
        auto result = komai::dbus::homeserverUrl(profileId);
        if (result.isEmpty()) {
            std::cerr << "Error: failed to get homeserver URL\n";
            return 1;
        }
        std::cout << QJsonDocument(QJsonObject{{QStringLiteral("homeserverUrl"), result}})
                       .toJson(QJsonDocument::Compact)
                       .toStdString()
                  << "\n";
        return 0;
    }

    if (subcmd == QLatin1String("device-id")) {
        auto result = komai::dbus::deviceId(profileId);
        if (result.isEmpty()) {
            std::cerr << "Error: failed to get device ID\n";
            return 1;
        }
        std::cout << QJsonDocument(QJsonObject{{QStringLiteral("deviceId"), result}})
                       .toJson(QJsonDocument::Compact)
                       .toStdString()
                  << "\n";
        return 0;
    }

    if (subcmd == QLatin1String("status")) {
        auto result = komai::dbus::statusMessage(profileId);
        std::cout << QJsonDocument(QJsonObject{{QStringLiteral("statusMessage"), result}})
                       .toJson(QJsonDocument::Compact)
                       .toStdString()
                  << "\n";
        return 0;
    }

    if (subcmd == QLatin1String("set-status")) {
        if (args.size() < 2) {
            std::cerr << "Usage: komai user set-status <message>\n";
            return 1;
        }
        // Join remaining args so unquoted multi-word messages work
        auto message = QStringList(args.mid(1)).join(QLatin1Char(' '));
        komai::dbus::setStatusMessage(profileId, message);
        return 0;
    }

    std::cerr << "Unknown subcommand: " << subcmd.toStdString() << "\n"
              << "Run 'komai user --help' for a list of subcommands.\n";
    return 1;
}

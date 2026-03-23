// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppCommands.h"

#include <iostream>

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

#include "CliDbusHelper.h"

int
runAppCommand(int argc, char *argv[], QCoreApplication & /*app*/)
{
    auto args   = cli_dbus::positionalsAfter(argc, argv, QStringLiteral("app"));
    auto subcmd = args.isEmpty() ? QString{} : args.first();

    if (subcmd.isEmpty()) {
        std::cout << "Usage: komai [-p <profile>] app <subcommand>\n\n"
                  << "Subcommands:\n"
                  << "  version        Print the Komai version (JSON)\n"
                  << "  api-version    Print the D-Bus API version (JSON)\n";
        return cli_dbus::hasHelpFlag(argc, argv) ? 0 : 1;
    }

    auto profileId = cli_dbus::profileFromArgs(argc, argv);
    if (!cli_dbus::ensureConnected(profileId))
        return 1;

    if (subcmd == QLatin1String("version")) {
        auto result = komai::dbus::appVersion(profileId);
        if (result.isEmpty()) {
            std::cerr << "Error: failed to get app version\n";
            return 1;
        }
        std::cout << QJsonDocument(QJsonObject{{QStringLiteral("version"), result}})
                       .toJson(QJsonDocument::Compact)
                       .toStdString()
                  << "\n";
        return 0;
    }

    if (subcmd == QLatin1String("api-version")) {
        auto result = komai::dbus::apiVersion(profileId);
        if (result.isEmpty()) {
            std::cerr << "Error: failed to get API version\n";
            return 1;
        }
        std::cout << QJsonDocument(QJsonObject{{QStringLiteral("apiVersion"), result}})
                       .toJson(QJsonDocument::Compact)
                       .toStdString()
                  << "\n";
        return 0;
    }

    std::cerr << "Unknown subcommand: " << subcmd.toStdString() << "\n"
              << "Run 'komai app --help' for a list of subcommands.\n";
    return 1;
}

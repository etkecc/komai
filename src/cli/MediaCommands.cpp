// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MediaCommands.h"

#include <cstdio>
#include <iostream>

#include <QBuffer>
#include <QCoreApplication>

#include "CliDbusHelper.h"

int
runMediaCommand(int argc, char *argv[], QCoreApplication & /*app*/)
{
    auto args   = cli_dbus::positionalsAfter(argc, argv, QStringLiteral("media"));
    auto subcmd = args.isEmpty() ? QString{} : args.first();

    if (subcmd.isEmpty()) {
        std::cout << "Usage: komai [-p <profile>] media <subcommand> [args...]\n\n"
                  << "Subcommands:\n"
                  << "  fetch <mxc-uri>   Fetch an image and write PNG to stdout\n";
        return cli_dbus::hasHelpFlag(argc, argv) ? 0 : 1;
    }

    auto profileId = cli_dbus::profileFromArgs(argc, argv);
    if (!cli_dbus::ensureConnected(profileId))
        return 1;

    if (subcmd == QLatin1String("fetch")) {
        if (args.size() < 2) {
            std::cerr << "Usage: komai media fetch <mxc-uri>\n";
            return 1;
        }
        auto image = komai::dbus::mediaFetch(profileId, args.at(1));
        if (image.isNull()) {
            std::cerr << "Error: failed to fetch image or empty response\n";
            return 1;
        }
        QByteArray pngData;
        QBuffer buffer(&pngData);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG")) {
            std::cerr << "Error: failed to encode image as PNG\n";
            return 1;
        }
        fwrite(pngData.constData(), 1, pngData.size(), stdout);
        return 0;
    }

    std::cerr << "Unknown subcommand: " << subcmd.toStdString() << "\n"
              << "Run 'komai media --help' for a list of subcommands.\n";
    return 1;
}

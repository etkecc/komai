// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MediaCommands.h"

#include <cstdio>
#include <iostream>

#include <QCoreApplication>

#include "IpcClient.h"

int
runMediaCommand(int argc, char *argv[], QCoreApplication & /*app*/)
{
    auto args   = cli_ipc::positionalsAfter(argc, argv, QStringLiteral("media"));
    auto subcmd = args.isEmpty() ? QString{} : args.first();

    if (subcmd.isEmpty()) {
        std::cout << "Usage: komai [-p <profile>] media <subcommand> [args...]\n\n"
                  << "Subcommands:\n"
                  << "  fetch <mxc-uri>   Fetch an image and write PNG to stdout\n";
        return cli_ipc::hasHelpFlag(argc, argv) ? 0 : 1;
    }

    auto profileId = cli_ipc::profileFromArgs(argc, argv);
    if (!cli_ipc::ensureConnected(profileId))
        return 1;

    if (subcmd == QLatin1String("fetch")) {
        if (args.size() < 2) {
            std::cerr << "Usage: komai media fetch <mxc-uri>\n";
            return 1;
        }
        auto response = cli_ipc::call(
          profileId, QStringLiteral("media.fetch"), {{QStringLiteral("mxcUri"), args.at(1)}});
        if (response.contains(QStringLiteral("error"))) {
            std::cerr << "Error: failed to fetch image or empty response\n";
            return 1;
        }
        auto base64Data = response.value(QStringLiteral("result")).toString();
        if (base64Data.isEmpty()) {
            std::cerr << "Error: failed to fetch image or empty response\n";
            return 1;
        }
        auto pngData = QByteArray::fromBase64(base64Data.toLatin1());
        fwrite(pngData.constData(), 1, pngData.size(), stdout);
        return 0;
    }

    std::cerr << "Unknown subcommand: " << subcmd.toStdString() << "\n"
              << "Run 'komai media --help' for a list of subcommands.\n";
    return 1;
}

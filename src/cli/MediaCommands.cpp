// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MediaCommands.h"

#include <cstdio>
#include <iostream>

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>

#include "IpcClient.h"

int
runMediaCommand(int argc, char *argv[], QCoreApplication & /*app*/)
{
    auto args   = cli_ipc::positionalsAfter(argc, argv, QStringLiteral("media"));
    auto subcmd = args.isEmpty() ? QString{} : args.first();

    if (subcmd.isEmpty()) {
        std::cout << "Usage: komai [-p <profile>] media <subcommand> [args...]\n\n"
                  << "Subcommands:\n"
                  << "  fetch <mxc-uri>              Fetch an image and write PNG to stdout\n"
                  << "  upload <path>                Upload a file and return its mxc:// URI\n"
                  << "    --filename <name>          Override the filename\n"
                  << "    --content-type <mime>      Override the MIME type\n"
                  << "  upload --stdin               Upload from stdin (requires --filename)\n"
                  << "    --filename <name>          Filename for the upload (required)\n"
                  << "    --content-type <mime>      MIME type (required if not deducible)\n";
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

    if (subcmd == QLatin1String("upload")) {
        const bool fromStdin = cli_ipc::hasFlag(argc, argv, QStringLiteral("--stdin"));
        auto filenameFlag    = cli_ipc::flagValue(argc, argv, QStringLiteral("--filename"));
        auto contentTypeFlag = cli_ipc::flagValue(argc, argv, QStringLiteral("--content-type"));

        QString filePath;
        QTemporaryFile tempFile;

        if (fromStdin) {
            if (filenameFlag.isEmpty()) {
                std::cerr << "Error: --stdin requires --filename\n";
                return 1;
            }
            tempFile.setAutoRemove(true);
            if (!tempFile.open()) {
                std::cerr << "Error: failed to create temporary file\n";
                return 1;
            }
            QByteArray chunk;
            chunk.resize(65536);
            while (!std::cin.eof()) {
                std::cin.read(chunk.data(), chunk.size());
                auto bytesRead = std::cin.gcount();
                if (bytesRead > 0)
                    tempFile.write(chunk.constData(), bytesRead);
            }
            tempFile.flush();
            filePath = tempFile.fileName();
        } else {
            if (args.size() < 2) {
                std::cerr << "Usage: komai media upload <path> [--filename <name>] "
                             "[--content-type <mime>]\n"
                          << "       komai media upload --stdin --filename <name> "
                             "[--content-type <mime>]\n";
                return 1;
            }
            filePath = args.at(1);
        }

        QJsonObject params{{QStringLiteral("path"), filePath}};
        if (!filenameFlag.isEmpty())
            params.insert(QStringLiteral("filename"), filenameFlag);
        if (!contentTypeFlag.isEmpty())
            params.insert(QStringLiteral("contentType"), contentTypeFlag);

        auto response = cli_ipc::call(profileId, QStringLiteral("media.upload"), params);
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
              << "Run 'komai media --help' for a list of subcommands.\n";
    return 1;
}

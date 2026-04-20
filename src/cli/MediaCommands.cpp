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
handleFetch(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    if (parsed.positionals.isEmpty()) {
        std::cerr << "Error: mxc-uri must not be empty\n";
        return 1;
    }
    const auto mxcUri = parsed.positionals.first().trimmed();
    if (mxcUri.isEmpty()) {
        std::cerr << "Error: mxc-uri must not be empty\n";
        return 1;
    }

    auto response = cli_ipc::call(
      parsed.profileId, QStringLiteral("media.fetch"), {{QStringLiteral("mxcUri"), mxcUri}});
    if (handleIpcError(response))
        return 1;

    auto base64Data = response.value(QStringLiteral("result")).toString();
    if (base64Data.isEmpty()) {
        std::cerr << "Error: failed to fetch image or empty response\n";
        return 1;
    }
    auto pngData = QByteArray::fromBase64(base64Data.toLatin1());
    std::fwrite(pngData.constData(), 1, pngData.size(), stdout);
    return 0;
}

int
handleUpload(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    const bool fromStdin   = parsed.hasFlag(QStringLiteral("--stdin"));
    const auto filename    = parsed.flagOr(QStringLiteral("--filename"));
    const auto contentType = parsed.flagOr(QStringLiteral("--content-type"));

    QString filePath;
    QTemporaryFile tempFile;

    if (fromStdin) {
        if (filename.isEmpty()) {
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
        if (parsed.positionals.isEmpty()) {
            std::cerr << "Error: upload requires <path> (or use --stdin with --filename)\n";
            return 1;
        }
        filePath = parsed.positionals.first().trimmed();
        if (filePath.isEmpty()) {
            std::cerr << "Error: path must not be empty\n";
            return 1;
        }
    }

    QJsonObject params{{QStringLiteral("path"), filePath}};
    if (!filename.isEmpty())
        params.insert(QStringLiteral("filename"), filename);
    if (!contentType.isEmpty())
        params.insert(QStringLiteral("contentType"), contentType);

    auto response = cli_ipc::call(parsed.profileId, QStringLiteral("media.upload"), params);
    if (handleIpcError(response))
        return 1;

    auto result = response.value(QStringLiteral("result")).toObject();
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << "\n";
    return 0;
}

} // namespace

cli_schema::GroupDef
mediaGroupDef()
{
    cli_schema::GroupDef group;
    group.name = QStringLiteral("media");
    group.help = QStringLiteral("Media content resolution");

    cli_schema::SubcommandDef fetch;
    fetch.name = QStringLiteral("fetch");
    fetch.help = QStringLiteral("Fetch an image and write PNG bytes to standard output");
    cli_schema::PositionalDef mxc;
    mxc.name = QStringLiteral("mxc-uri");
    fetch.positionals.append(mxc);
    fetch.handler = handleFetch;
    group.subcommands.append(fetch);

    cli_schema::SubcommandDef upload;
    upload.name = QStringLiteral("upload");
    upload.help = QStringLiteral("Upload a file (or stdin) and return its mxc:// URI (JSON)");
    cli_schema::PositionalDef path;
    path.name     = QStringLiteral("path");
    path.help     = QStringLiteral("Local file path; omit when --stdin is used.");
    path.optional = true;
    upload.positionals.append(path);

    cli_schema::FlagDef stdinFlag;
    stdinFlag.longName = QStringLiteral("--stdin");
    stdinFlag.help     = QStringLiteral("Upload from standard input; requires --filename.");
    upload.flags.append(stdinFlag);

    cli_schema::FlagDef filenameFlag;
    filenameFlag.longName         = QStringLiteral("--filename");
    filenameFlag.takesValue       = true;
    filenameFlag.valuePlaceholder = QStringLiteral("<name>");
    filenameFlag.help = QStringLiteral("Override the on-wire filename (required with --stdin).");
    upload.flags.append(filenameFlag);

    cli_schema::FlagDef contentTypeFlag;
    contentTypeFlag.longName         = QStringLiteral("--content-type");
    contentTypeFlag.takesValue       = true;
    contentTypeFlag.valuePlaceholder = QStringLiteral("<mime>");
    contentTypeFlag.help             = QStringLiteral("Override the MIME type.");
    upload.flags.append(contentTypeFlag);

    upload.handler = handleUpload;
    group.subcommands.append(upload);

    return group;
}

int
runMediaCommand(int argc, char *argv[], QCoreApplication &app)
{
    return cli_schema::dispatchGroup(mediaGroupDef(), argc, argv, app);
}

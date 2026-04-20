// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "McpCommands.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <vector>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

#include "profile/ProfileId.h"
#include "schema/Dispatcher.h"
#include "schema/SchemaTypes.h"

#if !defined(Q_OS_WIN)
#include <unistd.h>
#endif

namespace {

QString
resolveBinaryPath()
{
    const auto appDir = QCoreApplication::applicationDirPath();
    auto path         = QStandardPaths::findExecutable(QStringLiteral("komai-mcp"), {appDir});
    if (!path.isEmpty())
        return path;
    return QStandardPaths::findExecutable(QStringLiteral("komai-mcp"));
}

int
handleServe(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    const auto command = mcp_commands::buildServeCommand(parsed);

    const auto binaryPath = resolveBinaryPath();
    if (binaryPath.isEmpty()) {
        std::cerr << "Error: could not find the 'komai-mcp' helper binary next to komai or in "
                     "PATH.\n";
        return 1;
    }

#if defined(Q_OS_WIN)
    QProcess child;
    child.setInputChannelMode(QProcess::ForwardedInputChannel);
    child.setProcessChannelMode(QProcess::ForwardedChannels);
    child.start(binaryPath, command.childArguments());

    if (!child.waitForStarted()) {
        std::cerr << "Error: failed to launch '" << binaryPath.toStdString()
                  << "': " << child.errorString().toStdString() << "\n";
        return 1;
    }

    if (!child.waitForFinished(-1)) {
        std::cerr << "Error: failed while waiting for '" << binaryPath.toStdString()
                  << "': " << child.errorString().toStdString() << "\n";
        return 1;
    }

    if (child.exitStatus() != QProcess::NormalExit) {
        std::cerr << "Error: '" << binaryPath.toStdString() << "' terminated abnormally.\n";
        return 1;
    }

    return child.exitCode();
#else
    std::vector<QByteArray> encodedArgs;
    encodedArgs.reserve(static_cast<size_t>(command.childArguments().size()) + 1);
    encodedArgs.emplace_back(QFile::encodeName(binaryPath));
    for (const auto &arg : command.childArguments())
        encodedArgs.emplace_back(arg.toLocal8Bit());

    std::vector<char *> argvExec;
    argvExec.reserve(encodedArgs.size() + 1);
    for (auto &arg : encodedArgs)
        argvExec.push_back(arg.data());
    argvExec.push_back(nullptr);

    ::execv(encodedArgs.front().constData(), argvExec.data());

    std::cerr << "Error: failed to launch '" << binaryPath.toStdString()
              << "': " << std::strerror(errno) << "\n";
    return 1;
#endif
}

} // namespace

cli_schema::GroupDef
mcpGroupDef()
{
    cli_schema::GroupDef group;
    group.name     = QStringLiteral("mcp");
    group.help     = QStringLiteral("Model Context Protocol stdio server wrapper");
    group.longHelp = QStringLiteral("Expose a running Komai profile as an MCP server over stdio.\n"
                                    "The target profile must already be running.");

    cli_schema::SubcommandDef serve;
    serve.name = QStringLiteral("serve");
    serve.help = QStringLiteral("Start the MCP stdio server");
    // The serve handler execs into komai-mcp, which connects to the running
    // Komai instance itself. We don't need ensureConnected() here — that's the
    // child's job — so skip the dispatcher's IPC probe.
    serve.requiresProfile = false;

    cli_schema::FlagDef access;
    access.longName     = QStringLiteral("--access");
    access.takesValue   = true;
    access.valueEnum    = {QStringLiteral("read_only"), QStringLiteral("read_write")};
    access.defaultValue = QStringLiteral("read_only");
    access.help         = QStringLiteral("Access mode passed to the MCP server.");
    serve.flags.append(access);

    serve.handler = handleServe;
    group.subcommands.append(serve);

    return group;
}

namespace mcp_commands {

QStringList
ServeCommand::childArguments() const
{
    return {
      QStringLiteral("serve"),
      QStringLiteral("--profile"),
      profileId,
      QStringLiteral("--access"),
      accessMode,
    };
}

ServeCommand
buildServeCommand(const cli_schema::ParsedArgs &parsed)
{
    ServeCommand command;
    command.profileId  = profile_id::normalized(parsed.profileId);
    command.accessMode = parsed.flagOr(QStringLiteral("--access"), QStringLiteral("read_only"));
    return command;
}

} // namespace mcp_commands

int
runMcpCommand(int argc, char *argv[], QCoreApplication &app)
{
    return cli_schema::dispatchGroup(mcpGroupDef(), argc, argv, app);
}

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

#include "IpcClient.h"
#include "profile/ProfileId.h"

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

} // namespace

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

QString
usageText()
{
    return QStringLiteral("Usage: komai [-p <profile>] mcp serve [--access read_only|read_write]\n"
                          "\n"
                          "Expose a running Komai profile as an MCP server over stdio.\n"
                          "The target profile must already be running.\n"
                          "\n"
                          "Subcommands:\n"
                          "  serve    Start the MCP stdio server\n"
                          "\n"
                          "Options:\n"
                          "  -p, --profile <profile>        Target profile (default: default)\n"
                          "      --access <mode>            Access mode (default: read_only)\n"
                          "                                 Values: read_only, read_write\n");
}

ServeCommand
parseServeCommand(int argc, char *argv[])
{
    ServeCommand command;

    const auto args   = cli_ipc::positionalsAfter(argc, argv, QStringLiteral("mcp"));
    const auto subcmd = args.isEmpty() ? QString{} : args.first();

    if (cli_ipc::hasHelpFlag(argc, argv) || subcmd.isEmpty()) {
        command.status = ParseStatus::Help;
        return command;
    }

    if (subcmd != QLatin1String("serve")) {
        command.status       = ParseStatus::Error;
        command.errorMessage = QStringLiteral("Unknown subcommand: ") + subcmd;
        return command;
    }

    if (args.size() > 1) {
        command.status       = ParseStatus::Error;
        command.errorMessage = QStringLiteral("Unexpected positional arguments after 'serve'.");
        return command;
    }

    const auto accessMode =
      cli_ipc::flagValue(argc, argv, QStringLiteral("--access"), QStringLiteral("read_only"));
    if (accessMode != QLatin1String("read_only") && accessMode != QLatin1String("read_write")) {
        command.status       = ParseStatus::Error;
        command.errorMessage = QStringLiteral("Invalid --access value: ") + accessMode +
                               QStringLiteral(" (expected read_only or read_write)");
        return command;
    }

    const auto rawProfile = cli_ipc::profileFromArgs(argc, argv);
    command.status        = ParseStatus::Ready;
    command.profileId     = profile_id::normalized(rawProfile);
    command.accessMode    = accessMode;
    return command;
}

} // namespace mcp_commands

int
runMcpCommand(int argc, char *argv[], QCoreApplication & /*app*/)
{
    const auto command = mcp_commands::parseServeCommand(argc, argv);

    if (command.status == mcp_commands::ParseStatus::Help) {
        std::cout << mcp_commands::usageText().toStdString();
        return 0;
    }

    if (command.status == mcp_commands::ParseStatus::Error) {
        std::cerr << "Error: " << command.errorMessage.toStdString() << "\n\n"
                  << mcp_commands::usageText().toStdString();
        return 1;
    }

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

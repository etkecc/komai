// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProfileCommands.h"

#include <iostream>

#include <QCoreApplication>
#include <QFileInfo>

#include "profile/Paths.h"
#include "profile/ProfileId.h"

namespace profile_commands {

namespace {

QStringList
positionalsAfter(int argc, char *argv[], const QString &keyword)
{
    static const QString optionsWithValues[] = {
      QStringLiteral("-p"),
      QStringLiteral("--profile"),
      QStringLiteral("-l"),
      QStringLiteral("--log-level"),
      QStringLiteral("-L"),
      QStringLiteral("--log-type"),
    };

    QStringList result;
    bool pastKeyword = false;
    for (int i = 1; i < argc; ++i) {
        QString arg{argv[i]};

        bool skipped = false;
        for (const auto &opt : optionsWithValues) {
            if (arg == opt) {
                ++i;
                skipped = true;
                break;
            }
            if (arg.startsWith(opt + QLatin1Char('='))) {
                skipped = true;
                break;
            }
        }
        if (skipped)
            continue;

        if (arg.startsWith(QLatin1Char('-')))
            continue;

        if (!pastKeyword) {
            if (arg == keyword)
                pastKeyword = true;
            continue;
        }

        result.append(arg);
    }

    return result;
}

bool
hasHelpFlag(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        const QString arg{argv[i]};
        if (arg == QLatin1String("--help") || arg == QLatin1String("-h"))
            return true;
    }

    return false;
}

} // namespace

QString
usageText()
{
    return QStringLiteral(
      "Usage: komai profiles launcher <create|remove> <profile-id>\n"
      "\n"
      "Create or remove an explicit desktop launcher for a non-default profile.\n"
      "On native Linux, launching a profile from its own desktop launcher makes\n"
      "app/taskbar badges and launcher grouping reliable for that profile.\n"
      "\n"
      "Commands:\n"
      "  launcher create <profile-id>    Create or update a launcher for the profile\n"
      "  launcher remove <profile-id>    Remove the launcher for the profile\n");
}

LauncherCommand
parseLauncherCommand(int argc, char *argv[])
{
    LauncherCommand command;

    const auto args = positionalsAfter(argc, argv, QStringLiteral("profiles"));
    if (hasHelpFlag(argc, argv) || args.isEmpty()) {
        command.status = ParseStatus::Help;
        return command;
    }

    if (args.first() != QLatin1String("launcher")) {
        command.status       = ParseStatus::Error;
        command.errorMessage = QStringLiteral("Unknown subcommand group: ") + args.first();
        return command;
    }

    if (args.size() < 3) {
        command.status       = ParseStatus::Error;
        command.errorMessage = QStringLiteral("Expected 'launcher <create|remove> <profile-id>'.");
        return command;
    }

    if (args.size() > 3) {
        command.status = ParseStatus::Error;
        command.errorMessage =
          QStringLiteral("Unexpected positional arguments after <profile-id>.");
        return command;
    }

    const auto action = args.at(1);
    if (action == QLatin1String("create")) {
        command.action = LauncherAction::Create;
    } else if (action == QLatin1String("remove")) {
        command.action = LauncherAction::Remove;
    } else {
        command.status       = ParseStatus::Error;
        command.errorMessage = QStringLiteral("Unknown launcher action: ") + action;
        return command;
    }

    command.profileId = profile_id::normalized(args.at(2));
    if (const auto validationError = profile_id::validate(command.profileId); validationError) {
        command.status       = ParseStatus::Error;
        command.errorMessage = QStringLiteral("Invalid profile id: %1").arg(*validationError);
        return command;
    }

    if (command.profileId == QLatin1String("default")) {
        command.status       = ParseStatus::Error;
        command.errorMessage = QStringLiteral(
          "The default profile already uses the packaged Komai launcher. Explicit profile "
          "launchers are only needed for non-default profiles.");
        return command;
    }

    command.status = ParseStatus::Ready;
    return command;
}

} // namespace profile_commands

int
runProfileCommand(int argc, char *argv[], QCoreApplication &app)
{
    const auto command = profile_commands::parseLauncherCommand(argc, argv);

    if (command.status == profile_commands::ParseStatus::Help) {
        std::cout << profile_commands::usageText().toStdString();
        return 0;
    }

    if (command.status == profile_commands::ParseStatus::Error) {
        std::cerr << "Error: " << command.errorMessage.toStdString() << "\n\n"
                  << profile_commands::usageText().toStdString();
        return 1;
    }

    if (!app_paths::desktop::supportsProfileDesktopEntries()) {
        std::cerr << "Error: explicit profile launchers are currently supported only on native "
                     "Linux desktop installs.\n";
        return 1;
    }

    if (app.applicationFilePath().isEmpty()) {
        std::cerr << "Error: could not determine the current komai executable path.\n";
        return 1;
    }

    const auto launcherPath   = app_paths::desktop::profileDesktopEntryFile(command.profileId);
    const bool launcherExists = QFileInfo::exists(launcherPath);

    QString error;
    if (command.action == profile_commands::LauncherAction::Create) {
        if (!app_paths::desktop::ensureProfileDesktopEntry(
              command.profileId, app.applicationFilePath(), &error)) {
            std::cerr << "Error: " << error.toStdString() << "\n";
            return 1;
        }

        std::cout << (launcherExists ? "Updated" : "Created")
                  << " profile launcher: " << launcherPath.toStdString() << "\n"
                  << "Launch '" << command.profileId.toStdString()
                  << "' from this desktop launcher to get reliable app/taskbar badges.\n";
        return 0;
    }

    if (!app_paths::desktop::removeProfileDesktopEntry(command.profileId, &error)) {
        std::cerr << "Error: " << error.toStdString() << "\n";
        return 1;
    }

    std::cout << (launcherExists ? "Removed" : "No launcher present at") << ": "
              << launcherPath.toStdString() << "\n";
    return 0;
}

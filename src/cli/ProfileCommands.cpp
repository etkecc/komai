// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProfileCommands.h"

#include <iostream>

#include <QCoreApplication>
#include <QFileInfo>

#include "profile/Paths.h"
#include "profile/ProfileId.h"
#include "schema/Dispatcher.h"
#include "schema/SchemaTypes.h"

namespace profile_commands {

std::optional<QString>
validateLauncherProfileId(const QString &profileId)
{
    const auto normalized = profile_id::normalized(profileId);
    if (const auto validationError = profile_id::validate(normalized); validationError)
        return QStringLiteral("Invalid profile id: %1").arg(*validationError);

    if (normalized == QLatin1String("default"))
        return QStringLiteral(
          "The default profile already uses the packaged Komai launcher. Explicit profile "
          "launchers are only needed for non-default profiles.");

    return std::nullopt;
}

} // namespace profile_commands

namespace {

int
runLauncherAction(const cli_schema::ParsedArgs &parsed, QCoreApplication &app, bool isCreate)
{
    Q_UNUSED(app);
    const auto rawProfile = parsed.positionals.value(0);
    const auto profileId  = profile_id::normalized(rawProfile);

    if (const auto error = profile_commands::validateLauncherProfileId(profileId); error) {
        std::cerr << "Error: " << error->toStdString() << "\n";
        return 1;
    }

    if (!app_paths::desktop::supportsProfileDesktopEntries()) {
        std::cerr << "Error: explicit profile launchers are currently supported only on native "
                     "Linux desktop installs.\n";
        return 1;
    }

    const QString executablePath = app_paths::executablePathForRelaunch();
    if (executablePath.isEmpty()) {
        std::cerr << "Error: could not determine the current komai executable path.\n";
        return 1;
    }

    const auto launcherPath   = app_paths::desktop::profileDesktopEntryFile(profileId);
    const bool launcherExists = QFileInfo::exists(launcherPath);

    QString error;
    if (isCreate) {
        if (!app_paths::desktop::ensureProfileDesktopEntry(profileId, executablePath, &error)) {
            std::cerr << "Error: " << error.toStdString() << "\n";
            return 1;
        }
        std::cout << (launcherExists ? "Updated" : "Created")
                  << " profile launcher: " << launcherPath.toStdString() << "\n"
                  << "Launch '" << profileId.toStdString()
                  << "' from this desktop launcher to get reliable app/taskbar badges.\n";
        return 0;
    }

    if (!app_paths::desktop::removeProfileDesktopEntry(profileId, &error)) {
        std::cerr << "Error: " << error.toStdString() << "\n";
        return 1;
    }

    std::cout << (launcherExists ? "Removed" : "No launcher present at") << ": "
              << launcherPath.toStdString() << "\n";
    return 0;
}

int
handleLauncherCreate(const cli_schema::ParsedArgs &parsed, QCoreApplication &app)
{
    return runLauncherAction(parsed, app, /*isCreate=*/true);
}

int
handleLauncherRemove(const cli_schema::ParsedArgs &parsed, QCoreApplication &app)
{
    return runLauncherAction(parsed, app, /*isCreate=*/false);
}

} // namespace

cli_schema::GroupDef
profilesGroupDef()
{
    cli_schema::GroupDef group;
    group.name = QStringLiteral("profiles");
    group.help = QStringLiteral("Profile launcher management (offline)");
    group.longHelp =
      QStringLiteral("Create or remove an explicit desktop launcher for a non-default "
                     "profile.\n"
                     "On native Linux, launching a profile from its own desktop launcher makes\n"
                     "app/taskbar badges and launcher grouping reliable for that profile.");

    cli_schema::SubcommandDef launcher;
    launcher.name            = QStringLiteral("launcher");
    launcher.help            = QStringLiteral("Manage per-profile desktop launchers");
    launcher.requiresProfile = false;

    cli_schema::PositionalDef profileIdPos;
    profileIdPos.name = QStringLiteral("profile-id");
    profileIdPos.help = QStringLiteral("Target profile id (not 'default').");

    cli_schema::SubcommandDef create;
    create.name            = QStringLiteral("create");
    create.help            = QStringLiteral("Create or update a launcher for the profile");
    create.requiresProfile = false;
    create.positionals.append(profileIdPos);
    create.handler = handleLauncherCreate;
    launcher.subcommands.append(create);

    cli_schema::SubcommandDef remove;
    remove.name            = QStringLiteral("remove");
    remove.help            = QStringLiteral("Remove the launcher for the profile");
    remove.requiresProfile = false;
    remove.positionals.append(profileIdPos);
    remove.handler = handleLauncherRemove;
    launcher.subcommands.append(remove);

    group.subcommands.append(launcher);

    return group;
}

int
runProfileCommand(int argc, char *argv[], QCoreApplication &app)
{
    return cli_schema::dispatchGroup(profilesGroupDef(), argc, argv, app);
}

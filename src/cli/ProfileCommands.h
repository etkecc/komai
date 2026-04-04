// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

class QCoreApplication;

namespace profile_commands {

enum class ParseStatus
{
    Help,
    Ready,
    Error,
};

enum class LauncherAction
{
    Create,
    Remove,
};

struct LauncherCommand
{
    ParseStatus status = ParseStatus::Help;
    LauncherAction action{LauncherAction::Create};
    QString profileId;
    QString errorMessage;
};

LauncherCommand
parseLauncherCommand(int argc, char *argv[]);

QString
usageText();

} // namespace profile_commands

int
runProfileCommand(int argc, char *argv[], QCoreApplication &app);

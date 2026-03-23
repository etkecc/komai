// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

class QCoreApplication;

namespace mcp_commands {

enum class ParseStatus
{
    Help,
    Ready,
    Error,
};

struct ServeCommand
{
    ParseStatus status = ParseStatus::Help;
    QString profileId;
    QString accessMode;
    QString errorMessage;

    QStringList childArguments() const;
};

ServeCommand
parseServeCommand(int argc, char *argv[]);

QString
usageText();

} // namespace mcp_commands

int
runMcpCommand(int argc, char *argv[], QCoreApplication &app);

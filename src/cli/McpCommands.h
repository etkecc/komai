// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

#include "schema/SchemaTypes.h"

class QCoreApplication;

namespace mcp_commands {

// Concrete "serve" invocation: the normalised profile id and access mode,
// plus the argv vector we hand off to the `komai-mcp` helper binary.
struct ServeCommand
{
    QString profileId;
    QString accessMode;

    QStringList childArguments() const;
};

// Builds a ServeCommand from arguments already parsed by the schema
// dispatcher. Normalises the profile id (empty -> "default").
ServeCommand
buildServeCommand(const cli_schema::ParsedArgs &parsed);

} // namespace mcp_commands

cli_schema::GroupDef
mcpGroupDef();

int
runMcpCommand(int argc, char *argv[], QCoreApplication &app);

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

#include "SchemaTypes.h"

namespace cli_schema {

// Render the help text shown for `komai <group>` (when no subcommand is
// given, or `--help` is passed at the group level).
QString
renderGroupHelp(const GroupDef &group);

// Render the help text shown for `komai <group> <sub...> --help`. The path
// must begin with the group name and descend through each nested subcommand.
QString
renderSubcommandHelp(const GroupDef &group, const QStringList &subcommandPath);

} // namespace cli_schema

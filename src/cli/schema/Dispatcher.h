// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SchemaTypes.h"

class QCoreApplication;

namespace cli_schema {

// Parses argv against the declared schema and dispatches to the appropriate
// subcommand handler. Returns the handler's exit code, or:
//   0  if --help was requested (at any level).
//   1  on parse errors (unknown subcommand, unknown flag, bad enum value,
//      missing required positional). An error message is already written to
//      stderr.
//
// The caller (typically a thin runXxxCommand() wrapper) must pass the raw
// argc/argv the process received, not a subset.
int
dispatchGroup(const GroupDef &group, int argc, char *argv[], QCoreApplication &app);

} // namespace cli_schema

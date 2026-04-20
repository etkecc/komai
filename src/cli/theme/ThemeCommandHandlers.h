// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "cli/schema/SchemaTypes.h"

class QCoreApplication;

namespace theme_command {

int
handleTintedImport(const cli_schema::ParsedArgs &parsed, QCoreApplication &app);

int
handleTintedSearch(const cli_schema::ParsedArgs &parsed, QCoreApplication &app);

int
handleList(const cli_schema::ParsedArgs &parsed, QCoreApplication &app);

int
handleCreateSample(const cli_schema::ParsedArgs &parsed, QCoreApplication &app);

} // namespace theme_command

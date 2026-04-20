// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "schema/SchemaTypes.h"

class QCoreApplication;

cli_schema::GroupDef
appGroupDef();

int
runAppCommand(int argc, char *argv[], QCoreApplication &app);

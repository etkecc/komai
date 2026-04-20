// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>

#include "schema/SchemaTypes.h"

class QCoreApplication;

// Returns every CLI group's declarative schema (including `completions`
// itself). Used by the shell-completion renderers.
QList<cli_schema::GroupDef>
allCliGroupDefs();

cli_schema::GroupDef
completionsGroupDef();

namespace completions {

QString
renderBash(const QList<cli_schema::GroupDef> &groups);

QString
renderZsh(const QList<cli_schema::GroupDef> &groups);

QString
renderFish(const QList<cli_schema::GroupDef> &groups);

} // namespace completions

int
runCompletionsCommand(int argc, char *argv[], QCoreApplication &app);

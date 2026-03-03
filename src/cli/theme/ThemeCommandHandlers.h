// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QCoreApplication;

namespace theme_command {

int
handleTintedImport(int argc, char *argv[], QCoreApplication &app);
int
handleTintedSearch(int argc, char *argv[], QCoreApplication &app);
int
handleList(int argc, char *argv[], QCoreApplication &app);
int
handleCreateSample(int argc, char *argv[], QCoreApplication &app);

} // namespace theme_command

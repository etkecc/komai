// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Returns exit code (>= 0) if a CLI command was handled.
// Returns -1 if no CLI command detected (caller proceeds to GUI).
int
dispatchCliCommand(int argc, char *argv[]);

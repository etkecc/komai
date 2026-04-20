// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include <QString>

class QCoreApplication;

namespace profile_commands {

// Validates that `profileId` is usable as the target of an explicit profile
// launcher. Returns an error message on failure, or `std::nullopt` if the id
// is well-formed and not reserved.
//
// Exposed (rather than inlined into the handler) so unit tests can exercise
// the rules directly without going through the full dispatcher.
std::optional<QString>
validateLauncherProfileId(const QString &profileId);

} // namespace profile_commands

int
runProfileCommand(int argc, char *argv[], QCoreApplication &app);

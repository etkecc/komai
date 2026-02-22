// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

class QString;
class QStringView;

namespace settings::bootstrap {

/**
 * Read startup-only settings that must be applied before Q(Core)Application is created.
 */

/**
 * Read the initial UI scale factor from profile config, if present and valid.
 */
std::optional<float> readUiScaleFactor(QStringView profile);

} // namespace settings::bootstrap


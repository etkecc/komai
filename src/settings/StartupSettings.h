// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include <QString>

namespace settings::startup {

/**
 * Read startup-only settings values that must be loaded before a QApplication is
 * created (such as initial scale-factor environment setup).
 */
std::optional<float>
readUiScaleFactor(const QString &profile);

} // namespace settings::startup

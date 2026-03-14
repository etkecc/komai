// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace keyring_environment {

/// Returns the environment tag used as part of keyring key names
/// to isolate secrets across packaging formats depending on the
/// filesystem config paths they use, so that each unique filesystem
/// path produces a unique keyring prefix.
///
/// Known environments get human-friendly tags:
///   - "native"  — standard (non-sandboxed) builds on any platform, including AppImage
///   - "flatpak" — Flatpak builds
///   - "snap"    — Snap builds
///
/// Unknown/exotic environments get a short hash of the config root path,
/// e.g. "a1dfc4".
const QString &
tag();

/// Returns the "komai.<tag>." prefix for keyring key names.
const QString &
prefix();

/// Returns the environment tag for a given config root path.
/// This is the pure logic behind tag(), exposed for testing.
QString
tagForConfigRoot(QStringView configRoot);

} // namespace keyring_environment

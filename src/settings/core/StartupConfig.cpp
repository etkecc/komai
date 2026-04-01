// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StartupConfig.h"

#include "komai-rust-cxxbridge/lib.h"

namespace settings::core {

StartupConfigSnapshot
snapshotFromYamlFile(std::string_view path)
{
    const auto startupSnapshot =
      ::komai::rust::settings_load_startup_snapshot_from_path(std::string(path));

    StartupConfigSnapshot snapshot;
    if (startupSnapshot.has_ui_scale_factor)
        snapshot.uiScaleFactor = normalizeScaleFactor(startupSnapshot.ui_scale_factor);
    return snapshot;
}

} // namespace settings::core

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StartupConfig.h"

#include "komai-rust-cxxbridge/ffi.h"

namespace settings::core {

StartupConfigSnapshot
snapshotForProfile(std::string_view profileId)
{
    const auto startupSnapshot =
      ::komai::rust::settings_load_startup_snapshot_for_profile(std::string(profileId));

    StartupConfigSnapshot snapshot;
    if (startupSnapshot.has_ui_scale_factor)
        snapshot.uiScaleFactor = normalizeScaleFactor(startupSnapshot.ui_scale_factor);
    return snapshot;
}

} // namespace settings::core

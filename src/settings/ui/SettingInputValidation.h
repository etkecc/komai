// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QVariant>

#include "settings/ui/SettingDescriptor.h"

namespace settings::ui {

// Validate user-provided model input for a setting row before applying mutation callbacks.
bool
validateSettingInput(const SettingMeta &meta, const QVariant &value);

// Validate user-provided model input for SettingId-backed special role setters.
bool
validateRoleInput(settings::core::SettingId settingId, int role, const QVariant &value);

} // namespace settings::ui

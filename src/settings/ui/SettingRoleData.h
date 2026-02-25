// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QVariant>

#include "settings/core/SettingDefinition.h"

namespace settings::ui {

QVariant
roleDataForSetting(settings::core::SettingId id, int role);

bool
hasWritableRoleDataForSetting(settings::core::SettingId id, int role);

bool
setRoleDataForSetting(settings::core::SettingId id, int role, const QVariant &value);

} // namespace settings::ui

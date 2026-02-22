// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QString>

#include <yaml-cpp/yaml.h>

#include "Logging.h"
#include "Paths.h"
#include "UserSettingsPage.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsPersistence.h"
#include "settings/SettingsStorage.h"
#include "settings/StagedLoadPlan.h"
#include "settings/YamlSettings.h"

#include "UserSettingsPersistenceHelpers.inc"
#include "UserSettingsPersistenceLoad.inc"
#include "UserSettingsPersistenceSave.inc"

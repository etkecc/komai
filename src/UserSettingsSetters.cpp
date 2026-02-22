// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QString>
#include <cmath>

#include <QApplication>
#include <QFont>
#include <QFontDatabase>

#include "JdenticonProvider.h"
#include "Logging.h"
#include "MatrixClient.h"
#include "UserSettingsPage.h"
#include "encryption/Olm.h"
#include "settings/SettingKeys.h"
#include "settings/core/StartupConfig.h"

#include "UserSettingsSettersCore.inc"
#include "UserSettingsSettersLayout.inc"
#include "UserSettingsSettersMisc.inc"
#include "UserSettingsSettersUi.inc"

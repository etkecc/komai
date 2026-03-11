// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/Utils.h"

#include "settings/ui/facade/UserSettingsPage.h"

namespace utils {

QString
localUser()
{
    if (const auto settings = UserSettings::instance())
        return settings->userId();

    return {};
}

} // namespace utils

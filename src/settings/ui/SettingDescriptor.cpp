// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/SettingDescriptor.h"

#include <QHash>

#include "settings/core/SettingsDefinitions.h"

namespace settings::ui {

namespace {

bool
isRuntimeStatusSettingId(settings::core::SettingId id)
{
    switch (id) {
    case settings::core::SettingId::EncryptionOnlineBackupKeyStatus:
    case settings::core::SettingId::EncryptionSelfSigningKeyStatus:
    case settings::core::SettingId::EncryptionUserSigningKeyStatus:
    case settings::core::SettingId::EncryptionMasterSigningKeyStatus:
        return true;
    default:
        return false;
    }
}

} // namespace

int
rowForSettingId(settings::core::SettingId id)
{
    if (id == settings::core::SettingId::Unknown)
        return -1;

    static const auto lookup = [] {
        QHash<int, int> idToRow;
        idToRow.reserve(settingsTableRowCount());

        for (int i = 0; i < settingsTableRowCount(); ++i) {
            const auto settingId = settingsTable[i].settingId;
            if (settingId == settings::core::SettingId::Unknown)
                continue;

            const bool knownPersisted =
              settings::core::definitions::hasPersistedDefinition(settingId);
            const bool knownRuntime = isRuntimeStatusSettingId(settingId);
            Q_ASSERT_X(knownPersisted || knownRuntime,
                       "settings::ui::rowForSettingId",
                       "settingsTable row references an unknown non-persisted SettingId.");

            const int key = static_cast<int>(settingId);
            Q_ASSERT_X(!idToRow.contains(key),
                       "settings::ui::rowForSettingId",
                       "Duplicate non-Unknown SettingId found in settingsTable.");

            if (!idToRow.contains(key))
                idToRow.insert(key, i);
        }

        return idToRow;
    }();

    return lookup.value(static_cast<int>(id), -1);
}

} // namespace settings::ui

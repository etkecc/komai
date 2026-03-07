// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/SettingDescriptor.h"

#include <QHash>
#include <mutex>

#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/UserSettingsModel.h"

namespace settings::ui {

namespace {

bool
isRuntimeStatusSettingId(settings::core::SettingId id)
{
    switch (id) {
    case settings::core::SettingId::NotificationsAccountEnabled:
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

void
validateSettingsTable()
{
    static std::once_flag validationOnce;
    std::call_once(validationOnce, []() {
        for (int i = 0; i < settingsTableRowCount(); ++i) {
            const auto &row = settingsTable[i];

            Q_ASSERT_X(row.type >= UserSettingsModel::Toggle &&
                         row.type <= UserSettingsModel::AccessTokenField,
                       "settings::ui::validateSettingsTable",
                       "settingsTable row has an invalid type enum value.");

            Q_ASSERT_X(row.tab >= UserSettingsModel::TabLookFeel &&
                         row.tab <= UserSettingsModel::TabAbout,
                       "settings::ui::validateSettingsTable",
                       "settingsTable row has an invalid tab enum value.");

            if (row.type == UserSettingsModel::SectionTitle) {
                Q_ASSERT_X(!row.getValue && !row.setValue,
                           "settings::ui::validateSettingsTable",
                           "SectionTitle rows must not provide value mutators.");
            } else if (row.type == UserSettingsModel::KeyStatus) {
                Q_ASSERT_X(row.getValue && !row.setValue,
                           "settings::ui::validateSettingsTable",
                           "KeyStatus rows must be read-only and provide a value getter.");
            }

            if (row.settingId != settings::core::SettingId::Unknown) {
                Q_ASSERT_X(row.type != UserSettingsModel::SectionTitle,
                           "settings::ui::validateSettingsTable",
                           "SectionTitle rows must not be bound to a SettingId.");
            }
        }
    });
}

int
rowForSettingId(settings::core::SettingId id)
{
    validateSettingsTable();

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

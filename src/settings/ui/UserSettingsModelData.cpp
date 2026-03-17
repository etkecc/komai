// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/UserSettingsModel.h"

#include "logging/Logging.h"
#include "settings/ui/SettingDescriptor.h"
#include "settings/ui/SettingInputValidation.h"
#include "settings/ui/SettingRoleData.h"
#include "settings/ui/facade/UserSettingsPage.h"

using settings::ui::settingsTable;
using settings::ui::settingsTableRowCount;

QVariant
UserSettingsModel::data(const QModelIndex &index, int role) const
{
    if (index.row() >= settingsTableRowCount())
        return {};

    auto i = UserSettings::instance();
    if (!i)
        return {};

    const auto &m = settingsTable[index.row()];

    switch (role) {
    case Name:
        return m.name ? tr(m.name) : QVariant{};
    case Description:
        if (const auto roleData = settings::ui::roleDataForSetting(m.settingId, role);
            roleData.isValid())
            return roleData;

        if (!m.description)
            return QVariant{};
        return tr(m.description);
    case Icon:
        return m.icon ? QVariant{QString::fromUtf8(m.icon)} : QVariant{};
    case Type:
        return m.type;
    case Tab:
        return m.tab;
    case TagId:
        return m.tagId ? QVariant{QString::fromUtf8(m.tagId)} : QVariant{};
    case SyncedToMatrix:
        return m.syncedToMatrix;
    case Value:
        return m.getValue ? m.getValue() : QVariant{};
    case Enabled:
        return m.isEnabled ? m.isEnabled() : true;
    case ValueLowerBound:
        return m.lowerBound;
    case ValueUpperBound:
        return m.upperBound;
    case ValueStep:
        return m.step;
    case Values:
        return m.getValues ? m.getValues() : QVariant{};
    case Good:
    case ThemeVariantValue:
    case ThemeVariantValues:
        if (const auto roleData = settings::ui::roleDataForSetting(m.settingId, role);
            roleData.isValid())
            return roleData;
        if (role == ThemeVariantValue)
            return -1;
        if (role == ThemeVariantValues)
            return QStringList{};
        return false;
    }

    return {};
}

bool
UserSettingsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.row() >= settingsTableRowCount())
        return false;

    auto i = UserSettings::instance();
    if (!i)
        return false;

    const auto &m = settingsTable[index.row()];
    if (role == Value) {
        if (!m.setValue)
            return false;
        if (!validateSettingInput(m, value)) {
            nhlog::ui()->warn("Ignoring invalid settings input (setting_id={}, type={}, role={})",
                              static_cast<int>(m.settingId),
                              m.type,
                              role);
            return false;
        }
        return m.setValue(value);
    }

    if (settings::ui::hasWritableRoleDataForSetting(m.settingId, role)) {
        if (!settings::ui::validateRoleInput(m.settingId, role, value)) {
            nhlog::ui()->warn(
              "Ignoring invalid settings role input (setting_id={}, type={}, role={})",
              static_cast<int>(m.settingId),
              m.type,
              role);
            return false;
        }
    }

    if (settings::ui::hasWritableRoleDataForSetting(m.settingId, role))
        return settings::ui::setRoleDataForSetting(m.settingId, role, value);

    return false;
}

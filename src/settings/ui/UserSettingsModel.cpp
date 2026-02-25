// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSortFilterProxyModel>

#include "Logging.h"
#include "settings/ui/SessionKeyActions.h"
#include "settings/ui/SettingDescriptor.h"
#include "settings/ui/SettingInputValidation.h"
#include "settings/ui/SettingRoleData.h"
#include "settings/ui/UserSettingsModel.h"
#include "settings/ui/facade/UserSettingsPage.h"

/**
 * UserSettingsModel is a UI adapter: it exposes settings metadata through roles,
 * groups by tab, and translates UI edits into `UserSettings` mutations.
 *
 * Storage/load semantics are implemented in `UserSettings` and `settings::*`
 * modules; this file intentionally contains list-model and delegate-facing
 * behavior only.
 */
QHash<int, QByteArray>
UserSettingsModel::roleNames() const
{
    static QHash<int, QByteArray> roles{
      {Name, "name"},
      {Description, "description"},
      {Value, "value"},
      {Type, "type"},
      {ValueLowerBound, "valueLowerBound"},
      {ValueUpperBound, "valueUpperBound"},
      {ValueStep, "valueStep"},
      {Values, "values"},
      {Good, "good"},
      {Enabled, "enabled"},
      {ThemeVariantValue, "themeVariantValue"},
      {ThemeVariantValues, "themeVariantValues"},
      {Tab, "tab"},
    };

    return roles;
}

int
UserSettingsModel::rowCount(const QModelIndex &parent) const
{
    (void)parent;
    return settings::ui::settingsTableRowCount();
}

QObject *
UserSettingsModel::modelForTab(int tab) const
{
    auto it = filteredModels_.find(tab);
    if (it != filteredModels_.end())
        return it.value();

    auto *proxyModel = new QSortFilterProxyModel(const_cast<UserSettingsModel *>(this));
    proxyModel->setSourceModel(const_cast<UserSettingsModel *>(this));
    proxyModel->setFilterRole(Tab);
    proxyModel->setFilterRegularExpression(QStringLiteral("^%1$").arg(tab));
    filteredModels_.insert(tab, proxyModel);

    return proxyModel;
}

using settings::ui::rowForSettingId;
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
    case Type:
        return m.type;
    case Tab:
        return m.tab;
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
        if (!validateRoleInput(m, role, value)) {
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

void
UserSettingsModel::importSessionKeys()
{
    settings::ui::importSessionKeys();
}
void
UserSettingsModel::exportSessionKeys()
{
    settings::ui::exportSessionKeys();
}
void
UserSettingsModel::requestCrossSigningSecrets()
{
    settings::ui::requestCrossSigningSecrets();
}
void
UserSettingsModel::downloadCrossSigningSecrets()
{
    settings::ui::downloadCrossSigningSecrets();
}

UserSettingsModel::UserSettingsModel(QObject *p)
  : QAbstractListModel(p)
{
    wireSettingConnections(UserSettings::instance().get());
}

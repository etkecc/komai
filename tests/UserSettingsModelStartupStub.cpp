// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QHash>

#include "settings/ui/SettingDescriptor.h"
#include "settings/ui/UserSettingsModel.h"
#include "settings/ui/facade/UserSettingsPage.h"

UserSettingsModel::UserSettingsModel(QObject *parent)
  : QAbstractListModel(parent)
{
}

QHash<int, QByteArray>
UserSettingsModel::roleNames() const
{
    return {
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
      {SettingImage, "settingImage"},
      {Tab, "tab"},
    };
}

int
UserSettingsModel::rowCount(const QModelIndex &) const
{
    return 0;
}

QVariant
UserSettingsModel::data(const QModelIndex &, int) const
{
    return {};
}

bool
UserSettingsModel::setData(const QModelIndex &, const QVariant &, int)
{
    return false;
}

QObject *
UserSettingsModel::modelForTab(int) const
{
    return nullptr;
}

void
UserSettingsModel::importSessionKeys()
{
}

void
UserSettingsModel::exportSessionKeys()
{
}

void
UserSettingsModel::requestCrossSigningSecrets()
{
}

void
UserSettingsModel::downloadCrossSigningSecrets()
{
}

namespace settings::ui {

const SettingMeta settingsTable[] = {
  {nullptr,
   nullptr,
   0,
   0,
   nullptr,
   nullptr,
   {},
   {},
   {},
   nullptr,
   nullptr,
   settings::core::SettingId::Unknown},
};

int
settingsTableRowCount()
{
    return 0;
}

int
rowForSettingId(settings::core::SettingId)
{
    return -1;
}

} // namespace settings::ui

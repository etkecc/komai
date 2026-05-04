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
      {Enabled, "enabled"},
      {ThemeVariantValue, "themeVariantValue"},
      {ThemeVariantValues, "themeVariantValues"},
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
UserSettingsModel::setSearchQuery(const QString &query)
{
    if (query == searchQuery_)
        return;
    searchQuery_ = query;
    emit searchQueryChanged();
}

int
UserSettingsModel::matchCountForTab(int) const
{
    return 0;
}

bool
UserSettingsModel::tabHasCustomMatches(int) const
{
    return false;
}

bool
UserSettingsModel::customSectionMatches(int, const QString &) const
{
    return true;
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

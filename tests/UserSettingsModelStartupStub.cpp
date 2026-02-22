// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QHash>

#include "UserSettingsPage.h"

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

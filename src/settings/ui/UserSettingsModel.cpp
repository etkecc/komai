// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSortFilterProxyModel>

#include "encryption/Olm.h"
#include "settings/ui/SessionKeyActions.h"
#include "settings/ui/SettingDescriptor.h"
#include "settings/ui/UserSettingsModel.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "utils/Utils.h"
#include "voip/CallDevices.h"

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
      {Icon, "icon"},
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
      {TagId, "tagId"},
    };

    return roles;
}

int
UserSettingsModel::rowCount(const QModelIndex &parent) const
{
    (void)parent;
    settings::ui::validateSettingsTable();
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

QString
UserSettingsModel::deviceFingerprint() const
{
    auto fingerprint = utils::humanReadableFingerprint(olm::client()->identity_keys().ed25519);
    fingerprint.replace(u'\n', u' ');
    return fingerprint.simplified();
}

UserSettingsModel::UserSettingsModel(QObject *p)
  : QAbstractListModel(p)
{
    wireSettingConnections(UserSettings::instance().get());

    connect(&CallDevices::instance(), &CallDevices::devicesChanged, this, [this]() {
        const auto emitCallDeviceRowUpdate = [this](settings::core::SettingId id) {
            const int row = settings::ui::rowForSettingId(id);
            if (row < 0)
                return;

            const QModelIndex idx = index(row, 0);
            emit dataChanged(idx, idx, {Value, Values});
        };

        emitCallDeviceRowUpdate(settings::core::SettingId::CallsDevicesMicrophone);
        emitCallDeviceRowUpdate(settings::core::SettingId::CallsDevicesCamera);
        emitCallDeviceRowUpdate(settings::core::SettingId::CallsDevicesCameraResolution);
        emitCallDeviceRowUpdate(settings::core::SettingId::CallsDevicesCameraFrameRate);
    });
}

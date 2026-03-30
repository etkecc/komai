// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/UsersModel.h"

#include <QUrl>

#include "models/CompletionModelRoles.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "utils/Utils.h"

UsersModel::UsersModel(const std::string &roomId, QObject *parent)
  : QAbstractListModel(parent)
  , room_id(roomId)
{
    Q_UNUSED(room_id)
}

QHash<int, QByteArray>
UsersModel::roleNames() const
{
    return {
      {CompletionModel::CompletionRole, "completionRole"},
      {CompletionModel::SearchRole, "searchRole"},
      {CompletionModel::SearchRole2, "searchRole2"},
      {Roles::DisplayName, "displayName"},
      {Roles::AvatarUrl, "avatarUrl"},
      {Roles::UserID, "userid"},
    };
}

QVariant
UsersModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        switch (role) {
        case CompletionModel::CompletionRole:
            if (userids[index.row()] == QStringLiteral("@room"))
                return QStringLiteral("@room");
            if (UserSettings::instance()->composerInputMarkdownToHtmlEnabled())
                return QStringLiteral("[%1](https://matrix.to/#/%2)")
                  .arg(utils::escapeMentionMarkdown(QString(displayNames[index.row()])),
                       QString(QUrl::toPercentEncoding(userids[index.row()])));
            else
                return displayNames[index.row()];
        case CompletionModel::SearchRole:
            return displayNames[index.row()];
        case Qt::DisplayRole:
        case Roles::DisplayName:
            return displayNames[index.row()].toHtmlEscaped();
        case CompletionModel::SearchRole2:
            return userids[index.row()];
        case Roles::AvatarUrl:
            return avatarUrls[index.row()];
        case Roles::UserID:
            return userids[index.row()].toHtmlEscaped();
        }
    }
    return {};
}

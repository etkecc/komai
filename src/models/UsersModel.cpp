// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/UsersModel.h"

#include <QCoreApplication>
#include <QPointer>
#include <QUrl>
#include <thread>

#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "models/CompletionModelRoles.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

UsersModel::UsersModel(const std::string &roomId, QObject *parent)
  : QAbstractListModel(parent)
  , room_id(roomId)
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0)
        return;

    QPointer<UsersModel> self(this);
    auto qRoomId = QString::fromStdString(roomId);

    std::thread([self, handleId, qRoomId]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        std::vector<QString> fetchedUserIds;
        std::vector<QString> fetchedDisplayNames;
        std::vector<QString> fetchedAvatarUrls;

        // Add @room as the first entry
        fetchedUserIds.push_back(QStringLiteral("@room"));
        fetchedDisplayNames.push_back(QStringLiteral("@room"));
        fetchedAvatarUrls.push_back(QString());

        QString error;
        if (const auto members = komai::MatrixBackendRuntimeService::fetchRoomMembers(
              context, handleId, qRoomId, &error)) {
            fetchedUserIds.reserve(members->size() + 1);
            fetchedDisplayNames.reserve(members->size() + 1);
            fetchedAvatarUrls.reserve(members->size() + 1);

            for (const auto &member : *members) {
                fetchedUserIds.push_back(member.userId);
                fetchedDisplayNames.push_back(member.displayName);
                fetchedAvatarUrls.push_back(member.avatarUrl);
            }
        }

        auto *app = QCoreApplication::instance();
        if (!app)
            return;

        QMetaObject::invokeMethod(
          app,
          [self,
           ids     = std::move(fetchedUserIds),
           names   = std::move(fetchedDisplayNames),
           avatars = std::move(fetchedAvatarUrls)]() mutable {
              if (!self)
                  return;

              self->beginResetModel();
              self->userids      = std::move(ids);
              self->displayNames = std::move(names);
              self->avatarUrls   = std::move(avatars);
              self->endResetModel();
          },
          Qt::QueuedConnection);
    }).detach();
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

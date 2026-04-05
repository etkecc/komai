// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/UserDirectoryModel.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QPointer>
#include <thread>

#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"

namespace {

uint64_t
matrixBackendHandleId()
{
    const auto *mainWindow = MainWindow::instance();
    return mainWindow ? mainWindow->matrixBackendHandleId() : 0;
}

} // namespace

UserDirectoryModel::UserDirectoryModel(QObject *parent)
  : QAbstractListModel{parent}
{
}

QHash<int, QByteArray>
UserDirectoryModel::roleNames() const
{
    return {
      {Roles::DisplayName, "displayName"},
      {Roles::Mxid, "userid"},
      {Roles::AvatarUrl, "avatarUrl"},
    };
}

void
UserDirectoryModel::setSearchString(const QString &f)
{
    userSearchString_ = f.trimmed();
    searchGeneration_++;

    nhlog::ui()->debug("Received user directory query: {}", userSearchString_.toStdString());

    beginResetModel();
    results_.clear();
    endResetModel();

    const bool wasSearching = searchingUsers_;
    searchingUsers_         = false;
    if (wasSearching)
        emit searchingUsersChanged();

    if (userSearchString_.isEmpty()) {
        nhlog::ui()->debug("Rejecting empty search string");
        canFetchMore_ = false;
    } else {
        canFetchMore_ = true;
    }
}

void
UserDirectoryModel::fetchMore(const QModelIndex &)
{
    if (!canFetchMore_ || searchingUsers_)
        return;

    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        nhlog::ui()->warn(
          "User directory search requires an active matrix-sdk backend runtime handle");
        canFetchMore_ = false;
        return;
    }

    const auto generation = searchGeneration_;
    const auto searchTerm = userSearchString_;

    searchingUsers_ = true;
    canFetchMore_   = false;
    emit searchingUsersChanged();

    QPointer<UserDirectoryModel> guard(this);
    std::thread([guard, handleId, generation, searchTerm]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        const auto users = komai::MatrixBackendRuntimeService::searchUsers(
          context, handleId, searchTerm, searchLimit_, &error);

        QVector<UserDirectoryEntry> entries;
        if (users.has_value()) {
            entries.reserve(users->size());
            for (const auto &user : *users) {
                entries.push_back(UserDirectoryEntry{
                  .displayName = user.displayName,
                  .userId      = user.userId,
                  .avatarUrl   = user.avatarUrl,
                });
            }
        }

        QMetaObject::invokeMethod(QCoreApplication::instance(),
                                  [guard,
                                   generation,
                                   searchTerm,
                                   error,
                                   entries = std::move(entries),
                                   ok      = users.has_value()]() mutable {
                                      if (!guard)
                                          return;

                                      if (ok) {
                                          guard->finishSearch(generation, searchTerm, entries);
                                      } else {
                                          guard->failSearch(generation, searchTerm, error);
                                      }
                                  });
    }).detach();
}

QVariant
UserDirectoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= (int)results_.size() || index.row() < 0)
        return {};
    switch (role) {
    case Roles::DisplayName:
        return results_[index.row()].displayName;
    case Roles::Mxid:
        return results_[index.row()].userId;
    case Roles::AvatarUrl:
        return results_[index.row()].avatarUrl;
    }
    return {};
}

void
UserDirectoryModel::finishSearch(uint64_t generation,
                                 const QString &searchTerm,
                                 const QVector<UserDirectoryEntry> &results)
{
    if (generation != searchGeneration_ || searchTerm != userSearchString_)
        return;

    searchingUsers_ = false;
    emit searchingUsersChanged();

    beginResetModel();
    results_ = results;
    endResetModel();
    canFetchMore_ = false;

    nhlog::ui()->debug("Matrix user directory query '{}' returned {} result(s)",
                       searchTerm.toStdString(),
                       results_.size());
}

void
UserDirectoryModel::failSearch(uint64_t generation,
                               const QString &searchTerm,
                               const QString &errorMessage)
{
    if (generation != searchGeneration_ || searchTerm != userSearchString_)
        return;

    searchingUsers_ = false;
    canFetchMore_   = false;
    emit searchingUsersChanged();

    nhlog::ui()->warn("Matrix user directory query '{}' failed: {}",
                      searchTerm.toStdString(),
                      errorMessage.toStdString());
}

void
UserDirectoryModel::resolveUser(const QString &mxid)
{
    const auto trimmed = mxid.trimmed();
    if (trimmed.isEmpty())
        return;

    const auto handleId = matrixBackendHandleId();
    if (handleId == 0)
        return;

    QPointer<UserDirectoryModel> guard(this);
    std::thread([guard, handleId, mxid = trimmed]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        const auto profile =
          komai::MatrixBackendRuntimeService::fetchUserProfile(context, handleId, mxid);
        if (!guard || !profile.has_value())
            return;

        const auto displayName =
          profile->displayName.trimmed().isEmpty() ? mxid : profile->displayName.trimmed();
        const auto avatarUrl = profile->avatarUrl;

        QMetaObject::invokeMethod(
          QCoreApplication::instance(),
          [guard, mxid, displayName, avatarUrl]() {
              if (!guard)
                  return;

              emit guard->userResolved(mxid, displayName, avatarUrl);
          },
          Qt::QueuedConnection);
    }).detach();
}

#include "moc_UserDirectoryModel.cpp"

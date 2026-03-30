// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/InviteesModel.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QPointer>

#include <thread>

#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"

InviteesModel::InviteesModel(QString roomName, QObject *parent)
  : QAbstractListModel{parent}
  , roomName_{std::move(roomName)}
{
}

void
InviteesModel::addUser(QString mxid, QString displayName, QString avatarUrl)
{
    for (const auto &invitee : std::as_const(invitees_))
        if (invitee->mxid_ == mxid)
            return;

    beginInsertRows(QModelIndex(), invitees_.count(), invitees_.count());

    auto *invitee = new Invitee{mxid, displayName, avatarUrl, this};
    connect(invitee, &Invitee::userInfoLoaded, this, [this, invitee]() {
        const auto row = invitees_.indexOf(invitee);
        if (row >= 0)
            emit dataChanged(index(row), index(row));
    });

    invitees_.push_back(invitee);

    endInsertRows();
    emit countChanged();
}

void
InviteesModel::removeUser(QString mxid)
{
    for (int i = 0; i < invitees_.length(); ++i) {
        if (invitees_[i]->mxid_ == mxid) {
            beginRemoveRows(QModelIndex(), i, i);
            invitees_.removeAt(i);
            endRemoveRows();
            emit countChanged();
            break;
        }
    }
}

bool
InviteesModel::containsUser(const QString &mxid) const
{
    for (const auto &invitee : invitees_)
        if (invitee->mxid_ == mxid)
            return true;

    return false;
}

QHash<int, QByteArray>
InviteesModel::roleNames() const
{
    return {{Mxid, "mxid"}, {DisplayName, "displayName"}, {AvatarUrl, "avatarUrl"}};
}

QVariant
InviteesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= (int)invitees_.size() || index.row() < 0)
        return {};

    switch (role) {
    case Mxid:
        return invitees_[index.row()]->mxid_;
    case DisplayName:
        return invitees_[index.row()]->displayName_;
    case AvatarUrl:
        return invitees_[index.row()]->avatarUrl_;
    default:
        return {};
    }
}

QStringList
InviteesModel::mxids()
{
    QStringList mxidList;
    mxidList.reserve(invitees_.size());
    for (auto &invitee : std::as_const(invitees_))
        mxidList.push_back(invitee->mxid_);
    return mxidList;
}

Invitee::Invitee(QString mxid, QString displayName, QString avatarUrl, QObject *parent)
  : QObject{parent}
  , mxid_{std::move(mxid)}
{
    if (!displayName.isEmpty() || !avatarUrl.isEmpty()) {
        displayName_ = displayName;
        avatarUrl_   = avatarUrl;
        emit userInfoLoaded();
        return;
    }

    displayName_ = mxid_;
    emit userInfoLoaded();

    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() == 0)
        return;

    const auto handleId = mainWindow->matrixBackendHandleId();
    QPointer<Invitee> guard(this);
    std::thread([guard, handleId, mxid = mxid_]() mutable {
        const auto context = komai::matrix_backend::blockingCallContext();
        const auto profile =
          komai::MatrixBackendRuntimeService::fetchUserProfile(context, handleId, mxid);
        if (!guard || !profile.has_value())
            return;

        const auto displayName =
          profile->displayName.trimmed().isEmpty() ? mxid : profile->displayName.trimmed();
        const auto avatarUrl = profile->avatarUrl;

        auto *app = QCoreApplication::instance();
        if (!app) {
            guard->displayName_ = displayName;
            guard->avatarUrl_   = avatarUrl;
            emit guard->userInfoLoaded();
            return;
        }

        QMetaObject::invokeMethod(
          app,
          [guard, displayName, avatarUrl]() {
              if (!guard)
                  return;

              guard->displayName_ = displayName;
              guard->avatarUrl_   = avatarUrl;
              emit guard->userInfoLoaded();
          },
          Qt::QueuedConnection);
    }).detach();
}

#include "moc_InviteesModel.cpp"

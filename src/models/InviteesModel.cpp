// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/InviteesModel.h"

#include "logging/Logging.h"

InviteesModel::InviteesModel(QObject *room, QObject *parent)
  : QAbstractListModel{parent}
  , room_{room}
{
}

void
InviteesModel::addUser(QString mxid, QString displayName, QString avatarUrl)
{
    for (const auto &invitee : std::as_const(invitees_))
        if (invitee->mxid_ == mxid)
            return;

    beginInsertRows(QModelIndex(), invitees_.count(), invitees_.count());

    auto invitee        = new Invitee{mxid, displayName, avatarUrl, this};
    auto indexOfInvitee = invitees_.count();
    connect(invitee, &Invitee::userInfoLoaded, this, [this, indexOfInvitee]() {
        emit dataChanged(index(indexOfInvitee), index(indexOfInvitee));
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
    // checking for empty avatarUrl will cause profiles that don't have an avatar
    // to needlessly be loaded. Can we make sure we either provide both or none?
    if (displayName == "" && avatarUrl == "") {
        nhlog::ui()->debug("Invitee profile lookup for '{}' is not migrated to matrix-sdk yet",
                           mxid_.toStdString());
        displayName_ = mxid_;
        emit userInfoLoaded();
    } else {
        displayName_ = displayName;
        avatarUrl_   = avatarUrl;
        emit userInfoLoaded();
    }
}

#include "moc_InviteesModel.cpp"

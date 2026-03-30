// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/MemberList.h"

#include <QCoreApplication>
#include <QPointer>
#include <thread>

#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "timeline/DirectChatResolver.h"
#include "ui/MainWindow.h"

MemberListBackend::MemberListBackend(const QString &room_id, QObject *parent)
  : QAbstractListModel{parent}
  , room_id_{room_id}
{
    roomName_ = room_id_;

    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0)
        return;

    loadingMoreMembers_ = true;
    emit loadingMoreMembersChanged();

    QPointer<MemberListBackend> self(this);
    std::thread([self, handleId, roomId = room_id_]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString roomName   = roomId;
        QString avatarUrl;
        int memberCount = 0;
        QVector<MemberListBackend::MemberEntry> members;

        QString error;
        if (const auto settings = komai::MatrixBackendRuntimeService::fetchRoomSettings(
              context, handleId, roomId, &error)) {
            if (!settings->roomName.trimmed().isEmpty())
                roomName = settings->roomName;
            avatarUrl   = settings->roomAvatarUrl;
            memberCount = static_cast<int>(settings->memberCount);
        } else if (!error.isEmpty()) {
            nhlog::ui()->warn("Failed to load matrix-sdk room settings for members tab '{}': {}",
                              roomId.toStdString(),
                              error.toStdString());
        }

        error.clear();
        if (const auto runtimeMembers = komai::MatrixBackendRuntimeService::fetchRoomMembers(
              context, handleId, roomId, &error)) {
            members.reserve(runtimeMembers->size());
            for (const auto &member : *runtimeMembers) {
                members.push_back({
                  .userId      = member.userId,
                  .displayName = member.displayName,
                  .avatarUrl   = member.avatarUrl,
                  .powerLevel  = member.powerLevel,
                });
            }
            if (memberCount <= 0)
                memberCount = members.size();
        } else if (!error.isEmpty()) {
            nhlog::ui()->warn("Failed to load matrix-sdk room members for '{}': {}",
                              roomId.toStdString(),
                              error.toStdString());
        }

        auto *app = QCoreApplication::instance();
        if (!app)
            return;

        QMetaObject::invokeMethod(
          app,
          [self,
           roomName  = std::move(roomName),
           avatarUrl = std::move(avatarUrl),
           memberCount,
           members = std::move(members)]() mutable {
              if (!self)
                  return;

              self->setRoomInfo(roomName, avatarUrl, memberCount);
              self->setMembers(std::move(members), memberCount);
          },
          Qt::QueuedConnection);
    }).detach();
}

// Use the DM-aware display name so the Members tab header matches the room
// list and room header (e.g. showing "Someone" instead of "Someone and
// Messenger bridge bot" for bridged DM rooms).
QString
MemberListBackend::roomName() const
{
    auto dmName = DirectChatResolver::instance().dmRoomDisplayName(room_id_);
    if (!dmName.isEmpty())
        return dmName;
    if (!roomName_.isEmpty())
        return roomName_;
    return room_id_;
}

void
MemberListBackend::setRoomInfo(const QString &roomName, const QString &avatarUrl, int memberCount)
{
    const bool roomNameChangedNow    = roomName_ != roomName;
    const bool avatarChangedNow      = avatarUrl_ != avatarUrl;
    const bool memberCountChangedNow = memberCount_ != memberCount;

    roomName_    = roomName;
    avatarUrl_   = avatarUrl;
    memberCount_ = memberCount;

    if (roomNameChangedNow)
        emit roomNameChanged();
    if (avatarChangedNow)
        emit avatarUrlChanged();
    if (memberCountChangedNow)
        emit memberCountChanged();
}

void
MemberListBackend::setMembers(QVector<MemberEntry> members, int memberCount)
{
    beginResetModel();
    m_memberList        = std::move(members);
    numUsersLoaded_     = m_memberList.size();
    loadingMoreMembers_ = false;
    if (memberCount <= 0)
        memberCount_ = m_memberList.size();
    endResetModel();

    emit numUsersLoadedChanged();
    emit loadingMoreMembersChanged();
    emit memberCountChanged();
}

QHash<int, QByteArray>
MemberListBackend::roleNames() const
{
    return {
      {Mxid, "mxid"},
      {DisplayName, "displayName"},
      {AvatarUrl, "avatarUrl"},
      {Trustlevel, "trustlevel"},
      {Powerlevel, "powerlevel"},
    };
}

QVariant
MemberListBackend::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= (int)m_memberList.size() || index.row() < 0)
        return {};

    switch (role) {
    case Mxid:
        return m_memberList[index.row()].userId;
    case DisplayName:
        return m_memberList[index.row()].displayName;
    case AvatarUrl:
        return m_memberList[index.row()].avatarUrl;
    case Trustlevel:
        return 0;
    case Powerlevel:
        return m_memberList[index.row()].powerLevel;
    default:
        return {};
    }
}

bool
MemberListBackend::canFetchMore(const QModelIndex &) const
{
    return false;
}

void
MemberListBackend::fetchMore(const QModelIndex &)
{
    // Member paging still depends on the removed cache layer.
}

MemberList::MemberList(const QString &room_id, QObject *parent)
  : QSortFilterProxyModel{parent}
  , m_model{room_id, this}
{
    connect(&m_model, &MemberListBackend::roomNameChanged, this, &MemberList::roomNameChanged);
    connect(
      &m_model, &MemberListBackend::memberCountChanged, this, &MemberList::memberCountChanged);
    connect(&m_model, &MemberListBackend::avatarUrlChanged, this, &MemberList::avatarUrlChanged);
    connect(&m_model, &MemberListBackend::roomIdChanged, this, &MemberList::roomIdChanged);
    connect(&m_model,
            &MemberListBackend::numUsersLoadedChanged,
            this,
            &MemberList::numUsersLoadedChanged);
    connect(&m_model,
            &MemberListBackend::loadingMoreMembersChanged,
            this,
            &MemberList::loadingMoreMembersChanged);

    setSourceModel(&m_model);
    setSortRole(MemberSortRoles::PowerlevelThenName);
    sort(0, Qt::DescendingOrder);
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void
MemberList::setFilterString(const QString &text)
{
    filterString = text;
    setFilterFixedString(text);
}

void
MemberList::sortBy(const MemberSortRoles role)
{
    currentSortRole_ = role;
    setSortRole(role == MemberSortRoles::PowerlevelThenName ? MemberSortRoles::Powerlevel : role);
    const bool descending =
      (role == MemberSortRoles::Powerlevel || role == MemberSortRoles::PowerlevelThenName);
    sort(0, descending ? Qt::DescendingOrder : Qt::AscendingOrder);
}

bool
MemberList::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    if (currentSortRole_ == MemberSortRoles::PowerlevelThenName) {
        const auto plLeft =
          sourceModel()->data(source_left, MemberListBackend::Powerlevel).toLongLong();
        const auto plRight =
          sourceModel()->data(source_right, MemberListBackend::Powerlevel).toLongLong();
        if (plLeft != plRight)
            return plLeft < plRight; // DescendingOrder reverses this, so higher PL comes first
        // Same power level: sort by display name ascending, then by mxid.
        // Since Qt applies DescendingOrder, we reverse the comparisons here.
        const auto nameLeft =
          sourceModel()->data(source_left, MemberListBackend::DisplayName).toString().toLower();
        const auto nameRight =
          sourceModel()->data(source_right, MemberListBackend::DisplayName).toString().toLower();
        if (nameLeft != nameRight)
            return nameLeft > nameRight; // reversed because DescendingOrder
        const auto mxidLeft =
          sourceModel()->data(source_left, MemberListBackend::Mxid).toString().toLower();
        const auto mxidRight =
          sourceModel()->data(source_right, MemberListBackend::Mxid).toString().toLower();
        return mxidLeft > mxidRight; // reversed because DescendingOrder
    }
    return QSortFilterProxyModel::lessThan(source_left, source_right);
}

bool
MemberList::filterAcceptsRow(int source_row, const QModelIndex &) const
{
    return m_model.m_memberList[source_row].userId.contains(filterString, Qt::CaseInsensitive) ||
           m_model.m_memberList[source_row].displayName.contains(filterString, Qt::CaseInsensitive);
}

#include "moc_MemberList.cpp"

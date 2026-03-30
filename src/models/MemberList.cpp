// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/MemberList.h"

#include "timeline/DirectChatResolver.h"

MemberListBackend::MemberListBackend(const QString &room_id, QObject *parent)
  : QAbstractListModel{parent}
  , room_id_{room_id}
{
    info_.name = room_id_.toStdString();
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
    if (!info_.name.empty())
        return QString::fromStdString(info_.name);
    return room_id_;
}

void
MemberListBackend::addUsers(const std::vector<RoomMember> &members)
{
    if (members.empty())
        return;

    beginInsertRows(
      QModelIndex{}, m_memberList.count(), m_memberList.count() + (int)members.size() - 1);

    for (const auto &member : members)
        m_memberList.push_back({member, member.avatar_url});

    endInsertRows();
    info_.member_count = static_cast<size_t>(m_memberList.size());
    numUsersLoaded_    = m_memberList.size();
    emit memberCountChanged();
    emit numUsersLoadedChanged();
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
        return m_memberList[index.row()].first.user_id;
    case DisplayName:
        return m_memberList[index.row()].first.display_name;
    case AvatarUrl:
        return m_memberList[index.row()].second;
    case Trustlevel:
        return 0;
    case Powerlevel:
        return static_cast<qlonglong>(komai::matrix::effectiveUserPowerLevel(
          powerLevels_, create_, m_memberList[index.row()].first.user_id.toStdString()));
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
    return m_model.m_memberList[source_row].first.user_id.contains(filterString,
                                                                   Qt::CaseInsensitive) ||
           m_model.m_memberList[source_row].first.display_name.contains(filterString,
                                                                        Qt::CaseInsensitive);
}

#include "moc_MemberList.cpp"

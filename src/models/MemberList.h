// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QSortFilterProxyModel>

class MemberListBackend final : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QString roomName READ roomName NOTIFY roomNameChanged)
    Q_PROPERTY(int memberCount READ memberCount NOTIFY memberCountChanged)
    Q_PROPERTY(QString avatarUrl READ avatarUrl NOTIFY avatarUrlChanged)
    Q_PROPERTY(QString roomId READ roomId NOTIFY roomIdChanged)
    Q_PROPERTY(int numUsersLoaded READ numUsersLoaded NOTIFY numUsersLoadedChanged)
    Q_PROPERTY(bool loadingMoreMembers READ loadingMoreMembers NOTIFY loadingMoreMembersChanged)

public:
    enum Roles
    {
        Mxid,
        DisplayName,
        AvatarUrl,
        Trustlevel,
        Powerlevel,
        IsCreator,
    };

    MemberListBackend(const QString &room_id, QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        Q_UNUSED(parent)
        return static_cast<int>(m_memberList.size());
    }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QString roomName() const;
    int memberCount() const { return memberCount_; }
    QString avatarUrl() const { return avatarUrl_; }
    QString roomId() const { return room_id_; }
    int numUsersLoaded() const { return numUsersLoaded_; }
    bool loadingMoreMembers() const { return loadingMoreMembers_; }

signals:
    void roomNameChanged();
    void memberCountChanged();
    void avatarUrlChanged();
    void roomIdChanged();
    void numUsersLoadedChanged();
    void loadingMoreMembersChanged();

protected:
    bool canFetchMore(const QModelIndex &) const override;
    void fetchMore(const QModelIndex &) override;

private:
    struct MemberEntry
    {
        QString userId;
        QString displayName;
        QString avatarUrl;
        qlonglong powerLevel = 0;
    };

    void setRoomInfo(const QString &roomName, const QString &avatarUrl, int memberCount);
    void setMembers(QVector<MemberEntry> members, int memberCount);

    QVector<MemberEntry> m_memberList;
    QString room_id_;
    QString roomName_;
    QString avatarUrl_;
    int memberCount_ = 0;
    int numUsersLoaded_{0};
    bool loadingMoreMembers_{false};

    friend class MemberList;
};

class MemberList final : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    Q_PROPERTY(QString roomName READ roomName NOTIFY roomNameChanged)
    Q_PROPERTY(int memberCount READ memberCount NOTIFY memberCountChanged)
    Q_PROPERTY(QString avatarUrl READ avatarUrl NOTIFY avatarUrlChanged)
    Q_PROPERTY(QString roomId READ roomId NOTIFY roomIdChanged)
    Q_PROPERTY(int numUsersLoaded READ numUsersLoaded NOTIFY numUsersLoadedChanged)
    Q_PROPERTY(bool loadingMoreMembers READ loadingMoreMembers NOTIFY loadingMoreMembersChanged)

public:
    enum MemberSortRoles
    {
        Mxid        = MemberListBackend::Roles::Mxid,
        DisplayName = MemberListBackend::Roles::DisplayName,
        Powerlevel  = MemberListBackend::Roles::Powerlevel,
        PowerlevelThenName,
    };
    Q_ENUM(MemberSortRoles)

    MemberList(const QString &room_id, QObject *parent = nullptr);

    QString roomName() const { return m_model.roomName(); }
    int memberCount() const { return m_model.memberCount(); }
    QString avatarUrl() const { return m_model.avatarUrl(); }
    QString roomId() const { return m_model.roomId(); }
    int numUsersLoaded() const { return m_model.numUsersLoaded(); }
    bool loadingMoreMembers() const { return m_model.loadingMoreMembers(); }

signals:
    void roomNameChanged();
    void memberCountChanged();
    void avatarUrlChanged();
    void roomIdChanged();
    void numUsersLoadedChanged();
    void loadingMoreMembersChanged();

public slots:
    void setFilterString(const QString &text);
    void sortBy(const MemberSortRoles role);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
    MemberSortRoles currentSortRole_{PowerlevelThenName};
    QString filterString;
    MemberListBackend m_model;
};

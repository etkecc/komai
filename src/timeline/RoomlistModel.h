// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "matrix/MatrixStateTypes.h"
#include "matrix/MatrixSyncUpdate.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include <QAbstractListModel>
#include <QHash>
#include <QQmlEngine>
#include <QSortFilterProxyModel>
#include <QString>
#include <optional>
#include <set>
#include <string>

#ifdef KOMAI_DBUS_SYS
#include "dbus/Backend.h"
#endif

class TimelineViewManager;

namespace komai::ipc {
struct RoomInfo;
QVector<RoomInfo>
roomList();
} // namespace komai::ipc

class RoomPreview
{
    Q_GADGET
    Q_PROPERTY(QString roomid READ roomid CONSTANT)
    Q_PROPERTY(QString roomName READ roomName CONSTANT)
    Q_PROPERTY(QString roomTopic READ roomTopic CONSTANT)
    Q_PROPERTY(QString roomAvatarUrl READ roomAvatarUrl CONSTANT)
    Q_PROPERTY(QString directChatOtherUserId READ directChatOtherUserId CONSTANT)
    Q_PROPERTY(QString reason READ reason CONSTANT)
    Q_PROPERTY(QString inviterAvatarUrl READ inviterAvatarUrl CONSTANT)
    Q_PROPERTY(QString inviterDisplayName READ inviterDisplayName CONSTANT)
    Q_PROPERTY(QString inviterUserId READ inviterUserId CONSTANT)
    Q_PROPERTY(bool isDirect READ isDirect CONSTANT)
    Q_PROPERTY(bool isEncrypted READ isEncrypted CONSTANT)
    Q_PROPERTY(bool isPublic READ isPublic CONSTANT)
    Q_PROPERTY(bool isSpace READ isSpace CONSTANT)
    Q_PROPERTY(bool isInvite READ isInvite CONSTANT)
    Q_PROPERTY(bool isFetched READ isFetched CONSTANT)
    Q_PROPERTY(bool canJoin READ canJoin CONSTANT)
    Q_PROPERTY(bool isMatrixSummary READ isMatrixSummary CONSTANT)
    Q_PROPERTY(int memberCount READ memberCount CONSTANT)
    Q_PROPERTY(int roomMemberCount READ roomMemberCount CONSTANT)

public:
    RoomPreview() {}

    QString roomid() const { return roomid_; }
    QString roomName() const { return roomName_; }
    QString roomTopic() const { return roomTopic_; }
    QString roomAvatarUrl() const { return roomAvatarUrl_; }
    QString directChatOtherUserId() const { return directChatOtherUserId_; }
    QString reason() const { return reason_; }
    QString inviterAvatarUrl() const;
    QString inviterDisplayName() const;
    QString inviterUserId() const;
    bool isDirect() const { return isDirect_; }
    bool isEncrypted() const { return isEncrypted_; }
    bool isPublic() const { return isPublic_; }
    bool isSpace() const { return isSpace_; }
    bool isInvite() const { return isInvite_; }
    bool isFetched() const { return isFetched_; }
    bool canJoin() const { return canJoin_; }
    bool isMatrixSummary() const { return isMatrixSummary_; }
    int memberCount() const { return memberCount_; }
    int roomMemberCount() const { return memberCount_; }

    QString roomid_, roomName_, roomAvatarUrl_, roomTopic_, directChatOtherUserId_, reason_;
    int memberCount_ = 0;
    bool isDirect_ = false, isEncrypted_ = false, isPublic_ = true, isSpace_ = false;
    bool isInvite_ = false, isFetched_ = true, canJoin_ = false, isMatrixSummary_ = false;
};

class RoomlistModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(
      QString currentRoomId READ currentRoomId NOTIFY currentRoomIdChanged RESET resetCurrentRoom)
    Q_PROPERTY(RoomPreview currentRoomPreview READ currentRoomPreview NOTIFY
                 currentRoomPreviewChanged RESET resetCurrentRoom)
public:
    enum Roles
    {
        AvatarUrl = Qt::UserRole,
        RoomName,
        RoomId,
        LastMessage,
        Time,
        Timestamp,
        HasUnreadMessages,
        HasLoudNotification,
        NotificationCount,
        HasDraft,
        DraftPreview,
        IsInvite,
        IsSpace,
        IsPreview,
        IsPreviewFetched,
        Tags,
        ParentSpaces,
        IsDirect,
        DirectChatOtherUserId,
        IsBotRoom,
        IsEncrypted,
    };

    RoomlistModel(TimelineViewManager *parent = nullptr);
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        (void)parent;
        return (int)roomids.size();
    }
    QVariant data(const QModelIndex &index, int role) const override;
    RoomPreview getRoomPreviewById(QString roomid) const;
    QString currentRoomId() const;

    void refetchOnlineKeyBackupKeys();
    void clearDecryptionErrors();

    const QHash<QString, komai::MatrixRoomSummary> &matrixJoinedRooms() const
    {
        return matrixJoinedRooms_;
    }

    RoomPreview currentRoomPreview() const { return currentRoomPreview_.value_or(RoomPreview{}); }

public slots:
    void initializeRooms();
    void sync(const komai::SyncUpdate &sync);
    void clear();
    int roomidToIndex(const QString &roomid)
    {
        for (int i = 0; i < (int)roomids.size(); i++) {
            if (roomids[i] == roomid)
                return i;
        }

        return -1;
    }
    void joinPreview(const QString &roomid);
    void
    scheduleRoomPrewarm(const QString &roomid, const QString &trigger = QStringLiteral("manual"));
    void cancelRoomPrewarm(const QString &roomid,
                           const QString &trigger = QStringLiteral("manual"),
                           const QString &reason  = QStringLiteral("unspecified"));
    void prewarmRoom(const QString &roomid, const QString &trigger = QStringLiteral("manual"));
    void acceptInvite(QString roomid);
    void declineInvite(QString roomid);
    void leave(QString roomid, QString reason = "");
#ifdef KOMAI_DBUS_SYS
    void setDbusInterfaceEnabled(bool enabled);
#endif
    void setCurrentRoom(const QString &roomid);
    void resumeDeferredStartupCurrentRoomRestore();
    void resetCurrentRoom()
    {
        allowDeferredStartupCurrentRoomRestore_ = false;
        deferredStartupCurrentRoomId_.clear();
        pendingCurrentRoomId_.clear();
        currentRoomPreview_.reset();
        UserSettings::instance()->setCurrentRoomId(QString());
        notifyCurrentRoomIdChanged();
        scheduleCurrentRoomVisualStateChanged();
    }

private slots:
    void updateReadStatus(const std::map<QString, bool> &roomReadStatus_);

signals:
    void totalUnreadMessageCountUpdated(int unreadMessages);
    void currentRoomIdChanged(QString currentRoomId);
    void currentRoomPreviewChanged();
    void fetchedPreview(QString roomid, RoomInfo info);
    void spaceSelected(QString roomId);

private:
    std::optional<QVariant> commonRoomData(const QString &room_id, int role) const;
    QVariant
    dataForMatrixRoom(const QString &room_id, const komai::MatrixRoomSummary &room, int role) const;
    QVariant dataForInviteRoom(const RoomInfo &room, int role) const;
    QVariant dataForPreviewRoom(const RoomInfo &room, int role) const;
    QVariant dataForUnavailablePreview(int role) const;
    void resetRoomCollections(bool clearAllDrafts);
    void removeRoomState(const QString &room_id, bool clearDraftForRoom = true);
    void logRoomPrewarm(const QString &trigger,
                        const QString &roomid,
                        const QString &action,
                        const QString &reason = QString()) const;
    void emitCurrentRoomVisualStateChanged();
    void notifyCurrentRoomIdChanged();
    void scheduleCurrentRoomVisualStateChanged();
    void deferCurrentRoomVisualState(const QString &roomId);
    void flushDeferredCurrentRoomVisualState(const QString &roomId);
    void syncJoinedRoom(const komai::JoinedRoomSyncUpdate &roomUpdate);
    void syncLeftRoom(const QString &roomId);
    void syncInvitedRoom(const QString &roomId);
    void emitRoomRowUpdate(const QString &room_id);
    static bool isCachedEncryptedPreview(const QString &room_id, const DescInfo &description);
    bool isCurrentRoomSelection(const QString &roomid) const;
    void clearCurrentRoomSelection();
    bool trySelectCurrentMatrixSummaryRoom(const QString &roomid);
    bool trySelectCurrentPreviewRoom(const QString &roomid);
    void deferStartupCurrentRoomRestore(const QString &roomid);
    void deferCurrentRoomSelection(const QString &roomid);
    QString draftPreviewText(const QString &room_id) const;
    bool hasDraft(const QString &room_id) const;
    void persistDraftForRoom(const QString &room_id, const QString &draftText);
    void fetchPreviews(QString roomid, const std::string &from = "");
    void refreshMatrixBackendRooms();
    TimelineViewManager *manager = nullptr;
    std::vector<QString> roomids;
    QHash<QString, RoomInfo> invites;
    QHash<QString, komai::MatrixRoomSummary> matrixJoinedRooms_;
    std::map<QString, bool> roomReadStatus;
    QHash<QString, std::optional<RoomInfo>> previewedRooms;

    std::optional<RoomPreview> currentRoomPreview_;
    quint64 currentRoomVisualStateGeneration_ = 0;
    bool currentRoomVisualStateDeferred_      = false;
    QString currentRoomVisualStateDeferredRoomId_;
    QString deferredStartupCurrentRoomId_;
    bool allowDeferredStartupCurrentRoomRestore_ = false;
    // When UI requests opening a room before sync inserts it into the room summary list,
    // remember the target and switch once the room becomes available.
    QString pendingCurrentRoomId_;

#ifdef KOMAI_DBUS_SYS
    DbusHost *dbusHost_ = nullptr;
    friend class DbusRoomsInterface;
#endif

    friend class TimelineViewManager;
    friend QVector<komai::ipc::RoomInfo> komai::ipc::roomList();
    friend class FilteredRoomlistModel;
};

class FilteredRoomlistModel final : public QSortFilterProxyModel
{
    Q_OBJECT

    QML_NAMED_ELEMENT(Rooms)
    QML_SINGLETON

    Q_PROPERTY(
      QString currentRoomId READ currentRoomId NOTIFY currentRoomIdChanged RESET resetCurrentRoom)
    Q_PROPERTY(RoomPreview currentRoomPreview READ currentRoomPreview NOTIFY
                 currentRoomPreviewChanged RESET resetCurrentRoom)
public:
    FilteredRoomlistModel(RoomlistModel *model, QObject *parent = nullptr);

    static FilteredRoomlistModel *create(QQmlEngine *qmlEngine, QJSEngine *);

    static FilteredRoomlistModel *instance() { return instance_; }

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsRow(int sourceRow, const QModelIndex &) const override;

    struct FilterBadge
    {
        int unreadCount   = 0;
        bool hasHighlight = false;
    };
    QHash<QString, FilterBadge> computeFilterBadges(const QStringList &communityIds) const;
    QString currentRoomId() const { return roomlistmodel->currentRoomId(); }
    RoomPreview currentRoomPreview() const { return roomlistmodel->currentRoomPreview(); }

public slots:
    int roomidToIndex(QString roomid)
    {
        return mapFromSource(roomlistmodel->index(roomlistmodel->roomidToIndex(roomid))).row();
    }
    QString roomIdAt(int row) const
    {
        if (row < 0 || row >= rowCount())
            return {};

        return data(index(row, 0), RoomlistModel::Roles::RoomId).toString();
    }
    void joinPreview(QString roomid) { roomlistmodel->joinPreview(roomid); }
    void acceptInvite(QString roomid) { roomlistmodel->acceptInvite(roomid); }
    void declineInvite(QString roomid) { roomlistmodel->declineInvite(roomid); }
    void leave(QString roomid, QString reason = "") { roomlistmodel->leave(roomid, reason); }
    void toggleTag(const QString &roomid, const QString &tag, bool on);
    void copyLink(QString roomid);
    void scheduleRoomPrewarm(QString roomid, QString trigger = QStringLiteral("manual"))
    {
        roomlistmodel->scheduleRoomPrewarm(std::move(roomid), std::move(trigger));
    }
    void cancelRoomPrewarm(QString roomid,
                           QString trigger = QStringLiteral("manual"),
                           QString reason  = QStringLiteral("unspecified"))
    {
        roomlistmodel->cancelRoomPrewarm(std::move(roomid), std::move(trigger), std::move(reason));
    }
    void prewarmRoom(QString roomid, QString trigger = QStringLiteral("manual"))
    {
        roomlistmodel->prewarmRoom(std::move(roomid), std::move(trigger));
    }
    void setCurrentRoom(QString roomid) { roomlistmodel->setCurrentRoom(std::move(roomid)); }
    void resetCurrentRoom() { roomlistmodel->resetCurrentRoom(); }
    RoomPreview getRoomPreviewById(QString roomid) const
    {
        return roomlistmodel->getRoomPreviewById(roomid);
    }

    void nextRoomWithActivity();
    void nextRoom();
    void previousRoom();

    void updateFilterTag(QString tagId)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
#endif

        if (tagId.startsWith(QLatin1String("tag:"))) {
            filterType = FilterBy::Tag;
            filterStr  = tagId.mid(4);
        } else if (tagId.startsWith(QLatin1String("space:"))) {
            filterType = FilterBy::Space;
            filterStr  = tagId.mid(6);
            roomlistmodel->fetchPreviews(filterStr);
        } else if (tagId == QLatin1String("people")) {
            filterType = FilterBy::People;
            filterStr.clear();
        } else if (tagId == QLatin1String("bot")) {
            filterType = FilterBy::Bots;
            filterStr.clear();
        } else if (tagId == QLatin1String("group")) {
            filterType = FilterBy::Groups;
            filterStr.clear();
        } else {
            filterType = FilterBy::Nothing;
            filterStr.clear();
        }

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        endFilterChange();
#else
        invalidateFilter();
#endif
    }

    void updateGlobalExcludes();

signals:
    void currentRoomIdChanged(QString currentRoomId);
    void currentRoomPreviewChanged();

private:
    QModelIndex sourceRowIndex(int sourceRow) const;
    bool isPreviewRow(int sourceRow) const;
    bool isSpaceRow(int sourceRow) const;
    bool isDirectRow(int sourceRow) const;
    bool isBotRow(int sourceRow) const;
    QStringList rowTags(int sourceRow) const;
    QStringList rowParentSpaces(int sourceRow) const;
    bool excludedByTags(int sourceRow, const QString &requiredTag = QString()) const;
    bool excludedBySpaces(int sourceRow, const QString &requiredSpace = QString()) const;
    bool excludedByPeople(int sourceRow) const;
    bool excludedByBots(int sourceRow) const;
    bool excludedByGroups(int sourceRow) const;
    short int calculateImportance(const QModelIndex &idx) const;
    RoomlistModel *roomlistmodel;
    int sidebarsRoomListSort = 0; // UserSettings::RoomSortOrder enum value

    enum class FilterBy
    {
        Tag,
        Space,
        People,
        Bots,
        Groups,
        Nothing,
    };
    bool acceptsForFilter(int sourceRow, FilterBy type, const QString &str) const;
    QString filterStr   = QLatin1String("");
    FilterBy filterType = FilterBy::Nothing;
    QStringList globalExcludedTags, globalExcludedSpaces;
    bool excludePeople = false;
    bool excludeBots   = false;
    bool excludeGroups = false;

    inline static FilteredRoomlistModel *instance_ = nullptr;
};

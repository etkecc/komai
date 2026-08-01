// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QQmlEngine>
#include <QSortFilterProxyModel>
#include <QString>
#include <QVariant>
#include <optional>
#include <set>
#include <string>

#include "matrix/MatrixStateTypes.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "settings/ui/facade/UserSettingsPage.h"

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
    QString inviterAvatarUrl_, inviterDisplayName_, inviterUserId_;
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
    // Rooms that currently have a live MatrixRTC (Element Call) session, mapped
    // to their participant count. Lets QML (timeline call tiles, avatar call
    // indicators) react to calls happening in rooms the user has not joined.
    Q_PROPERTY(QVariantMap activeCalls READ activeCalls NOTIFY activeCallsChanged)
public:
    enum Roles
    {
        AvatarUrl = Qt::UserRole,
        RoomName,
        RoomId,
        LastMessage,
        LastMessagePreviewSenderName,
        LastMessagePreviewBody,
        Time,
        Timestamp,
        HasUnreadMessages,
        HasLoudNotification,
        UnreadCount,
        HasDraft,
        DraftPreview,
        HasStaleDraft,
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
        IsMarkedUnread,
        HasActiveCall,
        ActiveCallParticipantCount,
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
    bool hasSuppressedUpdates() const { return hasSuppressedUpdates_; }

    QHash<QString, komai::MatrixRoomSummary> &matrixJoinedRooms() { return matrixJoinedRooms_; }
    const QHash<QString, komai::MatrixRoomSummary> &matrixJoinedRooms() const
    {
        return matrixJoinedRooms_;
    }
    void notifyRoomPreviewsBackfilled();

    RoomPreview currentRoomPreview() const { return currentRoomPreview_.value_or(RoomPreview{}); }

    const QVariantMap &activeCalls() const { return activeCalls_; }

public slots:
    void initializeRooms();
    void clear();
    void setInteractionSuppressed(bool suppressed);
    void
    queueMatrixNotificationFetch(uint64_t handleId, const QString &roomId, const QString &eventId);
    int roomidToIndex(const QString &roomid)
    {
        for (int i = 0; i < (int)roomids.size(); i++) {
            if (roomids[i] == roomid)
                return i;
        }

        return -1;
    }
    void joinPreview(const QString &roomid);
    void acceptInvite(QString roomid);
    void declineInvite(QString roomid);
    void leave(QString roomid, QString reason = "");
#ifdef KOMAI_DBUS_SYS
    void setDbusInterfaceEnabled(bool enabled);
#endif
    void setCurrentRoom(const QString &roomid);
    /// Navigate to a room, joining it first if not already joined.  The
    /// join path already handles M_FORBIDDEN by offering to knock and calls
    /// setCurrentRoom on success, so a single entry point covers both
    /// "already a member, just switch" and "need to ask the server first".
    void openOrJoinRoom(const QString &roomid);
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
    void attentionCountUpdated(int attentionCount);
    void currentRoomIdChanged(QString currentRoomId);
    void currentRoomPreviewChanged();
    void fetchedPreview(QString roomid, RoomInfo info);
    void suppressedUpdatesChanged();
    void spaceSelected(QString roomId);
    void roomLeft(QString roomid);
    void activeCallsChanged();

private:
    // Recompute `activeCalls_` from the current room summaries; emits
    // activeCallsChanged() only when the set/counts actually change.
    void refreshActiveCalls();

    QVariantMap activeCalls_;

    struct AttentionState
    {
        bool hasUnread     = false;
        bool hasStaleDraft = false;
        bool hasHighlight  = false;
        bool isLowPriority = false;

        bool hasAnyAttention() const { return hasUnread || hasStaleDraft || hasHighlight; }
        bool countAsActive(bool includeLowPriorityUnread) const
        {
            if (includeLowPriorityUnread)
                return hasUnread || hasStaleDraft;

            return isLowPriority ? (hasStaleDraft || hasHighlight) : (hasUnread || hasStaleDraft);
        }
    };

    struct GlobalExcludeState
    {
        QStringList tags;
        QStringList spaces;
        bool excludePeople = false;
        bool excludeBots   = false;
        bool excludeGroups = false;
    };

    std::optional<QVariant> commonRoomData(const QString &room_id, int role) const;
    QVariant
    dataForMatrixRoom(const QString &room_id, const komai::MatrixRoomSummary &room, int role) const;
    QVariant dataForInviteRoom(const RoomInfo &room, int role) const;
    QVariant dataForPreviewRoom(const RoomInfo &room, int role) const;
    QVariant dataForUnavailablePreview(int role) const;
    void resetRoomCollections(bool clearAllDrafts);
    void removeRoomState(const QString &room_id, bool clearDraftForRoom = true);
    void emitCurrentRoomVisualStateChanged();
    void notifyCurrentRoomIdChanged();
    void scheduleCurrentRoomVisualStateChanged();
    void deferCurrentRoomVisualState(const QString &roomId);
    void flushDeferredCurrentRoomVisualState(const QString &roomId);
    static bool isCachedEncryptedPreview(const QString &room_id, const DescInfo &description);
    bool isCurrentRoomSelection(const QString &roomid) const;
    void clearCurrentRoomSelection();
    bool trySelectCurrentMatrixSummaryRoom(const QString &roomid);
    bool trySelectCurrentPreviewRoom(const QString &roomid);
    void deferStartupCurrentRoomRestore(const QString &roomid);
    void deferCurrentRoomSelection(const QString &roomid);
    QString draftPreviewText(const QString &room_id) const;
    bool hasDraft(const QString &room_id) const;
    bool hasStaleDraft(const QString &room_id) const;
    void persistDraftForRoom(const QString &room_id, const QString &draftText);
    void fetchPreviews(QString roomid, const std::string &from = "");
    void startMatrixBackendRoomsRefresh(uint64_t handleId);
    void fetchMatrixNotificationBatch(uint64_t handleId,
                                      QVector<komai::MatrixNotificationRequest> requests);
    void applyMatrixBackendRoomsSnapshot(const QVector<komai::MatrixRoomSummary> &roomList);
    bool matrixRoomListSnapshotDiffersFromAppliedState(
      const QVector<komai::MatrixRoomSummary> &roomList) const;
    bool resumeDeferredMatrixRoomRefresh();
    void setHasSuppressedUpdates(bool hasSuppressedUpdates);
    static AttentionState
    attentionStateForRow(const QAbstractItemModel *model, const QModelIndex &idx);
    static GlobalExcludeState globalExcludesFromSettings(const UserSettings &settings);
    bool
    isExcludedFromAllRooms(const QModelIndex &idx, const GlobalExcludeState &globalExcludes) const;
    int computeAttentionCount() const;
    void emitAttentionCount();
    void refreshMatrixBackendRooms();
    TimelineViewManager *manager = nullptr;
    std::vector<QString> roomids;
    QHash<QString, RoomInfo> invites;
    QHash<QString, komai::MatrixRoomSummary> matrixJoinedRooms_;
    std::map<QString, bool> roomReadStatus;
    QHash<QString, std::optional<RoomInfo>> previewedRooms;
    // Timestamp (epoch ms) of when a room's draft first became non-empty.
    // Used to delay counting an unsent draft toward the attention indicators
    // (window title, tray icon, app badge) until it has sat unsent for a
    // while -- typing a message shouldn't immediately look like a new
    // incoming message demanding attention.
    QHash<QString, qint64> draftStartedAtMs_;

    std::optional<RoomPreview> currentRoomPreview_;
    quint64 currentRoomVisualStateGeneration_ = 0;
    bool currentRoomVisualStateDeferred_      = false;
    QString currentRoomVisualStateDeferredRoomId_;
    QString deferredStartupCurrentRoomId_;
    bool allowDeferredStartupCurrentRoomRestore_ = false;
    // Latched true the first time resumeDeferredStartupCurrentRoomRestore()
    // fires (after the first chat frame swaps).  The latch survives the case
    // where frameSwapped beats the matrix-sdk room-list snapshot: a later
    // deferStartupCurrentRoomRestore() then performs the restore immediately
    // instead of waiting for a single-shot signal that has already passed.
    bool firstChatFrameReleased_ = false;
    // When UI requests opening a room before sync inserts it into the room summary list,
    // remember the target and switch once the room becomes available.
    QString pendingCurrentRoomId_;
    QString recentlyAcceptedInviteRoomId_;
    bool interactionSuppressed_     = false;
    bool hasSuppressedUpdates_      = false;
    bool matrixRoomRefreshInFlight_ = false;
    bool matrixRoomRefreshPending_  = false;
    std::optional<QVector<komai::MatrixRoomSummary>> deferredMatrixRoomListSnapshot_;
    bool matrixNotificationFetchQueued_         = false;
    uint64_t pendingMatrixNotificationHandleId_ = 0;
    QHash<QString, komai::MatrixNotificationRequest> pendingMatrixNotificationRequests_;

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
    Q_PROPERTY(
      bool hasSuppressedUpdates READ hasSuppressedUpdates NOTIFY hasSuppressedUpdatesChanged)
    // Forwards the source model's live "rooms with an active call" map
    // (roomId -> participant count) to QML; see RoomlistModel::activeCalls.
    Q_PROPERTY(QVariantMap activeCalls READ activeCalls NOTIFY activeCallsChanged)
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
    bool hasSuppressedUpdates() const { return roomlistmodel->hasSuppressedUpdates(); }
    QVariantMap activeCalls() const { return roomlistmodel->activeCalls(); }

public slots:
    int roomidToIndex(QString roomid)
    {
        return mapFromSource(roomlistmodel->index(roomlistmodel->roomidToIndex(roomid))).row();
    }
    // Look up a role value by room ID from the unfiltered source model.
    // Unlike roomidToIndex + data, this works even when the room is hidden by the current filter.
    Q_INVOKABLE int unfilteredRowCount() const { return roomlistmodel->rowCount(); }
    Q_INVOKABLE int roleId(const QString &roleName) const
    {
        const auto roles = roomlistmodel->roleNames();
        for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
            if (QString::fromUtf8(it.value()) == roleName)
                return it.key();
        }
        return -1;
    }
    Q_INVOKABLE QVariant unfilteredRoomData(const QString &roomid, int role)
    {
        int srcRow = roomlistmodel->roomidToIndex(roomid);
        if (srcRow < 0)
            return {};
        return roomlistmodel->data(roomlistmodel->index(srcRow, 0), role);
    }
    void setInteractionSuppressed(bool suppressed);
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
    void markAsRead(const QString &roomid);
    void markAsUnread(const QString &roomid);
    void copyLink(QString roomid);
    void setCurrentRoom(QString roomid) { roomlistmodel->setCurrentRoom(std::move(roomid)); }
    void openOrJoinRoom(QString roomid) { roomlistmodel->openOrJoinRoom(std::move(roomid)); }
    void resetCurrentRoom() { roomlistmodel->resetCurrentRoom(); }
    RoomPreview getRoomPreviewById(QString roomid) const
    {
        return roomlistmodel->getRoomPreviewById(roomid);
    }

    void persistDraftForRoom(const QString &roomId, const QString &draftText)
    {
        roomlistmodel->persistDraftForRoom(roomId, draftText);
    }
    QString composerDraftForRoom(const QString &roomId) const
    {
        const auto settings = UserSettings::instance();
        return settings ? settings->composerDraftForRoom(roomId) : QString{};
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
    void hasSuppressedUpdatesChanged();
    void roomLeft(QString roomid);
    void activeCallsChanged();

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
    int navigationRoomListSort = 0; // UserSettings::RoomSortOrder enum value

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
    bool excludePeople          = false;
    bool excludeBots            = false;
    bool excludeGroups          = false;
    bool interactionSuppressed_ = false;

    inline static FilteredRoomlistModel *instance_ = nullptr;
};

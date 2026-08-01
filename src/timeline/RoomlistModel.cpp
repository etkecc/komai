// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include <QDateTime>
#include <QTimer>
#include <algorithm>

#include "TimelineViewManager.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"
#include "utils/Utils.h"

#ifdef KOMAI_DBUS_SYS
#include <QDBusConnection>
#endif

namespace {
struct MatrixRoomListRefreshResult
{
    std::optional<QVector<komai::MatrixRoomSummary>> roomList;
    QString error;
};

struct MatrixNotificationFetchBatchResult
{
    uint64_t handleId = 0;
    QVector<komai::MatrixNotificationItem> items;
    QString error;
};

// A freshly-typed draft doesn't count toward the attention indicators (window
// title, tray icon, app badge) until it has sat unsent for this long -- a
// draft is the user's own in-progress text, not something demanding
// attention like an incoming message.
constexpr qint64 kDraftAttentionDelayMs = 60'000;

bool
matrixRoomSummaryEquals(const komai::MatrixRoomSummary &left, const komai::MatrixRoomSummary &right)
{
    return left.roomId == right.roomId && left.latestEventId == right.latestEventId &&
           left.displayName == right.displayName && left.avatarUrl == right.avatarUrl &&
           left.topic == right.topic && left.lastMessage == right.lastMessage &&
           left.lastMessageKind == right.lastMessageKind &&
           left.lastMessageSenderId == right.lastMessageSenderId &&
           left.lastMessageSenderDisplayName == right.lastMessageSenderDisplayName &&
           left.tags == right.tags && left.parentSpaceRoomIds == right.parentSpaceRoomIds &&
           left.directChatOtherUserId == right.directChatOtherUserId &&
           left.isInvite == right.isInvite && left.isSpace == right.isSpace &&
           left.isDirect == right.isDirect && left.isBotRoom == right.isBotRoom &&
           left.isEncrypted == right.isEncrypted && left.isPublic == right.isPublic &&
           left.memberCount == right.memberCount && left.unreadMessages == right.unreadMessages &&
           left.notificationCount == right.notificationCount &&
           left.highlightCount == right.highlightCount &&
           left.isMarkedUnread == right.isMarkedUnread &&
           left.hasActiveCall == right.hasActiveCall &&
           left.activeCallParticipantCount == right.activeCallParticipantCount &&
           left.timestamp == right.timestamp;
}

bool
roomPreviewEquals(const RoomPreview &left, const RoomPreview &right)
{
    return left.roomid_ == right.roomid_ && left.roomName_ == right.roomName_ &&
           left.roomTopic_ == right.roomTopic_ && left.roomAvatarUrl_ == right.roomAvatarUrl_ &&
           left.directChatOtherUserId_ == right.directChatOtherUserId_ &&
           left.reason_ == right.reason_ && left.memberCount_ == right.memberCount_ &&
           left.isDirect_ == right.isDirect_ && left.isEncrypted_ == right.isEncrypted_ &&
           left.isPublic_ == right.isPublic_ && left.isInvite_ == right.isInvite_ &&
           left.isFetched_ == right.isFetched_ && left.canJoin_ == right.canJoin_ &&
           left.isMatrixSummary_ == right.isMatrixSummary_;
}

uint64_t
totalNotificationCount(const QHash<QString, komai::MatrixRoomSummary> &rooms)
{
    uint64_t total = 0;
    for (auto it = rooms.cbegin(); it != rooms.cend(); ++it)
        total += it.value().unreadMessages;
    return total;
}

void
reconcileRoomNotificationCounts(const QHash<QString, komai::MatrixRoomSummary> &previousRooms,
                                const QHash<QString, komai::MatrixRoomSummary> &currentRooms)
{
    auto *chatPage = ChatPage::instance();
    if (!chatPage)
        return;

    for (auto it = previousRooms.cbegin(); it != previousRooms.cend(); ++it) {
        const auto currentIt     = currentRooms.constFind(it.key());
        const auto previousCount = static_cast<int>(it.value().unreadMessages);
        const auto currentCount =
          currentIt == currentRooms.cend() ? 0 : static_cast<int>(currentIt.value().unreadMessages);

        if (currentCount < previousCount)
            chatPage->reconcileRoomNotifications(it.key(), currentCount);
    }
}
} // namespace

bool
RoomlistModel::isCachedEncryptedPreview(const QString &room_id, const DescInfo &description)
{
    Q_UNUSED(room_id);
    Q_UNUSED(description);
    return false;
}

RoomlistModel::AttentionState
RoomlistModel::attentionStateForRow(const QAbstractItemModel *model, const QModelIndex &idx)
{
    AttentionState state;
    if (!model || !idx.isValid())
        return state;

    state.hasUnread     = model->data(idx, RoomlistModel::HasUnreadMessages).toBool();
    state.hasStaleDraft = model->data(idx, RoomlistModel::HasStaleDraft).toBool();
    state.hasHighlight  = model->data(idx, RoomlistModel::HasLoudNotification).toBool();

    if (state.hasUnread && !state.hasHighlight) {
        const auto tags     = model->data(idx, RoomlistModel::Tags).toStringList();
        state.isLowPriority = tags.contains(QStringLiteral("m.lowpriority"));
    }

    return state;
}

RoomlistModel::GlobalExcludeState
RoomlistModel::globalExcludesFromSettings(const UserSettings &settings)
{
    GlobalExcludeState state;
    const auto excluded = settings.globalExcludes();
    for (const auto &entry : excluded) {
        if (entry.startsWith(QLatin1String("tag:")))
            state.tags.push_back(entry.mid(4));
        else if (entry.startsWith(QLatin1String("space:")))
            state.spaces.push_back(entry.mid(6));
        else if (entry == QLatin1String("people"))
            state.excludePeople = true;
        else if (entry == QLatin1String("bot"))
            state.excludeBots = true;
        else if (entry == QLatin1String("group"))
            state.excludeGroups = true;
    }

    return state;
}

bool
RoomlistModel::isExcludedFromAllRooms(const QModelIndex &idx,
                                      const GlobalExcludeState &globalExcludes) const
{
    const auto tags = data(idx, RoomlistModel::Tags).toStringList();
    for (const auto &tag : tags) {
        if (globalExcludes.tags.contains(tag))
            return true;
    }

    const auto parents = data(idx, RoomlistModel::ParentSpaces).toStringList();
    for (const auto &space : parents) {
        if (globalExcludes.spaces.contains(space))
            return true;
    }

    const bool isDirect = data(idx, RoomlistModel::IsDirect).toBool();
    const bool isBot    = data(idx, RoomlistModel::IsBotRoom).toBool();
    if (globalExcludes.excludePeople && isDirect && !isBot)
        return true;
    if (globalExcludes.excludeBots && isBot)
        return true;
    if (globalExcludes.excludeGroups && !isDirect)
        return true;

    return false;
}

int
RoomlistModel::computeAttentionCount() const
{
    if (auto *filtered = FilteredRoomlistModel::instance();
        filtered && filtered->sourceModel() == this) {
        const auto badges = filtered->computeFilterBadges({QString()});
        if (const auto it = badges.constFind(QString()); it != badges.cend())
            return it.value().unreadCount;
    }

    const auto settings = UserSettings::instance();
    if (!settings)
        return 0;

    const auto globalExcludes = globalExcludesFromSettings(*settings);
    int totalAttentionCount   = 0;

    const int rows = rowCount();
    for (int row = 0; row < rows; ++row) {
        const auto idx            = index(row, 0);
        const auto attentionState = attentionStateForRow(this, idx);
        if (!attentionState.hasAnyAttention())
            continue;

        const bool isSpace = data(idx, RoomlistModel::IsSpace).toBool();
        if (isSpace) {
            const auto roomId = data(idx, RoomlistModel::RoomId).toString();
            if (globalExcludes.spaces.contains(roomId))
                continue;
        } else if (isExcludedFromAllRooms(idx, globalExcludes)) {
            continue;
        }

        if (attentionState.countAsActive(false))
            totalAttentionCount++;
    }

    return totalAttentionCount;
}

void
RoomlistModel::emitAttentionCount()
{
    emit attentionCountUpdated(computeAttentionCount());
}

RoomlistModel::RoomlistModel(TimelineViewManager *parent)
  : QAbstractListModel(parent)
  , manager(parent)
{
    connect(UserSettings::instance().get(),
            &UserSettings::navigationRoomListLastMessagePreviewChanged,
            this,
            [this]() {
                if (!roomids.empty()) {
                    emit dataChanged(index(0),
                                     index((int)roomids.size() - 1),
                                     {
                                       Roles::LastMessage,
                                       Roles::LastMessagePreviewSenderName,
                                       Roles::LastMessagePreviewBody,
                                       Roles::Time,
                                       Roles::Timestamp,
                                     });
                }
            });

    connect(this,
            &RoomlistModel::attentionCountUpdated,
            ChatPage::instance(),
            &ChatPage::attentionCountChanged);

    connect(
      UserSettings::instance().get(),
      &UserSettings::globalExcludesChanged,
      this,
      [this]() { emitAttentionCount(); },
      Qt::QueuedConnection);
    connect(UserSettings::instance().get(),
            &UserSettings::composerDraftsByRoomChanged,
            this,
            [this]() { emitAttentionCount(); });

    connect(
      this,
      &RoomlistModel::fetchedPreview,
      this,
      [this](QString roomid, RoomInfo info) {
          if (this->previewedRooms.contains(roomid)) {
              this->previewedRooms.insert(roomid, std::move(info));
              auto idx = this->roomidToIndex(roomid);
              emit dataChanged(index(idx),
                               index(idx),
                               {
                                 Roles::RoomName,
                                 Roles::AvatarUrl,
                                 Roles::IsSpace,
                                 Roles::IsPreviewFetched,
                                 Qt::DisplayRole,
                               });
          }
      },
      Qt::QueuedConnection);
}

void
RoomlistModel::emitCurrentRoomVisualStateChanged()
{
    emit currentRoomPreviewChanged();
}

void
RoomlistModel::notifyCurrentRoomIdChanged()
{
    emit currentRoomIdChanged(currentRoomId());
}

void
RoomlistModel::scheduleCurrentRoomVisualStateChanged()
{
    currentRoomVisualStateDeferred_ = false;
    currentRoomVisualStateDeferredRoomId_.clear();
    const auto generation = ++currentRoomVisualStateGeneration_;
    QTimer::singleShot(0, this, [this, generation]() {
        if (generation != currentRoomVisualStateGeneration_)
            return;

        emitCurrentRoomVisualStateChanged();
    });
}

void
RoomlistModel::deferCurrentRoomVisualState(const QString &roomId)
{
    currentRoomVisualStateGeneration_++;
    currentRoomVisualStateDeferred_       = !roomId.isEmpty();
    currentRoomVisualStateDeferredRoomId_ = currentRoomVisualStateDeferred_ ? roomId : QString{};
}

void
RoomlistModel::flushDeferredCurrentRoomVisualState(const QString &roomId)
{
    if (!currentRoomVisualStateDeferred_ || currentRoomVisualStateDeferredRoomId_ != roomId)
        return;

    currentRoomVisualStateDeferred_ = false;
    currentRoomVisualStateDeferredRoomId_.clear();
    emit currentRoomPreviewChanged();
}

QHash<int, QByteArray>
RoomlistModel::roleNames() const
{
    return {
      {AvatarUrl, "avatarUrl"},
      {RoomName, "roomName"},
      {RoomId, "roomId"},
      {LastMessage, "lastMessage"},
      {LastMessagePreviewSenderName, "lastMessagePreviewSenderName"},
      {LastMessagePreviewBody, "lastMessagePreviewBody"},
      {Time, "time"},
      {Timestamp, "timestamp"},
      {HasUnreadMessages, "hasUnreadMessages"},
      {HasLoudNotification, "hasLoudNotification"},
      {UnreadCount, "unreadCount"},
      {HasDraft, "hasDraft"},
      {DraftPreview, "draftPreview"},
      {HasStaleDraft, "hasStaleDraft"},
      {IsInvite, "isInvite"},
      {IsSpace, "isSpace"},
      {Tags, "tags"},
      {ParentSpaces, "parentSpaces"},
      {IsDirect, "isDirect"},
      {DirectChatOtherUserId, "directChatOtherUserId"},
      {IsBotRoom, "isBotRoom"},
      {IsEncrypted, "isEncrypted"},
      {IsMarkedUnread, "isMarkedUnread"},
      {HasActiveCall, "hasActiveCall"},
      {ActiveCallParticipantCount, "activeCallParticipantCount"},
    };
}

void
RoomlistModel::refreshActiveCalls()
{
    QVariantMap next;
    for (auto it = matrixJoinedRooms_.cbegin(); it != matrixJoinedRooms_.cend(); ++it) {
        if (it.value().hasActiveCall)
            next.insert(it.key(), static_cast<int>(it.value().activeCallParticipantCount));
    }

    if (next != activeCalls_) {
        activeCalls_ = std::move(next);
        emit activeCallsChanged();
    }
}

QString
RoomlistModel::currentRoomId() const
{
    return currentRoomPreview_ ? currentRoomPreview_->roomid() : QString();
}

void
RoomlistModel::resetRoomCollections(bool clearAllDrafts)
{
    matrixJoinedRooms_.clear();
    previewedRooms.clear();
    invites.clear();
    roomids.clear();
    roomReadStatus.clear();
    matrixNotificationFetchQueued_     = false;
    pendingMatrixNotificationHandleId_ = 0;
    pendingMatrixNotificationRequests_.clear();
    deferredMatrixRoomListSnapshot_.reset();
    setHasSuppressedUpdates(false);

    if (clearAllDrafts) {
        if (const auto settings = UserSettings::instance())
            settings->clearAllComposerDrafts();
        draftStartedAtMs_.clear();
    }
}

void
RoomlistModel::setInteractionSuppressed(bool suppressed)
{
    if (interactionSuppressed_ == suppressed)
        return;

    interactionSuppressed_ = suppressed;
    setHasSuppressedUpdates(false);
    if (!interactionSuppressed_)
        resumeDeferredMatrixRoomRefresh();
}

void
RoomlistModel::setHasSuppressedUpdates(bool hasSuppressedUpdates)
{
    if (hasSuppressedUpdates_ == hasSuppressedUpdates)
        return;

    hasSuppressedUpdates_ = hasSuppressedUpdates;
    emit suppressedUpdatesChanged();
}

bool
RoomlistModel::matrixRoomListSnapshotDiffersFromAppliedState(
  const QVector<komai::MatrixRoomSummary> &roomList) const
{
    if (roomids.size() != static_cast<size_t>(roomList.size()))
        return true;

    for (int i = 0; i < roomList.size(); ++i) {
        const auto &room = roomList.at(i);
        if (roomids[static_cast<size_t>(i)] != room.roomId)
            return true;

        const auto currentIt = matrixJoinedRooms_.constFind(room.roomId);
        if (currentIt == matrixJoinedRooms_.cend() ||
            !matrixRoomSummaryEquals(currentIt.value(), room)) {
            return true;
        }
    }

    return false;
}

void
RoomlistModel::removeRoomState(const QString &room_id, bool clearDraftForRoom)
{
    matrixJoinedRooms_.remove(room_id);
    invites.remove(room_id);
    previewedRooms.remove(room_id);
    roomReadStatus.erase(room_id);

    if (clearDraftForRoom) {
        if (const auto settings = UserSettings::instance())
            settings->clearComposerDraftForRoom(room_id);
        draftStartedAtMs_.remove(room_id);
    }

    if (pendingCurrentRoomId_ == room_id)
        pendingCurrentRoomId_.clear();
    if (currentRoomVisualStateDeferredRoomId_ == room_id) {
        currentRoomVisualStateDeferred_ = false;
        currentRoomVisualStateDeferredRoomId_.clear();
        currentRoomVisualStateGeneration_++;
    }
}

void
RoomlistModel::initializeRooms()
{
    const auto *mainWindow = MainWindow::instance();
    beginResetModel();
    resetRoomCollections(false);
    endResetModel();
    emitAttentionCount();

    if (mainWindow && mainWindow->matrixBackendHandleId() != 0) {
        refreshMatrixBackendRooms();
    } else {
        komai::logging::ui()->warn(
          "RoomlistModel initialization without an active matrix-sdk runtime is not supported");
    }

#ifdef KOMAI_DBUS_SYS
    setDbusInterfaceEnabled(MainWindow::instance()->dbusAvailable());
#endif
}

void
RoomlistModel::refreshMatrixBackendRooms()
{
    const auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() == 0) {
        return;
    }

    if (matrixRoomRefreshInFlight_) {
        matrixRoomRefreshPending_ = true;
        return;
    }

    startMatrixBackendRoomsRefresh(mainWindow->matrixBackendHandleId());
}

void
RoomlistModel::startMatrixBackendRoomsRefresh(uint64_t handleId)
{
    matrixRoomRefreshInFlight_ = true;
    komai::qt_worker_task::runQueued(
      this,
      [handleId]() {
          MatrixRoomListRefreshResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.roomList =
            komai::MatrixBackendRuntimeService::fetchRoomList(context, handleId, &result.error);
          return result;
      },
      [handleId](RoomlistModel *model, const MatrixRoomListRefreshResult &result) {
          model->matrixRoomRefreshInFlight_ = false;

          const auto *mainWindow   = MainWindow::instance();
          const auto currentHandle = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
          if (currentHandle != handleId) {
              model->matrixRoomRefreshPending_ = false;
              return;
          }

          if (!result.roomList.has_value()) {
              komai::logging::ui()->warn("Failed to fetch matrix-sdk room list snapshot: {}",
                                         result.error.toStdString());
              if (model->interactionSuppressed_)
                  model->matrixRoomRefreshPending_ = true;
          } else if (model->interactionSuppressed_) {
              const bool hasSuppressedUpdates =
                model->matrixRoomListSnapshotDiffersFromAppliedState(*result.roomList);
              if (hasSuppressedUpdates)
                  model->deferredMatrixRoomListSnapshot_ = *result.roomList;
              else
                  model->deferredMatrixRoomListSnapshot_.reset();
              model->setHasSuppressedUpdates(hasSuppressedUpdates);
          } else {
              model->applyMatrixBackendRoomsSnapshot(*result.roomList);
          }

          if (model->matrixRoomRefreshPending_) {
              if (!result.roomList.has_value() && model->interactionSuppressed_)
                  return;

              model->matrixRoomRefreshPending_ = false;
              model->startMatrixBackendRoomsRefresh(currentHandle);
          }
      });
}

void
RoomlistModel::fetchMatrixNotificationBatch(uint64_t handleId,
                                            QVector<komai::MatrixNotificationRequest> requests)
{
    if (handleId == 0 || requests.isEmpty())
        return;

    komai::qt_worker_task::runQueued(
      this,
      [handleId, requests = std::move(requests)]() {
          MatrixNotificationFetchBatchResult result;
          result.handleId = handleId;

          const auto context = komai::matrix_backend::blockingCallContext();
          auto items         = komai::MatrixBackendRuntimeService::fetchNotificationItems(
            context, handleId, requests, &result.error);
          if (items.has_value())
              result.items = std::move(*items);

          return result;
      },
      [](RoomlistModel *, MatrixNotificationFetchBatchResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (!result.error.isEmpty()) {
              komai::logging::ui()->warn("Failed to fetch matrix-sdk notification batch: {}",
                                         result.error.toStdString());
              return;
          }

          auto *chatPage = ChatPage::instance();
          if (!chatPage)
              return;

          for (const auto &item : std::as_const(result.items))
              chatPage->dispatchMatrixNotification(item);
      });
}

void
RoomlistModel::queueMatrixNotificationFetch(uint64_t handleId,
                                            const QString &roomId,
                                            const QString &eventId)
{
    const auto trimmedRoomId  = roomId.trimmed();
    const auto trimmedEventId = eventId.trimmed();
    if (handleId == 0 || trimmedRoomId.isEmpty() || trimmedEventId.isEmpty())
        return;

    const auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;

    const auto settings = UserSettings::instance().get();
    if (!settings || !settings->notificationsAccountEnabled() ||
        !settings->desktopNotificationsEnabled())
        return;

    if (pendingMatrixNotificationHandleId_ != 0 && pendingMatrixNotificationHandleId_ != handleId)
        pendingMatrixNotificationRequests_.clear();

    pendingMatrixNotificationHandleId_ = handleId;

    const auto key = trimmedRoomId + QLatin1Char('\x1f') + trimmedEventId;
    pendingMatrixNotificationRequests_.insert(key,
                                              {
                                                .roomId  = trimmedRoomId,
                                                .eventId = trimmedEventId,
                                              });

    if (matrixNotificationFetchQueued_)
        return;

    matrixNotificationFetchQueued_ = true;
    // Live matrix-sdk notifications may arrive in short bursts from a single
    // sync response. Coalesce briefly before resolving the full payload batch.
    QTimer::singleShot(50, this, [this]() {
        matrixNotificationFetchQueued_ = false;

        const auto handleId = pendingMatrixNotificationHandleId_;
        if (handleId == 0 || pendingMatrixNotificationRequests_.isEmpty())
            return;

        const auto *mainWindow = MainWindow::instance();
        if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId) {
            pendingMatrixNotificationRequests_.clear();
            pendingMatrixNotificationHandleId_ = 0;
            return;
        }

        QVector<komai::MatrixNotificationRequest> requests;
        requests.reserve(pendingMatrixNotificationRequests_.size());
        for (auto it = pendingMatrixNotificationRequests_.cbegin();
             it != pendingMatrixNotificationRequests_.cend();
             ++it) {
            requests.push_back(it.value());
        }

        pendingMatrixNotificationRequests_.clear();
        pendingMatrixNotificationHandleId_ = 0;
        fetchMatrixNotificationBatch(handleId, std::move(requests));
    });
}

void
RoomlistModel::applyMatrixBackendRoomsSnapshot(const QVector<komai::MatrixRoomSummary> &roomList)
{
    std::vector<QString> newRoomIds;
    newRoomIds.reserve(static_cast<size_t>(roomList.size()));

    QHash<QString, komai::MatrixRoomSummary> newMatrixRooms;
    const QString savedPendingRoomId = pendingCurrentRoomId_;
    const QString selectedRoomId =
      currentRoomPreview_
        ? currentRoomPreview_->roomid()
        : (!pendingCurrentRoomId_.isEmpty() ? pendingCurrentRoomId_
                                            : UserSettings::instance()->currentRoomId());
    const bool hadCurrentMatrixSummarySelection =
      !selectedRoomId.isEmpty() && currentRoomPreview_ && currentRoomPreview_->isMatrixSummary() &&
      currentRoomPreview_->roomid() == selectedRoomId;
    const auto previousCurrentRoomPreview =
      hadCurrentMatrixSummarySelection ? *currentRoomPreview_ : RoomPreview{};
    const bool restoringStartupSelection =
      !selectedRoomId.isEmpty() && !currentRoomPreview_ && pendingCurrentRoomId_.isEmpty() &&
      UserSettings::instance()->currentRoomId() == selectedRoomId;
    const auto previousMatrixRooms        = matrixJoinedRooms_;
    const bool hadPreviousMatrixSnapshot  = !previousMatrixRooms.isEmpty();
    const auto previousTotalNotifications = totalNotificationCount(previousMatrixRooms);
    const auto settings                   = UserSettings::instance().get();
    const bool shouldAlertOnIncoming      = settings && settings->notificationsAccountEnabled() &&
                                       settings->desktopNotificationsAttentionOnIncoming();

    for (const auto &room : roomList) {
        newRoomIds.push_back(room.roomId);
        newMatrixRooms.insert(room.roomId, room);
    }

    // Detect whether the room set or ordering changed (structural) vs only field
    // values changed (data-only).  A full model reset (beginResetModel/endResetModel)
    // causes QML ListView to lose its scroll position, so we avoid it when the room
    // set is identical and only emit targeted dataChanged signals.
    const bool structuralChange = roomids.size() != newRoomIds.size() || [&]() {
        for (size_t i = 0; i < newRoomIds.size(); ++i) {
            if (roomids[i] != newRoomIds[i])
                return true;
        }
        return false;
    }();

    if (!structuralChange) {
        // Same rooms in same order – find rows where field values differ.
        QVector<int> changedRows;
        for (size_t i = 0; i < newRoomIds.size(); ++i) {
            const auto it = matrixJoinedRooms_.constFind(newRoomIds[i]);
            if (it == matrixJoinedRooms_.cend() ||
                !matrixRoomSummaryEquals(it.value(), newMatrixRooms.value(newRoomIds[i]))) {
                changedRows.push_back(static_cast<int>(i));
            }
        }

        if (changedRows.isEmpty()) {
            emitAttentionCount();
            return;
        }

        matrixJoinedRooms_ = std::move(newMatrixRooms);

        for (int row : changedRows)
            emit dataChanged(index(row), index(row));
    } else {
        beginResetModel();
        resetRoomCollections(false);
        matrixJoinedRooms_ = std::move(newMatrixRooms);
        roomids            = std::move(newRoomIds);
        endResetModel();
    }

    refreshActiveCalls();

    const auto currentTotalNotifications = totalNotificationCount(matrixJoinedRooms_);
    if (hadPreviousMatrixSnapshot && shouldAlertOnIncoming &&
        currentTotalNotifications > previousTotalNotifications) {
        if (auto *mainWindow = MainWindow::instance())
            mainWindow->alert(0);
    }

    if (hadPreviousMatrixSnapshot)
        reconcileRoomNotificationCounts(previousMatrixRooms, matrixJoinedRooms_);

    if (!selectedRoomId.isEmpty() && matrixJoinedRooms_.contains(selectedRoomId)) {
        if (restoringStartupSelection) {
            deferStartupCurrentRoomRestore(selectedRoomId);
        } else if (hadCurrentMatrixSummarySelection) {
            allowDeferredStartupCurrentRoomRestore_ = false;
            deferredStartupCurrentRoomId_.clear();
            pendingCurrentRoomId_.clear();
            currentRoomPreview_ = getRoomPreviewById(selectedRoomId);
            UserSettings::instance()->setCurrentRoomId(selectedRoomId);

            const bool previewChanged =
              !roomPreviewEquals(previousCurrentRoomPreview, *currentRoomPreview_);
            const bool deferredMatch = currentRoomVisualStateDeferred_ &&
                                       currentRoomVisualStateDeferredRoomId_ == selectedRoomId;

            if (previewChanged) {
                if (deferredMatch) {
                    // Keep the newer preview cached, but avoid waking QML
                    // until the active matrix timeline has its first model.
                } else {
                    scheduleCurrentRoomVisualStateChanged();
                }
            }
        } else {
            setCurrentRoom(selectedRoomId);
        }
    } else if (hadCurrentMatrixSummarySelection) {
        clearCurrentRoomSelection();
    }

    // Resolve a deferred room switch (e.g. after creating a new room) that was
    // pending before this snapshot arrived.  The restore logic above preserves the
    // *previous* selection; now that the snapshot has landed we can honour the
    // pending switch if the target room is present.
    if (!savedPendingRoomId.isEmpty() && savedPendingRoomId != selectedRoomId &&
        matrixJoinedRooms_.contains(savedPendingRoomId)) {
        setCurrentRoom(savedPendingRoomId);
    }

    emitAttentionCount();
}

bool
RoomlistModel::resumeDeferredMatrixRoomRefresh()
{
    if (matrixRoomRefreshInFlight_)
        return false;

    if (matrixRoomRefreshPending_) {
        deferredMatrixRoomListSnapshot_.reset();
        setHasSuppressedUpdates(false);
        matrixRoomRefreshPending_ = false;
        refreshMatrixBackendRooms();
        return true;
    }

    if (!deferredMatrixRoomListSnapshot_.has_value())
        return false;

    const auto snapshot = std::move(*deferredMatrixRoomListSnapshot_);
    deferredMatrixRoomListSnapshot_.reset();
    setHasSuppressedUpdates(false);
    applyMatrixBackendRoomsSnapshot(snapshot);
    return true;
}

bool
RoomlistModel::hasDraft(const QString &room_id) const
{
    if (room_id.isEmpty())
        return false;

    if (invites.contains(room_id) || previewedRooms.contains(room_id))
        return false;

    const auto settings = UserSettings::instance();
    return settings && settings->hasComposerDraftForRoom(room_id);
}

bool
RoomlistModel::hasStaleDraft(const QString &room_id) const
{
    if (!hasDraft(room_id))
        return false;

    const auto it = draftStartedAtMs_.constFind(room_id);
    if (it == draftStartedAtMs_.constEnd())
        return false;

    return QDateTime::currentMSecsSinceEpoch() - it.value() >= kDraftAttentionDelayMs;
}

QString
RoomlistModel::draftPreviewText(const QString &room_id) const
{
    if (!hasDraft(room_id))
        return {};

    const auto settings = UserSettings::instance();
    if (!settings)
        return {};

    auto draft = settings->composerDraftForRoom(room_id);
    draft.replace(QChar(u'\n'), QChar(u' '));
    draft.replace(QChar(u'\r'), QChar(u' '));
    return draft.simplified();
}

void
RoomlistModel::persistDraftForRoom(const QString &room_id, const QString &draftText)
{
    if (room_id.isEmpty())
        return;

    const auto settings = UserSettings::instance();
    if (!settings)
        return;

    if (draftText.trimmed().isEmpty()) {
        settings->clearComposerDraftForRoom(room_id);
        draftStartedAtMs_.remove(room_id);
    } else {
        settings->setComposerDraftForRoom(room_id, draftText);
        if (!draftStartedAtMs_.contains(room_id)) {
            draftStartedAtMs_.insert(room_id, QDateTime::currentMSecsSinceEpoch());
            QTimer::singleShot(kDraftAttentionDelayMs, this, [this, room_id]() {
                if (!hasStaleDraft(room_id))
                    return;

                const auto staleIdx = roomidToIndex(room_id);
                if (staleIdx != -1)
                    emit dataChanged(index(staleIdx), index(staleIdx), {Roles::HasStaleDraft});
                emitAttentionCount();
            });
        }
    }

    const auto idx = roomidToIndex(room_id);
    if (idx == -1)
        return;

    emit dataChanged(index(idx),
                     index(idx),
                     {
                       Roles::HasDraft,
                       Roles::DraftPreview,
                       Roles::LastMessage,
                       Roles::LastMessagePreviewSenderName,
                       Roles::LastMessagePreviewBody,
                       Qt::DisplayRole,
                     });
}

void
RoomlistModel::updateReadStatus(const std::map<QString, bool> &roomReadStatus_)
{
    std::vector<int> roomsToUpdate;
    roomsToUpdate.resize(roomReadStatus_.size());
    for (const auto &[roomid, roomUnread] : roomReadStatus_) {
        if (roomUnread != roomReadStatus[roomid]) {
            roomsToUpdate.push_back(this->roomidToIndex(roomid));
        }

        this->roomReadStatus[roomid] = roomUnread;
    }

    for (auto idx : roomsToUpdate) {
        emit dataChanged(index(idx),
                         index(idx),
                         {
                           Roles::HasUnreadMessages,
                         });
    }

    emitAttentionCount();
}

void
RoomlistModel::notifyRoomPreviewsBackfilled()
{
    if (roomids.empty())
        return;
    emit dataChanged(index(0),
                     index(static_cast<int>(roomids.size()) - 1),
                     {
                       Roles::LastMessage,
                       Roles::LastMessagePreviewSenderName,
                       Roles::LastMessagePreviewBody,
                       Roles::Time,
                       Roles::Timestamp,
                     });
}

#ifdef KOMAI_DBUS_SYS
void
RoomlistModel::setDbusInterfaceEnabled(bool enabled)
{
    if (enabled) {
        if (dbusHost_)
            return;

        dbusHost_ = new DbusHost{this};
        if (!QDBusConnection::sessionBus().registerObject(
              QStringLiteral("/"), dbusHost_, QDBusConnection::ExportAdaptors)) {
            komai::logging::ui()->warn("Failed to register D-Bus interfaces");
            delete dbusHost_;
            dbusHost_ = nullptr;
            return;
        }
        return;
    }

    if (!dbusHost_)
        return;

    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/"));

    delete dbusHost_;
    dbusHost_ = nullptr;
}
#endif

void
RoomlistModel::clear()
{
    beginResetModel();
    resetRoomCollections(true);
    currentRoomPreview_.reset();
    pendingCurrentRoomId_.clear();
    deferredStartupCurrentRoomId_.clear();
    allowDeferredStartupCurrentRoomRestore_ = false;
    notifyCurrentRoomIdChanged();
    scheduleCurrentRoomVisualStateChanged();
    endResetModel();
    emitAttentionCount();
}

#include "moc_RoomlistModel.cpp"

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include <algorithm>

#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QPointer>
#include <QTimer>

#include "ChatPage.h"
#include "EventAccessors.h"
#include "Logging.h"
#include "MainWindow.h"
#include "MatrixClient.h"
#include "MxcImageProvider.h"
#include "TimelineModel.h"
#include "TimelineViewManager.h"
#include "Utils.h"
#include "cache/Cache.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "voip/CallManager.h"

#ifdef KOMAI_DBUS_SYS
#include <QDBusConnection>
#endif

namespace {
constexpr uint64_t kCachedLastMessageScanLimit    = 200;
constexpr int kCachedLastMessageBackfillPageSize  = 200;
constexpr int kCachedLastMessageBackfillMaxEvents = 5000;
constexpr int kCachedLastMessageBackfillMaxRequests =
  kCachedLastMessageBackfillMaxEvents / kCachedLastMessageBackfillPageSize;
constexpr int kCachedLastMessageBackfillMaxConcurrent = 1;
constexpr int kCurrentRoomWarmupTargetEvents          = 100;
constexpr int kCurrentRoomWarmupMaxRequests           = 3;
constexpr int kCurrentRoomWarmupStartDelayMs          = 260;
constexpr int kCurrentRoomWarmupStepDelayMs           = 80;

bool
isCachedEncryptedPreview(const QString &room_id, const DescInfo &description)
{
    if (room_id.isEmpty() || description.event_id.isEmpty())
        return false;

    const auto event = cache::getEvent(room_id.toStdString(), description.event_id.toStdString());
    return event.has_value() &&
           mtx::accessors::event_type(*event) == mtx::events::EventType::RoomEncrypted;
}

void
scheduleLastReadUpdate(const QSharedPointer<TimelineModel> &roomModel, const QString &roomId)
{
    if (roomModel.isNull() || roomId.isEmpty())
        return;

    QTimer::singleShot(0, roomModel.data(), [roomModel, roomId]() {
        if (!roomModel.isNull())
            roomModel->updateLastReadId(roomId);
    });
}
}

RoomlistModel::RoomlistModel(TimelineViewManager *parent)
  : QAbstractListModel(parent)
  , manager(parent)
{
    cache::onRoomReadStatusChanged(
      this, [this](const std::map<QString, bool> &status) { updateReadStatus(status); });

    connect(UserSettings::instance().get(),
            &UserSettings::sidebarsRoomListLastMessagePreviewChanged,
            this,
            [this]() {
                auto style   = UserSettings::instance()->sidebarsRoomListLastMessagePreview();
                bool decrypt = (style == UserSettings::LastMessagePreview::Always);

                cachedLastMessages_.clear();
                cachedLastMessagesComputed_.clear();
                QHash<QString, QSharedPointer<TimelineModel>>::iterator i;
                for (i = models.begin(); i != models.end(); ++i) {
                    auto ptr = i.value();

                    if (!ptr.isNull()) {
                        ptr->setDecryptDescription(decrypt);
                        ptr->updateLastMessage();
                    }
                }

                if (!roomids.empty()) {
                    emit dataChanged(index(0),
                                     index((int)roomids.size() - 1),
                                     {Roles::LastMessage, Roles::Time, Roles::Timestamp});
                }
            });

    connect(this,
            &RoomlistModel::totalUnreadMessageCountUpdated,
            ChatPage::instance(),
            &ChatPage::unreadMessages);

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

QHash<int, QByteArray>
RoomlistModel::roleNames() const
{
    return {
      {AvatarUrl, "avatarUrl"},
      {RoomName, "roomName"},
      {RoomId, "roomId"},
      {LastMessage, "lastMessage"},
      {Time, "time"},
      {Timestamp, "timestamp"},
      {HasUnreadMessages, "hasUnreadMessages"},
      {HasLoudNotification, "hasLoudNotification"},
      {NotificationCount, "notificationCount"},
      {HasDraft, "hasDraft"},
      {DraftPreview, "draftPreview"},
      {IsInvite, "isInvite"},
      {IsSpace, "isSpace"},
      {Tags, "tags"},
      {ParentSpaces, "parentSpaces"},
      {IsDirect, "isDirect"},
      {DirectChatOtherUserId, "directChatOtherUserId"},
      {IsEncrypted, "isEncrypted"},
    };
}

QSharedPointer<TimelineModel>
RoomlistModel::getRoomById(QString id) const
{
    return getRoomByIdWithReason(std::move(id), "cpp.getRoomById");
}

QSharedPointer<TimelineModel>
RoomlistModel::getRoomByIdWithReason(QString id, const char *reason) const
{
    if (models.contains(id))
        return models.value(id);

    if (!cachedJoinedRooms_.contains(id))
        return {};

    return const_cast<RoomlistModel *>(this)->ensureRoomModel(id, true, reason);
}

QSharedPointer<TimelineModel>
RoomlistModel::getMaterializedRoomById(QString id) const
{
    if (models.contains(id))
        return models.value(id);

    return {};
}

QSharedPointer<TimelineModel>
RoomlistModel::ensureRoomModel(const QString &room_id,
                               bool suppressInsertNotification,
                               const char *reason)
{
    if (!models.contains(room_id) && cachedJoinedRooms_.contains(room_id))
        addRoom(room_id, suppressInsertNotification, reason);

    return models.value(room_id);
}

void
RoomlistModel::refreshCachedRoomMetadata(const QString &room_id)
{
    cachedJoinedRooms_.insert(room_id, cache::singleRoomInfo(room_id.toStdString()));
    cachedEncryptedRooms_.insert(room_id, cache::isRoomEncrypted(room_id.toStdString()));
    invalidateCachedLastMessage(room_id);
}

DescInfo
RoomlistModel::computeCachedLastMessage(const QString &room_id) const
{
    DescInfo result;

    const auto roomId = room_id.toStdString();
    const auto range  = cache::getTimelineRange(roomId);
    if (!range.has_value())
        return result;

    const QString localUser = utils::localUser();

    uint64_t scanned = 0;
    for (uint64_t idx = range->last;; --idx) {
        const auto eventId = cache::getTimelineEventId(roomId, idx);
        if (!eventId.has_value())
            break;

        const auto event = cache::getEvent(roomId, *eventId);
        if (event.has_value() && mtx::accessors::is_message(*event)) {
            const auto sender = QString::fromStdString(mtx::accessors::sender(*event));
            return utils::getMessageDescription(
              *event, localUser, cache::displayName(room_id, sender));
        }

        if (idx == range->first || ++scanned >= kCachedLastMessageScanLimit)
            break;
    }

    return result;
}

void
RoomlistModel::ensureCachedLastMessage(const QString &room_id)
{
    if (cachedLastMessagesComputed_.contains(room_id))
        return;

    cachedLastMessagesComputed_.insert(room_id);
    cachedLastMessages_.insert(room_id, computeCachedLastMessage(room_id));
}

void
RoomlistModel::scheduleCurrentRoomTimelineWarmup(const QString &roomid)
{
    if (roomid.isEmpty())
        return;

    QTimer::singleShot(kCurrentRoomWarmupStartDelayMs, this, [this, roomid]() {
        warmupCurrentRoomTimeline(roomid);
    });
}

void
RoomlistModel::warmupCurrentRoomTimeline(const QString &roomid, int requestsDone)
{
    if (!currentRoom_ || currentRoom_->roomId() != roomid)
        return;

    if (currentRoom_->isSpace())
        return;

    if (requestsDone >= kCurrentRoomWarmupMaxRequests)
        return;

    if (currentRoom_->rowCount() >= kCurrentRoomWarmupTargetEvents)
        return;

    if (!currentRoom_->canPaginateBack())
        return;

    if (currentRoom_->paginationInProgress()) {
        connect(
          currentRoom_.data(),
          &TimelineModel::fetchedMore,
          this,
          [this, roomid, requestsDone]() {
              QTimer::singleShot(
                kCurrentRoomWarmupStepDelayMs, this, [this, roomid, requestsDone]() {
                    warmupCurrentRoomTimeline(roomid, requestsDone);
                });
          },
          Qt::SingleShotConnection);
        return;
    }

    connect(
      currentRoom_.data(),
      &TimelineModel::fetchedMore,
      this,
      [this, roomid, requestsDone]() {
          QTimer::singleShot(kCurrentRoomWarmupStepDelayMs, this, [this, roomid, requestsDone]() {
              warmupCurrentRoomTimeline(roomid, requestsDone + 1);
          });
      },
      Qt::SingleShotConnection);
    currentRoom_->requestMore();
}

void
RoomlistModel::maybeBackfillCachedLastMessage(const QString &room_id)
{
    if (room_id.isEmpty() || models.contains(room_id))
        return;

    if (cachedLastMessageBackfillAttempted_.contains(room_id) ||
        cachedLastMessageBackfillQueued_.contains(room_id) ||
        cachedLastMessageBackfillInProgress_.contains(room_id))
        return;

    const auto cachedDescription = cachedLastMessages_.value(room_id);
    if (!cachedDescription.body.isEmpty())
        return;

    // One backfill campaign per room list entry to avoid repeated fetches on re-render.
    cachedLastMessageBackfillAttempted_.insert(room_id);
    cachedLastMessageBackfillQueued_.insert(room_id);
    startQueuedCachedLastMessageBackfills();
}

void
RoomlistModel::startQueuedCachedLastMessageBackfills()
{
    while (cachedLastMessageBackfillInProgress_.size() < kCachedLastMessageBackfillMaxConcurrent &&
           !cachedLastMessageBackfillQueued_.isEmpty()) {
        const auto room_id = *cachedLastMessageBackfillQueued_.constBegin();
        cachedLastMessageBackfillQueued_.remove(room_id);

        const auto roomId    = room_id.toStdString();
        const auto fromToken = cache::previousBatchToken(roomId);
        if (roomids.empty() || roomidToIndex(room_id) == -1 || fromToken.empty())
            continue;

        cachedLastMessageBackfillInProgress_.insert(room_id);
        backfillCachedLastMessage(room_id, fromToken, 0);
    }
}

void
RoomlistModel::backfillCachedLastMessage(const QString &room_id,
                                         const std::string &fromToken,
                                         int requestsDone)
{
    if (room_id.isEmpty() || fromToken.empty() ||
        requestsDone >= kCachedLastMessageBackfillMaxRequests) {
        finalizeCachedLastMessageBackfill(room_id);
        return;
    }

    mtx::http::MessagesOpts opts;
    opts.room_id = room_id.toStdString();
    opts.from    = fromToken;
    opts.limit   = kCachedLastMessageBackfillPageSize;

    const QPointer<RoomlistModel> self(this);
    http::client()->messages(
      opts,
      [self, room_id, fromToken, requestsDone](const mtx::responses::Messages &res,
                                               mtx::http::RequestErr err) {
          if (!self)
              return;

          if (!self->cachedLastMessageBackfillInProgress_.contains(room_id))
              return;

          if (err) {
              nhlog::net()->warn(
                "Failed to backfill room list last-message preview for {}: {} - {} - {}",
                room_id.toStdString(),
                mtx::errors::to_string(err->matrix_error.errcode),
                err->matrix_error.error,
                err->parse_error);
              self->finalizeCachedLastMessageBackfill(room_id);
              return;
          }

          const auto roomId = room_id.toStdString();
          if (cache::previousBatchToken(roomId) != fromToken) {
              nhlog::net()->warn(
                "Room list preview backfill token changed for {}, dropping response",
                room_id.toStdString());
              self->finalizeCachedLastMessageBackfill(room_id);
              return;
          }

          const bool noMoreMessages = res.end.empty() || res.end == fromToken;
          if (!res.chunk.empty())
              cache::saveOldMessages(roomId, res);

          self->invalidateCachedLastMessage(room_id);
          self->ensureCachedLastMessage(room_id);

          const auto refreshedDescription = self->cachedLastMessages_.value(room_id);
          if (!refreshedDescription.body.isEmpty() &&
              !isCachedEncryptedPreview(room_id, refreshedDescription)) {
              if (auto idx = self->roomidToIndex(room_id); idx != -1) {
                  emit self->dataChanged(self->index(idx),
                                         self->index(idx),
                                         {Roles::LastMessage, Roles::Time, Roles::Timestamp});
              }
              self->finalizeCachedLastMessageBackfill(room_id);
              return;
          }

          if (noMoreMessages || requestsDone + 1 >= kCachedLastMessageBackfillMaxRequests) {
              self->finalizeCachedLastMessageBackfill(room_id);
              return;
          }

          self->backfillCachedLastMessage(room_id, res.end, requestsDone + 1);
      });
}

void
RoomlistModel::finalizeCachedLastMessageBackfill(const QString &room_id)
{
    cachedLastMessageBackfillQueued_.remove(room_id);
    cachedLastMessageBackfillInProgress_.remove(room_id);
    startQueuedCachedLastMessageBackfills();
}

void
RoomlistModel::invalidateCachedLastMessage(const QString &room_id)
{
    cachedLastMessagesComputed_.remove(room_id);
    cachedLastMessages_.remove(room_id);
}

void
RoomlistModel::resetRoomCollections(bool clearAllDrafts)
{
    models.clear();
    cachedJoinedRooms_.clear();
    cachedEncryptedRooms_.clear();
    cachedLastMessages_.clear();
    cachedLastMessagesComputed_.clear();
    cachedLastMessageBackfillAttempted_.clear();
    cachedLastMessageBackfillQueued_.clear();
    cachedLastMessageBackfillInProgress_.clear();
    scheduledPrewarms_.clear();
    activePrewarms_.clear();
    prewarmLastAttemptMs_.clear();
    startupMaterializationTrackingActive_ = false;
    startupMaterializationCount_          = 0;
    startupMaterializationWarningEmitted_ = false;
    previewedRooms.clear();
    invites.clear();
    roomids.clear();
    roomReadStatus.clear();
    pendingCurrentRoomId_.clear();
    currentRoom_ = nullptr;
    currentRoomPreview_.reset();

    if (clearAllDrafts) {
        if (const auto settings = UserSettings::instance())
            settings->clearAllComposerDrafts();
    }
}

void
RoomlistModel::removeRoomState(const QString &room_id, bool clearDraftForRoom)
{
    models.remove(room_id);
    invites.remove(room_id);
    previewedRooms.remove(room_id);
    cachedJoinedRooms_.remove(room_id);
    cachedEncryptedRooms_.remove(room_id);
    scheduledPrewarms_.remove(room_id);
    activePrewarms_.remove(room_id);
    prewarmLastAttemptMs_.remove(room_id);
    cachedLastMessageBackfillAttempted_.remove(room_id);
    cachedLastMessageBackfillQueued_.remove(room_id);
    cachedLastMessageBackfillInProgress_.remove(room_id);
    roomReadStatus.erase(room_id);
    invalidateCachedLastMessage(room_id);

    if (clearDraftForRoom) {
        if (const auto settings = UserSettings::instance())
            settings->clearComposerDraftForRoom(room_id);
    }

    if (pendingCurrentRoomId_ == room_id)
        pendingCurrentRoomId_.clear();
}

void
RoomlistModel::logRoomPrewarm(const QString &trigger,
                              const QString &roomid,
                              const QString &action,
                              const QString &reason) const
{
    const bool verboseInfo = manager && manager->roomSwitchPerfEnabled();

    if (reason.isEmpty()) {
        if (verboseInfo)
            nhlog::ui()->info("[prewarm][room-list] trigger={} room_id={} action={}",
                              trigger.toStdString(),
                              roomid.toStdString(),
                              action.toStdString());
        else
            nhlog::ui()->debug("[prewarm][room-list] trigger={} room_id={} action={}",
                               trigger.toStdString(),
                               roomid.toStdString(),
                               action.toStdString());
        return;
    }

    if (verboseInfo)
        nhlog::ui()->info("[prewarm][room-list] trigger={} room_id={} action={} reason={}",
                          trigger.toStdString(),
                          roomid.toStdString(),
                          action.toStdString(),
                          reason.toStdString());
    else
        nhlog::ui()->debug("[prewarm][room-list] trigger={} room_id={} action={} reason={}",
                           trigger.toStdString(),
                           roomid.toStdString(),
                           action.toStdString(),
                           reason.toStdString());
}

void
RoomlistModel::scheduleRoomPrewarm(const QString &roomid, const QString &trigger)
{
    if (roomid.isEmpty())
        return;

    if (scheduledPrewarms_.contains(roomid))
        return;

    scheduledPrewarms_.insert(roomid);
    logRoomPrewarm(trigger, roomid, QStringLiteral("scheduled"));
}

void
RoomlistModel::cancelRoomPrewarm(const QString &roomid,
                                 const QString &trigger,
                                 const QString &reason)
{
    if (roomid.isEmpty())
        return;

    if (!scheduledPrewarms_.contains(roomid))
        return;

    scheduledPrewarms_.remove(roomid);
    logRoomPrewarm(trigger, roomid, QStringLiteral("cancel"), reason);
}

void
RoomlistModel::prewarmRoom(const QString &roomid, const QString &trigger)
{
    if (roomid.isEmpty())
        return;

    scheduledPrewarms_.remove(roomid);

    if (models.contains(roomid)) {
        logRoomPrewarm(
          trigger, roomid, QStringLiteral("skip"), QStringLiteral("already_materialized"));
        return;
    }

    if (invites.contains(roomid)) {
        logRoomPrewarm(trigger, roomid, QStringLiteral("skip"), QStringLiteral("invite"));
        return;
    }

    if (previewedRooms.contains(roomid)) {
        logRoomPrewarm(trigger, roomid, QStringLiteral("skip"), QStringLiteral("preview_room"));
        return;
    }

    if (!cachedJoinedRooms_.contains(roomid)) {
        logRoomPrewarm(
          trigger, roomid, QStringLiteral("skip"), QStringLiteral("not_cached_joined_room"));
        return;
    }

    if (!activePrewarms_.isEmpty()) {
        logRoomPrewarm(
          trigger, roomid, QStringLiteral("skip"), QStringLiteral("another_prewarm_active"));
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // Debounce repeated prewarm attempts for the same room while keeping hover-triggered
    // prewarm responsive during quick pointer movement in the room list.
    constexpr qint64 cooldownMs    = 400;
    const qint64 previousAttemptMs = prewarmLastAttemptMs_.value(roomid, 0);
    if (previousAttemptMs > 0 && (now - previousAttemptMs) < cooldownMs) {
        logRoomPrewarm(trigger, roomid, QStringLiteral("skip"), QStringLiteral("cooldown"));
        return;
    }
    prewarmLastAttemptMs_.insert(roomid, now);

    activePrewarms_.insert(roomid);
    logRoomPrewarm(trigger, roomid, QStringLiteral("start"));

    auto roomModel = ensureRoomModel(roomid, true, "prewarm");
    if (roomModel.isNull())
        logRoomPrewarm(
          trigger, roomid, QStringLiteral("skip"), QStringLiteral("materialization_failed"));
    else
        logRoomPrewarm(trigger, roomid, QStringLiteral("done"));

    activePrewarms_.remove(roomid);
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

    if (draftText.trimmed().isEmpty())
        settings->clearComposerDraftForRoom(room_id);
    else
        settings->setComposerDraftForRoom(room_id, draftText);

    const auto idx = roomidToIndex(room_id);
    if (idx == -1)
        return;

    emit dataChanged(index(idx),
                     index(idx),
                     {Roles::HasDraft, Roles::DraftPreview, Roles::LastMessage, Qt::DisplayRole});
}

std::optional<QVariant>
RoomlistModel::commonRoomData(const QString &room_id, int role) const
{
    switch (role) {
    case Roles::ParentSpaces: {
        auto parents = cache::getParentRoomIds(room_id.toStdString());
        QStringList list;
        list.reserve(static_cast<int>(parents.size()));
        for (const auto &t : parents)
            list.push_back(QString::fromStdString(t));
        return QVariant{list};
    }
    case Roles::RoomId:
        return QVariant{room_id};
    case Roles::IsDirect:
        return QVariant{directChatToUser.count(room_id) > 0};
    case Roles::DirectChatOtherUserId:
        return QVariant{directChatToUser.count(room_id) ? directChatToUser.at(room_id).front()
                                                        : QString{}};
    case Roles::HasDraft:
        return QVariant{hasDraft(room_id)};
    case Roles::DraftPreview:
        return QVariant{draftPreviewText(room_id)};
    default:
        return std::nullopt;
    }
}

QVariant
RoomlistModel::dataForMaterializedRoom(const QString &room_id,
                                       const QSharedPointer<TimelineModel> &room,
                                       int role) const
{
    switch (role) {
    case Roles::AvatarUrl: {
        const auto roomModelAvatar = room->roomAvatarUrl();
        if (!roomModelAvatar.isEmpty())
            return roomModelAvatar;

        const auto avatarUrl = cache::roomAvatarUrl(room_id.toStdString());
        if (!avatarUrl.isEmpty())
            return avatarUrl;

        return roomModelAvatar;
    }
    case Roles::RoomName:
        return room->plainRoomName();
    case Roles::LastMessage:
        return room->lastMessage().body;
    case Roles::Time:
        return room->lastMessage().descriptiveTime;
    case Roles::Timestamp:
        return QVariant{static_cast<quint64>(room->lastMessageTimestamp())};
    case Roles::HasUnreadMessages:
        return this->roomReadStatus.count(room_id) && this->roomReadStatus.at(room_id);
    case Roles::HasLoudNotification:
        return room->hasMentions();
    case Roles::NotificationCount:
        return room->notificationCount();
    case Roles::IsInvite:
        return false;
    case Roles::IsSpace:
        return room->isSpace();
    case Roles::IsPreview:
        return false;
    case Roles::Tags: {
        auto info = cache::singleRoomInfo(room_id.toStdString());
        QStringList list;
        list.reserve(static_cast<int>(info.tags.size()));
        for (const auto &t : info.tags)
            list.push_back(QString::fromStdString(t));
        return list;
    }
    case Roles::IsEncrypted:
        return room->isEncrypted();
    default:
        return {};
    }
}

QVariant
RoomlistModel::dataForCachedRoom(const QString &room_id, const RoomInfo &room, int role) const
{
    switch (role) {
    case Roles::AvatarUrl: {
        if (!room.avatar_url.empty())
            return QString::fromStdString(room.avatar_url);
        return cache::roomAvatarUrl(room_id.toStdString());
    }
    case Roles::RoomName:
        return QString::fromStdString(room.name);
    case Roles::LastMessage: {
        const auto style = UserSettings::instance()->sidebarsRoomListLastMessagePreview();
        const bool encrypted =
          cachedEncryptedRooms_.value(room_id, cache::isRoomEncrypted(room_id.toStdString()));
        const bool previewsEnabled =
          style == UserSettings::LastMessagePreview::Always ||
          (style == UserSettings::LastMessagePreview::OnlyUnencrypted && !encrypted);
        if (!previewsEnabled)
            return QString();

        auto *self = const_cast<RoomlistModel *>(this);
        self->ensureCachedLastMessage(room_id);
        const auto cachedDescription = cachedLastMessages_.value(room_id);
        if (cachedDescription.body.isEmpty())
            self->maybeBackfillCachedLastMessage(room_id);
        else if (style == UserSettings::LastMessagePreview::Always && encrypted &&
                 isCachedEncryptedPreview(room_id, cachedDescription)) {
            self->scheduleRoomPrewarm(room_id, QStringLiteral("auto_preview_decrypt"));
            QTimer::singleShot(0, self, [self, room_id]() {
                if (self->scheduledPrewarms_.contains(room_id))
                    self->prewarmRoom(room_id, QStringLiteral("auto_preview_decrypt"));
            });
        }
        return cachedDescription.body;
    }
    case Roles::Time: {
        auto *self = const_cast<RoomlistModel *>(this);
        self->ensureCachedLastMessage(room_id);
        const auto cachedDescription = cachedLastMessages_.value(room_id);
        if (!cachedDescription.descriptiveTime.isEmpty())
            return cachedDescription.descriptiveTime;

        if (room.approximate_last_modification_ts > 0) {
            return utils::descriptiveTime(QDateTime::fromMSecsSinceEpoch(
              static_cast<qint64>(room.approximate_last_modification_ts)));
        }
        return QString();
    }
    case Roles::Timestamp: {
        auto *self = const_cast<RoomlistModel *>(this);
        self->ensureCachedLastMessage(room_id);
        const auto cachedDescription = cachedLastMessages_.value(room_id);
        const auto ts = std::max(static_cast<quint64>(room.approximate_last_modification_ts),
                                 static_cast<quint64>(cachedDescription.timestamp));
        return QVariant{ts};
    }
    case Roles::HasUnreadMessages:
        return this->roomReadStatus.count(room_id) && this->roomReadStatus.at(room_id);
    case Roles::HasLoudNotification:
        return room.highlight_count > 0;
    case Roles::NotificationCount:
        return static_cast<int>(room.notification_count);
    case Roles::IsInvite:
        return false;
    case Roles::IsSpace:
        return room.is_space;
    case Roles::IsPreview:
        return false;
    case Roles::Tags: {
        QStringList list;
        list.reserve(static_cast<int>(room.tags.size()));
        for (const auto &t : room.tags)
            list.push_back(QString::fromStdString(t));
        return list;
    }
    case Roles::IsEncrypted:
        return cachedEncryptedRooms_.value(room_id, cache::isRoomEncrypted(room_id.toStdString()));
    default:
        return {};
    }
}

QVariant
RoomlistModel::dataForInviteRoom(const RoomInfo &room, int role) const
{
    switch (role) {
    case Roles::AvatarUrl:
        return QString::fromStdString(room.avatar_url);
    case Roles::RoomName:
        return QString::fromStdString(room.name);
    case Roles::LastMessage:
        return tr("Pending invite.");
    case Roles::Time:
        return QString();
    case Roles::Timestamp:
        return QVariant{static_cast<quint64>(0)};
    case Roles::HasUnreadMessages:
    case Roles::HasLoudNotification:
        return false;
    case Roles::NotificationCount:
        return 0;
    case Roles::IsInvite:
        return true;
    case Roles::IsSpace:
        return false;
    case Roles::IsPreview:
        return false;
    case Roles::Tags:
        return QStringList();
    case Roles::IsEncrypted:
        return false; // Invites - assume unencrypted
    default:
        return {};
    }
}

QVariant
RoomlistModel::dataForPreviewRoom(const RoomInfo &room, int role) const
{
    switch (role) {
    case Roles::AvatarUrl:
        return QString::fromStdString(room.avatar_url);
    case Roles::RoomName:
        return QString::fromStdString(room.name);
    case Roles::LastMessage:
        return tr("Previewing this room");
    case Roles::Time:
        return QString();
    case Roles::Timestamp:
        return QVariant{static_cast<quint64>(0)};
    case Roles::HasUnreadMessages:
    case Roles::HasLoudNotification:
        return false;
    case Roles::NotificationCount:
        return 0;
    case Roles::IsInvite:
        return false;
    case Roles::IsSpace:
        return room.is_space;
    case Roles::IsPreview:
        return true;
    case Roles::IsPreviewFetched:
        return true;
    case Roles::Tags:
        return QStringList();
    case Roles::IsEncrypted:
        return false; // Previews - assume unencrypted
    default:
        return {};
    }
}

QVariant
RoomlistModel::dataForUnavailablePreview(int role) const
{
    if (role == Roles::IsPreview)
        return true;
    if (role == Roles::IsPreviewFetched)
        return false;

    switch (role) {
    case Roles::AvatarUrl:
        return QString();
    case Roles::RoomName:
        return tr("No preview available");
    case Roles::LastMessage:
        return tr("This room is possibly inaccessible");
    case Roles::Time:
        return QString();
    case Roles::Timestamp:
        return QVariant{static_cast<quint64>(0)};
    case Roles::HasUnreadMessages:
    case Roles::HasLoudNotification:
        return false;
    case Roles::NotificationCount:
        return 0;
    case Roles::IsInvite:
        return false;
    case Roles::IsSpace:
        return false;
    case Roles::Tags:
        return QStringList();
    case Roles::IsEncrypted:
        return false; // Unknown rooms - assume unencrypted
    default:
        return {};
    }
}

QVariant
RoomlistModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || static_cast<size_t>(index.row()) >= roomids.size())
        return {};

    const auto room_id = roomids.at(index.row());

    if (const auto value = commonRoomData(room_id, role); value.has_value())
        return *value;

    if (models.contains(room_id))
        return dataForMaterializedRoom(room_id, models.value(room_id), role);

    if (cachedJoinedRooms_.contains(room_id))
        return dataForCachedRoom(room_id, cachedJoinedRooms_.value(room_id), role);

    if (invites.contains(room_id))
        return dataForInviteRoom(invites.value(room_id), role);

    if (previewedRooms.contains(room_id)) {
        if (const auto room = previewedRooms.value(room_id); room.has_value())
            return dataForPreviewRoom(room.value(), role);
    }

    return dataForUnavailablePreview(role);
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
}

void
RoomlistModel::connectRoomModelSignals(const QString &room_id,
                                       const QSharedPointer<TimelineModel> &roomModel)
{
    connect(MainWindow::instance(),
            &MainWindow::activeChanged,
            roomModel.data(),
            &TimelineModel::lastReadIdOnWindowFocus);
    connect(roomModel.data(),
            &TimelineModel::newEncryptedImage,
            MainWindow::instance()->imageProvider(),
            &MxcImageProvider::addEncryptionInfo);
    connect(roomModel.data(),
            &TimelineModel::forwardToRoom,
            manager,
            &TimelineViewManager::forwardMessageToRoom);
    connect(roomModel.data(), &TimelineModel::lastMessageChanged, this, [room_id, this]() {
        auto idx = this->roomidToIndex(room_id);
        emit dataChanged(index(idx),
                         index(idx),
                         {
                           Roles::HasLoudNotification,
                           Roles::LastMessage,
                           Roles::Time,
                           Roles::Timestamp,
                           Roles::NotificationCount,
                           Qt::DisplayRole,
                         });
    });
    connect(roomModel.data(), &TimelineModel::roomAvatarUrlChanged, this, [room_id, this]() {
        auto idx = this->roomidToIndex(room_id);
        emit dataChanged(index(idx),
                         index(idx),
                         {
                           Roles::AvatarUrl,
                         });
    });
    connect(roomModel.data(), &TimelineModel::roomNameChanged, this, [room_id, this]() {
        auto idx = this->roomidToIndex(room_id);
        emit dataChanged(index(idx),
                         index(idx),
                         {
                           Roles::RoomName,
                         });
    });
    connect(roomModel.data(), &TimelineModel::notificationsChanged, this, [room_id, this]() {
        auto idx = this->roomidToIndex(room_id);
        emit dataChanged(index(idx),
                         index(idx),
                         {
                           Roles::HasLoudNotification,
                           Roles::NotificationCount,
                           Qt::DisplayRole,
                         });

        if (getRoomById(room_id)->isSpace())
            return; // no need to update space notifications

        int total_unread_msgs = 0;

        for (const auto &room : std::as_const(models)) {
            if (!room.isNull() && !room->isSpace())
                total_unread_msgs += room->notificationCount();
        }

        emit totalUnreadMessageCountUpdated(total_unread_msgs);
    });
}

void
RoomlistModel::restoreRoomDraft(const QString &room_id,
                                const QSharedPointer<TimelineModel> &roomModel)
{
    connect(roomModel->input(),
            &InputBar::draftTextChanged,
            this,
            [room_id, this](const QString &draftText) { persistDraftForRoom(room_id, draftText); });

    if (const auto settings = UserSettings::instance()) {
        const auto restoredDraft = settings->composerDraftForRoom(room_id);
        if (!restoredDraft.trimmed().isEmpty() && roomModel->input()->text().trimmed().isEmpty())
            roomModel->input()->setText(restoredDraft);
    }
}

QSharedPointer<TimelineModel>
RoomlistModel::createRoomModel(const QString &room_id)
{
    QSharedPointer<TimelineModel> roomModel(new TimelineModel(manager, room_id));
    refreshCachedRoomMetadata(room_id);
    auto style = UserSettings::instance()->sidebarsRoomListLastMessagePreview();
    roomModel->setDecryptDescription(style == UserSettings::LastMessagePreview::Always);

    restoreRoomDraft(room_id, roomModel);
    connectRoomModelSignals(room_id, roomModel);

    return roomModel;
}

void
RoomlistModel::addRoom(const QString &room_id, bool suppressInsertNotification, const char *reason)
{
    if (!models.contains(room_id)) {
        if (startupMaterializationTrackingActive_ && !activePrewarms_.contains(room_id)) {
            ++startupMaterializationCount_;
            const bool verboseInfo = manager && manager->roomSwitchPerfEnabled();
            if (verboseInfo) {
                nhlog::ui()->info("[perf][room-list] startup_materialized room_id={} count={} "
                                  "reason={}",
                                  room_id.toStdString(),
                                  startupMaterializationCount_,
                                  reason ? reason : "unknown");
            } else {
                nhlog::ui()->debug("[perf][room-list] startup_materialized room_id={} count={} "
                                   "reason={}",
                                   room_id.toStdString(),
                                   startupMaterializationCount_,
                                   reason ? reason : "unknown");
            }

            constexpr int kStartupMaterializationWarnThreshold = 8;
            if (!startupMaterializationWarningEmitted_ &&
                startupMaterializationCount_ >= kStartupMaterializationWarnThreshold) {
                startupMaterializationWarningEmitted_ = true;
                nhlog::ui()->warn(
                  "[perf][room-list] unexpected eager materialization during startup "
                  "(count={} threshold={} last_room_id={} reason={})",
                  startupMaterializationCount_,
                  kStartupMaterializationWarnThreshold,
                  room_id.toStdString(),
                  reason ? reason : "unknown");
            }
        }

        auto newRoom = createRoomModel(room_id);

        // newRoom->updateLastMessage();

        std::vector<QString> previewsToAdd;
        if (newRoom->isSpace()) {
            auto childs = cache::getChildRoomIds(room_id.toStdString());
            for (const auto &c : childs) {
                auto id = QString::fromStdString(c);
                if (!(models.contains(id) || invites.contains(id) || previewedRooms.contains(id))) {
                    previewsToAdd.push_back(std::move(id));
                }
            }
        }

        bool wasInvite                = invites.contains(room_id);
        bool wasPreview               = previewedRooms.contains(room_id);
        const bool alreadyListed      = roomidToIndex(room_id) != -1;
        const bool insertPrimaryRow   = !alreadyListed && !wasInvite && !wasPreview;
        const int insertedRows        = (insertPrimaryRow ? 1 : 0) + (int)previewsToAdd.size();
        const bool shouldNotifyInsert = !suppressInsertNotification && insertedRows > 0;

        if (shouldNotifyInsert)
            beginInsertRows(
              QModelIndex(), (int)roomids.size(), (int)(roomids.size() + insertedRows - 1));

        models.insert(room_id, std::move(newRoom));
        if (wasInvite) {
            auto idx = roomidToIndex(room_id);
            invites.remove(room_id);
            emit dataChanged(index(idx), index(idx));
        } else if (wasPreview) {
            auto idx = roomidToIndex(room_id);
            previewedRooms.remove(room_id);
            emit dataChanged(index(idx), index(idx));
        } else if (alreadyListed) {
            auto idx = roomidToIndex(room_id);
            emit dataChanged(index(idx),
                             index(idx),
                             {Roles::AvatarUrl,
                              Roles::RoomName,
                              Roles::LastMessage,
                              Roles::Time,
                              Roles::Timestamp,
                              Roles::HasDraft,
                              Roles::DraftPreview,
                              Roles::IsInvite,
                              Roles::IsSpace,
                              Roles::Tags,
                              Roles::IsEncrypted,
                              Qt::DisplayRole});
        } else if (!alreadyListed) {
            roomids.push_back(room_id);
        }

        bool switchedToCurrentPreview = false;
        if ((wasInvite || wasPreview) && currentRoomPreview_ &&
            currentRoomPreview_->roomid() == room_id) {
            currentRoom_ = models.value(room_id);
            currentRoomPreview_.reset();
            if (manager)
                manager->markRoomSwitchPhaseCpp(room_id, "cpp.room_available_from_preview");
            scheduleLastReadUpdate(currentRoom_, room_id);
            emit currentRoomChanged(room_id);
            scheduleCurrentRoomTimelineWarmup(room_id);
            switchedToCurrentPreview = true;
        }

        // Apply deferred focus requested earlier by setCurrentRoom() while this room
        // did not exist in `models` yet (common right after create-room/direct-chat).
        if (pendingCurrentRoomId_ == room_id) {
            pendingCurrentRoomId_.clear();
            if (!switchedToCurrentPreview) {
                currentRoom_ = models.value(room_id);
                currentRoomPreview_.reset();
                if (manager)
                    manager->markRoomSwitchPhaseCpp(room_id, "cpp.pending_room_available");
                scheduleLastReadUpdate(currentRoom_, room_id);
                emit currentRoomChanged(room_id);
                scheduleCurrentRoomTimelineWarmup(room_id);
                nhlog::ui()->debug("Switched to deferred room: {}", room_id.toStdString());

                if (currentRoom_->isSpace())
                    emit spaceSelected(room_id);
            }
        }

        for (auto p : previewsToAdd) {
            previewedRooms.insert(p, std::nullopt);
            roomids.push_back(std::move(p));
        }

        if (shouldNotifyInsert)
            endInsertRows();

        emit ChatPage::instance()->newRoom(room_id);
    }
}

void
RoomlistModel::fetchPreviews(QString roomid_, const std::string &from)
{
    auto roomid = roomid_.toStdString();
    if (from.empty()) {
        // check if we need to fetch anything
        auto children = cache::getChildRoomIds(roomid);
        bool fetch    = false;
        for (const auto &c : children) {
            auto id = QString::fromStdString(c);
            if (invites.contains(id) || models.contains(id) ||
                (previewedRooms.contains(id) && previewedRooms.value(id).has_value()))
                continue;
            else {
                fetch = true;
                break;
            }
        }
        if (!fetch) {
            nhlog::net()->info("Not feching previews for children of {}", roomid);
            return;
        }
    }

    nhlog::net()->info("Feching previews for children of {}", roomid);
    http::client()->get_hierarchy(
      roomid,
      [this, roomid, roomid_](const mtx::responses::HierarchyRooms &h, mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->error("Failed to fetch previews for children of {}: {}", roomid, *err);
              return;
          }

          nhlog::net()->info("Feched previews for children of {}: {}", roomid, h.rooms.size());

          for (const auto &e : h.rooms) {
              RoomInfo info{};
              info.name         = e.name;
              info.is_space     = e.room_type == mtx::events::state::room_type::space;
              info.avatar_url   = e.avatar_url;
              info.topic        = e.topic;
              info.guest_access = e.guest_can_join;
              info.join_rule    = e.join_rule;
              info.member_count = e.num_joined_members;

              emit fetchedPreview(QString::fromStdString(e.room_id), info);
          }

          if (!h.next_batch.empty())
              fetchPreviews(roomid_, h.next_batch);
      },
      from,
      50,
      1,
      false);
}

std::set<QString>
RoomlistModel::updateDMs(mtx::events::AccountDataEvent<mtx::events::account_data::Direct> event)
{
    std::set<QString> roomsToUpdate;
    std::map<QString, std::vector<QString>> directChatToUserTemp;

    for (const auto &[user, rooms] : event.content.user_to_rooms) {
        QString u = QString::fromStdString(user);

        for (const auto &r : rooms) {
            directChatToUserTemp[QString::fromStdString(r)].push_back(u);
        }
    }

    for (auto l = directChatToUser.begin(), r = directChatToUserTemp.begin();
         l != directChatToUser.end() && r != directChatToUserTemp.end();) {
        if (l == directChatToUser.end()) {
            while (r != directChatToUserTemp.end()) {
                roomsToUpdate.insert(r->first);
                ++r;
            }
        } else if (r == directChatToUserTemp.end()) {
            while (l != directChatToUser.end()) {
                roomsToUpdate.insert(l->first);
                ++l;
            }
        } else if (l->first == r->first) {
            if (l->second != r->second)
                roomsToUpdate.insert(l->first);

            ++l;
            ++r;
        } else if (l->first < r->first) {
            roomsToUpdate.insert(l->first);
            ++l;
        } else if (l->first > r->first) {
            roomsToUpdate.insert(r->first);
            ++r;
        } else {
            throw std::logic_error("Infinite loop when updating DMs!");
        }
    }

    this->directChatToUser = directChatToUserTemp;

    return roomsToUpdate;
}

void
RoomlistModel::emitRoomRowUpdate(const QString &room_id)
{
    if (auto idx = roomidToIndex(room_id); idx != -1) {
        emit dataChanged(index(idx),
                         index(idx),
                         {Roles::AvatarUrl,
                          Roles::RoomName,
                          Roles::LastMessage,
                          Roles::Time,
                          Roles::Timestamp,
                          Roles::HasDraft,
                          Roles::DraftPreview,
                          Roles::NotificationCount,
                          Roles::HasLoudNotification,
                          Roles::IsInvite,
                          Roles::IsSpace,
                          Roles::Tags,
                          Roles::IsEncrypted});
    }
}

void
RoomlistModel::syncJoinedRoom(const std::string &room_id, const mtx::responses::JoinedRoom &room)
{
    auto qroomid = QString::fromStdString(room_id);
    const bool shouldMaterialize =
      models.contains(qroomid) || pendingCurrentRoomId_ == qroomid ||
      (currentRoomPreview_ && currentRoomPreview_->roomid() == qroomid);

    if (shouldMaterialize) {
        // addRoom will only add the room, if it doesn't exist
        addRoom(qroomid, false, "sync.materialized_room");
        const auto &room_model = models.value(qroomid);

        if (!room_model.isNull()) {
            // WORKAROUND(Nico): This is not a lambda, but clazy on alpine currently doesn't
            // believe us
            connect(room_model.data(),
                    &TimelineModel::newCallEvent,
                    ChatPage::instance()->callManager(),
                    &CallManager::syncEvent,
                    Qt::UniqueConnection); // clazy:exclude=lambda-unique-connection

            room_model->sync(room);

            if (ChatPage::instance()->userSettings()->timelineTypingShowEnabled()) {
                for (const auto &ev : room.ephemeral.events) {
                    if (auto t =
                          std::get_if<mtx::events::EphemeralEvent<mtx::events::ephemeral::Typing>>(
                            &ev)) {
                        QStringList typing;
                        typing.reserve(t->content.user_ids.size());
                        for (const auto &user : t->content.user_ids) {
                            if (user != utils::localUser().toStdString())
                                typing.push_back(QString::fromStdString(user));
                        }
                        room_model->updateTypingUsers(typing);
                    }
                }
            }
        }
    } else {
        const int existingIdx = roomidToIndex(qroomid);
        if (existingIdx == -1) {
            beginInsertRows(QModelIndex(), (int)roomids.size(), (int)roomids.size());
            roomids.push_back(qroomid);
            endInsertRows();
        }

        if (invites.contains(qroomid))
            invites.remove(qroomid);
        if (previewedRooms.contains(qroomid))
            previewedRooms.remove(qroomid);
    }

    refreshCachedRoomMetadata(qroomid);
    emitRoomRowUpdate(qroomid);
}

void
RoomlistModel::syncLeftRoom(const std::string &room_id)
{
    auto qroomid = QString::fromStdString(room_id);

    if ((currentRoom_ && currentRoom_->roomId() == qroomid) ||
        (currentRoomPreview_ && currentRoomPreview_->roomid() == qroomid))
        resetCurrentRoom();

    auto idx = this->roomidToIndex(qroomid);
    if (idx != -1) {
        beginRemoveRows(QModelIndex(), idx, idx);
        roomids.erase(roomids.begin() + idx);
        endRemoveRows();
    }

    removeRoomState(qroomid);
}

void
RoomlistModel::syncInvitedRoom(const std::string &room_id)
{
    auto qroomid = QString::fromStdString(room_id);

    auto invite = cache::invite(room_id);
    if (!invite)
        return;

    if (invites.contains(qroomid)) {
        invites[qroomid] = *invite;
        auto idx         = roomidToIndex(qroomid);
        emit dataChanged(index(idx), index(idx));
    } else {
        beginInsertRows(QModelIndex(), (int)roomids.size(), (int)roomids.size());
        invites.insert(qroomid, *invite);
        roomids.push_back(std::move(qroomid));
        endInsertRows();
    }
}

void
RoomlistModel::sync(const mtx::responses::Sync &sync_)
{
    for (const auto &e : sync_.account_data.events) {
        if (auto event =
              std::get_if<mtx::events::AccountDataEvent<mtx::events::account_data::Direct>>(&e)) {
            auto updatedDMs = updateDMs(*event);
            for (const auto &r : updatedDMs) {
                if (auto idx = roomidToIndex(r); idx != -1)
                    emit dataChanged(index(idx), index(idx), {IsDirect, DirectChatOtherUserId});
            }
        }
    }

    for (const auto &[room_id, room] : sync_.rooms.join) {
        syncJoinedRoom(room_id, room);
    }

    for (const auto &[room_id, room] : sync_.rooms.leave) {
        (void)room;
        syncLeftRoom(room_id);
    }

    for (const auto &[room_id, room] : sync_.rooms.invite) {
        (void)room;
        syncInvitedRoom(room_id);
    }
}

void
RoomlistModel::initializeRooms()
{
    beginResetModel();
    resetRoomCollections(false);

    auto e = cache::getAccountData(mtx::events::EventType::Direct);
    if (e) {
        if (auto event =
              std::get_if<mtx::events::AccountDataEvent<mtx::events::account_data::Direct>>(
                &e.value())) {
            updateDMs(*event);
        }
    }

    invites               = cache::invites();
    const int inviteCount = invites.size();
    for (auto id = invites.keyBegin(); id != invites.keyEnd(); ++id) {
        roomids.push_back(*id);
    }

    const auto joinedRooms = cache::roomIds();
    for (const auto &id : joinedRooms) {
        roomids.push_back(id);
        refreshCachedRoomMetadata(id);
    }

    nhlog::db()->info("Restored {} rooms from cache (invites={}, joined={}, preview_rows={}, "
                      "models_initialized={})",
                      rowCount(),
                      inviteCount,
                      joinedRooms.size(),
                      previewedRooms.size(),
                      models.size());

    // Track unexpected eager materialization after metadata-only startup.
    startupMaterializationTrackingActive_ = true;

    endResetModel();

    const auto savedRoomId = UserSettings::instance()->currentRoomId();
    if (!savedRoomId.isEmpty() && cachedJoinedRooms_.contains(savedRoomId))
        setCurrentRoom(savedRoomId);

#ifdef KOMAI_DBUS_SYS
    setDbusInterfaceEnabled(MainWindow::instance()->dbusAvailable());
#endif
}

#ifdef KOMAI_DBUS_SYS
void
RoomlistModel::setDbusInterfaceEnabled(bool enabled)
{
    if (enabled) {
        if (dbusInterface_)
            return;

        dbusInterface_ = new DbusBackend{this};
        if (!QDBusConnection::sessionBus().registerObject(
              QStringLiteral("/"), dbusInterface_, QDBusConnection::ExportScriptableSlots)) {
            nhlog::ui()->warn("Failed to register rooms with D-Bus");
            delete dbusInterface_;
            dbusInterface_ = nullptr;
            return;
        }
        return;
    }

    if (!dbusInterface_)
        return;

    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/"));

    delete dbusInterface_;
    dbusInterface_ = nullptr;
}
#endif

void
RoomlistModel::clear()
{
    beginResetModel();
    resetRoomCollections(true);
    emit currentRoomChanged("");
    endResetModel();
}

void
RoomlistModel::joinPreview(const QString &roomid)
{
    if (previewedRooms.contains(roomid)) {
        ChatPage::instance()->joinRoomVia(
          roomid.toStdString(), utils::roomVias(roomid.toStdString()), false);
    }
}
void
RoomlistModel::acceptInvite(QString roomid)
{
    if (invites.contains(roomid)) {
        // Don't remove invite yet, so that we can switch to it
        auto members = cache::getMembersFromInvite(roomid.toStdString(), 0, -1);
        auto local   = utils::localUser();
        for (const auto &m : members) {
            if (m.user_id == local && m.is_direct) {
                nhlog::db()->info("marking {} as direct", roomid.toStdString());
                utils::markRoomAsDirect(roomid, members);
                break;
            }
        }

        ChatPage::instance()->joinRoom(roomid);
    }
}
void
RoomlistModel::declineInvite(QString roomid)
{
    if (invites.contains(roomid)) {
        auto idx = roomidToIndex(roomid);

        if (idx != -1) {
            beginRemoveRows(QModelIndex(), idx, idx);
            roomids.erase(roomids.begin() + idx);
            invites.remove(roomid);
            endRemoveRows();
            ChatPage::instance()->leaveRoom(roomid, "");
        }
    }
}
void
RoomlistModel::leave(QString roomid, QString reason)
{
    // We want to leave in any case, even if this is an invite or similar.
    ChatPage::instance()->leaveRoom(roomid, reason);
    if ((currentRoom_ && currentRoom_->roomId() == roomid) ||
        (currentRoomPreview_ && currentRoomPreview_->roomid() == roomid))
        resetCurrentRoom();

    auto idx = roomidToIndex(roomid);
    if (idx != -1) {
        beginRemoveRows(QModelIndex(), idx, idx);
        roomids.erase(roomids.begin() + idx);
        endRemoveRows();
    }

    removeRoomState(roomid);
}

RoomPreview
RoomlistModel::getRoomPreviewById(QString roomid) const
{
    RoomPreview preview{};

    if (invites.contains(roomid) || previewedRooms.contains(roomid)) {
        std::optional<RoomInfo> i;
        if (invites.contains(roomid)) {
            i                 = invites.value(roomid);
            preview.isInvite_ = true;

            auto member =
              cache::getInviteMember(roomid.toStdString(), utils::localUser().toStdString());

            if (member) {
                preview.reason_ = QString::fromStdString(member->reason);
            }
        } else {
            i                 = previewedRooms.value(roomid);
            preview.isInvite_ = false;
        }

        preview.isFetched_ = i.has_value();

        if (i) {
            preview.roomid_        = roomid;
            preview.roomName_      = QString::fromStdString(i->name);
            preview.roomTopic_     = QString::fromStdString(i->topic);
            preview.roomAvatarUrl_ = QString::fromStdString(i->avatar_url);
        } else {
            preview.roomid_ = roomid;
        }
    }

    return preview;
}

bool
RoomlistModel::isCurrentRoomSelection(const QString &roomid) const
{
    return (currentRoom_ && currentRoom_->roomId() == roomid) ||
           (currentRoomPreview_ && currentRoomPreview_->roomid() == roomid);
}

void
RoomlistModel::clearCurrentRoomSelection()
{
    pendingCurrentRoomId_.clear();
    currentRoom_ = nullptr;
    currentRoomPreview_.reset();
    UserSettings::instance()->setCurrentRoomId(QString());
    emit currentRoomChanged("");
}

bool
RoomlistModel::trySelectCurrentMaterializedRoom(const QString &roomid)
{
    if (!models.contains(roomid))
        return false;

    pendingCurrentRoomId_.clear();
    currentRoom_ = models.value(roomid);
    currentRoomPreview_.reset();
    currentRoom_->updateLastMessage();
    if (manager)
        manager->markRoomSwitchPhaseCpp(roomid, "cpp.room_model_selected");
    scheduleLastReadUpdate(currentRoom_, currentRoom_->roomId());
    UserSettings::instance()->setCurrentRoomId(currentRoom_->roomId());
    emit currentRoomChanged(currentRoom_->roomId());
    scheduleCurrentRoomTimelineWarmup(roomid);
    if (manager)
        manager->markRoomSwitchPhaseCpp(roomid, "cpp.current_room_changed_emitted");
    nhlog::ui()->debug("Switched to: {}", roomid.toStdString());

    if (currentRoom_->isSpace()) {
        emit spaceSelected(roomid);
        if (manager)
            manager->markRoomSwitchPhaseCpp(roomid, "cpp.space_selected_emitted");
    }

    return true;
}

bool
RoomlistModel::trySelectCurrentPreviewRoom(const QString &roomid)
{
    if (!(invites.contains(roomid) || previewedRooms.contains(roomid)))
        return false;

    pendingCurrentRoomId_.clear();
    currentRoom_        = nullptr;
    currentRoomPreview_ = getRoomPreviewById(roomid);

    if (currentRoomPreview_->isFetched()) {
        if (manager)
            manager->markRoomSwitchPhaseCpp(roomid, "cpp.preview_selected");
        nhlog::ui()->debug("Switched to (preview): {}", currentRoomPreview_->roomid_.toStdString());
    } else {
        if (manager)
            manager->markRoomSwitchPhaseCpp(roomid, "cpp.preview_placeholder_selected");
        nhlog::ui()->debug("Switched to (empty): {}", currentRoomPreview_->roomid_.toStdString());
    }

    emit currentRoomChanged("");
    if (manager)
        manager->markRoomSwitchPhaseCpp(roomid, "cpp.current_room_preview_changed_emitted");

    return true;
}

void
RoomlistModel::deferCurrentRoomSelection(const QString &roomid)
{
    pendingCurrentRoomId_ = roomid;
    if (manager)
        manager->markRoomSwitchPhaseCpp(roomid, "cpp.switch_deferred_room_unavailable");
    nhlog::ui()->debug("Deferring room switch until room is available: {}", roomid.toStdString());
}

void
RoomlistModel::setCurrentRoom(const QString &roomid)
{
    if (isCurrentRoomSelection(roomid))
        return;

    if (roomid.isEmpty()) {
        clearCurrentRoomSelection();
        return;
    }

    // After the first explicit room selection, startup eager-materialization tracking
    // is no longer meaningful.
    startupMaterializationTrackingActive_ = false;

    nhlog::ui()->debug("Trying to switch to: {}", roomid.toStdString());
    if (manager)
        manager->markRoomSwitchRequested(roomid, "setCurrentRoom");

    if (!models.contains(roomid) && cachedJoinedRooms_.contains(roomid))
        ensureRoomModel(roomid, false, "setCurrentRoom");

    if (trySelectCurrentMaterializedRoom(roomid))
        return;

    if (trySelectCurrentPreviewRoom(roomid))
        return;

    deferCurrentRoomSelection(roomid);
}

void
RoomlistModel::refetchOnlineKeyBackupKeys()
{
    for (auto i = models.begin(); i != models.end(); ++i) {
        auto ptr = i.value();

        if (!ptr.isNull()) {
            ptr->refetchOnlineKeyBackupKeys();
        }
    }
}

namespace {
enum NotificationImportance : short
{
    NoPreview          = -3,
    Preview            = -2,
    ImportanceDisabled = -1,
    AllEventsRead      = 0,
    Draft              = 1,
    NewMessage         = 2,
    NewMentions        = 3,
    Invite             = 4,
    SubSpace           = 5,
    CurrentSpace       = 6,
};
}

short int
FilteredRoomlistModel::calculateImportance(const QModelIndex &idx) const
{
    // Returns the degree of importance of the unread messages in the room.
    // If sorting by importance is disabled in settings, this only ever
    // returns ImportanceDisabled or Invite
    if (sourceModel()->data(idx, RoomlistModel::IsSpace).toBool()) {
        if (filterType == FilterBy::Space &&
            filterStr == sourceModel()->data(idx, RoomlistModel::RoomId).toString())
            return CurrentSpace;
        else
            return SubSpace;
    } else if (sourceModel()->data(idx, RoomlistModel::IsPreview).toBool()) {
        if (sourceModel()->data(idx, RoomlistModel::IsPreviewFetched).toBool())
            return Preview;
        else
            return NoPreview;
    } else if (sourceModel()->data(idx, RoomlistModel::IsInvite).toBool()) {
        return Invite;
    } else if (this->sidebarsRoomListSort ==
                 static_cast<int>(UserSettings::RoomSortOrder::Recent) ||
               this->sidebarsRoomListSort ==
                 static_cast<int>(UserSettings::RoomSortOrder::Alphabetical)) {
        return ImportanceDisabled;
    } else if (sourceModel()->data(idx, RoomlistModel::HasLoudNotification).toBool()) {
        return NewMentions;
    } else if (sourceModel()->data(idx, RoomlistModel::NotificationCount).toInt() > 0) {
        return NewMessage;
    } else if (sourceModel()->data(idx, RoomlistModel::HasDraft).toBool()) {
        return Draft;
    } else {
        return AllEventsRead;
    }
}

bool
FilteredRoomlistModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    QModelIndex const left_idx  = sourceModel()->index(left.row(), 0, QModelIndex());
    QModelIndex const right_idx = sourceModel()->index(right.row(), 0, QModelIndex());

    // Sort by "importance" (i.e. invites before mentions before
    // notifs before new events before old events), then secondly
    // by recency.

    // Checking importance first
    const auto a_importance = calculateImportance(left_idx);
    const auto b_importance = calculateImportance(right_idx);
    if (a_importance != b_importance) {
        return a_importance > b_importance;
    }

    // Now sort by recency or room name
    // Zero if empty, otherwise the time that the event occured

    bool sortAlphabetically =
      (this->sidebarsRoomListSort ==
         static_cast<int>(UserSettings::RoomSortOrder::UnreadFirst_Alpha) ||
       this->sidebarsRoomListSort == static_cast<int>(UserSettings::RoomSortOrder::Alphabetical));

    if (sortAlphabetically) {
        QString a_order = sourceModel()->data(left_idx, RoomlistModel::RoomName).toString();
        QString b_order = sourceModel()->data(right_idx, RoomlistModel::RoomName).toString();

        auto comp = a_order.compare(b_order, Qt::CaseInsensitive);
        if (comp != 0)
            return comp < 0;
    } else {
        uint64_t a_order = sourceModel()->data(left_idx, RoomlistModel::Timestamp).toULongLong();
        uint64_t b_order = sourceModel()->data(right_idx, RoomlistModel::Timestamp).toULongLong();

        if (a_order != b_order)
            return a_order > b_order;
    }

    return left.row() < right.row();
}

FilteredRoomlistModel::FilteredRoomlistModel(RoomlistModel *model, QObject *parent)
  : QSortFilterProxyModel(parent)
  , roomlistmodel(model)
{
    instance_ = this;

    this->sidebarsRoomListSort = static_cast<int>(UserSettings::instance()->sidebarsRoomListSort());
    setSourceModel(model);
    setDynamicSortFilter(true);

    QObject::connect(UserSettings::instance().get(),
                     &UserSettings::sidebarsRoomListSortChanged,
                     this,
                     [this](UserSettings::RoomSortOrder order) {
                         this->sidebarsRoomListSort = static_cast<int>(order);
                         invalidate();
                     });

    connect(roomlistmodel,
            &RoomlistModel::currentRoomChanged,
            this,
            &FilteredRoomlistModel::currentRoomChanged);

    sort(0);
}

FilteredRoomlistModel *
FilteredRoomlistModel::create(QQmlEngine *qmlEngine, QJSEngine *)
{
    // The instance has to exist before it is used. We cannot replace it.
    Q_ASSERT(instance_);

    // The engine has to have the same thread affinity as the singleton.
    Q_ASSERT(qmlEngine->thread() == instance_->thread());

    // There can only be one engine accessing the singleton.
    static QJSEngine *s_engine = nullptr;
    if (s_engine)
        Q_ASSERT(qmlEngine == s_engine);
    else
        s_engine = qmlEngine;

    QJSEngine::setObjectOwnership(instance_, QJSEngine::CppOwnership);
    return instance_;
}

TimelineModel *
FilteredRoomlistModel::getRoomById(const QString &id) const
{
    auto r = roomlistmodel->getRoomByIdWithReason(id, "qml.Rooms.getRoomById").data();
    QQmlEngine::setObjectOwnership(r, QQmlEngine::CppOwnership);
    return r;
}

void
FilteredRoomlistModel::updateHiddenTagsAndSpaces()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    beginFilterChange();
#endif

    hiddenTags.clear();
    hiddenSpaces.clear();
    hideDMs = false;

    auto hidden = UserSettings::instance()->hiddenTags();
    for (const auto &t : std::as_const(hidden)) {
        if (t.startsWith(u"tag:"))
            hiddenTags.push_back(t.mid(4));
        else if (t.startsWith(u"space:"))
            hiddenSpaces.push_back(t.mid(6));
        else if (t == QLatin1String("dm"))
            hideDMs = true;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    endFilterChange();
#else
    invalidateFilter();
#endif
}

QModelIndex
FilteredRoomlistModel::sourceRowIndex(int sourceRow) const
{
    return sourceModel()->index(sourceRow, 0);
}

bool
FilteredRoomlistModel::isPreviewRow(int sourceRow) const
{
    return sourceModel()->data(sourceRowIndex(sourceRow), RoomlistModel::IsPreview).toBool();
}

bool
FilteredRoomlistModel::isSpaceRow(int sourceRow) const
{
    return sourceModel()->data(sourceRowIndex(sourceRow), RoomlistModel::IsSpace).toBool();
}

bool
FilteredRoomlistModel::isDirectRow(int sourceRow) const
{
    return sourceModel()->data(sourceRowIndex(sourceRow), RoomlistModel::IsDirect).toBool();
}

QStringList
FilteredRoomlistModel::rowTags(int sourceRow) const
{
    return sourceModel()->data(sourceRowIndex(sourceRow), RoomlistModel::Tags).toStringList();
}

QStringList
FilteredRoomlistModel::rowParentSpaces(int sourceRow) const
{
    return sourceModel()
      ->data(sourceRowIndex(sourceRow), RoomlistModel::ParentSpaces)
      .toStringList();
}

bool
FilteredRoomlistModel::hiddenByTags(int sourceRow, const QString &requiredTag) const
{
    if (hiddenTags.empty())
        return false;

    const auto tags = rowTags(sourceRow);
    for (const auto &tag : tags) {
        if (!requiredTag.isEmpty() && tag == requiredTag)
            continue;
        if (hiddenTags.contains(tag))
            return true;
    }

    return false;
}

bool
FilteredRoomlistModel::hiddenBySpaces(int sourceRow, const QString &requiredSpace) const
{
    if (hiddenSpaces.empty())
        return false;

    const auto parents = rowParentSpaces(sourceRow);
    for (const auto &space : parents) {
        if (!requiredSpace.isEmpty() && space == requiredSpace)
            continue;
        if (hiddenSpaces.contains(space))
            return true;
    }

    return false;
}

bool
FilteredRoomlistModel::hiddenByDms(int sourceRow) const
{
    return hideDMs && isDirectRow(sourceRow);
}

bool
FilteredRoomlistModel::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    if (filterType == FilterBy::Nothing) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;
        if (hiddenByTags(sourceRow) || hiddenBySpaces(sourceRow) || hiddenByDms(sourceRow))
            return false;
        return true;
    } else if (filterType == FilterBy::DirectChats) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;
        if (hiddenByTags(sourceRow) || hiddenBySpaces(sourceRow))
            return false;
        return isDirectRow(sourceRow);
    } else if (filterType == FilterBy::Tag) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;

        const auto tags = rowTags(sourceRow);
        if (!tags.contains(filterStr))
            return false;
        if (hiddenByTags(sourceRow, filterStr) || hiddenBySpaces(sourceRow) ||
            hiddenByDms(sourceRow))
            return false;
        return true;
    } else if (filterType == FilterBy::Space) {
        const auto idx = sourceRowIndex(sourceRow);
        if (filterStr == sourceModel()->data(idx, RoomlistModel::RoomId).toString())
            return true;

        const auto parents = rowParentSpaces(sourceRow);
        if (!parents.contains(filterStr))
            return false;

        if (hiddenByTags(sourceRow) || hiddenBySpaces(sourceRow, filterStr) ||
            hiddenByDms(sourceRow))
            return false;

        if (isSpaceRow(sourceRow) && !parents.contains(filterStr))
            return false;

        // If it is a preview but it can't be fetched, it is probably an inaccessible private room.
        // Hide it if the user isn't an admin.
        if (isPreviewRow(sourceRow) &&
            !sourceModel()->data(idx, RoomlistModel::IsPreviewFetched).toBool() &&
            !Permissions(filterStr).canChange(qml_mtx_events::SpaceChild)) {
            return false;
        }

        return true;
    } else {
        return true;
    }
}

void
FilteredRoomlistModel::toggleTag(const QString &roomid, const QString &tag, bool on)
{
    if (on) {
        http::client()->put_tag(
          roomid.toStdString(), tag.toStdString(), {}, [tag](mtx::http::RequestErr err) {
              if (err) {
                  nhlog::ui()->error(
                    "Failed to add tag: {}, {}", tag.toStdString(), err->matrix_error.error);
              }
          });
    } else {
        http::client()->delete_tag(
          roomid.toStdString(), tag.toStdString(), [tag](mtx::http::RequestErr err) {
              if (err) {
                  nhlog::ui()->error(
                    "Failed to delete tag: {}, {}", tag.toStdString(), err->matrix_error.error);
              }
          });
    }
}

void
FilteredRoomlistModel::copyLink(QString roomid)
{
    auto link = QStringLiteral("%1?%2").arg(TimelineModel::getBareRoomLink(roomid),
                                            TimelineModel::getRoomVias(roomid));
    QGuiApplication::clipboard()->setText(link);
}

void
FilteredRoomlistModel::nextRoomWithActivity()
{
    int roomWithMention       = -1;
    int roomWithNotification  = -1;
    int roomWithUnreadMessage = -1;
    auto r                    = currentRoom();
    int currentRoomIdx        = r ? roomidToIndex(r->roomId()) : -1;
    // first look for mentions
    for (int i = 0; i < (int)roomlistmodel->roomids.size(); i++) {
        if (i == currentRoomIdx)
            continue;
        if (this->data(index(i, 0), RoomlistModel::HasLoudNotification).toBool()) {
            roomWithMention = i;
            break;
        }
        if (roomWithNotification == -1 &&
            this->data(index(i, 0), RoomlistModel::NotificationCount).toInt() > 0) {
            roomWithNotification = i;
            // don't break, we must continue looking for rooms with mentions
        }
        if (roomWithNotification == -1 && roomWithUnreadMessage == -1 &&
            this->data(index(i, 0), RoomlistModel::HasUnreadMessages).toBool()) {
            roomWithUnreadMessage = i;
            // don't break, we must continue looking for rooms with mentions
        }
    }
    QString targetRoomId = nullptr;
    if (roomWithMention != -1) {
        targetRoomId = this->data(index(roomWithMention, 0), RoomlistModel::RoomId).toString();
        nhlog::ui()->debug("choosing {} for mentions", targetRoomId.toStdString());
    } else if (roomWithNotification != -1) {
        targetRoomId = this->data(index(roomWithNotification, 0), RoomlistModel::RoomId).toString();
        nhlog::ui()->debug("choosing {} for notifications", targetRoomId.toStdString());
    } else if (roomWithUnreadMessage != -1) {
        targetRoomId =
          this->data(index(roomWithUnreadMessage, 0), RoomlistModel::RoomId).toString();
        nhlog::ui()->debug("choosing {} for unread messages", targetRoomId.toStdString());
    }
    if (targetRoomId != nullptr) {
        setCurrentRoom(targetRoomId);
    }
}

void
FilteredRoomlistModel::nextRoom()
{
    auto r = currentRoom();

    if (r) {
        int idx = roomidToIndex(r->roomId());
        idx++;
        if (idx < rowCount()) {
            setCurrentRoom(data(index(idx, 0), RoomlistModel::Roles::RoomId).toString());
        }
    }
}

void
FilteredRoomlistModel::previousRoom()
{
    auto r = currentRoom();

    if (r) {
        int idx = roomidToIndex(r->roomId());
        idx--;
        if (idx >= 0) {
            setCurrentRoom(data(index(idx, 0), RoomlistModel::Roles::RoomId).toString());
        }
    }
}

QString
RoomPreview::inviterAvatarUrl() const
{
    if (isInvite_) {
        auto self = cache::getInviteMember(roomid_.toStdString(), utils::localUser().toStdString());
        if (self && !self->inviter.empty()) {
            auto other = cache::getInviteMember(roomid_.toStdString(), self->inviter);
            if (other && other->avatar_url.starts_with("mxc://")) {
                return QString::fromStdString(other->avatar_url);
            }
        }
    }

    return QString();
}
QString
RoomPreview::inviterDisplayName() const
{
    if (isInvite_) {
        auto self = cache::getInviteMember(roomid_.toStdString(), utils::localUser().toStdString());
        if (self && !self->inviter.empty()) {
            auto other = cache::getInviteMember(roomid_.toStdString(), self->inviter);
            if (other) {
                return QString::fromStdString(other->name).toHtmlEscaped();
            }
        }
    }

    return QString();
}
QString
RoomPreview::inviterUserId() const
{
    if (isInvite_) {
        auto self = cache::getInviteMember(roomid_.toStdString(), utils::localUser().toStdString());
        if (self && !self->inviter.empty()) {
            return QString::fromStdString(self->inviter);
        }
    }

    return QString();
}

#include "moc_RoomlistModel.cpp"

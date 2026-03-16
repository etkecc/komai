// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include <algorithm>
#include <utility>

#include <QDateTime>

#include "TimelineViewManager.h"
#include "cache/Cache.h"
#include "cache/api/CacheApiTimeline.h"
#include "chat/ChatPage.h"
#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "models/ReadReceiptsModel.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/send/TimelineMessageSendPipeline.h"
#include "utils/Utils.h"

namespace std {
inline uint // clazy:exclude=qhash-namespace
qHash(const std::string &key, uint seed = 0)
{
    return qHash(QByteArray::fromRawData(key.data(), (int)key.length()), seed);
}
}

TimelineModel::~TimelineModel()
{
    nhlog::ui()->info("[lru] ~TimelineModel room_id={}", room_id_.toStdString());
}

TimelineModel::TimelineModel(TimelineViewManager *manager, QString room_id, QObject *parent)
  : QAbstractListModel(parent)
  , room_id_(std::move(room_id))
  , events(room_id_.toStdString(), this)
  , mediaController_(room_id_,
                     events,
                     [this](const QString &mxcUrl, const QString &cacheUrl) {
                         emit mediaCached(mxcUrl, cacheUrl);
                     })
  , manager_(manager)
  , permissions_{room_id_}
{
    this->isEncrypted_ = cache::isRoomEncrypted(room_id_.toStdString());

    auto roomInfo            = cache::singleRoomInfo(room_id_.toStdString());
    this->isSpace_           = roomInfo.is_space;
    this->isPublic_          = roomInfo.join_rule == mtx::events::state::JoinRule::Public;
    this->notification_count = roomInfo.notification_count;
    this->highlight_count    = roomInfo.highlight_count;
    lastMessage_.timestamp   = roomInfo.approximate_last_modification_ts;

    connect(
      this,
      &TimelineModel::redactionFailed,
      this,
      [](const QString &msg) { emit ChatPage::instance()->showNotification(msg); },
      Qt::QueuedConnection);

    connect(UserSettings::instance().get(), &UserSettings::uiThemeSlugChanged, this, [this]() {
        if (events.size() > 0)
            emit dataChanged(
              index(0, 0), index(events.size() - 1, 0), {FormattedBody, FormattedStateEvent});
    });

    connect(this, &TimelineModel::dataAtIdChanged, this, [this](const QString &id) {
        relatedEventCacheBuster++;

        auto idx = idToIndex(id);
        if (idx != -1) {
            auto pos = index(idx);
            nhlog::ui()->debug("data changed at {}", id.toStdString());
            emit dataChanged(pos, pos);
        } else {
            nhlog::ui()->debug("id not found {}", id.toStdString());
        }
    });

    connect(this,
            &TimelineModel::newMessageToSend,
            this,
            &TimelineModel::addPendingMessage,
            Qt::QueuedConnection);
    connect(this, &TimelineModel::addPendingMessageToStore, &events, &EventStore::addPending);

    connect(&events, &EventStore::dataChanged, this, [this](int from, int to) {
        relatedEventCacheBuster++;
        invalidateFrequentReactionsCache();
        nhlog::ui()->debug(
          "data changed {} to {}", events.size() - to - 1, events.size() - from - 1);
        emit dataChanged(index(events.size() - to - 1, 0), index(events.size() - from - 1, 0));

        if (!decryptDescription)
            return;

        if (lastMessage_.event_id.isEmpty()) {
            updateLastMessage();
            return;
        }

        const auto lastMessageIdx = events.idToIndex(lastMessage_.event_id.toStdString());
        if (!lastMessageIdx) {
            updateLastMessage();
            return;
        }

        const auto changedFrom = std::min(from, to);
        const auto changedTo   = std::max(from, to);
        if (*lastMessageIdx >= changedFrom && *lastMessageIdx <= changedTo)
            updateLastMessage();
    });
    connect(&events, &EventStore::pinsChanged, this, &TimelineModel::pinnedMessagesChanged);

    connect(&events, &EventStore::beginInsertRows, this, [this](int from, int to) {
        int first = events.size() - to;
        int last  = events.size() - from;
        if (from >= events.size()) {
            int batch_size = to - from;
            first += batch_size;
            last += batch_size;
        } else {
            first -= 1;
            last -= 1;
        }
        nhlog::ui()->debug("begin insert from {} to {}", first, last);
        beginInsertRows(QModelIndex(), first, last);
    });
    connect(&events, &EventStore::endInsertRows, this, [this]() { endInsertRows(); });
    connect(&events, &EventStore::beginResetModel, this, [this]() { beginResetModel(); });
    connect(&events, &EventStore::endResetModel, this, [this]() { endResetModel(); });
    connect(&events, &EventStore::newEncryptedImage, this, &TimelineModel::newEncryptedImage);
    connect(&events, &EventStore::fetchedMore, this, [this]() {
        setPaginationInProgress(false);
        updateLastMessage();
        emit fetchedMore();
    });
    connect(&events, &EventStore::fetchedMore, this, &TimelineModel::checkAfterFetch);
    connect(&events,
            &EventStore::startDMVerification,
            this,
            [this](const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationRequest> &msg) {
                ChatPage::instance()->receivedRoomDeviceVerificationRequest(msg, this);
            });
    connect(&events, &EventStore::updateFlowEventId, this, [this](std::string event_id) {
        this->updateFlowEventId(std::move(event_id));
    });

    // When a message is sent, check if the current edit/reply relates to that message,
    // and update the event_id so that it points to the sent message and not the pending one.
    connect(
      &events,
      &EventStore::messageSent,
      this,
      [this](const std::string &txn_id, const std::string &event_id) {
          if (edit_.toStdString() == txn_id) {
              edit_ = QString::fromStdString(event_id);
              emit editChanged(edit_);
          }
          if (reply_.toStdString() == txn_id) {
              reply_ = QString::fromStdString(event_id);
              emit replyChanged(reply_);
          }
      },
      Qt::QueuedConnection);

    connect(manager_,
            &TimelineViewManager::waitingForFirstSyncChanged,
            &events,
            &EventStore::enableKeyRequests);

    connect(this, &TimelineModel::encryptionChanged, this, &TimelineModel::trustlevelChanged);
    connect(this, &TimelineModel::roomMemberCountChanged, this, &TimelineModel::trustlevelChanged);
    cache::onVerificationStatusChanged(this,
                                       [this](const std::string &) { emit trustlevelChanged(); });

    showEventTimer.callOnTimeout(this, &TimelineModel::scrollTimerEvent);

    connect(this, &TimelineModel::newState, this, [this](mtx::responses::StateEvents events_) {
        cache::updateState(room_id_.toStdString(), events_, true);
        this->syncState({std::move(events_.events)});
    });
}

QHash<int, QByteArray>
TimelineModel::roleNames() const
{
    static QHash<int, QByteArray> roles{
      {Type, "type"},
      {TypeString, "typeString"},
      {IsOnlyEmoji, "isOnlyEmoji"},
      {Body, "body"},
      {FormattedBody, "formattedBody"},
      {HasFormattedBody, "hasFormattedBody"},
      {FormattedStateEvent, "formattedStateEvent"},
      {StateEventIconSource, "stateEventIconSource"},
      {IsSender, "isSender"},
      {UserId, "userId"},
      {UserName, "userName"},
      {UserPowerlevel, "userPowerlevel"},
      {Day, "day"},
      {Timestamp, "timestamp"},
      {Url, "url"},
      {ThumbnailUrl, "thumbnailUrl"},
      {Duration, "duration"},
      {Blurhash, "blurhash"},
      {Filename, "filename"},
      {Filesize, "filesize"},
      {FilesizeBytes, "filesizeBytes"},
      {MimeType, "mimetype"},
      {OriginalHeight, "originalHeight"},
      {OriginalWidth, "originalWidth"},
      {ProportionalHeight, "proportionalHeight"},
      {EventId, "eventId"},
      {State, "status"},
      {IsEdited, "isEdited"},
      {IsEditable, "isEditable"},
      {IsEncrypted, "isEncrypted"},
      {IsStateEvent, "isStateEvent"},
      {Trustlevel, "trustlevel"},
      {Notificationlevel, "notificationlevel"},
      {EncryptionError, "encryptionError"},
      {ReplyTo, "replyTo"},
      {ThreadId, "threadId"},
      {Reactions, "reactions"},
      {Room, "room"},
      {RoomId, "roomId"},
      {RoomName, "roomName"},
      {RoomTopic, "roomTopic"},
      {CallType, "callType"},
      {Dump, "dump"},
      {RelatedEventCacheBuster, "relatedEventCacheBuster"},
      {IsHiddenEvent, "isHiddenEvent"},
    };

    return roles;
}
int
TimelineModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return this->events.size();
}

QVariantMap
TimelineModel::getDump(const QString &eventId, const QString &relatedTo) const
{
    if (auto event = events.get(eventId.toStdString(), relatedTo.toStdString()))
        return data(*event, Dump).toMap();
    return {};
}

// Workaround. We also want to see a room at the top, if we just joined it
auto
isYourJoin(const mtx::events::StateEvent<mtx::events::state::Member> &e, EventStore &events)
{
    if (e.content.membership == mtx::events::state::Membership::Join &&
        e.state_key == utils::localUser().toStdString() &&
        !e.unsigned_data.replaces_state.empty()) {
        auto tempPrevEvent = events.get(e.unsigned_data.replaces_state, e.event_id);
        if (tempPrevEvent) {
            if (auto prevEvent =
                  std::get_if<mtx::events::StateEvent<mtx::events::state::Member>>(tempPrevEvent)) {
                if (prevEvent->content.membership != mtx::events::state::Membership::Join)
                    return true;
            }
        }
    }
    return false;
}
template<typename T>
auto
isYourJoin(const mtx::events::Event<T> &, EventStore &)
{
    return false;
}

DescInfo
TimelineModel::lastMessage() const
{
    if (lastMessage_.event_id.isEmpty())
        QTimer::singleShot(0, this, &TimelineModel::updateLastMessage);

    return lastMessage_;
}

void
TimelineModel::updateLastMessage()
{
    // only try to generate a preview for the last 1000 messages
    auto end = std::max(events.size() - 1001, 0);
    for (auto it = events.size() - 1; it >= end; --it) {
        auto event = events.get(it, decryptDescription);
        if (!event)
            continue;

        if (std::visit([this](const auto &e) -> bool { return isYourJoin(e, events); }, *event)) {
            auto time        = mtx::accessors::origin_server_ts(*event);
            uint64_t ts      = time.toMSecsSinceEpoch();
            auto description = DescInfo{QString::fromStdString(mtx::accessors::event_id(*event)),
                                        utils::localUser(),
                                        tr("You joined this room."),
                                        utils::descriptiveTime(time),
                                        ts,
                                        time};
            if (description != lastMessage_) {
                if (lastMessage_.timestamp == 0) {
                    cache::updateLastMessageTimestamp(room_id_.toStdString(),
                                                      description.timestamp);
                }
                lastMessage_ = description;
                emit lastMessageChanged();
            }
            return;
        }
        if (!mtx::accessors::is_message(*event))
            continue;

        auto description = utils::getMessageDescription(
          *event,
          utils::localUser(),
          cache::displayName(room_id_, QString::fromStdString(mtx::accessors::sender(*event))));
        if (description != lastMessage_) {
            if (lastMessage_.timestamp == 0) {
                cache::updateLastMessageTimestamp(room_id_.toStdString(), description.timestamp);
            }
            lastMessage_ = description;
            emit lastMessageChanged();
        }
        return;
    }
}

void
TimelineModel::addPendingMessage(mtx::events::collections::TimelineEvents event)
{
    timeline::send::sendPendingMessage(
      room_id_,
      std::move(event),
      [this](mtx::events::collections::TimelineEvents pendingMessage) {
          emit addPendingMessageToStore(std::move(pendingMessage));
      },
      [this](const mtx::crypto::EncryptedFile &encryptionInfo) {
          emit newEncryptedImage(encryptionInfo);
      },
      [this]() {
          emit ChatPage::instance()->showNotification(
            tr("Failed to encrypt event, sending aborted!"));
      });

    fullyReadEventId_ = this->EventId;
    emit fullyReadEventIdChanged();
}

void
TimelineModel::openMedia(const QString &eventId)
{
    mediaController_.openMedia(eventId);
}

bool
TimelineModel::saveMedia(const QString &eventId) const
{
    return mediaController_.saveMedia(eventId);
}

bool
TimelineModel::copyMedia(const QString &eventId) const
{
    return mediaController_.copyMedia(eventId);
}

void
TimelineModel::cacheMedia(const QString &eventId,
                          const std::function<void(const QString)> &callback)
{
    mediaController_.cacheMedia(eventId, callback);
}

void
TimelineModel::cacheMedia(const QString &eventId)
{
    cacheMedia(eventId, nullptr);
}

QStringList
TimelineModel::frequentReactions() const
{
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (frequentReactionsCache_.has_value() &&
        (now - frequentReactionsCacheTimestamp_) <
          settings::core::definitions::kReactionFrequencyCacheDurationMs) {
        return *frequentReactionsCache_;
    }

    auto result =
      cache::topUserReactions(room_id_.toStdString(),
                              settings::core::definitions::kReactionFrequencyLookbackDays,
                              settings::core::definitions::kMaxQuickReactionSlots);

    QStringList list;
    list.reserve(static_cast<int>(result.size()));
    for (const auto &r : result)
        list.append(QString::fromStdString(r));

    frequentReactionsCache_          = list;
    frequentReactionsCacheTimestamp_ = now;
    return list;
}

void
TimelineModel::invalidateFrequentReactionsCache()
{
    frequentReactionsCache_.reset();
    emit frequentReactionsChanged();
}

#include "moc_TimelineModel.cpp"

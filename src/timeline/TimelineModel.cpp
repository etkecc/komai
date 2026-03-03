// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include <algorithm>
#include <type_traits>
#include <utility>

#include <QFont>
#include <QFontMetrics>
#include <QScreen>
#include <QVariant>

#include "ChatPage.h"
#include "Config.h"
#include "EventAccessors.h"
#include "FormattedCodeBlockHighlighter.h"
#include "Logging.h"
#include "MatrixClient.h"
#include "ReadReceiptsModel.h"
#include "RoomlistModel.h"
#include "TimelineViewManager.h"
#include "Utils.h"
#include "cache/Cache.h"
#include "encryption/Olm.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/format/TimelineImagePackFormatter.h"
#include "timeline/format/TimelineMemberEventFormatter.h"
#include "timeline/format/TimelinePolicyRuleFormatter.h"
#include "timeline/format/TimelinePowerLevelFormatter.h"
#include "timeline/format/TimelineRedactedEventFormatter.h"
#include "timeline/send/TimelineMessageSendPipeline.h"
#include "ui/Theme.h"

namespace std {
inline uint // clazy:exclude=qhash-namespace
qHash(const std::string &key, uint seed = 0)
{
    return qHash(QByteArray::fromRawData(key.data(), (int)key.length()), seed);
}
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
    this->notification_count = roomInfo.notification_count;
    this->highlight_count    = roomInfo.highlight_count;
    lastMessage_.timestamp   = roomInfo.approximate_last_modification_ts;

    connect(
      this,
      &TimelineModel::redactionFailed,
      this,
      [](const QString &msg) { emit ChatPage::instance()->showNotification(msg); },
      Qt::QueuedConnection);

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

bool
TimelineModel::canFetchMore(const QModelIndex &) const
{
    if (!events.size())
        return true;
    // When the virtual window can still expand from cached DB entries,
    // return false to prevent Qt from auto-triggering fetchMore on model
    // assignment. The data() hack handles expansion on user scroll instead.
    if (events.canExpandWindow())
        return false;
    if (auto first = events.get(0);
        first &&
        !std::holds_alternative<mtx::events::StateEvent<mtx::events::state::Create>>(*first))
        return true;
    else

        return false;
}

void
TimelineModel::setPaginationInProgress(const bool paginationInProgress)
{
    if (m_paginationInProgress == paginationInProgress) {
        return;
    }

    m_paginationInProgress = paginationInProgress;
    emit paginationInProgressChanged(m_paginationInProgress);

    if (m_paginationInProgress) {
        // Expand cached history in chunks. Do not loop to exhaustion here:
        // with small initial windows this can eagerly inflate to the full room
        // during first paint and erase the performance benefit.
        if (events.canExpandWindow()) {
            events.expandWindow();
            setPaginationInProgress(false);
            emit fetchedMore();
            return;
        }
        events.fetchMore();
    }
}

bool
TimelineModel::canExpandWindow() const
{
    return events.canExpandWindow();
}

bool
TimelineModel::canPaginateBack() const
{
    return events.canExpandWindow() || canFetchMore(QModelIndex{});
}

void
TimelineModel::fetchMore(const QModelIndex &)
{
    if (m_paginationInProgress) {
        nhlog::ui()->warn("Already loading older messages");
        return;
    }

    setPaginationInProgress(true);
}

void
TimelineModel::sync(const mtx::responses::JoinedRoom &room)
{
    this->syncState(room.state);
    this->addEvents(room.timeline);

    if (room.unread_notifications.highlight_count != highlight_count ||
        room.unread_notifications.notification_count != notification_count) {
        notification_count = room.unread_notifications.notification_count;
        highlight_count    = room.unread_notifications.highlight_count;
        emit notificationsChanged();
    }
}

void
TimelineModel::syncState(const mtx::responses::State &s)
{
    bool avatarChanged      = false;
    bool nameChanged        = false;
    bool memberCountChanged = false;

    for (const auto &e : s.events) {
        applyStateEventSideEffects(e, avatarChanged, nameChanged, memberCountChanged);
    }

    emitRoomMetadataChanges(avatarChanged, nameChanged, memberCountChanged);
}

bool
TimelineModel::applyStateEventSideEffects(const mtx::events::collections::TimelineEvents &event,
                                          bool &avatarChanged,
                                          bool &nameChanged,
                                          bool &memberCountChanged)
{
    using namespace mtx::events;

    if (std::holds_alternative<StateEvent<state::Avatar>>(event)) {
        avatarChanged = true;
        return true;
    } else if (std::holds_alternative<StateEvent<state::Name>>(event)) {
        nameChanged = true;
        return true;
    } else if (std::holds_alternative<StateEvent<state::Topic>>(event)) {
        emit roomTopicChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::PinnedEvents>>(event)) {
        emit pinnedMessagesChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::Widget>>(event)) {
        emit widgetLinksChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::PowerLevels>>(event)) {
        permissions_.invalidate();
        emit permissionsChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::Member>>(event)) {
        avatarChanged      = true;
        nameChanged        = true;
        memberCountChanged = true;
        return true;
    } else if (std::holds_alternative<StateEvent<state::Encryption>>(event)) {
        this->isEncrypted_ = cache::isRoomEncrypted(room_id_.toStdString());
        emit encryptionChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::space::Parent>>(event)) {
        this->parentChecked = false;
        emit parentSpaceChanged();
        return true;
    }

    return false;
}

bool
TimelineModel::applyStateEventSideEffects(const mtx::events::collections::StateEvents &event,
                                          bool &avatarChanged,
                                          bool &nameChanged,
                                          bool &memberCountChanged)
{
    using namespace mtx::events;

    if (std::holds_alternative<StateEvent<state::Avatar>>(event)) {
        avatarChanged = true;
        return true;
    } else if (std::holds_alternative<StateEvent<state::Name>>(event)) {
        nameChanged = true;
        return true;
    } else if (std::holds_alternative<StateEvent<state::Topic>>(event)) {
        emit roomTopicChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::PinnedEvents>>(event)) {
        emit pinnedMessagesChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::Widget>>(event)) {
        emit widgetLinksChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::PowerLevels>>(event)) {
        permissions_.invalidate();
        emit permissionsChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::Member>>(event)) {
        avatarChanged      = true;
        nameChanged        = true;
        memberCountChanged = true;
        return true;
    } else if (std::holds_alternative<StateEvent<state::Encryption>>(event)) {
        this->isEncrypted_ = cache::isRoomEncrypted(room_id_.toStdString());
        emit encryptionChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::space::Parent>>(event)) {
        this->parentChecked = false;
        emit parentSpaceChanged();
        return true;
    }

    return false;
}

void
TimelineModel::emitRoomMetadataChanges(bool avatarChanged,
                                       bool nameChanged,
                                       bool memberCountChanged)
{
    if (avatarChanged)
        emit roomAvatarUrlChanged();
    if (nameChanged)
        emit roomNameChanged();

    if (memberCountChanged) {
        emit roomMemberCountChanged();
        if (roomMemberCount() <= 2) {
            emit isDirectChanged();
            emit directChatOtherUserIdChanged();
        }
    }
}

bool
TimelineModel::dispatchCallEventIfNeeded(mtx::events::collections::TimelineEvents &event,
                                         const std::string &localUserStd)
{
    using namespace mtx::events;
    if (!(std::holds_alternative<RoomEvent<voip::CallCandidates>>(event) ||
          std::holds_alternative<RoomEvent<voip::CallNegotiate>>(event) ||
          std::holds_alternative<RoomEvent<voip::CallInvite>>(event) ||
          std::holds_alternative<RoomEvent<voip::CallAnswer>>(event) ||
          std::holds_alternative<RoomEvent<voip::CallSelectAnswer>>(event) ||
          std::holds_alternative<RoomEvent<voip::CallReject>>(event) ||
          std::holds_alternative<RoomEvent<voip::CallHangUp>>(event)))
        return false;

    std::visit(
      [this, &localUserStd](auto &callEvent) {
          callEvent.room_id = room_id_.toStdString();
          if constexpr (
            std::is_same_v<std::decay_t<decltype(callEvent)>, RoomEvent<voip::CallAnswer>> ||
            std::is_same_v<std::decay_t<decltype(callEvent)>, RoomEvent<voip::CallInvite>> ||
            std::is_same_v<std::decay_t<decltype(callEvent)>, RoomEvent<voip::CallSelectAnswer>> ||
            std::is_same_v<std::decay_t<decltype(callEvent)>, RoomEvent<voip::CallReject>> ||
            std::is_same_v<std::decay_t<decltype(callEvent)>, RoomEvent<voip::CallHangUp>>)
              emit newCallEvent(callEvent);
          else if (callEvent.sender != localUserStd)
              emit newCallEvent(callEvent);
      },
      event);

    return true;
}

void
TimelineModel::processSpecialEffectEvent(const mtx::events::collections::TimelineEvents &event)
{
    using namespace mtx::events;
    if (auto text = std::get_if<RoomEvent<msg::Text>>(&event)) {
        if (const auto msg = QString::fromStdString(text->content.body);
            msg.contains("🎉") || msg.contains("🎊")) {
            needsSpecialEffects_ = true;
            specialEffects_.setFlag(Confetti);
        }
    } else if (auto unknown = std::get_if<RoomEvent<msg::Unknown>>(&event)) {
        if (const auto msg = QString::fromStdString(unknown->content.body);
            msg.contains("🎉") || msg.contains("🎊")) {
            needsSpecialEffects_ = true;
            specialEffects_.setFlag(Confetti);
        }
    } else if (auto effect = std::get_if<RoomEvent<msg::ElementEffect>>(&event)) {
        if (effect->content.msgtype == "nic.custom.confetti") {
            needsSpecialEffects_ = true;
            specialEffects_.setFlag(Confetti);
        } else if (effect->content.msgtype == "io.element.effect.rainfall") {
            needsSpecialEffects_ = true;
            specialEffects_.setFlag(Rainfall);
        }
    }
}

void
TimelineModel::addEvents(const mtx::responses::Timeline &timeline)
{
    if (timeline.limited)
        setPaginationInProgress(false);

    if (timeline.events.empty())
        return;

    events.handleSync(timeline);

    using namespace mtx::events;

    bool avatarChanged      = false;
    bool nameChanged        = false;
    bool memberCountChanged = false;
    const auto localUserStd = utils::localUser().toStdString();

    for (auto e : timeline.events) {
        if (auto encryptedEvent = std::get_if<EncryptedEvent<msg::Encrypted>>(&e)) {
            MegolmSessionIndex index(room_id_.toStdString(), encryptedEvent->content);

            auto result = olm::decryptEvent(index, *encryptedEvent);
            if (result.event)
                e = result.event.value();
        }

        if (dispatchCallEventIfNeeded(e, localUserStd))
            continue;

        if (applyStateEventSideEffects(e, avatarChanged, nameChanged, memberCountChanged))
            continue;

        processSpecialEffectEvent(e);
    }

    if (needsSpecialEffects_)
        triggerSpecialEffects();

    emitRoomMetadataChanges(avatarChanged, nameChanged, memberCountChanged);

    updateLastMessage();
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

QString
TimelineModel::formatJoinRuleEvent(
  const mtx::events::StateEvent<mtx::events::state::JoinRules> &event) const
{
    QString user = QString::fromStdString(event.sender);
    QString name = utils::replaceEmoji(displayName(user));

    switch (event.content.join_rule) {
    case mtx::events::state::JoinRule::Public:
        return tr("%1 opened the room to the public.").arg(name);
    case mtx::events::state::JoinRule::Invite:
        return tr("%1 made this room require an invitation to join.").arg(name);
    case mtx::events::state::JoinRule::Knock:
        return tr("%1 allowed to join this room by knocking.").arg(name);
    case mtx::events::state::JoinRule::Restricted: {
        QStringList rooms;
        for (const auto &r : event.content.allow) {
            if (r.type == mtx::events::state::JoinAllowanceType::RoomMembership)
                rooms.push_back(QString::fromStdString(r.room_id));
        }
        return tr("%1 allowed members of the following rooms to automatically join this "
                  "room: %2")
          .arg(name, rooms.join(QStringLiteral(", ")));
    }
    default:
        // Currently, knock and private are reserved keywords and not implemented in Matrix.
        return {};
    }
}

QString
TimelineModel::formatGuestAccessEvent(
  const mtx::events::StateEvent<mtx::events::state::GuestAccess> &event) const
{
    QString user = QString::fromStdString(event.sender);
    QString name = utils::replaceEmoji(displayName(user));

    switch (event.content.guest_access) {
    case mtx::events::state::AccessState::CanJoin:
        return tr("%1 made the room open to guests.").arg(name);
    case mtx::events::state::AccessState::Forbidden:
        return tr("%1 has closed the room to guest access.").arg(name);
    default:
        return {};
    }
}

QString
TimelineModel::formatHistoryVisibilityEvent(
  const mtx::events::StateEvent<mtx::events::state::HistoryVisibility> &event) const
{
    QString user = QString::fromStdString(event.sender);
    QString name = utils::replaceEmoji(displayName(user));

    switch (event.content.history_visibility) {
    case mtx::events::state::Visibility::WorldReadable:
        return tr("%1 made the room history world readable. Events may be now read by "
                  "non-joined people.")
          .arg(name);
    case mtx::events::state::Visibility::Shared:
        return tr("%1 set the room history visible to members from this point on.").arg(name);
    case mtx::events::state::Visibility::Invited:
        return tr("%1 set the room history visible to members since they were invited.").arg(name);
    case mtx::events::state::Visibility::Joined:
        return tr("%1 set the room history visible to members since they joined the room.")
          .arg(name);
    default:
        return {};
    }
}

QString
TimelineModel::formatPowerLevelEvent(
  const mtx::events::StateEvent<mtx::events::state::PowerLevels> &event) const
{
    return timeline::format::formatPowerLevelEvent(
      event, events, [this](const QString &userId) { return displayName(userId); });
}

QString
TimelineModel::formatImagePackEvent(
  const mtx::events::StateEvent<mtx::events::msc2545::ImagePack> &event) const
{
    return timeline::format::formatImagePackEvent(
      event,
      events,
      QFontMetrics(UserSettings::instance()->uiFontFamily()).ascent(),
      [this](const QString &userId) { return displayName(userId); });
}

QString
TimelineModel::formatPolicyRule(const QString &id) const
{
    return timeline::format::formatPolicyRule(
      id, events, [this](const QString &userId) { return displayName(userId); });
}

QVariantMap
TimelineModel::formatRedactedEvent(const QString &id)
{
    return timeline::format::formatRedactedEvent(
      id, events, [this](const QString &userId) { return displayName(userId); });
}

void
TimelineModel::acceptKnock(const QString &id)
{
    auto e = events.get(id.toStdString(), "");
    if (!e)
        return;

    auto event = std::get_if<mtx::events::StateEvent<mtx::events::state::Member>>(e);
    if (!event)
        return;

    if (!permissions_.canInvite())
        return;

    if (cache::isRoomMember(event->state_key, room_id_.toStdString()))
        return;

    using namespace mtx::events::state;
    if (event->content.membership != Membership::Knock)
        return;

    ChatPage::instance()->inviteUser(
      room_id_, QString::fromStdString(event->state_key), QLatin1String(""));
}

bool
TimelineModel::showAcceptKnockButton(const QString &id)
{
    auto e = events.get(id.toStdString(), "");
    if (!e)
        return false;

    auto event = std::get_if<mtx::events::StateEvent<mtx::events::state::Member>>(e);
    if (!event)
        return false;

    if (!permissions_.canInvite())
        return false;

    if (cache::isRoomMember(event->state_key, room_id_.toStdString()))
        return false;

    using namespace mtx::events::state;
    return event->content.membership == Membership::Knock;
}

void
TimelineModel::joinReplacementRoom(const QString &id)
{
    auto e = events.get(id.toStdString(), "");
    if (!e)
        return;

    auto event = std::get_if<mtx::events::StateEvent<mtx::events::state::Tombstone>>(e);
    if (!event)
        return;

    auto joined_rooms = cache::joinedRooms();
    for (const auto &roomid : joined_rooms) {
        if (roomid == event->content.replacement_room) {
            manager_->rooms()->setCurrentRoom(
              QString::fromStdString(event->content.replacement_room));
            return;
        }
    }

    ChatPage::instance()->joinRoomVia(
      event->content.replacement_room,
      {mtx::identifiers::parse<mtx::identifiers::User>(event->sender).hostname()},
      true);
}

QString
TimelineModel::formatMemberEvent(
  const mtx::events::StateEvent<mtx::events::state::Member> &event) const
{
    return timeline::format::formatMemberEvent(
      mtx::events::collections::TimelineEvents{event}, events, [this](const QString &userId) {
          return displayName(userId);
      });
}

void
TimelineModel::resetState()
{
    http::client()->get_state(
      room_id_.toStdString(),
      [this](const mtx::responses::StateEvents &events_, mtx::http::RequestErr e) {
          if (e) {
              nhlog::net()->error("Failed to retrieve current room state: {}", *e);
              return;
          }

          emit newState(events_);
      });
}

#include "moc_TimelineModel.cpp"

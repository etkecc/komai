// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include <type_traits>

#include "cache/Cache.h"
#include "encryption/Olm.h"
#include "logging/Logging.h"
#include "utils/Utils.h"

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
    } else if (std::holds_alternative<StateEvent<state::JoinRules>>(event)) {
        auto newPublic = std::get<StateEvent<state::JoinRules>>(event).content.join_rule ==
                         state::JoinRule::Public;
        if (this->isPublic_ != newPublic) {
            this->isPublic_ = newPublic;
            emit joinRuleChanged();
        }
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
    } else if (std::holds_alternative<StateEvent<state::JoinRules>>(event)) {
        auto newPublic = std::get<StateEvent<state::JoinRules>>(event).content.join_rule ==
                         state::JoinRule::Public;
        if (this->isPublic_ != newPublic) {
            this->isPublic_ = newPublic;
            emit joinRuleChanged();
        }
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

    if (memberCountChanged)
        emit roomMemberCountChanged();
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

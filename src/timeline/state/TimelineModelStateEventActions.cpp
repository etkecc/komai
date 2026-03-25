// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include <QFontMetrics>

#include "RoomlistModel.h"
#include "TimelineViewManager.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/format/TimelineImagePackFormatter.h"
#include "timeline/format/TimelineMemberEventFormatter.h"
#include "timeline/format/TimelinePolicyRuleFormatter.h"
#include "timeline/format/TimelinePowerLevelFormatter.h"
#include "timeline/format/TimelineRedactedEventFormatter.h"
#include "utils/Utils.h"

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
      event, permissions_.createEvent(), events, [this](const QString &userId) {
          return displayName(userId);
      });
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
    nhlog::ui()->warn("Skipping legacy room-state reset for room '{}' because the old Matrix "
                      "network path has been removed",
                      room_id_.toStdString());
}

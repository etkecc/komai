// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/format/TimelineMemberEventFormatter.h"

#include <QCoreApplication>

#include "EventStore.h"
#include "Utils.h"

namespace {
QString
tr(const char *source)
{
    return QCoreApplication::translate("TimelineModel", source);
}

QString
trWithComment(const char *source, const char *comment)
{
    return QCoreApplication::translate("TimelineModel", source, comment);
}
}

QString
timeline::format::formatMemberEvent(const mtx::events::collections::TimelineEvents &event,
                                    EventStore &eventStore,
                                    const DisplayNameForUserFn &displayNameForUser)
{
    auto memberEvent = std::get_if<mtx::events::StateEvent<mtx::events::state::Member>>(&event);
    if (!memberEvent)
        return {};

    mtx::events::StateEvent<mtx::events::state::Member> const *prevEvent = nullptr;
    if (!memberEvent->unsigned_data.replaces_state.empty()) {
        auto tempPrevEvent =
          eventStore.get(memberEvent->unsigned_data.replaces_state, memberEvent->event_id);
        if (tempPrevEvent) {
            prevEvent =
              std::get_if<mtx::events::StateEvent<mtx::events::state::Member>>(tempPrevEvent);
        }
    }

    auto renderName = [&displayNameForUser](const QString &userId) {
        return utils::replaceEmoji(displayNameForUser(userId));
    };

    QString user       = QString::fromStdString(memberEvent->state_key);
    QString name       = renderName(user);
    QString rendered;
    QString sender     = QString::fromStdString(memberEvent->sender);
    QString senderName = renderName(sender);

    // see table https://matrix.org/docs/spec/client_server/latest#m-room-member
    using namespace mtx::events::state;
    switch (memberEvent->content.membership) {
    case Membership::Invite:
        rendered = tr("%1 invited %2.").arg(senderName, name);
        break;
    case Membership::Join:
        if (prevEvent && prevEvent->content.membership == Membership::Join) {
            QString oldName = utils::replaceEmoji(
              QString::fromStdString(prevEvent->content.display_name).toHtmlEscaped());

            bool displayNameChanged =
              prevEvent->content.display_name != memberEvent->content.display_name;
            bool avatarChanged = prevEvent->content.avatar_url != memberEvent->content.avatar_url;

            if (displayNameChanged && avatarChanged)
                rendered =
                  tr("%1 has changed their avatar and changed their "
                     "display name to %2.")
                    .arg(oldName, name);
            else if (displayNameChanged)
                rendered = tr("%1 has changed their display name to %2.").arg(oldName, name);
            else if (avatarChanged)
                rendered = tr("%1 changed their avatar.").arg(name);
            else
                rendered = tr("%1 changed some profile info.").arg(name);
            // the case of nothing changed but join follows join shouldn't happen, so
            // just show it as join
        } else {
            if (memberEvent->content.join_authorised_via_users_server.empty())
                rendered = tr("%1 joined.").arg(name);
            else
                rendered = tr("%1 joined via authorisation from %2's server.")
                             .arg(name,
                                  QString::fromStdString(
                                    memberEvent->content.join_authorised_via_users_server));
        }
        break;
    case Membership::Leave:
        if (!prevEvent || prevEvent->content.membership == Membership::Join) {
            if (memberEvent->state_key == memberEvent->sender)
                rendered = tr("%1 left the room.").arg(name);
            else
                rendered = tr("%2 kicked %1.").arg(name, senderName);
        } else if (prevEvent->content.membership == Membership::Invite) {
            if (memberEvent->state_key == memberEvent->sender)
                rendered = tr("%1 rejected their invite.").arg(name);
            else
                rendered = tr("%2 revoked the invite to %1.").arg(name, senderName);
        } else if (prevEvent->content.membership == Membership::Ban) {
            rendered = tr("%2 unbanned %1.").arg(name, senderName);
        } else if (prevEvent->content.membership == Membership::Knock) {
            if (memberEvent->state_key == memberEvent->sender)
                rendered = tr("%1 redacted their knock.").arg(name);
            else
                rendered = tr("%2 rejected the knock from %1.").arg(name, senderName);
        } else
            return trWithComment("%1 left after having already left!",
                                 "This is a leave event after the user already left and shouldn't "
                                 "happen apart from state resets")
              .arg(name);
        break;

    case Membership::Ban:
        rendered = tr("%1 banned %2").arg(senderName, name);
        break;
    case Membership::Knock:
        rendered = tr("%1 knocked.").arg(name);
        break;
    }

    if (memberEvent->content.reason != "") {
        rendered += " " + tr("Reason: %1").arg(QString::fromStdString(memberEvent->content.reason));
    }

    return rendered;
}

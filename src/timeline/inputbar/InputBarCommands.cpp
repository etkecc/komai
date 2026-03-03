// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "InputBar.h"

#include <QRegularExpression>

#include <optional>
#include <string_view>

#include <nlohmann/json.hpp>

#include <mtx/responses/common.hpp>

#include "ChatPage.h"
#include "Logging.h"
#include "MatrixClient.h"
#include "TimelineModel.h"
#include "TimelineViewManager.h"
#include "Utils.h"
#include "cache/Cache.h"
#include "ui/MainWindow.h"
#include "ui/UserProfile.h"

namespace {
using InvitePermissionsContent = mtx::events::account_data::nheko_extensions::InvitePermissions;
constexpr std::string_view KOMAI_INVITE_PERMISSIONS_TYPE = "cc.etke.komai.invite_permissions";

std::optional<InvitePermissionsContent>
parseInvitePermissionsFromRawAccountData(const std::string &eventJson)
{
    try {
        const auto parsedEvent = nlohmann::json::parse(eventJson);
        if (!parsedEvent.is_object() || !parsedEvent.contains("content"))
            return std::nullopt;

        return parsedEvent.at("content").get<InvitePermissionsContent>();
    } catch (const std::exception &) {
        return std::nullopt;
    }
}
} // namespace

QPair<QString, QString>
InputBar::getCommandAndArgs(const QString &currentText) const
{
    if (!currentText.startsWith('/'))
        return {{}, currentText};

    static QRegularExpression spaceRegex(QStringLiteral("\\s"));

    int command_end = currentText.indexOf(spaceRegex);
    if (command_end == -1)
        command_end = currentText.size();
    auto name = currentText.mid(1, command_end - 1);
    auto args = currentText.mid(command_end + 1);
    if (name.isEmpty() || name == QLatin1String("/")) {
        return {{}, currentText};
    } else {
        return {name, args};
    }
}

bool
InputBar::command(const QString &command, QString args)
{
    if (command == QLatin1String("me")) {
        emote(args, false);
    } else if (command == QLatin1String("react")) {
        auto eventId = room->reply();
        if (!eventId.isEmpty())
            reaction(eventId, args.trimmed());
    } else if (command == QLatin1String("join")) {
        ChatPage::instance()->joinRoom(args.section(' ', 0, 0), args.section(' ', 1, -1));
    } else if (command == QLatin1String("knock")) {
        ChatPage::instance()->knockRoom(args.section(' ', 0, 0), args.section(' ', 1, -1));
    } else if (command == QLatin1String("part") || command == QLatin1String("leave")) {
        ChatPage::instance()->timelineManager()->openLeaveRoomDialog(room->roomId(), args);
    } else if (command == QLatin1String("invite")) {
        ChatPage::instance()->inviteUser(
          room->roomId(), args.section(' ', 0, 0), args.section(' ', 1, -1));
    } else if (command == QLatin1String("kick")) {
        if (args.startsWith('@')) {
            ChatPage::instance()->kickUser(
              room->roomId(), args.section(' ', 0, 0), args.section(' ', 1, -1));
        } else if (auto reply = room->reply(); !reply.isEmpty()) {
            auto replySender =
              room->dataById(room->reply(), TimelineModel::Roles::UserId, "").toString();
            if (!replySender.isEmpty()) {
                ChatPage::instance()->kickUser(room->roomId(), replySender, args);
            }
        }
    } else if (command == QLatin1String("ban")) {
        if (args.startsWith('@')) {
            ChatPage::instance()->banUser(
              room->roomId(), args.section(' ', 0, 0), args.section(' ', 1, -1));
        } else if (auto reply = room->reply(); !reply.isEmpty()) {
            auto replySender =
              room->dataById(room->reply(), TimelineModel::Roles::UserId, "").toString();
            if (!replySender.isEmpty()) {
                ChatPage::instance()->banUser(room->roomId(), replySender, args);
            }
        }
    } else if (command == QLatin1String("unban")) {
        if (args.startsWith('@')) {
            ChatPage::instance()->unbanUser(
              room->roomId(), args.section(' ', 0, 0), args.section(' ', 1, -1));
        } else if (auto reply = room->reply(); !reply.isEmpty()) {
            auto replySender =
              room->dataById(room->reply(), TimelineModel::Roles::UserId, "").toString();
            if (!replySender.isEmpty()) {
                ChatPage::instance()->unbanUser(room->roomId(), replySender, args);
            }
        }
    } else if (command == QLatin1String("redact")) {
        if (args.startsWith('@')) {
            room->redactAllFromUser(args.section(' ', 0, 0), args.section(' ', 1, -1));
        } else if (args.startsWith('$')) {
            room->redactEvent(args.section(' ', 0, 0), args.section(' ', 1, -1));
        } else if (auto reply = room->reply(); !reply.isEmpty()) {
            room->redactEvent(reply, args);
        }
    } else if (command == QLatin1String("roomnick")) {
        mtx::events::state::Member member;
        member.display_name = args.toStdString();
        member.avatar_url   = cache::avatarUrl(room->roomId(), utils::localUser()).toStdString();
        member.membership   = mtx::events::state::Membership::Join;

        http::client()->send_state_event(
          room->roomId().toStdString(),
          utils::localUser().toStdString(),
          member,
          [](const mtx::responses::EventId &, mtx::http::RequestErr err) {
              if (err)
                  nhlog::net()->error("Failed to set room displayname: {}",
                                      err->matrix_error.error);
          });
    } else if (command == QLatin1String("shrug")) {
        message("¯\\\\\\_(ツ)\\_/¯" + (args.isEmpty() ? QLatin1String("") : " " + args));
    } else if (command == QLatin1String("fliptable")) {
        message(QStringLiteral("(╯°□°)╯︵ ┻━┻"));
    } else if (command == QLatin1String("unfliptable")) {
        message(QStringLiteral(" ┯━┯╭( º _ º╭)"));
    } else if (command == QLatin1String("sovietflip")) {
        message(QStringLiteral("ノ┬─┬ノ ︵ ( \\o°o)\\"));
    } else if (command == QLatin1String("clear-timeline")) {
        room->clearTimeline();
    } else if (command == QLatin1String("reset-state")) {
        room->resetState();
    } else if (command == QLatin1String("rotate-megolm-session")) {
        cache::dropOutboundMegolmSession(room->roomId().toStdString());
    } else if (command == QLatin1String("md")) {
        message(args, MarkdownOverride::ON);
    } else if (command == QLatin1String("cmark")) {
        message(args, MarkdownOverride::CMARK);
    } else if (command == QLatin1String("plain")) {
        message(args, MarkdownOverride::OFF);
    } else if (command == QLatin1String("rainbow")) {
        message(args, MarkdownOverride::ON, true);
    } else if (command == QLatin1String("rainbowme")) {
        emote(args, true);
    } else if (command == QLatin1String("notice")) {
        notice(args, false);
    } else if (command == QLatin1String("rainbownotice")) {
        notice(args, true);
    } else if (command == QLatin1String("confetti")) {
        confetti(args, false);
    } else if (command == QLatin1String("rainbowconfetti")) {
        confetti(args, true);
    } else if (command == QLatin1String("rainfall")) {
        rainfall(args);
    } else if (command == QLatin1String("msgtype")) {
        customMsgtype(args.section(' ', 0, 0), args.section(' ', 1, -1));
    } else if (command == QLatin1String("glitch")) {
        message(utils::glitchText(args));
    } else if (command == QLatin1String("gradualglitch")) {
        message(utils::graduallyGlitchText(args));
    } else if (command == QLatin1String("goto")) {
        // Goto has three different modes:
        // 1 - Going directly to a given event ID
        if (args[0] == '$') {
            room->showEvent(args);
            return true;
        }
        // 2 - Going directly to a given message index
        if (args[0] >= '0' && args[0] <= '9') {
            room->showEvent(args);
            return true;
        }
        // 3 - Matrix URI handler, as if you clicked the URI
        if (ChatPage::instance()->tryHandleMatrixUri(args)) {
            return true;
        }
        nhlog::net()->error("Could not resolve goto: {}", args.toStdString());
    } else if (command == QLatin1String("converttodm")) {
        utils::markRoomAsDirect(this->room->roomId(),
                                cache::getMembers(this->room->roomId().toStdString(), 0, -1));
    } else if (command == QLatin1String("converttoroom")) {
        utils::removeDirectFromRoom(this->room->roomId());
    } else if (command == QLatin1String("ignore")) {
        this->toggleIgnore(args.trimmed(), true);
    } else if (command == QLatin1String("unignore")) {
        this->toggleIgnore(args.trimmed(), false);
    } else if (command == QLatin1String("blockinvites")) {
        this->toggleInvitePermission(args.trimmed(), true);
    } else if (command == QLatin1String("allowinvites")) {
        this->toggleInvitePermission(args.trimmed(), false);
    } else {
        return false;
    }

    return true;
}

void
InputBar::toggleIgnore(const QString &user, const bool ignored)
{
    if (!user.startsWith(u"@")) {
        MainWindow::instance()->showNotification(
          tr("You need to pass a valid mxid when ignoring a user. '%1' is not a valid userid.")
            .arg(user));
        return;
    }

    UserProfile *profile = new UserProfile(QString(), user, TimelineViewManager::instance());
    connect(profile, &UserProfile::failedToFetchProfile, [user, profile] {
        MainWindow::instance()->showNotification(tr("Failed to fetch user %1").arg(user));
        profile->deleteLater();
    });

    connect(
      profile, &UserProfile::globalUsernameRetrieved, [profile, ignored](const QString &user_id) {
          Q_UNUSED(user_id)
          profile->setIgnored(ignored);
          profile->deleteLater();
      });
}

void
InputBar::toggleInvitePermission(const QString &id, bool block)
{
    InvitePermissionsContent permissions;
    if (auto raw = cache::getAccountDataByType(std::string(KOMAI_INVITE_PERMISSIONS_TYPE))) {
        if (auto content = parseInvitePermissionsFromRawAccountData(*raw))
            permissions = std::move(*content);
    }

    auto idstr = id.toStdString();

    if (id.startsWith("matrix:") || id.startsWith("https://matrix.to")) {
        auto m = utils::parseMatrixUri(id);
        if (m) {
            idstr = m->mxid1.toStdString();
        } else {
            return;
        }
    }

    if (idstr.starts_with("@")) {
        if (block) {
            permissions.user_allow.erase(idstr);
            permissions.user_deny.emplace(idstr, "{}");
        } else {
            permissions.user_deny.erase(idstr);
            permissions.user_allow.emplace(idstr, "{}");
        }
    } else if (idstr.starts_with("!")) {
        if (block) {
            permissions.room_allow.erase(idstr);
            permissions.room_deny.emplace(idstr, "{}");
        } else {
            permissions.room_deny.erase(idstr);
            permissions.room_allow.emplace(idstr, "{}");
        }
    } else if (idstr == "all" || idstr == "default") {
        if (block)
            permissions.default_ = "deny";
        else
            permissions.default_ = "allow";
    } else if (!idstr.starts_with("#")) {
        if (block) {
            permissions.server_allow.erase(idstr);
            permissions.server_deny.emplace(idstr, "{}");
        } else {
            permissions.server_deny.erase(idstr);
            permissions.server_allow.emplace(idstr, "{}");
        }
    }

    http::client()->put_account_data(
      std::string(KOMAI_INVITE_PERMISSIONS_TYPE), permissions, [](mtx::http::RequestErr err) {
          if (err) {
              nhlog::ui()->error("Failed to update invite permissions: {}", *err);
          }
      });

    auto invites = cache::invites();

    for (const auto &[roomid, info] : invites.asKeyValueRange()) {
        auto roomid_ = roomid.toStdString();
        auto self    = cache::getInviteMember(roomid_, utils::localUser().toStdString());
        if (!self->inviter.empty()) {
            if (!permissions.invite_allowed(roomid_, self->inviter)) {
                ChatPage::instance()->leaveRoom(roomid, "");
            }
        }
    }
}

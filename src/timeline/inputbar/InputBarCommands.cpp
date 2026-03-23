// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "InputBar.h"

#include <QCoreApplication>

#include <optional>
#include <string_view>

#include <nlohmann/json.hpp>

#include <mtx/responses/common.hpp>

#include "TimelineModel.h"
#include "TimelineViewManager.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "ui/MainWindow.h"
#include "ui/UserProfile.h"
#include "utils/Utils.h"

namespace {
using InvitePermissionsContent = mtx::events::account_data::nheko_extensions::InvitePermissions;
constexpr std::string_view KOMAI_INVITE_PERMISSIONS_TYPE = "cc.etke.komai.invite_permissions";
constexpr auto kInputBarTranslationContext               = "InputBar";

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

QString
firstToken(const QString &arguments)
{
    const auto trimmed = arguments.trimmed();
    if (trimmed.isEmpty())
        return {};

    int end = 0;
    while (end < trimmed.size() && !trimmed.at(end).isSpace())
        ++end;

    return trimmed.left(end);
}

QString
remainingAfterFirstToken(const QString &arguments)
{
    const auto trimmed = arguments.trimmed();
    if (trimmed.isEmpty())
        return {};

    int split = 0;
    while (split < trimmed.size() && !trimmed.at(split).isSpace())
        ++split;
    while (split < trimmed.size() && trimmed.at(split).isSpace())
        ++split;

    return trimmed.mid(split);
}
} // namespace

timeline::slash_commands::CommandResult
timeline::slash_commands::execute(InputBar &inputBar, const ParsedCommand &parsed)
{
    if (!parsed.definition)
        return CommandResult::rejected();

    const auto command = parsed.definition->id;
    const auto args    = parsed.arguments;

    switch (command) {
    case CommandId::Me:
        inputBar.emote(args, false);
        return CommandResult::dispatched();
    case CommandId::React: {
        auto eventId = inputBar.room->reply();
        if (!eventId.isEmpty())
            inputBar.reaction(eventId, args.trimmed());
        return CommandResult::dispatched();
    }
    case CommandId::Join: {
        const auto target = firstToken(args);
        const auto reason = remainingAfterFirstToken(args);
        ChatPage::instance()->joinRoom(target, reason);
        return CommandResult::dispatched();
    }
    case CommandId::Knock: {
        const auto target = firstToken(args);
        const auto reason = remainingAfterFirstToken(args);
        ChatPage::instance()->knockRoom(target, reason);
        return CommandResult::dispatched();
    }
    case CommandId::Leave:
        ChatPage::instance()->timelineManager()->openLeaveRoomDialog(inputBar.room->roomId(), args);
        return CommandResult::dispatched();
    case CommandId::Invite: {
        const auto target = firstToken(args);
        const auto reason = remainingAfterFirstToken(args);
        ChatPage::instance()->inviteUser(inputBar.room->roomId(), target, reason);
        return CommandResult::dispatched();
    }
    case CommandId::Kick: {
        if (args.trimmed().startsWith(u'@')) {
            const auto target = firstToken(args);
            const auto reason = remainingAfterFirstToken(args);
            ChatPage::instance()->kickUser(inputBar.room->roomId(), target, reason);
        } else if (auto reply = inputBar.room->reply(); !reply.isEmpty()) {
            auto replySender =
              inputBar.room->dataById(inputBar.room->reply(), TimelineModel::Roles::UserId, "")
                .toString();
            if (!replySender.isEmpty()) {
                ChatPage::instance()->kickUser(inputBar.room->roomId(), replySender, args);
            }
        }
        return CommandResult::dispatched();
    }
    case CommandId::Ban: {
        if (args.trimmed().startsWith(u'@')) {
            const auto target = firstToken(args);
            const auto reason = remainingAfterFirstToken(args);
            ChatPage::instance()->banUser(inputBar.room->roomId(), target, reason);
        } else if (auto reply = inputBar.room->reply(); !reply.isEmpty()) {
            auto replySender =
              inputBar.room->dataById(inputBar.room->reply(), TimelineModel::Roles::UserId, "")
                .toString();
            if (!replySender.isEmpty()) {
                ChatPage::instance()->banUser(inputBar.room->roomId(), replySender, args);
            }
        }
        return CommandResult::dispatched();
    }
    case CommandId::Unban: {
        if (args.trimmed().startsWith(u'@')) {
            const auto target = firstToken(args);
            const auto reason = remainingAfterFirstToken(args);
            ChatPage::instance()->unbanUser(inputBar.room->roomId(), target, reason);
        } else if (auto reply = inputBar.room->reply(); !reply.isEmpty()) {
            auto replySender =
              inputBar.room->dataById(inputBar.room->reply(), TimelineModel::Roles::UserId, "")
                .toString();
            if (!replySender.isEmpty()) {
                ChatPage::instance()->unbanUser(inputBar.room->roomId(), replySender, args);
            }
        }
        return CommandResult::dispatched();
    }
    case CommandId::Redact: {
        const auto trimmedArgs = args.trimmed();
        if (trimmedArgs.startsWith(u'@')) {
            inputBar.room->redactAllFromUser(firstToken(args), remainingAfterFirstToken(args));
        } else if (trimmedArgs.startsWith(u'$')) {
            inputBar.room->redactEvent(firstToken(args), remainingAfterFirstToken(args));
        } else if (auto reply = inputBar.room->reply(); !reply.isEmpty()) {
            inputBar.room->redactEvent(reply, trimmedArgs);
        }
        return CommandResult::dispatched();
    }
    case CommandId::Roomnick: {
        mtx::events::state::Member member;
        member.display_name = args.toStdString();
        member.avatar_url =
          cache::avatarUrl(inputBar.room->roomId(), utils::localUser()).toStdString();
        member.membership = mtx::events::state::Membership::Join;

        http::client()->send_state_event(
          inputBar.room->roomId().toStdString(),
          utils::localUser().toStdString(),
          member,
          [](const mtx::responses::EventId &, mtx::http::RequestErr err) {
              if (err)
                  nhlog::net()->error("Failed to set room displayname: {}",
                                      err->matrix_error.error);
          });
        return CommandResult::dispatched();
    }
    case CommandId::Shrug:
        inputBar.message(QStringLiteral("¯\\\\\\_(ツ)\\_/¯") +
                         (args.isEmpty() ? QLatin1String("") : QLatin1String(" ") + args));
        return CommandResult::dispatched();
    case CommandId::ClearTimeline:
        inputBar.room->clearTimeline();
        return CommandResult::dispatched();
    case CommandId::ResetState:
        inputBar.room->resetState();
        return CommandResult::dispatched();
    case CommandId::RotateMegolmSession:
        cache::dropOutboundMegolmSession(inputBar.room->roomId().toStdString());
        return CommandResult::dispatched();
    case CommandId::Md:
        inputBar.message(args, MarkdownOverride::ON);
        return CommandResult::dispatched();
    case CommandId::Cmark:
        inputBar.message(args, MarkdownOverride::CMARK);
        return CommandResult::dispatched();
    case CommandId::Plain:
        inputBar.message(args, MarkdownOverride::OFF);
        return CommandResult::dispatched();
    case CommandId::Notice:
        inputBar.notice(args, false);
        return CommandResult::dispatched();
    case CommandId::Msgtype:
        inputBar.customMsgtype(firstToken(args), remainingAfterFirstToken(args));
        return CommandResult::dispatched();
    case CommandId::Goto: {
        const auto trimmedArgs = args.trimmed();

        if (trimmedArgs.startsWith(u'$')) {
            if (inputBar.room->showEvent(trimmedArgs))
                return CommandResult::dispatched();

            return CommandResult::rejected(QCoreApplication::translate(
              kInputBarTranslationContext, "That event ID could not be resolved in this room."));
        }

        bool allDigits = !trimmedArgs.isEmpty();
        for (const auto ch : trimmedArgs) {
            if (!ch.isDigit()) {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            if (inputBar.room->showEvent(trimmedArgs))
                return CommandResult::dispatched();

            return CommandResult::rejected(
              QCoreApplication::translate(kInputBarTranslationContext,
                                          "That message index could not be resolved in this "
                                          "room."));
        }

        if (ChatPage::instance()->tryHandleMatrixUri(trimmedArgs))
            return CommandResult::dispatched();

        return CommandResult::rejected(QCoreApplication::translate(
          kInputBarTranslationContext,
          "Could not resolve that /goto target. Use an event ID, numeric message index, or "
          "Matrix link."));
    }
    case CommandId::ConvertToDm:
        utils::markRoomAsDirect(inputBar.room->roomId(),
                                cache::getMembers(inputBar.room->roomId().toStdString(), 0, -1));
        return CommandResult::dispatched();
    case CommandId::ConvertToRoom:
        utils::removeDirectFromRoom(inputBar.room->roomId());
        return CommandResult::dispatched();
    case CommandId::Ignore:
        inputBar.toggleIgnore(args.trimmed(), true);
        return CommandResult::dispatched();
    case CommandId::Unignore:
        inputBar.toggleIgnore(args.trimmed(), false);
        return CommandResult::dispatched();
    case CommandId::BlockInvites:
        inputBar.toggleInvitePermission(args.trimmed(), true);
        return CommandResult::dispatched();
    case CommandId::AllowInvites:
        inputBar.toggleInvitePermission(args.trimmed(), false);
        return CommandResult::dispatched();
    }

    return CommandResult::rejected();
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

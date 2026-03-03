// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/format/TimelinePolicyRuleFormatter.h"

#include <QCoreApplication>

#include <string_view>

#include "EventStore.h"
#include "utils/Utils.h"

namespace {
QString
tr(const char *source)
{
    return QCoreApplication::translate("TimelineModel", source);
}
}

QString
timeline::format::formatPolicyRule(const QString &id,
                                   EventStore &eventStore,
                                   const DisplayNameForUserFn &displayNameForUser)
{
    auto idStr = id.toStdString();
    auto e     = eventStore.get(idStr, "");
    if (!e)
        return {};

    auto qsHtml = [](const std::string &s) { return QString::fromStdString(s).toHtmlEscaped(); };
    auto senderName = [&displayNameForUser](const std::string &sender) {
        return utils::replaceEmoji(displayNameForUser(QString::fromStdString(sender)));
    };
    constexpr std::string_view unstable_ban = "org.matrix.mjolnir.ban";

    if (auto userRule =
          std::get_if<mtx::events::StateEvent<mtx::events::state::policy_rule::UserRule>>(e)) {
        auto sender = senderName(userRule->sender);
        if (userRule->content.entity.empty() ||
            (userRule->content.recommendation !=
               mtx::events::state::policy_rule::recommendation::ban &&
             userRule->content.recommendation != unstable_ban)) {
            while (userRule->content.entity.empty() &&
                   !userRule->unsigned_data.replaces_state.empty()) {
                auto temp = eventStore.get(userRule->unsigned_data.replaces_state, idStr);
                if (!temp)
                    break;
                if (auto tempRule = std::get_if<
                      mtx::events::StateEvent<mtx::events::state::policy_rule::UserRule>>(temp))
                    userRule = tempRule;
                else
                    break;
            }

            return tr("%1 disabled the rule to ban users matching %2.")
              .arg(sender, qsHtml(userRule->content.entity));
        } else {
            return tr("%1 added a rule to ban users matching %2 for '%3'.")
              .arg(sender, qsHtml(userRule->content.entity), qsHtml(userRule->content.reason));
        }
    } else if (auto roomRule =
                 std::get_if<mtx::events::StateEvent<mtx::events::state::policy_rule::RoomRule>>(
                   e)) {
        auto sender = senderName(roomRule->sender);
        if (roomRule->content.entity.empty() ||
            (roomRule->content.recommendation !=
               mtx::events::state::policy_rule::recommendation::ban &&
             roomRule->content.recommendation != unstable_ban)) {
            while (roomRule->content.entity.empty() &&
                   !roomRule->unsigned_data.replaces_state.empty()) {
                auto temp = eventStore.get(roomRule->unsigned_data.replaces_state, idStr);
                if (!temp)
                    break;
                if (auto tempRule = std::get_if<
                      mtx::events::StateEvent<mtx::events::state::policy_rule::RoomRule>>(temp))
                    roomRule = tempRule;
                else
                    break;
            }
            return tr("%1 disabled the rule to ban rooms matching %2.")
              .arg(sender, qsHtml(roomRule->content.entity));
        } else {
            return tr("%1 added a rule to ban rooms matching %2 for '%3'.")
              .arg(sender, qsHtml(roomRule->content.entity), qsHtml(roomRule->content.reason));
        }
    } else if (auto serverRule =
                 std::get_if<mtx::events::StateEvent<mtx::events::state::policy_rule::ServerRule>>(
                   e)) {
        auto sender = senderName(serverRule->sender);
        if (serverRule->content.entity.empty() ||
            (serverRule->content.recommendation !=
               mtx::events::state::policy_rule::recommendation::ban &&
             serverRule->content.recommendation != unstable_ban)) {
            while (serverRule->content.entity.empty() &&
                   !serverRule->unsigned_data.replaces_state.empty()) {
                auto temp = eventStore.get(serverRule->unsigned_data.replaces_state, idStr);
                if (!temp)
                    break;
                if (auto tempRule = std::get_if<
                      mtx::events::StateEvent<mtx::events::state::policy_rule::ServerRule>>(temp))
                    serverRule = tempRule;
                else
                    break;
            }
            return tr("%1 disabled the rule to ban servers matching %2.")
              .arg(sender, qsHtml(serverRule->content.entity));
        } else {
            return tr("%1 added a rule to ban servers matching %2 for '%3'.")
              .arg(sender, qsHtml(serverRule->content.entity), qsHtml(serverRule->content.reason));
        }
    }

    return {};
}

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/format/TimelinePowerLevelFormatter.h"

#include <QCoreApplication>
#include <QStringList>

#include "EventStore.h"
#include "utils/Utils.h"

namespace {
QString
tr(const char *source)
{
    return QCoreApplication::translate("TimelineModel", source);
}

QString
trn(const char *source, int count)
{
    return QCoreApplication::translate("TimelineModel", source, nullptr, count);
}
}

QString
timeline::format::formatPowerLevelEvent(
  const mtx::events::StateEvent<mtx::events::state::PowerLevels> &event,
  const mtx::events::StateEvent<mtx::events::state::Create> &create,
  EventStore &eventStore,
  const DisplayNameForUserFn &displayNameForUser)
{
    mtx::events::StateEvent<mtx::events::state::PowerLevels> const *prevEvent = nullptr;
    if (!event.unsigned_data.replaces_state.empty()) {
        auto tempPrevEvent = eventStore.get(event.unsigned_data.replaces_state, event.event_id);
        if (tempPrevEvent) {
            prevEvent =
              std::get_if<mtx::events::StateEvent<mtx::events::state::PowerLevels>>(tempPrevEvent);
        }
    }

    auto renderName = [&displayNameForUser](const QString &userId) {
        return utils::replaceEmoji(displayNameForUser(userId));
    };

    QString sender_name = renderName(QString::fromStdString(event.sender));
    // Get the rooms levels for redactions and powerlevel changes to determine "Administrator" and
    // "Moderator" powerlevels.
    auto administrator_power_level = event.content.state_level("m.room.power_levels");
    auto moderator_power_level     = event.content.redact;
    auto default_powerlevel        = event.content.users_default;
    if (!prevEvent)
        return tr("%1 has changed the room's permissions.").arg(sender_name);

    auto calc_affected =
      [&event, &prevEvent, &create](int64_t newPowerlevelSetting) -> std::pair<QStringList, int> {
        QStringList affected{};
        auto numberOfAffected = 0;
        // We do only compare to people with explicit PL. Usually others are not going to be
        // affected either way and this is cheaper to iterate over.
        for (auto const &[mxid, currentPowerlevel] : event.content.users) {
            if (currentPowerlevel == newPowerlevelSetting &&
                prevEvent->content.user_level(mxid, create) < newPowerlevelSetting) {
                numberOfAffected++;
                if (numberOfAffected <= 2) {
                    affected.push_back(QString::fromStdString(mxid));
                }
            }
        }
        return {affected, numberOfAffected};
    };

    QStringList resultingMessage{};
    // These affect only a few people. Therefor we can print who is affected.
    if (event.content.kick != prevEvent->content.kick) {
        auto default_message = tr("%1 has changed the room's kick powerlevel from %2 to %3.")
                                 .arg(sender_name)
                                 .arg(prevEvent->content.kick)
                                 .arg(event.content.kick);

        // We only calculate affected users if we change to a level above the default users PL
        // to not accidentally have a DoS vector
        if (event.content.kick > default_powerlevel) {
            auto [affected, number_of_affected] = calc_affected(event.content.kick);

            if (number_of_affected != 0) {
                auto true_affected_rest = number_of_affected - affected.size();
                if (number_of_affected > 1) {
                    resultingMessage.append(
                      default_message + QStringLiteral(" ") +
                      trn("%n member(s) can now kick room members.", true_affected_rest));
                } else if (number_of_affected == 1) {
                    resultingMessage.append(
                      default_message + QStringLiteral(" ") +
                      tr("%1 can now kick room members.").arg(renderName(affected.at(0))));
                }
            } else {
                resultingMessage.append(default_message);
            }
        } else {
            resultingMessage.append(default_message);
        }
    }

    if (event.content.redact != prevEvent->content.redact) {
        auto default_message = tr("%1 has changed the room's redact powerlevel from %2 to %3.")
                                 .arg(sender_name)
                                 .arg(prevEvent->content.redact)
                                 .arg(event.content.redact);

        // We only calculate affected users if we change to a level above the default users PL
        // to not accidentally have a DoS vector
        if (event.content.redact > default_powerlevel) {
            auto [affected, number_of_affected] = calc_affected(event.content.redact);

            if (number_of_affected != 0) {
                auto true_affected_rest = number_of_affected - affected.size();
                if (number_of_affected > 1) {
                    resultingMessage.append(
                      default_message + QStringLiteral(" ") +
                      trn("%n member(s) can now redact room messages.", true_affected_rest));
                } else if (number_of_affected == 1) {
                    resultingMessage.append(
                      default_message + QStringLiteral(" ") +
                      tr("%1 can now redact room messages.").arg(renderName(affected.at(0))));
                }
            } else {
                resultingMessage.append(default_message);
            }
        } else {
            resultingMessage.append(default_message);
        }
    }

    if (event.content.ban != prevEvent->content.ban) {
        auto default_message = tr("%1 has changed the room's ban powerlevel from %2 to %3.")
                                 .arg(sender_name)
                                 .arg(prevEvent->content.ban)
                                 .arg(event.content.ban);

        // We only calculate affected users if we change to a level above the default users PL
        // to not accidentally have a DoS vector
        if (event.content.ban > default_powerlevel) {
            auto [affected, number_of_affected] = calc_affected(event.content.ban);

            if (number_of_affected != 0) {
                auto true_affected_rest = number_of_affected - affected.size();
                if (number_of_affected > 1) {
                    resultingMessage.append(
                      default_message + QStringLiteral(" ") +
                      trn("%n member(s) can now ban room members.", true_affected_rest));
                } else if (number_of_affected == 1) {
                    resultingMessage.append(
                      default_message + QStringLiteral(" ") +
                      tr("%1 can now ban room members.").arg(renderName(affected.at(0))));
                }
            } else {
                resultingMessage.append(default_message);
            }
        } else {
            resultingMessage.append(default_message);
        }
    }

    if (event.content.state_default != prevEvent->content.state_default) {
        auto default_message =
          tr("%1 has changed the room's state_default powerlevel from %2 to %3.")
            .arg(sender_name)
            .arg(prevEvent->content.state_default)
            .arg(event.content.state_default);

        // We only calculate affected users if we change to a level above the default users PL
        // to not accidentally have a DoS vector
        if (event.content.state_default > default_powerlevel) {
            auto [affected, number_of_affected] = calc_affected(event.content.kick);

            if (number_of_affected != 0) {
                auto true_affected_rest = number_of_affected - affected.size();
                if (number_of_affected > 1) {
                    resultingMessage.append(
                      default_message + QStringLiteral(" ") +
                      trn("%n member(s) can now send state events.", true_affected_rest));
                } else if (number_of_affected == 1) {
                    resultingMessage.append(
                      default_message + QStringLiteral(" ") +
                      tr("%1 can now send state events.").arg(renderName(affected.at(0))));
                }
            } else {
                resultingMessage.append(default_message);
            }
        } else {
            resultingMessage.append(default_message);
        }
    }

    // These affect potentially the whole room. We there for do not calculate who gets affected
    // by this to prevent huge lists of people.
    if (event.content.invite != prevEvent->content.invite) {
        resultingMessage.append(tr("%1 has changed the room's invite powerlevel from %2 to %3.")
                                  .arg(sender_name,
                                       QString::number(prevEvent->content.invite),
                                       QString::number(event.content.invite)));
    }

    if (event.content.events_default != prevEvent->content.events_default) {
        if ((event.content.events_default > default_powerlevel) &&
            prevEvent->content.events_default <= default_powerlevel) {
            resultingMessage.append(
              tr("%1 has changed the room's events_default powerlevel from %2 to %3. New "
                 "users can now not send any events.")
                .arg(sender_name,
                     QString::number(prevEvent->content.events_default),
                     QString::number(event.content.events_default)));
        } else if ((event.content.events_default < prevEvent->content.events_default) &&
                   (event.content.events_default < default_powerlevel) &&
                   (prevEvent->content.events_default > default_powerlevel)) {
            resultingMessage.append(
              tr("%1 has changed the room's events_default powerlevel from %2 to %3. New "
                 "users can now send events that are not otherwise restricted.")
                .arg(sender_name,
                     QString::number(prevEvent->content.events_default),
                     QString::number(event.content.events_default)));
        } else {
            resultingMessage.append(
              tr("%1 has changed the room's events_default powerlevel from %2 to %3.")
                .arg(sender_name,
                     QString::number(prevEvent->content.events_default),
                     QString::number(event.content.events_default)));
        }
    }

    // Compare if a Powerlevel of a user changed
    for (auto const &[mxid, powerlevel] : event.content.users) {
        auto nameOfChangedUser = renderName(QString::fromStdString(mxid));
        if (prevEvent->content.user_level(mxid, create) != powerlevel) {
            if (powerlevel >= administrator_power_level) {
                resultingMessage.append(tr("%1 has made %2 an administrator of this room.")
                                          .arg(sender_name, nameOfChangedUser));
            } else if (powerlevel >= moderator_power_level &&
                       powerlevel > prevEvent->content.user_level(mxid, create)) {
                resultingMessage.append(tr("%1 has made %2 a moderator of this room.")
                                          .arg(sender_name, nameOfChangedUser));
            } else if (powerlevel >= moderator_power_level &&
                       powerlevel < prevEvent->content.user_level(mxid, create)) {
                resultingMessage.append(tr("%1 has downgraded %2 to moderator of this room.")
                                          .arg(sender_name, nameOfChangedUser));
            } else {
                resultingMessage.append(
                  tr("%1 has changed the powerlevel of %2 from %3 to %4.")
                    .arg(sender_name,
                         nameOfChangedUser,
                         QString::number(prevEvent->content.user_level(mxid, create)),
                         QString::number(powerlevel)));
            }
        }
    }

    // Handle added/removed/changed event type
    for (auto const &[event_type, powerlevel] : event.content.events) {
        auto prev_not_present =
          prevEvent->content.events.find(event_type) == prevEvent->content.events.end();

        if (prev_not_present || prevEvent->content.events.at(event_type) != powerlevel) {
            if (powerlevel >= administrator_power_level) {
                resultingMessage.append(tr("%1 allowed only administrators to send \"%2\".")
                                          .arg(sender_name, QString::fromStdString(event_type)));
            } else if (powerlevel >= moderator_power_level) {
                resultingMessage.append(tr("%1 allowed only moderators to send \"%2\".")
                                          .arg(sender_name, QString::fromStdString(event_type)));
            } else if (powerlevel == default_powerlevel) {
                resultingMessage.append(tr("%1 allowed everyone to send \"%2\".")
                                          .arg(sender_name, QString::fromStdString(event_type)));
            } else if (prev_not_present) {
                resultingMessage.append(
                  tr("%1 has changed the powerlevel of event type \"%2\" from the default to %3.")
                    .arg(sender_name,
                         QString::fromStdString(event_type),
                         QString::number(powerlevel)));
            } else {
                resultingMessage.append(
                  tr("%1 has changed the powerlevel of event type \"%2\" from %3 to %4.")
                    .arg(sender_name,
                         QString::fromStdString(event_type),
                         QString::number(prevEvent->content.events.at(event_type)),
                         QString::number(powerlevel)));
            }
        }
    }

    if (!resultingMessage.isEmpty()) {
        return resultingMessage.join("<br/>");
    } else {
        return tr("%1 has changed the room's permissions.").arg(sender_name);
    }
}

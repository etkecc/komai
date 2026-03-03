// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ChatPage.h"

#include <QImage>
#include <QPixmap>

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include <mtx/responses.hpp>

#include "AvatarProvider.h"
#include "EventAccessors.h"
#include "MainWindow.h"
#include "Utils.h"
#include "cache/Cache.h"
#include "encryption/Olm.h"
#include "notifications/Manager.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/Permissions.h"
#include "timeline/TimelineViewManager.h"

void
ChatPage::processSyncUi(const mtx::responses::Sync &sync)
{
    view_manager_->sync(sync);

    static unsigned int prevNotificationCount = 0;
    unsigned int notificationCount            = 0;
    for (const auto &room : sync.rooms.join) {
        notificationCount +=
          static_cast<unsigned int>(room.second.unread_notifications.notification_count);
    }

    // HACK: If we had less notifications last time we checked, send an alert if the
    // user wanted one. Technically, this may cause an alert to be missed if new ones
    // come in while you are reading old ones. Since the window is almost certainly open
    // in this edge case, that's probably a non-issue.
    // TODO: Replace this once we have proper pushrules support. This is a horrible hack
    if (prevNotificationCount < notificationCount) {
        if (userSettings_->notificationsAttentionOnIncoming())
            MainWindow::instance()->alert(0);
    }
    prevNotificationCount = notificationCount;

    // No need to check amounts for this section, as this function internally checks for
    // duplicates.
    if (notificationCount && userSettings_->hasNotifications())
        for (const auto &e : sync.account_data.events) {
            if (auto newRules =
                  std::get_if<mtx::events::AccountDataEvent<mtx::pushrules::GlobalRuleset>>(&e))
                pushrules =
                  std::make_unique<mtx::pushrules::PushRuleEvaluator>(newRules->content.global);
        }
    if (!pushrules) {
        auto eventInDb = cache::getAccountData(mtx::events::EventType::PushRules);
        if (eventInDb) {
            if (auto newRules =
                  std::get_if<mtx::events::AccountDataEvent<mtx::pushrules::GlobalRuleset>>(
                    &*eventInDb)) {
                pushrules =
                  std::make_unique<mtx::pushrules::PushRuleEvaluator>(newRules->content.global);
            }
        }
    }
    if (pushrules) {
        const auto local_user = utils::localUser().toStdString();

        // Desktop notifications to be sent
        struct PendingNotification
        {
            QString roomId;
            QString roomName;
            QString roomAvatarUrl;
            mtx::events::collections::TimelineEvents event;
            std::vector<mtx::pushrules::actions::Action> actions;
        };

        std::vector<PendingNotification> notifications;
        for (const auto &[room_id, room] : sync.rooms.join) {
            // clear old notifications
            for (const auto &e : room.ephemeral.events) {
                if (auto receiptsEv =
                      std::get_if<mtx::events::EphemeralEvent<mtx::events::ephemeral::Receipt>>(
                        &e)) {
                    std::vector<QString> receipts;

                    for (const auto &[event_id, userReceipts] : receiptsEv->content.receipts) {
                        if (auto r = userReceipts.find(mtx::events::ephemeral::Receipt::Read);
                            r != userReceipts.end()) {
                            for (const auto &[user_id, receipt] : r->second.users) {
                                (void)receipt;

                                if (user_id == local_user) {
                                    receipts.push_back(QString::fromStdString(event_id));
                                    break;
                                }
                            }
                        }
                        if (auto r =
                              userReceipts.find(mtx::events::ephemeral::Receipt::ReadPrivate);
                            r != userReceipts.end()) {
                            for (const auto &[user_id, receipt] : r->second.users) {
                                (void)receipt;

                                if (user_id == local_user) {
                                    receipts.push_back(QString::fromStdString(event_id));
                                    break;
                                }
                            }
                        }
                    }
                    if (!receipts.empty())
                        notificationsManager->removeNotifications(QString::fromStdString(room_id),
                                                                  receipts);
                }
            }

            // calculate new notifications
            if (!room.timeline.events.empty() && (room.unread_notifications.notification_count ||
                                                  room.unread_notifications.highlight_count)) {
                const auto qRoomId  = QString::fromStdString(room_id);
                const auto roomInfo = cache::singleRoomInfo(room_id);
                QString roomName    = QString::fromStdString(roomInfo.name);
                QString roomAvatar  = QString::fromStdString(roomInfo.avatar_url);
                if (roomAvatar.isEmpty())
                    roomAvatar = cache::roomAvatarUrl(room_id);

                auto currentReadMarker =
                  cache::getEventIndex(room_id, cache::getFullyReadEventId(room_id));

                auto ctx = mtx::pushrules::PushRuleEvaluator::RoomContext{
                  .user_display_name = cache::displayName(room_id, local_user),
                  .member_count      = cache::memberCount(room_id),
                  .power_levels      = Permissions(qRoomId).powerlevelEvent(),
                };
                std::vector<
                  std::pair<mtx::common::Relation, mtx::events::collections::TimelineEvents>>
                  relatedEvents;

                for (const auto &event : room.timeline.events) {
                    auto event_id = mtx::accessors::event_id(event);

                    // skip already read events
                    if (currentReadMarker &&
                        currentReadMarker > cache::getEventIndex(room_id, event_id))
                        continue;

                    // skip our messages
                    auto sender = mtx::accessors::sender(event);
                    if (sender == local_user)
                        continue;

                    mtx::events::collections::TimelineEvents te{event};
                    std::visit([room_id_ = room_id](auto &event_) { event_.room_id = room_id_; },
                               te);

                    const auto notificationsMessageContentPolicy =
                      userSettings_->notificationsMessageContentPolicy();
                    const bool decryptEncryptedNotificationContent =
                      notificationsMessageContentPolicy ==
                      UserSettings::NotificationMessageContentPolicy::WheneverAvailable;

                    if (auto encryptedEvent =
                          std::get_if<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(
                            &event);
                        encryptedEvent && decryptEncryptedNotificationContent) {
                        MegolmSessionIndex index(room_id, encryptedEvent->content);

                        auto result = olm::decryptEvent(index, *encryptedEvent);
                        if (result.event)
                            te = std::move(result.event).value();
                    }

                    relatedEvents.clear();
                    for (const auto &r : mtx::accessors::relations(te).relations) {
                        auto related = cache::getEvent(room_id, r.event_id);
                        if (related) {
                            relatedEvents.emplace_back(r, *related);
                            if (auto encryptedEvent = std::get_if<
                                  mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(
                                  &related.value());
                                encryptedEvent && decryptEncryptedNotificationContent) {
                                MegolmSessionIndex index(room_id, encryptedEvent->content);

                                auto result = olm::decryptEvent(index, *encryptedEvent);
                                if (result.event)
                                    relatedEvents.back().second = std::move(result.event).value();
                            }
                        }
                    }

                    auto actions = pushrules->evaluate(te, ctx, relatedEvents);
                    if (std::find(actions.begin(),
                                  actions.end(),
                                  mtx::pushrules::actions::Action{
                                    mtx::pushrules::actions::notify{}}) != actions.end()) {
                        if (!cache::isNotificationSent(event_id)) {
                            // We should only send one notification per event.
                            cache::markSentNotification(event_id);

                            // Don't send a notification when the current room is opened.
                            if (isRoomActive(qRoomId))
                                continue;

                            if (userSettings_->notificationsEnabled()) {
                                notifications.push_back(PendingNotification{
                                  .roomId        = qRoomId,
                                  .roomName      = roomName,
                                  .roomAvatarUrl = roomAvatar,
                                  .event         = std::move(te),
                                  .actions       = actions,
                                });
                            }
                        }
                    }
                }
            }
        }
        if (notifications.size() <= 5) {
            for (const auto &notification : notifications) {
                AvatarProvider::resolve(notification.roomAvatarUrl,
                                        96,
                                        this,
                                        [this,
                                         te_      = notification.event,
                                         room_id_ = notification.roomId.toStdString(),
                                         actions_ = notification.actions](QPixmap image) {
                                            notificationsManager->postNotification(
                                              mtx::responses::Notification{
                                                .actions     = actions_,
                                                .event       = std::move(te_),
                                                .read        = false,
                                                .profile_tag = "",
                                                .room_id     = room_id_,
                                                .ts          = 0,
                                              },
                                              image.toImage());
                                        });
            }
        } else if (!notifications.empty()) {
            std::map<QString, std::size_t> missedEvents;
            std::map<QString, QString> roomNames;
            for (const auto &notification : notifications) {
                missedEvents[notification.roomId]++;
                if (!notification.roomName.isEmpty())
                    roomNames[notification.roomId] = notification.roomName;
            }
            QString body;
            for (const auto &[roomId, nbNotifs] : missedEvents) {
                const auto roomName = roomNames.count(roomId) ? roomNames[roomId] : roomId;
                body += tr("%n unread message(s) in room %1\n", nullptr, nbNotifs).arg(roomName);
            }
            emit notificationsManager->systemPostNotificationCb(
              "", "", "New messages while away", body, QImage());
        }
    }
}

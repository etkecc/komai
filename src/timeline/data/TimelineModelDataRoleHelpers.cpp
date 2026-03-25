// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include <algorithm>

#include <QTime>

#include "utils/MediaIcons.h"

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "events/EventAccessors.h"
#include "utils/Utils.h"

QVariant
TimelineModel::mediaMetadataForEvent(const mtx::events::collections::TimelineEvents &event,
                                     int role) const
{
    switch (role) {
    case Url:
        return QVariant(QString::fromStdString(mtx::accessors::url(event)));
    case ThumbnailUrl:
        return QVariant(QString::fromStdString(mtx::accessors::thumbnail_url(event)));
    case Duration:
        return QVariant(static_cast<qulonglong>(mtx::accessors::duration(event)));
    case Blurhash:
        return QVariant(QString::fromStdString(mtx::accessors::blurhash(event)));
    case Filename:
        return QVariant(QString::fromStdString(mtx::accessors::filename(event)));
    case Filesize:
        return QVariant(utils::humanReadableFileSize(mtx::accessors::filesize(event)));
    case FilesizeBytes:
        return QVariant(static_cast<qulonglong>(mtx::accessors::filesize(event)));
    case MimeType:
        return QVariant(QString::fromStdString(mtx::accessors::mimetype(event)));
    case FileTypeIconSource:
        return QVariant(
          utils::fileTypeIconSource(QString::fromStdString(mtx::accessors::mimetype(event))));
    case OriginalHeight:
        return QVariant(qulonglong{mtx::accessors::media_height(event)});
    case OriginalWidth:
        return QVariant(qulonglong{mtx::accessors::media_width(event)});
    case ProportionalHeight: {
        auto w = mtx::accessors::media_width(event);
        if (w == 0)
            w = 1;

        double prop = (double)mtx::accessors::media_height(event) / (double)w;

        return {prop > 0 ? prop : 1.};
    }
    default:
        return {};
    }
}

QVariant
TimelineModel::senderRoleDataForEvent(const mtx::events::collections::TimelineEvents &event,
                                      int role,
                                      const std::string &localUserStd) const
{
    switch (role) {
    case IsSender:
        return {mtx::accessors::sender(event) == localUserStd};
    case UserId:
        return QVariant(QString::fromStdString(mtx::accessors::sender(event)));
    case UserName:
        return QVariant(displayName(QString::fromStdString(mtx::accessors::sender(event))));
    case UserPowerlevel:
        return static_cast<qlonglong>(permissions_.powerlevelEvent().user_level(
          mtx::accessors::sender(event), permissions_.createEvent()));
    default:
        return {};
    }
}

QVariant
TimelineModel::messageSummaryRoleDataForEvent(const mtx::events::collections::TimelineEvents &event,
                                              int role) const
{
    switch (role) {
    case Day: {
        QDateTime prevDate = mtx::accessors::origin_server_ts(event);
        prevDate.setTime(QTime());
        return QVariant(prevDate.toMSecsSinceEpoch());
    }
    case Timestamp:
        return QVariant(mtx::accessors::origin_server_ts(event));
    case Type:
        return {qml_mtx_events::toRoomEventType(event)};
    case TypeString:
        return QVariant(qml_mtx_events::toRoomEventTypeString(event));
    case IsOnlyEmoji: {
        QString qBody = QString::fromStdString(mtx::accessors::body(event));

        QVector<uint> utf32_string = qBody.toUcs4();
        int emojiCount             = 0;

        for (auto &code : utf32_string) {
            if (utils::codepointIsEmoji(code)) {
                emojiCount++;
            } else {
                return {0};
            }
        }

        return {emojiCount};
    }
    case Body:
        return QVariant(
          utils::replaceEmoji(QString::fromStdString(mtx::accessors::body(event)).toHtmlEscaped()));
    case HasFormattedBody:
        return QVariant(!mtx::accessors::formatted_body(event).empty());
    default:
        return {};
    }
}

QVariant
TimelineModel::messageStatusRoleDataForEvent(const mtx::events::collections::TimelineEvents &event,
                                             int role,
                                             const std::string &localUserStd) const
{
    switch (role) {
    case EventId:
        return QVariant(effectiveEventIdForEvent(event));
    case State:
        return deliveryStateForEvent(event, localUserStd);
    case IsEdited:
        return {mtx::accessors::relations(event).replaces().has_value()};
    case IsEditable:
        return {!mtx::accessors::is_state_event(event) &&
                mtx::accessors::sender(event) == localUserStd};
    case IsEncrypted:
        return isEncryptedForEvent(event);
    case IsStateEvent:
        return mtx::accessors::is_state_event(event);
    case Trustlevel:
        return trustLevelForEvent(event);
    default:
        return {};
    }
}

QVariant
TimelineModel::roomContextRoleDataForEvent(const mtx::events::collections::TimelineEvents &event,
                                           int role) const
{
    switch (role) {
    case Room:
        return QVariant::fromValue(this);
    case RoomId:
        return QVariant(room_id_);
    case RoomName:
        return QVariant(utils::replaceEmoji(
          QString::fromStdString(mtx::accessors::room_name(event)).toHtmlEscaped()));
    case RoomTopic:
        return QVariant(utils::replaceEmoji(
          utils::linkifyMessage(QString::fromStdString(mtx::accessors::room_topic(event))
                                  .toHtmlEscaped()
                                  .replace(QLatin1String("\n"), QLatin1String("<br>")))));
    case CallType:
        return QVariant(QString::fromStdString(mtx::accessors::call_type(event)));
    default:
        return {};
    }
}

QVariant
TimelineModel::deliveryStateForEvent(const mtx::events::collections::TimelineEvents &event,
                                     const std::string &localUserStd) const
{
    auto idstr          = mtx::accessors::event_id(event);
    auto id             = QString::fromStdString(idstr);
    auto containsOthers = [&localUserStd](const auto &vec) {
        for (const auto &e : vec)
            if (e.second != localUserStd)
                return true;
        return false;
    };

    // only show read receipts for messages not from us
    if (mtx::accessors::sender(event) != localUserStd)
        return qml_mtx_events::Empty;
    else if (!id.isEmpty() && id[0] == 'm') {
        auto pending = cache::pendingEvents(this->room_id_.toStdString());
        if (std::find(pending.begin(), pending.end(), idstr) != pending.end())
            return qml_mtx_events::Sent;
        else
            return qml_mtx_events::Failed;
    } else if (containsOthers(cache::readReceipts(id, room_id_)))
        return qml_mtx_events::Read;
    else
        return qml_mtx_events::Received;
}

QVariant
TimelineModel::notificationLevelForEvent(const mtx::events::collections::TimelineEvents &event,
                                         const std::string &localUserStd) const
{
    const auto &push = ChatPage::instance()->pushruleEvaluator();
    if (push) {
        // skip our messages
        auto sender = mtx::accessors::sender(event);
        if (sender == localUserStd)
            return qml_mtx_events::NotificationLevel::Nothing;

        const auto &id = mtx::accessors::event_id(event);
        std::vector<std::pair<mtx::common::Relation, mtx::events::collections::TimelineEvents>>
          relatedEvents;
        for (const auto &r : mtx::accessors::relations(event).relations) {
            auto related = events.get(r.event_id, id);
            if (related) {
                relatedEvents.emplace_back(r, *related);
            }
        }

        auto actions = push->evaluate({event}, pushrulesRoomContext(), relatedEvents);
        if (std::find(actions.begin(),
                      actions.end(),
                      mtx::pushrules::actions::Action{
                        mtx::pushrules::actions::set_tweak_highlight{}}) != actions.end()) {
            return qml_mtx_events::NotificationLevel::Highlight;
        }
        if (std::find(actions.begin(),
                      actions.end(),
                      mtx::pushrules::actions::Action{mtx::pushrules::actions::notify{}}) !=
            actions.end()) {
            return qml_mtx_events::NotificationLevel::Notify;
        }
    }
    return qml_mtx_events::NotificationLevel::Nothing;
}

QString
TimelineModel::effectiveEventIdForEvent(const mtx::events::collections::TimelineEvents &event) const
{
    if (auto replaces = mtx::accessors::relations(event).replaces())
        return QString::fromStdString(replaces.value());
    return QString::fromStdString(mtx::accessors::event_id(event));
}

QString
TimelineModel::replyToForEvent(const mtx::events::collections::TimelineEvents &event) const
{
    const auto &rels = mtx::accessors::relations(event);
    return QString::fromStdString(rels.reply_to(!rels.thread()).value_or(""));
}

QString
TimelineModel::threadIdForEvent(const mtx::events::collections::TimelineEvents &event) const
{
    return QString::fromStdString(mtx::accessors::relations(event).thread().value_or(""));
}

QVariant
TimelineModel::reactionsForEvent(const mtx::events::collections::TimelineEvents &event) const
{
    auto id = mtx::accessors::relations(event).replaces().value_or(mtx::accessors::event_id(event));
    return QVariant::fromValue(events.reactions(id));
}

bool
TimelineModel::isEncryptedForEvent(const mtx::events::collections::TimelineEvents &event) const
{
    auto encrypted_event = events.get(mtx::accessors::event_id(event), "", false);
    return encrypted_event &&
           std::holds_alternative<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(
             *encrypted_event);
}

crypto::Trust
TimelineModel::trustLevelForEvent(const mtx::events::collections::TimelineEvents &event) const
{
    (void)event;
    return crypto::Trust::Unverified;
}

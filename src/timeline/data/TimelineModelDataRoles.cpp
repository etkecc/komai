// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include "TimelineEventTypes.h"
#include "events/EventAccessors.h"
#include "utils/Utils.h"

QVariant
TimelineModel::data(const mtx::events::collections::TimelineEvents &event, int role) const
{
    using namespace mtx::accessors;
    const auto localUser    = utils::localUser();
    const auto localUserStd = localUser.toStdString();

    switch (role) {
    case IsSender:
    case UserId:
    case UserName:
    case UserPowerlevel:
        return senderRoleDataForEvent(event, role, localUserStd);

    case Day:
    case Timestamp:
    case Type:
    case TypeString:
    case IsOnlyEmoji:
    case Body:
    case HasFormattedBody:
        return messageSummaryRoleDataForEvent(event, role);
    case FormattedBody:
        return QVariant(formattedBodyForEvent(event));
    case FormattedStateEvent:
        return formattedStateEventForEvent(event);
    case StateEventIconSource:
        return stateEventIconSourceForEvent(event);
    case Url:
    case ThumbnailUrl:
    case Duration:
    case Blurhash:
    case Filename:
    case Filesize:
    case FilesizeBytes:
    case MimeType:
    case OriginalHeight:
    case OriginalWidth:
    case ProportionalHeight:
        return mediaMetadataForEvent(event, role);
    case EventId:
    case State:
    case IsEdited:
    case IsEditable:
    case IsEncrypted:
    case IsStateEvent:
    case Trustlevel:
        return messageStatusRoleDataForEvent(event, role, localUserStd);

    case Notificationlevel:
        return notificationLevelForEvent(event, localUserStd);

    case EncryptionError:
        return events.decryptionError(event_id(event));

    case ReplyTo:
        return QVariant(replyToForEvent(event));
    case ThreadId:
        return QVariant(threadIdForEvent(event));
    case Reactions:
        return reactionsForEvent(event);
    case Room:
    case RoomId:
    case RoomName:
    case RoomTopic:
    case CallType:
        return roomContextRoleDataForEvent(event, role);
    case Dump:
        return QVariant(dumpForEvent(event));
    case RelatedEventCacheBuster:
        return relatedEventCacheBuster;
    case IsHiddenEvent: {
        auto eventType = std::visit([](const auto &ev) { return ev.type; }, event);
        auto hidden    = qml_mtx_events::defaultHiddenEventTypes();
        if (std::find(hidden.begin(), hidden.end(), eventType) != hidden.end())
            return true;
        return false;
    }
    default:
        return {};
    }
}

QVariant
TimelineModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 && index.row() >= rowCount())
        return {};

    auto idx   = rowCount() - index.row() - 1;
    auto event = events.get(idx);

    if (!event)
        return "";

    if (role == IsHiddenEvent) {
        auto result = data(*event, static_cast<int>(IsHiddenEvent));
        if (result.toBool())
            return result;

        // Hide edit events that have their own timeline index (e.g. encrypted rooms where the
        // event was indexed before decryption revealed it was an edit). Don't hide edit events
        // that are serving as content replacements for the original message.
        if (mtx::accessors::relations(*event).replaces()) {
            auto indexedId = events.indexToId(idx);
            if (indexedId && *indexedId == mtx::accessors::event_id(*event))
                return true;
        }

        return false;
    }

    return data(*event, role);
}

void
TimelineModel::multiData(const QModelIndex &index, QModelRoleDataSpan roleDataSpan) const
{
    if (index.row() < 0 && index.row() >= rowCount()) {
        for (QModelRoleData &roleData : roleDataSpan)
            roleData.clearData();
        return;
    }

    // nhlog::db()->debug("MultiData called for {}", index.row());

    auto idx   = rowCount() - index.row() - 1;
    auto event = events.get(idx);

    if (!event) {
        for (QModelRoleData &roleData : roleDataSpan)
            roleData.clearData();
        return;
    }

    for (QModelRoleData &roleData : roleDataSpan) {
        if (roleData.role() == IsHiddenEvent)
            roleData.setData(data(index, roleData.role()));
        else
            roleData.setData(data(*event, roleData.role()));
    }
}

void
TimelineModel::multiData(const QString &id,
                         const QString &relatedTo,
                         QModelRoleDataSpan roleDataSpan) const
{
    if (id.isEmpty()) {
        for (QModelRoleData &roleData : roleDataSpan)
            roleData.clearData();
        return;
    }

    // nhlog::db()->debug("MultiData called for {}", id.toStdString());

    auto event = events.get(id.toStdString(), relatedTo.toStdString());

    if (!event) {
        for (QModelRoleData &roleData : roleDataSpan)
            roleData.clearData();
        return;
    }

    for (QModelRoleData &roleData : roleDataSpan) {
        int role = roleData.role();

        roleData.setData(data(*event, role));
    }
}

QVariant
TimelineModel::dataById(const QString &id, int role, const QString &relatedTo)
{
    if (auto event = events.get(id.toStdString(), relatedTo.toStdString()))
        return data(*event, role);
    return {};
}

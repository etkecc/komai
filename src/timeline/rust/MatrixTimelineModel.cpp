// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/rust/MatrixTimelineModel.h"

#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/TimelineEventTypes.h"
#include "utils/MediaIcons.h"
#include "utils/Utils.h"

#include <QByteArray>
#include <QDateTime>
#include <algorithm>

namespace komai {

namespace {
std::optional<int>
configuredInitialVisibleWindow()
{
    const auto value = qgetenv("KOMAI_PERF_MATRIX_TIMELINE_INITIAL_WINDOW").trimmed();
    if (value.isEmpty())
        return std::nullopt;

    bool ok          = false;
    const auto count = value.toInt(&ok);
    if (!ok || count <= 0)
        return std::nullopt;

    return count;
}

int
emojiOnlyCodepointCount(const QString &body)
{
    if (body.isEmpty())
        return 0;

    const auto utf32 = body.toUcs4();
    int emojiCount   = 0;

    for (const auto code : utf32) {
        if (!utils::codepointIsEmoji(code))
            return 0;
        ++emojiCount;
    }

    return emojiCount;
}

bool
isStateLikeKind(const QString &kind)
{
    return kind == QStringLiteral("membership_change") ||
           kind == QStringLiteral("profile_change") || kind == QStringLiteral("other_state") ||
           kind == QStringLiteral("failed_to_parse_state");
}

int
dayKeyFromTimestamp(uint64_t timestampMs)
{
    if (timestampMs == 0)
        return 0;
    auto date = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(timestampMs)).date();
    return date.year() * 10000 + date.month() * 100 + date.day();
}

int
deliveryStateToEventState(const QString &state)
{
    if (state == QStringLiteral("pending"))
        return qml_mtx_events::Pending;
    if (state == QStringLiteral("sent"))
        return qml_mtx_events::Sent;
    if (state == QStringLiteral("failed"))
        return qml_mtx_events::Failed;
    if (state == QStringLiteral("read"))
        return qml_mtx_events::Read;
    if (state == QStringLiteral("received"))
        return qml_mtx_events::Received;
    return qml_mtx_events::Empty;
}

QString
stateEventIconForKind(const QString &kind)
{
    if (kind == QStringLiteral("membership_change"))
        return QStringLiteral(":/icons/icons/ui/state-member-join.svg");
    if (kind == QStringLiteral("profile_change"))
        return QStringLiteral(":/icons/icons/ui/state-member-display-name.svg");
    return QStringLiteral(":/icons/icons/ui/state-event.svg");
}

QString
formatBodyHtml(const QString &body, const QString &formattedBody = {})
{
    if (body.isEmpty() && formattedBody.isEmpty())
        return {};

    QString html;
    if (!formattedBody.isEmpty()) {
        // The formatted body is already HTML from the server; sanitize and linkify it directly
        // without reinterpreting plain text as markdown locally.
        html = utils::escapeBlacklistedHtml(formattedBody);
        html = utils::linkifyMessage(html);
    } else {
        html = body.toHtmlEscaped().replace(u'\n', QStringLiteral("<br>"));
        html = utils::linkifyMessage(html);
    }
    return utils::replaceEmoji(html);
}

QString
stableTimelineItemKey(const MatrixTimelineItem &item)
{
    const auto eventId = item.eventId.trimmed();
    if (!eventId.isEmpty())
        return eventId;
    return item.itemId.trimmed();
}

QString
effectiveReplyPreviewBody(const MatrixTimelineItem &item)
{
    if (!item.replyBody.isEmpty() || !item.replyFormattedBody.isEmpty())
        return item.replyBody;

    return QStringLiteral("[Original message unavailable]");
}

QString
effectiveReplyPreviewDisplayName(const MatrixTimelineItem &item)
{
    if (!item.replySenderDisplayName.trimmed().isEmpty())
        return item.replySenderDisplayName.trimmed();
    if (!item.replySenderId.trimmed().isEmpty())
        return item.replySenderId.trimmed();

    return QStringLiteral("Unknown sender");
}

void
computeDerivedFields(MatrixTimelineItem &item, const QString &roomId)
{
    const bool isState = isStateLikeKind(item.itemKind);
    item.cachedType = qml_mtx_events::matrixTimelineEventType(item.itemKind, item.matrixEventType);
    item.cachedIsHiddenEvent =
      UserSettings::instance() &&
      UserSettings::instance()->isTimelineEventHiddenInRoom(item.cachedType, roomId);
    item.cachedEmojiOnlyCount =
      item.cachedType == qml_mtx_events::TextMessage ? emojiOnlyCodepointCount(item.body) : 0;
    item.cachedDay          = dayKeyFromTimestamp(item.timestamp);
    item.cachedStatus       = deliveryStateToEventState(item.deliveryState);
    item.cachedIsStateEvent = isState;
    item.cachedIsEncrypted  = item.mediaIsEncrypted || item.thumbnailIsEncrypted ||
                             item.itemKind == QStringLiteral("unable_to_decrypt");
    item.cachedIsEditable    = item.isOwn && (item.itemKind == QStringLiteral("message") ||
                                           item.itemKind == QStringLiteral("notice") ||
                                           item.itemKind == QStringLiteral("emote"));
    item.cachedProportionalH = (item.mediaWidth > 0 && item.mediaHeight > 0)
                                 ? static_cast<double>(item.mediaHeight) / item.mediaWidth
                                 : 0.0;
    item.cachedFormattedBody = isState ? QString() : formatBodyHtml(item.body, item.formattedBody);
    item.cachedFormattedStateEvent =
      isState ? formatBodyHtml(item.body, item.formattedBody) : QString();
    item.cachedStateEventIcon = isState ? stateEventIconForKind(item.itemKind) : QString();
    item.cachedFilesize =
      item.mediaSizeBytes > 0 ? utils::humanReadableFileSize(item.mediaSizeBytes) : QString();
    item.cachedFilename =
      item.fileName.isEmpty() ? (item.body.isEmpty() ? QString() : item.body) : item.fileName;
    item.cachedFileTypeIcon = utils::fileTypeIconSource(item.mimeType);
}

} // namespace

MatrixTimelineModel::MatrixTimelineModel(QObject *parent)
  : EventDataSource(parent)
{
    if (const auto settings = UserSettings::instance()) {
        connect(settings.get(),
                &UserSettings::hiddenTimelineEventTypesChanged,
                this,
                &MatrixTimelineModel::refreshDerivedFields);
    }
}

void
MatrixTimelineModel::setRoomId(const QString &roomId)
{
    const auto normalizedRoomId = roomId.trimmed();
    if (roomId_ == normalizedRoomId)
        return;

    roomId_ = normalizedRoomId;
    refreshDerivedFields();
}

int
MatrixTimelineModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return items_.size();
}

QVariant
MatrixTimelineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size())
        return {};

    const auto &item = items_.at(index.row());

    // clang-format off
    switch (role) {
    // --- TimelineModel-compatible roles (most use pre-computed cached fields) ---
    case Type:               return item.cachedType;
    case TypeString:         return item.itemKind;
    case IsOnlyEmoji:        return item.cachedEmojiOnlyCount;
    case Body:               return item.body;
    case FormattedBody:      return item.cachedFormattedBody;
    case HasFormattedBody:   return !item.cachedIsStateEvent && !item.body.isEmpty();
    case FormattedStateEvent:return item.cachedFormattedStateEvent;
    case StateEventIconSource:return item.cachedStateEventIcon;
    case IsSender:           return item.isOwn;
    case UserId:             return item.senderId;
    case UserName:           return item.senderDisplayName;
    case UserPowerlevel:     return 0;
    case Day:                return item.cachedDay;
    case Timestamp:          return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(item.timestamp));
    case Url:                return item.mediaUrl;
    case ThumbnailUrl:       return item.thumbnailUrl;
    case Duration:           return static_cast<int>(item.mediaDurationMs);
    case Blurhash:           return item.blurhash;
    case Filename:           return item.cachedFilename;
    case Filesize:           return item.cachedFilesize;
    case FilesizeBytes:      return static_cast<int>(item.mediaSizeBytes);
    case MimeType:           return item.mimeType;
    case OriginalHeight:     return static_cast<int>(item.mediaHeight);
    case OriginalWidth:      return static_cast<int>(item.mediaWidth);
    case ProportionalHeight: return item.cachedProportionalH;
    case EventId:            return item.eventId;
    case Status:             return item.cachedStatus;
    case IsEdited:           return item.isEdited;
    case IsEditable:         return item.cachedIsEditable;
    case IsEncrypted:        return item.cachedIsEncrypted;
    case IsStateEvent:       return item.cachedIsStateEvent;
    case Trustlevel:         return 0;
    case Notificationlevel:  return static_cast<int>(qml_mtx_events::Nothing);
    case EncryptionError:    return QString();
    case ReplyTo:            return item.cachedIsStateEvent ? QString() : item.replyEventId;
    case ThreadId:           return item.threadId;
    case Reactions:          return item.reactions;
    case Room:               return false;
    case RoomId:             return QString();
    case CallType:           return QString();
    case Dump:               return QVariant();
    case RelatedEventCacheBuster: return 0;
    case IsHiddenEvent:      return item.cachedIsHiddenEvent;
    case FileTypeIconSource: return item.cachedFileTypeIcon;

    // --- Extra roles ---
    case ItemId:             return item.itemId;
    case SenderAvatarUrl:    return item.senderAvatarUrl;
    case ReactionsSummary:   return item.reactionsSummary;
    case PreviousTimestamp: {
        const auto prev = previousVisibleRowFrom(index.row());
        return prev >= 0
            ? static_cast<qulonglong>(items_.at(prev).timestamp)
            : static_cast<qulonglong>(0);
    }
    case PreviousSenderId: {
        const auto prev = previousVisibleRowFrom(index.row());
        return prev >= 0 ? items_.at(prev).senderId : QString();
    }
    case PreviousItemKind: {
        const auto prev = previousVisibleRowFrom(index.row());
        return prev >= 0 ? items_.at(prev).itemKind : QString();
    }
    case DeliveryState:      return item.deliveryState;

    default:                 return {};
    }
    // clang-format on
}

QVariant
MatrixTimelineModel::replyData(const MatrixTimelineItem &parentItem, int role) const
{
    const auto effectiveBody        = effectiveReplyPreviewBody(parentItem);
    const auto effectiveDisplayName = effectiveReplyPreviewDisplayName(parentItem);
    const auto effectiveFormattedBody =
      formatBodyHtml(effectiveBody, parentItem.replyFormattedBody);
    const auto replyType =
      parentItem.replyItemKind.isEmpty() && parentItem.replyMatrixEventType.isEmpty()
        ? static_cast<int>(qml_mtx_events::TextMessage)
        : qml_mtx_events::matrixTimelineEventType(parentItem.replyItemKind,
                                                  parentItem.replyMatrixEventType);
    const auto replyProportionalHeight =
      (parentItem.replyMediaWidth > 0 && parentItem.replyMediaHeight > 0)
        ? static_cast<double>(parentItem.replyMediaHeight) / parentItem.replyMediaWidth
        : 0.0;
    const auto replyFilesize     = parentItem.replyMediaSizeBytes > 0
                                     ? utils::humanReadableFileSize(parentItem.replyMediaSizeBytes)
                                     : QString();
    const auto replyFilename     = parentItem.replyFileName.isEmpty()
                                     ? (effectiveBody.isEmpty() ? QString() : effectiveBody)
                                     : parentItem.replyFileName;
    const auto replyFileTypeIcon = utils::fileTypeIconSource(parentItem.replyMimeType);

    // clang-format off
    switch (role) {
    case Type:               return replyType;
    case TypeString:         return parentItem.replyItemKind.isEmpty() ? QStringLiteral("message")
                                                                       : parentItem.replyItemKind;
    case IsOnlyEmoji:        return emojiOnlyCodepointCount(effectiveBody);
    case Body:               return effectiveBody;
    case FormattedBody:      return effectiveFormattedBody;
    case HasFormattedBody:   return !effectiveBody.isEmpty() || !parentItem.replyFormattedBody.isEmpty();
    case FormattedStateEvent:return QString();
    case StateEventIconSource:return QString();
    case IsSender:           return false;
    case UserId:             return parentItem.replySenderId;
    case UserName:           return effectiveDisplayName;
    case UserPowerlevel:     return 0;
    case Day:                return 0;
    case Timestamp:          return QDateTime::fromMSecsSinceEpoch(0);
    case Url:                return parentItem.replyMediaUrl;
    case ThumbnailUrl:       return parentItem.replyThumbnailUrl;
    case Duration:           return static_cast<int>(parentItem.replyMediaDurationMs);
    case Blurhash:           return parentItem.replyBlurhash;
    case Filename:           return replyFilename;
    case Filesize:           return replyFilesize;
    case FilesizeBytes:      return static_cast<int>(parentItem.replyMediaSizeBytes);
    case MimeType:           return parentItem.replyMimeType;
    case OriginalHeight:     return static_cast<int>(parentItem.replyMediaHeight);
    case OriginalWidth:      return static_cast<int>(parentItem.replyMediaWidth);
    case ProportionalHeight: return replyProportionalHeight;
    case EventId:            return parentItem.replyEventId;
    case Status:             return 0;
    case IsEdited:           return false;
    case IsEditable:         return false;
    case IsEncrypted:        return false;
    case IsStateEvent:       return false;
    case Trustlevel:         return 0;
    case Notificationlevel:  return 0;
    case EncryptionError:    return QString();
    case ReplyTo:            return QString();
    case ThreadId:           return QString();
    case Reactions:          return QVariant();
    case Room:               return false;
    case RoomId:             return QString();
    case CallType:           return QString();
    case Dump:               return QVariant();
    case RelatedEventCacheBuster: return 0;
    case IsHiddenEvent:      return false;
    case FileTypeIconSource: return replyFileTypeIcon;
    default:                 return {};
    }
    // clang-format on
}

QHash<int, QByteArray>
MatrixTimelineModel::roleNames() const
{
    return {
      // TimelineModel-compatible roles
      {Type, "type"},
      {TypeString, "typeString"},
      {IsOnlyEmoji, "isOnlyEmoji"},
      {Body, "body"},
      {FormattedBody, "formattedBody"},
      {HasFormattedBody, "hasFormattedBody"},
      {FormattedStateEvent, "formattedStateEvent"},
      {StateEventIconSource, "stateEventIconSource"},
      {IsSender, "isSender"},
      {UserId, "userId"},
      {UserName, "userName"},
      {UserPowerlevel, "userPowerlevel"},
      {Day, "day"},
      {Timestamp, "timestamp"},
      {Url, "url"},
      {ThumbnailUrl, "thumbnailUrl"},
      {Duration, "duration"},
      {Blurhash, "blurhash"},
      {Filename, "filename"},
      {Filesize, "filesize"},
      {FilesizeBytes, "filesizeBytes"},
      {MimeType, "mimetype"},
      {FileTypeIconSource, "fileTypeIconSource"},
      {OriginalHeight, "originalHeight"},
      {OriginalWidth, "originalWidth"},
      {ProportionalHeight, "proportionalHeight"},
      {EventId, "eventId"},
      {Status, "status"},
      {IsEdited, "isEdited"},
      {IsEditable, "isEditable"},
      {IsEncrypted, "isEncrypted"},
      {IsStateEvent, "isStateEvent"},
      {Trustlevel, "trustlevel"},
      {Notificationlevel, "notificationlevel"},
      {EncryptionError, "encryptionError"},
      {ReplyTo, "replyTo"},
      {ThreadId, "threadId"},
      {Reactions, "reactions"},
      {Room, "room"},
      {RoomId, "roomId"},
      {CallType, "callType"},
      {Dump, "dump"},
      {RelatedEventCacheBuster, "relatedEventCacheBuster"},
      {IsHiddenEvent, "isHiddenEvent"},

      // Extra roles
      {ItemId, "itemId"},
      {SenderAvatarUrl, "senderAvatarUrl"},
      {ReactionsSummary, "reactionsSummary"},
      {PreviousTimestamp, "previousTimestamp"},
      {PreviousSenderId, "previousSenderId"},
      {PreviousItemKind, "previousItemKind"},
      {DeliveryState, "deliveryState"},
    };
}

// --- EventDataSource interface ---

QVariant
MatrixTimelineModel::dataById(const QString &id, int role, const QString &relatedTo)
{
    if (id.isEmpty())
        return {};

    // Look up the item directly by its event/item ID.
    const auto row = rowForEventId(id);
    if (row >= 0 && row < items_.size())
        return data(index(row), role);

    // If not found as a standalone item but we have a parent event,
    // return inline reply data from the parent.
    if (!relatedTo.isEmpty()) {
        const auto parentRow = rowForEventIdInItems(allItems_, relatedTo);
        if (parentRow >= 0 && parentRow < allItems_.size())
            return replyData(allItems_.at(parentRow), role);
    }

    return {};
}

void
MatrixTimelineModel::multiData(const QString &id,
                               const QString &relatedTo,
                               QModelRoleDataSpan roleDataSpan) const
{
    if (id.isEmpty()) {
        for (QModelRoleData &roleData : roleDataSpan)
            roleData.clearData();
        return;
    }

    const auto row = rowForEventId(id);
    if (row >= 0 && row < items_.size()) {
        const auto idx = index(row);
        for (QModelRoleData &roleData : roleDataSpan)
            roleData.setData(data(idx, roleData.role()));
        return;
    }

    // Reply fallback: return inline reply data from the parent event.
    if (!relatedTo.isEmpty()) {
        const auto parentRow = rowForEventIdInItems(allItems_, relatedTo);
        if (parentRow >= 0 && parentRow < allItems_.size()) {
            const auto &parentItem = allItems_.at(parentRow);
            for (QModelRoleData &roleData : roleDataSpan)
                roleData.setData(replyData(parentItem, roleData.role()));
            return;
        }
    }

    for (QModelRoleData &roleData : roleDataSpan)
        roleData.clearData();
}

int
MatrixTimelineModel::idToIndex(const QString &id) const
{
    return rowForEventId(id);
}

int
MatrixTimelineModel::rowForEventId(const QString &eventId) const
{
    return rowForEventIdInItems(items_, eventId);
}

int
MatrixTimelineModel::rowForEventIdInItems(const QVector<MatrixTimelineItem> &items,
                                          const QString &eventId) const
{
    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return -1;

    for (int row = 0; row < items.size(); ++row) {
        const auto &item = items.at(row);
        if (item.eventId == trimmedEventId || item.itemId == trimmedEventId)
            return row;
    }

    return -1;
}

QVariantMap
MatrixTimelineModel::itemAt(int row) const
{
    QVariantMap itemData;

    if (row < 0 || row >= items_.size())
        return itemData;

    const auto roleNameMap = roleNames();
    const auto itemIndex   = index(row);
    for (auto it = roleNameMap.cbegin(); it != roleNameMap.cend(); ++it) {
        itemData.insert(QString::fromUtf8(it.value()), data(itemIndex, it.key()));
    }

    return itemData;
}

QVariantMap
MatrixTimelineModel::previewDataForEvent(const QString &eventId, const QString &relatedTo) const
{
    QVariantMap previewData;
    const auto normalizedEventId   = eventId.trimmed();
    const auto normalizedRelatedTo = relatedTo.trimmed();

    auto insertReplyRole = [&previewData](const QString &key, const QVariant &value) {
        previewData.insert(key, value);
    };

    auto insertItemPreview = [&previewData](const MatrixTimelineItem &item) {
        previewData.insert(QStringLiteral("eventId"), item.eventId);
        previewData.insert(QStringLiteral("type"), item.cachedType);
        previewData.insert(QStringLiteral("typeString"), item.itemKind);
        previewData.insert(QStringLiteral("userId"), item.senderId);
        previewData.insert(QStringLiteral("userName"), item.senderDisplayName);
        previewData.insert(QStringLiteral("avatarUrl"), item.senderAvatarUrl);
        previewData.insert(QStringLiteral("body"), item.body);
        previewData.insert(QStringLiteral("formattedBody"), item.cachedFormattedBody);
        previewData.insert(QStringLiteral("formattedStateEvent"), item.cachedFormattedStateEvent);
        previewData.insert(QStringLiteral("stateEventIconSource"), item.cachedStateEventIcon);
        previewData.insert(QStringLiteral("isOnlyEmoji"), item.cachedEmojiOnlyCount);
        previewData.insert(QStringLiteral("url"), item.mediaUrl);
        previewData.insert(QStringLiteral("thumbnailUrl"), item.thumbnailUrl);
        previewData.insert(QStringLiteral("duration"),
                           static_cast<qulonglong>(item.mediaDurationMs));
        previewData.insert(QStringLiteral("blurhash"), QString());
        previewData.insert(QStringLiteral("filename"), item.cachedFilename);
        previewData.insert(QStringLiteral("filesize"), item.cachedFilesize);
        previewData.insert(QStringLiteral("filesizeBytes"),
                           static_cast<qulonglong>(item.mediaSizeBytes));
        previewData.insert(QStringLiteral("mimetype"), item.mimeType);
        previewData.insert(QStringLiteral("fileTypeIconSource"), item.cachedFileTypeIcon);
        previewData.insert(QStringLiteral("originalHeight"),
                           static_cast<qulonglong>(item.mediaHeight));
        previewData.insert(QStringLiteral("originalWidth"),
                           static_cast<qulonglong>(item.mediaWidth));
        previewData.insert(QStringLiteral("proportionalHeight"), item.cachedProportionalH);
        previewData.insert(QStringLiteral("callType"), QString());
        previewData.insert(QStringLiteral("isEdited"), item.isEdited);
        previewData.insert(QStringLiteral("isEditable"), item.cachedIsEditable);
        previewData.insert(QStringLiteral("isEncrypted"), item.cachedIsEncrypted);
        previewData.insert(QStringLiteral("isStateEvent"), item.cachedIsStateEvent);
        previewData.insert(QStringLiteral("replyTo"),
                           item.cachedIsStateEvent ? QString() : item.replyEventId);
        previewData.insert(QStringLiteral("threadId"), item.threadId);
    };

    if (!normalizedEventId.isEmpty()) {
        const auto directRow = rowForEventIdInItems(allItems_, normalizedEventId);
        if (directRow >= 0 && directRow < allItems_.size()) {
            insertItemPreview(allItems_.at(directRow));
            return previewData;
        }
    }

    if (normalizedEventId.isEmpty() || normalizedRelatedTo.isEmpty())
        return previewData;

    const auto parentRow = rowForEventIdInItems(allItems_, normalizedRelatedTo);
    if (parentRow < 0 || parentRow >= allItems_.size())
        return previewData;

    const auto &parentItem = allItems_.at(parentRow);
    insertReplyRole(QStringLiteral("eventId"), replyData(parentItem, EventId));
    insertReplyRole(QStringLiteral("type"), replyData(parentItem, Type));
    insertReplyRole(QStringLiteral("typeString"), replyData(parentItem, TypeString));
    insertReplyRole(QStringLiteral("userId"), replyData(parentItem, UserId));
    insertReplyRole(QStringLiteral("userName"), replyData(parentItem, UserName));
    insertReplyRole(QStringLiteral("avatarUrl"), QString());
    insertReplyRole(QStringLiteral("body"), replyData(parentItem, Body));
    insertReplyRole(QStringLiteral("formattedBody"), replyData(parentItem, FormattedBody));
    insertReplyRole(QStringLiteral("formattedStateEvent"),
                    replyData(parentItem, FormattedStateEvent));
    insertReplyRole(QStringLiteral("stateEventIconSource"),
                    replyData(parentItem, StateEventIconSource));
    insertReplyRole(QStringLiteral("isOnlyEmoji"), replyData(parentItem, IsOnlyEmoji));
    insertReplyRole(QStringLiteral("url"), replyData(parentItem, Url));
    insertReplyRole(QStringLiteral("thumbnailUrl"), replyData(parentItem, ThumbnailUrl));
    insertReplyRole(QStringLiteral("duration"), replyData(parentItem, Duration));
    insertReplyRole(QStringLiteral("blurhash"), replyData(parentItem, Blurhash));
    insertReplyRole(QStringLiteral("filename"), replyData(parentItem, Filename));
    insertReplyRole(QStringLiteral("filesize"), replyData(parentItem, Filesize));
    insertReplyRole(QStringLiteral("filesizeBytes"), replyData(parentItem, FilesizeBytes));
    insertReplyRole(QStringLiteral("mimetype"), replyData(parentItem, MimeType));
    insertReplyRole(QStringLiteral("fileTypeIconSource"),
                    replyData(parentItem, FileTypeIconSource));
    insertReplyRole(QStringLiteral("originalHeight"), replyData(parentItem, OriginalHeight));
    insertReplyRole(QStringLiteral("originalWidth"), replyData(parentItem, OriginalWidth));
    insertReplyRole(QStringLiteral("proportionalHeight"),
                    replyData(parentItem, ProportionalHeight));
    insertReplyRole(QStringLiteral("callType"), replyData(parentItem, CallType));
    insertReplyRole(QStringLiteral("isEdited"), replyData(parentItem, IsEdited));
    insertReplyRole(QStringLiteral("isEditable"), replyData(parentItem, IsEditable));
    insertReplyRole(QStringLiteral("isEncrypted"), replyData(parentItem, IsEncrypted));
    insertReplyRole(QStringLiteral("isStateEvent"), replyData(parentItem, IsStateEvent));
    insertReplyRole(QStringLiteral("replyTo"), replyData(parentItem, ReplyTo));
    insertReplyRole(QStringLiteral("threadId"), replyData(parentItem, ThreadId));
    return previewData;
}

QString
MatrixTimelineModel::avatarUrl(const QString &userId) const
{
    if (userId.isEmpty())
        return {};

    for (const auto &item : allItems_) {
        if (item.senderId == userId && !item.senderAvatarUrl.isEmpty())
            return item.senderAvatarUrl;
    }

    return {};
}

QString
MatrixTimelineModel::userNameForEvent(const QString &eventId) const
{
    const auto item = itemByEventId(eventId);
    return item ? item->senderDisplayName : QString();
}

QString
MatrixTimelineModel::userIdForEvent(const QString &eventId) const
{
    const auto item = itemByEventId(eventId);
    return item ? item->senderId : QString();
}

QString
MatrixTimelineModel::bodyForEvent(const QString &eventId) const
{
    const auto item = itemByEventId(eventId);
    return item ? item->body : QString();
}

QString
MatrixTimelineModel::typeStringForEvent(const QString &eventId) const
{
    const auto item = itemByEventId(eventId);
    return item ? item->itemKind : QString();
}

QString
MatrixTimelineModel::filenameForEvent(const QString &eventId) const
{
    const auto item = itemByEventId(eventId);
    if (!item)
        return {};
    if (!item->fileName.isEmpty())
        return item->fileName;
    if (!item->body.isEmpty())
        return item->body;
    return {};
}

std::optional<MatrixTimelineItem>
MatrixTimelineModel::itemByEventId(const QString &eventId) const
{
    const auto row = rowForEventIdInItems(allItems_, eventId);
    if (row < 0 || row >= allItems_.size())
        return std::nullopt;

    return allItems_.at(row);
}

int
MatrixTimelineModel::hiddenCount() const
{
    return std::max(0, static_cast<int>(allItems_.size() - revealedItemCount_));
}

void
MatrixTimelineModel::applyRedactedPresentation(MatrixTimelineItem &item) const
{
    item.body.clear();
    item.replyEventId.clear();
    item.replySenderId.clear();
    item.replySenderDisplayName.clear();
    item.replyBody.clear();
    item.reactions.clear();
    item.reactionsSummary.clear();
    item.itemKind = QStringLiteral("redacted");
    item.isEdited = false;
    item.mediaUrl.clear();
    item.thumbnailUrl.clear();
    item.fileName.clear();
    item.mimeType.clear();
    item.mediaWidth           = 0;
    item.mediaHeight          = 0;
    item.mediaDurationMs      = 0;
    item.mediaSizeBytes       = 0;
    item.mediaIsEncrypted     = false;
    item.thumbnailIsEncrypted = false;
    computeDerivedFields(item, roomId_);
}

bool
MatrixTimelineModel::redactItemByEventId(const QString &eventId)
{
    const auto normalizedEventId = eventId.trimmed();
    const auto row               = rowForEventId(normalizedEventId);
    if (row < 0 || row >= items_.size())
        return false;

    optimisticRedactedEventIds_.insert(normalizedEventId);

    auto &item = items_[row];
    applyRedactedPresentation(item);

    emit dataChanged(index(row), index(row));
    return true;
}

bool
MatrixTimelineModel::revealOlderItems(int additionalCount)
{
    const auto availableHiddenCount = hiddenCount();
    if (availableHiddenCount <= 0)
        return false;

    const auto revealCount = std::clamp(
      additionalCount > 0 ? additionalCount : availableHiddenCount, 1, availableHiddenCount);
    const auto oldVisibleCount = items_.size();
    revealedItemCount_ =
      std::min(static_cast<int>(allItems_.size()), revealedItemCount_ + revealCount);
    replaceVisibleItems(visibleItemsForRawCount(revealedItemCount_));

    // "countChanged" also acts as the TimelineManager state nudge used by
    // pending-jump recovery and viewport bookkeeping. Emit it even when the
    // visible row count stays flat because the newly revealed raw items are
    // all hidden by local preferences.
    if (items_.size() == oldVisibleCount)
        emit countChanged();
    return true;
}

void
MatrixTimelineModel::applyOptimisticRedactions(QVector<MatrixTimelineItem> &items)
{
    if (optimisticRedactedEventIds_.isEmpty())
        return;

    QSet<QString> resolvedEventIds;
    for (auto &item : items) {
        const auto eventId = item.eventId.trimmed();
        const auto itemId  = item.itemId.trimmed();
        if (!optimisticRedactedEventIds_.contains(eventId) &&
            !optimisticRedactedEventIds_.contains(itemId)) {
            continue;
        }

        if (item.itemKind == QStringLiteral("redacted")) {
            if (!eventId.isEmpty())
                resolvedEventIds.insert(eventId);
            if (!itemId.isEmpty())
                resolvedEventIds.insert(itemId);
            continue;
        }

        applyRedactedPresentation(item);
    }

    for (const auto &resolvedEventId : resolvedEventIds)
        optimisticRedactedEventIds_.remove(resolvedEventId);
}

void
MatrixTimelineModel::refreshDerivedFields()
{
    if (allItems_.isEmpty()) {
        revealedItemCount_ = 0;
        replaceVisibleItems({});
        return;
    }

    for (auto &item : allItems_)
        computeDerivedFields(item, roomId_);

    revealedItemCount_ = std::clamp(revealedItemCount_, 0, static_cast<int>(allItems_.size()));
    replaceVisibleItems(visibleItemsForRawCount(revealedItemCount_));
}

int
MatrixTimelineModel::previousVisibleRowFrom(int row) const
{
    for (int candidate = row + 1; candidate < items_.size(); ++candidate) {
        if (!items_.at(candidate).cachedIsHiddenEvent)
            return candidate;
    }

    return -1;
}

QVector<MatrixTimelineItem>
MatrixTimelineModel::visibleItemsForRawCount(int rawVisibleCount) const
{
    QVector<MatrixTimelineItem> visibleItems;
    const auto clampedRawVisibleCount =
      std::clamp(rawVisibleCount, 0, static_cast<int>(allItems_.size()));
    visibleItems.reserve(clampedRawVisibleCount);

    for (int i = 0; i < clampedRawVisibleCount; ++i) {
        const auto &item = allItems_.at(i);
        if (!item.cachedIsHiddenEvent)
            visibleItems.push_back(item);
    }

    return visibleItems;
}

void
MatrixTimelineModel::replaceVisibleItems(QVector<MatrixTimelineItem> items)
{
    if (items_ == items)
        return;

    const auto oldCount       = items_.size();
    const auto newCount       = items.size();
    const bool countDidChange = oldCount != newCount;

    auto prefix                = 0;
    const auto comparableCount = std::min(oldCount, newCount);
    while (prefix < comparableCount && items_.at(prefix) == items.at(prefix))
        ++prefix;

    auto oldSuffix = oldCount - 1;
    auto newSuffix = newCount - 1;
    while (oldSuffix >= prefix && newSuffix >= prefix &&
           items_.at(oldSuffix) == items.at(newSuffix)) {
        --oldSuffix;
        --newSuffix;
    }

    const auto oldChanged = oldSuffix - prefix + 1;
    const auto newChanged = newSuffix - prefix + 1;

    if (oldChanged == 0 && newChanged > 0) {
        beginInsertRows({}, prefix, prefix + newChanged - 1);
        items_ = std::move(items);
        endInsertRows();
    } else if (newChanged == 0 && oldChanged > 0) {
        beginRemoveRows({}, prefix, prefix + oldChanged - 1);
        items_ = std::move(items);
        endRemoveRows();
    } else if (oldChanged == newChanged) {
        items_ = std::move(items);
        if (oldChanged > 0)
            emit dataChanged(index(prefix), index(prefix + oldChanged - 1));
    } else {
        // Avoid beginResetModel — it resets the ListView's contentY
        // in BottomToTop mode, causing unwanted scroll-to-bottom.
        // Decompose into remove + insert instead.
        if (oldChanged > 0) {
            beginRemoveRows({}, prefix, prefix + oldChanged - 1);
            items_.erase(items_.begin() + prefix, items_.begin() + prefix + oldChanged);
            endRemoveRows();
        }
        if (newChanged > 0) {
            beginInsertRows({}, prefix, prefix + newChanged - 1);
            items_ = std::move(items);
            endInsertRows();
        } else {
            items_ = std::move(items);
        }
    }

    // Bubble grouping/section/avatar layout for row N depends on the next visible row (N + 1).
    // When hidden-event filtering inserts/removes rows, the unchanged boundary row just before the
    // diff can keep stale grouping state unless we nudge it explicitly.
    if (prefix > 0 && prefix - 1 < items_.size())
        emit dataChanged(index(prefix - 1), index(prefix - 1));

    if (countDidChange)
        emit countChanged();
}

void
MatrixTimelineModel::replaceItems(QVector<MatrixTimelineItem> items)
{
    applyOptimisticRedactions(items);

    // Filter out date_divider items — the bubble style's built-in section
    // headers handle date display, so keeping date dividers as model rows
    // just wastes delegate instantiations.
    items.erase(std::remove_if(items.begin(),
                               items.end(),
                               [](const MatrixTimelineItem &item) {
                                   return item.itemKind == QStringLiteral("date_divider");
                               }),
                items.end());

    for (auto &item : items)
        computeDerivedFields(item, roomId_);

    emitEffectsForPrependedItems(items);
    allItems_ = items;

    const auto initialVisibleWindow      = configuredInitialVisibleWindow();
    const auto uncappedVisibleCount      = static_cast<int>(allItems_.size());
    const auto cappedInitialVisibleCount = initialVisibleWindow
                                             ? std::min(uncappedVisibleCount, *initialVisibleWindow)
                                             : uncappedVisibleCount;

    auto targetVisibleCount = cappedInitialVisibleCount;
    if (initialVisibleWindow && revealedItemCount_ > 0)
        targetVisibleCount = std::max(revealedItemCount_, cappedInitialVisibleCount);

    revealedItemCount_ = std::clamp(targetVisibleCount, 0, static_cast<int>(allItems_.size()));
    replaceVisibleItems(visibleItemsForRawCount(revealedItemCount_));
}

void
MatrixTimelineModel::emitEffectsForPrependedItems(const QVector<MatrixTimelineItem> &nextItems)
{
    if (allItems_.isEmpty() || nextItems.isEmpty())
        return;

    const auto oldHeadKey = stableTimelineItemKey(allItems_.front());
    if (oldHeadKey.isEmpty())
        return;

    int prependCount = -1;
    for (int i = 0; i < nextItems.size(); ++i) {
        if (stableTimelineItemKey(nextItems.at(i)) == oldHeadKey) {
            prependCount = i;
            break;
        }
    }

    if (prependCount <= 0)
        return;

    QStringList effectNames;
    effectNames.reserve(prependCount);
    QSet<QString> seenEffects;
    for (int i = prependCount - 1; i >= 0; --i) {
        const auto &item = nextItems.at(i);
        for (const auto &effectName : item.specialEffectNames) {
            const auto trimmed = effectName.trimmed();
            if (trimmed.isEmpty() || seenEffects.contains(trimmed))
                continue;
            seenEffects.insert(trimmed);
            effectNames.push_back(trimmed);
        }
    }

    if (!effectNames.isEmpty())
        emit specialEffectsTriggered(effectNames);
}

void
MatrixTimelineModel::clear()
{
    optimisticRedactedEventIds_.clear();
    allItems_.clear();
    revealedItemCount_ = 0;
    replaceVisibleItems({});
}

} // namespace komai

#include "moc_MatrixTimelineModel.cpp"

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/rust/MatrixTimelineModel.h"

#include <algorithm>

namespace komai {

MatrixTimelineModel::MatrixTimelineModel(QObject *parent)
  : QAbstractListModel(parent)
{
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

    switch (role) {
    case ItemId:
        return item.itemId;
    case EventId:
        return item.eventId;
    case ThreadId:
        return item.threadId;
    case SenderId:
        return item.senderId;
    case SenderDisplayName:
        return item.senderDisplayName;
    case SenderAvatarUrl:
        return item.senderAvatarUrl;
    case Body:
        return item.body;
    case ReplyEventId:
        return item.replyEventId;
    case ReplySenderId:
        return item.replySenderId;
    case ReplySenderDisplayName:
        return item.replySenderDisplayName;
    case ReplyBody:
        return item.replyBody;
    case Reactions:
        return item.reactions;
    case ReactionsSummary:
        return item.reactionsSummary;
    case Timestamp:
        return static_cast<qulonglong>(item.timestamp);
    case ItemKind:
        return item.itemKind;
    case IsEdited:
        return item.isEdited;
    case IsOwn:
        return item.isOwn;
    case MediaUrl:
        return item.mediaUrl;
    case ThumbnailUrl:
        return item.thumbnailUrl;
    case FileName:
        return item.fileName;
    case MimeType:
        return item.mimeType;
    case MediaWidth:
        return static_cast<qulonglong>(item.mediaWidth);
    case MediaHeight:
        return static_cast<qulonglong>(item.mediaHeight);
    case MediaDurationMs:
        return static_cast<qulonglong>(item.mediaDurationMs);
    case MediaSizeBytes:
        return static_cast<qulonglong>(item.mediaSizeBytes);
    case MediaIsEncrypted:
        return item.mediaIsEncrypted;
    case ThumbnailIsEncrypted:
        return item.thumbnailIsEncrypted;
    default:
        return {};
    }
}

QHash<int, QByteArray>
MatrixTimelineModel::roleNames() const
{
    return {
      {ItemId, "itemId"},
      {EventId, "eventId"},
      {ThreadId, "threadId"},
      {SenderId, "senderId"},
      {SenderDisplayName, "senderDisplayName"},
      {SenderAvatarUrl, "senderAvatarUrl"},
      {Body, "body"},
      {ReplyEventId, "replyEventId"},
      {ReplySenderId, "replySenderId"},
      {ReplySenderDisplayName, "replySenderDisplayName"},
      {ReplyBody, "replyBody"},
      {Reactions, "reactions"},
      {ReactionsSummary, "reactionsSummary"},
      {Timestamp, "timestamp"},
      {ItemKind, "itemKind"},
      {IsEdited, "isEdited"},
      {IsOwn, "isOwn"},
      {MediaUrl, "mediaUrl"},
      {ThumbnailUrl, "thumbnailUrl"},
      {FileName, "fileName"},
      {MimeType, "mimeType"},
      {MediaWidth, "mediaWidth"},
      {MediaHeight, "mediaHeight"},
      {MediaDurationMs, "mediaDurationMs"},
      {MediaSizeBytes, "mediaSizeBytes"},
      {MediaIsEncrypted, "mediaIsEncrypted"},
      {ThumbnailIsEncrypted, "thumbnailIsEncrypted"},
    };
}

int
MatrixTimelineModel::rowForEventId(const QString &eventId) const
{
    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return -1;

    for (int row = 0; row < items_.size(); ++row) {
        const auto &item = items_.at(row);
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

std::optional<MatrixTimelineItem>
MatrixTimelineModel::itemByEventId(const QString &eventId) const
{
    const auto row = rowForEventId(eventId);
    if (row < 0 || row >= items_.size())
        return std::nullopt;

    return items_.at(row);
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
MatrixTimelineModel::replaceItems(QVector<MatrixTimelineItem> items)
{
    applyOptimisticRedactions(items);

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

    if (countDidChange)
        emit countChanged();
}

void
MatrixTimelineModel::clear()
{
    optimisticRedactedEventIds_.clear();
    replaceItems({});
}

} // namespace komai

#include "moc_MatrixTimelineModel.cpp"

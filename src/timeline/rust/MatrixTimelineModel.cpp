// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/rust/MatrixTimelineModel.h"

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
    case SenderId:
        return item.senderId;
    case SenderDisplayName:
        return item.senderDisplayName;
    case SenderAvatarUrl:
        return item.senderAvatarUrl;
    case Body:
        return item.body;
    case Timestamp:
        return static_cast<qulonglong>(item.timestamp);
    case ItemKind:
        return item.itemKind;
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
      {SenderId, "senderId"},
      {SenderDisplayName, "senderDisplayName"},
      {SenderAvatarUrl, "senderAvatarUrl"},
      {Body, "body"},
      {Timestamp, "timestamp"},
      {ItemKind, "itemKind"},
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

void
MatrixTimelineModel::replaceItems(QVector<MatrixTimelineItem> items)
{
    if (items_ == items)
        return;

    const bool countDidChange = items_.size() != items.size();

    beginResetModel();
    items_ = std::move(items);
    endResetModel();

    if (countDidChange)
        emit countChanged();
}

void
MatrixTimelineModel::clear()
{
    replaceItems({});
}

} // namespace komai

#include "moc_MatrixTimelineModel.cpp"

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "matrix/backend/MatrixBackendRuntimeService.h"

namespace komai {

class MatrixTimelineModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles
    {
        ItemId = Qt::UserRole,
        EventId,
        SenderId,
        SenderDisplayName,
        SenderAvatarUrl,
        Body,
        Timestamp,
        ItemKind,
        IsOwn,
        MediaUrl,
        ThumbnailUrl,
        FileName,
        MimeType,
        MediaWidth,
        MediaHeight,
        MediaDurationMs,
        MediaSizeBytes,
        MediaIsEncrypted,
        ThumbnailIsEncrypted,
    };

    explicit MatrixTimelineModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return items_.size(); }
    void replaceItems(QVector<MatrixTimelineItem> items);
    void clear();

signals:
    void countChanged();

private:
    QVector<MatrixTimelineItem> items_;
};

} // namespace komai

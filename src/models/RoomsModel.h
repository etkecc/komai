// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "matrix/backend/MatrixBackendRuntimeService.h"

#include <QAbstractListModel>
#include <QHash>
#include <QString>

class RoomsModel final : public QAbstractListModel
{
public:
    enum Roles
    {
        AvatarUrl = Qt::UserRole,
        RoomAlias,
        RoomID,
        RawRoomID,
        RoomName,
        IsTombstoned,
        IsSpace,
    };

    explicit RoomsModel(const QHash<QString, komai::MatrixRoomSummary> &matrixRooms,
                        QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        (void)parent;
        return static_cast<int>(rooms_.size());
    }
    QVariant data(const QModelIndex &index, int role) const override;

private:
    struct RoomEntry
    {
        QString roomId;
        QString displayName;
        QString avatarUrl;
        bool isSpace = false;
    };

    QVector<RoomEntry> rooms_;
};

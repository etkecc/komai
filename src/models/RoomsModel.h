// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "matrix/MatrixStateTypes.h"

#include <QAbstractListModel>
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

    RoomsModel(bool showOnlyRoomWithAliases = false,
               bool forwardMode             = false,
               QObject *parent              = nullptr);
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        (void)parent;
        return (int)rooms.size();
    }
    QVariant data(const QModelIndex &index, int role) const override;

    //! Number of "preferred" rooms (not low-priority, with recent own activity).
    //! Only meaningful when forwardMode is true; otherwise equals rooms.size().
    int preferredCount() const { return preferredCount_; }

private:
    std::vector<RoomNameAlias> rooms;
    bool showOnlyRoomWithAliases_;
    int preferredCount_ = 0;
};

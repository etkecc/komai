// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QSharedPointer>

#include "imagepacks/SingleImagePackModel.h"

class ImagePackListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    Q_PROPERTY(bool containsAccountPack READ containsAccountPack NOTIFY containsAccountPackChanged)
    Q_PROPERTY(int packCount READ packCount NOTIFY packCountChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
public:
    enum Roles
    {
        DisplayName = Qt::UserRole,
        AvatarUrl,
        FromAccountData,
        FromCurrentRoom,
        FromSpace,
        StateKey,
        RoomId,
    };

    ImagePackListModel(const std::string &roomId, QObject *parent = nullptr);
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    Q_INVOKABLE SingleImagePackModel *packAt(int row);
    Q_INVOKABLE SingleImagePackModel *newPack(bool inRoom);
    Q_INVOKABLE void refresh();

    bool containsAccountPack() const;
    int packCount() const { return static_cast<int>(packs.size()); }
    int revision() const { return revision_; }

signals:
    void containsAccountPackChanged();
    void packCountChanged();
    void revisionChanged();

private:
    void loadFromRuntime();

    std::string room_id;
    int revision_ = 0;

    std::vector<QSharedPointer<SingleImagePackModel>> packs;
};

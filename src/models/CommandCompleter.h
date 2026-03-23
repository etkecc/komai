// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QString>

#include <cstddef>

class CommandCompleter final : public QAbstractListModel
{
public:
    enum Roles
    {
        Name = Qt::UserRole,
        Description,
    };

    CommandCompleter(QObject *parent = nullptr);
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        (void)parent;
        return static_cast<int>(commandCount_);
    }
    QVariant data(const QModelIndex &index, int role) const override;

private:
    std::size_t commandCount_ = 0;
};

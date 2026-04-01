// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>

class CombinedImagePackModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles
    {
        Url = Qt::UserRole,
        ShortCode,
        Body,
        PackName,
        Unicode,
    };

    CombinedImagePackModel(const std::string &roomId,
                           bool includeUnicode = true,
                           QObject *parent     = nullptr);
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    void loadFromRuntime();

    std::string room_id;
    bool includeUnicode_;

    struct ImageDesc
    {
        QString shortcode;
        QString packname;
        QString url;
        QString body;
    };

    std::vector<ImageDesc> images;
};

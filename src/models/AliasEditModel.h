// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QVector>

#include <vector>

namespace komai {
struct MatrixRoomAliases;
}

class AliasEditingModel final : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Please use editAliases to create the models")

    Q_PROPERTY(bool canAdvertize READ canAdvertize NOTIFY canAdvertizeChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool committing READ committing NOTIFY committingChanged)

public:
    enum Roles
    {
        Name,
        IsPublished,
        IsCanonical,
        IsAdvertized,
    };

    explicit AliasEditingModel(const std::string &room_id_, QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &) const override { return static_cast<int>(aliases.size()); }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool canAdvertize() const { return canSendStateEvent; }
    bool loading() const { return loading_; }
    bool committing() const { return committing_; }

    Q_INVOKABLE bool deleteAlias(int row);
    Q_INVOKABLE void addAlias(QString newAlias);
    Q_INVOKABLE void makeCanonical(int row);
    Q_INVOKABLE void togglePublish(int row);
    Q_INVOKABLE void toggleAdvertize(int row);
    Q_INVOKABLE void commit();

signals:
    void canAdvertizeChanged();
    void loadingChanged();
    void committingChanged();

private:
    void loadAsync();
    void applyLoadedState(const komai::MatrixRoomAliases &aliases, bool canSendStateEvent);
    [[nodiscard]] komai::MatrixRoomAliases desiredAliases() const;

    struct CanonicalAliasDraft
    {
        std::string alias;
        std::vector<std::string> alt_aliases;
    };

    struct Entry
    {
        ~Entry() = default;

        std::string alias;
        bool canonical  = false;
        bool advertized = false;
        bool published  = false;
    };

    std::string room_id;
    QVector<Entry> aliases;
    CanonicalAliasDraft aliasEvent;
    bool canSendStateEvent = false;
    bool loading_          = false;
    bool committing_       = false;
};

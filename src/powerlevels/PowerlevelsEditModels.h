// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QSortFilterProxyModel>

#include "matrix/MatrixRoomPowerLevels.h"

namespace komai::powerlevels {
inline constexpr auto CreatorPowerLevel = komai::matrix::RuntimeCreatorPowerLevel;
}

class PowerlevelsTypeListModel final : public QAbstractListModel
{
    Q_OBJECT

signals:
    void adminLevelChanged();
    void moderatorLevelChanged();

public:
    enum Roles
    {
        DisplayName,
        Powerlevel,
        IsType,
        Moveable,
        Removeable,
    };

    explicit PowerlevelsTypeListModel(const komai::MatrixRoomPowerLevels &powerLevels,
                                      QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &) const override { return static_cast<int>(types.size()); }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    Q_INVOKABLE bool remove(int row);
    Q_INVOKABLE bool move(int from, int to);
    Q_INVOKABLE void add(int index, QString type);
    void addRole(int64_t role);

    bool moveRows(const QModelIndex &sourceParent,
                  int sourceRow,
                  int count,
                  const QModelIndex &destinationParent,
                  int destinationChild) override;

    void setPowerLevels(const komai::MatrixRoomPowerLevels &powerLevels);

    QVector<komai::MatrixPowerLevelEntry> toEvents() const;
    qlonglong kick() const;
    qlonglong invite() const;
    qlonglong ban() const;
    qlonglong redact() const;
    qlonglong eventsDefault() const;
    qlonglong stateDefault() const;

    struct Entry
    {
        ~Entry() = default;

        QString type;
        qlonglong pl = 0;
    };

    QVector<Entry> types;
    komai::MatrixRoomPowerLevels powerLevels_;
};

class PowerlevelsUserListModel final : public QAbstractListModel
{
    Q_OBJECT

signals:
    void defaultUserLevelChanged();

public:
    enum Roles
    {
        Mxid,
        DisplayName,
        AvatarUrl,
        Powerlevel,
        IsUser,
        Moveable,
        Removeable,
        IsCreator,
    };

    explicit PowerlevelsUserListModel(const komai::MatrixRoomPowerLevels &powerLevels,
                                      QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &) const override { return static_cast<int>(users.size()); }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    Q_INVOKABLE bool remove(int row);
    Q_INVOKABLE bool move(int from, int to);
    Q_INVOKABLE void add(int index, QString user);
    void addRole(int64_t role);

    bool moveRows(const QModelIndex &sourceParent,
                  int sourceRow,
                  int count,
                  const QModelIndex &destinationParent,
                  int destinationChild) override;

    void setPowerLevels(const komai::MatrixRoomPowerLevels &powerLevels);

    QVector<komai::MatrixPowerLevelEntry> toUsers() const;
    qlonglong usersDefault() const;

    struct Entry
    {
        ~Entry() = default;

        QString mxid;
        qlonglong pl = 0;
    };

    QVector<Entry> users;
    komai::MatrixRoomPowerLevels powerLevels_;
};

class PowerlevelsSpacesListModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool applyToChildren READ applyToChildren WRITE setApplyToChildren NOTIFY
                 applyToChildrenChanged)
    Q_PROPERTY(bool overwriteDiverged READ overwriteDiverged WRITE setOverwriteDiverged NOTIFY
                 overwriteDivergedChanged)

signals:
    void applyToChildrenChanged();
    void overwriteDivergedChanged();

public:
    enum Roles
    {
        DisplayName,
        AvatarUrl,
        IsSpace,
        IsEditable,
        IsDifferentFromBase,
        IsAlreadyUpToDate,
        ApplyPermissions,
    };

    explicit PowerlevelsSpacesListModel(const QString &roomId,
                                        const komai::MatrixRoomPowerLevels &powerLevels,
                                        QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &) const override { return static_cast<int>(spaces.size()); }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool
    setData(const QModelIndex &index, const QVariant &value, int role = Qt::DisplayRole) override;

    bool applyToChildren() const { return applyToChildren_; }
    bool overwriteDiverged() const { return overwriteDiverged_; }

    void setApplyToChildren(bool val)
    {
        applyToChildren_ = val;
        emit applyToChildrenChanged();
        updateToDefaults();
    }
    void setOverwriteDiverged(bool val)
    {
        overwriteDiverged_ = val;
        emit overwriteDivergedChanged();
        updateToDefaults();
    }

    void updateToDefaults();

    Q_INVOKABLE void commit();

    struct Entry
    {
        ~Entry() = default;

        QString roomid;
        komai::MatrixRoomPowerLevels pl;
        bool apply = false;
    };

    QString room_id;
    QVector<Entry> spaces;
    komai::MatrixRoomPowerLevels oldPowerLevels_, newPowerlevels_;

    bool applyToChildren_ = true, overwriteDiverged_ = false;
};

class PowerlevelEditingModels final : public QObject
{
    Q_OBJECT

    QML_ELEMENT
    QML_UNCREATABLE("Please use editPowerlevels to create the models")

    Q_PROPERTY(PowerlevelsUserListModel *users READ users CONSTANT)
    Q_PROPERTY(PowerlevelsTypeListModel *types READ types CONSTANT)
    Q_PROPERTY(PowerlevelsSpacesListModel *spaces READ spaces CONSTANT)
    Q_PROPERTY(qlonglong creatorLevel READ creatorLevel CONSTANT)
    Q_PROPERTY(qlonglong adminLevel READ adminLevel NOTIFY adminLevelChanged)
    Q_PROPERTY(qlonglong moderatorLevel READ moderatorLevel NOTIFY moderatorLevelChanged)
    Q_PROPERTY(qlonglong defaultUserLevel READ defaultUserLevel NOTIFY defaultUserLevelChanged)
    Q_PROPERTY(bool isSpace READ isSpace CONSTANT)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool committing READ committing NOTIFY committingChanged)

signals:
    void adminLevelChanged();
    void moderatorLevelChanged();
    void defaultUserLevelChanged();
    void loadedChanged();
    void committingChanged();

private:
    komai::MatrixRoomPowerLevels calculateNewPowerlevel() const;
    void setPowerLevels(komai::MatrixRoomPowerLevels powerLevels);

public:
    explicit PowerlevelEditingModels(QString room_id, QObject *parent = nullptr);

    PowerlevelsUserListModel *users() { return &users_; }
    PowerlevelsTypeListModel *types() { return &types_; }
    PowerlevelsSpacesListModel *spaces() { return &spaces_; }
    qlonglong creatorLevel() const { return komai::powerlevels::CreatorPowerLevel; }
    qlonglong adminLevel() const
    {
        for (const auto &entry : powerLevels_.events) {
            if (entry.key == "m.room.power_levels")
                return entry.level;
        }
        return powerLevels_.stateDefault;
    }
    qlonglong moderatorLevel() const { return powerLevels_.redact; }
    qlonglong defaultUserLevel() const { return powerLevels_.usersDefault; }
    bool isSpace() const;
    bool loaded() const { return loaded_; }
    bool committing() const { return committing_; }

    Q_INVOKABLE void commit();
    Q_INVOKABLE void updateSpacesModel();
    Q_INVOKABLE void addRole(int pl);

    komai::MatrixRoomPowerLevels powerLevels_;
    PowerlevelsTypeListModel types_;
    PowerlevelsUserListModel users_;
    PowerlevelsSpacesListModel spaces_;
    QString roomId_;
    bool loaded_     = false;
    bool committing_ = false;
};

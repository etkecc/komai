// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include <QObject>
#include <QQmlEngine>

class RoomSummary final : public QObject
{
    Q_OBJECT

    QML_ELEMENT
    QML_UNCREATABLE("Please use joinRoom to create a room summary.")

    Q_PROPERTY(QString reason READ reason WRITE setReason NOTIFY reasonChanged)

    Q_PROPERTY(QString roomid READ roomid NOTIFY loaded)
    Q_PROPERTY(QString roomName READ roomName NOTIFY loaded)
    Q_PROPERTY(QString roomTopic READ roomTopic NOTIFY loaded)
    Q_PROPERTY(QString roomAvatarUrl READ roomAvatarUrl NOTIFY loaded)
    Q_PROPERTY(bool isInvite READ isInvite NOTIFY loaded)
    Q_PROPERTY(bool isSpace READ isSpace NOTIFY loaded)
    Q_PROPERTY(bool isKnockOnly READ isKnockOnly NOTIFY loaded)
    Q_PROPERTY(bool isLoaded READ isLoaded NOTIFY loaded)
    Q_PROPERTY(int memberCount READ memberCount NOTIFY loaded)

public:
    explicit RoomSummary(std::string roomIdOrAlias_,
                         std::vector<std::string> vias_,
                         QString reason_,
                         QObject *p = nullptr);

    void setReason(const QString &r)
    {
        reason_ = r;
        emit reasonChanged();
    }
    QString reason() const { return reason_; }

    QString roomid() const { return room ? room->roomId : QString::fromStdString(roomIdOrAlias); }
    QString roomName() const;
    QString roomTopic() const;
    QString roomAvatarUrl() const { return room ? room->avatarUrl : ""; }
    bool isInvite() const { return room && room->isInvite; }
    bool isSpace() const { return room && room->isSpace; }
    int memberCount() const { return room ? room->memberCount : 0; }
    bool isKnockOnly() const { return room && room->isKnockOnly; }

    bool isLoaded() const { return room.has_value() || loaded_; }

    Q_INVOKABLE void join();
    Q_INVOKABLE void promptJoin();

signals:
    void loaded();
    void reasonChanged();

private:
    struct LoadedRoomSummary
    {
        QString roomId;
        QString name;
        QString topic;
        QString avatarUrl;
        int memberCount  = 0;
        bool isInvite    = false;
        bool isSpace     = false;
        bool isKnockOnly = false;
    };

    std::string roomIdOrAlias;
    std::vector<std::string> vias;
    std::optional<LoadedRoomSummary> room;
    QString reason_;
    bool loaded_ = false;
};

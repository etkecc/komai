// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>

#include <cstdint>

class EventExpiry : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString roomid READ roomid WRITE setRoomid NOTIFY roomidChanged REQUIRED)
    Q_PROPERTY(int expireEventsAfterDays READ expireEventsAfterDays WRITE setExpireEventsAfterDays
                 NOTIFY expireEventsAfterDaysChanged)
    Q_PROPERTY(bool expireStateEvents READ expireStateEvents WRITE setExpireStateEvents NOTIFY
                 expireStateEventsChanged)
    Q_PROPERTY(int expireEventsAfterCount READ expireEventsAfterCount WRITE
                 setExpireEventsAfterCount NOTIFY expireEventsAfterCountChanged)
    Q_PROPERTY(int protectLatestEvents READ protectLatestEvents WRITE setProtectLatestEvents NOTIFY
                 protectLatestEventsChanged)
public:
    explicit EventExpiry(QObject *p = nullptr)
      : QObject(p)
    {
    }

    Q_INVOKABLE void save();

    [[nodiscard]] QString roomid() const { return roomid_; }
    void setRoomid(const QString &r)
    {
        roomid_ = r;
        emit roomidChanged();

        load();
    }

    [[nodiscard]] int expireEventsAfterDays() const;
    [[nodiscard]] int expireEventsAfterCount() const;
    [[nodiscard]] int protectLatestEvents() const;
    [[nodiscard]] bool expireStateEvents() const;
    void setExpireEventsAfterDays(int);
    void setExpireEventsAfterCount(int);
    void setProtectLatestEvents(int);
    void setExpireStateEvents(bool);

signals:
    void roomidChanged();

    void expireEventsAfterDaysChanged();
    void expireEventsAfterCountChanged();
    void protectLatestEventsChanged();
    void expireStateEventsChanged();

private:
    struct EventExpiryDraft
    {
        std::uint64_t expire_after_ms  = 0;
        std::uint64_t keep_only_latest = 0;
        std::uint64_t protect_latest   = 0;
        bool exclude_state_events      = false;
    };

    QString roomid_;
    EventExpiryDraft event = {};

    void load();
};

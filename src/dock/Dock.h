// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QObject>
#if defined(KOMAI_DBUS_SYS)
#include <QDBusServiceWatcher>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusInterface>
#endif

class Dock final : public QObject
{
    Q_OBJECT
public:
    Dock(QObject *parent = nullptr);
public slots:
    void setAttentionCount(const int count);

private:
#if defined(KOMAI_DBUS_SYS)
    void unitySetNotificationCount(const int count);
    QDBusServiceWatcher *unityServiceWatcher = nullptr;
    bool unityServiceAvailable               = false;
#endif
};

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusMessage>
#include <QObject>

#include "Api.h"

#include <memory>

namespace nhlog {
class Logger;
}

struct DbusBackendLoggers
{
    std::shared_ptr<nhlog::Logger> ui;
};

void
setLoggers(DbusBackendLoggers loggers);
const DbusBackendLoggers &
activeLoggers();

class RoomlistModel;

/// Host object registered at "/" on the session bus.
/// D-Bus interfaces are implemented as QDBusAbstractAdaptor children.
class DbusHost final : public QObject
{
    Q_OBJECT

public:
    explicit DbusHost(RoomlistModel *roomlist);
    RoomlistModel *roomlist() const { return m_roomlist; }

private:
    RoomlistModel *m_roomlist;
};

// ---------------------------------------------------------------------------
// cc.etke.komai.App
// ---------------------------------------------------------------------------

class DbusAppInterface final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "cc.etke.komai.App")

public:
    explicit DbusAppInterface(DbusHost *parent);

public slots:
    QString apiVersion() const;
    QString appVersion() const;
};

// ---------------------------------------------------------------------------
// cc.etke.komai.Rooms
// ---------------------------------------------------------------------------

class DbusRoomsInterface final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "cc.etke.komai.Rooms")

public:
    explicit DbusRoomsInterface(DbusHost *parent);

public slots:
    QVector<komai::dbus::RoomInfoItem> list() const;
    QString timeline(const QString &roomIdOrAlias,
                     int limit,
                     const QString &beforeEventId,
                     bool includeUnsignedFields,
                     const QString &fetchMode,
                     const QDBusMessage &message) const;
    void join(const QString &roomIdOrAlias) const;
    void newDirectChat(const QString &userId) const;
    QString send(const QString &roomIdOrAlias,
                 const QString &body,
                 const QString &msgtype,
                 const QString &format,
                 const QDBusMessage &message) const;
    QString sendImageFile(const QString &roomIdOrAlias,
                          const QString &filePath,
                          const QString &body,
                          const QDBusMessage &message) const;
    QString sendImage(const QString &roomIdOrAlias,
                      const QString &mxcUri,
                      const QString &body,
                      const QString &filename,
                      const QDBusMessage &message) const;
};

// ---------------------------------------------------------------------------
// cc.etke.komai.User
// ---------------------------------------------------------------------------

class DbusUserInterface final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "cc.etke.komai.User")

public:
    explicit DbusUserInterface(DbusHost *parent);

public slots:
    QString userId() const;
    QString homeserverUrl() const;
    QString deviceId() const;
    QString statusMessage() const;
    void setStatusMessage(const QString &message);
};

// ---------------------------------------------------------------------------
// cc.etke.komai.Settings.UI
// ---------------------------------------------------------------------------

class DbusSettingsUiInterface final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "cc.etke.komai.Settings.UI")

public:
    explicit DbusSettingsUiInterface(DbusHost *parent);

public slots:
    QString theme() const;
    void setTheme(const QString &theme);
};

// ---------------------------------------------------------------------------
// cc.etke.komai.Media
// ---------------------------------------------------------------------------

class DbusMediaInterface final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "cc.etke.komai.Media")

public:
    explicit DbusMediaInterface(DbusHost *parent);

public slots:
    QImage fetch(const QString &mxcUri, const QDBusMessage &message) const;
    QString upload(const QString &filePath,
                   const QString &filename,
                   const QString &contentType,
                   const QDBusMessage &message) const;
};

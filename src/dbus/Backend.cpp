// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Backend.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "ipc/SharedLogic.h"
#include "settings/SettingKeys.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"
#include <spdlog/logger.h>

#include <QDBusConnection>
#include <spdlog/sinks/null_sink.h>

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

namespace {

static const auto DbusArgTypePrefix = QStringLiteral("string:");

QString
stripDbusTypePrefix(QString arg)
{
    auto normalized = arg.trimmed();
    if (!normalized.startsWith(DbusArgTypePrefix))
        return normalized;
    normalized = normalized.mid(DbusArgTypePrefix.size());
    if ((normalized.startsWith(u'\"') && normalized.endsWith(u'\"')) ||
        (normalized.startsWith(u'\'') && normalized.endsWith(u'\'')))
        return normalized.mid(1, normalized.size() - 2);
    return normalized;
}

bool
dbusReadAccessEnabled()
{
    const auto settings = UserSettings::instance();
    return settings && settings->integrationsDbusApiAccess() >= IntegrationsDbusAccessReadOnly;
}

bool
dbusWriteAccessEnabled()
{
    const auto settings = UserSettings::instance();
    return settings && settings->integrationsDbusApiAccess() >= IntegrationsDbusAccessReadWrite;
}

std::shared_ptr<spdlog::logger>
nullLogger(std::string_view name)
{
    static auto sink         = std::make_shared<spdlog::sinks::null_sink_mt>();
    static auto dbusUiLogger = std::make_shared<spdlog::logger>(std::string("dbus-ui"), sink);

    if (name == "dbus-ui")
        return dbusUiLogger;
    return dbusUiLogger;
}

DbusBackendLoggers
defaultLoggers()
{
    return {
      .ui = nullLogger("dbus-ui"),
    };
}

DbusBackendLoggers &
backendCurrentLoggers()
{
    static DbusBackendLoggers loggers = defaultLoggers();
    return loggers;
}

DbusBackendLoggers
setMissingLoggers(DbusBackendLoggers loggers)
{
    const auto &defaults = defaultLoggers();
    if (!loggers.ui)
        loggers.ui = defaults.ui;
    return loggers;
}
} // namespace

void
setLoggers(DbusBackendLoggers loggers)
{
    backendCurrentLoggers() = setMissingLoggers(std::move(loggers));
}

const DbusBackendLoggers &
activeLoggers()
{
    return backendCurrentLoggers();
}

// ---------------------------------------------------------------------------
// DbusHost
// ---------------------------------------------------------------------------

DbusHost::DbusHost(RoomlistModel *roomlist)
  : QObject{roomlist}
  , m_roomlist{roomlist}
{
    // Adaptors attach themselves to this host via their constructors.
    new DbusAppInterface{this};
    new DbusRoomsInterface{this};
    new DbusUserInterface{this};
    new DbusSettingsUiInterface{this};
    new DbusMediaInterface{this};
}

// ---------------------------------------------------------------------------
// cc.etke.komai.App
// ---------------------------------------------------------------------------

DbusAppInterface::DbusAppInterface(DbusHost *parent)
  : QDBusAbstractAdaptor{parent}
{
}

QString
DbusAppInterface::apiVersion() const
{
    return komai::ipc::apiVersion();
}

QString
DbusAppInterface::appVersion() const
{
    return komai::ipc::appVersion();
}

// ---------------------------------------------------------------------------
// cc.etke.komai.Rooms
// ---------------------------------------------------------------------------

DbusRoomsInterface::DbusRoomsInterface(DbusHost *parent)
  : QDBusAbstractAdaptor{parent}
{
}

QVector<komai::dbus::RoomInfoItem>
DbusRoomsInterface::list() const
{
    if (!dbusReadAccessEnabled())
        return {};

    activeLoggers().ui->debug("Rooms requested over D-Bus.");

    const auto rooms = komai::ipc::roomList();

    QVector<komai::dbus::RoomInfoItem> model;
    model.reserve(rooms.size());
    for (const auto &r : rooms)
        model.push_back(
          komai::dbus::RoomInfoItem{r.roomId, r.alias, r.name, r.avatarUrl, r.unreadNotifications});

    activeLoggers().ui->debug("Sending {} rooms over D-Bus...", model.size());
    return model;
}

void
DbusRoomsInterface::activate(const QString &roomIdOrAlias) const
{
    if (!dbusWriteAccessEnabled())
        return;

    const auto normalized = stripDbusTypePrefix(roomIdOrAlias);
    if (normalized.isEmpty())
        return;

    komai::ipc::activateRoom(normalized);
}

void
DbusRoomsInterface::join(const QString &roomIdOrAlias) const
{
    if (!dbusWriteAccessEnabled())
        return;

    const auto normalized = stripDbusTypePrefix(roomIdOrAlias);
    if (normalized.isEmpty())
        return;

    komai::ipc::joinRoom(normalized);
}

void
DbusRoomsInterface::newDirectChat(const QString &userId) const
{
    if (!dbusWriteAccessEnabled())
        return;

    const auto normalized = stripDbusTypePrefix(userId);
    if (normalized.isEmpty())
        return;

    komai::ipc::newDirectChat(normalized);
}

QString
DbusRoomsInterface::send(const QString &roomIdOrAlias,
                         const QString &body,
                         const QString &msgtype,
                         const QString &format,
                         const QDBusMessage &message) const
{
    if (!dbusWriteAccessEnabled())
        return {};

    message.setDelayedReply(true);
    komai::ipc::sendMessage(stripDbusTypePrefix(roomIdOrAlias),
                            stripDbusTypePrefix(body),
                            stripDbusTypePrefix(msgtype),
                            stripDbusTypePrefix(format),
                            [message](const QString &eventId, const QString &error) {
                                auto reply = message.createReply();
                                reply << (error.isEmpty() ? eventId : QString{});
                                QDBusConnection::sessionBus().send(reply);
                            });
    return {};
}

QString
DbusRoomsInterface::sendImageFile(const QString &roomIdOrAlias,
                                  const QString &filePath,
                                  const QString &body,
                                  const QDBusMessage &message) const
{
    if (!dbusWriteAccessEnabled())
        return {};

    message.setDelayedReply(true);
    komai::ipc::sendImageFromFile(stripDbusTypePrefix(roomIdOrAlias),
                                  stripDbusTypePrefix(filePath),
                                  stripDbusTypePrefix(body),
                                  [message](const QString &eventId, const QString &error) {
                                      auto reply = message.createReply();
                                      reply << (error.isEmpty() ? eventId : QString{});
                                      QDBusConnection::sessionBus().send(reply);
                                  });
    return {};
}

QString
DbusRoomsInterface::sendImage(const QString &roomIdOrAlias,
                              const QString &mxcUri,
                              const QString &body,
                              const QString &filename,
                              const QDBusMessage &message) const
{
    if (!dbusWriteAccessEnabled())
        return {};

    message.setDelayedReply(true);
    komai::ipc::sendImage(stripDbusTypePrefix(roomIdOrAlias),
                          stripDbusTypePrefix(mxcUri),
                          stripDbusTypePrefix(body),
                          stripDbusTypePrefix(filename),
                          {},
                          [message](const QString &eventId, const QString &error) {
                              auto reply = message.createReply();
                              reply << (error.isEmpty() ? eventId : QString{});
                              QDBusConnection::sessionBus().send(reply);
                          });
    return {};
}

// ---------------------------------------------------------------------------
// cc.etke.komai.User
// ---------------------------------------------------------------------------

DbusUserInterface::DbusUserInterface(DbusHost *parent)
  : QDBusAbstractAdaptor{parent}
{
}

QString
DbusUserInterface::userId() const
{
    if (!dbusReadAccessEnabled())
        return {};

    return komai::ipc::userId();
}

QString
DbusUserInterface::homeserverUrl() const
{
    if (!dbusReadAccessEnabled())
        return {};

    return komai::ipc::homeserverUrl();
}

QString
DbusUserInterface::deviceId() const
{
    if (!dbusReadAccessEnabled())
        return {};

    return komai::ipc::deviceId();
}

QString
DbusUserInterface::statusMessage() const
{
    if (!dbusReadAccessEnabled())
        return {};

    return komai::ipc::statusMessage();
}

void
DbusUserInterface::setStatusMessage(const QString &message)
{
    if (!dbusWriteAccessEnabled())
        return;

    komai::ipc::setStatusMessage(stripDbusTypePrefix(message));
}

// ---------------------------------------------------------------------------
// cc.etke.komai.Settings.UI
// ---------------------------------------------------------------------------

DbusSettingsUiInterface::DbusSettingsUiInterface(DbusHost *parent)
  : QDBusAbstractAdaptor{parent}
{
}

QString
DbusSettingsUiInterface::theme() const
{
    if (!dbusReadAccessEnabled())
        return {};

    return komai::ipc::uiTheme();
}

void
DbusSettingsUiInterface::setTheme(const QString &theme)
{
    if (!dbusWriteAccessEnabled()) {
        activeLoggers().ui->warn(
          "Ignoring D-Bus setTheme call: write access is disabled (theme: '{}')",
          theme.toStdString());
        return;
    }

    const auto normalizedTheme = stripDbusTypePrefix(theme);
    const auto oldTheme        = komai::ipc::uiTheme();
    komai::ipc::setUiTheme(normalizedTheme);
    const auto newTheme = komai::ipc::uiTheme();
    if (newTheme == oldTheme) {
        activeLoggers().ui->warn("Ignoring D-Bus setTheme call: theme '{}' is not applicable",
                                 normalizedTheme.toStdString());
    } else {
        activeLoggers().ui->info(
          "Applied D-Bus theme: {} -> {}", oldTheme.toStdString(), newTheme.toStdString());
    }
}

// ---------------------------------------------------------------------------
// cc.etke.komai.Media
// ---------------------------------------------------------------------------

DbusMediaInterface::DbusMediaInterface(DbusHost *parent)
  : QDBusAbstractAdaptor{parent}
{
}

QImage
DbusMediaInterface::fetch(const QString &mxcUri, const QDBusMessage &message) const
{
    if (!dbusReadAccessEnabled())
        return {};

    const auto normalizedUri = stripDbusTypePrefix(mxcUri);

    message.setDelayedReply(true);
    activeLoggers().ui->debug("Media fetch requested over D-Bus.");
    komai::ipc::mediaFetch(normalizedUri, [message](const QImage &image) {
        auto reply = message.createReply();
        reply << QVariant::fromValue(image);
        QDBusConnection::sessionBus().send(reply);
    });
    return {};
}

QString
DbusMediaInterface::upload(const QString &filePath,
                           const QString &filename,
                           const QString &contentType,
                           const QDBusMessage &message) const
{
    if (!dbusWriteAccessEnabled())
        return {};

    message.setDelayedReply(true);
    komai::ipc::mediaUpload(
      stripDbusTypePrefix(filePath),
      stripDbusTypePrefix(filename),
      stripDbusTypePrefix(contentType),
      [message](const komai::ipc::UploadResult &result, const QString &error) {
          auto reply = message.createReply();
          reply << (error.isEmpty() ? result.mxcUri : QString{});
          QDBusConnection::sessionBus().send(reply);
      });
    return {};
}

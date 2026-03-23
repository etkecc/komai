// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Backend.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "config/komai.h"
#include "matrix/MatrixClient.h"
#include "providers/MxcImageProvider.h"
#include "settings/SettingKeys.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineModel.h"
#include "ui/MainWindow.h"
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
    return komai::dbus::dbusApiVersion.toString();
}

QString
DbusAppInterface::appVersion() const
{
    return komai::version;
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

    auto *rl = static_cast<DbusHost *>(parent())->roomlist();
    activeLoggers().ui->debug("Rooms requested over D-Bus.");

    QVector<komai::dbus::RoomInfoItem> model;
    model.reserve((int)rl->roomids.size());

    for (const auto &roomId : rl->roomids) {
        if (rl->invites.contains(roomId) || rl->previewedRooms.contains(roomId))
            continue;

        const auto aliases =
          cache::getStateEvent<mtx::events::state::CanonicalAlias>(roomId.toStdString());
        QString alias;
        if (aliases.has_value()) {
            const auto &val = aliases.value().content;
            if (!val.alias.empty())
                alias = QString::fromStdString(val.alias);
            else if (val.alt_aliases.size() > 0)
                alias = QString::fromStdString(val.alt_aliases.front());
        }

        QString roomName;
        QString roomAvatar;
        int notificationCount = 0;

        if (rl->models.contains(roomId)) {
            const auto &room = rl->models.value(roomId);
            if (room.isNull())
                continue;

            roomName          = room->plainRoomName();
            roomAvatar        = room->roomAvatarUrl();
            notificationCount = room->notificationCount();
        } else if (rl->cachedJoinedRooms_.contains(roomId)) {
            const auto roomInfo = rl->cachedJoinedRooms_.value(roomId);
            roomName            = QString::fromStdString(roomInfo.name);
            roomAvatar          = QString::fromStdString(roomInfo.avatar_url);
            notificationCount   = static_cast<int>(roomInfo.notification_count);
        } else {
            continue;
        }

        if (roomAvatar.isEmpty())
            roomAvatar = cache::roomAvatarUrl(roomId.toStdString());

        model.push_back(
          komai::dbus::RoomInfoItem{roomId, alias, roomName, roomAvatar, notificationCount});
    }

    activeLoggers().ui->debug("Sending {} rooms over D-Bus...", model.size());
    return model;
}

void
DbusRoomsInterface::activate(const QString &roomIdOrAlias) const
{
    if (!dbusWriteAccessEnabled())
        return;

    bringWindowToTop();
    static_cast<DbusHost *>(parent())->roomlist()->setCurrentRoom(
      stripDbusTypePrefix(roomIdOrAlias));
}

void
DbusRoomsInterface::join(const QString &roomIdOrAlias) const
{
    if (!dbusWriteAccessEnabled())
        return;

    bringWindowToTop();
    ChatPage::instance()->joinRoom(stripDbusTypePrefix(roomIdOrAlias));
}

void
DbusRoomsInterface::newDirectChat(const QString &userId) const
{
    if (!dbusWriteAccessEnabled())
        return;

    bringWindowToTop();
    ChatPage::instance()->startChat(stripDbusTypePrefix(userId));
}

void
DbusRoomsInterface::bringWindowToTop() const
{
    MainWindow::instance()->show();
    MainWindow::instance()->raise();
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

    return QString::fromStdString(http::client()->user_id().to_string());
}

QString
DbusUserInterface::homeserverUrl() const
{
    if (!dbusReadAccessEnabled())
        return {};

    return QString::fromStdString(http::client()->server_url());
}

QString
DbusUserInterface::deviceId() const
{
    if (!dbusReadAccessEnabled())
        return {};

    return QString::fromStdString(http::client()->device_id());
}

QString
DbusUserInterface::statusMessage() const
{
    if (!dbusReadAccessEnabled())
        return {};

    return ChatPage::instance()->status();
}

void
DbusUserInterface::setStatusMessage(const QString &message)
{
    if (!dbusWriteAccessEnabled())
        return;

    ChatPage::instance()->setStatus(stripDbusTypePrefix(message));
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

    const auto settings = UserSettings::instance();
    if (!settings)
        return {};

    return settings->uiThemeSlug();
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

    const auto settings = UserSettings::instance();
    if (!settings)
        return;

    const auto normalizedTheme = stripDbusTypePrefix(theme);
    const auto oldTheme        = settings->uiThemeSlug();
    settings->setUiThemeSlug(normalizedTheme);
    if (settings->uiThemeSlug() == oldTheme) {
        activeLoggers().ui->warn("Ignoring D-Bus setTheme call: theme '{}' is not applicable",
                                 normalizedTheme.toStdString());
    } else {
        activeLoggers().ui->info("Applied D-Bus theme: {} -> {}",
                                 oldTheme.toStdString(),
                                 settings->uiThemeSlug().toStdString());
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
    MainWindow::instance()->imageProvider()->download(
      QString(normalizedUri).remove("mxc://"),
      {96, 96},
      [message](const QString &, const QSize &, const QImage &image, const QString &) {
          auto reply = message.createReply();
          reply << QVariant::fromValue(image);
          QDBusConnection::sessionBus().send(reply);
      },
      true);
    return {};
}

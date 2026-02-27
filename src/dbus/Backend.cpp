// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Backend.h"

#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include "ChatPage.h"
#include "MainWindow.h"
#include "MxcImageProvider.h"
#include "cache/core/Cache.h"
#include "config/nheko.h"
#include "settings/SettingKeys.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineModel.h"
#include <spdlog/logger.h>

#include <QDBusConnection>
#include <spdlog/sinks/null_sink.h>

DbusBackend::DbusBackend(RoomlistModel *parent)
  : QObject{parent}
  , m_parent{parent}
{
}

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

QString
DbusBackend::apiVersion() const
{
    return nheko::dbus::dbusApiVersion.toString();
}

QString
DbusBackend::appVersion() const
{
    return nheko::version;
}

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

struct RoomReplyState
{
    QVector<nheko::dbus::RoomInfoItem> model;
    std::map<QString, RoomInfo> roominfos;
    std::mutex m;
};

QVector<nheko::dbus::RoomInfoItem>
DbusBackend::rooms() const
{
    if (!dbusReadAccessEnabled())
        return {};

    activeLoggers().ui->debug("Rooms requested over D-Bus.");

    const auto roomListModel = m_parent->models;
    QVector<nheko::dbus::RoomInfoItem> model;

    for (const auto &room : roomListModel) {
        const auto aliases =
          cache::getStateEvent<mtx::events::state::CanonicalAlias>(room->roomId().toStdString());
        QString alias;
        if (aliases.has_value()) {
            const auto &val = aliases.value().content;
            if (!val.alias.empty())
                alias = QString::fromStdString(val.alias);
            else if (val.alt_aliases.size() > 0)
                alias = QString::fromStdString(val.alt_aliases.front());
        }

        model.push_back(nheko::dbus::RoomInfoItem{room->roomId(),
                                                  alias,
                                                  room->plainRoomName(),
                                                  room->roomAvatarUrl(),
                                                  room->notificationCount()});
    }

    activeLoggers().ui->debug("Sending {} rooms over D-Bus...", model.size());
    return model;
}

QImage
DbusBackend::image(const QString &uri, const QDBusMessage &message) const
{
    if (!dbusReadAccessEnabled())
        return {};

    const auto normalizedUri = stripDbusTypePrefix(uri);

    message.setDelayedReply(true);
    activeLoggers().ui->debug("Rooms requested over D-Bus.");
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

void
DbusBackend::activateRoom(const QString &alias) const
{
    if (!dbusWriteAccessEnabled())
        return;

    bringWindowToTop();
    m_parent->setCurrentRoom(stripDbusTypePrefix(alias));
}

void
DbusBackend::joinRoom(const QString &alias) const
{
    if (!dbusWriteAccessEnabled())
        return;

    bringWindowToTop();
    ChatPage::instance()->joinRoom(stripDbusTypePrefix(alias));
}

void
DbusBackend::directChat(const QString &userId) const
{
    if (!dbusWriteAccessEnabled())
        return;

    bringWindowToTop();
    ChatPage::instance()->startChat(stripDbusTypePrefix(userId));
}

QString
DbusBackend::statusMessage() const
{
    if (!dbusReadAccessEnabled())
        return {};

    return ChatPage::instance()->status();
}

void
DbusBackend::setStatusMessage(const QString &message)
{
    if (!dbusWriteAccessEnabled())
        return;

    ChatPage::instance()->setStatus(stripDbusTypePrefix(message));
}

void
DbusBackend::setTheme(const QString &theme)
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

void
DbusBackend::bringWindowToTop() const
{
    MainWindow::instance()->show();
    MainWindow::instance()->raise();
}

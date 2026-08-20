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
#include "logging/Logging.h"
#include "settings/SettingKeys.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"

#include <QDBusConnection>
#include <QJsonDocument>

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

/// Sends a D-Bus error reply. Unlike returning an empty string, this lets a
/// caller tell failure from a legitimately empty result.
void
replyWithDbusError(const QDBusMessage &message, const QString &error)
{
    QDBusConnection::sessionBus().send(
      message.createErrorReply(QStringLiteral("cc.etke.komai.Error.Failed"), error));
}

void
replyWithDbusAccessDenied(const QDBusMessage &message)
{
    QDBusConnection::sessionBus().send(
      message.createErrorReply(QStringLiteral("cc.etke.komai.Error.AccessDenied"),
                               QStringLiteral("D-Bus write access is disabled.")));
}

/// Relays a room action that carries no payload beyond success.
komai::ipc::RoomActionCallback
dbusActionResponder(const QDBusMessage &message)
{
    return [message](const QString &error) {
        if (!error.isEmpty()) {
            replyWithDbusError(message, error);
            return;
        }
        QDBusConnection::sessionBus().send(message.createReply());
    };
}

/// Relays a result that is a single event ID.
komai::ipc::SendMessageCallback
dbusEventIdResponder(const QDBusMessage &message)
{
    return [message](const QString &eventId, const QString &error) {
        if (!error.isEmpty()) {
            replyWithDbusError(message, error);
            return;
        }
        auto reply = message.createReply();
        reply << eventId;
        QDBusConnection::sessionBus().send(reply);
    };
}

/// Takes over replying to `message`, so the caller answers exactly once.
///
/// Qt sends the slot's return value as a reply unless the message is marked
/// delayed. Sending an error without marking it first therefore puts two
/// replies on the bus for one call, so the mark comes before any early exit.
void
beginDbusReply(const QDBusMessage &message)
{
    message.setDelayedReply(true);
}

/// Begins a reply and refuses when read access is off.
bool
beginDbusRead(const QDBusMessage &message)
{
    beginDbusReply(message);
    if (dbusReadAccessEnabled())
        return true;

    QDBusConnection::sessionBus().send(
      message.createErrorReply(QStringLiteral("cc.etke.komai.Error.AccessDenied"),
                               QStringLiteral("D-Bus read access is disabled.")));
    return false;
}

/// Begins a reply and refuses when write access is off.
bool
beginDbusWrite(const QDBusMessage &message)
{
    beginDbusReply(message);
    if (dbusWriteAccessEnabled())
        return true;

    replyWithDbusAccessDenied(message);
    return false;
}

DbusBackendLoggers
defaultLoggers()
{
    return {
      .ui = std::make_shared<komai::logging::Logger>("dbus-ui"),
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
        model.push_back(komai::dbus::RoomInfoItem{r.roomId,
                                                  r.alias,
                                                  r.name,
                                                  r.avatarUrl,
                                                  r.read,
                                                  r.unreadCount,
                                                  r.memberCount,
                                                  r.mostRecentEventTimestampMs,
                                                  r.highlighted,
                                                  r.categories,
                                                  r.tags,
                                                  r.parentSpaces,
                                                  r.directUserId,
                                                  r.encrypted});

    activeLoggers().ui->debug("Sending {} rooms over D-Bus...", model.size());
    return model;
}

QString
DbusRoomsInterface::timeline(const QString &roomIdOrAlias,
                             const int limit,
                             const QString &beforeEventId,
                             const bool includeUnsignedFields,
                             const QString &fetchMode,
                             const QDBusMessage &message) const
{
    if (!beginDbusRead(message))
        return {};

    komai::ipc::readTimeline(stripDbusTypePrefix(roomIdOrAlias),
                             limit,
                             stripDbusTypePrefix(beforeEventId),
                             includeUnsignedFields,
                             stripDbusTypePrefix(fetchMode),
                             [message](const QJsonObject &result, const QString &error) {
                                 if (!error.isEmpty()) {
                                     replyWithDbusError(message, error);
                                     return;
                                 }

                                 auto reply = message.createReply();
                                 reply << QString::fromUtf8(
                                   QJsonDocument(result).toJson(QJsonDocument::Compact));
                                 QDBusConnection::sessionBus().send(reply);
                             });
    return {};
}

void
DbusRoomsInterface::join(const QString &roomIdOrAlias, const QDBusMessage &message) const
{
    if (!beginDbusWrite(message))
        return;

    const auto normalized = stripDbusTypePrefix(roomIdOrAlias);
    if (normalized.isEmpty()) {
        replyWithDbusError(message, QStringLiteral("roomIdOrAlias must not be empty"));
        return;
    }

    // joinRoom() hands off to the UI and does not report back, so this can
    // confirm the request was accepted but not that the join succeeded.
    komai::ipc::joinRoom(normalized);
    QDBusConnection::sessionBus().send(message.createReply());
}

void
DbusRoomsInterface::newDirectChat(const QString &userId, const QDBusMessage &message) const
{
    if (!beginDbusWrite(message))
        return;

    const auto normalized = stripDbusTypePrefix(userId);
    if (normalized.isEmpty()) {
        replyWithDbusError(message, QStringLiteral("userId must not be empty"));
        return;
    }

    // As with join(), this confirms the request rather than the outcome.
    komai::ipc::newDirectChat(normalized);
    QDBusConnection::sessionBus().send(message.createReply());
}

QString
DbusRoomsInterface::send(const QString &roomIdOrAlias,
                         const QString &body,
                         const QString &msgtype,
                         const QString &format,
                         const QDBusMessage &message) const
{
    if (!beginDbusWrite(message))
        return {};

    komai::ipc::sendMessage(stripDbusTypePrefix(roomIdOrAlias),
                            stripDbusTypePrefix(body),
                            stripDbusTypePrefix(msgtype),
                            stripDbusTypePrefix(format),
                            dbusEventIdResponder(message));
    return {};
}

QString
DbusRoomsInterface::sendImageFile(const QString &roomIdOrAlias,
                                  const QString &filePath,
                                  const QString &body,
                                  const QDBusMessage &message) const
{
    if (!beginDbusWrite(message))
        return {};

    komai::ipc::sendImageFromFile(stripDbusTypePrefix(roomIdOrAlias),
                                  stripDbusTypePrefix(filePath),
                                  stripDbusTypePrefix(body),
                                  dbusEventIdResponder(message));
    return {};
}

QString
DbusRoomsInterface::sendImage(const QString &roomIdOrAlias,
                              const QString &mxcUri,
                              const QString &body,
                              const QString &filename,
                              const QDBusMessage &message) const
{
    if (!beginDbusWrite(message))
        return {};

    komai::ipc::sendImage(stripDbusTypePrefix(roomIdOrAlias),
                          stripDbusTypePrefix(mxcUri),
                          stripDbusTypePrefix(body),
                          stripDbusTypePrefix(filename),
                          {},
                          dbusEventIdResponder(message));
    return {};
}

// -- membership --

namespace {

/// The four user-targeting membership methods differ only in the SharedLogic
/// call, so the adaptor keeps one body and a small dispatch.
using MembershipFn = void (*)(const QString &,
                              const QString &,
                              const QString &,
                              komai::ipc::RoomActionCallback);

void
runDbusMembership(const QString &roomIdOrAlias,
                  const QString &userId,
                  const QString &reason,
                  const QDBusMessage &message,
                  MembershipFn membershipFn)
{
    if (!beginDbusWrite(message))
        return;

    membershipFn(stripDbusTypePrefix(roomIdOrAlias),
                 stripDbusTypePrefix(userId),
                 stripDbusTypePrefix(reason),
                 dbusActionResponder(message));
}

} // namespace

void
DbusRoomsInterface::invite(const QString &roomIdOrAlias,
                           const QString &userId,
                           const QString &reason,
                           const QDBusMessage &message) const
{
    runDbusMembership(roomIdOrAlias, userId, reason, message, &komai::ipc::inviteUser);
}

void
DbusRoomsInterface::kick(const QString &roomIdOrAlias,
                         const QString &userId,
                         const QString &reason,
                         const QDBusMessage &message) const
{
    runDbusMembership(roomIdOrAlias, userId, reason, message, &komai::ipc::kickUser);
}

void
DbusRoomsInterface::ban(const QString &roomIdOrAlias,
                        const QString &userId,
                        const QString &reason,
                        const QDBusMessage &message) const
{
    runDbusMembership(roomIdOrAlias, userId, reason, message, &komai::ipc::banUser);
}

void
DbusRoomsInterface::unban(const QString &roomIdOrAlias,
                          const QString &userId,
                          const QString &reason,
                          const QDBusMessage &message) const
{
    runDbusMembership(roomIdOrAlias, userId, reason, message, &komai::ipc::unbanUser);
}

void
DbusRoomsInterface::leave(const QString &roomIdOrAlias,
                          const QString &reason,
                          const QDBusMessage &message) const
{
    if (!beginDbusWrite(message))
        return;

    komai::ipc::leaveRoom(stripDbusTypePrefix(roomIdOrAlias),
                          stripDbusTypePrefix(reason),
                          dbusActionResponder(message));
}

QString
DbusRoomsInterface::create(const QString &optionsJson, const QDBusMessage &message) const
{
    if (!beginDbusWrite(message))
        return {};

    const auto normalized = stripDbusTypePrefix(optionsJson).trimmed();
    QJsonObject options;
    if (!normalized.isEmpty()) {
        QJsonParseError parseError;
        const auto doc = QJsonDocument::fromJson(normalized.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            replyWithDbusError(
              message, QStringLiteral("options is not valid JSON: ") + parseError.errorString());
            return {};
        }
        if (!doc.isObject()) {
            replyWithDbusError(message, QStringLiteral("options must be a JSON object"));
            return {};
        }
        options = doc.object();
    }

    komai::ipc::CreateRoomRequest request;
    request.name           = options.value(QStringLiteral("name")).toString();
    request.topic          = options.value(QStringLiteral("topic")).toString();
    request.aliasLocalpart = options.value(QStringLiteral("aliasLocalpart")).toString();
    request.preset         = options.value(QStringLiteral("preset")).toString();
    request.roomVersion    = options.value(QStringLiteral("roomVersion")).toString();
    request.isDirect       = options.value(QStringLiteral("isDirect")).toBool();
    request.isEncrypted    = options.value(QStringLiteral("isEncrypted")).toBool();
    request.isSpace        = options.value(QStringLiteral("isSpace")).toBool();
    request.isPublic       = options.value(QStringLiteral("isPublic")).toBool();
    request.powerLevelContentOverride =
      options.value(QStringLiteral("powerLevelContentOverride")).toObject();
    request.initialState    = options.value(QStringLiteral("initialState")).toArray();
    request.creationContent = options.value(QStringLiteral("creationContent")).toObject();
    for (const auto invitee : options.value(QStringLiteral("invite")).toArray())
        request.inviteUserIds.append(invitee.toString());

    komai::ipc::createRoom(request, [message](const QString &roomId, const QString &error) {
        if (!error.isEmpty()) {
            replyWithDbusError(message, error);
            return;
        }
        auto reply = message.createReply();
        reply << roomId;
        QDBusConnection::sessionBus().send(reply);
    });
    return {};
}

// -- state --

QString
DbusRoomsInterface::getState(const QString &roomIdOrAlias,
                             const QString &eventType,
                             const QString &stateKey,
                             const QDBusMessage &message) const
{
    if (!beginDbusRead(message))
        return {};

    komai::ipc::readStateEvent(
      stripDbusTypePrefix(roomIdOrAlias),
      stripDbusTypePrefix(eventType),
      stripDbusTypePrefix(stateKey),
      [message](const komai::ipc::StateEventResult &result, const QString &error) {
          if (!error.isEmpty()) {
              replyWithDbusError(message, error);
              return;
          }

          const QJsonObject payload{
            {QStringLiteral("exists"), result.exists},
            {QStringLiteral("content"), result.content},
          };
          auto reply = message.createReply();
          reply << QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
          QDBusConnection::sessionBus().send(reply);
      });
    return {};
}

QString
DbusRoomsInterface::setState(const QString &roomIdOrAlias,
                             const QString &eventType,
                             const QString &stateKey,
                             const QString &contentJson,
                             const QDBusMessage &message) const
{
    if (!beginDbusWrite(message))
        return {};

    const auto doc = QJsonDocument::fromJson(stripDbusTypePrefix(contentJson).toUtf8());
    if (!doc.isObject()) {
        replyWithDbusError(message, QStringLiteral("content must be a JSON object"));
        return {};
    }

    komai::ipc::sendStateEvent(stripDbusTypePrefix(roomIdOrAlias),
                               stripDbusTypePrefix(eventType),
                               stripDbusTypePrefix(stateKey),
                               doc.object(),
                               dbusEventIdResponder(message));
    return {};
}

void
DbusRoomsInterface::setName(const QString &roomIdOrAlias,
                            const QString &name,
                            const QDBusMessage &message) const
{
    if (!beginDbusWrite(message))
        return;

    komai::ipc::setRoomName(
      stripDbusTypePrefix(roomIdOrAlias), stripDbusTypePrefix(name), dbusActionResponder(message));
}

void
DbusRoomsInterface::setTopic(const QString &roomIdOrAlias,
                             const QString &topic,
                             const QDBusMessage &message) const
{
    if (!beginDbusWrite(message))
        return;

    komai::ipc::setRoomTopic(
      stripDbusTypePrefix(roomIdOrAlias), stripDbusTypePrefix(topic), dbusActionResponder(message));
}

void
DbusRoomsInterface::setPowerLevel(const QString &roomIdOrAlias,
                                  const QString &userId,
                                  const int powerLevel,
                                  const QDBusMessage &message) const
{
    if (!beginDbusWrite(message))
        return;

    komai::ipc::setUserPowerLevel(stripDbusTypePrefix(roomIdOrAlias),
                                  stripDbusTypePrefix(userId),
                                  powerLevel,
                                  dbusActionResponder(message));
}

// -- moderation and read state --

QString
DbusRoomsInterface::redact(const QString &roomIdOrAlias,
                           const QString &eventId,
                           const QString &reason,
                           const QDBusMessage &message) const
{
    if (!beginDbusWrite(message))
        return {};

    komai::ipc::redactEvent(stripDbusTypePrefix(roomIdOrAlias),
                            stripDbusTypePrefix(eventId),
                            stripDbusTypePrefix(reason),
                            dbusEventIdResponder(message));
    return {};
}

void
DbusRoomsInterface::markRead(const QString &roomIdOrAlias,
                             const QString &eventId,
                             const QString &receipt,
                             const QDBusMessage &message) const
{
    if (!beginDbusWrite(message))
        return;

    // D-Bus has no optional argument, so an empty string is how a caller says
    // "follow the user's own setting for this room".
    const auto normalizedReceipt = stripDbusTypePrefix(receipt).trimmed();
    std::optional<bool> publicReceipt;
    if (normalizedReceipt == QLatin1String("public"))
        publicReceipt = true;
    else if (normalizedReceipt == QLatin1String("private"))
        publicReceipt = false;
    else if (!normalizedReceipt.isEmpty()) {
        replyWithDbusError(message,
                           QStringLiteral("receipt must be 'public', 'private', or empty"));
        return;
    }

    komai::ipc::markRoomRead(stripDbusTypePrefix(roomIdOrAlias),
                             stripDbusTypePrefix(eventId),
                             publicReceipt,
                             dbusActionResponder(message));
}

void
DbusRoomsInterface::markUnread(const QString &roomIdOrAlias,
                               const bool unread,
                               const QDBusMessage &message) const
{
    if (!beginDbusWrite(message))
        return;

    komai::ipc::markRoomUnread(
      stripDbusTypePrefix(roomIdOrAlias), unread, dbusActionResponder(message));
}

QString
DbusRoomsInterface::readReceipts(const QString &roomIdOrAlias,
                                 const QString &eventId,
                                 const QDBusMessage &message) const
{
    if (!beginDbusRead(message))
        return {};

    komai::ipc::readReceipts(
      stripDbusTypePrefix(roomIdOrAlias),
      stripDbusTypePrefix(eventId),
      [message](const QVector<komai::ipc::ReadReceipt> &receipts, const QString &error) {
          if (!error.isEmpty()) {
              replyWithDbusError(message, error);
              return;
          }

          QJsonArray arr;
          for (const auto &receipt : receipts)
              arr.append(receipt.toJson());

          auto reply = message.createReply();
          reply << QString::fromUtf8(QJsonDocument(QJsonObject{{QStringLiteral("receipts"), arr}})
                                       .toJson(QJsonDocument::Compact));
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

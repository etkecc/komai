// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SharedLogic.h"

#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QPointer>

#include <optional>

#include <mtx/events/collections.hpp>
#include <mtx/responses/media.hpp>
#include <mtx/responses/messages.hpp>
#include <mtxclient/crypto/utils.hpp>

#include "blurhash.hpp"

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "config/komai.h"
#include "encryption/Olm.h"
#include "events/EventAccessors.h"
#include "matrix/MatrixClient.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "providers/MxcImageProvider.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineModel.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

namespace {

RoomlistModel *
currentRoomlistModel()
{
    auto *chatPage = ChatPage::instance();
    if (!chatPage || !chatPage->timelineManager())
        return nullptr;
    return chatPage->timelineManager()->rooms();
}

std::optional<uint64_t>
currentMatrixRuntimeHandleId()
{
    const auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() == 0)
        return std::nullopt;

    return mainWindow->matrixBackendHandleId();
}

QString
primaryAliasForRoom(const QString &roomId)
{
    const auto aliases =
      cache::getStateEvent<mtx::events::state::CanonicalAlias>(roomId.toStdString());
    if (!aliases.has_value())
        return {};

    const auto &value = aliases.value().content;
    if (!value.alias.empty())
        return QString::fromStdString(value.alias);
    if (!value.alt_aliases.empty())
        return QString::fromStdString(value.alt_aliases.front());
    return {};
}

QStringList
roomCategories(RoomlistModel *roomlist, const QModelIndex &index)
{
    QStringList categories;

    const bool isSpace     = roomlist->data(index, RoomlistModel::IsSpace).toBool();
    const bool isDirect    = roomlist->data(index, RoomlistModel::IsDirect).toBool();
    const bool isBotRoom   = roomlist->data(index, RoomlistModel::IsBotRoom).toBool();
    const bool isEncrypted = roomlist->data(index, RoomlistModel::IsEncrypted).toBool();

    if (isSpace) {
        categories.push_back(QStringLiteral("space"));
    } else if (isDirect) {
        categories.push_back(QStringLiteral("direct"));
        categories.push_back(isBotRoom ? QStringLiteral("bot") : QStringLiteral("person"));
    } else {
        categories.push_back(QStringLiteral("group"));
    }

    if (isEncrypted)
        categories.push_back(QStringLiteral("encrypted"));

    return categories;
}

constexpr int kMaxTimelineLimit                 = 500;
constexpr int kTimelineServerFetchPageSize      = 200;
constexpr int kTimelineServerFetchMaxRequests   = 10;
constexpr auto kTimelineFetchModeCachedOnly     = "cached_only";
constexpr auto kTimelineFetchModeServerIfNeeded = "server_fetch_if_needed";

enum class TimelineFetchMode
{
    CachedOnly,
    ServerFetchIfNeeded,
};

std::optional<TimelineFetchMode>
parseTimelineFetchMode(const QString &value)
{
    const auto normalized = value.trimmed();
    if (normalized.isEmpty() || normalized == QLatin1String(kTimelineFetchModeCachedOnly))
        return TimelineFetchMode::CachedOnly;
    if (normalized == QLatin1String(kTimelineFetchModeServerIfNeeded))
        return TimelineFetchMode::ServerFetchIfNeeded;
    return std::nullopt;
}

QJsonObject
jsonObjectFromNlohmann(const nlohmann::json &value)
{
    const auto encoded = QByteArray::fromStdString(value.dump());
    return QJsonDocument::fromJson(encoded).object();
}

mtx::events::collections::TimelineEvents
materializedTimelineEvent(const std::string &roomId,
                          const mtx::events::collections::TimelineEvents &event)
{
    if (const auto *encrypted =
          std::get_if<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(&event)) {
        MegolmSessionIndex index;
        index.room_id    = roomId;
        index.session_id = encrypted->content.session_id;

        auto decrypted = olm::decryptEvent(index, *encrypted);
        if (decrypted.event.has_value())
            return *decrypted.event;
    }

    return event;
}

QJsonObject
serializedTimelineEvent(const std::string &roomId,
                        const mtx::events::collections::TimelineEvents &event,
                        const bool includeUnsignedFields)
{
    auto serialized = mtx::accessors::serialize_event(materializedTimelineEvent(roomId, event));
    serialized.erase("room_id");
    if (!includeUnsignedFields)
        serialized.erase("unsigned");
    return jsonObjectFromNlohmann(serialized);
}

struct TimelineSlice
{
    QJsonArray events;
    bool hasMoreLocal         = false;
    QString nextBeforeEventId = {};
};

bool
hasOlderCachedTimelineEvents(const std::string &roomId,
                             const std::uint64_t nextIndex,
                             const std::uint64_t rangeFirst)
{
    if (nextIndex < rangeFirst)
        return false;

    for (std::uint64_t idx = nextIndex;; --idx) {
        if (cache::getTimelineEventId(roomId, idx).has_value())
            return true;

        if (idx == rangeFirst)
            break;
    }

    return false;
}

std::optional<TimelineSlice>
sliceTimelineFromCache(const std::string &roomId,
                       const QString &beforeEventId,
                       const int limit,
                       const bool includeUnsignedFields,
                       QString *error)
{
    TimelineSlice slice;

    const auto range = cache::getTimelineRange(roomId);
    if (!range.has_value())
        return slice;

    std::uint64_t startIndex = range->last;
    if (!beforeEventId.isEmpty()) {
        const auto anchorIndex = cache::getTimelineIndex(roomId, beforeEventId.toStdString());
        if (!anchorIndex.has_value()) {
            if (error)
                *error =
                  QStringLiteral("beforeEventId not found in local timeline: ") + beforeEventId;
            return std::nullopt;
        }

        if (*anchorIndex == range->first)
            return slice;

        startIndex = *anchorIndex - 1;
    }

    if (startIndex < range->first)
        return slice;

    std::optional<std::uint64_t> nextIndexToProbe;
    for (std::uint64_t idx = startIndex;; --idx) {
        const auto eventId = cache::getTimelineEventId(roomId, idx);
        if (eventId.has_value()) {
            const auto event = cache::getEvent(roomId, *eventId);
            if (event.has_value()) {
                slice.events.append(serializedTimelineEvent(roomId, *event, includeUnsignedFields));
                if (slice.events.size() >= limit) {
                    nextIndexToProbe =
                      (idx > range->first) ? std::optional<std::uint64_t>(idx - 1) : std::nullopt;
                    break;
                }
            }
        }

        if (idx == range->first)
            break;
    }

    if (nextIndexToProbe.has_value())
        slice.hasMoreLocal = hasOlderCachedTimelineEvents(roomId, *nextIndexToProbe, range->first);

    if (!slice.events.isEmpty())
        slice.nextBeforeEventId = slice.events.at(slice.events.size() - 1)
                                    .toObject()
                                    .value(QStringLiteral("event_id"))
                                    .toString();

    return slice;
}

QJsonObject
timelineResultToJson(const QString &roomId, const TimelineSlice &slice, const bool hasMore)
{
    return {
      {QStringLiteral("roomId"), roomId},
      {QStringLiteral("events"), slice.events},
      {QStringLiteral("hasMore"), hasMore},
      {QStringLiteral("nextBeforeEventId"),
       hasMore && !slice.nextBeforeEventId.isEmpty() ? QJsonValue(slice.nextBeforeEventId)
                                                     : QJsonValue(QJsonValue::Null)},
    };
}

class TimelineReadOperation final : public QObject
{
public:
    TimelineReadOperation(const QString &roomId,
                          const int limit,
                          const QString &beforeEventId,
                          const bool includeUnsignedFields,
                          const TimelineFetchMode fetchMode,
                          komai::ipc::ReadTimelineCallback callback)
      : QObject(ChatPage::instance())
      , roomId_{roomId}
      , roomIdStd_{roomId.toStdString()}
      , limit_{limit}
      , beforeEventId_{beforeEventId}
      , includeUnsignedFields_{includeUnsignedFields}
      , fetchMode_{fetchMode}
      , callback_{std::move(callback)}
    {
    }

    void start()
    {
        if (!refreshSlice())
            return;

        maybeFetchOlder();
    }

private:
    bool refreshSlice()
    {
        QString error;
        const auto slice = sliceTimelineFromCache(
          roomIdStd_, beforeEventId_, limit_, includeUnsignedFields_, &error);
        if (!slice.has_value()) {
            fail(error);
            return false;
        }

        slice_ = *slice;
        return true;
    }

    bool serverCanFetchMore() const
    {
        if (fetchMode_ != TimelineFetchMode::ServerFetchIfNeeded || serverHistoryExhausted_)
            return false;

        return !cache::previousBatchToken(roomIdStd_).empty();
    }

    bool shouldFetchOlder() const
    {
        return slice_.events.size() < limit_ && serverCanFetchMore() &&
               requestsDone_ < kTimelineServerFetchMaxRequests;
    }

    void maybeFetchOlder()
    {
        if (!shouldFetchOlder()) {
            finish();
            return;
        }

        const auto fromToken = QString::fromStdString(cache::previousBatchToken(roomIdStd_));
        if (fromToken.isEmpty()) {
            finish();
            return;
        }

        ++requestsDone_;

        mtx::http::MessagesOpts opts;
        opts.room_id = roomIdStd_;
        opts.from    = fromToken.toStdString();
        opts.limit   = kTimelineServerFetchPageSize;

        QPointer<TimelineReadOperation> self(this);
        http::client()->messages(
          opts, [self, fromToken](const mtx::responses::Messages &res, mtx::http::RequestErr err) {
              if (!self)
                  return;

              QMetaObject::invokeMethod(
                self.data(),
                [self, fromToken, res, err]() {
                    if (self)
                        self->handleFetchResult(fromToken, res, err);
                },
                Qt::QueuedConnection);
          });
    }

    void handleFetchResult(const QString &fromToken,
                           const mtx::responses::Messages &res,
                           mtx::http::RequestErr err)
    {
        if (err) {
            fail(QStringLiteral("failed to fetch older messages: ") +
                 QString::fromStdString(err->matrix_error.error));
            return;
        }

        const auto currentToken = QString::fromStdString(cache::previousBatchToken(roomIdStd_));
        if (currentToken != fromToken) {
            if (!refreshSlice())
                return;
            maybeFetchOlder();
            return;
        }

        const bool noMoreServerHistory =
          res.end.empty() || QString::fromStdString(res.end) == fromToken;
        const bool tokenAdvanced = !res.end.empty() && QString::fromStdString(res.end) != fromToken;

        if (!res.chunk.empty() || tokenAdvanced)
            cache::saveOldMessages(roomIdStd_, res);

        if (noMoreServerHistory)
            serverHistoryExhausted_ = true;

        if (!refreshSlice())
            return;

        maybeFetchOlder();
    }

    void finish()
    {
        const bool hasMore = slice_.hasMoreLocal || serverCanFetchMore();
        if (callback_)
            callback_(timelineResultToJson(roomId_, slice_, hasMore), {});
        deleteLater();
    }

    void fail(const QString &error)
    {
        if (callback_)
            callback_({}, error);
        deleteLater();
    }

    QString roomId_;
    std::string roomIdStd_;
    int limit_;
    QString beforeEventId_;
    bool includeUnsignedFields_;
    TimelineFetchMode fetchMode_;
    komai::ipc::ReadTimelineCallback callback_;
    TimelineSlice slice_;
    int requestsDone_            = 0;
    bool serverHistoryExhausted_ = false;
};

} // namespace

namespace komai::ipc {

// -- app --

QString
apiVersion()
{
    return apiVersionNumber.toString();
}

QString
appVersion()
{
    return komai::version;
}

// -- rooms --

QJsonObject
RoomInfo::toJson() const
{
    return {
      {QStringLiteral("id"), roomId},
      {QStringLiteral("alias"), alias},
      {QStringLiteral("name"), name},
      {QStringLiteral("avatarUrl"), avatarUrl},
      {QStringLiteral("read"), read},
      {QStringLiteral("serverNotificationCount"), serverNotificationCount},
      {QStringLiteral("memberCount"), memberCount},
      {QStringLiteral("mostRecentEventTimestampMs"),
       static_cast<qint64>(mostRecentEventTimestampMs)},
      {QStringLiteral("highlighted"), highlighted},
      {QStringLiteral("categories"), QJsonArray::fromStringList(categories)},
      {QStringLiteral("tags"), QJsonArray::fromStringList(tags)},
      {QStringLiteral("parentSpaces"), QJsonArray::fromStringList(parentSpaces)},
      {QStringLiteral("dmUserId"), directUserId},
      {QStringLiteral("encrypted"), encrypted},
    };
}

QVector<RoomInfo>
roomList()
{
    auto *rl = currentRoomlistModel();
    if (!rl)
        return {};

    QVector<RoomInfo> result;
    result.reserve(rl->rowCount());

    for (int row = 0; row < rl->rowCount(); ++row) {
        const auto index = rl->index(row, 0);
        if (rl->data(index, RoomlistModel::IsInvite).toBool() ||
            rl->data(index, RoomlistModel::IsPreview).toBool()) {
            continue;
        }

        const auto roomId = rl->data(index, RoomlistModel::RoomId).toString();
        if (roomId.isEmpty())
            continue;

        const auto memberCount =
          rl->cachedJoinedRooms_.contains(roomId)
            ? static_cast<int>(rl->cachedJoinedRooms_.value(roomId).member_count)
            : 0;

        result.push_back({
          .roomId                     = roomId,
          .alias                      = primaryAliasForRoom(roomId),
          .name                       = rl->data(index, RoomlistModel::RoomName).toString(),
          .avatarUrl                  = rl->data(index, RoomlistModel::AvatarUrl).toString(),
          .read                       = !rl->data(index, RoomlistModel::HasUnreadMessages).toBool(),
          .serverNotificationCount    = rl->data(index, RoomlistModel::NotificationCount).toInt(),
          .memberCount                = memberCount,
          .mostRecentEventTimestampMs = rl->data(index, RoomlistModel::Timestamp).toULongLong(),
          .highlighted  = rl->data(index, RoomlistModel::HasLoudNotification).toBool(),
          .categories   = roomCategories(rl, index),
          .tags         = rl->data(index, RoomlistModel::Tags).toStringList(),
          .parentSpaces = rl->data(index, RoomlistModel::ParentSpaces).toStringList(),
          .directUserId = rl->data(index, RoomlistModel::DirectChatOtherUserId).toString(),
          .encrypted    = rl->data(index, RoomlistModel::IsEncrypted).toBool(),
        });
    }

    return result;
}

void
activateRoom(const QString &roomIdOrAlias)
{
    MainWindow::instance()->show();
    MainWindow::instance()->raise();
    if (auto *rl = currentRoomlistModel())
        rl->setCurrentRoom(roomIdOrAlias);
}

void
joinRoom(const QString &roomIdOrAlias)
{
    MainWindow::instance()->show();
    MainWindow::instance()->raise();
    ChatPage::instance()->joinRoom(roomIdOrAlias);
}

void
newDirectChat(const QString &userId)
{
    MainWindow::instance()->show();
    MainWindow::instance()->raise();
    ChatPage::instance()->startChat(userId);
}

// -- user --

QString
userId()
{
    const auto settings = UserSettings::instance();
    return settings ? settings->sessionSnapshot().userId : QString{};
}

QString
homeserverUrl()
{
    const auto settings = UserSettings::instance();
    return settings ? settings->sessionSnapshot().homeserver : QString{};
}

QString
deviceId()
{
    const auto settings = UserSettings::instance();
    return settings ? settings->sessionSnapshot().deviceId : QString{};
}

QString
statusMessage()
{
    return ChatPage::instance()->status();
}

void
setStatusMessage(const QString &message)
{
    ChatPage::instance()->setStatus(message);
}

// -- settings.ui --

QString
uiTheme()
{
    const auto settings = UserSettings::instance();
    if (!settings)
        return {};

    return settings->uiThemeSlug();
}

void
setUiTheme(const QString &theme)
{
    const auto settings = UserSettings::instance();
    if (!settings)
        return;

    settings->setUiThemeSlug(theme);
}

// -- rooms (messaging) --

QString
resolveRoomId(const QString &roomIdOrAlias)
{
    if (roomIdOrAlias.startsWith(QLatin1Char('!')))
        return roomIdOrAlias;

    if (roomIdOrAlias.startsWith(QLatin1Char('#'))) {
        const auto rooms  = cache::roomNamesAndAliases();
        const auto needle = roomIdOrAlias.toStdString();
        for (const auto &room : rooms) {
            if (room.alias == needle)
                return QString::fromStdString(room.id);
        }
    }
    return {};
}

void
readTimeline(const QString &roomIdOrAlias,
             const int limit,
             const QString &beforeEventId,
             const bool includeUnsignedFields,
             const QString &fetchMode,
             ReadTimelineCallback callback)
{
    const auto roomId = resolveRoomId(roomIdOrAlias);
    if (roomId.isEmpty()) {
        if (callback)
            callback({}, QStringLiteral("room not found: ") + roomIdOrAlias);
        return;
    }

    if (limit <= 0 || limit > kMaxTimelineLimit) {
        if (callback) {
            callback({}, QStringLiteral("limit must be between 1 and %1").arg(kMaxTimelineLimit));
        }
        return;
    }

    const auto parsedFetchMode = parseTimelineFetchMode(fetchMode);
    if (!parsedFetchMode.has_value()) {
        if (callback) {
            callback({},
                     QStringLiteral("fetchMode must be one of: %1, %2")
                       .arg(QLatin1String(kTimelineFetchModeCachedOnly),
                            QLatin1String(kTimelineFetchModeServerIfNeeded)));
        }
        return;
    }

    auto *operation = new TimelineReadOperation{roomId,
                                                limit,
                                                beforeEventId.trimmed(),
                                                includeUnsignedFields,
                                                *parsedFetchMode,
                                                std::move(callback)};
    operation->start();
}

void
sendMessage(const QString &roomIdOrAlias,
            const QString &body,
            const QString &msgtype,
            const QString &format,
            SendMessageCallback callback)
{
    if (msgtype != QLatin1String("m.text") && msgtype != QLatin1String("m.notice")) {
        if (callback)
            callback({}, QStringLiteral("msgtype must be one of: m.text, m.notice"));
        return;
    }

    if (format != QLatin1String("auto") && format != QLatin1String("plain") &&
        format != QLatin1String("html")) {
        if (callback)
            callback({}, QStringLiteral("format must be one of: auto, plain, html"));
        return;
    }

    const auto roomId = resolveRoomId(roomIdOrAlias);
    if (roomId.isEmpty()) {
        if (callback)
            callback({}, QStringLiteral("room not found: ") + roomIdOrAlias);
        return;
    }

    const auto trimmedBody = body.trimmed();
    if (trimmedBody.isEmpty()) {
        if (callback)
            callback({}, QStringLiteral("message body must not be empty"));
        return;
    }

    std::string formattedBody;
    if (format == QLatin1String("html")) {
        formattedBody = utils::markdownToHtml(trimmedBody, false).toStdString();
    } else if (format == QLatin1String("auto")) {
        if (UserSettings::instance()->composerInputMarkdownToHtmlEnabled())
            formattedBody = utils::markdownToHtml(trimmedBody, false).toStdString();
    }

    if (!formattedBody.empty() && formattedBody.find('<') == std::string::npos &&
        trimmedBody.toStdString().find('\n') == std::string::npos &&
        trimmedBody.toStdString().find('\\') == std::string::npos) {
        formattedBody.clear();
    }

    const auto handleId = currentMatrixRuntimeHandleId();
    if (!handleId.has_value()) {
        if (callback)
            callback({}, QStringLiteral("matrix-sdk runtime is not active"));
        return;
    }

    QString error;
    const auto messageKind =
      msgtype == QLatin1String("m.notice") ? QStringLiteral("notice") : QStringLiteral("text");
    const bool ok = komai::MatrixBackendRuntimeService::sendRoomMessage(
      *handleId, roomId, trimmedBody, QString::fromStdString(formattedBody), messageKind, &error);
    if (!ok) {
        if (callback)
            callback({},
                     error.isEmpty() ? QStringLiteral("failed to send matrix-sdk room message")
                                     : error);
        return;
    }

    if (callback)
        callback(QStringLiteral("queued"), {});
}

// -- media --

void
mediaFetch(const QString &mxcUri, MediaFetchCallback callback)
{
    MainWindow::instance()->imageProvider()->download(
      QString(mxcUri).remove(QStringLiteral("mxc://")),
      {96, 96},
      [callback](const QString &, const QSize &, const QImage &image, const QString &) {
          if (callback)
              callback(image);
      },
      true);
}

QJsonObject
UploadResult::toJson() const
{
    return {
      {QStringLiteral("mxcUri"), mxcUri},
      {QStringLiteral("contentType"), contentType},
      {QStringLiteral("filename"), filename},
      {QStringLiteral("size"), static_cast<qint64>(size)},
    };
}

void
mediaUpload(const QString &filePath,
            const QString &filename,
            const QString &contentType,
            MediaUploadCallback callback)
{
    Q_UNUSED(filePath);
    Q_UNUSED(filename);
    Q_UNUSED(contentType);
    if (callback) {
        callback({},
                 QStringLiteral("IPC media upload is not migrated to the matrix-sdk backend yet"));
    }
}

void
sendImageFromFile(const QString &roomIdOrAlias,
                  const QString &filePath,
                  const QString &body,
                  SendMessageCallback callback)
{
    Q_UNUSED(roomIdOrAlias);
    Q_UNUSED(filePath);
    Q_UNUSED(body);
    if (callback) {
        callback(
          {},
          QStringLiteral("IPC image upload/send is not migrated to the matrix-sdk backend yet"));
    }
}

void
sendImage(const QString &roomIdOrAlias,
          const QString &mxcUri,
          const QString &body,
          const QString &filename,
          const QJsonObject &info,
          SendMessageCallback callback)
{
    Q_UNUSED(roomIdOrAlias);
    Q_UNUSED(mxcUri);
    Q_UNUSED(body);
    Q_UNUSED(filename);
    Q_UNUSED(info);
    if (callback) {
        callback({},
                 QStringLiteral("IPC MXC image send is not migrated to the matrix-sdk backend "
                                "yet"));
    }
}

} // namespace komai::ipc

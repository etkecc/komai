// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SharedLogic.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QMimeDatabase>
#include <QPointer>
#include <QThread>

#include <optional>
#include <thread>
#include <utility>

#include "chat/ChatPage.h"
#include "config/komai.h"
#include "emoji/EmoticonReplace.h"
#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "providers/MxcImageProvider.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineViewManager.h"
#include "timeline/rust/MatrixTimelineModel.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

namespace {

struct AsyncSendResult
{
    QString eventId;
    QString error;
};

struct AsyncUploadResult
{
    komai::ipc::UploadResult result;
    QString error;
};

template<typename Callback>
void
postToAppThread(Callback callback)
{
    auto *app = QCoreApplication::instance();
    if (!app || QThread::currentThread() == app->thread()) {
        callback();
        return;
    }

    if (!QMetaObject::invokeMethod(app, callback, Qt::QueuedConnection))
        callback();
}

template<typename WorkFnT, typename UiFnT>
void
runIpcTask(WorkFnT work, UiFnT ui)
{
    std::thread([work = std::move(work), ui = std::move(ui)]() mutable {
        auto result = work();
        postToAppThread(
          [result = std::move(result), ui = std::move(ui)]() mutable { ui(std::move(result)); });
    }).detach();
}

RoomlistModel *
currentRoomlistModel()
{
    auto *chatPage = ChatPage::instance();
    if (!chatPage || !chatPage->timelineManager())
        return nullptr;
    return chatPage->timelineManager()->rooms();
}

komai::MatrixTimelineModel *
currentMatrixTimelineModel()
{
    auto *chatPage = ChatPage::instance();
    auto *manager  = chatPage ? chatPage->timelineManager() : nullptr;
    return manager ? qobject_cast<komai::MatrixTimelineModel *>(manager->matrixTimelineModel())
                   : nullptr;
}

std::optional<uint64_t>
currentMatrixRuntimeHandleId()
{
    const auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() == 0)
        return std::nullopt;

    return mainWindow->matrixBackendHandleId();
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

struct TimelineSlice
{
    QJsonArray events;
    bool hasMoreLocal         = false;
    QString nextBeforeEventId = {};
};

QString
matrixTimelineEventId(const komai::MatrixTimelineItem &item)
{
    return item.eventId.isEmpty() ? item.itemId : item.eventId;
}

QString
effectiveUploadFileName(const QFileInfo &fileInfo, const QString &filenameOverride)
{
    const auto trimmedOverride = filenameOverride.trimmed();
    if (!trimmedOverride.isEmpty())
        return trimmedOverride;

    return fileInfo.fileName().trimmed();
}

QString
effectiveMimeTypeForFile(const QString &filePath, const QString &contentTypeOverride)
{
    const auto trimmedOverride = contentTypeOverride.trimmed();
    if (!trimmedOverride.isEmpty())
        return trimmedOverride;

    const auto mime = QMimeDatabase().mimeTypeForFile(filePath, QMimeDatabase::MatchContent);
    const auto name = mime.name().trimmed();
    if (!name.isEmpty())
        return name;

    return QStringLiteral("application/octet-stream");
}

bool
shouldExposeMatrixTimelineItemOverIpc(const komai::MatrixTimelineItem &item)
{
    return item.itemKind != QStringLiteral("date_divider");
}

QString
matrixTimelineItemMsgType(const QString &itemKind)
{
    if (itemKind == QStringLiteral("notice"))
        return QStringLiteral("m.notice");
    if (itemKind == QStringLiteral("emote"))
        return QStringLiteral("m.emote");
    if (itemKind == QStringLiteral("image"))
        return QStringLiteral("m.image");
    if (itemKind == QStringLiteral("video"))
        return QStringLiteral("m.video");
    if (itemKind == QStringLiteral("audio"))
        return QStringLiteral("m.audio");
    if (itemKind == QStringLiteral("file"))
        return QStringLiteral("m.file");
    return QStringLiteral("m.text");
}

QJsonObject
serializedMatrixTimelineItem(const QString &roomId,
                             const komai::MatrixTimelineItem &item,
                             bool includeUnsignedFields)
{
    QJsonObject content{
      {QStringLiteral("body"), item.body},
      {QStringLiteral("msgtype"), matrixTimelineItemMsgType(item.itemKind)},
      {QStringLiteral("komai_item_kind"), item.itemKind},
    };

    if (!item.formattedBody.isEmpty())
        content.insert(QStringLiteral("formatted_body"), item.formattedBody);
    if (!item.mediaUrl.isEmpty())
        content.insert(QStringLiteral("url"), item.mediaUrl);
    if (!item.thumbnailUrl.isEmpty())
        content.insert(QStringLiteral("thumbnail_url"), item.thumbnailUrl);
    if (!item.fileName.isEmpty())
        content.insert(QStringLiteral("filename"), item.fileName);
    if (!item.mimeType.isEmpty())
        content.insert(QStringLiteral("mimetype"), item.mimeType);

    QJsonObject event{
      {QStringLiteral("event_id"), matrixTimelineEventId(item)},
      {QStringLiteral("room_id"), roomId},
      {QStringLiteral("sender"), item.senderId},
      {QStringLiteral("origin_server_ts"), static_cast<qint64>(item.timestamp)},
      {QStringLiteral("type"), QStringLiteral("m.room.message")},
      {QStringLiteral("content"), content},
    };

    if (includeUnsignedFields) {
        event.insert(QStringLiteral("unsigned"),
                     QJsonObject{
                       {QStringLiteral("komai_item_id"), item.itemId},
                       {QStringLiteral("delivery_state"), item.deliveryState},
                     });
    }

    return event;
}

std::optional<TimelineSlice>
sliceTimelineFromActiveMatrixTimeline(const QString &roomId,
                                      const QString &beforeEventId,
                                      const int limit,
                                      const bool includeUnsignedFields,
                                      QString *error)
{
    TimelineSlice slice;
    auto *roomlist = currentRoomlistModel();
    if (!roomlist) {
        if (error)
            *error = QStringLiteral("room list is not available");
        return std::nullopt;
    }

    if (roomlist->currentRoomId() != roomId) {
        if (error)
            *error = QStringLiteral("timeline IPC currently supports only the active room");
        return std::nullopt;
    }

    const auto *timelineModel = currentMatrixTimelineModel();
    if (!timelineModel) {
        if (error)
            *error = QStringLiteral("matrix timeline model is not available");
        return std::nullopt;
    }

    const auto items = timelineModel->visibleItemsSnapshot();

    int startIndex = 0;
    if (!beforeEventId.isEmpty()) {
        startIndex = -1;
        for (int idx = 0; idx < items.size(); ++idx) {
            if (matrixTimelineEventId(items.at(idx)) == beforeEventId) {
                startIndex = idx + 1;
                break;
            }
        }

        if (startIndex < 0) {
            if (error)
                *error = QStringLiteral("beforeEventId not found in active matrix timeline: ") +
                         beforeEventId;
            return std::nullopt;
        }
    }

    int nextIndex = startIndex;
    for (int idx = startIndex; idx < items.size(); ++idx) {
        const auto &item = items.at(idx);
        if (!shouldExposeMatrixTimelineItemOverIpc(item))
            continue;

        slice.events.append(serializedMatrixTimelineItem(roomId, item, includeUnsignedFields));
        nextIndex = idx + 1;
        if (slice.events.size() >= limit)
            break;
    }

    for (int idx = nextIndex; idx < items.size(); ++idx) {
        if (shouldExposeMatrixTimelineItemOverIpc(items.at(idx))) {
            slice.hasMoreLocal = true;
            break;
        }
    }

    if (!slice.events.isEmpty()) {
        slice.nextBeforeEventId = slice.events.at(slice.events.size() - 1)
                                    .toObject()
                                    .value(QStringLiteral("event_id"))
                                    .toString();
    }

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
      , limit_{limit}
      , beforeEventId_{beforeEventId}
      , includeUnsignedFields_{includeUnsignedFields}
      , fetchMode_{fetchMode}
      , callback_{std::move(callback)}
    {
    }

    void start()
    {
        // Try the active room's in-memory model first (fast path).
        if (refreshSlice() && (fetchMode_ != TimelineFetchMode::ServerFetchIfNeeded ||
                               slice_.events.size() >= limit_)) {
            finish();
            return;
        }

        if (fetchMode_ != TimelineFetchMode::ServerFetchIfNeeded) {
            finish();
            return;
        }

        // Server fetch: use the Rust timeline with backfill (works for any room).
        const auto *mainWindow = MainWindow::instance();
        const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
        if (handleId == 0) {
            fail(QStringLiteral("matrix backend is not available"));
            return;
        }

        QPointer<TimelineReadOperation> self(this);
        std::thread([self,
                     handleId,
                     roomId                = roomId_,
                     limit                 = limit_,
                     beforeEventId         = beforeEventId_,
                     includeUnsignedFields = includeUnsignedFields_]() {
            const auto context = komai::matrix_backend::blockingCallContext();
            QString error;
            const auto items = komai::MatrixBackendRuntimeService::fetchRoomTimeline(
              context, handleId, roomId, static_cast<uint16_t>(limit), &error);

            auto *app = QCoreApplication::instance();
            if (!app)
                return;

            QMetaObject::invokeMethod(
              app,
              [self, roomId, beforeEventId, includeUnsignedFields, items, error]() {
                  if (!self)
                      return;

                  if (!items) {
                      self->fail(error.isEmpty() ? QStringLiteral("failed to fetch room timeline")
                                                 : error);
                      return;
                  }

                  TimelineSlice slice;
                  bool pastBefore = beforeEventId.isEmpty();
                  for (const auto &item : *items) {
                      if (!pastBefore) {
                          if (matrixTimelineEventId(item) == beforeEventId)
                              pastBefore = true;
                          continue;
                      }
                      if (!shouldExposeMatrixTimelineItemOverIpc(item))
                          continue;
                      slice.events.append(
                        serializedMatrixTimelineItem(roomId, item, includeUnsignedFields));
                      if (slice.events.size() >= self->limit_)
                          break;
                  }

                  self->slice_ = std::move(slice);
                  self->finish();
              },
              Qt::QueuedConnection);
        }).detach();
    }

private:
    bool refreshSlice()
    {
        QString error;
        const auto slice = sliceTimelineFromActiveMatrixTimeline(
          roomId_, beforeEventId_, limit_, includeUnsignedFields_, &error);
        if (!slice.has_value())
            return false;

        slice_ = *slice;
        return true;
    }

    void finish()
    {
        const bool hasMore = slice_.hasMoreLocal;
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
    int limit_;
    QString beforeEventId_;
    bool includeUnsignedFields_;
    TimelineFetchMode fetchMode_;
    komai::ipc::ReadTimelineCallback callback_;
    TimelineSlice slice_;
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
RoomInfo::toJson(const QStringList &fields) const
{
    const QJsonObject all{
      {QStringLiteral("id"), roomId},
      {QStringLiteral("alias"), alias},
      {QStringLiteral("name"), name},
      {QStringLiteral("avatarUrl"), avatarUrl},
      {QStringLiteral("read"), read},
      {QStringLiteral("unreadCount"), unreadCount},
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

    if (fields.isEmpty())
        return all;

    QJsonObject projected;
    for (const auto &field : fields) {
        const auto it = all.find(field);
        if (it != all.end())
            projected.insert(field, it.value());
    }
    return projected;
}

QStringList
RoomInfo::fieldNames()
{
    return RoomInfo{}.toJson().keys();
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

        const auto joinedRoomIt = rl->matrixJoinedRooms().constFind(roomId);
        const auto memberCount =
          joinedRoomIt != rl->matrixJoinedRooms().cend() ? joinedRoomIt->memberCount : 0;

        RoomInfo info;
        info.roomId = roomId;
        info.alias =
          joinedRoomIt != rl->matrixJoinedRooms().cend() ? joinedRoomIt->roomAlias : QString{};
        info.name        = rl->data(index, RoomlistModel::RoomName).toString();
        info.avatarUrl   = rl->data(index, RoomlistModel::AvatarUrl).toString();
        info.read        = !rl->data(index, RoomlistModel::HasUnreadMessages).toBool();
        info.unreadCount = rl->data(index, RoomlistModel::UnreadCount).toInt();
        info.memberCount = memberCount;
        info.mostRecentEventTimestampMs = rl->data(index, RoomlistModel::Timestamp).toULongLong();
        info.highlighted  = rl->data(index, RoomlistModel::HasLoudNotification).toBool();
        info.categories   = roomCategories(rl, index);
        info.tags         = rl->data(index, RoomlistModel::Tags).toStringList();
        info.parentSpaces = rl->data(index, RoomlistModel::ParentSpaces).toStringList();
        info.directUserId = rl->data(index, RoomlistModel::DirectChatOtherUserId).toString();
        info.encrypted    = rl->data(index, RoomlistModel::IsEncrypted).toBool();
        result.push_back(std::move(info));
    }

    return result;
}

namespace {

/// Whether `info` survives every filter set on `query`.
bool
roomMatchesQuery(const RoomInfo &info, const RoomListQuery &query)
{
    if (!query.ids.isEmpty() && !query.ids.contains(info.roomId) &&
        (info.alias.isEmpty() || !query.ids.contains(info.alias))) {
        return false;
    }

    if (!query.query.isEmpty() && !info.name.contains(query.query, Qt::CaseInsensitive) &&
        !info.alias.contains(query.query, Qt::CaseInsensitive)) {
        return false;
    }

    // "direct" is derived alongside the other categories, and is the same
    // signal the room list itself uses.
    if (query.isDm.has_value() &&
        info.categories.contains(QStringLiteral("direct")) != *query.isDm) {
        return false;
    }

    if (query.encrypted.has_value() && info.encrypted != *query.encrypted)
        return false;

    if (!query.parentSpace.isEmpty() && !info.parentSpaces.contains(query.parentSpace))
        return false;

    if (!query.tag.isEmpty() && !info.tags.contains(query.tag))
        return false;

    if (query.minMemberCount.has_value() && info.memberCount < *query.minMemberCount)
        return false;

    return true;
}

} // namespace

std::variant<RoomListPage, QString>
roomList(const RoomListQuery &query)
{
    for (const auto &field : query.fields) {
        if (!RoomInfo::fieldNames().contains(field)) {
            return QStringLiteral("unknown field '%1'; known fields are: %2")
              .arg(field, RoomInfo::fieldNames().join(QStringLiteral(", ")));
        }
    }

    if (query.offset < 0)
        return QStringLiteral("offset must not be negative");

    RoomListPage page;
    for (const auto &info : roomList()) {
        if (!roomMatchesQuery(info, query))
            continue;

        // matchCount counts every match; the page skips and stops around it.
        const int matchIndex = page.matchCount++;
        if (matchIndex < query.offset)
            continue;
        if (query.limit >= 0 && page.rooms.size() >= query.limit)
            continue;

        page.rooms.push_back(info);
    }

    return page;
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

    auto *roomlist = currentRoomlistModel();
    if (!roomlist)
        return {};

    const auto &joinedRooms = roomlist->matrixJoinedRooms();
    for (int row = 0; row < roomlist->rowCount(); ++row) {
        const auto roomId =
          roomlist->data(roomlist->index(row, 0), RoomlistModel::RoomId).toString();
        if (roomId == roomIdOrAlias)
            return roomId;

        const auto joinedRoomIt = joinedRooms.constFind(roomId);
        if (joinedRoomIt != joinedRooms.cend() && joinedRoomIt->roomAlias == roomIdOrAlias)
            return roomId;
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

    const auto trimmedBody = emoji::replaceEmoticons(
      body.trimmed(), UserSettings::instance()->composerInputAutoReplaceEmoji());
    if (trimmedBody.isEmpty()) {
        if (callback)
            callback({}, QStringLiteral("message body must not be empty"));
        return;
    }

    bool useMarkdownFormatting = false;
    if (format == QLatin1String("html")) {
        useMarkdownFormatting = true;
    } else if (format == QLatin1String("auto")) {
        if (UserSettings::instance()->composerInputMarkdownToHtmlEnabled())
            useMarkdownFormatting = true;
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
    runIpcTask(
      [handleId = *handleId, roomId, trimmedBody, useMarkdownFormatting, messageKind]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          AsyncSendResult result;
          QString error;
          // Automation callers need the real event ID to correlate a send with
          // what comes back, so they bypass the offline send queue.
          const auto eventId =
            komai::MatrixBackendRuntimeService::sendRoomMessage(context,
                                                                handleId,
                                                                roomId,
                                                                trimmedBody,
                                                                useMarkdownFormatting,
                                                                messageKind,
                                                                MatrixSendMode::Direct,
                                                                QString(),
                                                                false,
                                                                &error);
          if (eventId.has_value()) {
              result.eventId = *eventId;
          } else {
              result.error =
                error.isEmpty() ? QStringLiteral("failed to send matrix-sdk room message") : error;
          }

          return result;
      },
      [callback = std::move(callback)](AsyncSendResult result) mutable {
          if (callback)
              callback(result.eventId, result.error);
      });
}

// -- rooms (creation) --

namespace {

struct AsyncCreateRoomResult
{
    QString roomId;
    QString error;
};

std::optional<MatrixCreateRoomPreset>
parseCreateRoomPreset(const QString &value)
{
    const auto normalized = value.trimmed();
    if (normalized.isEmpty() || normalized == QLatin1String("private_chat"))
        return MatrixCreateRoomPreset::PrivateChat;
    if (normalized == QLatin1String("public_chat"))
        return MatrixCreateRoomPreset::PublicChat;
    if (normalized == QLatin1String("trusted_private_chat"))
        return MatrixCreateRoomPreset::TrustedPrivateChat;
    return std::nullopt;
}

/// Serializes a pass-through JSON value, or returns an empty string when there
/// is nothing to send.
template<typename JsonT>
QString
compactJsonOrEmpty(const JsonT &value)
{
    if (value.isEmpty())
        return {};

    return QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact));
}

} // namespace

void
createRoom(const CreateRoomRequest &request, CreateRoomCallback callback)
{
    const auto preset = parseCreateRoomPreset(request.preset);
    if (!preset.has_value()) {
        if (callback) {
            callback({},
                     QStringLiteral("preset must be one of: private_chat, public_chat, "
                                    "trusted_private_chat"));
        }
        return;
    }

    const auto aliasLocalpart = request.aliasLocalpart.trimmed();
    if (aliasLocalpart.contains(QLatin1Char(':')) || aliasLocalpart.contains(QLatin1Char('#'))) {
        if (callback) {
            callback({},
                     QStringLiteral("aliasLocalpart must be the local part only, without '#' or "
                                    "':': ") +
                       aliasLocalpart);
        }
        return;
    }

    for (const auto &userId : request.inviteUserIds) {
        if (!userId.trimmed().startsWith(QLatin1Char('@'))) {
            if (callback) {
                callback({},
                         QStringLiteral("invite entries must be fully-qualified Matrix IDs: ") +
                           userId);
            }
            return;
        }
    }

    const auto handleId = currentMatrixRuntimeHandleId();
    if (!handleId.has_value()) {
        if (callback)
            callback({}, QStringLiteral("matrix-sdk runtime is not active"));
        return;
    }

    MatrixCreateRoomRequest serviceRequest;
    serviceRequest.name               = request.name.trimmed();
    serviceRequest.topic              = request.topic.trimmed();
    serviceRequest.roomAliasLocalpart = aliasLocalpart;
    serviceRequest.preset             = *preset;
    serviceRequest.isDirect           = request.isDirect;
    serviceRequest.isEncrypted        = request.isEncrypted;
    serviceRequest.isSpace            = request.isSpace;
    serviceRequest.isPublic           = request.isPublic;
    serviceRequest.roomVersion        = request.roomVersion.trimmed();
    serviceRequest.powerLevelContentOverrideJson =
      compactJsonOrEmpty(request.powerLevelContentOverride);
    serviceRequest.initialStateJson    = compactJsonOrEmpty(request.initialState);
    serviceRequest.creationContentJson = compactJsonOrEmpty(request.creationContent);

    serviceRequest.inviteUserIds.reserve(request.inviteUserIds.size());
    for (const auto &userId : request.inviteUserIds)
        serviceRequest.inviteUserIds.push_back(userId.trimmed());

    runIpcTask(
      [handleId = *handleId, serviceRequest]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          AsyncCreateRoomResult result;
          QString error;
          const auto roomId = komai::MatrixBackendRuntimeService::createRoom(
            context, handleId, serviceRequest, &error);
          if (roomId.has_value())
              result.roomId = *roomId;
          else
              result.error = error.isEmpty() ? QStringLiteral("failed to create room") : error;

          return result;
      },
      [callback = std::move(callback)](AsyncCreateRoomResult result) mutable {
          if (callback)
              callback(result.roomId, result.error);
      });
}

// -- rooms (membership) --

namespace {

using MembershipServiceFn = bool (*)(komai::matrix_backend::BlockingCallContext,
                                     uint64_t,
                                     const QString &,
                                     const QString &,
                                     const QString &,
                                     QString *);

/// Shared plumbing for the four user-targeting membership operations, which
/// differ only in which matrix-sdk call they dispatch to.
void
runMembershipAction(const QString &roomIdOrAlias,
                    const QString &userId,
                    const QString &reason,
                    const char *actionName,
                    MembershipServiceFn serviceFn,
                    komai::ipc::RoomActionCallback callback)
{
    const auto roomId = komai::ipc::resolveRoomId(roomIdOrAlias);
    if (roomId.isEmpty()) {
        if (callback)
            callback(QStringLiteral("room not found: ") + roomIdOrAlias);
        return;
    }

    const auto trimmedUserId = userId.trimmed();
    if (!trimmedUserId.startsWith(QLatin1Char('@'))) {
        if (callback) {
            callback(QStringLiteral("user ID must be a fully-qualified Matrix ID: ") +
                     trimmedUserId);
        }
        return;
    }

    const auto handleId = currentMatrixRuntimeHandleId();
    if (!handleId.has_value()) {
        if (callback)
            callback(QStringLiteral("matrix-sdk runtime is not active"));
        return;
    }

    runIpcTask(
      [handleId = *handleId,
       roomId,
       trimmedUserId,
       trimmedReason = reason.trimmed(),
       actionName,
       serviceFn]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          if (serviceFn(context, handleId, roomId, trimmedUserId, trimmedReason, &error))
              return QString{};

          if (!error.isEmpty())
              return error;

          return QStringLiteral("failed to %1 %2 in %3")
            .arg(QString::fromLatin1(actionName), trimmedUserId, roomId);
      },
      [callback = std::move(callback)](QString error) mutable {
          if (callback)
              callback(error);
      });
}

} // namespace

void
inviteUser(const QString &roomIdOrAlias,
           const QString &userId,
           const QString &reason,
           RoomActionCallback callback)
{
    runMembershipAction(roomIdOrAlias,
                        userId,
                        reason,
                        "invite",
                        &komai::MatrixBackendRuntimeService::inviteUser,
                        std::move(callback));
}

void
kickUser(const QString &roomIdOrAlias,
         const QString &userId,
         const QString &reason,
         RoomActionCallback callback)
{
    runMembershipAction(roomIdOrAlias,
                        userId,
                        reason,
                        "kick",
                        &komai::MatrixBackendRuntimeService::kickUser,
                        std::move(callback));
}

void
banUser(const QString &roomIdOrAlias,
        const QString &userId,
        const QString &reason,
        RoomActionCallback callback)
{
    runMembershipAction(roomIdOrAlias,
                        userId,
                        reason,
                        "ban",
                        &komai::MatrixBackendRuntimeService::banUser,
                        std::move(callback));
}

void
unbanUser(const QString &roomIdOrAlias,
          const QString &userId,
          const QString &reason,
          RoomActionCallback callback)
{
    runMembershipAction(roomIdOrAlias,
                        userId,
                        reason,
                        "unban",
                        &komai::MatrixBackendRuntimeService::unbanUser,
                        std::move(callback));
}

void
leaveRoom(const QString &roomIdOrAlias, const QString &reason, RoomActionCallback callback)
{
    const auto roomId = resolveRoomId(roomIdOrAlias);
    if (roomId.isEmpty()) {
        if (callback)
            callback(QStringLiteral("room not found: ") + roomIdOrAlias);
        return;
    }

    const auto handleId = currentMatrixRuntimeHandleId();
    if (!handleId.has_value()) {
        if (callback)
            callback(QStringLiteral("matrix-sdk runtime is not active"));
        return;
    }

    runIpcTask(
      [handleId = *handleId, roomId, trimmedReason = reason.trimmed()]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          if (komai::MatrixBackendRuntimeService::leaveRoom(
                context, handleId, roomId, trimmedReason, &error)) {
              return QString{};
          }

          return error.isEmpty() ? QStringLiteral("failed to leave ") + roomId : error;
      },
      [callback = std::move(callback)](QString error) mutable {
          if (callback)
              callback(error);
      });
}

// -- rooms (state) --

namespace {

struct AsyncStateReadResult
{
    komai::ipc::StateEventResult result;
    QString error;
};

/// Resolves the room and runtime handle both state calls need, reporting the
/// same failures the rest of the surface reports.
template<typename FailFnT>
std::optional<std::pair<QString, uint64_t>>
resolveStateTarget(const QString &roomIdOrAlias, const QString &eventType, FailFnT fail)
{
    const auto roomId = komai::ipc::resolveRoomId(roomIdOrAlias);
    if (roomId.isEmpty()) {
        fail(QStringLiteral("room not found: ") + roomIdOrAlias);
        return std::nullopt;
    }

    if (eventType.trimmed().isEmpty()) {
        fail(QStringLiteral("eventType must not be empty"));
        return std::nullopt;
    }

    const auto handleId = currentMatrixRuntimeHandleId();
    if (!handleId.has_value()) {
        fail(QStringLiteral("matrix-sdk runtime is not active"));
        return std::nullopt;
    }

    return std::make_pair(roomId, *handleId);
}

} // namespace

void
readStateEvent(const QString &roomIdOrAlias,
               const QString &eventType,
               const QString &stateKey,
               ReadStateCallback callback)
{
    const auto target =
      resolveStateTarget(roomIdOrAlias, eventType, [&callback](const QString &error) {
          if (callback)
              callback({}, error);
      });
    if (!target.has_value())
        return;

    runIpcTask(
      [handleId    = target->second,
       roomId      = target->first,
       trimmedType = eventType.trimmed(),
       stateKey]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          AsyncStateReadResult async;
          QString error;
          const auto fetched = komai::MatrixBackendRuntimeService::fetchRoomStateEvent(
            context, handleId, roomId, trimmedType, stateKey, &error);
          if (!fetched.has_value()) {
              async.error = error.isEmpty() ? QStringLiteral("failed to read room state") : error;
              return async;
          }

          async.result.exists = fetched->exists;
          if (fetched->exists) {
              const auto doc = QJsonDocument::fromJson(fetched->contentJson.toUtf8());
              if (!doc.isObject()) {
                  async.error = QStringLiteral("homeserver returned a non-object state content");
                  return async;
              }
              async.result.content = doc.object();
          }

          return async;
      },
      [callback = std::move(callback)](AsyncStateReadResult async) mutable {
          if (callback)
              callback(async.result, async.error);
      });
}

void
sendStateEvent(const QString &roomIdOrAlias,
               const QString &eventType,
               const QString &stateKey,
               const QJsonObject &content,
               SendMessageCallback callback)
{
    const auto target =
      resolveStateTarget(roomIdOrAlias, eventType, [&callback](const QString &error) {
          if (callback)
              callback({}, error);
      });
    if (!target.has_value())
        return;

    const auto contentJson =
      QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact));

    runIpcTask(
      [handleId    = target->second,
       roomId      = target->first,
       trimmedType = eventType.trimmed(),
       stateKey,
       contentJson]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          AsyncSendResult result;
          QString error;
          const auto eventId = komai::MatrixBackendRuntimeService::sendRoomStateEvent(
            context, handleId, roomId, trimmedType, stateKey, contentJson, &error);
          if (eventId.has_value())
              result.eventId = *eventId;
          else
              result.error = error.isEmpty() ? QStringLiteral("failed to send room state") : error;

          return result;
      },
      [callback = std::move(callback)](AsyncSendResult result) mutable {
          if (callback)
              callback(result.eventId, result.error);
      });
}

namespace {

/// Shared plumbing for the named setters, which differ only in the value they
/// carry and the runtime call they make.
template<typename WorkFnT>
void
runRoomSettingAction(const QString &roomIdOrAlias,
                     komai::ipc::RoomActionCallback callback,
                     WorkFnT makeWork)
{
    const auto roomId = komai::ipc::resolveRoomId(roomIdOrAlias);
    if (roomId.isEmpty()) {
        if (callback)
            callback(QStringLiteral("room not found: ") + roomIdOrAlias);
        return;
    }

    const auto handleId = currentMatrixRuntimeHandleId();
    if (!handleId.has_value()) {
        if (callback)
            callback(QStringLiteral("matrix-sdk runtime is not active"));
        return;
    }

    runIpcTask(makeWork(roomId, *handleId),
               [callback = std::move(callback)](QString error) mutable {
                   if (callback)
                       callback(error);
               });
}

} // namespace

void
setRoomName(const QString &roomIdOrAlias, const QString &name, RoomActionCallback callback)
{
    runRoomSettingAction(
      roomIdOrAlias, std::move(callback), [name](const QString &roomId, uint64_t handleId) {
          return [roomId, handleId, name]() {
              const auto context = komai::matrix_backend::blockingCallContext();
              QString error;
              if (komai::MatrixBackendRuntimeService::setRoomName(
                    context, handleId, roomId, name, &error)) {
                  return QString{};
              }
              return error.isEmpty() ? QStringLiteral("failed to set the room name") : error;
          };
      });
}

void
setRoomTopic(const QString &roomIdOrAlias, const QString &topic, RoomActionCallback callback)
{
    runRoomSettingAction(
      roomIdOrAlias, std::move(callback), [topic](const QString &roomId, uint64_t handleId) {
          return [roomId, handleId, topic]() {
              const auto context = komai::matrix_backend::blockingCallContext();
              QString error;
              if (komai::MatrixBackendRuntimeService::setRoomTopic(
                    context, handleId, roomId, topic, &error)) {
                  return QString{};
              }
              return error.isEmpty() ? QStringLiteral("failed to set the room topic") : error;
          };
      });
}

void
setUserPowerLevel(const QString &roomIdOrAlias,
                  const QString &userId,
                  const int powerLevel,
                  RoomActionCallback callback)
{
    const auto trimmedUserId = userId.trimmed();
    if (!trimmedUserId.startsWith(QLatin1Char('@'))) {
        if (callback) {
            callback(QStringLiteral("user ID must be a fully-qualified Matrix ID: ") +
                     trimmedUserId);
        }
        return;
    }

    runRoomSettingAction(
      roomIdOrAlias,
      std::move(callback),
      [trimmedUserId, powerLevel](const QString &roomId, uint64_t handleId) {
          return [roomId, handleId, trimmedUserId, powerLevel]() {
              const auto context = komai::matrix_backend::blockingCallContext();
              QString error;
              if (komai::MatrixBackendRuntimeService::setUserPowerLevel(
                    context, handleId, roomId, trimmedUserId, powerLevel, &error)) {
                  return QString{};
              }
              return error.isEmpty() ? QStringLiteral("failed to set the power level") : error;
          };
      });
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
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        if (callback)
            callback({}, QStringLiteral("file not found: ") + filePath);
        return;
    }

    const auto handleId = currentMatrixRuntimeHandleId();
    if (!handleId.has_value()) {
        if (callback)
            callback({}, QStringLiteral("matrix-sdk runtime is not active"));
        return;
    }

    const auto effectiveFilename = effectiveUploadFileName(fileInfo, filename);
    if (effectiveFilename.isEmpty()) {
        if (callback)
            callback({}, QStringLiteral("file path does not include a file name: ") + filePath);
        return;
    }

    const auto effectiveContentType =
      effectiveMimeTypeForFile(fileInfo.absoluteFilePath(), contentType);
    const auto effectiveFilePath = fileInfo.absoluteFilePath();
    const auto fileSize          = static_cast<uint64_t>(fileInfo.size());
    const bool stripImageMetadata =
      UserSettings::instance()->composerAttachmentsStripImageMetadata();

    runIpcTask(
      [handleId = *handleId,
       effectiveFilePath,
       effectiveFilename,
       effectiveContentType,
       fileSize,
       stripImageMetadata]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          AsyncUploadResult result;
          QString error;
          const auto mxcUri = komai::MatrixBackendRuntimeService::uploadMedia(
            context, handleId, effectiveFilePath, effectiveContentType, stripImageMetadata, &error);
          if (!mxcUri.has_value()) {
              result.error =
                error.isEmpty() ? QStringLiteral("failed to upload matrix media") : error;
              return result;
          }

          result.result = komai::ipc::UploadResult{
            .mxcUri      = *mxcUri,
            .contentType = effectiveContentType,
            .filename    = effectiveFilename,
            .size        = fileSize,
          };
          return result;
      },
      [callback = std::move(callback)](AsyncUploadResult result) mutable {
          if (callback)
              callback(result.result, result.error);
      });
}

void
sendImageFromFile(const QString &roomIdOrAlias,
                  const QString &filePath,
                  const QString &body,
                  SendMessageCallback callback)
{
    const auto roomId = resolveRoomId(roomIdOrAlias);
    if (roomId.isEmpty()) {
        if (callback)
            callback({}, QStringLiteral("room not found: ") + roomIdOrAlias);
        return;
    }

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        if (callback)
            callback({}, QStringLiteral("file not found: ") + filePath);
        return;
    }

    const auto handleId = currentMatrixRuntimeHandleId();
    if (!handleId.has_value()) {
        if (callback)
            callback({}, QStringLiteral("matrix-sdk runtime is not active"));
        return;
    }

    const auto effectiveFilePath = fileInfo.absoluteFilePath();
    const auto mimeType          = QMimeDatabase()
                            .mimeTypeForFile(effectiveFilePath, QMimeDatabase::MatchContent)
                            .name()
                            .trimmed();
    if (!mimeType.startsWith(QStringLiteral("image/"), Qt::CaseInsensitive)) {
        if (callback)
            callback({}, QStringLiteral("file is not an image: ") + effectiveFilePath);
        return;
    }

    const auto effectiveFilename = effectiveUploadFileName(fileInfo, {});
    if (effectiveFilename.isEmpty()) {
        if (callback)
            callback({}, QStringLiteral("file path does not include a file name: ") + filePath);
        return;
    }

    const auto trimmedBody = body.trimmed();
    const bool stripImageMetadata =
      UserSettings::instance()->composerAttachmentsStripImageMetadata();
    runIpcTask(
      [handleId = *handleId,
       roomId,
       effectiveFilePath,
       effectiveFilename,
       trimmedBody,
       mimeType,
       stripImageMetadata]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          AsyncSendResult result;
          QString error;
          // IPC callers ship their own bodies verbatim; don't reinterpret
          // them as Markdown.
          const bool useMarkdownFormatting = false;
          const auto eventId =
            komai::MatrixBackendRuntimeService::sendRoomAttachment(context,
                                                                   handleId,
                                                                   roomId,
                                                                   effectiveFilePath,
                                                                   effectiveFilename,
                                                                   trimmedBody,
                                                                   useMarkdownFormatting,
                                                                   {},
                                                                   {},
                                                                   mimeType,
                                                                   0,
                                                                   false,
                                                                   {},
                                                                   stripImageMetadata,
                                                                   &error);
          if (eventId.has_value()) {
              result.eventId = *eventId;
          } else {
              result.error = error.isEmpty()
                               ? QStringLiteral("failed to send matrix-sdk room image attachment")
                               : error;
          }
          return result;
      },
      [callback = std::move(callback)](AsyncSendResult result) mutable {
          if (callback)
              callback(result.eventId, result.error);
      });
}

void
sendImage(const QString &roomIdOrAlias,
          const QString &mxcUri,
          const QString &body,
          const QString &filename,
          const QJsonObject &info,
          SendMessageCallback callback)
{
    const auto roomId = resolveRoomId(roomIdOrAlias);
    if (roomId.isEmpty()) {
        if (callback)
            callback({}, QStringLiteral("room not found: ") + roomIdOrAlias);
        return;
    }

    const auto normalizedMxcUri = komai::matrix::normalizeMxcUri(mxcUri);
    if (!normalizedMxcUri.startsWith(QStringLiteral("mxc://"))) {
        if (callback)
            callback({}, QStringLiteral("invalid mxc uri: ") + mxcUri);
        return;
    }

    const auto handleId = currentMatrixRuntimeHandleId();
    if (!handleId.has_value()) {
        if (callback)
            callback({}, QStringLiteral("matrix-sdk runtime is not active"));
        return;
    }

    const auto trimmedBody     = body.trimmed();
    const auto trimmedFilename = filename.trimmed();
    const auto infoJson        = info.isEmpty()
                                   ? QString{}
                                   : QString::fromUtf8(QJsonDocument(info).toJson(QJsonDocument::Compact));

    runIpcTask(
      [handleId = *handleId, roomId, normalizedMxcUri, trimmedBody, trimmedFilename, infoJson]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          AsyncSendResult result;
          QString error;
          const auto eventId =
            komai::MatrixBackendRuntimeService::sendRoomImage(context,
                                                              handleId,
                                                              roomId,
                                                              normalizedMxcUri,
                                                              trimmedBody,
                                                              trimmedFilename,
                                                              infoJson,
                                                              MatrixSendMode::Direct,
                                                              &error);
          if (eventId.has_value()) {
              result.eventId = *eventId;
          } else {
              result.error =
                error.isEmpty() ? QStringLiteral("failed to send matrix-sdk room image") : error;
          }
          return result;
      },
      [callback = std::move(callback)](AsyncSendResult result) mutable {
          if (callback)
              callback(result.eventId, result.error);
      });
}

} // namespace komai::ipc

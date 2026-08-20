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
RoomInfo::toJson() const
{
    return {
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
          if (QString error;
              komai::MatrixBackendRuntimeService::sendRoomMessage(context,
                                                                  handleId,
                                                                  roomId,
                                                                  trimmedBody,
                                                                  useMarkdownFormatting,
                                                                  messageKind,
                                                                  QString(),
                                                                  false,
                                                                  &error)) {
              result.eventId = QStringLiteral("queued");
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
          const bool ok =
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
          if (ok) {
              result.eventId = QStringLiteral("queued");
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
          const bool ok = komai::MatrixBackendRuntimeService::sendRoomImage(context,
                                                                            handleId,
                                                                            roomId,
                                                                            normalizedMxcUri,
                                                                            trimmedBody,
                                                                            trimmedFilename,
                                                                            infoJson,
                                                                            &error);
          if (ok) {
              result.eventId = QStringLiteral("queued");
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

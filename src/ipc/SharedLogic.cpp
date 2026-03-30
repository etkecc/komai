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

#include <mtx/responses/media.hpp>

#include "blurhash.hpp"

#include "chat/ChatPage.h"
#include "config/komai.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "providers/MxcImageProvider.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"
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

    const auto handleId = currentMatrixRuntimeHandleId();
    if (!handleId.has_value()) {
        if (error)
            *error = QStringLiteral("matrix-sdk runtime is not active");
        return std::nullopt;
    }

    QString backendError;
    const auto items =
      komai::MatrixBackendRuntimeService::fetchActiveRoomTimeline(*handleId, &backendError);
    if (!items.has_value()) {
        if (error)
            *error = backendError.isEmpty() ? QStringLiteral("failed to fetch active room timeline")
                                            : backendError;
        return std::nullopt;
    }

    int startIndex = 0;
    if (!beforeEventId.isEmpty()) {
        startIndex = -1;
        for (int idx = 0; idx < items->size(); ++idx) {
            if (matrixTimelineEventId(items->at(idx)) == beforeEventId) {
                startIndex = idx + 1;
                break;
            }
        }

        if (startIndex < 0) {
            if (error)
                *error =
                  QStringLiteral("beforeEventId not found in active matrix timeline: ") +
                  beforeEventId;
            return std::nullopt;
        }
    }

    int nextIndex = startIndex;
    for (int idx = startIndex; idx < items->size(); ++idx) {
        const auto &item = items->at(idx);
        if (!shouldExposeMatrixTimelineItemOverIpc(item))
            continue;

        slice.events.append(serializedMatrixTimelineItem(roomId, item, includeUnsignedFields));
        nextIndex = idx + 1;
        if (slice.events.size() >= limit)
            break;
    }

    for (int idx = nextIndex; idx < items->size(); ++idx) {
        if (shouldExposeMatrixTimelineItemOverIpc(items->at(idx))) {
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
        if (!refreshSlice())
            return;

        if (fetchMode_ == TimelineFetchMode::ServerFetchIfNeeded) {
            nhlog::ui()->debug("IPC timeline read requested server fetch for room '{}', but this "
                               "migration branch is cache-only here",
                               roomId_.toStdString());
        }

        finish();
    }

private:
    bool refreshSlice()
    {
        QString error;
        const auto slice = sliceTimelineFromActiveMatrixTimeline(
          roomId_, beforeEventId_, limit_, includeUnsignedFields_, &error);
        if (!slice.has_value()) {
            fail(error);
            return false;
        }

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
          .alias                      = {},
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

    auto *roomlist = currentRoomlistModel();
    if (!roomlist)
        return {};

    for (int row = 0; row < roomlist->rowCount(); ++row) {
        const auto roomId = roomlist->data(roomlist->index(row, 0), RoomlistModel::RoomId).toString();
        if (roomId == roomIdOrAlias)
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

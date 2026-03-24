// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SharedLogic.h"

#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QMimeDatabase>

#include <mtx/events/collections.hpp>
#include <mtx/responses/media.hpp>
#include <mtxclient/crypto/utils.hpp>

#include "blurhash.hpp"

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "config/komai.h"
#include "encryption/Olm.h"
#include "matrix/MatrixClient.h"
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
    return QString::fromStdString(http::client()->user_id().to_string());
}

QString
homeserverUrl()
{
    return QString::fromStdString(http::client()->server_url());
}

QString
deviceId()
{
    return QString::fromStdString(http::client()->device_id());
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

    const auto roomIdStd = roomId.toStdString();

    // Build formatted_body based on format parameter.
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
    // "plain" or anything else: no formatted_body

    // Smart-skip: if formatted_body has no HTML tags and body has no newlines
    // or backslashes, clear it to avoid redundant formatting.
    if (!formattedBody.empty() && formattedBody.find('<') == std::string::npos &&
        trimmedBody.toStdString().find('\n') == std::string::npos &&
        trimmedBody.toStdString().find('\\') == std::string::npos)
        formattedBody.clear();

    const auto txnId     = std::string("m") + std::to_string(QDateTime::currentMSecsSinceEpoch());
    const bool encrypted = cache::isRoomEncrypted(roomIdStd);

    auto doSend = [roomIdStd, txnId, encrypted, callback](auto content) {
        auto sendCb = [callback](const mtx::responses::EventId &eventId,
                                 mtx::http::RequestErr err) {
            if (err) {
                if (callback)
                    callback({}, QString::fromStdString(err->matrix_error.error));
                return;
            }
            if (callback)
                callback(QString::fromStdString(eventId.event_id.to_string()), {});
        };

        if (encrypted) {
            try {
                nlohmann::json doc = {{"type", "m.room.message"},
                                      {"content", nlohmann::json(content)},
                                      {"room_id", roomIdStd}};
                auto encryptedContent =
                  olm::encrypt_group_message(roomIdStd, http::client()->device_id(), doc);
                http::client()->send_room_message(roomIdStd, txnId, encryptedContent, sendCb);
            } catch (const std::exception &e) {
                if (callback)
                    callback(
                      {}, QStringLiteral("encryption failed: ") + QString::fromStdString(e.what()));
            }
        } else {
            http::client()->send_room_message(roomIdStd, txnId, content, sendCb);
        }
    };

    if (msgtype == QLatin1String("m.notice")) {
        mtx::events::msg::Notice notice;
        notice.body = trimmedBody.toStdString();
        if (!formattedBody.empty()) {
            notice.formatted_body = formattedBody;
            notice.format         = "org.matrix.custom.html";
        }
        doSend(notice);
    } else {
        mtx::events::msg::Text text;
        text.body = trimmedBody.toStdString();
        if (!formattedBody.empty()) {
            text.formatted_body = formattedBody;
            text.format         = "org.matrix.custom.html";
        }
        doSend(text);
    }
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
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (callback)
            callback({}, QStringLiteral("cannot open file: ") + filePath);
        return;
    }

    const auto data = file.readAll();
    file.close();

    const auto resolvedFilename = filename.isEmpty() ? QFileInfo(filePath).fileName() : filename;

    auto resolvedContentType = contentType;
    if (resolvedContentType.isEmpty()) {
        QMimeDatabase db;
        resolvedContentType = db.mimeTypeForFileNameAndData(resolvedFilename, data).name();
    }

    const auto payload = std::string(data.constData(), data.size());

    http::client()->upload(payload,
                           resolvedContentType.toStdString(),
                           resolvedFilename.toStdString(),
                           [callback,
                            resolvedFilename,
                            resolvedContentType,
                            fileSize = static_cast<uint64_t>(data.size())](
                             const mtx::responses::ContentURI &res, mtx::http::RequestErr err) {
                               if (err) {
                                   if (callback)
                                       callback({},
                                                QString::fromStdString(err->matrix_error.error));
                                   return;
                               }
                               if (callback) {
                                   UploadResult result;
                                   result.mxcUri      = QString::fromStdString(res.content_uri);
                                   result.contentType = resolvedContentType;
                                   result.filename    = resolvedFilename;
                                   result.size        = fileSize;
                                   callback(result, {});
                               }
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

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (callback)
            callback({}, QStringLiteral("cannot open file: ") + filePath);
        return;
    }

    const auto fileData = file.readAll();
    file.close();

    const auto fname = QFileInfo(filePath).fileName();

    QMimeDatabase mimeDb;
    const auto mime = mimeDb.mimeTypeForFileNameAndData(fname, fileData).name();

    // Read image for dimensions and blurhash.
    QImage img            = utils::readImage(fileData);
    const auto dimensions = img.size();

    // Compute blurhash on a scaled-down version.
    QString blurhashStr;
    if (!img.isNull()) {
        QImage bhImg = img;
        if (bhImg.height() > 200 && bhImg.width() > 360)
            bhImg = bhImg.scaled(360, 200, Qt::KeepAspectRatioByExpanding);
        std::vector<unsigned char> rgbData;
        for (int y = 0; y < bhImg.height(); y++) {
            for (int x = 0; x < bhImg.width(); x++) {
                auto p = bhImg.pixel(x, y);
                rgbData.push_back(static_cast<unsigned char>(qRed(p)));
                rgbData.push_back(static_cast<unsigned char>(qGreen(p)));
                rgbData.push_back(static_cast<unsigned char>(qBlue(p)));
            }
        }
        blurhashStr = QString::fromStdString(
          blurhash::encode(rgbData.data(), bhImg.width(), bhImg.height(), 4, 3));
    }

    // Generate thumbnail (scale to max 800x800 if larger, save as PNG).
    QByteArray thumbnailData;
    QSize thumbnailDimensions;
    if (!img.isNull() && (img.width() > 800 || img.height() > 800)) {
        QImage thumbImg = img.scaled(std::min(800, img.width()),
                                     std::min(800, img.height()),
                                     Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);
        QBuffer thumbBuffer(&thumbnailData);
        thumbBuffer.open(QIODevice::WriteOnly);
        thumbImg.save(&thumbBuffer, "PNG");
        thumbnailDimensions = thumbImg.size();
    }

    const auto roomIdStd = roomId.toStdString();
    const bool encrypted = cache::isRoomEncrypted(roomIdStd);

    auto mainPayload    = std::string(fileData.constData(), fileData.size());
    const auto mainSize = static_cast<uint64_t>(fileData.size());

    std::optional<mtx::crypto::EncryptedFile> mainEncFile;
    std::optional<mtx::crypto::EncryptedFile> thumbEncFile;

    if (encrypted) {
        mtx::crypto::BinaryBuf buf;
        std::tie(buf, mainEncFile) = mtx::crypto::encrypt_file(std::move(mainPayload));
        mainPayload                = mtx::crypto::to_string(buf);
    }

    std::string thumbPayload;
    uint64_t thumbSize = 0;
    if (!thumbnailData.isEmpty()) {
        thumbPayload = std::string(thumbnailData.constData(), thumbnailData.size());
        thumbSize    = static_cast<uint64_t>(thumbnailData.size());
        if (encrypted) {
            mtx::crypto::BinaryBuf buf;
            std::tie(buf, thumbEncFile) = mtx::crypto::encrypt_file(std::move(thumbPayload));
            thumbPayload                = mtx::crypto::to_string(buf);
        }
    }

    // Upload thumbnail first (if present), then main file.
    auto uploadMainAndSend = [roomIdStd,
                              encrypted,
                              fname,
                              mime,
                              body,
                              dimensions,
                              mainSize,
                              mainEncFile,
                              blurhashStr,
                              thumbnailDimensions,
                              thumbSize,
                              thumbEncFile,
                              callback](std::string mainPayload, const QString &thumbUrl) {
        const auto mainContentType =
          encrypted ? std::string("application/octet-stream") : mime.toStdString();
        const auto mainFilename = encrypted ? std::string() : fname.toStdString();
        http::client()->upload(
          mainPayload,
          mainContentType,
          mainFilename,
          [=](const mtx::responses::ContentURI &res, mtx::http::RequestErr err) {
              if (err) {
                  if (callback)
                      callback({}, QString::fromStdString(err->matrix_error.error));
                  return;
              }

              mtx::events::msg::Image image;
              image.info.mimetype = mime.toStdString();
              image.info.size     = mainSize;
              image.info.h        = dimensions.height();
              image.info.w        = dimensions.width();
              image.info.blurhash = blurhashStr.toStdString();
              image.body = body.isEmpty() ? fname.toStdString() : body.trimmed().toStdString();
              if (!fname.isEmpty())
                  image.filename = fname.toStdString();

              if (mainEncFile) {
                  auto ef    = mainEncFile.value();
                  ef.url     = res.content_uri;
                  image.file = ef;
              } else {
                  image.url = res.content_uri;
              }

              if (!thumbUrl.isEmpty()) {
                  if (thumbEncFile) {
                      auto tef                  = thumbEncFile.value();
                      tef.url                   = thumbUrl.toStdString();
                      image.info.thumbnail_file = tef;
                  } else {
                      image.info.thumbnail_url = thumbUrl.toStdString();
                  }
                  image.info.thumbnail_info.h        = thumbnailDimensions.height();
                  image.info.thumbnail_info.w        = thumbnailDimensions.width();
                  image.info.thumbnail_info.size     = thumbSize;
                  image.info.thumbnail_info.mimetype = "image/png";
              }

              auto txnId = std::string("m") + std::to_string(QDateTime::currentMSecsSinceEpoch());

              auto sendCb = [callback](const mtx::responses::EventId &eventId,
                                       mtx::http::RequestErr sendErr) {
                  if (sendErr) {
                      if (callback)
                          callback({}, QString::fromStdString(sendErr->matrix_error.error));
                      return;
                  }
                  if (callback)
                      callback(QString::fromStdString(eventId.event_id.to_string()), {});
              };

              if (encrypted) {
                  try {
                      nlohmann::json doc = {{"type", "m.room.message"},
                                            {"content", nlohmann::json(image)},
                                            {"room_id", roomIdStd}};
                      auto enc =
                        olm::encrypt_group_message(roomIdStd, http::client()->device_id(), doc);
                      http::client()->send_room_message(roomIdStd, txnId, enc, sendCb);
                  } catch (const std::exception &e) {
                      if (callback)
                          callback({},
                                   QStringLiteral("encryption failed: ") +
                                     QString::fromStdString(e.what()));
                  }
              } else {
                  http::client()->send_room_message(roomIdStd, txnId, image, sendCb);
              }
          });
    };

    if (!thumbPayload.empty()) {
        const auto thumbContentType =
          encrypted ? std::string("application/octet-stream") : std::string("image/png");
        http::client()->upload(
          thumbPayload,
          thumbContentType,
          encrypted ? std::string() : std::string("thumbnail.png"),
          [uploadMainAndSend, mainPayload = std::move(mainPayload), callback](
            const mtx::responses::ContentURI &res, mtx::http::RequestErr err) mutable {
              if (err) {
                  if (callback)
                      callback({}, QString::fromStdString(err->matrix_error.error));
                  return;
              }
              uploadMainAndSend(std::move(mainPayload), QString::fromStdString(res.content_uri));
          });
    } else {
        uploadMainAndSend(std::move(mainPayload), {});
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
    const auto normalizedMxcUri = mxcUri.trimmed();
    if (normalizedMxcUri.isEmpty()) {
        if (callback)
            callback({}, QStringLiteral("mxcUri must not be empty"));
        return;
    }

    const auto roomId = resolveRoomId(roomIdOrAlias);
    if (roomId.isEmpty()) {
        if (callback)
            callback({}, QStringLiteral("room not found: ") + roomIdOrAlias);
        return;
    }

    const auto roomIdStd = roomId.toStdString();
    if (cache::isRoomEncrypted(roomIdStd)) {
        if (callback)
            callback({},
                     QStringLiteral("cannot send unencrypted media to encrypted room; "
                                    "use rooms.sendImageFile with a file path"));
        return;
    }

    mtx::events::msg::Image image;
    image.url  = normalizedMxcUri.toStdString();
    image.body = body.isEmpty()
                   ? (filename.isEmpty() ? std::string("image") : filename.toStdString())
                   : body.trimmed().toStdString();
    if (!filename.isEmpty())
        image.filename = filename.toStdString();

    if (!info.isEmpty()) {
        image.info.mimetype = info.value(QStringLiteral("mimetype")).toString().toStdString();
        image.info.w        = info.value(QStringLiteral("w")).toInt();
        image.info.h        = info.value(QStringLiteral("h")).toInt();
        image.info.size     = static_cast<uint64_t>(info.value(QStringLiteral("size")).toInt());
    }

    const auto txnId = std::string("m") + std::to_string(QDateTime::currentMSecsSinceEpoch());
    auto sendCb = [callback](const mtx::responses::EventId &eventId, mtx::http::RequestErr err) {
        if (err) {
            if (callback)
                callback({}, QString::fromStdString(err->matrix_error.error));
            return;
        }
        if (callback)
            callback(QString::fromStdString(eventId.event_id.to_string()), {});
    };

    http::client()->send_room_message(roomIdStd, txnId, image, sendCb);
}

} // namespace komai::ipc

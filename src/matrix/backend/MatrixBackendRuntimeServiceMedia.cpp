// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixBackendRuntimeService.h"

#include <QByteArray>

#include "komai-rust-cxxbridge/ffi.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendBridge.h"
#include "matrix/backend/MatrixBackendRuntimeServiceInternal.h"
#include "matrix/backend/MatrixBlockingCall.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"

namespace komai {

std::optional<QString>
MatrixBackendRuntimeService::sendRoomAttachment(matrix_backend::BlockingCallContext context,
                                                uint64_t handleId,
                                                const QString &roomId,
                                                const QString &filePath,
                                                const QString &filename,
                                                const QString &caption,
                                                bool useMarkdownFormatting,
                                                const QString &replyEventId,
                                                const QString &threadId,
                                                const QString &mimeType,
                                                uint64_t durationMs,
                                                bool isVoice,
                                                const QList<float> &waveform,
                                                bool stripImageMetadata,
                                                QString *errorOut)
{
    try {
        ::rust::String eventId;
        matrix_backend::invokeBlockingCall(
          "matrix_send_room_attachment",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId,
           roomId,
           filePath,
           filename,
           caption,
           useMarkdownFormatting,
           replyEventId,
           threadId,
           mimeType,
           durationMs,
           isVoice,
           waveform,
           stripImageMetadata,
           context,
           &eventId]() {
              const auto waveformSlice = ::rust::Slice<const float>(
                waveform.constData(), static_cast<size_t>(waveform.size()));
              eventId = ::komai::rust::matrix_send_room_attachment(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                filePath.toStdString(),
                filename.toStdString(),
                caption.toStdString(),
                useMarkdownFormatting,
                replyEventId.toStdString(),
                threadId.toStdString(),
                mimeType.toStdString(),
                durationMs,
                isVoice,
                waveformSlice,
                stripImageMetadata);
          });
        return QString::fromStdString(std::string(eventId));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QString>
MatrixBackendRuntimeService::uploadMedia(matrix_backend::BlockingCallContext context,
                                         uint64_t handleId,
                                         const QString &filePath,
                                         const QString &mimeType,
                                         bool stripImageMetadata,
                                         QString *errorOut)
{
    try {
        return matrix::normalizeMxcUri(QString::fromStdString(std::string(invokeRuntimeWorkerCall(
          "matrix_upload_media", [context, handleId, filePath, mimeType, stripImageMetadata]() {
              return ::komai::rust::matrix_upload_media(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                filePath.toStdString(),
                mimeType.toStdString(),
                stripImageMetadata);
          }))));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QString>
MatrixBackendRuntimeService::sendRoomImage(matrix_backend::BlockingCallContext context,
                                           uint64_t handleId,
                                           const QString &roomId,
                                           const QString &mxcUri,
                                           const QString &body,
                                           const QString &filename,
                                           const QString &infoJson,
                                           MatrixSendMode sendMode,
                                           QString *errorOut)
{
    try {
        const auto eventId = invokeRuntimeWorkerCall(
          "matrix_send_room_image",
          [context, handleId, roomId, mxcUri, body, filename, infoJson, sendMode]() {
              return ::komai::rust::matrix_send_room_image(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                mxcUri.toStdString(),
                body.toStdString(),
                filename.toStdString(),
                infoJson.toStdString(),
                sendMode == MatrixSendMode::Queued);
          });
        return QString::fromStdString(std::string(eventId));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QByteArray>
MatrixBackendRuntimeService::fetchActiveRoomTimelineMediaContent(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &itemId,
  int width,
  int height,
  bool crop,
  QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_active_room_timeline_media_content",
          [context, handleId, &itemId, width, height, crop]() {
              return ::komai::rust::matrix_fetch_active_room_timeline_media_content(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                itemId.toStdString(),
                width,
                height,
                crop);
          });
        QByteArray data;
        data.reserve(static_cast<qsizetype>(result.size()));
        data.append(reinterpret_cast<const char *>(result.data()),
                    static_cast<qsizetype>(result.size()));
        return data;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QByteArray>
MatrixBackendRuntimeService::fetchActiveRoomTimelineMediaContentWithProgress(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &itemId,
  QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_active_room_timeline_media_content_with_progress",
          [context, handleId, &itemId]() {
              return ::komai::rust::matrix_fetch_active_room_timeline_media_content_with_progress(
                matrix_backend::toRustBlockingContext(context), handleId, itemId.toStdString());
          });
        QByteArray data;
        data.reserve(static_cast<qsizetype>(result.size()));
        data.append(reinterpret_cast<const char *>(result.data()),
                    static_cast<qsizetype>(result.size()));
        return data;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::pair<uint64_t, uint64_t>
MatrixBackendRuntimeService::activeTimelineMediaDownloadProgress(uint64_t handleId,
                                                                 const QString &itemId)
{
    const auto progress =
      ::komai::rust::matrix_active_timeline_media_download_progress(handleId, itemId.toStdString());
    return {progress.received_bytes, progress.total_bytes};
}

std::optional<QByteArray>
MatrixBackendRuntimeService::fetchMediaContent(matrix_backend::BlockingCallContext context,
                                               uint64_t handleId,
                                               const QString &mxcUri,
                                               int width,
                                               int height,
                                               bool crop,
                                               QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_media_content", [context, handleId, &mxcUri, width, height, crop]() {
              return ::komai::rust::matrix_fetch_media_content(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                mxcUri.toStdString(),
                width,
                height,
                crop);
          });
        QByteArray data;
        data.reserve(static_cast<qsizetype>(result.size()));
        data.append(reinterpret_cast<const char *>(result.data()),
                    static_cast<qsizetype>(result.size()));
        return data;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

} // namespace komai

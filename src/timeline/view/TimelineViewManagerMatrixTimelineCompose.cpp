// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/TimelineViewManager.h"

#include <QPointer>
#include <QUuid>

#include "chat/ChatPage.h"
#include "emoji/EmoticonReplace.h"
#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"
#include "timeline/rust/MatrixTimelineModel.h"
#include "timeline/view/TimelineViewManagerMatrixTimelineInternal.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"
#include "utils/Utils.h"

using namespace komai::timeline::view::internal;

// Declared in TimelineViewManagerMatrixTimelineInternal.h. Shared with the
// slash-command send path so /me-style emotes carry the same mentions.
namespace komai::timeline::view::internal {
void
splitComposerMentions(const QStringList &mentions, QString *userIdsOut, bool *roomOut)
{
    QStringList userIds;
    bool room = false;
    for (const auto &mention : mentions) {
        const auto trimmed = mention.trimmed();
        if (trimmed.isEmpty())
            continue;
        if (trimmed == QStringLiteral("@room")) {
            room = true;
            continue;
        }
        if (!userIds.contains(trimmed))
            userIds.push_back(trimmed);
    }
    if (userIdsOut)
        *userIdsOut = userIds.join(QChar(u'\n'));
    if (roomOut)
        *roomOut = room;
}
} // namespace komai::timeline::view::internal

QString
TimelineViewManager::formatMatrixMessageHtml(const QString &body) const
{
    if (perfUiFlagEnabled(QStringLiteral("disable_timeline_rich_text")))
        return body.toHtmlEscaped().replace(u'\n', QStringLiteral("<br>"));

    return renderPlainMatrixMessageHtml(body);
}
bool
TimelineViewManager::sendActiveMatrixTextMessage(const QString &body, const QStringList &mentions)
{
    const auto plainBody = emoji::replaceEmoticons(
      body.trimmed(), UserSettings::instance()->composerInputAutoReplaceEmoji());
    if (plainBody.isEmpty())
        return false;

    QString mentionUserIds;
    bool mentionsRoom = false;
    splitComposerMentions(mentions, &mentionUserIds, &mentionsRoom);

    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to send matrix-sdk room message without an active runtime "
          "handle or selected matrix room");
        return false;
    }

    const auto roomId                = activeMatrixTimelineRoomId_;
    const auto useMarkdownFormatting = matrixMessageUsesMarkdownFormatting();
    const auto replyEventId          = matrixTimelineReplyEventId_.trimmed();
    const auto threadId              = matrixTimelineThreadEventId_.trimmed();
    // When in a thread but no explicit reply, reply to the thread root.
    const auto effectiveReplyEventId = replyEventId.isEmpty() ? threadId : replyEventId;
    const auto isReplyOrThread       = !effectiveReplyEventId.isEmpty();
    const auto action = isReplyOrThread ? QStringLiteral("reply") : QStringLiteral("message");

    komai::qt_worker_task::runQueued(
      this,
      [handleId,
       roomId,
       plainBody,
       useMarkdownFormatting,
       effectiveReplyEventId,
       threadId,
       mentionUserIds,
       mentionsRoom,
       action]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok =
            effectiveReplyEventId.isEmpty()
              ? komai::MatrixBackendRuntimeService::sendRoomMessage(context,
                                                                    handleId,
                                                                    roomId,
                                                                    plainBody,
                                                                    useMarkdownFormatting,
                                                                    QStringLiteral("text"),
                                                                    komai::MatrixSendMode::Queued,
                                                                    mentionUserIds,
                                                                    mentionsRoom,
                                                                    &error)
                  .has_value()
              : komai::MatrixBackendRuntimeService::sendRoomReplyMessage(context,
                                                                         handleId,
                                                                         roomId,
                                                                         effectiveReplyEventId,
                                                                         plainBody,
                                                                         useMarkdownFormatting,
                                                                         QStringLiteral("text"),
                                                                         threadId,
                                                                         mentionUserIds,
                                                                         mentionsRoom,
                                                                         &error);

          return MatrixTimelineMessageSendResult{
            .handleId      = handleId,
            .roomId        = roomId,
            .targetEventId = effectiveReplyEventId,
            .action        = action,
            .error         = error,
            .ok            = ok,
          };
      },
      [](TimelineViewManager *, MatrixTimelineMessageSendResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (result.ok)
              return;

          komai::logging::ui()->warn("Failed to queue matrix-sdk room {} for '{}' on handle {}: {}",
                                     result.action.toStdString(),
                                     result.roomId.toStdString(),
                                     result.handleId,
                                     result.error.toStdString());
          mainWindow->showNotification(
            TimelineViewManager::tr("Failed to send message: %1").arg(result.error));
      });

    // Clear the reply state but keep the thread state — the user stays in
    // the thread view after sending and the message will appear via the
    // subscription receiver.
    if (clearActiveMatrixReplyState())
        emit matrixTimelineStateChanged();

    return true;
}
QString
TimelineViewManager::normalizedMatrixMessageKind(const QString &messageKind) const
{
    const auto normalizedKind = messageKind.trimmed().toLower();
    if (normalizedKind == QStringLiteral("notice"))
        return QStringLiteral("notice");
    if (normalizedKind == QStringLiteral("emote"))
        return QStringLiteral("emote");
    return QStringLiteral("text");
}
bool
TimelineViewManager::queueActiveMatrixEdit(const QString &eventId,
                                           const QString &body,
                                           const QString &messageKind)
{
    if (activeMatrixTimelineRoomId_.isEmpty())
        return false;

    if (matrixAttachmentUploadInFlight_ || !pendingMatrixAttachments_.empty())
        return false;

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    if (body.trimmed().isEmpty())
        return false;

    bool clearedReplyState = clearActiveMatrixReplyState();
    const auto stateChanged =
      setActiveMatrixEditState(trimmedEventId, normalizedMatrixMessageKind(messageKind));

    if (!stateChanged && !clearedReplyState)
        return false;

    if (clearedReplyState)
        emit replyClosed();
    emit matrixTimelineStateChanged();
    return true;
}
void
TimelineViewManager::clearActiveMatrixEdit()
{
    if (clearActiveMatrixEditState())
        emit matrixTimelineStateChanged();
}
bool
TimelineViewManager::sendActiveMatrixEditMessage(const QString &body, const QStringList &mentions)
{
    const auto plainBody = emoji::replaceEmoticons(
      body.trimmed(), UserSettings::instance()->composerInputAutoReplaceEmoji());
    if (plainBody.isEmpty())
        return false;

    QString mentionUserIds;
    bool mentionsRoom = false;
    splitComposerMentions(mentions, &mentionUserIds, &mentionsRoom);

    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty() ||
        matrixTimelineEditEventId_.trimmed().isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to send matrix-sdk room edit without an active runtime "
          "handle, selected matrix room, and edit target");
        return false;
    }

    const auto roomId                = activeMatrixTimelineRoomId_;
    const auto targetEventId         = matrixTimelineEditEventId_.trimmed();
    const auto useMarkdownFormatting = matrixMessageUsesMarkdownFormatting();
    const auto messageKind           = matrixTimelineEditMessageKind_;

    komai::qt_worker_task::runQueued(
      this,
      [handleId,
       roomId,
       targetEventId,
       plainBody,
       useMarkdownFormatting,
       messageKind,
       mentionUserIds,
       mentionsRoom]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok =
            komai::MatrixBackendRuntimeService::sendRoomEditMessage(context,
                                                                    handleId,
                                                                    roomId,
                                                                    targetEventId,
                                                                    plainBody,
                                                                    useMarkdownFormatting,
                                                                    messageKind,
                                                                    mentionUserIds,
                                                                    mentionsRoom,
                                                                    &error);

          return MatrixTimelineMessageSendResult{
            .handleId      = handleId,
            .roomId        = roomId,
            .targetEventId = targetEventId,
            .action        = QStringLiteral("edit"),
            .error         = error,
            .ok            = ok,
          };
      },
      [](TimelineViewManager *, MatrixTimelineMessageSendResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (result.ok)
              return;

          komai::logging::ui()->warn(
            "Failed to queue matrix-sdk room edit for '{}' on handle {} targeting '{}': {}",
            result.roomId.toStdString(),
            result.handleId,
            result.targetEventId.toStdString(),
            result.error.toStdString());
          mainWindow->showNotification(
            TimelineViewManager::tr("Failed to edit message: %1").arg(result.error));
      });

    if (clearActiveMatrixEditState())
        emit matrixTimelineStateChanged();

    return true;
}
bool
TimelineViewManager::queueActiveMatrixReply(const QString &eventId,
                                            const QString &senderId,
                                            const QString &senderDisplayName,
                                            const QString &body)
{
    if (activeMatrixTimelineRoomId_.isEmpty())
        return false;

    const auto trimmedEventId  = eventId.trimmed();
    const auto trimmedSenderId = senderId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    const auto trimmedSenderDisplayName = senderDisplayName.trimmed();
    const auto trimmedBody              = body.trimmed();
    const auto changed                  = setActiveMatrixReplyState(
      trimmedEventId, trimmedSenderId, trimmedSenderDisplayName, trimmedBody);
    if (changed)
        emit matrixTimelineStateChanged();
    focusMessageInput();
    return true;
}
void
TimelineViewManager::clearActiveMatrixReply()
{
    if (!clearActiveMatrixReplyState())
        return;

    emit matrixTimelineStateChanged();
    emit replyClosed();
}
bool
TimelineViewManager::toggleActiveMatrixTimelineReaction(const QString &eventId,
                                                        const QString &reactionKey)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to toggle a matrix-sdk room reaction without an active "
          "runtime handle or selected matrix room");
        return false;
    }

    const auto trimmedEventId     = eventId.trimmed();
    const auto trimmedReactionKey = reactionKey.trimmed();
    if (trimmedEventId.isEmpty() || trimmedReactionKey.isEmpty())
        return false;

    const auto roomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedEventId, trimmedReactionKey]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::toggleRoomReaction(
            context, handleId, roomId, trimmedEventId, trimmedReactionKey, &error);
          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedEventId,
            .detail   = trimmedReactionKey,
            .error    = error,
            .ok       = ok,
          };
      },
      [](TimelineViewManager *manager, MatrixTimelineEventActionResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (result.ok) {
              if (manager && manager->activeMatrixTimelineRoomId_ == result.roomId) {
                  manager->invalidateMatrixTimelineFrequentReactionsCache(result.roomId);
                  manager->refreshActiveMatrixTimelineRoomStateAsync();
              }
              return;
          }

          komai::logging::ui()->warn(
            "Failed to toggle matrix-sdk room reaction '{}' for event '{}' in "
            "'{}' on handle {}: {}",
            result.detail.toStdString(),
            result.eventId.toStdString(),
            result.roomId.toStdString(),
            result.handleId,
            result.error.toStdString());
          mainWindow->showNotification(
            TimelineViewManager::tr("Failed to react: %1").arg(result.error));
      });
    return true;
}
bool
TimelineViewManager::setActiveMatrixReplyState(const QString &eventId,
                                               const QString &senderId,
                                               const QString &senderDisplayName,
                                               const QString &body)
{
    const auto trimmedEventId           = eventId.trimmed();
    const auto trimmedSenderId          = senderId.trimmed();
    const auto trimmedSenderDisplayName = senderDisplayName.trimmed();
    const auto trimmedBody              = body.trimmed();

    if (matrixTimelineReplyEventId_ == trimmedEventId &&
        matrixTimelineReplySenderId_ == trimmedSenderId &&
        matrixTimelineReplySenderDisplayName_ == trimmedSenderDisplayName &&
        matrixTimelineReplyBody_ == trimmedBody) {
        return false;
    }

    matrixTimelineReplyEventId_           = trimmedEventId;
    matrixTimelineReplySenderId_          = trimmedSenderId;
    matrixTimelineReplySenderDisplayName_ = trimmedSenderDisplayName;
    matrixTimelineReplyBody_              = trimmedBody;
    emit replyingEventChanged(matrixTimelineReplyEventId_);
    return true;
}
bool
TimelineViewManager::clearActiveMatrixReplyState()
{
    if (matrixTimelineReplyEventId_.isEmpty() && matrixTimelineReplySenderId_.isEmpty() &&
        matrixTimelineReplySenderDisplayName_.isEmpty() && matrixTimelineReplyBody_.isEmpty()) {
        return false;
    }

    matrixTimelineReplyEventId_.clear();
    matrixTimelineReplySenderId_.clear();
    matrixTimelineReplySenderDisplayName_.clear();
    matrixTimelineReplyBody_.clear();
    emit replyingEventChanged(QString());
    return true;
}
bool
TimelineViewManager::setActiveMatrixEditState(const QString &eventId, const QString &messageKind)
{
    const auto trimmedEventId    = eventId.trimmed();
    const auto normalizedMessage = normalizedMatrixMessageKind(messageKind);

    if (matrixTimelineEditEventId_ == trimmedEventId &&
        matrixTimelineEditMessageKind_ == normalizedMessage) {
        return false;
    }

    matrixTimelineEditEventId_     = trimmedEventId;
    matrixTimelineEditMessageKind_ = normalizedMessage;
    return true;
}
bool
TimelineViewManager::clearActiveMatrixEditState()
{
    if (matrixTimelineEditEventId_.isEmpty() && matrixTimelineEditMessageKind_.isEmpty())
        return false;

    matrixTimelineEditEventId_.clear();
    matrixTimelineEditMessageKind_.clear();
    return true;
}

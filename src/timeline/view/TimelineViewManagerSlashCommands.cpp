// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/TimelineViewManager.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QPointer>

#include <optional>
#include <thread>
#include <utility>

#include "chat/ChatPage.h"
#include "emoji/EmoticonReplace.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"
#include "timeline/SlashCommands.h"
#include "timeline/rust/MatrixTimelineModel.h"
#include "timeline/view/TimelineViewManagerMatrixTimelineInternal.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"
#include "utils/Utils.h"

namespace {

using timeline::slash_commands::CommandId;
using timeline::slash_commands::ParsedCommand;
using timeline::slash_commands::SubmitAction;

enum class SlashFormatMode
{
    Auto,
    ForceMarkdown,
    ForcePlain,
};

QString
submitActionName(SubmitAction action)
{
    switch (action) {
    case SubmitAction::None:
        return QStringLiteral("none");
    case SubmitAction::SendPlainText:
        return QStringLiteral("sendPlainText");
    case SubmitAction::ExecuteCommand:
        return QStringLiteral("executeCommand");
    case SubmitAction::PreserveComposer:
        return QStringLiteral("preserveComposer");
    }

    return QStringLiteral("none");
}

QString
trimmedArguments(const ParsedCommand &parsed)
{
    return parsed.arguments.trimmed();
}

struct FirstArgumentSplit
{
    QString first;
    QString rest;
};

struct SlashCommandSendResult
{
    uint64_t handleId = 0;
    QString roomId;
    QString messageKind;
    QString error;
    bool ok = false;
};

FirstArgumentSplit
splitFirstArgument(const ParsedCommand &parsed)
{
    const auto trimmed = trimmedArguments(parsed);
    if (trimmed.isEmpty())
        return {};

    int end = 0;
    while (end < trimmed.size() && !trimmed.at(end).isSpace())
        ++end;

    FirstArgumentSplit result;
    result.first = trimmed.left(end);
    if (end < trimmed.size())
        result.rest = trimmed.mid(end).trimmed();
    return result;
}

bool
useMarkdownFormattingForMatrixSend(SlashFormatMode formatMode)
{
    if (formatMode == SlashFormatMode::ForcePlain)
        return false;

    auto *chatPage       = ChatPage::instance();
    const auto *settings = chatPage ? chatPage->userSettings().get() : nullptr;
    if (formatMode == SlashFormatMode::Auto &&
        (!settings || !settings->composerInputMarkdownToHtmlEnabled())) {
        return false;
    }

    return true;
}

bool
isAllDigits(const QString &text)
{
    if (text.isEmpty())
        return false;

    for (const auto ch : text) {
        if (!ch.isDigit())
            return false;
    }

    return true;
}

} // namespace

QVariantMap
TimelineViewManager::inspectActiveMatrixSlashCommand(const QString &text) const
{
    const auto inspection = timeline::slash_commands::inspect(
      text,
      {.replyEventId = matrixTimelineReplyEventId_, .replySenderId = matrixTimelineReplySenderId_});

    QVariantMap result;
    result.insert(QStringLiteral("validationState"),
                  timeline::slash_commands::validationStateName(inspection.validation.state));
    result.insert(QStringLiteral("validationMessage"), inspection.validation.message);
    result.insert(QStringLiteral("submitAction"), submitActionName(inspection.submitAction));
    return result;
}

QString
TimelineViewManager::activeMatrixCommandCompletionSearchString(const QString &text,
                                                               int cursorPosition) const
{
    return timeline::slash_commands::completionSearchString(text, cursorPosition);
}

QString
TimelineViewManager::activeMatrixApplyCommandCompletion(const QString &text,
                                                        int cursorPosition,
                                                        const QString &completion) const
{
    return timeline::slash_commands::applyCompletion(text, cursorPosition, completion);
}

int
TimelineViewManager::activeMatrixCommandCompletionCursorPosition(const QString &text,
                                                                 int cursorPosition,
                                                                 const QString &completion) const
{
    return timeline::slash_commands::completionCursorPosition(text, cursorPosition, completion);
}

bool
TimelineViewManager::activeMatrixCommandExpectsUserIdAt(const QString &text,
                                                        int cursorPosition) const
{
    return timeline::slash_commands::argumentExpectsUserId(text, cursorPosition);
}

bool
TimelineViewManager::executeActiveMatrixSlashCommand(const QString &text,
                                                     const QStringList &mentions)
{
    const auto inspection = timeline::slash_commands::inspect(
      text,
      {.replyEventId = matrixTimelineReplyEventId_, .replySenderId = matrixTimelineReplySenderId_});

    if (inspection.submitAction != SubmitAction::ExecuteCommand || !inspection.parsed.definition)
        return false;

    // Commands that send the composed body (/me, /notice, /plain, /html, …)
    // should carry the same intentional mentions the composer tracked, so the
    // "You are about to mention …" bar is not misleading for these.
    QString mentionUserIds;
    bool mentionsRoom = false;
    komai::timeline::view::internal::splitComposerMentions(
      mentions, &mentionUserIds, &mentionsRoom);

    auto *mainWindow = MainWindow::instance();
    auto *chatPage   = ChatPage::instance();

    const auto showNotification = [mainWindow](const QString &message) {
        if (mainWindow)
            mainWindow->showNotification(message);
    };
    const auto clearReplyIfNeeded = [this]() {
        if (clearActiveMatrixReplyState())
            emit matrixTimelineStateChanged();
    };
    const auto activeHandleId = [mainWindow]() -> uint64_t {
        return mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    };
    const auto requireActiveRoom = [this, &showNotification]() {
        if (!activeMatrixTimelineRoomId_.isEmpty())
            return true;

        showNotification(tr("This command needs an active room."));
        return false;
    };
    const auto requireChatPage = [chatPage, &showNotification]() {
        if (chatPage)
            return true;

        showNotification(
          QCoreApplication::translate("TimelineViewManager", "The chat page is not ready yet."));
        return false;
    };
    const auto requireHandle = [&activeHandleId, &showNotification]() {
        if (activeHandleId() != 0)
            return true;

        showNotification(QCoreApplication::translate("TimelineViewManager",
                                                     "The Matrix session is not ready yet."));
        return false;
    };
    const auto sendMessage = [this, mainWindow, mentionUserIds, mentionsRoom](
                               const QString &body,
                               const QString &messageKind,
                               SlashFormatMode formatMode) {
        const auto plainBody = emoji::replaceEmoticons(
          body.trimmed(), UserSettings::instance()->composerInputAutoReplaceEmoji());
        if (plainBody.isEmpty())
            return false;

        const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
        if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
            komai::logging::ui()->warn(
              "Refusing to send matrix-sdk slash-command message without an "
              "active runtime handle or selected matrix room");
            return false;
        }

        const auto useMarkdownFormatting = useMarkdownFormattingForMatrixSend(formatMode);
        const auto replyEventId          = matrixTimelineReplyEventId_.trimmed();
        const auto threadId              = matrixTimelineThreadEventId_.trimmed();
        const auto effectiveReplyEventId = replyEventId.isEmpty() ? threadId : replyEventId;

        const auto roomId = activeMatrixTimelineRoomId_;
        komai::qt_worker_task::runQueued(
          this,
          [handleId,
           roomId,
           effectiveReplyEventId,
           threadId,
           plainBody,
           useMarkdownFormatting,
           messageKind,
           mentionUserIds,
           mentionsRoom]() {
              const auto context = komai::matrix_backend::blockingCallContext();
              QString error;
              const bool ok =
                effectiveReplyEventId.isEmpty()
                  ? komai::MatrixBackendRuntimeService::sendRoomMessage(
                      context,
                      handleId,
                      roomId,
                      plainBody,
                      useMarkdownFormatting,
                      messageKind,
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
                                                                             messageKind,
                                                                             threadId,
                                                                             mentionUserIds,
                                                                             mentionsRoom,
                                                                             &error);

              return SlashCommandSendResult{
                .handleId    = handleId,
                .roomId      = roomId,
                .messageKind = messageKind,
                .error       = error,
                .ok          = ok,
              };
          },
          [](TimelineViewManager *, SlashCommandSendResult result) {
              auto *mainWindow = MainWindow::instance();
              if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
                  return;

              if (result.ok)
                  return;

              komai::logging::ui()->warn(
                "Failed to queue matrix-sdk slash-command room message kind='{}' "
                "room='{}' handle={} error='{}'",
                result.messageKind.toStdString(),
                result.roomId.toStdString(),
                result.handleId,
                result.error.toStdString());
              mainWindow->showNotification(
                QCoreApplication::translate("TimelineViewManager", "Failed to send message: %1")
                  .arg(result.error));
          });

        return true;
    };
    const auto parsed      = inspection.parsed;
    const auto arguments   = trimmedArguments(parsed);
    const auto argSplit    = splitFirstArgument(parsed);
    const auto replyEvent  = matrixTimelineReplyEventId_.trimmed();
    const auto replySender = matrixTimelineReplySenderId_.trimmed();

    bool ok = false;

    switch (parsed.definition->id) {
    case CommandId::Me:
        ok = sendMessage(arguments, QStringLiteral("emote"), SlashFormatMode::Auto);
        break;
    case CommandId::React:
        ok = !replyEvent.isEmpty() && toggleActiveMatrixTimelineReaction(replyEvent, arguments);
        break;
    case CommandId::Join:
        if (!requireChatPage())
            return false;
        if (argSplit.first.isEmpty())
            return false;
        chatPage->joinRoom(argSplit.first, argSplit.rest);
        ok = true;
        break;
    case CommandId::Knock:
        if (!requireChatPage())
            return false;
        if (argSplit.first.isEmpty())
            return false;
        chatPage->knockRoom(argSplit.first, argSplit.rest);
        ok = true;
        break;
    case CommandId::Leave:
        if (!requireChatPage() || !requireActiveRoom())
            return false;
        // Route through FilteredRoomlistModel so the row is removed from the
        // model synchronously and the roomLeft signal fires, matching the
        // Leave dialog path (closes any open tab, resets the current room).
        FilteredRoomlistModel::instance()->leave(activeMatrixTimelineRoomId_, arguments);
        ok = true;
        break;
    case CommandId::Invite:
        if (!requireChatPage() || !requireActiveRoom())
            return false;
        if (argSplit.first.isEmpty())
            return false;
        chatPage->inviteUser(activeMatrixTimelineRoomId_, argSplit.first, argSplit.rest);
        ok = true;
        break;
    case CommandId::Kick:
    case CommandId::Ban:
    case CommandId::Unban: {
        if (!requireChatPage() || !requireActiveRoom())
            return false;

        QString targetUserId;
        QString reason;
        if (argSplit.first.startsWith(u"@")) {
            targetUserId = argSplit.first;
            reason       = argSplit.rest;
        } else if (!replySender.isEmpty()) {
            targetUserId = replySender;
            reason       = arguments;
        }

        if (targetUserId.isEmpty())
            return false;

        switch (parsed.definition->id) {
        case CommandId::Kick:
            chatPage->kickUser(activeMatrixTimelineRoomId_, targetUserId, reason);
            break;
        case CommandId::Ban:
            chatPage->banUser(activeMatrixTimelineRoomId_, targetUserId, reason);
            break;
        case CommandId::Unban:
            chatPage->unbanUser(activeMatrixTimelineRoomId_, targetUserId, reason);
            break;
        default:
            break;
        }
        ok = true;
        break;
    }
    case CommandId::Redact: {
        if (!requireActiveRoom())
            return false;

        QString targetEventId;
        QString reason;
        if (arguments.isEmpty()) {
            targetEventId = replyEvent;
        } else if (argSplit.first.startsWith(u"$")) {
            targetEventId = argSplit.first;
            reason        = argSplit.rest;
        } else if (argSplit.first.startsWith(u"@")) {
            reason = argSplit.rest;
            ok     = redactActiveMatrixTimelineEventsByUser(argSplit.first, reason);
            if (!ok) {
                ChatPage::instance()->showNotification(
                  tr("No messages found from %1 in the visible timeline.").arg(argSplit.first));
                return true;
            }
            break;
        } else if (!replyEvent.isEmpty()) {
            targetEventId = replyEvent;
            reason        = arguments;
        }

        ok = !targetEventId.isEmpty() && redactActiveMatrixTimelineEvent(targetEventId, reason);
        break;
    }
    case CommandId::Roomnick: {
        if (!requireHandle() || !requireActiveRoom())
            return false;

        const auto handleId    = activeHandleId();
        const auto roomId      = activeMatrixTimelineRoomId_;
        const auto displayName = arguments;

        komai::qt_worker_task::runQueued(
          this,
          [handleId, roomId, displayName]() {
              const auto context = komai::matrix_backend::blockingCallContext();
              QString error;
              const bool ok = komai::MatrixBackendRuntimeService::setOwnRoomDisplayName(
                context, handleId, roomId, displayName, &error);
              return std::make_pair(ok, error);
          },
          [roomId](TimelineViewManager *manager, const std::pair<bool, QString> &result) {
              const auto &[ok, error] = result;
              auto *mainWindow        = MainWindow::instance();

              if (!ok) {
                  if (mainWindow) {
                      mainWindow->showNotification(
                        TimelineViewManager::tr(
                          "Failed to update your room-specific display name for %1: %2")
                          .arg(roomId, error));
                  }
                  return;
              }

              manager->scheduleMatrixSidebarRefresh();
          });

        ok = true;
        break;
    }
    case CommandId::Shrug: {
        const auto body = arguments.isEmpty() ? QStringLiteral("¯\\_(ツ)_/¯")
                                              : arguments + QStringLiteral(" ¯\\_(ツ)_/¯");
        ok              = sendMessage(body, QStringLiteral("text"), SlashFormatMode::Auto);
        break;
    }
    case CommandId::Markdown:
        ok = sendMessage(arguments, QStringLiteral("text"), SlashFormatMode::ForceMarkdown);
        break;
    case CommandId::Plain:
        ok = sendMessage(arguments, QStringLiteral("text"), SlashFormatMode::ForcePlain);
        break;
    case CommandId::Notice:
        ok = sendMessage(arguments, QStringLiteral("notice"), SlashFormatMode::Auto);
        break;
    case CommandId::Msgtype:
        if (argSplit.first.isEmpty())
            return false;
        ok = sendMessage(argSplit.rest, argSplit.first, SlashFormatMode::Auto);
        break;
    case CommandId::Goto: {
        if (!requireActiveRoom())
            return false;

        const auto target = arguments.trimmed();
        if (target.isEmpty())
            return false;

        if (target.startsWith(u'$')) {
            showEvent(activeMatrixTimelineRoomId_, target);
            ok = true;
            break;
        }

        if (isAllDigits(target)) {
            if (!matrixTimelineModel_) {
                showNotification(tr("The room timeline is not ready yet."));
                return false;
            }

            bool rowOk     = false;
            const auto row = target.toInt(&rowOk);
            if (!rowOk || row < 0) {
                showNotification(tr("That message index could not be resolved in this room."));
                return false;
            }

            const auto item = matrixTimelineModel_->itemAt(row);
            const auto targetId =
              item.value(QStringLiteral("eventId")).toString().trimmed().isEmpty()
                ? item.value(QStringLiteral("itemId")).toString().trimmed()
                : item.value(QStringLiteral("eventId")).toString().trimmed();
            if (targetId.isEmpty()) {
                showNotification(tr("That message index could not be resolved in this room."));
                return false;
            }

            showEvent(activeMatrixTimelineRoomId_, targetId);
            ok = true;
            break;
        }

        if (!requireChatPage())
            return false;

        ok = chatPage->tryHandleMatrixUri(target);
        if (!ok) {
            showNotification(
              tr("Could not resolve that /goto target. Use an event ID, numeric message index, "
                 "or Matrix link."));
        }
        break;
    }
    case CommandId::ConvertToDm:
    case CommandId::ConvertToRoom: {
        if (!requireHandle() || !requireActiveRoom())
            return false;

        const bool isDirect   = parsed.definition->id == CommandId::ConvertToDm;
        const auto handleId   = activeHandleId();
        const auto roomId     = activeMatrixTimelineRoomId_;
        const auto roomIdText = roomId;

        komai::qt_worker_task::runQueued(
          this,
          [handleId, roomId, isDirect]() {
              const auto context = komai::matrix_backend::blockingCallContext();
              QString error;
              const bool ok = komai::MatrixBackendRuntimeService::setRoomIsDirect(
                context, handleId, roomId, isDirect, &error);
              return std::make_pair(ok, error);
          },
          [isDirect, roomIdText](TimelineViewManager *manager,
                                 const std::pair<bool, QString> &result) {
              const auto &[ok, error] = result;
              auto *mainWindow        = MainWindow::instance();

              if (!ok) {
                  if (mainWindow) {
                      mainWindow->showNotification(
                        TimelineViewManager::tr("Failed to update direct-message state for %1: %2")
                          .arg(roomIdText, error));
                  }
                  return;
              }

              manager->scheduleMatrixSidebarRefresh();

              if (mainWindow) {
                  mainWindow->showNotification(
                    isDirect ? TimelineViewManager::tr("Marked this room as a direct message.")
                             : TimelineViewManager::tr("Marked this room as a regular room."));
              }
          });

        ok = true;
        break;
    }
    case CommandId::Ignore:
    case CommandId::Unignore: {
        if (!requireHandle())
            return false;

        const auto handleId  = activeHandleId();
        const auto target    = arguments;
        const auto shouldAdd = parsed.definition->id == CommandId::Ignore;

        komai::qt_worker_task::runQueued(
          this,
          [handleId, target, shouldAdd]() {
              const auto context = komai::matrix_backend::blockingCallContext();
              QString error;
              const bool ok = shouldAdd ? komai::MatrixBackendRuntimeService::ignoreUser(
                                            context, handleId, target, &error)
                                        : komai::MatrixBackendRuntimeService::unignoreUser(
                                            context, handleId, target, &error);
              return std::make_pair(ok, error);
          },
          [](TimelineViewManager *, const std::pair<bool, QString> &result) {
              const auto &[ok, error] = result;
              if (ok)
                  return;

              if (auto *mainWindow = MainWindow::instance()) {
                  mainWindow->showNotification(
                    TimelineViewManager::tr("Failed to update ignored-user state: %1").arg(error));
              }
          });
        ok = true;
        break;
    }
    case CommandId::UpgradeRoom: {
        if (!requireActiveRoom())
            return false;

        // /upgraderoom              → default version, no extra creators
        // /upgraderoom 11           → version 11, no extra creators
        // /upgraderoom 11 @a:s @b:s → version 11 + creators
        // /upgraderoom @a:s @b:s    → default version + creators (no leading version)
        QString version;
        QStringList additionalCreators;
        const auto tokens    = arguments.split(QChar(u' '), Qt::SkipEmptyParts);
        int firstUserIdIndex = 0;
        if (!tokens.isEmpty() && !tokens.first().startsWith(u'@')) {
            version          = tokens.first();
            firstUserIdIndex = 1;
        }
        for (int i = firstUserIdIndex; i < tokens.size(); ++i)
            additionalCreators << tokens.at(i);

        // Defer to the homeserver-advertised default when the user omits a
        // version.  Falls back to a local default the first time around (the
        // capability is fetched lazily); refresh in the background so the
        // *next* invocation gets the server's preference.
        if (version.isEmpty()) {
            version = defaultRoomVersion();
            if (version.isEmpty()) {
                refreshRoomVersionsCapability();
                version = QStringLiteral("12");
            }
        }

        performRoomUpgrade(activeMatrixTimelineRoomId_, version, additionalCreators);
        ok = true;
        break;
    }
    }

    if (!ok)
        return false;

    clearReplyIfNeeded();
    return true;
}

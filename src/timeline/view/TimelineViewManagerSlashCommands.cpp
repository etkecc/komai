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
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/SlashCommands.h"
#include "timeline/rust/MatrixTimelineModel.h"
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

QString
formattedHtmlForMatrixSend(const QString &body, SlashFormatMode formatMode)
{
    if (formatMode == SlashFormatMode::ForcePlain)
        return {};

    auto *chatPage       = ChatPage::instance();
    const auto *settings = chatPage ? chatPage->userSettings().get() : nullptr;
    if (formatMode == SlashFormatMode::Auto &&
        (!settings || !settings->composerInputMarkdownToHtmlEnabled())) {
        return {};
    }

    const auto html        = utils::markdownToHtml(body, false);
    const auto trimmedBody = body.trimmed();

    if (html.contains(u'<') || trimmedBody.contains(u'\n') || trimmedBody.contains(u'\\'))
        return html;

    return {};
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

std::optional<QString>
normalizeInvitePermissionTarget(const QString &text)
{
    const auto trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return std::nullopt;

    if (trimmed.compare(QStringLiteral("all"), Qt::CaseInsensitive) == 0 ||
        trimmed.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("all");
    }

    if (trimmed.startsWith(QStringLiteral("matrix:")) ||
        trimmed.startsWith(QStringLiteral("https://matrix.to"))) {
        const auto parsed = utils::parseMatrixUri(trimmed);
        if (!parsed || parsed->mxid1.isEmpty() || parsed->mxid1.startsWith(u'#'))
            return std::nullopt;

        return parsed->mxid1.trimmed();
    }

    if (trimmed.startsWith(u'#'))
        return std::nullopt;

    return trimmed;
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
TimelineViewManager::executeActiveMatrixSlashCommand(const QString &text)
{
    const auto inspection = timeline::slash_commands::inspect(
      text,
      {.replyEventId = matrixTimelineReplyEventId_, .replySenderId = matrixTimelineReplySenderId_});

    if (inspection.submitAction != SubmitAction::ExecuteCommand || !inspection.parsed.definition)
        return false;

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
    const auto sendMessage = [this, mainWindow](const QString &body,
                                                const QString &messageKind,
                                                SlashFormatMode formatMode) {
        const auto plainBody = body.trimmed();
        if (plainBody.isEmpty() &&
            (messageKind == QStringLiteral("text") || messageKind == QStringLiteral("notice") ||
             messageKind == QStringLiteral("emote"))) {
            return false;
        }

        const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
        if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
            nhlog::ui()->warn("Refusing to send matrix-sdk slash-command message without an "
                              "active runtime handle or selected matrix room");
            return false;
        }

        const auto formattedHtml = formattedHtmlForMatrixSend(body, formatMode);
        const auto replyEventId  = matrixTimelineReplyEventId_.trimmed();

        const auto roomId = activeMatrixTimelineRoomId_;
        komai::qt_worker_task::runQueued(
          this,
          [handleId, roomId, replyEventId, plainBody, formattedHtml, messageKind]() {
              const auto context = komai::matrix_backend::blockingCallContext();
              QString error;
              const bool ok =
                replyEventId.isEmpty()
                  ? komai::MatrixBackendRuntimeService::sendRoomMessage(
                      context, handleId, roomId, plainBody, formattedHtml, messageKind, &error)
                  : komai::MatrixBackendRuntimeService::sendRoomReplyMessage(context,
                                                                             handleId,
                                                                             roomId,
                                                                             replyEventId,
                                                                             plainBody,
                                                                             formattedHtml,
                                                                             messageKind,
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

              nhlog::ui()->warn("Failed to queue matrix-sdk slash-command room message kind='{}' "
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
    const auto notifyUnsupported = [this, &showNotification](const char *commandName) {
        showNotification(tr("The /%1 command is not migrated to the matrix-sdk room composer yet.")
                           .arg(QString::fromLatin1(commandName)));
        return false;
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
        chatPage->leaveRoom(activeMatrixTimelineRoomId_, arguments);
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
            return notifyUnsupported(parsed.definition->name);
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

        const auto context = komai::matrix_backend::allowUiThreadBlockingCallContext();
        QString error;
        ok = parsed.definition->id == CommandId::Ignore
               ? komai::MatrixBackendRuntimeService::ignoreUser(
                   context, activeHandleId(), arguments, &error)
               : komai::MatrixBackendRuntimeService::unignoreUser(
                   context, activeHandleId(), arguments, &error);
        if (!ok) {
            showNotification(tr("Failed to update ignored-user state: %1").arg(error));
            return false;
        }
        break;
    }
    case CommandId::BlockInvites:
    case CommandId::AllowInvites: {
        if (!requireHandle())
            return false;

        const auto maybeTarget = normalizeInvitePermissionTarget(arguments);
        if (!maybeTarget.has_value()) {
            showNotification(
              tr("Use a Matrix user ID, room ID, server name, Matrix link, or 'all'."));
            return false;
        }

        const bool block    = parsed.definition->id == CommandId::BlockInvites;
        const auto handleId = activeHandleId();
        const auto target   = *maybeTarget;
        const auto targetUi = arguments.trimmed().isEmpty() ? target : arguments.trimmed();

        komai::qt_worker_task::runQueued(
          this,
          [handleId, target, block]() {
              const auto context = komai::matrix_backend::blockingCallContext();
              QString error;
              const bool ok = komai::MatrixBackendRuntimeService::setInvitePermission(
                context, handleId, target, block, &error);
              return std::make_pair(ok, error);
          },
          [block, targetUi](TimelineViewManager *manager, const std::pair<bool, QString> &result) {
              const auto &[ok, error] = result;
              auto *mainWindow        = MainWindow::instance();

              if (!ok) {
                  if (mainWindow) {
                      mainWindow->showNotification(
                        TimelineViewManager::tr("Failed to update invite permissions for %1: %2")
                          .arg(targetUi, error));
                  }
                  return;
              }

              manager->scheduleMatrixSidebarRefresh();

              if (mainWindow) {
                  mainWindow->showNotification(
                    block ? TimelineViewManager::tr("Blocked invites from %1.").arg(targetUi)
                          : TimelineViewManager::tr("Allowed invites from %1.").arg(targetUi));
              }
          });

        ok = true;
        break;
    }
    }

    if (!ok)
        return false;

    clearReplyIfNeeded();
    return true;
}

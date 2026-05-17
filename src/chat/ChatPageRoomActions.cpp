// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chat/ChatPage.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QPointer>
#include <QQmlEngine>
#include <QUrl>

#include <algorithm>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"
#include "ui/RoomSummary.h"
#include "utils/Utils.h"

namespace {
std::optional<uint64_t>
requireMatrixBackendHandle(ChatPage *page, const char *action)
{
    const auto *mainWindow = MainWindow::instance();
    if (mainWindow && mainWindow->matrixBackendHandleId() != 0)
        return mainWindow->matrixBackendHandleId();

    komai::logging::ui()->warn("Cannot {} because no active matrix-sdk runtime handle exists",
                               action);
    if (page)
        Q_EMIT page->showNotification(ChatPage::tr("Matrix backend is not ready yet."));
    return std::nullopt;
}

QVector<QString>
toQStringVector(const std::vector<std::string> &values)
{
    QVector<QString> result;
    result.reserve(static_cast<qsizetype>(values.size()));
    for (const auto &value : values)
        result.push_back(QString::fromStdString(value));
    return result;
}

template<typename WorkFnT, typename UiFnT>
void
runMatrixRuntimeTask(ChatPage *page, WorkFnT work, UiFnT ui)
{
    QPointer<ChatPage> guard(page);
    std::thread([guard, work = std::move(work), ui = std::move(ui)]() mutable {
        const auto result = work();

        if (!guard)
            return;

        Q_EMIT guard->callFunctionOnGuiThread([guard, result, ui = std::move(ui)]() mutable {
            if (!guard || guard->isShuttingDown())
                return;

            ui(guard.data(), result);
        });
    }).detach();
}

} // namespace

void
ChatPage::knockRoom(const QString &room,
                    const std::vector<std::string> &via,
                    QString reason,
                    bool failedJoin,
                    bool promptForConfirmation)
{
    bool confirmed = false;
    if (promptForConfirmation) {
        reason = QInputDialog::getText(
          nullptr,
          tr("Knock on room"),
          // clang-format off
      failedJoin
        ? tr("You failed to join %1. You can try to knock so that others can invite you in. Do you want to do so?\nYou may optionally provide a reason for others to accept your knock:").arg(room)
        : tr("Do you really want to knock on %1? You may optionally provide a reason for others to accept your knock:").arg(room),
          // clang-format on
          QLineEdit::Normal,
          reason,
          &confirmed);
        if (!confirmed) {
            return;
        }
    }

    const auto handleId = requireMatrixBackendHandle(this, "knock on a room");
    if (!handleId)
        return;

    runMatrixRuntimeTask(
      this,
      [handleId = *handleId, room, via = toQStringVector(via), reason]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          return std::make_pair(komai::MatrixBackendRuntimeService::knockRoom(
                                  context, handleId, room, via, reason, &error),
                                error);
      },
      [room](ChatPage *page, const auto &result) {
          const auto &[roomId, error] = result;
          if (!roomId.has_value()) {
              Q_EMIT page->showNotification(ChatPage::tr("Failed to knock room: %1").arg(error));
              return;
          }

          komai::logging::ui()->info("Knocked on room '{}' via matrix-sdk runtime",
                                     roomId->toStdString());
      });
}

void
ChatPage::joinRoom(const QString &room, const QString &reason)
{
    const auto room_id = room.toStdString();
    joinRoomVia(room_id, {}, false, reason);
}

void
ChatPage::joinRoomVia(const std::string &room_id,
                      const std::vector<std::string> &via,
                      bool promptForConfirmation,
                      const QString &reason)
{
    if (promptForConfirmation) {
        auto prompt = new RoomSummary(room_id, via, reason);
        QQmlEngine::setObjectOwnership(prompt, QQmlEngine::JavaScriptOwnership);
        emit showRoomJoinPrompt(prompt);
        return;
    }

    const auto handleId = requireMatrixBackendHandle(this, "join a room");
    if (!handleId)
        return;

    const auto requestedRoomId = QString::fromStdString(room_id);
    runMatrixRuntimeTask(
      this,
      [handleId = *handleId, requestedRoomId, via = toQStringVector(via), reason]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          return komai::MatrixBackendRuntimeService::joinRoom(
            context, handleId, requestedRoomId, via, reason);
      },
      [requestedRoomId, reason, via](ChatPage *page, const komai::MatrixJoinRoomResult &result) {
          if (!result.ok) {
              if (result.matrixErrcode == QLatin1String("M_FORBIDDEN")) {
                  Q_EMIT page->internalKnock(requestedRoomId, via, reason, true, true);
              } else {
                  Q_EMIT page->showNotification(
                    ChatPage::tr("Failed to join room: %1").arg(result.error));
              }
              return;
          }

          page->view_manager_->rooms()->setCurrentRoom(result.roomId);
      });
}

void
ChatPage::createRoom(const komai::MatrixCreateRoomRequest &request)
{
    if (request.roomAliasLocalpart.contains(u':') || request.roomAliasLocalpart.contains(u'#')) {
        komai::logging::net()->warn(
          "Failed to create room: Some characters are not allowed in alias");
        emit this->showNotification(tr("Room creation failed: Bad Alias"));
        return;
    }

    const auto handleId = requireMatrixBackendHandle(this, "create a room");
    if (!handleId)
        return;

    runMatrixRuntimeTask(
      this,
      [handleId = *handleId, request]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          return std::make_pair(
            komai::MatrixBackendRuntimeService::createRoom(context, handleId, request, &error),
            error);
      },
      [](ChatPage *page, const auto &result) {
          const auto &[roomId, error] = result;
          if (!roomId.has_value()) {
              Q_EMIT page->showNotification(ChatPage::tr("Room creation failed: %1").arg(error));
              return;
          }

          Q_EMIT page->newRoom(*roomId);
          Q_EMIT page->changeToRoom(*roomId);
      });
}

void
ChatPage::changeRoom(const QString &room_id)
{
    view_manager_->rooms()->setCurrentRoom(room_id);
}

void
ChatPage::inviteUser(const QString &room, QString userid, QString reason)
{
    const auto handleId = requireMatrixBackendHandle(this, "invite a user");
    if (!handleId)
        return;

    runMatrixRuntimeTask(
      this,
      [handleId = *handleId, room, userid, reason]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::inviteUser(
            context, handleId, room, userid, reason.trimmed(), &error);
          return std::make_pair(ok, error);
      },
      [userid, room](ChatPage *page, const auto &result) {
          const auto &[ok, error] = result;
          if (!ok) {
              komai::logging::ui()->warn("Failed to invite {} to {} via matrix-sdk runtime: {}",
                                         userid.toStdString(),
                                         room.toStdString(),
                                         error.toStdString());
              Q_EMIT page->showNotification(
                ChatPage::tr("Failed to invite %1 to %2: %3").arg(userid, room, error));
          }
          if (page->view_manager_)
              Q_EMIT page->view_manager_->roomMembersChanged(room);
      });
}

void
ChatPage::kickUser(const QString &room, QString userid, QString reason)
{
    const auto handleId = requireMatrixBackendHandle(this, "kick a user");
    if (!handleId)
        return;

    runMatrixRuntimeTask(
      this,
      [handleId = *handleId, room, userid, reason]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::kickUser(
            context, handleId, room, userid, reason.trimmed(), &error);
          return std::make_pair(ok, error);
      },
      [userid, room](ChatPage *page, const auto &result) {
          const auto &[ok, error] = result;
          if (!ok) {
              Q_EMIT page->showNotification(
                ChatPage::tr("Failed to kick %1 from %2: %3").arg(userid, room, error));
              return;
          }

          Q_EMIT page->showNotification(ChatPage::tr("Kicked user: %1").arg(userid));
      });
}

void
ChatPage::banUser(const QString &room, QString userid, QString reason)
{
    const auto handleId = requireMatrixBackendHandle(this, "ban a user");
    if (!handleId)
        return;

    runMatrixRuntimeTask(
      this,
      [handleId = *handleId, room, userid, reason]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::banUser(
            context, handleId, room, userid, reason.trimmed(), &error);
          return std::make_pair(ok, error);
      },
      [userid, room](ChatPage *page, const auto &result) {
          const auto &[ok, error] = result;
          if (!ok) {
              Q_EMIT page->showNotification(
                ChatPage::tr("Failed to ban %1 in %2: %3").arg(userid, room, error));
              return;
          }

          Q_EMIT page->showNotification(ChatPage::tr("Banned user: %1").arg(userid));
      });
}

void
ChatPage::unbanUser(const QString &room, QString userid, QString reason)
{
    const auto handleId = requireMatrixBackendHandle(this, "unban a user");
    if (!handleId)
        return;

    runMatrixRuntimeTask(
      this,
      [handleId = *handleId, room, userid, reason]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::unbanUser(
            context, handleId, room, userid, reason.trimmed(), &error);
          return std::make_pair(ok, error);
      },
      [userid, room](ChatPage *page, const auto &result) {
          const auto &[ok, error] = result;
          if (!ok) {
              Q_EMIT page->showNotification(
                ChatPage::tr("Failed to unban %1 in %2: %3").arg(userid, room, error));
              return;
          }

          Q_EMIT page->showNotification(ChatPage::tr("Unbanned user: %1").arg(userid));
      });
}

void
ChatPage::startChat(QString userid, std::optional<bool> encryptionEnabled)
{
    const auto handleId = requireMatrixBackendHandle(this, "start a direct chat");
    if (!handleId)
        return;

    if (auto *roomsModel = view_manager_ ? view_manager_->rooms() : nullptr) {
        const auto &rooms = roomsModel->matrixJoinedRooms();
        for (auto it = rooms.cbegin(); it != rooms.cend(); ++it) {
            const auto &room = it.value();
            if (room.isDirect && !room.isSpace && room.directChatOtherUserId == userid) {
                roomsModel->setCurrentRoom(it.key());
                return;
            }
        }
    }

    komai::MatrixCreateRoomRequest request;
    request.preset   = komai::MatrixCreateRoomPreset::TrustedPrivateChat;
    request.isPublic = false;

    if (!encryptionEnabled.has_value())
        encryptionEnabled = true;

    request.isEncrypted = encryptionEnabled.value_or(false);

    if (utils::localUser() != userid) {
        request.inviteUserIds = {userid};
        request.isDirect      = true;
    }
    emit ChatPage::instance()->createRoom(request);
}

bool
ChatPage::tryHandleMatrixUri(QString uri)
{
    const QUrl parsedUri(uri);
    const bool hasMatrixScheme = parsedUri.scheme() == QLatin1String("matrix");
    const bool isMatrixToLink  = parsedUri.scheme() == QLatin1String("https") &&
                                parsedUri.host() == QLatin1String("matrix.to");

    if (!hasMatrixScheme && !isMatrixToLink)
        return false;

    komai::logging::ui()->debug("Received matrix uri: {}", uri.toStdString());

    auto m = utils::parseMatrixUri(uri);

    if (!m) {
        komai::logging::ui()->warn("Failed to parse matrix uri: {}", uri.toStdString());
        return false;
    }

    const auto &[sigil1, mxid1, sigil2, mxid2, action, vias] = *m;
    const auto matrixBackendHandleId =
      MainWindow::instance() ? MainWindow::instance()->matrixBackendHandleId() : 0;

    if (sigil1 == u"u") {
        if (action.isEmpty()) {
            emit view_manager_->openGlobalUserProfile(mxid1);
        } else if (action == u"chat") {
            this->startChat(mxid1);
        }
        return true;
    } else if (sigil1 == u"roomid") {
        if (matrixBackendHandleId != 0) {
            if (auto *roomsModel = view_manager_ ? view_manager_->rooms() : nullptr;
                roomsModel && roomsModel->matrixJoinedRooms().contains(mxid1)) {
                roomsModel->setCurrentRoom(mxid1);
                if (!mxid2.isEmpty())
                    view_manager_->showEvent(mxid1, mxid2);
                return true;
            }

            if (action == u"join" || action.isEmpty()) {
                joinRoomVia(mxid1.toStdString(), vias);
                return true;
            }

            if (action == u"knock" || action.isEmpty()) {
                knockRoom(mxid1, vias);
                return true;
            }

            return false;
        }

        if (action == u"join" || action.isEmpty()) {
            joinRoomVia(mxid1.toStdString(), vias);
            return true;
        }
        if (action == u"knock" || action.isEmpty()) {
            knockRoom(mxid1, vias);
            return true;
        }
        return false;
    } else if (sigil1 == u"r") {
        if (matrixBackendHandleId != 0) {
            if (auto *roomsModel = view_manager_ ? view_manager_->rooms() : nullptr) {
                const auto &rooms = roomsModel->matrixJoinedRooms();
                for (auto it = rooms.cbegin(); it != rooms.cend(); ++it) {
                    if (it.value().roomAlias == mxid1) {
                        roomsModel->setCurrentRoom(it.key());
                        if (!mxid2.isEmpty())
                            view_manager_->showEvent(it.key(), mxid2);
                        return true;
                    }
                }
            }
        }

        if (action == u"join" || action.isEmpty()) {
            joinRoomVia(mxid1.toStdString(), vias);
            return true;
        } else if (action == u"knock" || action.isEmpty()) {
            knockRoom(mxid1, vias);
            return true;
        }
        return false;
    }
    return false;
}

void
ChatPage::sendNotificationReply(const QString &roomid, const QString &eventid, const QString &body)
{
    view_manager_->queueReply(roomid, eventid, body);
    clearRoomNotifications(roomid);

    auto exWin = MainWindow::instance()->windowForRoom(roomid);
    if (exWin) {
        exWin->setVisible(true);
        exWin->raise();
        exWin->requestActivate();
    } else {
        view_manager_->rooms()->setCurrentRoom(roomid);
        MainWindow::instance()->setVisible(true);
        MainWindow::instance()->raise();
        MainWindow::instance()->requestActivate();
    }
}

bool
ChatPage::tryHandleMatrixUri(const QUrl &uri)
{
    return tryHandleMatrixUri(uri.toString(QUrl::ComponentFormattingOption::FullyEncoded).toUtf8());
}

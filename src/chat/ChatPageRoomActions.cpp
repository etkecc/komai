// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chat/ChatPage.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QQmlEngine>
#include <QUrl>

#include <algorithm>
#include <mtx/requests.hpp>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "cache/Cache.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineModel.h"
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

    nhlog::ui()->warn("Cannot {} because no active matrix-sdk runtime handle exists", action);
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

QString
displayNameOrUserId(const QString &roomId, const QString &userId)
{
    if (!cache::isInitialized())
        return userId;

    const auto displayName = cache::displayName(roomId, userId).trimmed();
    return displayName.isEmpty() ? userId : displayName;
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

komai::MatrixCreateRoomRequest
toMatrixCreateRoomRequest(const mtx::requests::CreateRoom &request)
{
    komai::MatrixCreateRoomRequest result;
    result.name               = QString::fromStdString(request.name);
    result.topic              = QString::fromStdString(request.topic);
    result.roomAliasLocalpart = QString::fromStdString(request.room_alias_name);
    result.preset             = komai::MatrixCreateRoomPreset::PrivateChat;
    result.isDirect           = request.is_direct;
    result.isSpace            = request.creation_content.has_value() &&
                     request.creation_content->type == mtx::events::state::room_type::space;
    result.isPublic = request.visibility == mtx::common::RoomVisibility::Public;

    result.inviteUserIds.reserve(static_cast<qsizetype>(request.invite.size()));
    for (const auto &userId : request.invite)
        result.inviteUserIds.push_back(QString::fromStdString(userId));

    switch (request.preset) {
    case mtx::requests::Preset::PublicChat:
        result.preset = komai::MatrixCreateRoomPreset::PublicChat;
        break;
    case mtx::requests::Preset::TrustedPrivateChat:
        result.preset = komai::MatrixCreateRoomPreset::TrustedPrivateChat;
        break;
    case mtx::requests::Preset::PrivateChat:
    default:
        result.preset = komai::MatrixCreateRoomPreset::PrivateChat;
        break;
    }

    result.isEncrypted = std::any_of(
      request.initial_state.cbegin(), request.initial_state.cend(), [](const auto &event) {
          return std::holds_alternative<mtx::events::StrippedEvent<mtx::events::state::Encryption>>(
            event);
      });

    return result;
}

std::optional<QVector<komai::MatrixRoomSummary>>
fetchMatrixRoomList(uint64_t handleId)
{
    QString error;
    const auto roomList = komai::MatrixBackendRuntimeService::fetchRoomList(handleId, &error);
    if (roomList.has_value())
        return roomList;

    nhlog::ui()->warn("Failed to fetch matrix-sdk room list snapshot for handle {}: {}",
                      handleId,
                      error.toStdString());
    return std::nullopt;
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
          QString error;
          return std::make_pair(
            komai::MatrixBackendRuntimeService::knockRoom(handleId, room, via, reason, &error),
            error);
      },
      [room](ChatPage *page, const auto &result) {
          const auto &[roomId, error] = result;
          if (!roomId.has_value()) {
              Q_EMIT page->showNotification(ChatPage::tr("Failed to knock room: %1").arg(error));
              return;
          }

          nhlog::ui()->info("Knocked on room '{}' via matrix-sdk runtime", roomId->toStdString());
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
          return komai::MatrixBackendRuntimeService::joinRoom(
            handleId, requestedRoomId, via, reason);
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
ChatPage::createRoom(const mtx::requests::CreateRoom &req)
{
    if (req.room_alias_name.find(":") != std::string::npos ||
        req.room_alias_name.find("#") != std::string::npos) {
        nhlog::net()->warn("Failed to create room: Some characters are not allowed in alias");
        emit this->showNotification(tr("Room creation failed: Bad Alias"));
        return;
    }

    const auto handleId = requireMatrixBackendHandle(this, "create a room");
    if (!handleId)
        return;

    const auto request = toMatrixCreateRoomRequest(req);
    runMatrixRuntimeTask(
      this,
      [handleId = *handleId, request]() {
          QString error;
          return std::make_pair(
            komai::MatrixBackendRuntimeService::createRoom(handleId, request, &error), error);
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
ChatPage::leaveRoom(const QString &room_id, const QString &reason)
{
    const auto handleId = requireMatrixBackendHandle(this, "leave a room");
    if (!handleId)
        return;

    runMatrixRuntimeTask(
      this,
      [handleId = *handleId, room_id, reason]() {
          QString error;
          const bool ok =
            komai::MatrixBackendRuntimeService::leaveRoom(handleId, room_id, reason, &error);
          return std::make_pair(ok, error);
      },
      [room_id](ChatPage *page, const auto &result) {
          const auto &[ok, error] = result;
          if (!ok) {
              Q_EMIT page->showNotification(ChatPage::tr("Failed to leave room: %1").arg(error));
              nhlog::ui()->warn("Failed to leave room '{}' via matrix-sdk runtime: {}",
                                room_id.toStdString(),
                                error.toStdString());
              return;
          }

          Q_EMIT page->leftRoom(room_id);
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
    if (QMessageBox::question(nullptr,
                              tr("Confirm invite"),
                              tr("Do you really want to invite %1 (%2)?")
                                .arg(displayNameOrUserId(room, userid), userid)) !=
        QMessageBox::Yes)
        return;

    const auto handleId = requireMatrixBackendHandle(this, "invite a user");
    if (!handleId)
        return;

    runMatrixRuntimeTask(
      this,
      [handleId = *handleId, room, userid, reason]() {
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::inviteUser(
            handleId, room, userid, reason.trimmed(), &error);
          return std::make_pair(ok, error);
      },
      [userid, room](ChatPage *page, const auto &result) {
          const auto &[ok, error] = result;
          if (!ok) {
              nhlog::ui()->warn("Failed to invite {} to {} via matrix-sdk runtime: {}",
                                userid.toStdString(),
                                room.toStdString(),
                                error.toStdString());
              Q_EMIT page->showNotification(
                ChatPage::tr("Failed to invite %1 to %2: %3").arg(userid, room, error));
              return;
          }

          Q_EMIT page->showNotification(ChatPage::tr("Invited user: %1").arg(userid));
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
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::kickUser(
            handleId, room, userid, reason.trimmed(), &error);
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
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::banUser(
            handleId, room, userid, reason.trimmed(), &error);
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
    if (QMessageBox::question(nullptr,
                              tr("Confirm unban"),
                              tr("Do you really want to unban %1 (%2)?")
                                .arg(displayNameOrUserId(room, userid).toHtmlEscaped(),
                                     userid.toHtmlEscaped())) != QMessageBox::Yes)
        return;

    const auto handleId = requireMatrixBackendHandle(this, "unban a user");
    if (!handleId)
        return;

    runMatrixRuntimeTask(
      this,
      [handleId = *handleId, room, userid, reason]() {
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::unbanUser(
            handleId, room, userid, reason.trimmed(), &error);
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

    if (const auto rooms = fetchMatrixRoomList(*handleId); rooms.has_value()) {
        const auto existingRoom =
          std::find_if(rooms->cbegin(), rooms->cend(), [&userid](const auto &room) {
              return room.isDirect && !room.isInvite && !room.isSpace &&
                     room.directChatOtherUserId == userid;
          });
        if (existingRoom != rooms->cend()) {
            view_manager_->rooms()->setCurrentRoom(existingRoom->roomId);
            return;
        }
    }

    if (QMessageBox::Yes !=
        QMessageBox::question(
          nullptr,
          tr("Confirm invite"),
          tr("Do you really want to start a private chat with %1?").arg(userid)))
        return;

    mtx::requests::CreateRoom req;
    req.preset     = mtx::requests::Preset::TrustedPrivateChat;
    req.visibility = mtx::common::RoomVisibility::Private;

    if (!encryptionEnabled.has_value()) {
        if (cache::isInitialized()) {
            if (auto keys = cache::userKeys(userid.toStdString()))
                encryptionEnabled = !keys->device_keys.empty();
        } else {
            encryptionEnabled = true;
        }
    }

    if (encryptionEnabled.value_or(false)) {
        mtx::events::StrippedEvent<mtx::events::state::Encryption> enc;
        enc.type              = mtx::events::EventType::RoomEncryption;
        enc.content.algorithm = mtx::crypto::MEGOLM_ALGO;
        req.initial_state.emplace_back(std::move(enc));
    }

    if (utils::localUser() != userid) {
        req.invite    = {userid.toStdString()};
        req.is_direct = true;
    }
    emit ChatPage::instance()->createRoom(req);
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

    nhlog::ui()->debug("Received matrix uri: {}", uri.toStdString());

    auto m = utils::parseMatrixUri(uri);

    if (!m) {
        nhlog::ui()->warn("Failed to parse matrix uri: {}", uri.toStdString());
        return false;
    }

    const auto &[sigil1, mxid1, sigil2, mxid2, action, vias] = *m;
    const auto matrixBackendHandleId =
      MainWindow::instance() ? MainWindow::instance()->matrixBackendHandleId() : 0;

    if (sigil1 == u"u") {
        if (action.isEmpty()) {
            auto t = MainWindow::instance()->focusedRoom();
            if (!t.isEmpty() && cache::isInitialized() &&
                cache::isRoomMember(mxid1.toStdString(), t.toStdString())) {
                auto rm = view_manager_->rooms()->getRoomById(t);
                if (rm)
                    rm->openUserProfile(mxid1);
                return true;
            }
            emit view_manager_->openGlobalUserProfile(mxid1);
        } else if (action == u"chat") {
            this->startChat(mxid1);
        }
        return true;
    } else if (sigil1 == u"roomid") {
        if (matrixBackendHandleId != 0) {
            if (const auto rooms = fetchMatrixRoomList(matrixBackendHandleId); rooms.has_value()) {
                const auto existingRoom =
                  std::find_if(rooms->cbegin(), rooms->cend(), [&mxid1](const auto &room) {
                      return room.roomId == mxid1 && !room.isInvite;
                  });
                if (existingRoom != rooms->cend()) {
                    view_manager_->rooms()->setCurrentRoom(existingRoom->roomId);
                    if (!mxid2.isEmpty())
                        view_manager_->showEvent(existingRoom->roomId, mxid2);
                    return true;
                }
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

        if (cache::isInitialized()) {
            auto joined_rooms = cache::joinedRooms();
            auto targetRoomId = mxid1.toStdString();

            for (const auto &roomid : joined_rooms) {
                if (roomid == targetRoomId) {
                    view_manager_->rooms()->setCurrentRoom(mxid1);
                    if (!mxid2.isEmpty())
                        view_manager_->showEvent(mxid1, mxid2);
                    return true;
                }
            }
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
        if (matrixBackendHandleId == 0 && cache::isInitialized()) {
            auto joined_rooms    = cache::joinedRooms();
            auto targetRoomAlias = mxid1.toStdString();

            for (const auto &roomid : joined_rooms) {
                auto aliases = cache::getStateEvent<mtx::events::state::CanonicalAlias>(roomid);
                if (aliases) {
                    if (aliases->content.alias == targetRoomAlias) {
                        view_manager_->rooms()->setCurrentRoom(QString::fromStdString(roomid));
                        if (!mxid2.isEmpty())
                            view_manager_->showEvent(QString::fromStdString(roomid), mxid2);
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

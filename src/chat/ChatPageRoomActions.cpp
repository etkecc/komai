// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ChatPage.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QQmlEngine>
#include <QUrl>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <mtx/responses.hpp>
#include <mtxclient/crypto/client.hpp>

#include "Logging.h"
#include "MainWindow.h"
#include "MatrixClient.h"
#include "Utils.h"
#include "cache/Cache.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineModel.h"
#include "timeline/TimelineViewManager.h"
#include "ui/RoomSummary.h"

void
ChatPage::knockRoom(const QString &room,
                    const std::vector<std::string> &via,
                    QString reason,
                    bool failedJoin,
                    bool promptForConfirmation)
{
    const auto room_id = room.toStdString();
    bool confirmed     = false;
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

    http::client()->knock_room(
      room_id,
      via,
      [this, room_id](const mtx::responses::RoomId &, mtx::http::RequestErr err) {
          if (err) {
              emit showNotification(tr("Failed to knock room: %1")
                                      .arg(QString::fromStdString(err->matrix_error.error)));
              return;
          }
      },
      reason.toStdString());
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

    http::client()->join_room(
      room_id,
      via,
      [this, room_id, reason, via](const mtx::responses::RoomId &, mtx::http::RequestErr err) {
          if (err) {
              if (err->matrix_error.errcode == mtx::errors::ErrorCode::M_FORBIDDEN)
                  emit internalKnock(QString::fromStdString(room_id), via, reason, true, true);
              else
                  emit showNotification(tr("Failed to join room: %1")
                                          .arg(QString::fromStdString(err->matrix_error.error)));
              return;
          }

          // We remove any invites with the same room_id.
          try {
              cache::removeInvite(room_id);
          } catch (const std::exception &e) {
              emit showNotification(tr("Failed to remove invite: %1").arg(e.what()));
          }

          view_manager_->rooms()->setCurrentRoom(QString::fromStdString(room_id));
      },
      reason.toStdString());
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

    bool direct = req.is_direct;
    std::string direct_user;
    if (direct && !req.invite.empty())
        direct_user = req.invite.front();

    http::client()->create_room(
      req,
      [this, direct, direct_user](const mtx::responses::CreateRoom &res,
                                  mtx::http::RequestErr err) {
          if (err) {
              const auto err_code = mtx::errors::to_string(err->matrix_error.errcode);
              const auto error    = err->matrix_error.error;

              nhlog::net()->warn("failed to create room: {})", err);

              emit showNotification(
                tr("Room creation failed: %1").arg(QString::fromStdString(error)));
              return;
          }

          QString newRoomId = QString::fromStdString(res.room_id.to_string());

          if (direct && !direct_user.empty()) {
              utils::markRoomAsDirect(newRoomId,
                                      {RoomMember{
                                        .user_id      = QString::fromStdString(direct_user),
                                        .display_name = "",
                                        .avatar_url   = "",
                                      }});
          }

          emit showNotification(tr("Room %1 created.").arg(newRoomId));
          emit newRoom(newRoomId);
          emit changeToRoom(newRoomId);
      });
}

void
ChatPage::leaveRoom(const QString &room_id, const QString &reason)
{
    http::client()->leave_room(
      room_id.toStdString(),
      [this, room_id](const mtx::responses::Empty &, mtx::http::RequestErr err) {
          if (err) {
              emit showNotification(tr("Failed to leave room: %1")
                                      .arg(QString::fromStdString(err->matrix_error.error)));
              nhlog::net()->error("Failed to leave room '{}': {}", room_id.toStdString(), err);

              if (err->status_code == 404 &&
                  err->matrix_error.errcode == mtx::errors::ErrorCode::M_UNKNOWN) {
                  nhlog::db()->debug(
                    "Removing invite and room for {}, even though we couldn't leave.",
                    room_id.toStdString());
                  cache::removeInvite(room_id.toStdString());
                  cache::removeRoom(room_id.toStdString());
              }
              return;
          }

          emit leftRoom(room_id);
      },
      reason.toStdString());
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
                                .arg(cache::displayName(room, userid), userid)) != QMessageBox::Yes)
        return;

    http::client()->invite_user(
      room.toStdString(),
      userid.toStdString(),
      [this, userid, room](const mtx::responses::Empty &, mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->error(
                "Failed to invite {} to {}: {}", userid.toStdString(), room.toStdString(), *err);
              emit showNotification(
                tr("Failed to invite %1 to %2: %3")
                  .arg(userid, room, QString::fromStdString(err->matrix_error.error)));
          } else
              emit showNotification(tr("Invited user: %1").arg(userid));
      },
      reason.trimmed().toStdString());
}

void
ChatPage::kickUser(const QString &room, QString userid, QString reason)
{
    bool confirmed;
    reason = QInputDialog::getText(
      nullptr,
      tr("Reason for the kick"),
      tr("Enter reason for kicking %1 (%2) or hit enter for no reason:")
        .arg(cache::displayName(room, userid).toHtmlEscaped(), userid.toHtmlEscaped()),
      QLineEdit::Normal,
      reason,
      &confirmed);
    if (!confirmed) {
        return;
    }

    http::client()->kick_user(
      room.toStdString(),
      userid.toStdString(),
      [this, userid, room](const mtx::responses::Empty &, mtx::http::RequestErr err) {
          if (err) {
              emit showNotification(
                tr("Failed to kick %1 from %2: %3")
                  .arg(userid, room, QString::fromStdString(err->matrix_error.error)));
          } else
              emit showNotification(tr("Kicked user: %1").arg(userid));
      },
      reason.trimmed().toStdString());
}

void
ChatPage::banUser(const QString &room, QString userid, QString reason)
{
    bool confirmed;
    reason = QInputDialog::getText(
      nullptr,
      tr("Reason for the ban"),
      tr("Enter reason for banning %1 (%2) or hit enter for no reason:")
        .arg(cache::displayName(room, userid).toHtmlEscaped(), userid.toHtmlEscaped()),
      QLineEdit::Normal,
      reason,
      &confirmed);
    if (!confirmed) {
        return;
    }

    http::client()->ban_user(
      room.toStdString(),
      userid.toStdString(),
      [this, userid, room](const mtx::responses::Empty &, mtx::http::RequestErr err) {
          if (err) {
              emit showNotification(
                tr("Failed to ban %1 in %2: %3")
                  .arg(userid, room, QString::fromStdString(err->matrix_error.error)));
          } else
              emit showNotification(tr("Banned user: %1").arg(userid));
      },
      reason.trimmed().toStdString());
}

void
ChatPage::unbanUser(const QString &room, QString userid, QString reason)
{
    if (QMessageBox::question(nullptr,
                              tr("Confirm unban"),
                              tr("Do you really want to unban %1 (%2)?")
                                .arg(cache::displayName(room, userid).toHtmlEscaped(),
                                     userid.toHtmlEscaped())) != QMessageBox::Yes)
        return;

    http::client()->unban_user(
      room.toStdString(),
      userid.toStdString(),
      [this, userid, room](const mtx::responses::Empty &, mtx::http::RequestErr err) {
          if (err) {
              emit showNotification(
                tr("Failed to unban %1 in %2: %3")
                  .arg(userid, room, QString::fromStdString(err->matrix_error.error)));
          } else
              emit showNotification(tr("Unbanned user: %1").arg(userid));
      },
      reason.trimmed().toStdString());
}

void
ChatPage::startChat(QString userid, std::optional<bool> encryptionEnabled)
{
    auto joined_rooms = cache::joinedRooms();
    auto room_infos   = cache::getRoomInfo(joined_rooms);

    for (const std::string &room_id : joined_rooms) {
        if (const auto &info = room_infos[QString::fromStdString(room_id)];
            info.member_count == 2 && !info.is_space) {
            auto room_members = cache::roomMembers(room_id);
            if (std::find(room_members.begin(), room_members.end(), userid.toStdString()) !=
                room_members.end()) {
                view_manager_->rooms()->setCurrentRoom(QString::fromStdString(room_id));
                return;
            }
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
        if (auto keys = cache::userKeys(userid.toStdString()))
            encryptionEnabled = !keys->device_keys.empty();
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

    if (sigil1 == u"u") {
        if (action.isEmpty()) {
            auto t = MainWindow::instance()->focusedRoom();
            if (!t.isEmpty() && cache::isRoomMember(mxid1.toStdString(), t.toStdString())) {
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

        if (action == u"join" || action.isEmpty()) {
            joinRoomVia(targetRoomId, vias);
            return true;
        } else if (action == u"knock" || action.isEmpty()) {
            knockRoom(mxid1, vias);
            return true;
        }
        return false;
    } else if (sigil1 == u"r") {
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

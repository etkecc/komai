// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include <chrono>
#include <thread>

#include <QTimer>

#include "Logging.h"
#include "MatrixClient.h"
#include "TimelineViewManager.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "events/EventAccessors.h"
#include "models/ReadReceiptsModel.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/rawmessage/RawMessageDialogPayload.h"
#include "ui/Theme.h"
#include "ui/UserProfile.h"
#include "utils/Utils.h"

void
TimelineModel::viewRawMessage(const QString &id)
{
    auto e = events.get(id.toStdString(), "", false);
    if (!e)
        return;

    const auto eventJson       = mtx::accessors::serialize_event(*e);
    const auto timelinePalette = Theme::paletteFromTheme(UserSettings::instance()->uiThemeSlug());
    const auto dialogPayload =
      timeline::rawmessage::buildRawMessageDialogPayload(eventJson, timelinePalette);
    emit showRawMessageDialog(dialogPayload.renderedRawMessage,
                              dialogPayload.rawMessageJson,
                              dialogPayload.rawMessageBody,
                              dialogPayload.rawMessageFormattedBody);
}

void
TimelineModel::forwardMessage(const QString &eventId, QString roomId)
{
    auto e = events.get(eventId.toStdString(), "");
    if (!e)
        return;

    emit forwardToRoom(e, std::move(roomId));
}

void
TimelineModel::viewDecryptedRawMessage(const QString &id)
{
    auto e = events.get(id.toStdString(), "");
    if (!e)
        return;

    const auto eventJson       = mtx::accessors::serialize_event(*e);
    const auto timelinePalette = Theme::paletteFromTheme(UserSettings::instance()->uiThemeSlug());
    const auto dialogPayload =
      timeline::rawmessage::buildRawMessageDialogPayload(eventJson, timelinePalette);
    emit showRawMessageDialog(dialogPayload.renderedRawMessage,
                              dialogPayload.rawMessageJson,
                              dialogPayload.rawMessageBody,
                              dialogPayload.rawMessageFormattedBody);
}

void
TimelineModel::openUserProfile(QString userid)
{
    UserProfile *userProfile = new UserProfile(room_id_, std::move(userid), manager_, this);
    connect(this, &TimelineModel::roomAvatarUrlChanged, userProfile, &UserProfile::updateAvatarUrl);
    emit manager_->openProfile(userProfile);
}

void
TimelineModel::unpin(const QString &id)
{
    auto pinned = cache::getStateEvent<mtx::events::state::PinnedEvents>(room_id_.toStdString());

    mtx::events::state::PinnedEvents content{};
    if (pinned)
        content = pinned->content;

    auto idStr = id.toStdString();

    for (auto it = content.pinned.begin(); it != content.pinned.end(); ++it) {
        if (*it == idStr) {
            content.pinned.erase(it);
            break;
        }
    }

    http::client()->send_state_event(
      room_id_.toStdString(),
      content,
      [idStr](const mtx::responses::EventId &, mtx::http::RequestErr err) {
          if (err)
              nhlog::net()->error("Failed to unpin {}: {}", idStr, *err);
          else
              nhlog::net()->debug("Unpinned {}", idStr);
      });
}

void
TimelineModel::pin(const QString &id)
{
    auto pinned = cache::getStateEvent<mtx::events::state::PinnedEvents>(room_id_.toStdString());

    mtx::events::state::PinnedEvents content{};
    if (pinned)
        content = pinned->content;

    auto idStr = id.toStdString();
    content.pinned.push_back(idStr);

    http::client()->send_state_event(
      room_id_.toStdString(),
      content,
      [idStr](const mtx::responses::EventId &, mtx::http::RequestErr err) {
          if (err)
              nhlog::net()->error("Failed to pin {}: {}", idStr, *err);
          else
              nhlog::net()->debug("Pinned {}", idStr);
      });
}

RelatedInfo
TimelineModel::relatedInfo(const QString &id)
{
    auto event = events.get(id.toStdString(), "");
    if (!event)
        return {};

    return utils::stripReplyFallbacks(*event, id.toStdString(), room_id_);
}

void
TimelineModel::showReadReceipts(const QString &id)
{
    emit openReadReceiptsDialog(new ReadReceiptsProxy{id, roomId(), this});
}

void
TimelineModel::redactAllFromUser(const QString &userid, const QString &reason)
{
    auto user = userid.toStdString();
    std::vector<QString> toRedact;
    for (auto it = events.size() - 1; it >= 0; --it) {
        auto event = events.get(it, false);
        if (event && mtx::accessors::sender(*event) == user &&
            !std::holds_alternative<mtx::events::RoomEvent<mtx::events::msg::Redacted>>(*event)) {
            toRedact.push_back(QString::fromStdString(mtx::accessors::event_id(*event)));
        }
    }

    for (const auto &e : toRedact) {
        redactEvent(e, reason);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void
TimelineModel::reportEvent(const QString &eventId, const QString &reason, const int score)
{
    http::client()->report_event(
      room_id_.toStdString(), eventId.toStdString(), reason.toStdString(), score);
}

void
TimelineModel::redactEvent(const QString &id, const QString &reason)
{
    if (!id.isEmpty()) {
        auto edits = events.edits(id.toStdString());
        http::client()->redact_event(
          room_id_.toStdString(),
          id.toStdString(),
          [this, id, reason](const mtx::responses::EventId &, mtx::http::RequestErr err) {
              if (err) {
                  if (err->status_code == 429 && err->matrix_error.retry_after.count() != 0) {
                      ChatPage::instance()->callFunctionOnGuiThread(
                        [this, id, reason, interval = err->matrix_error.retry_after] {
                            QTimer::singleShot(interval * 2, this, [this, id, reason]() {
                                this->redactEvent(id, reason);
                            });
                        });
                      return;
                  }
                  emit redactionFailed(tr("Message redaction failed: %1")
                                         .arg(QString::fromStdString(err->matrix_error.error)));
                  return;
              }

              emit dataAtIdChanged(id);
          },
          reason.toStdString());

        // redact all edits to prevent leaks
        for (const auto &e : edits) {
            const auto &id_ = mtx::accessors::event_id(e);
            http::client()->redact_event(
              room_id_.toStdString(),
              id_,
              [this, id, id_](const mtx::responses::EventId &, mtx::http::RequestErr err) {
                  if (err) {
                      emit redactionFailed(tr("Message redaction failed: %1")
                                             .arg(QString::fromStdString(err->matrix_error.error)));
                      return;
                  }

                  emit dataAtIdChanged(id);
              },
              reason.toStdString());
        }
    }
}

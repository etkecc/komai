// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include <nlohmann/json.hpp>

#include "TimelineViewManager.h"
#include "chat/ChatPage.h"
#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "models/ReadReceiptsModel.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/rawmessage/RawMessageDialogPayload.h"
#include "ui/Theme.h"
#include "ui/UserProfile.h"
#include "utils/Utils.h"

namespace {
void
notifyLegacyTimelineActionUnavailable(const QString &roomId, const QString &action)
{
    nhlog::ui()->warn(
      "Refusing legacy timeline action '{}' in room '{}'; this flow is not migrated to the "
      "matrix-sdk backend yet",
      action.toStdString(),
      roomId.toStdString());
    emit ChatPage::instance()->showNotification(
      QObject::tr("%1 from this legacy timeline is not migrated to the matrix-sdk backend yet.")
        .arg(action));
}
}

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
    (void)id;
    notifyLegacyTimelineActionUnavailable(room_id_, tr("Unpinning messages"));
}

void
TimelineModel::pin(const QString &id)
{
    (void)id;
    notifyLegacyTimelineActionUnavailable(room_id_, tr("Pinning messages"));
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
    (void)userid;
    (void)reason;
    notifyLegacyTimelineActionUnavailable(room_id_, tr("Deleting messages"));
}

void
TimelineModel::reportEvent(const QString &eventId, const QString &reason, const int score)
{
    (void)eventId;
    (void)reason;
    (void)score;
    notifyLegacyTimelineActionUnavailable(room_id_, tr("Reporting messages"));
}

void
TimelineModel::redactEvent(const QString &id, const QString &reason)
{
    (void)id;
    (void)reason;
    notifyLegacyTimelineActionUnavailable(room_id_, tr("Deleting messages"));
}

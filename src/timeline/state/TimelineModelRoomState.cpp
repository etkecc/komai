// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include <iterator>
#include <utility>

#include <QQmlEngine>
#include <QUrl>

#include "DirectChatResolver.h"
#include "cache/Cache.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "utils/Utils.h"

// Use the DM-aware display name so the room header shows the partner's name
// for bridged DM rooms, consistent with the room list sidebar.
QString
TimelineModel::roomName() const
{
    auto dmName = DirectChatResolver::instance().dmRoomDisplayName(room_id_);
    if (!dmName.isEmpty())
        return utils::replaceEmoji(dmName.toHtmlEscaped());

    auto info = cache::getRoomInfo({room_id_.toStdString()});

    if (!info.count(room_id_))
        return {};
    return utils::replaceEmoji(QString::fromStdString(info[room_id_].name).toHtmlEscaped());
}

// Plain variant of roomName() without HTML escaping, also DM-aware.
QString
TimelineModel::plainRoomName() const
{
    auto dmName = DirectChatResolver::instance().dmRoomDisplayName(room_id_);
    if (!dmName.isEmpty())
        return dmName;

    auto info = cache::getRoomInfo({room_id_.toStdString()});

    if (!info.count(room_id_))
        return {};
    return QString::fromStdString(info[room_id_].name);
}

QString
TimelineModel::roomAvatarUrl() const
{
    auto info = cache::getRoomInfo({room_id_.toStdString()});

    if (info.count(room_id_) && !info[room_id_].avatar_url.empty())
        return QString::fromStdString(info[room_id_].avatar_url);
    else {
        auto roomAvatar = cache::roomAvatarUrl(room_id_.toStdString());
        if (!roomAvatar.isEmpty())
            return roomAvatar;

        return {};
    }
}

QString
TimelineModel::roomTopic() const
{
    auto info = cache::getRoomInfo({room_id_.toStdString()});

    if (!info.count(room_id_))
        return {};
    else
        return utils::replaceEmoji(
          utils::linkifyMessage(QString::fromStdString(info[room_id_].topic).toHtmlEscaped()));
}

QStringList
TimelineModel::pinnedMessages() const
{
    auto pinned = cache::getStateEvent<mtx::events::state::PinnedEvents>(room_id_.toStdString());

    if (!pinned || pinned->content.pinned.empty())
        return {};

    QStringList list;
    list.reserve((int)pinned->content.pinned.size());
    for (const auto &p : pinned->content.pinned)
        list.push_back(QString::fromStdString(p));

    return list;
}

QStringList
TimelineModel::widgetLinks() const
{
    auto evs  = cache::getStateEventsWithType<mtx::events::state::Widget>(room_id_.toStdString());
    auto evs2 = cache::getStateEventsWithType<mtx::events::state::Widget>(
      room_id_.toStdString(), mtx::events::EventType::Widget);
    evs.insert(
      evs.end(), std::make_move_iterator(evs2.begin()), std::make_move_iterator(evs2.end()));

    if (evs.empty())
        return {};

    QStringList list;

    auto user = utils::localUser();
    // auto av   = QUrl::toPercentEncoding(
    //   QString::fromStdString(http::client()->mxc_to_download_url(avatarUrl(user).toStdString())));
    auto disp  = QUrl::toPercentEncoding(displayName(user));
    auto theme = UserSettings::instance()->uiThemeSlug();
    if (theme == QStringLiteral("system"))
        theme.clear();
    user = QUrl::toPercentEncoding(user);

    list.reserve((int)evs.size());
    for (const auto &p : evs) {
        auto url = QString::fromStdString(p.content.url);

        if (url.isEmpty())
            continue;

        for (const auto &[k, v] : p.content.data)
            url.replace("$" + QString::fromStdString(k),
                        QUrl::toPercentEncoding(QString::fromStdString(v)));

        url.replace("$matrix_user_id", user);
        url.replace("$matrix_room_id", QUrl::toPercentEncoding(room_id_));
        url.replace("$matrix_display_name", disp);
        // url.replace("$matrix_avatar_url", av);

        url.replace("$matrix_widget_id",
                    QUrl::toPercentEncoding(QString::fromStdString(p.content.id)));

        // url.replace("$matrix_client_theme", theme);
        url.replace("$org.matrix.msc2873.client_theme", theme);
        url.replace("$org.matrix.msc2873.client_id", "cc.etke.komai");

        // compat with some widgets, i.e. FOSDEM
        url.replace("$theme", theme);

        // See https://bugreports.qt.io/browse/QTBUG-110446
        // We want to make sure that urls are encoded, even if the source is untrustworthy.
        url = QUrl(url).toEncoded();

        list.push_back(
          QLatin1String("<a href='%1'>%2</a>")
            .arg(url,
                 QString::fromStdString(p.content.name.empty() ? p.state_key : p.content.name)
                   .toHtmlEscaped()));
    }

    return list;
}

crypto::Trust
TimelineModel::trustlevel() const
{
    if (!isEncrypted_)
        return crypto::Trust::Unverified;

    return cache::roomVerificationStatus(room_id_.toStdString());
}

int
TimelineModel::roomMemberCount() const
{
    return (int)cache::memberCount(room_id_.toStdString());
}

bool
TimelineModel::isDirect() const
{
    return DirectChatResolver::instance().isDirectChat(room_id_);
}

QString
TimelineModel::directChatOtherUserId() const
{
    return DirectChatResolver::instance().directChatPartner(room_id_);
}

mtx::pushrules::PushRuleEvaluator::RoomContext
TimelineModel::pushrulesRoomContext() const
{
    return mtx::pushrules::PushRuleEvaluator::RoomContext{
      .user_display_name =
        cache::displayName(room_id_.toStdString(), utils::localUser().toStdString()),
      .member_count = cache::memberCount(room_id_.toStdString()),
      .power_levels = permissions_.powerlevelEvent(),
    };
}

RoomSummary *
TimelineModel::parentSpace()
{
    if (!parentChecked) {
        auto parents = cache::getStateEventsWithType<mtx::events::state::space::Parent>(
          this->room_id_.toStdString());

        for (const auto &p : parents) {
            if (p.content.canonical and p.content.via and not p.content.via->empty()) {
                parentSummary.reset(new RoomSummary(p.state_key, *p.content.via, ""));
                QQmlEngine::setObjectOwnership(parentSummary.get(), QQmlEngine::CppOwnership);
                break;
            }
        }
        parentChecked = true;
    }

    return parentSummary.get();
}

bool
TimelineModel::showImage() const
{
    auto show = UserSettings::instance()->timelineMediaImageDisplay();

    switch (show) {
    case UserSettings::ShowImage::Always:
        return true;
    case UserSettings::ShowImage::OnlyPrivate: {
        auto accessRules =
          cache::getStateEvent<mtx::events::state::JoinRules>(room_id_.toStdString())
            .value_or(mtx::events::StateEvent<mtx::events::state::JoinRules>{})
            .content;

        return accessRules.join_rule != mtx::events::state::JoinRule::Public;
    }
    case UserSettings::ShowImage::Never:
    default:
        return false;
    }
}

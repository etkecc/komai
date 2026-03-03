// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include <algorithm>

#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QScreen>
#include <QTime>
#include <QUrl>
#include <QVariant>

#include "ChatPage.h"
#include "EventAccessors.h"
#include "FormattedCodeBlockHighlighter.h"
#include "Utils.h"
#include "cache/Cache.h"
#include "encryption/Olm.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/Theme.h"

QString
TimelineModel::formattedBodyForEvent(const mtx::events::collections::TimelineEvents &event) const
{
    const static QRegularExpression replyFallback(QStringLiteral("<mx-reply>.*</mx-reply>"),
                                                  QRegularExpression::DotMatchesEverythingOption);

    auto ascent = QFontMetrics(UserSettings::instance()->uiFontFamily()).ascent();

    bool isReply = mtx::accessors::relations(event).reply_to(false).has_value();

    auto formattedBody_ = QString::fromStdString(mtx::accessors::formatted_body(event));
    if (formattedBody_.isEmpty()) {
        // NOTE(Nico): replies without html can't have a fallback. If they do, eh, who cares.
        formattedBody_ = QString::fromStdString(mtx::accessors::body(event))
                           .toHtmlEscaped()
                           .replace('\n', QLatin1String("<br>"));
    } else if (isReply) {
        formattedBody_ = formattedBody_.remove(replyFallback);
    }
    formattedBody_ = utils::escapeBlacklistedHtml(formattedBody_);

    // TODO(Nico): Don't parse html with a regex
    const static QRegularExpression matchIsImg(QStringLiteral("<img [^>]+>"));
    auto itIsImg = matchIsImg.globalMatch(formattedBody_);
    while (itIsImg.hasNext()) {
        // The current <img> tag.
        const QString curImg = itIsImg.next().captured(0);
        // The replacement for the current <img>.
        auto imgReplacement = curImg;

        // Construct image parameters later used by MxcImageProvider.
        QString imgParams;
        if (curImg.contains(QLatin1String("height"))) {
            const static QRegularExpression matchImgHeight(
              QStringLiteral("height=([\"\']?)(\\d+)([\"\']?)"));
            // Make emoticons twice as high as the font.
            if (curImg.contains(QLatin1String("data-mx-emoticon"))) {
                imgReplacement =
                  imgReplacement.replace(matchImgHeight, "height=\\1%1\\3").arg(ascent * 2);
            }
            const auto height = matchImgHeight.match(imgReplacement).captured(2).toInt();
            imgParams         = QStringLiteral("?scale&height=%1").arg(height);
        }

        // Replace src in current <img>.
        const static QRegularExpression matchImgUri(QStringLiteral("src=\"mxc://([^\"]*)\""));
        imgReplacement.replace(matchImgUri,
                               QStringLiteral(R"(src="image://mxcImage/\1%1")").arg(imgParams));
        // Same regex but for single quotes around the src
        const static QRegularExpression matchImgUri2(QStringLiteral("src=\'mxc://([^\']*)\'"));
        imgReplacement.replace(matchImgUri2,
                               QStringLiteral("src=\'image://mxcImage/\\1%1\'").arg(imgParams));

        // Replace <img> in formattedBody_ with our new <img>.
        formattedBody_.replace(curImg, imgReplacement);
    }

    if (auto effectMessage =
          std::get_if<mtx::events::RoomEvent<mtx::events::msg::ElementEffect>>(&event)) {
        if (effectMessage->content.msgtype == std::string_view("nic.custom.confetti")) {
            formattedBody_.append(QUtf8StringView(u8"🎊"));
        } else if (effectMessage->content.msgtype ==
                   std::string_view("io.element.effect.rainfall")) {
            formattedBody_.append(QUtf8StringView(u8"🌧️"));
        }
    }

    const auto timelinePalette = Theme::paletteFromTheme(UserSettings::instance()->uiThemeSlug());
    formattedBody_             = timeline::highlightFormattedCodeBlocks(
      formattedBody_,
      timelinePalette,
      UserSettings::instance()->timelineFormattedCodeSyntaxHighlighting());

    return utils::replaceEmoji(utils::linkifyMessage(formattedBody_));
}

QString
TimelineModel::formattedStateEventForEvent(
  const mtx::events::collections::TimelineEvents &event) const
{
    if (!mtx::accessors::is_state_event(event))
        return QString();

    return std::visit(
      [this](const auto &e) {
          constexpr auto t = mtx::events::state_content_to_type<decltype(e.content)>;
          if constexpr (t == mtx::events::EventType::RoomServerAcl)
              return tr("%1 changed which servers are allowed in this room.")
                .arg(displayName(QString::fromStdString(e.sender)));
          else if constexpr (t == mtx::events::EventType::RoomName) {
              if (e.content.name.empty())
                  return tr("%1 removed the room name.")
                    .arg(displayName(QString::fromStdString(e.sender)));
              else
                  return tr("%1 changed the room name to: %2")
                    .arg(displayName(QString::fromStdString(e.sender)))
                    .arg(QString::fromStdString(e.content.name).toHtmlEscaped());
          } else if constexpr (t == mtx::events::EventType::RoomTopic) {
              if (e.content.topic.empty())
                  return tr("%1 removed the topic.")
                    .arg(displayName(QString::fromStdString(e.sender)));
              else
                  return tr("%1 changed the topic to: %2")
                    .arg(displayName(QString::fromStdString(e.sender)))
                    .arg(QString::fromStdString(e.content.topic).toHtmlEscaped());
          } else if constexpr (t == mtx::events::EventType::RoomAvatar) {
              if (e.content.url.starts_with("mxc://")) {
                  const auto compactMode   = UserSettings::instance()->uiLayoutCompactMode();
                  const auto uiFontMetrics = QFontMetricsF(QGuiApplication::font());
                  const int inlinePreviewLogicalPx = qMax(1, qRound(uiFontMetrics.height()));
                  int avatarThumbLogicalPx         = compactMode
                                                       ? qMax(1,
                                                      qCeil(uiFontMetrics.lineSpacing() * 1.25))
                                                       : 40; // matches Komai.avatarSize
                  if (avatarThumbLogicalPx > 1)
                      avatarThumbLogicalPx -= (avatarThumbLogicalPx % 2);

                  const auto screen       = QGuiApplication::primaryScreen();
                  const auto dpr          = screen ? screen->devicePixelRatio() : 1.0;
                  const int avatarThumbPx = qMax(1, qRound(avatarThumbLogicalPx * dpr));
                  // Match avatar rounding used in Avatar.qml + MxcImageProvider.
                  const int avatarCornerRadiusPercent =
                    UserSettings::instance()->uiAvatarsCircular() ? 100 : 25;

                  auto avatarMxcUrl = QString::fromStdString(e.content.url);
                  avatarMxcUrl.append("#room-avatar");
                  auto avatarPreviewUrl = QString::fromStdString(e.content.url);
                  avatarPreviewUrl.replace("mxc://", "image://MxcImage/");
                  avatarPreviewUrl.append(QStringLiteral("?height=%1&radius=%2")
                                            .arg(avatarThumbPx)
                                            .arg(avatarCornerRadiusPercent));

                  return tr("%1 changed the room avatar to: %2")
                    .arg(displayName(QString::fromStdString(e.sender)))
                    .arg(QStringLiteral("<a href=\"%1\"><img height=\"%2\" "
                                        "style=\"vertical-align:middle\" src=\"%3\"></a>")
                           .arg(avatarMxcUrl.toHtmlEscaped())
                           .arg(inlinePreviewLogicalPx)
                           .arg(QUrl::toPercentEncoding(avatarPreviewUrl, ":/?&=")));
              } else
                  return tr("%1 removed the room avatar.")
                    .arg(displayName(QString::fromStdString(e.sender)));
          } else if constexpr (t == mtx::events::EventType::RoomPinnedEvents)
              return tr("%1 changed the pinned messages.")
                .arg(displayName(QString::fromStdString(e.sender)));
          else if constexpr (t == mtx::events::EventType::RoomJoinRules)
              return formatJoinRuleEvent(e);
          else if constexpr (t == mtx::events::EventType::ImagePackInRoom)
              return formatImagePackEvent(e);
          else if constexpr (t == mtx::events::EventType::RoomCanonicalAlias)
              return tr("%1 changed the addresses for this room.")
                .arg(displayName(QString::fromStdString(e.sender)));
          else if constexpr (t == mtx::events::EventType::SpaceParent)
              return tr("%1 changed the parent communities for this room.")
                .arg(displayName(QString::fromStdString(e.sender)));
          else if constexpr (t == mtx::events::EventType::RoomCreate)
              return tr("%1 created and configured room: %2")
                .arg(displayName(QString::fromStdString(e.sender)))
                .arg(room_id_);
          else if constexpr (t == mtx::events::EventType::RoomPowerLevels)
              return formatPowerLevelEvent(e);
          else if constexpr (t == mtx::events::EventType::PolicyRuleRoom)
              return formatPolicyRule(QString::fromStdString(e.event_id));
          else if constexpr (t == mtx::events::EventType::PolicyRuleUser)
              return formatPolicyRule(QString::fromStdString(e.event_id));
          else if constexpr (t == mtx::events::EventType::PolicyRuleServer)
              return formatPolicyRule(QString::fromStdString(e.event_id));
          else if constexpr (t == mtx::events::EventType::RoomHistoryVisibility)
              return formatHistoryVisibilityEvent(e);
          else if constexpr (t == mtx::events::EventType::RoomGuestAccess)
              return formatGuestAccessEvent(e);
          else if constexpr (t == mtx::events::EventType::RoomMember)
              return formatMemberEvent(e);

          return tr("%1 changed unknown state event %2.")
            .arg(displayName(QString::fromStdString(e.sender)))
            .arg(QString::fromStdString(to_string(e.type)));
      },
      event);
}

QVariantMap
TimelineModel::dumpForEvent(const mtx::events::collections::TimelineEvents &event) const
{
    QVariantMap m;
    auto names = roleNames();

    m.insert(names[Type], data(event, static_cast<int>(Type)));
    m.insert(names[TypeString], data(event, static_cast<int>(TypeString)));
    m.insert(names[IsOnlyEmoji], data(event, static_cast<int>(IsOnlyEmoji)));
    m.insert(names[Body], data(event, static_cast<int>(Body)));
    m.insert(names[FormattedBody], data(event, static_cast<int>(FormattedBody)));
    m.insert(names[IsSender], data(event, static_cast<int>(IsSender)));
    m.insert(names[UserId], data(event, static_cast<int>(UserId)));
    m.insert(names[UserName], data(event, static_cast<int>(UserName)));
    m.insert(names[Day], data(event, static_cast<int>(Day)));
    m.insert(names[Timestamp], data(event, static_cast<int>(Timestamp)));
    m.insert(names[Url], data(event, static_cast<int>(Url)));
    m.insert(names[ThumbnailUrl], data(event, static_cast<int>(ThumbnailUrl)));
    m.insert(names[Duration], data(event, static_cast<int>(Duration)));
    m.insert(names[Blurhash], data(event, static_cast<int>(Blurhash)));
    m.insert(names[Filename], data(event, static_cast<int>(Filename)));
    m.insert(names[Filesize], data(event, static_cast<int>(Filesize)));
    m.insert(names[MimeType], data(event, static_cast<int>(MimeType)));
    m.insert(names[OriginalHeight], data(event, static_cast<int>(OriginalHeight)));
    m.insert(names[OriginalWidth], data(event, static_cast<int>(OriginalWidth)));
    m.insert(names[ProportionalHeight], data(event, static_cast<int>(ProportionalHeight)));
    m.insert(names[EventId], data(event, static_cast<int>(EventId)));
    m.insert(names[State], data(event, static_cast<int>(State)));
    m.insert(names[IsEdited], data(event, static_cast<int>(IsEdited)));
    m.insert(names[IsEditable], data(event, static_cast<int>(IsEditable)));
    m.insert(names[IsEncrypted], data(event, static_cast<int>(IsEncrypted)));
    m.insert(names[IsStateEvent], data(event, static_cast<int>(IsStateEvent)));
    m.insert(names[ReplyTo], data(event, static_cast<int>(ReplyTo)));
    m.insert(names[RoomName], data(event, static_cast<int>(RoomName)));
    m.insert(names[RoomTopic], data(event, static_cast<int>(RoomTopic)));
    m.insert(names[CallType], data(event, static_cast<int>(CallType)));
    m.insert(names[EncryptionError], data(event, static_cast<int>(EncryptionError)));

    return m;
}

QVariant
TimelineModel::mediaMetadataForEvent(const mtx::events::collections::TimelineEvents &event,
                                     int role) const
{
    switch (role) {
    case Url:
        return QVariant(QString::fromStdString(mtx::accessors::url(event)));
    case ThumbnailUrl:
        return QVariant(QString::fromStdString(mtx::accessors::thumbnail_url(event)));
    case Duration:
        return QVariant(static_cast<qulonglong>(mtx::accessors::duration(event)));
    case Blurhash:
        return QVariant(QString::fromStdString(mtx::accessors::blurhash(event)));
    case Filename:
        return QVariant(QString::fromStdString(mtx::accessors::filename(event)));
    case Filesize:
        return QVariant(utils::humanReadableFileSize(mtx::accessors::filesize(event)));
    case MimeType:
        return QVariant(QString::fromStdString(mtx::accessors::mimetype(event)));
    case OriginalHeight:
        return QVariant(qulonglong{mtx::accessors::media_height(event)});
    case OriginalWidth:
        return QVariant(qulonglong{mtx::accessors::media_width(event)});
    case ProportionalHeight: {
        auto w = mtx::accessors::media_width(event);
        if (w == 0)
            w = 1;

        double prop = (double)mtx::accessors::media_height(event) / (double)w;

        return {prop > 0 ? prop : 1.};
    }
    default:
        return {};
    }
}

QVariant
TimelineModel::senderRoleDataForEvent(const mtx::events::collections::TimelineEvents &event,
                                      int role,
                                      const std::string &localUserStd) const
{
    switch (role) {
    case IsSender:
        return {mtx::accessors::sender(event) == localUserStd};
    case UserId:
        return QVariant(QString::fromStdString(mtx::accessors::sender(event)));
    case UserName:
        return QVariant(displayName(QString::fromStdString(mtx::accessors::sender(event))));
    case UserPowerlevel:
        return static_cast<qlonglong>(
          permissions_.powerlevelEvent().user_level(mtx::accessors::sender(event)));
    default:
        return {};
    }
}

QVariant
TimelineModel::deliveryStateForEvent(const mtx::events::collections::TimelineEvents &event,
                                     const std::string &localUserStd) const
{
    auto idstr          = mtx::accessors::event_id(event);
    auto id             = QString::fromStdString(idstr);
    auto containsOthers = [&localUserStd](const auto &vec) {
        for (const auto &e : vec)
            if (e.second != localUserStd)
                return true;
        return false;
    };

    // only show read receipts for messages not from us
    if (mtx::accessors::sender(event) != localUserStd)
        return qml_mtx_events::Empty;
    else if (!id.isEmpty() && id[0] == 'm') {
        auto pending = cache::pendingEvents(this->room_id_.toStdString());
        if (std::find(pending.begin(), pending.end(), idstr) != pending.end())
            return qml_mtx_events::Sent;
        else
            return qml_mtx_events::Failed;
    } else if (read.contains(id) || containsOthers(cache::readReceipts(id, room_id_)))
        return qml_mtx_events::Read;
    else
        return qml_mtx_events::Received;
}

QVariant
TimelineModel::notificationLevelForEvent(const mtx::events::collections::TimelineEvents &event,
                                         const std::string &localUserStd) const
{
    const auto &push = ChatPage::instance()->pushruleEvaluator();
    if (push) {
        // skip our messages
        auto sender = mtx::accessors::sender(event);
        if (sender == localUserStd)
            return qml_mtx_events::NotificationLevel::Nothing;

        const auto &id = mtx::accessors::event_id(event);
        std::vector<std::pair<mtx::common::Relation, mtx::events::collections::TimelineEvents>>
          relatedEvents;
        for (const auto &r : mtx::accessors::relations(event).relations) {
            auto related = events.get(r.event_id, id);
            if (related) {
                relatedEvents.emplace_back(r, *related);
            }
        }

        auto actions = push->evaluate({event}, pushrulesRoomContext(), relatedEvents);
        if (std::find(actions.begin(),
                      actions.end(),
                      mtx::pushrules::actions::Action{
                        mtx::pushrules::actions::set_tweak_highlight{}}) != actions.end()) {
            return qml_mtx_events::NotificationLevel::Highlight;
        }
        if (std::find(actions.begin(),
                      actions.end(),
                      mtx::pushrules::actions::Action{mtx::pushrules::actions::notify{}}) !=
            actions.end()) {
            return qml_mtx_events::NotificationLevel::Notify;
        }
    }
    return qml_mtx_events::NotificationLevel::Nothing;
}

QString
TimelineModel::effectiveEventIdForEvent(const mtx::events::collections::TimelineEvents &event) const
{
    if (auto replaces = mtx::accessors::relations(event).replaces())
        return QString::fromStdString(replaces.value());
    return QString::fromStdString(mtx::accessors::event_id(event));
}

QString
TimelineModel::replyToForEvent(const mtx::events::collections::TimelineEvents &event) const
{
    const auto &rels = mtx::accessors::relations(event);
    return QString::fromStdString(rels.reply_to(!rels.thread()).value_or(""));
}

QString
TimelineModel::threadIdForEvent(const mtx::events::collections::TimelineEvents &event) const
{
    return QString::fromStdString(mtx::accessors::relations(event).thread().value_or(""));
}

QVariant
TimelineModel::reactionsForEvent(const mtx::events::collections::TimelineEvents &event) const
{
    auto id = mtx::accessors::relations(event).replaces().value_or(mtx::accessors::event_id(event));
    return QVariant::fromValue(events.reactions(id));
}

bool
TimelineModel::isEncryptedForEvent(const mtx::events::collections::TimelineEvents &event) const
{
    auto encrypted_event = events.get(mtx::accessors::event_id(event), "", false);
    return encrypted_event &&
           std::holds_alternative<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(
             *encrypted_event);
}

crypto::Trust
TimelineModel::trustLevelForEvent(const mtx::events::collections::TimelineEvents &event) const
{
    auto encrypted_event = events.get(mtx::accessors::event_id(event), "", false);
    if (encrypted_event) {
        if (auto encrypted = std::get_if<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(
              &*encrypted_event)) {
            return olm::calculate_trust(
              encrypted->sender, room_id_.toStdString(), encrypted->content);
        }
    }
    return crypto::Trust::Unverified;
}

QVariant
TimelineModel::data(const mtx::events::collections::TimelineEvents &event, int role) const
{
    using namespace mtx::accessors;
    const auto localUser    = utils::localUser();
    const auto localUserStd = localUser.toStdString();

    switch (role) {
    case IsSender:
    case UserId:
    case UserName:
    case UserPowerlevel:
        return senderRoleDataForEvent(event, role, localUserStd);

    case Day: {
        QDateTime prevDate = origin_server_ts(event);
        prevDate.setTime(QTime());
        return QVariant(prevDate.toMSecsSinceEpoch());
    }
    case Timestamp:
        return QVariant(origin_server_ts(event));
    case Type:
        return {qml_mtx_events::toRoomEventType(event)};
    case TypeString:
        return QVariant(qml_mtx_events::toRoomEventTypeString(event));
    case IsOnlyEmoji: {
        QString qBody = QString::fromStdString(body(event));

        QVector<uint> utf32_string = qBody.toUcs4();
        int emojiCount             = 0;

        for (auto &code : utf32_string) {
            if (utils::codepointIsEmoji(code)) {
                emojiCount++;
            } else {
                return {0};
            }
        }

        return {emojiCount};
    }
    case Body:
        return QVariant(utils::replaceEmoji(QString::fromStdString(body(event)).toHtmlEscaped()));
    case HasFormattedBody:
        return QVariant(!formatted_body(event).empty());
    case FormattedBody:
        return QVariant(formattedBodyForEvent(event));
    case FormattedStateEvent:
        return formattedStateEventForEvent(event);
    case Url:
    case ThumbnailUrl:
    case Duration:
    case Blurhash:
    case Filename:
    case Filesize:
    case MimeType:
    case OriginalHeight:
    case OriginalWidth:
    case ProportionalHeight:
        return mediaMetadataForEvent(event, role);
    case EventId:
        return QVariant(effectiveEventIdForEvent(event));
    case State:
        return deliveryStateForEvent(event, localUserStd);
    case IsEdited:
        return {relations(event).replaces().has_value()};
    case IsEditable:
        return {!is_state_event(event) && mtx::accessors::sender(event) == localUserStd};
    case IsEncrypted:
        return isEncryptedForEvent(event);
    case IsStateEvent: {
        return is_state_event(event);
    }

    case Trustlevel:
        return trustLevelForEvent(event);

    case Notificationlevel:
        return notificationLevelForEvent(event, localUserStd);

    case EncryptionError:
        return events.decryptionError(event_id(event));

    case ReplyTo:
        return QVariant(replyToForEvent(event));
    case ThreadId:
        return QVariant(threadIdForEvent(event));
    case Reactions:
        return reactionsForEvent(event);
    case Room:
        return QVariant::fromValue(this);
    case RoomId:
        return QVariant(room_id_);
    case RoomName:
        return QVariant(
          utils::replaceEmoji(QString::fromStdString(room_name(event)).toHtmlEscaped()));
    case RoomTopic:
        return QVariant(utils::replaceEmoji(
          utils::linkifyMessage(QString::fromStdString(room_topic(event))
                                  .toHtmlEscaped()
                                  .replace(QLatin1String("\n"), QLatin1String("<br>")))));
    case CallType:
        return QVariant(QString::fromStdString(call_type(event)));
    case Dump:
        return QVariant(dumpForEvent(event));
    case RelatedEventCacheBuster:
        return relatedEventCacheBuster;
    default:
        return {};
    }
}

QVariant
TimelineModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 && index.row() >= rowCount())
        return {};

    auto event = events.get(rowCount() - index.row() - 1);

    if (!event)
        return "";

    return data(*event, role);
}

void
TimelineModel::multiData(const QModelIndex &index, QModelRoleDataSpan roleDataSpan) const
{
    if (index.row() < 0 && index.row() >= rowCount()) {
        for (QModelRoleData &roleData : roleDataSpan)
            roleData.clearData();
        return;
    }

    // nhlog::db()->debug("MultiData called for {}", index.row());

    auto event = events.get(rowCount() - index.row() - 1);

    if (!event) {
        for (QModelRoleData &roleData : roleDataSpan)
            roleData.clearData();
        return;
    }

    for (QModelRoleData &roleData : roleDataSpan) {
        roleData.setData(data(*event, roleData.role()));
    }
}

void
TimelineModel::multiData(const QString &id,
                         const QString &relatedTo,
                         QModelRoleDataSpan roleDataSpan) const
{
    if (id.isEmpty()) {
        for (QModelRoleData &roleData : roleDataSpan)
            roleData.clearData();
        return;
    }

    // nhlog::db()->debug("MultiData called for {}", id.toStdString());

    auto event = events.get(id.toStdString(), relatedTo.toStdString());

    if (!event) {
        for (QModelRoleData &roleData : roleDataSpan)
            roleData.clearData();
        return;
    }

    for (QModelRoleData &roleData : roleDataSpan) {
        int role = roleData.role();

        roleData.setData(data(*event, role));
    }
}

QVariant
TimelineModel::dataById(const QString &id, int role, const QString &relatedTo)
{
    if (auto event = events.get(id.toStdString(), relatedTo.toStdString()))
        return data(*event, role);
    return {};
}

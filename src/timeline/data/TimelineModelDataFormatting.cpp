// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QScreen>
#include <QUrl>
#include <QVariant>

#include "FormattedCodeBlockHighlighter.h"
#include "cache/api/CacheApiRooms.h"
#include "cache/api/CacheApiUsers.h"
#include "events/EventAccessors.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/formattedmessage/HtmlProcessor.h"
#include "timeline/litehtml/LitehtmlStylesheet.h"
#include "ui/KomaiGlobalObject.h"
#include "ui/Theme.h"
#include "utils/Utils.h"

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

    formattedBody_ = utils::linkifyMessage(formattedBody_);

    // Decorate matrix.to links as styled pills with avatars.
    // Use the same standard avatar thumbnail size as Avatar.qml so that pill
    // avatars share the same cache entry and avoid extra server thumbnail hits.
    const auto roomId = room_id_;
    // Pill avatar URL carries the logical listIconSize.  The image loader
    // (MxcImageProvider / LitehtmlContainer) applies QScreen DPR to get the
    // physical thumbnail size.
    const int pillThumbSourcePx = Komai::listIconLogicalSize();
    // Lazy alias→roomId map, built on first #alias mention.
    QHash<QString, std::string> aliasToRoomId;
    bool aliasMapBuilt = false;

    formattedBody_ = timeline::formattedmessage::decorateMatrixPills(
      formattedBody_,
      [&roomId, pillThumbSourcePx, &aliasToRoomId, &aliasMapBuilt](
        const QString &matrixId) -> QString {
          QString mxcUrl;
          if (matrixId.startsWith(QLatin1Char('@'))) {
              mxcUrl = cache::avatarUrl(roomId, matrixId);
          } else if (matrixId.startsWith(QLatin1Char('!'))) {
              mxcUrl = cache::roomAvatarUrl(matrixId.toStdString());
          } else if (matrixId.startsWith(QLatin1Char('#'))) {
              // Resolve room alias to room ID via the local room list.
              if (!aliasMapBuilt) {
                  for (const auto &r : cache::roomNamesAndAliases()) {
                      if (!r.alias.empty())
                          aliasToRoomId.insert(QString::fromStdString(r.alias), r.id);
                  }
                  aliasMapBuilt = true;
              }
              auto it = aliasToRoomId.constFind(matrixId);
              if (it != aliasToRoomId.cend())
                  mxcUrl = cache::roomAvatarUrl(it.value());
          }
          if (mxcUrl.isEmpty() || !mxcUrl.startsWith(QLatin1String("mxc://")))
              return {};

          // Convert mxc:// to image://mxcImage/ with thumbnail sizing params.
          auto src = mxcUrl;
          src.replace(QLatin1String("mxc://"), QLatin1String("image://mxcImage/"));
          src.append(QStringLiteral("?avatarSize=%1&radius=25").arg(pillThumbSourcePx));
          return src;
      });

    return utils::replaceEmoji(formattedBody_);
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
                  // Size the inline preview to the state event text line height.
                  // State events render at 0.95x the UI font (NoticeMessage.qml).
                  QFont stateFont = QGuiApplication::font();
                  if (auto s = UserSettings::instance())
                      stateFont.setPointSizeF(s->uiFontSizePt() * 0.95);
                  const int inlinePreviewLogicalPx = qMax(12, QFontMetrics(stateFont).height());
                  // Download at DPR-scaled size for crisp HiDPI rendering.
                  double dpr = 1.0;
                  for (const auto *screen : QGuiApplication::screens())
                      dpr = qMax(dpr, screen->devicePixelRatio());
                  const int avatarThumbPx = qMax(1, qRound(inlinePreviewLogicalPx * dpr));
                  // Match avatar rounding used in Avatar.qml + MxcImageProvider.
                  const int avatarCornerRadiusPercent =
                    UserSettings::instance()->uiAvatarsCircular() ? 100 : 25;

                  auto avatarMxcUrl = QString::fromStdString(e.content.url);
                  avatarMxcUrl.append("#room-avatar");
                  auto avatarPreviewUrl = QString::fromStdString(e.content.url);
                  avatarPreviewUrl.replace("mxc://", "image://mxcImage/");
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

QString
TimelineModel::stateEventIconSourceForEvent(
  const mtx::events::collections::TimelineEvents &event) const
{
    if (!mtx::accessors::is_state_event(event))
        return {};

    auto icon = [](const char *ref) { return QStringLiteral(":/%1").arg(QLatin1String(ref)); };

    return std::visit(
      [this, &icon](const auto &e) -> QString {
          constexpr auto t = mtx::events::state_content_to_type<decltype(e.content)>;

          if constexpr (t == mtx::events::EventType::RoomName)
              return icon("icons/icons/ui/state-room-name.svg");
          else if constexpr (t == mtx::events::EventType::RoomTopic)
              return icon("icons/icons/ui/state-room-topic.svg");
          else if constexpr (t == mtx::events::EventType::RoomAvatar)
              return icon("icons/icons/ui/state-room-avatar.svg");
          else if constexpr (t == mtx::events::EventType::RoomPinnedEvents)
              return icon(e.content.pinned.empty() ? "icons/icons/ui/state-room-pinned-off.svg"
                                                   : "icons/icons/ui/state-room-pinned.svg");
          else if constexpr (t == mtx::events::EventType::RoomPowerLevels)
              return icon("icons/icons/ui/state-room-permissions.svg");
          else if constexpr (t == mtx::events::EventType::RoomMember) {
              using namespace mtx::events::state;

              const mtx::events::StateEvent<Member> *prevEvent = nullptr;
              if (!e.unsigned_data.replaces_state.empty()) {
                  auto tempPrevEvent = events.get(e.unsigned_data.replaces_state, e.event_id);
                  if (tempPrevEvent) {
                      prevEvent = std::get_if<mtx::events::StateEvent<Member>>(tempPrevEvent);
                  }
              }

              switch (e.content.membership) {
              case Membership::Invite:
                  return icon("icons/icons/ui/state-member-invite.svg");
              case Membership::Join:
                  if (prevEvent && prevEvent->content.membership == Membership::Join) {
                      const bool displayNameChanged =
                        prevEvent->content.display_name != e.content.display_name;
                      const bool avatarChanged =
                        prevEvent->content.avatar_url != e.content.avatar_url;

                      if (avatarChanged)
                          return icon("icons/icons/ui/state-member-avatar.svg");
                      if (displayNameChanged)
                          return icon("icons/icons/ui/state-member-display-name.svg");
                  }
                  return icon("icons/icons/ui/state-member-join.svg");
              case Membership::Leave:
                  if (!prevEvent || prevEvent->content.membership == Membership::Join) {
                      return icon(e.state_key == e.sender ? "icons/icons/ui/state-member-leave.svg"
                                                          : "icons/icons/ui/state-member-kick.svg");
                  } else if (prevEvent->content.membership == Membership::Invite) {
                      return icon(e.state_key == e.sender ? "icons/icons/ui/state-member-leave.svg"
                                                          : "icons/icons/ui/state-member-kick.svg");
                  } else if (prevEvent->content.membership == Membership::Ban) {
                      return icon("icons/icons/ui/state-member-ban.svg");
                  } else if (prevEvent->content.membership == Membership::Knock) {
                      return icon(e.state_key == e.sender ? "icons/icons/ui/state-member-knock.svg"
                                                          : "icons/icons/ui/state-member-kick.svg");
                  }
                  return icon("icons/icons/ui/state-event.svg");
              case Membership::Ban:
                  return icon("icons/icons/ui/state-member-ban.svg");
              case Membership::Knock:
                  return icon("icons/icons/ui/state-member-knock.svg");
              }

              return icon("icons/icons/ui/state-event.svg");
          }

          return icon("icons/icons/ui/state-event.svg");
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
    m.insert(names[StateEventIconSource], data(event, static_cast<int>(StateEventIconSource)));
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
    m.insert(names[FileTypeIconSource], data(event, static_cast<int>(FileTypeIconSource)));
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

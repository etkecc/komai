// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include <algorithm>
#include <thread>
#include <type_traits>
#include <utility>

#include <QClipboard>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QScreen>
#include <QVariant>

#include "ChatPage.h"
#include "Config.h"
#include "EventAccessors.h"
#include "FormattedCodeBlockHighlighter.h"
#include "Logging.h"
#include "MainWindow.h"
#include "MatrixClient.h"
#include "ReadReceiptsModel.h"
#include "RoomlistModel.h"
#include "TimelineViewManager.h"
#include "Utils.h"
#include "cache/Cache.h"
#include "encryption/Olm.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/format/TimelineImagePackFormatter.h"
#include "timeline/format/TimelineMemberEventFormatter.h"
#include "timeline/format/TimelinePolicyRuleFormatter.h"
#include "timeline/format/TimelinePowerLevelFormatter.h"
#include "timeline/format/TimelineRedactedEventFormatter.h"
#include "timeline/rawmessage/RawMessageDialogPayload.h"
#include "timeline/send/TimelineMessageSendPipeline.h"
#include "ui/Theme.h"
#include "ui/UserProfile.h"

namespace std {
inline uint // clazy:exclude=qhash-namespace
qHash(const std::string &key, uint seed = 0)
{
    return qHash(QByteArray::fromRawData(key.data(), (int)key.length()), seed);
}
}

TimelineModel::TimelineModel(TimelineViewManager *manager, QString room_id, QObject *parent)
  : QAbstractListModel(parent)
  , room_id_(std::move(room_id))
  , events(room_id_.toStdString(), this)
  , mediaController_(room_id_,
                     events,
                     [this](const QString &mxcUrl, const QString &cacheUrl) {
                         emit mediaCached(mxcUrl, cacheUrl);
                     })
  , manager_(manager)
  , permissions_{room_id_}
{
    this->isEncrypted_ = cache::isRoomEncrypted(room_id_.toStdString());

    auto roomInfo            = cache::singleRoomInfo(room_id_.toStdString());
    this->isSpace_           = roomInfo.is_space;
    this->notification_count = roomInfo.notification_count;
    this->highlight_count    = roomInfo.highlight_count;
    lastMessage_.timestamp   = roomInfo.approximate_last_modification_ts;

    connect(
      this,
      &TimelineModel::redactionFailed,
      this,
      [](const QString &msg) { emit ChatPage::instance()->showNotification(msg); },
      Qt::QueuedConnection);

    connect(this, &TimelineModel::dataAtIdChanged, this, [this](const QString &id) {
        relatedEventCacheBuster++;

        auto idx = idToIndex(id);
        if (idx != -1) {
            auto pos = index(idx);
            nhlog::ui()->debug("data changed at {}", id.toStdString());
            emit dataChanged(pos, pos);
        } else {
            nhlog::ui()->debug("id not found {}", id.toStdString());
        }
    });

    connect(this,
            &TimelineModel::newMessageToSend,
            this,
            &TimelineModel::addPendingMessage,
            Qt::QueuedConnection);
    connect(this, &TimelineModel::addPendingMessageToStore, &events, &EventStore::addPending);

    connect(&events, &EventStore::dataChanged, this, [this](int from, int to) {
        relatedEventCacheBuster++;
        nhlog::ui()->debug(
          "data changed {} to {}", events.size() - to - 1, events.size() - from - 1);
        emit dataChanged(index(events.size() - to - 1, 0), index(events.size() - from - 1, 0));

        if (!decryptDescription)
            return;

        if (lastMessage_.event_id.isEmpty()) {
            updateLastMessage();
            return;
        }

        const auto lastMessageIdx = events.idToIndex(lastMessage_.event_id.toStdString());
        if (!lastMessageIdx) {
            updateLastMessage();
            return;
        }

        const auto changedFrom = std::min(from, to);
        const auto changedTo   = std::max(from, to);
        if (*lastMessageIdx >= changedFrom && *lastMessageIdx <= changedTo)
            updateLastMessage();
    });
    connect(&events, &EventStore::pinsChanged, this, &TimelineModel::pinnedMessagesChanged);

    connect(&events, &EventStore::beginInsertRows, this, [this](int from, int to) {
        int first = events.size() - to;
        int last  = events.size() - from;
        if (from >= events.size()) {
            int batch_size = to - from;
            first += batch_size;
            last += batch_size;
        } else {
            first -= 1;
            last -= 1;
        }
        nhlog::ui()->debug("begin insert from {} to {}", first, last);
        beginInsertRows(QModelIndex(), first, last);
    });
    connect(&events, &EventStore::endInsertRows, this, [this]() { endInsertRows(); });
    connect(&events, &EventStore::beginResetModel, this, [this]() { beginResetModel(); });
    connect(&events, &EventStore::endResetModel, this, [this]() { endResetModel(); });
    connect(&events, &EventStore::newEncryptedImage, this, &TimelineModel::newEncryptedImage);
    connect(&events, &EventStore::fetchedMore, this, [this]() {
        setPaginationInProgress(false);
        updateLastMessage();
        emit fetchedMore();
    });
    connect(&events, &EventStore::fetchedMore, this, &TimelineModel::checkAfterFetch);
    connect(&events,
            &EventStore::startDMVerification,
            this,
            [this](const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationRequest> &msg) {
                ChatPage::instance()->receivedRoomDeviceVerificationRequest(msg, this);
            });
    connect(&events, &EventStore::updateFlowEventId, this, [this](std::string event_id) {
        this->updateFlowEventId(std::move(event_id));
    });

    // When a message is sent, check if the current edit/reply relates to that message,
    // and update the event_id so that it points to the sent message and not the pending one.
    connect(
      &events,
      &EventStore::messageSent,
      this,
      [this](const std::string &txn_id, const std::string &event_id) {
          if (edit_.toStdString() == txn_id) {
              edit_ = QString::fromStdString(event_id);
              emit editChanged(edit_);
          }
          if (reply_.toStdString() == txn_id) {
              reply_ = QString::fromStdString(event_id);
              emit replyChanged(reply_);
          }
      },
      Qt::QueuedConnection);

    connect(manager_,
            &TimelineViewManager::waitingForFirstSyncChanged,
            &events,
            &EventStore::enableKeyRequests);

    connect(this, &TimelineModel::encryptionChanged, this, &TimelineModel::trustlevelChanged);
    connect(this, &TimelineModel::roomMemberCountChanged, this, &TimelineModel::trustlevelChanged);
    cache::onVerificationStatusChanged(this,
                                       [this](const std::string &) { emit trustlevelChanged(); });

    showEventTimer.callOnTimeout(this, &TimelineModel::scrollTimerEvent);

    connect(this, &TimelineModel::newState, this, [this](mtx::responses::StateEvents events_) {
        cache::updateState(room_id_.toStdString(), events_, true);
        this->syncState({std::move(events_.events)});
    });
}

QHash<int, QByteArray>
TimelineModel::roleNames() const
{
    static QHash<int, QByteArray> roles{
      {Type, "type"},
      {TypeString, "typeString"},
      {IsOnlyEmoji, "isOnlyEmoji"},
      {Body, "body"},
      {FormattedBody, "formattedBody"},
      {HasFormattedBody, "hasFormattedBody"},
      {FormattedStateEvent, "formattedStateEvent"},
      {IsSender, "isSender"},
      {UserId, "userId"},
      {UserName, "userName"},
      {UserPowerlevel, "userPowerlevel"},
      {Day, "day"},
      {Timestamp, "timestamp"},
      {Url, "url"},
      {ThumbnailUrl, "thumbnailUrl"},
      {Duration, "duration"},
      {Blurhash, "blurhash"},
      {Filename, "filename"},
      {Filesize, "filesize"},
      {MimeType, "mimetype"},
      {OriginalHeight, "originalHeight"},
      {OriginalWidth, "originalWidth"},
      {ProportionalHeight, "proportionalHeight"},
      {EventId, "eventId"},
      {State, "status"},
      {IsEdited, "isEdited"},
      {IsEditable, "isEditable"},
      {IsEncrypted, "isEncrypted"},
      {IsStateEvent, "isStateEvent"},
      {Trustlevel, "trustlevel"},
      {Notificationlevel, "notificationlevel"},
      {EncryptionError, "encryptionError"},
      {ReplyTo, "replyTo"},
      {ThreadId, "threadId"},
      {Reactions, "reactions"},
      {Room, "room"},
      {RoomId, "roomId"},
      {RoomName, "roomName"},
      {RoomTopic, "roomTopic"},
      {CallType, "callType"},
      {Dump, "dump"},
      {RelatedEventCacheBuster, "relatedEventCacheBuster"},
    };

    return roles;
}
int
TimelineModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return this->events.size();
}

QVariantMap
TimelineModel::getDump(const QString &eventId, const QString &relatedTo) const
{
    if (auto event = events.get(eventId.toStdString(), relatedTo.toStdString()))
        return data(*event, Dump).toMap();
    return {};
}

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
    namespace acc           = mtx::accessors;
    const auto localUser    = utils::localUser();
    const auto localUserStd = localUser.toStdString();

    switch (role) {
    case IsSender:
        return {acc::sender(event) == localUserStd};
    case UserId:
        return QVariant(QString::fromStdString(acc::sender(event)));
    case UserName:
        return QVariant(displayName(QString::fromStdString(acc::sender(event))));
    case UserPowerlevel: {
        return static_cast<qlonglong>(
          permissions_.powerlevelEvent().user_level(acc::sender(event)));
    }

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
        return QVariant(QString::fromStdString(url(event)));
    case ThumbnailUrl:
        return QVariant(QString::fromStdString(thumbnail_url(event)));
    case Duration:
        return QVariant(static_cast<qulonglong>(duration(event)));
    case Blurhash:
        return QVariant(QString::fromStdString(blurhash(event)));
    case Filename:
        return QVariant(QString::fromStdString(filename(event)));
    case Filesize:
        return QVariant(utils::humanReadableFileSize(filesize(event)));
    case MimeType:
        return QVariant(QString::fromStdString(mimetype(event)));
    case OriginalHeight:
        return QVariant(qulonglong{media_height(event)});
    case OriginalWidth:
        return QVariant(qulonglong{media_width(event)});
    case ProportionalHeight: {
        auto w = media_width(event);
        if (w == 0)
            w = 1;

        double prop = (double)media_height(event) / (double)w;

        return {prop > 0 ? prop : 1.};
    }
    case EventId: {
        if (auto replaces = relations(event).replaces())
            return QVariant(QString::fromStdString(replaces.value()));
        else
            return QVariant(QString::fromStdString(event_id(event)));
    }
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

    case ReplyTo: {
        const auto &rels = relations(event);
        return QVariant(QString::fromStdString(rels.reply_to(!rels.thread()).value_or("")));
    }
    case ThreadId:
        return QVariant(QString::fromStdString(relations(event).thread().value_or("")));
    case Reactions: {
        auto id = relations(event).replaces().value_or(event_id(event));
        return QVariant::fromValue(events.reactions(id));
    }
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
    using namespace mtx::accessors;
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

bool
TimelineModel::canFetchMore(const QModelIndex &) const
{
    if (!events.size())
        return true;
    // When the virtual window can still expand from cached DB entries,
    // return false to prevent Qt from auto-triggering fetchMore on model
    // assignment. The data() hack handles expansion on user scroll instead.
    if (events.canExpandWindow())
        return false;
    if (auto first = events.get(0);
        first &&
        !std::holds_alternative<mtx::events::StateEvent<mtx::events::state::Create>>(*first))
        return true;
    else

        return false;
}

void
TimelineModel::setPaginationInProgress(const bool paginationInProgress)
{
    if (m_paginationInProgress == paginationInProgress) {
        return;
    }

    m_paginationInProgress = paginationInProgress;
    emit paginationInProgressChanged(m_paginationInProgress);

    if (m_paginationInProgress) {
        // Expand cached history in chunks. Do not loop to exhaustion here:
        // with small initial windows this can eagerly inflate to the full room
        // during first paint and erase the performance benefit.
        if (events.canExpandWindow()) {
            events.expandWindow();
            setPaginationInProgress(false);
            emit fetchedMore();
            return;
        }
        events.fetchMore();
    }
}

bool
TimelineModel::canExpandWindow() const
{
    return events.canExpandWindow();
}

bool
TimelineModel::canPaginateBack() const
{
    return events.canExpandWindow() || canFetchMore(QModelIndex{});
}

void
TimelineModel::fetchMore(const QModelIndex &)
{
    if (m_paginationInProgress) {
        nhlog::ui()->warn("Already loading older messages");
        return;
    }

    setPaginationInProgress(true);
}

void
TimelineModel::sync(const mtx::responses::JoinedRoom &room)
{
    this->syncState(room.state);
    this->addEvents(room.timeline);

    if (room.unread_notifications.highlight_count != highlight_count ||
        room.unread_notifications.notification_count != notification_count) {
        notification_count = room.unread_notifications.notification_count;
        highlight_count    = room.unread_notifications.highlight_count;
        emit notificationsChanged();
    }
}

void
TimelineModel::syncState(const mtx::responses::State &s)
{
    bool avatarChanged      = false;
    bool nameChanged        = false;
    bool memberCountChanged = false;

    for (const auto &e : s.events) {
        applyStateEventSideEffects(e, avatarChanged, nameChanged, memberCountChanged);
    }

    emitRoomMetadataChanges(avatarChanged, nameChanged, memberCountChanged);
}

bool
TimelineModel::applyStateEventSideEffects(const mtx::events::collections::TimelineEvents &event,
                                          bool &avatarChanged,
                                          bool &nameChanged,
                                          bool &memberCountChanged)
{
    using namespace mtx::events;

    if (std::holds_alternative<StateEvent<state::Avatar>>(event)) {
        avatarChanged = true;
        return true;
    } else if (std::holds_alternative<StateEvent<state::Name>>(event)) {
        nameChanged = true;
        return true;
    } else if (std::holds_alternative<StateEvent<state::Topic>>(event)) {
        emit roomTopicChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::PinnedEvents>>(event)) {
        emit pinnedMessagesChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::Widget>>(event)) {
        emit widgetLinksChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::PowerLevels>>(event)) {
        permissions_.invalidate();
        emit permissionsChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::Member>>(event)) {
        avatarChanged      = true;
        nameChanged        = true;
        memberCountChanged = true;
        return true;
    } else if (std::holds_alternative<StateEvent<state::Encryption>>(event)) {
        this->isEncrypted_ = cache::isRoomEncrypted(room_id_.toStdString());
        emit encryptionChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::space::Parent>>(event)) {
        this->parentChecked = false;
        emit parentSpaceChanged();
        return true;
    }

    return false;
}

bool
TimelineModel::applyStateEventSideEffects(const mtx::events::collections::StateEvents &event,
                                          bool &avatarChanged,
                                          bool &nameChanged,
                                          bool &memberCountChanged)
{
    using namespace mtx::events;

    if (std::holds_alternative<StateEvent<state::Avatar>>(event)) {
        avatarChanged = true;
        return true;
    } else if (std::holds_alternative<StateEvent<state::Name>>(event)) {
        nameChanged = true;
        return true;
    } else if (std::holds_alternative<StateEvent<state::Topic>>(event)) {
        emit roomTopicChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::PinnedEvents>>(event)) {
        emit pinnedMessagesChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::Widget>>(event)) {
        emit widgetLinksChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::PowerLevels>>(event)) {
        permissions_.invalidate();
        emit permissionsChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::Member>>(event)) {
        avatarChanged      = true;
        nameChanged        = true;
        memberCountChanged = true;
        return true;
    } else if (std::holds_alternative<StateEvent<state::Encryption>>(event)) {
        this->isEncrypted_ = cache::isRoomEncrypted(room_id_.toStdString());
        emit encryptionChanged();
        return true;
    } else if (std::holds_alternative<StateEvent<state::space::Parent>>(event)) {
        this->parentChecked = false;
        emit parentSpaceChanged();
        return true;
    }

    return false;
}

void
TimelineModel::emitRoomMetadataChanges(bool avatarChanged,
                                       bool nameChanged,
                                       bool memberCountChanged)
{
    if (avatarChanged)
        emit roomAvatarUrlChanged();
    if (nameChanged)
        emit roomNameChanged();

    if (memberCountChanged) {
        emit roomMemberCountChanged();
        if (roomMemberCount() <= 2) {
            emit isDirectChanged();
            emit directChatOtherUserIdChanged();
        }
    }
}

bool
TimelineModel::dispatchCallEventIfNeeded(mtx::events::collections::TimelineEvents &event,
                                         const std::string &localUserStd)
{
    using namespace mtx::events;
    if (!(std::holds_alternative<RoomEvent<voip::CallCandidates>>(event) ||
          std::holds_alternative<RoomEvent<voip::CallNegotiate>>(event) ||
          std::holds_alternative<RoomEvent<voip::CallInvite>>(event) ||
          std::holds_alternative<RoomEvent<voip::CallAnswer>>(event) ||
          std::holds_alternative<RoomEvent<voip::CallSelectAnswer>>(event) ||
          std::holds_alternative<RoomEvent<voip::CallReject>>(event) ||
          std::holds_alternative<RoomEvent<voip::CallHangUp>>(event)))
        return false;

    std::visit(
      [this, &localUserStd](auto &callEvent) {
          callEvent.room_id = room_id_.toStdString();
          if constexpr (
            std::is_same_v<std::decay_t<decltype(callEvent)>, RoomEvent<voip::CallAnswer>> ||
            std::is_same_v<std::decay_t<decltype(callEvent)>, RoomEvent<voip::CallInvite>> ||
            std::is_same_v<std::decay_t<decltype(callEvent)>, RoomEvent<voip::CallSelectAnswer>> ||
            std::is_same_v<std::decay_t<decltype(callEvent)>, RoomEvent<voip::CallReject>> ||
            std::is_same_v<std::decay_t<decltype(callEvent)>, RoomEvent<voip::CallHangUp>>)
              emit newCallEvent(callEvent);
          else if (callEvent.sender != localUserStd)
              emit newCallEvent(callEvent);
      },
      event);

    return true;
}

void
TimelineModel::processSpecialEffectEvent(const mtx::events::collections::TimelineEvents &event)
{
    using namespace mtx::events;
    if (auto text = std::get_if<RoomEvent<msg::Text>>(&event)) {
        if (const auto msg = QString::fromStdString(text->content.body);
            msg.contains("🎉") || msg.contains("🎊")) {
            needsSpecialEffects_ = true;
            specialEffects_.setFlag(Confetti);
        }
    } else if (auto unknown = std::get_if<RoomEvent<msg::Unknown>>(&event)) {
        if (const auto msg = QString::fromStdString(unknown->content.body);
            msg.contains("🎉") || msg.contains("🎊")) {
            needsSpecialEffects_ = true;
            specialEffects_.setFlag(Confetti);
        }
    } else if (auto effect = std::get_if<RoomEvent<msg::ElementEffect>>(&event)) {
        if (effect->content.msgtype == "nic.custom.confetti") {
            needsSpecialEffects_ = true;
            specialEffects_.setFlag(Confetti);
        } else if (effect->content.msgtype == "io.element.effect.rainfall") {
            needsSpecialEffects_ = true;
            specialEffects_.setFlag(Rainfall);
        }
    }
}

void
TimelineModel::addEvents(const mtx::responses::Timeline &timeline)
{
    if (timeline.limited)
        setPaginationInProgress(false);

    if (timeline.events.empty())
        return;

    events.handleSync(timeline);

    using namespace mtx::events;

    bool avatarChanged      = false;
    bool nameChanged        = false;
    bool memberCountChanged = false;
    const auto localUserStd = utils::localUser().toStdString();

    for (auto e : timeline.events) {
        if (auto encryptedEvent = std::get_if<EncryptedEvent<msg::Encrypted>>(&e)) {
            MegolmSessionIndex index(room_id_.toStdString(), encryptedEvent->content);

            auto result = olm::decryptEvent(index, *encryptedEvent);
            if (result.event)
                e = result.event.value();
        }

        if (dispatchCallEventIfNeeded(e, localUserStd))
            continue;

        if (applyStateEventSideEffects(e, avatarChanged, nameChanged, memberCountChanged))
            continue;

        processSpecialEffectEvent(e);
    }

    if (needsSpecialEffects_)
        triggerSpecialEffects();

    emitRoomMetadataChanges(avatarChanged, nameChanged, memberCountChanged);

    updateLastMessage();
}

// Workaround. We also want to see a room at the top, if we just joined it
auto
isYourJoin(const mtx::events::StateEvent<mtx::events::state::Member> &e, EventStore &events)
{
    if (e.content.membership == mtx::events::state::Membership::Join &&
        e.state_key == utils::localUser().toStdString() &&
        !e.unsigned_data.replaces_state.empty()) {
        auto tempPrevEvent = events.get(e.unsigned_data.replaces_state, e.event_id);
        if (tempPrevEvent) {
            if (auto prevEvent =
                  std::get_if<mtx::events::StateEvent<mtx::events::state::Member>>(tempPrevEvent)) {
                if (prevEvent->content.membership != mtx::events::state::Membership::Join)
                    return true;
            }
        }
    }
    return false;
}
template<typename T>
auto
isYourJoin(const mtx::events::Event<T> &, EventStore &)
{
    return false;
}

DescInfo
TimelineModel::lastMessage() const
{
    if (lastMessage_.event_id.isEmpty())
        QTimer::singleShot(0, this, &TimelineModel::updateLastMessage);

    return lastMessage_;
}

void
TimelineModel::updateLastMessage()
{
    // only try to generate a preview for the last 1000 messages
    auto end = std::max(events.size() - 1001, 0);
    for (auto it = events.size() - 1; it >= end; --it) {
        auto event = events.get(it, decryptDescription);
        if (!event)
            continue;

        if (std::visit([this](const auto &e) -> bool { return isYourJoin(e, events); }, *event)) {
            auto time        = mtx::accessors::origin_server_ts(*event);
            uint64_t ts      = time.toMSecsSinceEpoch();
            auto description = DescInfo{QString::fromStdString(mtx::accessors::event_id(*event)),
                                        utils::localUser(),
                                        tr("You joined this room."),
                                        utils::descriptiveTime(time),
                                        ts,
                                        time};
            if (description != lastMessage_) {
                if (lastMessage_.timestamp == 0) {
                    cache::updateLastMessageTimestamp(room_id_.toStdString(),
                                                      description.timestamp);
                }
                lastMessage_ = description;
                emit lastMessageChanged();
            }
            return;
        }
        if (!mtx::accessors::is_message(*event))
            continue;

        auto description = utils::getMessageDescription(
          *event,
          utils::localUser(),
          cache::displayName(room_id_, QString::fromStdString(mtx::accessors::sender(*event))));
        if (description != lastMessage_) {
            if (lastMessage_.timestamp == 0) {
                cache::updateLastMessageTimestamp(room_id_.toStdString(), description.timestamp);
            }
            lastMessage_ = description;
            emit lastMessageChanged();
        }
        return;
    }
}

void
TimelineModel::setCurrentIndex(int index)
{
    setCurrentIndex(index, false);
}

void
TimelineModel::setCurrentIndex(int index, bool ignoreInactiveState)
{
    auto oldIndex = idToIndex(currentId);
    currentId     = indexToId(index);
    if (index != oldIndex)
        emit currentIndexChanged(index);

    if (!ignoreInactiveState &&
        (!QGuiApplication::focusWindow() || !QGuiApplication::focusWindow()->isActive() ||
         MainWindow::instance()->windowForRoom(roomId()) != QGuiApplication::focusWindow()))
        return;

    if (!currentId.startsWith('m')) {
        auto oldReadIndex =
          cache::getEventIndex(roomId().toStdString(), currentReadId.toStdString());
        auto nextEventIndexAndId =
          cache::lastInvisibleEventAfter(roomId().toStdString(), currentId.toStdString());

        if (nextEventIndexAndId && (!oldReadIndex || *oldReadIndex < nextEventIndexAndId->first)) {
            readEvent(nextEventIndexAndId->second);
            currentReadId = QString::fromStdString(nextEventIndexAndId->second);
        }
    }
}

void
TimelineModel::readEvent(const std::string &id)
{
    http::client()->read_event(
      room_id_.toStdString(),
      id,
      [this, newId = id, oldId = currentReadId](mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->warn("failed to read_event ({}, {})", room_id_.toStdString(), newId);

              ChatPage::instance()->callFunctionOnGuiThread([this, newId, oldId] {
                  if (currentReadId.toStdString() == newId)
                      this->currentReadId = oldId;
              });
          }
      },
      !UserSettings::instance()->timelineReadReceiptsEnabled());
}

QString
TimelineModel::displayName(const QString &id) const
{
    return cache::displayName(room_id_, id).toHtmlEscaped();
}

QString
TimelineModel::avatarUrl(const QString &id) const
{
    return cache::avatarUrl(room_id_, id);
}

QString
TimelineModel::formatDateSeparator(QDate date) const
{
    auto now = QDateTime::currentDateTime();

    QString fmt = QLocale::system().dateFormat(QLocale::LongFormat);

    if (now.date().year() == date.year()) {
        static QRegularExpression rx(QStringLiteral("[^a-zA-Z]*y+[^a-zA-Z]*"));
        fmt = fmt.remove(rx);
    }

    return date.toString(fmt);
}

QString
TimelineModel::formatLaterSeparator(QDateTime prevDate, QDateTime date) const
{
    auto deltaHours = prevDate.secsTo(date) / 60 / 60;
    return tr("%n hour(s) later", "", deltaHours);
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

int
TimelineModel::idToIndex(const QString &id) const
{
    if (id.isEmpty())
        return -1;

    auto idx = events.idToIndex(id.toStdString());
    if (idx)
        return events.size() - *idx - 1;
    else
        return -1;
}

QString
TimelineModel::indexToId(int index) const
{
    auto id = events.indexToId(events.size() - index - 1);
    return id ? QString::fromStdString(*id) : QLatin1String("");
}

// Note: this will only be called for our messages
void
TimelineModel::markEventsAsRead(const std::vector<QString> &event_ids)
{
    for (const auto &id : event_ids) {
        read.insert(id);
        int idx = idToIndex(id);
        if (idx < 0) {
            return;
        }
        emit dataChanged(index(idx, 0), index(idx, 0));
    }
}

void
TimelineModel::markRoomAsRead()
{
    setCurrentIndex(0, true);
}

void
TimelineModel::updateLastReadId(const QString &currentRoomId)
{
    if (currentRoomId == room_id_) {
        last_event_id = cache::getFullyReadEventId(room_id_.toStdString());
        auto lastVisibleEventIndexAndId =
          cache::lastVisibleEvent(room_id_.toStdString(), last_event_id);
        if (lastVisibleEventIndexAndId) {
            fullyReadEventId_ = lastVisibleEventIndexAndId->second;
            emit fullyReadEventIdChanged();
        }
    }
}

void
TimelineModel::lastReadIdOnWindowFocus()
{
    /* this stops it from removing the line when focusing another window
     * and from removing the line when refocusing Komai */
    if (ChatPage::instance()->isRoomActive(room_id_) &&
        cache::calculateRoomReadStatus(room_id_.toStdString())) {
        updateLastReadId(room_id_);
    }
}

/*
 * if the event2order db didn't have the messages we needed when the room was opened
 * try again after these new messages were fetched
 */
void
TimelineModel::checkAfterFetch()
{
    if (fullyReadEventId_.empty()) {
        auto lastVisibleEventIndexAndId =
          cache::lastVisibleEvent(room_id_.toStdString(), last_event_id);
        if (lastVisibleEventIndexAndId) {
            fullyReadEventId_ = lastVisibleEventIndexAndId->second;
            emit fullyReadEventIdChanged();
        }
    }
}

void
TimelineModel::addPendingMessage(mtx::events::collections::TimelineEvents event)
{
    timeline::send::sendPendingMessage(
      room_id_,
      std::move(event),
      [this](mtx::events::collections::TimelineEvents pendingMessage) {
          emit addPendingMessageToStore(std::move(pendingMessage));
      },
      [this](const mtx::crypto::EncryptedFile &encryptionInfo) {
          emit newEncryptedImage(encryptionInfo);
      },
      [this]() {
          emit ChatPage::instance()->showNotification(
            tr("Failed to encrypt event, sending aborted!"));
      });

    fullyReadEventId_ = this->EventId;
    emit fullyReadEventIdChanged();
}

void
TimelineModel::openMedia(const QString &eventId)
{
    mediaController_.openMedia(eventId);
}

bool
TimelineModel::saveMedia(const QString &eventId) const
{
    return mediaController_.saveMedia(eventId);
}

bool
TimelineModel::copyMedia(const QString &eventId) const
{
    return mediaController_.copyMedia(eventId);
}

void
TimelineModel::cacheMedia(const QString &eventId,
                          const std::function<void(const QString)> &callback)
{
    mediaController_.cacheMedia(eventId, callback);
}

void
TimelineModel::cacheMedia(const QString &eventId)
{
    cacheMedia(eventId, nullptr);
}

void
TimelineModel::showEvent(QString eventId)
{
    using namespace std::chrono_literals;
    // Direct to eventId
    if (eventId[0] == '$') {
        int idx = idToIndex(eventId);
        if (idx == -1) {
            nhlog::ui()->warn("Scrolling to event id {}, failed - no known index",
                              eventId.toStdString());
            return;
        }
        eventIdToShow = eventId;
        emit scrollTargetChanged();
        showEventTimer.start(50ms);
        return;
    }
    // to message index
    eventId       = indexToId(eventId.toInt());
    eventIdToShow = eventId;
    emit scrollTargetChanged();
    showEventTimer.start(50ms);
    return;
}

void
TimelineModel::eventShown()
{
    eventIdToShow.clear();
    emit scrollTargetChanged();
}

QString
TimelineModel::scrollTarget() const
{
    return eventIdToShow;
}

void
TimelineModel::scrollTimerEvent()
{
    if (eventIdToShow.isEmpty() || showEventTimerCounter > 3) {
        showEventTimer.stop();
        showEventTimerCounter = 0;
    } else {
        emit scrollToIndex(idToIndex(eventIdToShow));
        showEventTimerCounter++;
    }
}

void
TimelineModel::requestKeyForEvent(const QString &id)
{
    auto encrypted_event = events.get(id.toStdString(), "", false);
    if (encrypted_event) {
        if (auto ev = std::get_if<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(
              encrypted_event))
            events.requestSession(*ev, true);
    }
}

QString
TimelineModel::getBareRoomLink(const QString &roomId)
{
    auto alias = cache::getStateEvent<mtx::events::state::CanonicalAlias>(roomId.toStdString());
    QString room;
    if (alias) {
        room = QString::fromStdString(alias->content.alias);
        if (room.isEmpty() && !alias->content.alt_aliases.empty()) {
            room = QString::fromStdString(alias->content.alt_aliases.front());
        }
    }

    if (room.isEmpty())
        room = roomId;

    return QStringLiteral("https://matrix.to/#/%1").arg(QString(QUrl::toPercentEncoding(room)));
}

QString
TimelineModel::getRoomVias(const QString &roomId)
{
    QStringList vias;

    for (const auto &m : utils::roomVias(roomId.toStdString())) {
        if (vias.size() >= 4)
            break;

        QString server =
          QStringLiteral("via=%1").arg(QString(QUrl::toPercentEncoding(QString::fromStdString(m))));

        if (!vias.contains(server))
            vias.push_back(server);
    }

    return vias.join("&");
}

void
TimelineModel::copyLinkToEvent(const QString &eventId) const
{
    // Event links shouldn't use an alias, since that can be repointed.
    auto link = QStringLiteral("https://matrix.to/#/%1/%2?%3")
                  .arg(QUrl::toPercentEncoding(room_id_),
                       QString(QUrl::toPercentEncoding(eventId)),
                       getRoomVias(room_id_));
    QGuiApplication::clipboard()->setText(link);
}

void
TimelineModel::triggerSpecialEffects()
{
    if (needsSpecialEffects_) {
        // Note (Loren): Without the timer, this apparently emits before QML is ready
        if (specialEffects_.testFlag(Confetti)) {
            QTimer::singleShot(1, this, [this] { emit confetti(); });
            specialEffects_.setFlag(Confetti, false);
        }
        if (specialEffects_.testFlag(Rainfall)) {
            QTimer::singleShot(1, this, [this] { emit rainfall(); });
            specialEffects_.setFlag(Rainfall, false);
        }
        needsSpecialEffects_ = false;
    }
}

void
TimelineModel::markSpecialEffectsDone()
{
    needsSpecialEffects_ = false;
    emit confettiDone();
    emit rainfallDone();

    specialEffects_.setFlag(Confetti, false);
    specialEffects_.setFlag(Rainfall, false);
}

QString
TimelineModel::formatTypingUsers(const QStringList &users, const QColor &bg, const QColor &accent)
{
    QString temp =
      tr("%1 and %2 are typing.",
         "Multiple users are typing. First argument is a comma separated list of potentially "
         "multiple users. Second argument is the last user of that list. (If only one user is "
         "typing, %1 is empty. You should still use it in your string though to silence Qt "
         "warnings.)",
         (int)users.size());

    if (users.empty()) {
        return {};
    }

    QStringList uidWithoutLast;

    auto formatUser = [this, bg, accent](const QString &user_id) -> QString {
        auto uncoloredUsername = utils::replaceEmoji(displayName(user_id));
        QString prefix =
          QStringLiteral("<font color=\"%1\">")
            .arg(manager_
                   ->roomUserColor(
                     roomId(),
                     user_id,
                     bg,
                     accent,
                     static_cast<int>(UserSettings::instance()->timelineUserColorCodingPolicy()))
                   .darker(130)
                   .name());

        // color only parts that don't have a font already specified
        QString coloredUsername;
        int index = 0;
        do {
            auto startIndex = uncoloredUsername.indexOf(QLatin1String("<font"), index);

            if (startIndex - index != 0)
                coloredUsername +=
                  prefix + uncoloredUsername.mid(index, startIndex > 0 ? startIndex - index : -1) +
                  QStringLiteral("</font>");

            auto endIndex = uncoloredUsername.indexOf(QLatin1String("</font>"), startIndex);
            if (endIndex > 0)
                endIndex += sizeof("</font>") - 1;

            if (endIndex - startIndex != 0)
                coloredUsername +=
                  QStringView(uncoloredUsername).mid(startIndex, endIndex - startIndex);

            index = endIndex;
        } while (index > 0 && index < uncoloredUsername.size());

        return coloredUsername;
    };

    uidWithoutLast.reserve(static_cast<int>(users.size()));
    for (qsizetype i = 0; i + 1 < users.size(); i++) {
        uidWithoutLast.append(formatUser(users[i]));
    }

    return temp.arg(uidWithoutLast.join(QStringLiteral(", ")), formatUser(users.back()));
}

QString
TimelineModel::formatJoinRuleEvent(
  const mtx::events::StateEvent<mtx::events::state::JoinRules> &event) const
{
    QString user = QString::fromStdString(event.sender);
    QString name = utils::replaceEmoji(displayName(user));

    switch (event.content.join_rule) {
    case mtx::events::state::JoinRule::Public:
        return tr("%1 opened the room to the public.").arg(name);
    case mtx::events::state::JoinRule::Invite:
        return tr("%1 made this room require an invitation to join.").arg(name);
    case mtx::events::state::JoinRule::Knock:
        return tr("%1 allowed to join this room by knocking.").arg(name);
    case mtx::events::state::JoinRule::Restricted: {
        QStringList rooms;
        for (const auto &r : event.content.allow) {
            if (r.type == mtx::events::state::JoinAllowanceType::RoomMembership)
                rooms.push_back(QString::fromStdString(r.room_id));
        }
        return tr("%1 allowed members of the following rooms to automatically join this "
                  "room: %2")
          .arg(name, rooms.join(QStringLiteral(", ")));
    }
    default:
        // Currently, knock and private are reserved keywords and not implemented in Matrix.
        return {};
    }
}

QString
TimelineModel::formatGuestAccessEvent(
  const mtx::events::StateEvent<mtx::events::state::GuestAccess> &event) const
{
    QString user = QString::fromStdString(event.sender);
    QString name = utils::replaceEmoji(displayName(user));

    switch (event.content.guest_access) {
    case mtx::events::state::AccessState::CanJoin:
        return tr("%1 made the room open to guests.").arg(name);
    case mtx::events::state::AccessState::Forbidden:
        return tr("%1 has closed the room to guest access.").arg(name);
    default:
        return {};
    }
}

QString
TimelineModel::formatHistoryVisibilityEvent(
  const mtx::events::StateEvent<mtx::events::state::HistoryVisibility> &event) const
{
    QString user = QString::fromStdString(event.sender);
    QString name = utils::replaceEmoji(displayName(user));

    switch (event.content.history_visibility) {
    case mtx::events::state::Visibility::WorldReadable:
        return tr("%1 made the room history world readable. Events may be now read by "
                  "non-joined people.")
          .arg(name);
    case mtx::events::state::Visibility::Shared:
        return tr("%1 set the room history visible to members from this point on.").arg(name);
    case mtx::events::state::Visibility::Invited:
        return tr("%1 set the room history visible to members since they were invited.").arg(name);
    case mtx::events::state::Visibility::Joined:
        return tr("%1 set the room history visible to members since they joined the room.")
          .arg(name);
    default:
        return {};
    }
}

QString
TimelineModel::formatPowerLevelEvent(
  const mtx::events::StateEvent<mtx::events::state::PowerLevels> &event) const
{
    return timeline::format::formatPowerLevelEvent(
      event, events, [this](const QString &userId) { return displayName(userId); });
}

QString
TimelineModel::formatImagePackEvent(
  const mtx::events::StateEvent<mtx::events::msc2545::ImagePack> &event) const
{
    return timeline::format::formatImagePackEvent(
      event,
      events,
      QFontMetrics(UserSettings::instance()->uiFontFamily()).ascent(),
      [this](const QString &userId) { return displayName(userId); });
}

QString
TimelineModel::formatPolicyRule(const QString &id) const
{
    return timeline::format::formatPolicyRule(
      id, events, [this](const QString &userId) { return displayName(userId); });
}

QVariantMap
TimelineModel::formatRedactedEvent(const QString &id)
{
    return timeline::format::formatRedactedEvent(
      id, events, [this](const QString &userId) { return displayName(userId); });
}

void
TimelineModel::acceptKnock(const QString &id)
{
    auto e = events.get(id.toStdString(), "");
    if (!e)
        return;

    auto event = std::get_if<mtx::events::StateEvent<mtx::events::state::Member>>(e);
    if (!event)
        return;

    if (!permissions_.canInvite())
        return;

    if (cache::isRoomMember(event->state_key, room_id_.toStdString()))
        return;

    using namespace mtx::events::state;
    if (event->content.membership != Membership::Knock)
        return;

    ChatPage::instance()->inviteUser(
      room_id_, QString::fromStdString(event->state_key), QLatin1String(""));
}

bool
TimelineModel::showAcceptKnockButton(const QString &id)
{
    auto e = events.get(id.toStdString(), "");
    if (!e)
        return false;

    auto event = std::get_if<mtx::events::StateEvent<mtx::events::state::Member>>(e);
    if (!event)
        return false;

    if (!permissions_.canInvite())
        return false;

    if (cache::isRoomMember(event->state_key, room_id_.toStdString()))
        return false;

    using namespace mtx::events::state;
    return event->content.membership == Membership::Knock;
}

void
TimelineModel::joinReplacementRoom(const QString &id)
{
    auto e = events.get(id.toStdString(), "");
    if (!e)
        return;

    auto event = std::get_if<mtx::events::StateEvent<mtx::events::state::Tombstone>>(e);
    if (!event)
        return;

    auto joined_rooms = cache::joinedRooms();
    for (const auto &roomid : joined_rooms) {
        if (roomid == event->content.replacement_room) {
            manager_->rooms()->setCurrentRoom(
              QString::fromStdString(event->content.replacement_room));
            return;
        }
    }

    ChatPage::instance()->joinRoomVia(
      event->content.replacement_room,
      {mtx::identifiers::parse<mtx::identifiers::User>(event->sender).hostname()},
      true);
}

QString
TimelineModel::formatMemberEvent(
  const mtx::events::StateEvent<mtx::events::state::Member> &event) const
{
    return timeline::format::formatMemberEvent(
      mtx::events::collections::TimelineEvents{event}, events, [this](const QString &userId) {
          return displayName(userId);
      });
}

void
TimelineModel::setThread(const QString &id)
{
    if (id.isEmpty()) {
        resetThread();
        return;
    } else if (id != thread_) {
        thread_ = id;
        emit threadChanged(thread_);
    }
}
void
TimelineModel::resetThread()
{
    if (!thread_.isEmpty()) {
        thread_.clear();
        emit threadChanged(thread_);
    }
}
void
TimelineModel::setReply(const QString &newReply)
{
    if (reply_ != newReply) {
        reply_ = newReply;

        if (auto ev = events.get(reply_.toStdString(), "", false, true))
            setThread(QString::fromStdString(
              mtx::accessors::relations(*ev).thread().value_or(thread_.toStdString())));

        emit replyChanged(reply_);
    }
}

void
TimelineModel::populateEditMentions(const mtx::events::collections::TimelineEvents &event,
                                    QStringList &mentions,
                                    QStringList &mentionTexts) const
{
    auto mentionsList = mtx::accessors::mentions(event);
    if (!mentionsList)
        return;

    if (mentionsList->room) {
        mentions.append(QStringLiteral(u"@room"));
        mentionTexts.append(QStringLiteral(u"@room"));
    }

    for (const auto &user : mentionsList->user_ids) {
        auto userid = QString::fromStdString(user);
        mentions.append(userid);
        mentionTexts.append(QStringLiteral("[%1](https://matrix.to/#/%2)")
                              .arg(utils::escapeMentionMarkdown(
                                     // not using TimelineModel::displayName here,
                                     // because it would double html escape
                                     cache::displayName(room_id_, userid)),
                                   QString(QUrl::toPercentEncoding(userid))));
    }
}

QString
TimelineModel::normalizeFormattedEditText(QString editText,
                                          const QString &quotedFormattedBody) const
{
    if (quotedFormattedBody.isEmpty())
        return editText;

    auto matches = conf::strings::matrixToLink.globalMatch(quotedFormattedBody);
    std::map<QString, QString> reverseNameMapping;
    while (matches.hasNext()) {
        auto m                            = matches.next();
        reverseNameMapping[m.captured(2)] = m.captured(1);
    }

    for (const auto &[user, link] : reverseNameMapping) {
        // TODO(Nico): html unescape the user name
        editText.replace(user,
                         QStringLiteral("[%1](%2)").arg(utils::escapeMentionMarkdown(user), link));
    }

    return editText;
}

bool
TimelineModel::isEditableTextMessageType(mtx::events::MessageType msgType) const
{
    return msgType == mtx::events::MessageType::Text ||
           msgType == mtx::events::MessageType::Notice ||
           msgType == mtx::events::MessageType::Emote ||
           msgType == mtx::events::MessageType::ElementEffect ||
           msgType == mtx::events::MessageType::Unknown;
}

void
TimelineModel::applyEditedMessageText(const mtx::events::collections::TimelineEvents &event,
                                      mtx::events::MessageType msgType,
                                      const QString &editText)
{
    if (msgType == mtx::events::MessageType::Emote)
        input()->setText("/me " + editText);
    else if (msgType == mtx::events::MessageType::ElementEffect) {
        auto u = std::get_if<mtx::events::RoomEvent<mtx::events::msg::ElementEffect>>(&event);
        auto msgtypeString = u ? u->content.msgtype : "";
        if (msgtypeString == "io.element.effect.rainfall")
            input()->setText("/rainfall " + editText);
        else if (msgtypeString == "nic.custom.confetti")
            input()->setText("/confetti " + editText);
        else
            input()->setText("/msgtype " + QString::fromStdString(msgtypeString) + " " + editText);
    } else if (msgType == mtx::events::MessageType::Unknown) {
        auto u = std::get_if<mtx::events::RoomEvent<mtx::events::msg::Unknown>>(&event);
        input()->setText("/msgtype " + (u ? QString::fromStdString(u->content.msgtype) : "") + " " +
                         editText);
    } else
        input()->setText(editText);
}

void
TimelineModel::setEdit(const QString &newEdit)
{
    if (newEdit.isEmpty()) {
        resetEdit();
        return;
    }

    if (edit_.isEmpty()) {
        input()->storeForEdit();
    }

    if (edit_ != newEdit) {
        auto ev = events.get(newEdit.toStdString(), "");
        if (ev && mtx::accessors::sender(*ev) == utils::localUser().toStdString()) {
            auto e = *ev;
            setReply(QString::fromStdString(mtx::accessors::relations(e).reply_to().value_or("")));
            setThread(QString::fromStdString(mtx::accessors::relations(e).thread().value_or("")));

            QStringList mentions, mentionTexts;
            populateEditMentions(e, mentions, mentionTexts);

            auto msgType = mtx::accessors::msg_type(e);
            if (isEditableTextMessageType(msgType)) {
                auto relInfo = relatedInfo(newEdit);
                auto editText =
                  normalizeFormattedEditText(relInfo.quoted_body, relInfo.quoted_formatted_body);
                applyEditedMessageText(e, msgType, editText);
            } else {
                input()->setText(QLatin1String(""));
            }
            input()->replaceMentions(std::move(mentions), std::move(mentionTexts));

            edit_ = newEdit;
        } else {
            resetReply();

            input()->setText(QLatin1String(""));
            edit_ = QLatin1String("");
        }
        emit editChanged(edit_);
    }
}

void
TimelineModel::resetEdit()
{
    if (!edit_.isEmpty()) {
        edit_ = QLatin1String("");
        emit editChanged(edit_);
        input()->restoreAfterEdit();
        if (replyBeforeEdit.isEmpty())
            resetReply();
        else
            setReply(replyBeforeEdit);
        replyBeforeEdit.clear();
    }
}

void
TimelineModel::resetState()
{
    http::client()->get_state(
      room_id_.toStdString(),
      [this](const mtx::responses::StateEvents &events_, mtx::http::RequestErr e) {
          if (e) {
              nhlog::net()->error("Failed to retrieve current room state: {}", *e);
              return;
          }

          emit newState(events_);
      });
}

QString
TimelineModel::roomName() const
{
    auto info = cache::getRoomInfo({room_id_.toStdString()});

    if (!info.count(room_id_))
        return {};
    else
        return utils::replaceEmoji(QString::fromStdString(info[room_id_].name).toHtmlEscaped());
}

QString
TimelineModel::plainRoomName() const
{
    auto info = cache::getRoomInfo({room_id_.toStdString()});

    if (!info.count(room_id_))
        return {};
    else
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

QString
TimelineModel::directChatOtherUserId() const
{
    if (roomMemberCount() < 3) {
        QString id;
        for (const auto &member : cache::getMembers(room_id_.toStdString()))
            if (member.user_id != UserSettings::instance()->userId())
                id = member.user_id;
        return id;
    } else
        return {};
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

#include "moc_TimelineModel.cpp"

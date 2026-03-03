// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Utils.h"

#include <array>
#include <variant>

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringBuilder>

#include "EventAccessors.h"
#include "MatrixClient.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {
QString
getQuoteBody(const RelatedInfo &related)
{
    using MsgType = mtx::events::MessageType;

    switch (related.type) {
    case MsgType::File: {
        return QStringLiteral("sent a file.");
    }
    case MsgType::Image: {
        return QStringLiteral("sent an image.");
    }
    case MsgType::Audio: {
        return QStringLiteral("sent an audio file.");
    }
    case MsgType::Video: {
        return QStringLiteral("sent a video");
    }
    default: {
        return related.quoted_body;
    }
    }
}

//! Match widgets/events with a description message.
template<class T>
QString
messageDescription(const QString &username,
                   const QString &body,
                   const bool isLocal,
                   bool containsSpoiler)
{
    using Audio         = mtx::events::RoomEvent<mtx::events::msg::Audio>;
    using Emote         = mtx::events::RoomEvent<mtx::events::msg::Emote>;
    using File          = mtx::events::RoomEvent<mtx::events::msg::File>;
    using Image         = mtx::events::RoomEvent<mtx::events::msg::Image>;
    using Notice        = mtx::events::RoomEvent<mtx::events::msg::Notice>;
    using Sticker       = mtx::events::Sticker;
    using Text          = mtx::events::RoomEvent<mtx::events::msg::Text>;
    using Unknown       = mtx::events::RoomEvent<mtx::events::msg::Unknown>;
    using Video         = mtx::events::RoomEvent<mtx::events::msg::Video>;
    using ElementEffect = mtx::events::RoomEvent<mtx::events::msg::ElementEffect>;
    using CallInvite    = mtx::events::RoomEvent<mtx::events::voip::CallInvite>;
    using CallAnswer    = mtx::events::RoomEvent<mtx::events::voip::CallAnswer>;
    using CallHangUp    = mtx::events::RoomEvent<mtx::events::voip::CallHangUp>;
    using CallReject    = mtx::events::RoomEvent<mtx::events::voip::CallReject>;
    using Encrypted     = mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>;

    if (std::is_same<T, Audio>::value) {
        if (isLocal)
            return QCoreApplication::translate("message-description sent:",
                                               "You sent an audio clip");
        else
            return QCoreApplication::translate("message-description sent:", "%1 sent an audio clip")
              .arg(username);
    } else if (std::is_same<T, Image>::value) {
        if (isLocal)
            return QCoreApplication::translate("message-description sent:", "You sent an image");
        else
            return QCoreApplication::translate("message-description sent:", "%1 sent an image")
              .arg(username);
    } else if (std::is_same<T, File>::value) {
        if (isLocal)
            return QCoreApplication::translate("message-description sent:", "You sent a file");
        else
            return QCoreApplication::translate("message-description sent:", "%1 sent a file")
              .arg(username);
    } else if (std::is_same<T, Video>::value) {
        if (isLocal)
            return QCoreApplication::translate("message-description sent:", "You sent a video");
        else
            return QCoreApplication::translate("message-description sent:", "%1 sent a video")
              .arg(username);
    } else if (std::is_same<T, Sticker>::value) {
        if (isLocal)
            return QCoreApplication::translate("message-description sent:", "You sent a sticker");
        else
            return QCoreApplication::translate("message-description sent:", "%1 sent a sticker")
              .arg(username);
    } else if (std::is_same<T, Notice>::value) {
        if (isLocal)
            return QCoreApplication::translate("message-description sent:",
                                               "You sent a notification");
        else
            return QCoreApplication::translate("message-description sent:",
                                               "%1 sent a notification")
              .arg(username);
    } else if (std::is_same<T, Text>::value || std::is_same<T, Unknown>::value) {
        if (containsSpoiler) {
            if (isLocal)
                return QCoreApplication::translate("message-description sent:",
                                                   "You sent a spoiler.");
            else
                return QCoreApplication::translate("message-description sent:",
                                                   "%1 sent a spoiler.")
                  .arg(username);
        }

        if (isLocal)
            return QCoreApplication::translate("message-description sent:", "You: %1").arg(body);
        else
            return QCoreApplication::translate("message-description sent:", "%1: %2")
              .arg(username, body);
    } else if (std::is_same<T, ElementEffect>::value) {
        if (body.isEmpty()) {
            // TODO: what is the best way to handle this?
            if (isLocal)
                return QCoreApplication::translate("message-description sent:",
                                                   "You sent a chat effect");
            else
                return QCoreApplication::translate("message-description sent:",
                                                   "%1 sent a chat effect")
                  .arg(username);
        } else {
            if (containsSpoiler) {
                if (isLocal)
                    return QCoreApplication::translate("message-description sent:",
                                                       "You sent a spoiler.");
                else
                    return QCoreApplication::translate("message-description sent:",
                                                       "%1 sent a spoiler.")
                      .arg(username);
            }

            if (isLocal)
                return QCoreApplication::translate("message-description sent:", "You: %1")
                  .arg(body);
            else
                return QCoreApplication::translate("message-description sent:", "%1: %2")
                  .arg(username, body);
        }
    } else if (std::is_same<T, Emote>::value) {
        if (containsSpoiler) {
            return QCoreApplication::translate("message-description sent:",
                                               "* %1 spoils something.")
              .arg(username);
        }

        return QStringLiteral("* %1 %2").arg(username, body);
    } else if (std::is_same<T, Encrypted>::value) {
        if (isLocal)
            return QCoreApplication::translate("message-description sent:",
                                               "You sent an encrypted message");
        else
            return QCoreApplication::translate("message-description sent:",
                                               "%1 sent an encrypted message")
              .arg(username);
    } else if (std::is_same<T, CallInvite>::value) {
        if (isLocal)
            return QCoreApplication::translate("message-description sent:", "You placed a call");
        else
            return QCoreApplication::translate("message-description sent:", "%1 placed a call")
              .arg(username);
    } else if (std::is_same<T, CallAnswer>::value) {
        if (isLocal)
            return QCoreApplication::translate("message-description sent:", "You answered a call");
        else
            return QCoreApplication::translate("message-description sent:", "%1 answered a call")
              .arg(username);
    } else if (std::is_same<T, CallHangUp>::value) {
        if (isLocal)
            return QCoreApplication::translate("message-description sent:", "You ended a call");
        else
            return QCoreApplication::translate("message-description sent:", "%1 ended a call")
              .arg(username);
    } else if (std::is_same<T, CallReject>::value) {
        if (isLocal)
            return QCoreApplication::translate("message-description sent:", "You rejected a call");
        else
            return QCoreApplication::translate("message-description sent:", "%1 rejected a call")
              .arg(username);
    } else {
        return QCoreApplication::translate("utils", "Unknown Message Type");
    }
}

template<class T, class Event>
DescInfo
createDescriptionInfo(const Event &event, const QString &localUser, const QString &displayName)
{
    const auto msg    = std::get<T>(event);
    const auto sender = QString::fromStdString(msg.sender);

    const auto username = displayName;
    const auto ts       = QDateTime::fromMSecsSinceEpoch(msg.origin_server_ts);
    auto body           = mtx::accessors::body(event);
    auto formatted_body = mtx::accessors::formatted_body(event);
    if (mtx::accessors::relations(event).reply_to()) {
        body           = utils::stripReplyFromBody(body);
        formatted_body = utils::stripReplyFromFormattedBody(formatted_body);
    }

    // Simplistic heuristic
    bool containsSpoiler = formatted_body.find("<span data-mx-spoiler") != formatted_body.npos;

    return DescInfo{QString::fromStdString(msg.event_id),
                    sender,
                    messageDescription<T>(
                      username, QString::fromStdString(body), sender == localUser, containsSpoiler),
                    utils::descriptiveTime(ts),
                    msg.origin_server_ts,
                    ts};
}
} // namespace

std::string
utils::stripReplyFromBody(const std::string &bodyi)
{
    QString body = QString::fromStdString(bodyi);
    if (body.startsWith(QLatin1String("> <"))) {
        auto segments = body.split('\n');
        while (!segments.isEmpty() && segments.begin()->startsWith('>'))
            segments.erase(segments.cbegin());
        if (!segments.empty() && segments.first().isEmpty())
            segments.erase(segments.cbegin());
        body = segments.join('\n');
    }

    body.replace(QLatin1String("@room"), QString::fromUtf8("@\u2060room"));
    return body.toStdString();
}

std::string
utils::stripReplyFromFormattedBody(const std::string &formatted_bodyi)
{
    QString formatted_body = QString::fromStdString(formatted_bodyi);
    static QRegularExpression replyRegex(QStringLiteral("<mx-reply>.*</mx-reply>"),
                                         QRegularExpression::DotMatchesEverythingOption);
    formatted_body.remove(replyRegex);
    formatted_body.replace(QLatin1String("@room"), QString::fromUtf8("@\u2060room"));
    return formatted_body.toStdString();
}

RelatedInfo
utils::stripReplyFallbacks(const mtx::events::collections::TimelineEvents &event,
                           std::string id,
                           QString room_id_)
{
    RelatedInfo related   = {};
    related.quoted_user   = QString::fromStdString(mtx::accessors::sender(event));
    related.related_event = std::move(id);
    related.type          = mtx::accessors::msg_type(event);

    // get body, strip reply fallback, then transform the event to text, if it is a media event
    // etc
    related.quoted_body = QString::fromStdString(mtx::accessors::body(event));
    related.quoted_body =
      QString::fromStdString(stripReplyFromBody(related.quoted_body.toStdString()));
    related.quoted_body = getQuoteBody(related);

    // get quoted body and strip reply fallback
    related.quoted_formatted_body = mtx::accessors::formattedBodyWithFallback(event);
    related.quoted_formatted_body = QString::fromStdString(
      stripReplyFromFormattedBody(related.quoted_formatted_body.toStdString()));
    related.room = room_id_;

    return related;
}

QString
utils::localUser()
{
    if (const auto settings = UserSettings::instance()) {
        const auto sessionUserId = settings->userId().trimmed();
        if (!sessionUserId.isEmpty())
            return sessionUserId;
    }

    return QString::fromStdString(http::client()->user_id().to_string());
}

bool
utils::codepointIsEmoji(uint code)
{
    // TODO: Be more precise here.
    return (code >= 0x2600 && code <= 0x27bf) || (code >= 0x2b00 && code <= 0x2bff) ||
           (code >= 0x1f000 && code <= 0x1faff) || code == 0x200d || code == 0xfe0f ||
           code == 0x231a || code == 0x231b || (code >= 0x23e9 && code <= 0x23ff) ||
           code == 0x25aa || code == 0x25ab || code == 0x25b6 || code == 0x25c0 ||
           (code >= 0x25fb && code <= 0x25fe) || (code >= 0x2460 && code <= 0x24ff);
}

QString
utils::replaceEmoji(const QString &body)
{
    if constexpr (QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)) {
        return body;
    } else {
        QString fmtBody;
        fmtBody.reserve(body.size());

        QVector<uint> utf32_string = body.toUcs4();

        bool insideFontBlock = false;
        bool insideTag       = false;
        for (auto &code : utf32_string) {
            if (code == U'<')
                insideTag = true;
            else if (code == U'>')
                insideTag = false;

            if (!insideTag && utils::codepointIsEmoji(code)) {
                if (!insideFontBlock) {
                    fmtBody += QStringLiteral("<font face=\"") %
                               UserSettings::instance()->uiFontEmojiFamily() %
                               (UserSettings::instance()->timelineMessagesEmojiOnlyEnlarge()
                                  ? QStringLiteral("\" size=\"4\">")
                                  : QStringLiteral("\">") );
                    insideFontBlock = true;
                } else if (code == 0xfe0f) {
                    // BUG(Nico):
                    // Workaround https://bugreports.qt.io/browse/QTBUG-97401
                    // See also https://github.com/matrix-org/matrix-react-sdk/pull/1458/files
                    // Upstream bug: https://github.com/Nheko-Reborn/nheko/issues/439
                    continue;
                }
            } else {
                if (insideFontBlock) {
                    fmtBody += QStringLiteral("</font>");
                    insideFontBlock = false;
                }
            }
            if (QChar::requiresSurrogates(code)) {
                QChar emoji[] = {static_cast<ushort>(QChar::highSurrogate(code)),
                                 static_cast<ushort>(QChar::lowSurrogate(code))};
                fmtBody.append(emoji, 2);
            } else {
                fmtBody.append(QChar(static_cast<ushort>(code)));
            }
        }
        if (insideFontBlock) {
            fmtBody += QStringLiteral("</font>");
        }

        return fmtBody;
    }
}

QString
utils::descriptiveTime(const QDateTime &then)
{
    const auto now  = QDateTime::currentDateTime();
    const auto days = then.daysTo(now);

    if (days == 0)
        return QLocale::system().toString(then.time(), QLocale::ShortFormat);
    else if (days < 2)
        return QString(QCoreApplication::translate("descriptiveTime", "Yesterday"));
    else if (days < 7)
        return then.toString(QStringLiteral("dddd"));

    return QLocale::system().toString(then.date(), QLocale::ShortFormat);
}

DescInfo
utils::getMessageDescription(const mtx::events::collections::TimelineEvents &event,
                             const QString &localUser,
                             const QString &displayName)
{
    using Audio         = mtx::events::RoomEvent<mtx::events::msg::Audio>;
    using Emote         = mtx::events::RoomEvent<mtx::events::msg::Emote>;
    using File          = mtx::events::RoomEvent<mtx::events::msg::File>;
    using Image         = mtx::events::RoomEvent<mtx::events::msg::Image>;
    using Notice        = mtx::events::RoomEvent<mtx::events::msg::Notice>;
    using Text          = mtx::events::RoomEvent<mtx::events::msg::Text>;
    using Unknown       = mtx::events::RoomEvent<mtx::events::msg::Unknown>;
    using Video         = mtx::events::RoomEvent<mtx::events::msg::Video>;
    using ElementEffect = mtx::events::RoomEvent<mtx::events::msg::ElementEffect>;
    using CallInvite    = mtx::events::RoomEvent<mtx::events::voip::CallInvite>;
    using CallAnswer    = mtx::events::RoomEvent<mtx::events::voip::CallAnswer>;
    using CallHangUp    = mtx::events::RoomEvent<mtx::events::voip::CallHangUp>;
    using CallReject    = mtx::events::RoomEvent<mtx::events::voip::CallReject>;
    using Encrypted     = mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>;

    if (std::holds_alternative<Audio>(event)) {
        return createDescriptionInfo<Audio>(event, localUser, displayName);
    } else if (std::holds_alternative<Emote>(event)) {
        return createDescriptionInfo<Emote>(event, localUser, displayName);
    } else if (std::holds_alternative<File>(event)) {
        return createDescriptionInfo<File>(event, localUser, displayName);
    } else if (std::holds_alternative<Image>(event)) {
        return createDescriptionInfo<Image>(event, localUser, displayName);
    } else if (std::holds_alternative<Notice>(event)) {
        return createDescriptionInfo<Notice>(event, localUser, displayName);
    } else if (std::holds_alternative<Text>(event)) {
        return createDescriptionInfo<Text>(event, localUser, displayName);
    } else if (std::holds_alternative<Unknown>(event)) {
        return createDescriptionInfo<Unknown>(event, localUser, displayName);
    } else if (std::holds_alternative<Video>(event)) {
        return createDescriptionInfo<Video>(event, localUser, displayName);
    } else if (std::holds_alternative<ElementEffect>(event)) {
        return createDescriptionInfo<ElementEffect>(event, localUser, displayName);
    } else if (std::holds_alternative<CallInvite>(event)) {
        return createDescriptionInfo<CallInvite>(event, localUser, displayName);
    } else if (std::holds_alternative<CallAnswer>(event)) {
        return createDescriptionInfo<CallAnswer>(event, localUser, displayName);
    } else if (std::holds_alternative<CallHangUp>(event)) {
        return createDescriptionInfo<CallHangUp>(event, localUser, displayName);
    } else if (std::holds_alternative<CallReject>(event)) {
        return createDescriptionInfo<CallReject>(event, localUser, displayName);
    } else if (std::holds_alternative<mtx::events::Sticker>(event)) {
        return createDescriptionInfo<mtx::events::Sticker>(event, localUser, displayName);
    } else if (auto msg = std::get_if<Encrypted>(&event); msg != nullptr) {
        const auto sender = QString::fromStdString(msg->sender);

        const auto username = displayName;
        const auto ts       = QDateTime::fromMSecsSinceEpoch(msg->origin_server_ts);

        DescInfo info;
        info.userid = sender;
        info.body   = QStringLiteral(" %1").arg(
          messageDescription<Encrypted>(username, QLatin1String(""), sender == localUser, false));
        info.timestamp       = msg->origin_server_ts;
        info.descriptiveTime = utils::descriptiveTime(ts);
        info.event_id        = QString::fromStdString(msg->event_id);
        info.datetime        = ts;

        return info;
    }

    return DescInfo{};
}

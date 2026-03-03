// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Utils.h"

#include <array>
#include <cmath>
#include <unordered_set>
#include <variant>

#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QCryptographicHash>
#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QProcessEnvironment>
#include <QRandomGenerator64>
#include <QScreen>
#include <QStringBuilder>
#include <QTextDocument>
#include <QTimer>
#include <QWindow>
#include <QXmlStreamReader>

#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include <mtx/responses/messages.hpp>

#include "ChatPage.h"
#include "Config.h"
#include "EventAccessors.h"
#include "Logging.h"
#include "MatrixClient.h"
#include "cache/Cache.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/formattedmessage/HtmlProcessor.h"

static QString
getQuoteBody(const RelatedInfo &related);

//! Match widgets/events with a description message.
namespace {
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
}

template<class T, class Event>
static DescInfo
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
                                  : QStringLiteral("\">"));
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

QString
utils::firstChar(const QString &input)
{
    if (input.isEmpty())
        return input;

    for (auto const &c : input.toStdU32String()) {
        if (QString::fromUcs4(&c, 1) != QStringLiteral("#"))
            return QString::fromUcs4(&c, 1).toUpper();
    }

    return QString::fromUcs4(&input.toStdU32String().at(0), 1).toUpper();
}

QString
utils::humanReadableFileSize(uint64_t bytes)
{
    constexpr static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    constexpr static const int length    = sizeof(units) / sizeof(units[0]);

    int u       = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && u < length) {
        ++u;
        size /= 1024.0;
    }

    return QString::number(size, 'g', 4) + ' ' + units[u];
}

int
utils::levenshtein_distance(const std::string &s1, const std::string &s2)
{
    const auto nlen = s1.size();
    const auto hlen = s2.size();

    if (hlen == 0)
        return -1;
    if (nlen == 1)
        return (int)s2.find(s1);

    std::vector<int> row1(hlen + 1, 0);

    for (size_t i = 0; i < nlen; ++i) {
        std::vector<int> row2(1, (int)i + 1);

        for (size_t j = 0; j < hlen; ++j) {
            const int cost = s1[i] != s2[j];
            row2.push_back(std::min(row1[j + 1] + 1, std::min(row2[j] + 1, row1[j] + cost)));
        }

        row1.swap(row2);
    }

    return *std::min_element(row1.begin(), row1.end());
}

QPixmap
utils::scaleImageToPixmap(const QImage &img, int size)
{
    if (img.isNull())
        return QPixmap();

    // Deprecated in 5.13: const double sz =
    //  std::ceil(QApplication::desktop()->screen()->devicePixelRatioF() * (double)size);
    const int sz = static_cast<int>(
      std::ceil(QGuiApplication::primaryScreen()->devicePixelRatio() * (double)size));
    return QPixmap::fromImage(img.scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

QPixmap
utils::scaleDown(uint64_t maxWidth, uint64_t maxHeight, const QPixmap &source)
{
    if (source.isNull())
        return QPixmap();

    const double widthRatio     = (double)maxWidth / (double)source.width();
    const double heightRatio    = (double)maxHeight / (double)source.height();
    const double minAspectRatio = std::min(widthRatio, heightRatio);

    // Size of the output image.
    int w, h = 0;

    if (minAspectRatio > 1) {
        w = source.width();
        h = source.height();
    } else {
        w = static_cast<int>(static_cast<double>(source.width()) * minAspectRatio);
        h = static_cast<int>(static_cast<double>(source.height()) * minAspectRatio);
    }

    return source.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QString
utils::mxcToHttp(const QUrl &url, const QString &server, int port)
{
    auto mxcParts = mtx::client::utils::parse_mxc_url(url.toString().toStdString());

    return QStringLiteral("https://%1:%2/_matrix/media/r0/download/%3/%4")
      .arg(server)
      .arg(port)
      .arg(QString::fromStdString(mxcParts.server), QString::fromStdString(mxcParts.media_id));
}

QString
utils::humanReadableFingerprint(const std::string &ed25519_)
{
    auto ed25519 = QString::fromStdString(ed25519_);
    QString fingerprint;
    for (int i = 0; i < ed25519.length(); i = i + 4) {
        fingerprint.append(QStringView(ed25519).mid(i, 4));
        if (i > 0 && i == 20)
            fingerprint.append('\n');
        else if (i < ed25519.length())
            fingerprint.append(' ');
    }
    return fingerprint;
}

QString
utils::linkifyMessage(const QString &body)
{
    return timeline::formattedmessage::linkifyHtml(body);
}

QString
utils::escapeMentionMarkdown(QString input)
{
    input = input.toHtmlEscaped();

    constexpr std::array<char, 10> markdownChars = {
      '\\',
      '`',
      '*',
      '_',
      /*'{', '}',*/ '[',
      ']',
      '<',
      '>',
      /* '(', ')',  '#', '-', '+', '.', '!', */ '~',
      '|',
    };

    QByteArray replacement = "\\\\";

    for (char c : markdownChars) {
        replacement[1] = c;
        input.replace(QChar::fromLatin1(c), QLatin1StringView(replacement));
    }

    return input;
}

QString
utils::escapeBlacklistedHtml(const QString &rawStr)
{
    return timeline::formattedmessage::sanitizeHtml(rawStr);
}

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

QString
utils::linkColor()
{
    const auto theme = UserSettings::instance()->uiThemeSlug();

    if (theme == QLatin1String("light")) {
        return QStringLiteral("#0077b5");
    } else if (theme == QLatin1String("dark")) {
        return QStringLiteral("#38A3D8");
    } else {
        return QPalette().color(QPalette::Link).name();
    }
}

uint32_t
utils::hashQString(const QString &input)
{
    auto h = QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha1);

    return (static_cast<uint32_t>(h[0]) << 24) ^ (static_cast<uint32_t>(h[1]) << 16) ^
           (static_cast<uint32_t>(h[2]) << 8) ^ static_cast<uint32_t>(h[3]);
}

QColor
utils::generateContrastingHexColor(const QString &input, const QColor &backgroundCol)
{
    const qreal backgroundLum = luminance(backgroundCol);

    // Create a color for the input
    auto hash = hashQString(input);
    // create a hue value based on the hash of the input.
    // Adapted to make Nico blue
    auto userHue = static_cast<double>(hash - static_cast<uint32_t>(0x60'00'00'00)) /
                   std::numeric_limits<uint32_t>::max() * 360.;
    // start with moderate saturation and lightness values.
    auto sat       = 230.;
    auto lightness = 125.;

    // converting to a QColor makes the luminance calc easier.
    QColor inputColor = QColor::fromHsl(
      static_cast<int>(userHue), static_cast<int>(sat), static_cast<int>(lightness));

    // calculate the initial luminance and contrast of the
    // generated color.  It's possible that no additional
    // work will be necessary.
    auto lum      = luminance(inputColor);
    auto contrast = computeContrast(lum, backgroundLum);

    // If the contrast doesn't meet our criteria,
    // try again and again until they do by modifying first
    // the lightness and then the saturation of the color.
    int iterationCount = 9;
    while (contrast < 4.5) {
        // if our lightness is at it's bounds, try changing
        // saturation instead.
        if (lightness >= 242 || lightness <= 13) {
            qreal newSat = qBound(26.0, sat * 1.25, 242.0);

            inputColor.setHsl(static_cast<int>(userHue),
                              static_cast<int>(qFloor(newSat)),
                              static_cast<int>(lightness));
            auto tmpLum         = luminance(inputColor);
            auto higherContrast = computeContrast(tmpLum, backgroundLum);
            if (higherContrast > contrast) {
                contrast = higherContrast;
                sat      = newSat;
            } else {
                newSat = qBound(26.0, sat / 1.25, 242.0);
                inputColor.setHsl(static_cast<int>(userHue),
                                  static_cast<int>(qFloor(newSat)),
                                  static_cast<int>(lightness));
                tmpLum             = luminance(inputColor);
                auto lowerContrast = computeContrast(tmpLum, backgroundLum);
                if (lowerContrast > contrast) {
                    contrast = lowerContrast;
                    sat      = newSat;
                }
            }
        } else {
            qreal newLightness = qBound(13.0, lightness * 1.25, 242.0);

            inputColor.setHsl(static_cast<int>(userHue),
                              static_cast<int>(sat),
                              static_cast<int>(qFloor(newLightness)));

            auto tmpLum         = luminance(inputColor);
            auto higherContrast = computeContrast(tmpLum, backgroundLum);

            // Check to make sure we have actually improved contrast
            if (higherContrast > contrast) {
                contrast  = higherContrast;
                lightness = newLightness;
                // otherwise, try going the other way instead.
            } else {
                newLightness = qBound(13.0, lightness / 1.25, 242.0);
                inputColor.setHsl(static_cast<int>(userHue),
                                  static_cast<int>(sat),
                                  static_cast<int>(qFloor(newLightness)));
                tmpLum             = luminance(inputColor);
                auto lowerContrast = computeContrast(tmpLum, backgroundLum);
                if (lowerContrast > contrast) {
                    contrast  = lowerContrast;
                    lightness = newLightness;
                }
            }
        }

        // don't loop forever, just give up at some point!
        // Someone smart may find a better solution
        if (--iterationCount < 0)
            break;
    }

    // get the hex value of the generated color.
    auto colorHex = inputColor.name();

    return colorHex;
}

qreal
utils::computeContrast(const qreal &one, const qreal &two)
{
    auto ratio = (one + 0.05) / (two + 0.05);

    if (two > one) {
        ratio = 1 / ratio;
    }

    return ratio;
}

qreal
utils::luminance(const QColor &col)
{
    int colRgb[3] = {col.red(), col.green(), col.blue()};
    qreal lumRgb[3];

    for (int i = 0; i < 3; i++) {
        qreal v   = colRgb[i] / 255.0;
        lumRgb[i] = v <= 0.03928 ? v / 12.92 : qPow((v + 0.055) / 1.055, 2.4);
    }

    auto lum = lumRgb[0] * 0.2126 + lumRgb[1] * 0.7152 + lumRgb[2] * 0.0722;

    return lum;
}

void
utils::centerWidget(QWidget *widget, QWindow *parent)
{
    if (parent) {
        widget->window()->windowHandle()->setTransientParent(parent);
        return;
    }

    auto findCenter = [childRect = widget->rect()](QRect hostRect) -> QPoint {
        return QPoint(static_cast<int>(hostRect.center().x() - (childRect.width() * 0.5)),
                      static_cast<int>(hostRect.center().y() - (childRect.height() * 0.5)));
    };
    widget->move(findCenter(QGuiApplication::primaryScreen()->geometry()));
}

void
utils::restoreCombobox(QComboBox *combo, const QString &value)
{
    for (auto i = 0; i < combo->count(); ++i) {
        if (value == combo->itemText(i)) {
            combo->setCurrentIndex(i);
            break;
        }
    }
}

QImage
utils::readImageFromFile(const QString &filename)
{
    QImageReader reader(filename);
    reader.setAutoTransform(true);
    return reader.read();
}
QImage
utils::readImage(const QByteArray &data)
{
    QBuffer buf;
    buf.setData(data);
    QImageReader reader(&buf);
    reader.setAutoTransform(true);
    return reader.read();
}

bool
utils::isReply(const mtx::events::collections::TimelineEvents &e)
{
    return mtx::accessors::relations(e).reply_to().has_value();
}

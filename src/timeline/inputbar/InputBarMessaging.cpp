// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "InputBar.h"

#include <algorithm>
#include <functional>
#include <map>
#include <string_view>

#include <nlohmann/json.hpp>

#include "TimelineModel.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "events/EventAccessors.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "utils/Utils.h"

std::string
threadFallbackEventId(const std::string &room_id, const std::string &thread_id)
{
    auto event_ids = cache::relatedEvents(room_id, thread_id);

    std::map<uint64_t, std::string_view, std::greater<>> orderedEvents;

    for (const auto &e : event_ids) {
        if (auto index = cache::getTimelineIndex(room_id, e))
            orderedEvents.emplace(*index, e);
    }

    for (const auto &[index, event_id] : orderedEvents) {
        (void)index;
        if (auto event = cache::getEvent(room_id, event_id)) {
            if (mtx::accessors::relations(event.value()).thread() == thread_id)
                return std::string(event_id);
        }
    }
    return thread_id;
}

namespace {
QString
replaceMatrixToMarkdownLink(QString input)
{
    bool replaced = false;
    do {
        replaced = false;

        int endOfName = input.indexOf("](https://matrix.to/#/");
        int startOfName;
        int nestingCount = 0;
        for (startOfName = endOfName - 1; startOfName > 0; startOfName--) {
            // skip escaped chars
            if (startOfName > 0 && input[startOfName - 1] == '\\')
                continue;

            if (input[startOfName] == '[') {
                if (nestingCount <= 0)
                    break;
                else
                    nestingCount--;
            }
            if (input[startOfName] == ']')
                nestingCount++;
        }
        if (startOfName < 0 || nestingCount > 0)
            break;

        int endOfLink = input.indexOf(')', endOfName);
        int newline   = input.indexOf('\n', endOfName);
        if (endOfLink > endOfName && (newline == -1 || endOfLink < newline)) {
            auto name = input.mid(startOfName + 1, endOfName - startOfName - 1);
            name.remove(QChar(u'\\'), Qt::CaseSensitive);
            input.replace(startOfName, endOfLink - startOfName + 1, name);
            replaced = true;
        }
    } while (replaced);

    return input;
}
} // namespace

mtx::common::Relations
InputBar::generateRelations() const
{
    mtx::common::Relations relations;
    if (!room->thread().isEmpty()) {
        relations.relations.push_back(
          {mtx::common::RelationType::Thread, room->thread().toStdString()});
        if (room->reply().isEmpty())
            relations.relations.push_back(
              {mtx::common::RelationType::InReplyTo,
               threadFallbackEventId(room->roomId().toStdString(), room->thread().toStdString()),
               std::nullopt,
               true});
    }
    if (!room->reply().isEmpty()) {
        relations.relations.push_back(
          {mtx::common::RelationType::InReplyTo, room->reply().toStdString()});
    }
    if (!room->edit().isEmpty()) {
        relations.relations.push_back(
          {mtx::common::RelationType::Replace, room->edit().toStdString()});
    }
    return relations;
}

mtx::common::Mentions
InputBar::generateMentions() const
{
    std::vector<std::string> userMentions;
    bool atRoom = false;
    for (const auto &m : std::as_const(mentions_))
        if (m == u"@room")
            atRoom = true;
        else
            userMentions.push_back(m.toStdString());

    if (!room->reply().isEmpty()) {
        auto replyToSender =
          room->dataById(room->reply(), TimelineModel::Roles::UserId, "").toString().toStdString();
        if (!replyToSender.empty() &&
            std::ranges::find(userMentions, replyToSender) == userMentions.end()) {
            userMentions.push_back(replyToSender);
        }
    }

    auto mention = mtx::common::Mentions{
      .user_ids = userMentions,
      // We use the atRoom from the mentions list to allow suppressing a room mention
      .room = atRoom,
    };

    // this->containsAtRoom_ = false;
    // this->mentions_.clear();
    // this->mentionTexts_.clear();

    return mention;
}

QString
InputBar::replaceTextEmoticons(const QString &input) const
{
    auto setting = ChatPage::instance()->userSettings()->composerInputAutoReplaceEmoji();

    if (setting == UserSettings::AutoReplaceEmoji::Never)
        return input;

    // Emoticon table: longest patterns first to avoid partial matches.
    // Order matters: </3 must be checked before <3.
    struct Emoticon
    {
        const char *pattern;
        const char *emoji;
    };
    static const Emoticon table[] = {
      // Longer (with-nose) variants first to prevent partial matches
      {":-)", "\xF0\x9F\x99\x82"}, // U+1F642 slightly smiling face
      {":-(", "\xF0\x9F\x99\x81"}, // U+1F641 slightly frowning face
      {":-D", "\xF0\x9F\x98\x80"}, // U+1F600 grinning face
      {";-)", "\xF0\x9F\x98\x89"}, // U+1F609 winking face
      {":-P", "\xF0\x9F\x98\x9B"}, // U+1F61B tongue face
      {":-O", "\xF0\x9F\x98\xAE"}, // U+1F62E open mouth
      {":-/", "\xF0\x9F\x98\x95"}, // U+1F615 confused face
      {":'(", "\xF0\x9F\x98\xA2"}, // U+1F622 crying face
      {"</3", "\xF0\x9F\x92\x94"}, // U+1F494 broken heart (before <3!)
      // Short variants
      {":)", "\xF0\x9F\x99\x82"}, // U+1F642 slightly smiling face
      {":(", "\xF0\x9F\x99\x81"}, // U+1F641 slightly frowning face
      {":D", "\xF0\x9F\x98\x80"}, // U+1F600 grinning face
      {";)", "\xF0\x9F\x98\x89"}, // U+1F609 winking face
      {":P", "\xF0\x9F\x98\x9B"}, // U+1F61B tongue face
      {":O", "\xF0\x9F\x98\xAE"}, // U+1F62E open mouth
      {"<3", "\xE2\x9D\xA4"},     // U+2764 red heart
      {":/", "\xF0\x9F\x98\x95"}, // U+1F615 confused face
    };

    QString result = input;

    if (setting == UserSettings::AutoReplaceEmoji::OnlyAtEnd) {
        // Only replace a single emoticon at the very end of the message.
        QString trimmed = result.trimmed();
        if (trimmed.isEmpty())
            return result;

        for (const auto &e : table) {
            QString pat = QString::fromUtf8(e.pattern);
            if (trimmed.endsWith(pat, Qt::CaseInsensitive)) {
                int patLen   = pat.length();
                int endPos   = trimmed.length();
                int startPos = endPos - patLen;
                // Check boundary: must be at start or preceded by whitespace
                if (startPos == 0 || trimmed.at(startPos - 1).isSpace()) {
                    // Find this suffix in the original (untrimmed) string
                    // by locating the trimmed content within result.
                    int trimStart = 0;
                    while (trimStart < result.length() && result.at(trimStart).isSpace())
                        trimStart++;
                    int originalPos = trimStart + startPos;
                    result.replace(originalPos, patLen, QString::fromUtf8(e.emoji));
                    break; // only one replacement at the end
                }
            }
        }
    } else {
        // "Always" mode: replace all emoticons, but only when preceded by
        // whitespace or at the start of the string (boundary-safe).
        for (const auto &e : table) {
            QString pat   = QString::fromUtf8(e.pattern);
            QString emoji = QString::fromUtf8(e.emoji);
            int patLen    = pat.length();
            int pos       = 0;

            while (pos <= result.length() - patLen) {
                int found = result.indexOf(pat, pos, Qt::CaseInsensitive);
                if (found < 0)
                    break;
                // Boundary check: must be at start or preceded by whitespace
                if (found == 0 || result.at(found - 1).isSpace()) {
                    result.replace(found, patLen, emoji);
                    pos = found + emoji.length();
                } else {
                    pos = found + 1;
                }
            }
        }
    }

    return result;
}

void
InputBar::message(const QString &msg, MarkdownOverride useMarkdown, bool rainbowify)
{
    const QString body          = replaceTextEmoticons(msg);
    mtx::events::msg::Text text = {};
    text.body                   = body.trimmed().toStdString();

    if ((ChatPage::instance()->userSettings()->composerInputMarkdownToHtmlEnabled() &&
         useMarkdown == MarkdownOverride::NOT_SPECIFIED) ||
        useMarkdown == MarkdownOverride::ON) {
        text.formatted_body = utils::markdownToHtml(body, rainbowify).toStdString();
        // Remove markdown links by completer
        text.body = replaceMatrixToMarkdownLink(body.trimmed()).toStdString();

        // Don't send formatted_body, when we don't need to
        // Specifically, if it includes no html tag and no newlines or
        // backslashes (which behave differently in formatted bodies). Probably
        // we forgot something, so this might need to expand at some point.
        if (text.formatted_body.find('<') == std::string::npos &&
            text.body.find('\n') == std::string::npos && text.body.find('\\') == std::string::npos)
            text.formatted_body = "";
        else
            text.format = "org.matrix.custom.html";
    } else if (useMarkdown == MarkdownOverride::CMARK) {
        // disable all markdown extensions
        text.formatted_body = utils::markdownToHtml(body, rainbowify, true).toStdString();
        // keep everything as it was
        text.body = body.trimmed().toStdString();

        // always send formatted
        text.format = "org.matrix.custom.html";
    }

    text.mentions  = generateMentions();
    text.relations = generateRelations();

    room->sendMessageEvent(text, mtx::events::EventType::RoomMessage);
}

void
InputBar::emote(const QString &msg, bool rainbowify)
{
    const QString body = replaceTextEmoticons(msg);
    auto html          = utils::markdownToHtml(body, rainbowify);

    mtx::events::msg::Emote emote;
    emote.body = body.trimmed().toStdString();

    if (html != body.trimmed().toHtmlEscaped() &&
        ChatPage::instance()->userSettings()->composerInputMarkdownToHtmlEnabled()) {
        emote.formatted_body = html.toStdString();
        emote.format         = "org.matrix.custom.html";
        // Remove markdown links by completer
        emote.body = replaceMatrixToMarkdownLink(body.trimmed()).toStdString();
    }

    emote.mentions  = generateMentions();
    emote.relations = generateRelations();

    room->sendMessageEvent(emote, mtx::events::EventType::RoomMessage);
}

void
InputBar::notice(const QString &msg, bool rainbowify)
{
    const QString body = replaceTextEmoticons(msg);
    auto html          = utils::markdownToHtml(body, rainbowify);

    mtx::events::msg::Notice notice;
    notice.body = body.trimmed().toStdString();

    if (html != body.trimmed().toHtmlEscaped() &&
        ChatPage::instance()->userSettings()->composerInputMarkdownToHtmlEnabled()) {
        notice.formatted_body = html.toStdString();
        notice.format         = "org.matrix.custom.html";
        // Remove markdown links by completer
        notice.body = replaceMatrixToMarkdownLink(body.trimmed()).toStdString();
    }

    notice.mentions  = generateMentions();
    notice.relations = generateRelations();

    room->sendMessageEvent(notice, mtx::events::EventType::RoomMessage);
}

void
InputBar::confetti(const QString &body, bool rainbowify)
{
    const QString emoBody = replaceTextEmoticons(body);
    auto html             = utils::markdownToHtml(emoBody, rainbowify);

    mtx::events::msg::ElementEffect confetti;
    confetti.msgtype = "nic.custom.confetti";
    confetti.body    = emoBody.trimmed().toStdString();

    if (html != emoBody.trimmed().toHtmlEscaped() &&
        ChatPage::instance()->userSettings()->composerInputMarkdownToHtmlEnabled()) {
        confetti.formatted_body = html.toStdString();
        confetti.format         = "org.matrix.custom.html";
        // Remove markdown links by completer
        confetti.body = replaceMatrixToMarkdownLink(emoBody.trimmed()).toStdString();
    }

    confetti.mentions  = generateMentions();
    confetti.relations = generateRelations();

    room->sendMessageEvent(confetti, mtx::events::EventType::RoomMessage);
}

void
InputBar::rainfall(const QString &body)
{
    const QString emoBody = replaceTextEmoticons(body);
    auto html             = utils::markdownToHtml(emoBody);

    mtx::events::msg::Unknown rain;
    rain.msgtype = "io.element.effect.rainfall";
    rain.body    = emoBody.trimmed().toStdString();

    if (html != emoBody.trimmed().toHtmlEscaped() &&
        ChatPage::instance()->userSettings()->composerInputMarkdownToHtmlEnabled()) {
        nlohmann::json j;
        j["formatted_body"] = html.toStdString();
        j["format"]         = "org.matrix.custom.html";
        rain.content        = j.dump();
        // Remove markdown links by completer
        rain.body = replaceMatrixToMarkdownLink(emoBody.trimmed()).toStdString();
    }

    rain.mentions  = generateMentions();
    rain.relations = generateRelations();

    room->sendMessageEvent(rain, mtx::events::EventType::RoomMessage);
}

void
InputBar::customMsgtype(const QString &msgtype, const QString &body)
{
    const QString emoBody = replaceTextEmoticons(body);
    auto html             = utils::markdownToHtml(emoBody);

    mtx::events::msg::Unknown msg;
    msg.msgtype = msgtype.toStdString();
    msg.body    = emoBody.trimmed().toStdString();

    if (html != emoBody.trimmed().toHtmlEscaped() &&
        ChatPage::instance()->userSettings()->composerInputMarkdownToHtmlEnabled()) {
        nlohmann::json j;
        j["formatted_body"] = html.toStdString();
        j["format"]         = "org.matrix.custom.html";
        msg.content         = j.dump();
        // Remove markdown links by completer
        msg.body = replaceMatrixToMarkdownLink(emoBody.trimmed()).toStdString();
    }

    msg.mentions  = generateMentions();
    msg.relations = generateRelations();

    room->sendMessageEvent(msg, mtx::events::EventType::RoomMessage);
}

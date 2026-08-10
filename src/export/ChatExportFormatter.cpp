// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ChatExportFormatter.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSet>

#include "timeline/StateEventText.h"

namespace komai::chat_export {

// Translation-context shim; see the explanation in StateEventText.cpp.
class Tr
{
    Q_DECLARE_TR_FUNCTIONS(ChatExportFormatter)
};

namespace {

constexpr int kReplySnippetMaxChars = 60;

struct ReactionCount
{
    QString key;
    QStringList senderIds;
};

struct ReplyRef
{
    QString senderName;
    QString snippet;
};

bool
isStateKind(const QString &kind)
{
    return kind == QStringLiteral("membership_change") ||
           kind == QStringLiteral("profile_change") || kind == QStringLiteral("other_state");
}

bool
isMediaKind(const QString &kind)
{
    return kind == QStringLiteral("image") || kind == QStringLiteral("video") ||
           kind == QStringLiteral("audio") || kind == QStringLiteral("file") ||
           kind == QStringLiteral("sticker");
}

QString
senderText(const MatrixTimelineItem &item)
{
    if (item.senderId.isEmpty())
        return item.senderDisplayName;
    if (item.senderDisplayName.isEmpty() || item.senderDisplayName == item.senderId)
        return item.senderId;
    return QStringLiteral("%1 (%2)").arg(item.senderDisplayName, item.senderId);
}

QString
senderName(const MatrixTimelineItem &item)
{
    return item.senderDisplayName.isEmpty() ? item.senderId : item.senderDisplayName;
}

// Strip the legacy rich-reply fallback ("> <@user> quoted…\n\nreply") from a
// reply's plain body; the export renders its own quote line instead.
QString
effectiveBody(const MatrixTimelineItem &item)
{
    if (item.replyEventId.isEmpty() || !item.body.startsWith(QStringLiteral(">")))
        return item.body;

    const auto lines     = item.body.split(QLatin1Char('\n'));
    int firstContentLine = 0;
    while (firstContentLine < lines.size() &&
           lines.at(firstContentLine).startsWith(QStringLiteral(">")))
        ++firstContentLine;
    while (firstContentLine < lines.size() && lines.at(firstContentLine).trimmed().isEmpty())
        ++firstContentLine;

    if (firstContentLine >= lines.size())
        return item.body;
    return lines.mid(firstContentLine).join(QLatin1Char('\n'));
}

// Strip a leading <mx-reply>…</mx-reply> fallback block from a formatted
// body. The sanitizer keeps the quoted text inside, which would duplicate
// the export's own quote line.
QString
stripMxReply(const QString &formattedBody)
{
    if (!formattedBody.startsWith(QStringLiteral("<mx-reply>")))
        return formattedBody;
    const auto end = formattedBody.indexOf(QStringLiteral("</mx-reply>"));
    if (end < 0)
        return formattedBody;
    return formattedBody.mid(end + QStringLiteral("</mx-reply>").size());
}

QString
utdCauseSentence(const QString &cause)
{
    // Keep in sync with resources/qml/delegates/Encrypted.qml.
    if (cause == QStringLiteral("sent_before_we_joined"))
        return Tr::tr("you weren't in the room when this message was sent");
    if (cause == QStringLiteral("verification_violation"))
        return Tr::tr("the sender's identity is no longer verified");
    if (cause == QStringLiteral("unsigned_device"))
        return Tr::tr("the message was sent from a device that isn't signed by its owner");
    if (cause == QStringLiteral("unknown_device"))
        return Tr::tr("the message was sent from a device we couldn't securely identify");
    if (cause == QStringLiteral("historical_message_and_backup_disabled"))
        return Tr::tr("history isn't available on this device because key backup is off");
    if (cause == QStringLiteral("historical_message_and_device_unverified"))
        return Tr::tr("this device is not verified");
    if (cause == QStringLiteral("withheld_for_unverified_or_insecure_device"))
        return Tr::tr("the sender's security settings prevented sharing the encryption keys");
    if (cause == QStringLiteral("withheld_by_sender"))
        return Tr::tr("the sender didn't share the encryption keys with this device");
    return Tr::tr("the encryption keys are missing");
}

QString
mediaKindLabel(const MatrixTimelineItem &item)
{
    if (item.itemKind == QStringLiteral("image"))
        return Tr::tr("image");
    if (item.itemKind == QStringLiteral("video"))
        return Tr::tr("video");
    if (item.itemKind == QStringLiteral("audio"))
        return item.isVoiceMessage ? Tr::tr("voice message") : Tr::tr("audio");
    if (item.itemKind == QStringLiteral("sticker"))
        return Tr::tr("sticker");
    return Tr::tr("file");
}

QString
elidedSnippet(const QString &text)
{
    const auto firstLine = text.section(QLatin1Char('\n'), 0, 0).trimmed();
    if (firstLine.size() <= kReplySnippetMaxChars)
        return firstLine;
    return firstLine.left(kReplySnippetMaxChars).trimmed() + QStringLiteral("…");
}

bool
shouldRender(const MatrixChatExportEvent &event)
{
    const auto &item = event.item;
    if (item.eventId.isEmpty())
        return false;
    // Reactions and edits are folded into their target events.
    if (event.relationKind == QStringLiteral("annotation") ||
        event.relationKind == QStringLiteral("replacement"))
        return false;
    if (item.itemKind == QStringLiteral("reaction"))
        return false;
    // The redaction command event; its target already renders as deleted.
    if (item.itemKind == QStringLiteral("redacted") &&
        item.matrixEventType == QStringLiteral("m.room.redaction"))
        return false;
    // Non-user-visible events (call signaling, custom event types, …).
    if (item.itemKind == QStringLiteral("other_message"))
        return false;
    return true;
}

QString
formatReactions(const QVector<ReactionCount> &reactions)
{
    QStringList parts;
    parts.reserve(reactions.size());
    for (const auto &reaction : reactions) {
        parts.push_back(QStringLiteral("%1 %2 (%3)")
                          .arg(reaction.key,
                               QString::number(reaction.senderIds.size()),
                               reaction.senderIds.join(QStringLiteral(", "))));
    }
    return parts.join(QStringLiteral(", "));
}

QString
htmlEscape(const QString &text)
{
    return text.toHtmlEscaped();
}

QString
matrixToEventUrl(const QString &roomId, const QString &eventId)
{
    return QStringLiteral("https://matrix.to/#/%1/%2").arg(roomId, eventId);
}

// The message body for transcript purposes (before reply/reaction/edited
// decoration). Returns a null QString for state events (rendered via
// StateEventText) and an empty result for kinds with nothing to show.
QString
plainBodyForItem(const MatrixTimelineItem &item, int *utdCount)
{
    if (item.itemKind == QStringLiteral("unable_to_decrypt")) {
        ++(*utdCount);
        return Tr::tr("[Unable to decrypt: %1]").arg(utdCauseSentence(item.utdCause));
    }
    if (item.itemKind == QStringLiteral("redacted"))
        return Tr::tr("[Message deleted]");
    if (isMediaKind(item.itemKind)) {
        const auto label = item.fileName.isEmpty() ? item.body : item.fileName;
        auto line        = QStringLiteral("[%1: %2]").arg(mediaKindLabel(item), label);
        if (!item.mediaUrl.isEmpty())
            line += QStringLiteral(" (%1)").arg(item.mediaUrl);
        // Attachments can carry a caption in `body` distinct from the filename.
        if (!item.body.isEmpty() && item.body != label)
            line += QLatin1Char('\n') + item.body;
        return line;
    }
    return effectiveBody(item);
}

QString
dayHeading(const QDate &date)
{
    return QLocale::system().toString(date, QLocale::LongFormat);
}

QString
timestampText(const MatrixTimelineItem &item)
{
    const auto dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(item.timestamp));
    return QLocale::system().toString(dt, QLocale::ShortFormat);
}

// ── Plain-text rendering ────────────────────────────────────────────

void
renderPlainTextItem(QString &out,
                    const MatrixTimelineItem &item,
                    const QString &body,
                    const QHash<QString, ReplyRef> &replyIndex,
                    const QVector<ReactionCount> &reactions)
{
    const auto header = QStringLiteral("[%1] ").arg(timestampText(item));

    if (isStateKind(item.itemKind)) {
        const auto sentence = StateEventText::translate(item);
        if (sentence.isEmpty())
            return;
        out += header + sentence + QLatin1Char('\n');
        return;
    }

    QString quoteLine;
    if (!item.replyEventId.isEmpty()) {
        const auto ref = replyIndex.value(item.replyEventId);
        if (!ref.senderName.isEmpty()) {
            quoteLine = Tr::tr("> in reply to %1: \"%2\"").arg(ref.senderName, ref.snippet);
        } else {
            quoteLine = Tr::tr("> in reply to an earlier message");
        }
    }

    auto bodyText = body;
    if (item.isEdited)
        bodyText += QLatin1Char(' ') + Tr::tr("(edited)");

    QString reactionsLine;
    if (!reactions.isEmpty())
        reactionsLine = Tr::tr("[reactions: %1]").arg(formatReactions(reactions));

    if (item.itemKind == QStringLiteral("emote")) {
        out +=
          header + QStringLiteral("* %1 %2").arg(senderName(item), bodyText) + QLatin1Char('\n');
        if (!reactionsLine.isEmpty())
            out += QStringLiteral("    ") + reactionsLine + QLatin1Char('\n');
        return;
    }

    const bool multiLine = !quoteLine.isEmpty() || bodyText.contains(QLatin1Char('\n'));
    if (!multiLine) {
        out += header + senderText(item) + QStringLiteral(": ") + bodyText + QLatin1Char('\n');
    } else {
        out += header + senderText(item) + QStringLiteral(":") + QLatin1Char('\n');
        if (!quoteLine.isEmpty())
            out += QStringLiteral("    ") + quoteLine + QLatin1Char('\n');
        const auto lines = bodyText.split(QLatin1Char('\n'));
        for (const auto &line : lines)
            out += QStringLiteral("    ") + line + QLatin1Char('\n');
    }
    if (!reactionsLine.isEmpty())
        out += QStringLiteral("    ") + reactionsLine + QLatin1Char('\n');
}

// ── JSON Lines rendering ────────────────────────────────────────────
//
// Machine-readable output: no translation, no locale-dependent dates.
// Timestamps are ISO 8601 UTC; kind and cause tags are the stable
// snake_case identifiers from the Rust summarizer.

QString
isoUtcTimestamp(uint64_t timestampMs)
{
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(timestampMs))
      .toUTC()
      .toString(Qt::ISODateWithMs);
}

void
renderJsonLinesItem(QString &out,
                    const MatrixChatExportEvent &event,
                    const QString &body,
                    const QVector<ReactionCount> &reactions,
                    int *utdCount)
{
    const auto &item = event.item;

    QJsonObject obj;
    obj[QStringLiteral("event_id")]         = item.eventId;
    obj[QStringLiteral("origin_server_ts")] = static_cast<qint64>(item.timestamp);
    obj[QStringLiteral("timestamp")]        = isoUtcTimestamp(item.timestamp);
    obj[QStringLiteral("sender_id")]        = item.senderId;
    if (!item.senderDisplayName.isEmpty() && item.senderDisplayName != item.senderId)
        obj[QStringLiteral("sender_display_name")] = item.senderDisplayName;

    if (isStateKind(item.itemKind)) {
        obj[QStringLiteral("type")]       = QStringLiteral("state");
        obj[QStringLiteral("kind")]       = item.itemKind;
        obj[QStringLiteral("event_type")] = item.matrixEventType;
        if (!item.membershipChangeKind.isEmpty())
            obj[QStringLiteral("change")] = item.membershipChangeKind;
        if (!item.stateEventTargetUserId.isEmpty())
            obj[QStringLiteral("target_id")] = item.stateEventTargetUserId;
        if (!item.stateEventTargetUser.isEmpty() &&
            item.stateEventTargetUser != item.stateEventTargetUserId)
            obj[QStringLiteral("target_display_name")] = item.stateEventTargetUser;
        if (!item.stateEventDetail.isEmpty())
            obj[QStringLiteral("detail")] = item.stateEventDetail;
        if (!item.stateEventReason.isEmpty())
            obj[QStringLiteral("reason")] = item.stateEventReason;
        if (!item.tombstoneReplacementRoomId.isEmpty())
            obj[QStringLiteral("replacement_room_id")] = item.tombstoneReplacementRoomId;
    } else {
        obj[QStringLiteral("type")] = QStringLiteral("message");
        obj[QStringLiteral("kind")] = item.itemKind;
        if (item.itemKind == QStringLiteral("unable_to_decrypt")) {
            ++(*utdCount);
            obj[QStringLiteral("utd_cause")] =
              item.utdCause.isEmpty() ? QStringLiteral("unknown") : item.utdCause;
        } else if (item.itemKind == QStringLiteral("redacted")) {
            obj[QStringLiteral("redacted")] = true;
        } else {
            if (!body.isEmpty())
                obj[QStringLiteral("body")] = body;
            if (!item.formattedBody.isEmpty())
                obj[QStringLiteral("formatted_body")] = item.formattedBody;
        }
        if (isMediaKind(item.itemKind)) {
            QJsonObject media;
            if (!item.fileName.isEmpty())
                media[QStringLiteral("file_name")] = item.fileName;
            if (!item.mediaUrl.isEmpty())
                media[QStringLiteral("mxc_url")] = item.mediaUrl;
            if (!item.mimeType.isEmpty())
                media[QStringLiteral("mime_type")] = item.mimeType;
            if (item.mediaSizeBytes > 0)
                media[QStringLiteral("size_bytes")] = static_cast<qint64>(item.mediaSizeBytes);
            obj[QStringLiteral("media")] = media;
        }
        if (item.isEdited)
            obj[QStringLiteral("edited")] = true;
        if (!item.replyEventId.isEmpty())
            obj[QStringLiteral("reply_to_event_id")] = item.replyEventId;
        if (!item.threadId.isEmpty())
            obj[QStringLiteral("thread_root_event_id")] = item.threadId;
        if (!reactions.isEmpty()) {
            QJsonArray reactionArray;
            for (const auto &reaction : reactions) {
                QJsonObject entry;
                entry[QStringLiteral("key")]   = reaction.key;
                entry[QStringLiteral("count")] = reaction.senderIds.size();
                entry[QStringLiteral("sender_ids")] =
                  QJsonArray::fromStringList(reaction.senderIds);
                reactionArray.push_back(entry);
            }
            obj[QStringLiteral("reactions")] = reactionArray;
        }
    }

    out += QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)) + QLatin1Char('\n');
}

// ── HTML rendering ──────────────────────────────────────────────────

QString
htmlDocumentCss()
{
    return QStringLiteral(R"(
    body {
      margin: 0 auto;
      padding: 1.5rem 1rem 3rem;
      max-width: 46rem;
      font-family: system-ui, sans-serif;
      line-height: 1.45;
      color: #1a1a1a;
      background: #ffffff;
    }
    header { border-bottom: 1px solid #d0d0d0; padding-bottom: 0.75rem; }
    header h1 { margin: 0 0 0.25rem; font-size: 1.4rem; }
    header p { margin: 0; color: #666666; font-size: 0.9rem; }
    section.day h2 {
      margin: 1.5rem 0 0.5rem;
      font-size: 0.95rem;
      color: #666666;
      border-bottom: 1px solid #e5e5e5;
      padding-bottom: 0.25rem;
    }
    article.msg { margin: 0.6rem 0; }
    .meta { font-size: 0.85rem; color: #666666; }
    .meta .sender { color: #1a1a1a; font-size: 0.95rem; }
    .meta time { float: right; }
    .body { overflow-wrap: break-word; }
    .body pre { overflow-x: auto; background: #f5f5f5; padding: 0.5rem; }
    .body blockquote, blockquote.reply {
      margin: 0.25rem 0;
      padding-left: 0.75rem;
      border-left: 3px solid #d0d0d0;
      color: #555555;
    }
    blockquote.reply { font-size: 0.85rem; }
    .reactions { font-size: 0.85rem; color: #666666; margin-top: 0.15rem; }
    p.state { color: #666666; font-size: 0.85rem; margin: 0.4rem 0; }
    .edited { color: #666666; font-size: 0.85rem; }
    .utd { color: #8a6d3b; }
    a { color: #1667c2; }
)");
}

void
renderHtmlItem(QString &out,
               const MatrixChatExportEvent &event,
               const QString &body,
               const RenderInput &input,
               const QHash<QString, ReplyRef> &replyIndex,
               const QVector<ReactionCount> &reactions,
               int *utdCount)
{
    const auto &item = event.item;

    if (isStateKind(item.itemKind)) {
        const auto sentence = StateEventText::translate(item);
        if (sentence.isEmpty())
            return;
        out += QStringLiteral(
                 "  <p class=\"state\">%1 <span class=\"meta\"><time>%2</time></span></p>\n")
                 .arg(htmlEscape(sentence), htmlEscape(timestampText(item)));
        return;
    }

    out += QStringLiteral("  <article class=\"msg\" id=\"%1\">\n").arg(htmlEscape(item.eventId));
    out += QStringLiteral(
             "    <div class=\"meta\"><b class=\"sender\">%1</b> %2 <time>%3</time></div>\n")
             .arg(htmlEscape(senderName(item)),
                  htmlEscape(item.senderId),
                  htmlEscape(timestampText(item)));

    if (!item.replyEventId.isEmpty()) {
        const auto ref   = replyIndex.value(item.replyEventId);
        const auto quote = !ref.senderName.isEmpty()
                             ? Tr::tr("in reply to %1: \"%2\"").arg(ref.senderName, ref.snippet)
                             : Tr::tr("in reply to an earlier message");
        out += QStringLiteral("    <blockquote class=\"reply\">%1</blockquote>\n")
                 .arg(htmlEscape(quote));
    }

    QString bodyHtml;
    if (item.itemKind == QStringLiteral("unable_to_decrypt")) {
        ++(*utdCount);
        bodyHtml = QStringLiteral("<span class=\"utd\">%1</span>")
                     .arg(htmlEscape(
                       Tr::tr("[Unable to decrypt: %1]").arg(utdCauseSentence(item.utdCause))));
    } else if (item.itemKind == QStringLiteral("redacted")) {
        bodyHtml = htmlEscape(Tr::tr("[Message deleted]"));
    } else if (isMediaKind(item.itemKind)) {
        const auto label = item.fileName.isEmpty() ? item.body : item.fileName;
        // Raw homeserver download URLs need authentication, so link the
        // event permalink instead and keep the mxc URI as hover text.
        bodyHtml = QStringLiteral("[%1: <a href=\"%2\" title=\"%3\">%4</a>]")
                     .arg(htmlEscape(mediaKindLabel(item)),
                          htmlEscape(matrixToEventUrl(input.roomId, item.eventId)),
                          htmlEscape(item.mediaUrl),
                          htmlEscape(label));
        if (!item.body.isEmpty() && item.body != label)
            bodyHtml += QStringLiteral("<br>") + htmlEscape(item.body);
    } else {
        const auto formatted = stripMxReply(item.formattedBody);
        if (!formatted.isEmpty() && input.htmlBodyPipeline) {
            bodyHtml = input.htmlBodyPipeline(formatted);
        } else {
            bodyHtml = htmlEscape(body);
            bodyHtml.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
        }
        if (item.itemKind == QStringLiteral("emote"))
            bodyHtml = QStringLiteral("* %1 %2").arg(htmlEscape(senderName(item)), bodyHtml);
    }

    if (item.isEdited)
        bodyHtml +=
          QStringLiteral(" <span class=\"edited\">%1</span>").arg(htmlEscape(Tr::tr("(edited)")));

    out += QStringLiteral("    <div class=\"body\">%1</div>\n").arg(bodyHtml);

    if (!reactions.isEmpty()) {
        QStringList parts;
        parts.reserve(reactions.size());
        for (const auto &reaction : reactions) {
            parts.push_back(QStringLiteral("%1 %2 (%3)")
                              .arg(htmlEscape(reaction.key),
                                   QString::number(reaction.senderIds.size()),
                                   htmlEscape(reaction.senderIds.join(QStringLiteral(", ")))));
        }
        out += QStringLiteral("    <div class=\"reactions\">%1</div>\n")
                 .arg(parts.join(QStringLiteral(" · ")));
    }

    out += QStringLiteral("  </article>\n");
}

} // namespace

RenderResult
render(const QList<MatrixChatExportEvent> &eventsNewestFirst,
       const RenderInput &input,
       Format format)
{
    // Pass 1 (newest → oldest): dedup across pagination-token boundaries,
    // aggregate reactions onto their targets, index reply snippets, and
    // keep the events that render.
    QSet<QString> seenEventIds;
    QHash<QString, QVector<ReactionCount>> reactionsByTarget;
    QHash<QString, ReplyRef> replyIndex;
    QList<const MatrixChatExportEvent *> renderable;
    renderable.reserve(eventsNewestFirst.size());

    for (const auto &event : eventsNewestFirst) {
        const auto &item = event.item;
        if (!item.eventId.isEmpty()) {
            if (seenEventIds.contains(item.eventId))
                continue;
            seenEventIds.insert(item.eventId);
        }

        if (event.relationKind == QStringLiteral("annotation") &&
            !event.relatesToEventId.isEmpty() && !event.annotationKey.isEmpty()) {
            auto &reactions = reactionsByTarget[event.relatesToEventId];
            auto it = std::find_if(reactions.begin(), reactions.end(), [&](const ReactionCount &r) {
                return r.key == event.annotationKey;
            });
            if (it == reactions.end()) {
                reactions.push_back(ReactionCount{event.annotationKey, {item.senderId}});
            } else if (!it->senderIds.contains(item.senderId)) {
                it->senderIds.push_back(item.senderId);
            }
        }

        if (!item.eventId.isEmpty() && !isStateKind(item.itemKind)) {
            replyIndex.insert(item.eventId,
                              ReplyRef{senderName(item), elidedSnippet(effectiveBody(item))});
        }

        if (shouldRender(event))
            renderable.push_back(&event);
    }

    // Pass 2 (oldest → newest): render chronologically, with day headings
    // for the human-readable formats.
    RenderResult result;
    QString out;
    QDate currentDay;
    const bool html  = format == Format::Html;
    const bool jsonl = format == Format::JsonLines;

    for (auto it = renderable.crbegin(); it != renderable.crend(); ++it) {
        const auto &event = **it;
        const auto &item  = event.item;

        const auto day = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(item.timestamp)).date();
        if (!jsonl && day != currentDay) {
            if (html) {
                if (currentDay.isValid())
                    out += QStringLiteral("</section>\n");
                out += QStringLiteral("<section class=\"day\">\n  <h2>%1</h2>\n")
                         .arg(htmlEscape(dayHeading(day)));
            } else {
                if (currentDay.isValid())
                    out += QLatin1Char('\n');
                out += QStringLiteral("----- %1 -----\n\n").arg(dayHeading(day));
            }
            currentDay = day;
        }

        const auto sizeBefore = out.size();
        const auto reactions  = reactionsByTarget.value(item.eventId);
        if (jsonl) {
            renderJsonLinesItem(out, event, effectiveBody(item), reactions, &result.utdCount);
        } else if (html) {
            renderHtmlItem(
              out, event, effectiveBody(item), input, replyIndex, reactions, &result.utdCount);
        } else {
            const auto body = plainBodyForItem(item, &result.utdCount);
            renderPlainTextItem(out, item, body, replyIndex, reactions);
        }
        if (out.size() != sizeBefore)
            ++result.messageCount;
    }

    if (html && currentDay.isValid())
        out += QStringLiteral("</section>\n");

    // Assemble the final document around the rendered body.
    const auto exportedOn = QLocale::system().toString(input.exportedAt, QLocale::ShortFormat);
    // Rooms without an m.room.name state event (e.g. direct chats) have no
    // canonical name; fall back to the room id for the human-readable titles.
    const auto displayRoomName = input.roomName.isEmpty() ? input.roomId : input.roomName;
    if (jsonl) {
        QJsonObject header;
        header[QStringLiteral("type")]    = QStringLiteral("export_info");
        header[QStringLiteral("format")]  = QStringLiteral("komai-chat-export");
        header[QStringLiteral("version")] = 1;
        header[QStringLiteral("room_id")] = input.roomId;
        if (!input.roomName.isEmpty())
            header[QStringLiteral("room_name")] = input.roomName;
        header[QStringLiteral("exported_by")] = input.exportingUserId;
        header[QStringLiteral("exported_at")] =
          input.exportedAt.toUTC().toString(Qt::ISODateWithMs);
        header[QStringLiteral("message_count")]       = result.messageCount;
        header[QStringLiteral("undecryptable_count")] = result.utdCount;
        result.document =
          input.includeMetadata
            ? QString::fromUtf8(QJsonDocument(header).toJson(QJsonDocument::Compact)) +
                QLatin1Char('\n') + out
            : out;
    } else if (html) {
        QString doc;
        doc += QStringLiteral("<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n");
        doc += QStringLiteral(
          "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n");
        doc += QStringLiteral("<title>%1</title>\n")
                 .arg(htmlEscape(Tr::tr("%1 - chat export").arg(displayRoomName)));
        doc += QStringLiteral("<style>%1</style>\n</head>\n<body>\n").arg(htmlDocumentCss());
        if (input.includeMetadata) {
            doc += QStringLiteral("<header>\n  <h1>%1</h1>\n  <p>%2<br>%3</p>\n</header>\n")
                     .arg(htmlEscape(displayRoomName),
                          htmlEscape(input.roomId),
                          htmlEscape(Tr::tr("Exported by %1 on %2 with Komai. %n message(s).",
                                            nullptr,
                                            result.messageCount)
                                       .arg(input.exportingUserId, exportedOn)));
        }
        doc += out;
        doc += QStringLiteral("</body>\n</html>\n");
        result.document = doc;
    } else {
        QString doc;
        if (input.includeMetadata) {
            doc +=
              Tr::tr("Chat export: %1 (%2)").arg(displayRoomName, input.roomId) + QLatin1Char('\n');
            doc +=
              Tr::tr("Exported by %1 on %2 with Komai.").arg(input.exportingUserId, exportedOn) +
              QLatin1Char('\n');
            doc += QString(60, QLatin1Char('-')) + QLatin1Char('\n');
        }
        doc += out;
        result.document = doc;
    }

    return result;
}

} // namespace komai::chat_export

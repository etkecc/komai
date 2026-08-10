// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string_view>

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "export/ChatExportFormatter.h"

namespace {

using komai::MatrixChatExportEvent;
using komai::MatrixTimelineItem;
using komai::chat_export::Format;
using komai::chat_export::RenderInput;
using komai::chat_export::render;

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

// Timestamps count up from a fixed local morning so day-boundary tests are
// deterministic in any timezone.
qint64
ts(int day, int minute)
{
    const QDateTime base(QDate(2026, 8, day), QTime(10, 0));
    return base.addSecs(minute * 60).toMSecsSinceEpoch();
}

MatrixChatExportEvent
message(const QString &eventId,
        const QString &senderId,
        const QString &senderName,
        const QString &body,
        qint64 timestamp)
{
    MatrixChatExportEvent event;
    event.item.eventId           = eventId;
    event.item.senderId          = senderId;
    event.item.senderDisplayName = senderName;
    event.item.body              = body;
    event.item.itemKind          = QStringLiteral("text");
    event.item.matrixEventType   = QStringLiteral("m.room.message");
    event.item.timestamp         = static_cast<uint64_t>(timestamp);
    return event;
}

MatrixChatExportEvent
reaction(const QString &eventId,
         const QString &senderId,
         const QString &targetEventId,
         const QString &key,
         qint64 timestamp)
{
    auto event                 = message(eventId, senderId, QString(), QString(), timestamp);
    event.item.itemKind        = QStringLiteral("reaction");
    event.item.matrixEventType = QStringLiteral("m.reaction");
    event.relationKind         = QStringLiteral("annotation");
    event.relatesToEventId     = targetEventId;
    event.annotationKey        = key;
    return event;
}

RenderInput
input()
{
    RenderInput in;
    in.roomName        = QStringLiteral("Test Room");
    in.roomId          = QStringLiteral("!room:example.com");
    in.exportingUserId = QStringLiteral("@me:example.com");
    in.exportedAt      = QDateTime::fromMSecsSinceEpoch(ts(9, 0));
    return in;
}

bool
testChronologicalOrderAndHeader()
{
    // Walk order is newest first; output must be oldest first.
    const QList<MatrixChatExportEvent> events = {
        message(QStringLiteral("$2"),
                QStringLiteral("@bob:example.com"),
                QStringLiteral("Bob"),
                QStringLiteral("second"),
                ts(7, 1)),
        message(QStringLiteral("$1"),
                QStringLiteral("@alice:example.com"),
                QStringLiteral("Alice"),
                QStringLiteral("first"),
                ts(7, 0)),
    };

    const auto result = render(events, input(), Format::PlainText);
    bool ok           = true;
    ok &= expect(result.messageCount == 2, "two messages rendered");
    ok &= expect(result.document.indexOf(QStringLiteral("first")) <
                   result.document.indexOf(QStringLiteral("second")),
                 "output is chronological");
    ok &= expect(result.document.contains(QStringLiteral("Alice (@alice:example.com): first")),
                 "header shows display name and mxid");
    ok &= expect(result.document.contains(QStringLiteral("Test Room")), "file header has room name");
    return ok;
}

bool
testDeduplicatesAcrossBatches()
{
    const auto original = message(QStringLiteral("$1"),
                                  QStringLiteral("@alice:example.com"),
                                  QStringLiteral("Alice"),
                                  QStringLiteral("hello"),
                                  ts(7, 0));
    const QList<MatrixChatExportEvent> events = { original, original };

    const auto result = render(events, input(), Format::PlainText);
    return expect(result.messageCount == 1, "duplicate event ids are dropped");
}

bool
testReactionAggregationWithSenders()
{
    const QList<MatrixChatExportEvent> events = {
        reaction(QStringLiteral("$r2"),
                 QStringLiteral("@bob:example.com"),
                 QStringLiteral("$1"),
                 QStringLiteral("👍"),
                 ts(7, 2)),
        reaction(QStringLiteral("$r1"),
                 QStringLiteral("@carol:example.com"),
                 QStringLiteral("$1"),
                 QStringLiteral("👍"),
                 ts(7, 1)),
        message(QStringLiteral("$1"),
                QStringLiteral("@alice:example.com"),
                QStringLiteral("Alice"),
                QStringLiteral("hello"),
                ts(7, 0)),
    };

    const auto result = render(events, input(), Format::PlainText);
    bool ok           = true;
    ok &= expect(result.messageCount == 1, "reaction events are not rendered standalone");
    ok &= expect(result.document.contains(
                   QStringLiteral("👍 2 (@bob:example.com, @carol:example.com)")),
                 "reaction line aggregates count and sender mxids");
    return ok;
}

bool
testEditedMarker()
{
    auto edited          = message(QStringLiteral("$1"),
                          QStringLiteral("@alice:example.com"),
                          QStringLiteral("Alice"),
                          QStringLiteral("edited body"),
                          ts(7, 0));
    edited.item.isEdited = true;

    // The standalone edit event must be dropped.
    auto editEvent          = message(QStringLiteral("$2"),
                             QStringLiteral("@alice:example.com"),
                             QStringLiteral("Alice"),
                             QStringLiteral(" * edited body"),
                             ts(7, 1));
    editEvent.relationKind     = QStringLiteral("replacement");
    editEvent.relatesToEventId = QStringLiteral("$1");

    const QList<MatrixChatExportEvent> events = { editEvent, edited };

    const auto result = render(events, input(), Format::PlainText);
    bool ok           = true;
    ok &= expect(result.messageCount == 1, "edit event folded into target");
    ok &= expect(result.document.contains(QStringLiteral("edited body (edited)")),
                 "edited marker appended");
    return ok;
}

bool
testReplyQuote()
{
    auto reply = message(QStringLiteral("$2"),
                         QStringLiteral("@bob:example.com"),
                         QStringLiteral("Bob"),
                         QStringLiteral("> <@alice:example.com> hello there\n\nhi Alice"),
                         ts(7, 1));
    reply.item.replyEventId = QStringLiteral("$1");

    const QList<MatrixChatExportEvent> events = {
        reply,
        message(QStringLiteral("$1"),
                QStringLiteral("@alice:example.com"),
                QStringLiteral("Alice"),
                QStringLiteral("hello there"),
                ts(7, 0)),
    };

    const auto result = render(events, input(), Format::PlainText);
    bool ok           = true;
    ok &= expect(result.document.contains(QStringLiteral("> in reply to Alice: \"hello there\"")),
                 "reply quote references target");
    ok &= expect(result.document.contains(QStringLiteral("hi Alice")), "reply body kept");
    ok &= expect(!result.document.contains(QStringLiteral("<@alice:example.com>")),
                 "legacy reply fallback stripped");
    return ok;
}

bool
testUtdNeverDropped()
{
    auto utd              = message(QStringLiteral("$1"),
                       QStringLiteral("@alice:example.com"),
                       QStringLiteral("Alice"),
                       QStringLiteral("[Unable to decrypt message]"),
                       ts(7, 0));
    utd.item.itemKind     = QStringLiteral("unable_to_decrypt");
    utd.item.utdCause     = QStringLiteral("sent_before_we_joined");

    const auto result = render({ utd }, input(), Format::PlainText);
    bool ok           = true;
    ok &= expect(result.utdCount == 1, "utd counted");
    ok &= expect(result.document.contains(QStringLiteral("[Unable to decrypt:")),
                 "utd rendered with cause");
    return ok;
}

bool
testRedactionCommandDroppedTargetRendered()
{
    auto redactionCommand              = message(QStringLiteral("$2"),
                                    QStringLiteral("@mod:example.com"),
                                    QStringLiteral("Mod"),
                                    QStringLiteral("Deleted message"),
                                    ts(7, 1));
    redactionCommand.item.itemKind        = QStringLiteral("redacted");
    redactionCommand.item.matrixEventType = QStringLiteral("m.room.redaction");

    auto redactedTarget              = message(QStringLiteral("$1"),
                                  QStringLiteral("@alice:example.com"),
                                  QStringLiteral("Alice"),
                                  QStringLiteral("Deleted message"),
                                  ts(7, 0));
    redactedTarget.item.itemKind = QStringLiteral("redacted");

    const auto result = render({ redactionCommand, redactedTarget }, input(), Format::PlainText);
    bool ok           = true;
    ok &= expect(result.messageCount == 1, "redaction command dropped, target kept");
    ok &= expect(result.document.contains(QStringLiteral("[Message deleted]")),
                 "redacted target rendered as deleted");
    return ok;
}

bool
testMediaAsLink()
{
    auto media              = message(QStringLiteral("$1"),
                         QStringLiteral("@alice:example.com"),
                         QStringLiteral("Alice"),
                         QStringLiteral("cat.jpg"),
                         ts(7, 0));
    media.item.itemKind = QStringLiteral("image");
    media.item.fileName = QStringLiteral("cat.jpg");
    media.item.mediaUrl = QStringLiteral("mxc://example.com/abc");

    const auto result = render({ media }, input(), Format::PlainText);
    return expect(result.document.contains(QStringLiteral("cat.jpg]")) &&
                    result.document.contains(QStringLiteral("(mxc://example.com/abc)")),
                  "media rendered as filename plus mxc uri");
}

bool
testDaySeparators()
{
    const QList<MatrixChatExportEvent> events = {
        message(QStringLiteral("$2"),
                QStringLiteral("@alice:example.com"),
                QStringLiteral("Alice"),
                QStringLiteral("day two"),
                ts(8, 0)),
        message(QStringLiteral("$1"),
                QStringLiteral("@alice:example.com"),
                QStringLiteral("Alice"),
                QStringLiteral("day one"),
                ts(7, 0)),
    };

    const auto result = render(events, input(), Format::PlainText);
    return expect(result.document.count(QStringLiteral("----- ")) == 2,
                  "one separator per calendar day");
}

bool
testHtmlEscapingAndStructure()
{
    auto item = message(QStringLiteral("$1"),
                        QStringLiteral("@alice:example.com"),
                        QStringLiteral("Alice <script>"),
                        QStringLiteral("2 < 3 & 4 > 1"),
                        ts(7, 0));

    const auto result = render({ item }, input(), Format::Html);
    bool ok           = true;
    ok &= expect(result.document.contains(QStringLiteral("<article class=\"msg\" id=\"$1\">")),
                 "html article per message");
    ok &= expect(result.document.contains(QStringLiteral("2 &lt; 3 &amp; 4 &gt; 1")),
                 "plain body html-escaped");
    ok &= expect(!result.document.contains(QStringLiteral("<script>")),
                 "sender name html-escaped");
    ok &= expect(result.document.startsWith(QStringLiteral("<!DOCTYPE html>")),
                 "self-contained html document");
    return ok;
}

bool
testHtmlUsesPipelineForFormattedBody()
{
    auto item               = message(QStringLiteral("$1"),
                        QStringLiteral("@alice:example.com"),
                        QStringLiteral("Alice"),
                        QStringLiteral("bold"),
                        ts(7, 0));
    item.item.formattedBody = QStringLiteral("<strong>bold</strong>");

    auto in            = input();
    bool pipelineCalled = false;
    in.htmlBodyPipeline = [&pipelineCalled](const QString &html) {
        pipelineCalled = true;
        return html;
    };

    const auto result = render({ item }, in, Format::Html);
    bool ok           = true;
    ok &= expect(pipelineCalled, "formatted body routed through pipeline");
    ok &= expect(result.document.contains(QStringLiteral("<strong>bold</strong>")),
                 "pipeline output embedded");
    return ok;
}

bool
testJsonLinesStructure()
{
    auto utd          = message(QStringLiteral("$utd"),
                       QStringLiteral("@carol:example.com"),
                       QStringLiteral("Carol"),
                       QString(),
                       ts(7, 3));
    utd.item.itemKind = QStringLiteral("unable_to_decrypt");
    utd.item.utdCause = QStringLiteral("sent_before_we_joined");

    auto join                            = message(QStringLiteral("$join"),
                        QStringLiteral("@bob:example.com"),
                        QStringLiteral("Bob"),
                        QString(),
                        ts(7, 2));
    join.item.itemKind                   = QStringLiteral("membership_change");
    join.item.matrixEventType            = QStringLiteral("m.room.member");
    join.item.membershipChangeKind       = QStringLiteral("joined");
    join.item.stateEventTargetUser       = QStringLiteral("Bob");
    join.item.stateEventTargetUserId     = QStringLiteral("@bob:example.com");

    const QList<MatrixChatExportEvent> events = {
        utd,
        join,
        reaction(QStringLiteral("$r1"),
                 QStringLiteral("@bob:example.com"),
                 QStringLiteral("$1"),
                 QStringLiteral("👍"),
                 ts(7, 1)),
        message(QStringLiteral("$1"),
                QStringLiteral("@alice:example.com"),
                QStringLiteral("Alice"),
                QStringLiteral("hello"),
                ts(7, 0)),
    };

    const auto result = render(events, input(), Format::JsonLines);
    const auto lines  = result.document.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    bool ok = true;
    ok &= expect(lines.size() == 4, "header plus three rendered lines");

    for (const auto &line : lines) {
        QJsonParseError parseError;
        QJsonDocument::fromJson(line.toUtf8(), &parseError);
        ok &= expect(parseError.error == QJsonParseError::NoError, "every line is valid JSON");
    }

    const auto header = QJsonDocument::fromJson(lines.value(0).toUtf8()).object();
    ok &= expect(header[QStringLiteral("type")].toString() == QStringLiteral("export_info"),
                 "first line is the export_info header");
    ok &= expect(header[QStringLiteral("message_count")].toInt() == 3, "header carries counts");
    ok &= expect(header[QStringLiteral("undecryptable_count")].toInt() == 1,
                 "header carries utd count");

    const auto first = QJsonDocument::fromJson(lines.value(1).toUtf8()).object();
    ok &= expect(first[QStringLiteral("type")].toString() == QStringLiteral("message"),
                 "oldest event first after header");
    ok &= expect(first[QStringLiteral("body")].toString() == QStringLiteral("hello"),
                 "message body present");
    ok &= expect(first[QStringLiteral("timestamp")].toString().endsWith(QStringLiteral("Z")),
                 "timestamps are ISO 8601 UTC");
    const auto reactions = first[QStringLiteral("reactions")].toArray();
    ok &= expect(reactions.size() == 1 &&
                   reactions.first().toObject()[QStringLiteral("count")].toInt() == 1,
                 "reactions aggregated onto the target");

    const auto state = QJsonDocument::fromJson(lines.value(2).toUtf8()).object();
    ok &= expect(state[QStringLiteral("type")].toString() == QStringLiteral("state") &&
                   state[QStringLiteral("change")].toString() == QStringLiteral("joined"),
                 "state event carries structured change tag");

    const auto undecryptable = QJsonDocument::fromJson(lines.value(3).toUtf8()).object();
    ok &= expect(undecryptable[QStringLiteral("utd_cause")].toString() ==
                   QStringLiteral("sent_before_we_joined"),
                 "utd cause is the stable machine tag");

    ok &= expect(!result.document.contains(QStringLiteral("-----")),
                 "no day separators in jsonl");
    return ok;
}

bool
testMetadataToggle()
{
    const QList<MatrixChatExportEvent> events = {
        message(QStringLiteral("$1"),
                QStringLiteral("@alice:example.com"),
                QStringLiteral("Alice"),
                QStringLiteral("hello"),
                ts(7, 0)),
    };

    auto in            = input();
    in.includeMetadata = false;

    bool ok = true;

    const auto text = render(events, in, Format::PlainText);
    ok &= expect(!text.document.contains(QStringLiteral("Chat export:")),
                 "txt has no header when metadata is off");
    ok &= expect(text.document.contains(QStringLiteral("hello")), "txt content intact");

    const auto jsonl      = render(events, in, Format::JsonLines);
    const auto firstLine  = jsonl.document.section(QLatin1Char('\n'), 0, 0);
    const auto firstEvent = QJsonDocument::fromJson(firstLine.toUtf8()).object();
    ok &= expect(firstEvent[QStringLiteral("type")].toString() == QStringLiteral("message"),
                 "jsonl starts with the first event when metadata is off");

    const auto html = render(events, in, Format::Html);
    ok &= expect(!html.document.contains(QStringLiteral("<header>")),
                 "html has no header block when metadata is off");
    ok &= expect(html.document.startsWith(QStringLiteral("<!DOCTYPE html>")),
                 "html is still a complete document");
    return ok;
}

} // namespace

int
main()
{
    bool ok = true;
    ok &= testChronologicalOrderAndHeader();
    ok &= testDeduplicatesAcrossBatches();
    ok &= testReactionAggregationWithSenders();
    ok &= testEditedMarker();
    ok &= testReplyQuote();
    ok &= testUtdNeverDropped();
    ok &= testRedactionCommandDroppedTargetRendered();
    ok &= testMediaAsLink();
    ok &= testDaySeparators();
    ok &= testHtmlEscapingAndStructure();
    ok &= testHtmlUsesPipelineForFormattedBody();
    ok &= testJsonLinesStructure();
    ok &= testMetadataToggle();

    if (!ok)
        return 1;

    std::cout << "All ChatExportFormatter tests passed\n";
    return 0;
}

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string_view>

#include <QCoreApplication>
#include <QString>

#include "timeline/formattedmessage/HtmlProcessor.h"

namespace {

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

int
countOccurrences(const QString &text, const QString &needle)
{
    int count = 0;
    int pos   = 0;
    while ((pos = text.indexOf(needle, pos)) >= 0) {
        ++count;
        pos += needle.size();
    }
    return count;
}

bool
testEscapesDisallowedTags()
{
    const QString input = QStringLiteral("<script>alert(1)</script><b>ok</b>");
    const QString out   = timeline::formattedmessage::sanitizeHtml(input);

    bool ok = true;
    ok &= expect(!out.contains(QStringLiteral("<script")), "script tag is not kept");
    ok &= expect(out.contains(QStringLiteral("&lt;script")), "script opening is escaped");
    ok &= expect(out.contains(QStringLiteral("<b>ok</b>")), "allowed tag remains");
    return ok;
}

bool
testStripsMxReplyFallback()
{
    const QString input = QStringLiteral("<mx-reply><blockquote>old</blockquote></mx-reply><p>new</p>");
    const QString out   = timeline::formattedmessage::sanitizeHtml(input);

    bool ok = true;
    ok &= expect(!out.contains(QStringLiteral("mx-reply")), "mx-reply tags are stripped");
    ok &= expect(!out.contains(QStringLiteral("old")), "mx-reply content is stripped");
    ok &= expect(out.contains(QStringLiteral("<p>new</p>")), "non-reply content remains");
    return ok;
}

bool
testAnchorAttributeFiltering()
{
    const QString input = QStringLiteral(
      "<a href=\"https://example.org\" onclick=\"evil()\" target=\"_blank\">ok</a>");
    const QString out = timeline::formattedmessage::sanitizeHtml(input);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("href=\"https://example.org\"")),
                 "safe href is preserved");
    ok &= expect(out.contains(QStringLiteral("target=\"_blank\"")),
                 "target is preserved");
    ok &= expect(!out.contains(QStringLiteral("onclick")), "unsafe onclick attribute is removed");
    return ok;
}

bool
testHrefSchemeAndRelativeFiltering()
{
    const QString javascriptInput =
      QStringLiteral("<a href=\"javascript:alert(1)\">x</a>");
    const QString relativeInput = QStringLiteral("<a href=\"/relative\">x</a>");

    const QString javascriptOut = timeline::formattedmessage::sanitizeHtml(javascriptInput);
    const QString relativeOut   = timeline::formattedmessage::sanitizeHtml(relativeInput);

    bool ok = true;
    ok &= expect(!javascriptOut.contains(QStringLiteral("href=")), "javascript href is removed");
    ok &= expect(!relativeOut.contains(QStringLiteral("href=")), "relative href is removed");
    return ok;
}

bool
testImageSrcFiltering()
{
    const QString safeInput =
      QStringLiteral("<img src=\"mxc://example.org/id\" height=\"24\" width=\"32\" alt=\"a\">");
    const QString unsafeInput = QStringLiteral("<img src=\"https://example.org/x.png\" onerror=\"e()\">");

    const QString safeOut   = timeline::formattedmessage::sanitizeHtml(safeInput);
    const QString unsafeOut = timeline::formattedmessage::sanitizeHtml(unsafeInput);

    bool ok = true;
    ok &= expect(safeOut.contains(QStringLiteral("src=\"mxc://example.org/id\"")),
                 "mxc image source is preserved");
    ok &= expect(!unsafeOut.contains(QStringLiteral("src=\"https://")),
                 "non-mxc image source is removed");
    ok &= expect(!unsafeOut.contains(QStringLiteral("onerror")), "unsafe image attribute is removed");
    return ok;
}

bool
testCodeClassFiltering()
{
    const QString input =
      QStringLiteral("<code class=\"foo language-cpp language-python bad\">x</code>");
    const QString out = timeline::formattedmessage::sanitizeHtml(input);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("class=\"language-cpp language-python\"")),
                 "only language-* code classes are preserved");
    ok &= expect(!out.contains(QStringLiteral("foo")), "non-language classes are removed");
    return ok;
}

bool
testSpanColorValidation()
{
    const QString input = QStringLiteral(
      "<span data-mx-color=\"#112233\" data-mx-bg-color=\"#GGGGGG\" data-mx-spoiler=\"spoiler\" style=\"x\">x</span>");
    const QString out = timeline::formattedmessage::sanitizeHtml(input);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("data-mx-color=\"#112233\"")),
                 "valid data-mx-color is preserved");
    ok &= expect(!out.contains(QStringLiteral("data-mx-bg-color=\"#GGGGGG\"")),
                 "invalid color value is removed");
    ok &= expect(out.contains(QStringLiteral("data-mx-spoiler=\"spoiler\"")),
                 "spoiler attribute is preserved");
    ok &= expect(!out.contains(QStringLiteral("style=")), "unsupported style attribute is removed");
    return ok;
}

bool
testFontColorValidation()
{
    const QString input =
      QStringLiteral("<font color=\"#112233\">x</font>"
                     "<font color=\"orange\">x</font>"
                     "<font color=\"not-a-color\">x</font>");
    const QString out = timeline::formattedmessage::sanitizeHtml(input);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("color=\"#112233\"")),
                 "valid hex color is preserved");
    ok &= expect(out.contains(QStringLiteral("color=\"orange\"")),
                 "valid named color is preserved");
    ok &= expect(!out.contains(QStringLiteral("not-a-color")),
                 "invalid color is removed");
    return ok;
}

bool
testDepthLimit()
{
    QString input;
    for (int i = 0; i < 101; ++i)
        input += QStringLiteral("<div>");
    input += QStringLiteral("payload");
    for (int i = 0; i < 101; ++i)
        input += QStringLiteral("</div>");

    const QString out = timeline::formattedmessage::sanitizeHtml(input);

    bool ok = true;
    ok &= expect(countOccurrences(out, QStringLiteral("<div>")) <= 100,
                 "opening tag depth is capped at 100");
    ok &= expect(out.contains(QStringLiteral("payload")), "inner payload remains readable");
    return ok;
}

bool
testLinkifyPlainTextAndMatrixUri()
{
    const QString input =
      QStringLiteral("See https://example.org and matrix:r/example.org/abc");
    const QString out = timeline::formattedmessage::linkifyHtml(input);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("<a href=\"https://example.org\">https://example.org</a>")),
                 "plain https URL is linkified");
    ok &= expect(out.contains(QStringLiteral("<a href=\"matrix:r/example.org/abc\">matrix:r/example.org/abc</a>")),
                 "matrix URI is linkified");
    return ok;
}

bool
testLinkifySkipsAnchorAttributesAndCodeBlocks()
{
    const QString input = QStringLiteral(
      "<a href=\"https://already.example\">https://text.example</a> "
      "<code>https://code.example</code> "
      "<pre>https://pre.example</pre> "
      "https://tail.example");
    const QString out = timeline::formattedmessage::linkifyHtml(input);

    bool ok = true;
    ok &= expect(countOccurrences(out, QStringLiteral("<a href=\"https://already.example\">")) == 1,
                 "existing anchor tag remains unchanged");
    ok &= expect(out.contains(QStringLiteral("<code>https://code.example</code>")),
                 "code block text is not linkified");
    ok &= expect(out.contains(QStringLiteral("<pre>https://pre.example</pre>")),
                 "pre block text is not linkified");
    ok &= expect(out.contains(QStringLiteral("<a href=\"https://tail.example\">https://tail.example</a>")),
                 "trailing plain URL is linkified");
    return ok;
}

bool
testPillDecoratesUserMention()
{
    const QString input = QStringLiteral(
      "hello <a href=\"https://matrix.to/#/%40slavi%3Adevture.com\">Slavi</a> world");

    auto stubResolver = [](const QString &matrixId) -> QString {
        if (matrixId == QStringLiteral("@slavi:devture.com"))
            return QStringLiteral("image://mxcImage/devture.com/abc123?scale&height=32&radius=100");
        return {};
    };

    const QString out = timeline::formattedmessage::decorateMatrixPills(input, stubResolver);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("class=\"pill pill-user\"")),
                 "user pill has pill-user class");
    ok &= expect(out.contains(QStringLiteral("<img class=\"pill-avatar\"")),
                 "user pill has avatar img");
    ok &= expect(out.contains(QStringLiteral("src=\"image://mxcImage/devture.com/abc123")),
                 "avatar img has correct src from resolver");
    ok &= expect(out.contains(QStringLiteral("Slavi")),
                 "display name text is preserved");
    ok &= expect(out.contains(QStringLiteral("hello ")),
                 "text before pill is preserved");
    ok &= expect(out.contains(QStringLiteral(" world")),
                 "text after pill is preserved");
    return ok;
}

bool
testPillDecoratesRoomMention()
{
    const QString input = QStringLiteral(
      "<a href=\"https://matrix.to/#/%23room%3Aexample.org\">#room:example.org</a>");

    auto stubResolver = [](const QString &matrixId) -> QString {
        if (matrixId == QStringLiteral("#room:example.org"))
            return QStringLiteral("image://mxcImage/example.org/roomavatar");
        return {};
    };

    const QString out = timeline::formattedmessage::decorateMatrixPills(input, stubResolver);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("class=\"pill pill-room\"")),
                 "room pill has pill-room class");
    ok &= expect(out.contains(QStringLiteral("<img class=\"pill-avatar\"")),
                 "room pill has avatar img");
    ok &= expect(out.contains(QStringLiteral("#room:example.org")),
                 "room name text is preserved");
    return ok;
}

bool
testPillDecoratesRoomIdMention()
{
    const QString input = QStringLiteral(
      "<a href=\"https://matrix.to/#/!abc123%3Aexample.org\">My Room</a>");

    auto stubResolver = [](const QString &) -> QString { return {}; };

    const QString out = timeline::formattedmessage::decorateMatrixPills(input, stubResolver);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("class=\"pill pill-room\"")),
                 "room ID pill has pill-room class");
    ok &= expect(!out.contains(QStringLiteral("<img class=\"pill-avatar\"")),
                 "no avatar img when resolver returns empty");
    ok &= expect(out.contains(QStringLiteral("My Room")),
                 "display text is preserved");
    return ok;
}

bool
testPillSkipsNonMatrixToLinks()
{
    const QString input = QStringLiteral(
      "<a href=\"https://example.org\">Example</a>");

    auto stubResolver = [](const QString &) -> QString {
        return QStringLiteral("should-not-appear");
    };

    const QString out = timeline::formattedmessage::decorateMatrixPills(input, stubResolver);

    bool ok = true;
    ok &= expect(!out.contains(QStringLiteral("pill")),
                 "non-matrix.to link is not decorated");
    ok &= expect(out == input, "non-matrix.to link is unchanged");
    return ok;
}

bool
testPillPreservesMultipleLinks()
{
    const QString input = QStringLiteral(
      "<a href=\"https://matrix.to/#/%40alice%3Aexample.org\">Alice</a> and "
      "<a href=\"https://matrix.to/#/%40bob%3Aexample.org\">Bob</a>");

    auto stubResolver = [](const QString &matrixId) -> QString {
        if (matrixId == QStringLiteral("@alice:example.org"))
            return QStringLiteral("image://mxcImage/example.org/alice");
        if (matrixId == QStringLiteral("@bob:example.org"))
            return QStringLiteral("image://mxcImage/example.org/bob");
        return {};
    };

    const QString out = timeline::formattedmessage::decorateMatrixPills(input, stubResolver);

    bool ok = true;
    ok &= expect(countOccurrences(out, QStringLiteral("class=\"pill pill-user\"")) == 2,
                 "both user links are decorated");
    ok &= expect(out.contains(QStringLiteral("example.org/alice")),
                 "first avatar is present");
    ok &= expect(out.contains(QStringLiteral("example.org/bob")),
                 "second avatar is present");
    ok &= expect(out.contains(QStringLiteral("Alice")),
                 "first display name is preserved");
    ok &= expect(out.contains(QStringLiteral(" and ")),
                 "text between pills is preserved");
    ok &= expect(out.contains(QStringLiteral("Bob")),
                 "second display name is preserved");
    return ok;
}

bool
testPillWithNullResolver()
{
    const QString input = QStringLiteral(
      "<a href=\"https://matrix.to/#/%40user%3Aexample.org\">User</a>");

    const QString out =
      timeline::formattedmessage::decorateMatrixPills(input, nullptr);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("class=\"pill pill-user\"")),
                 "pill class is added even without resolver");
    ok &= expect(!out.contains(QStringLiteral("<img")),
                 "no img tag when resolver is null");
    return ok;
}

bool
testPillWithEventLink()
{
    // matrix.to link to a specific event: https://matrix.to/#/!room:server/$event:server
    const QString input = QStringLiteral(
      "<a href=\"https://matrix.to/#/!room%3Aserver/%24event%3Aserver\">link</a>");

    auto stubResolver = [](const QString &matrixId) -> QString {
        if (matrixId == QStringLiteral("!room:server"))
            return QStringLiteral("image://mxcImage/server/roomavatar");
        return {};
    };

    const QString out = timeline::formattedmessage::decorateMatrixPills(input, stubResolver);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("class=\"pill pill-room\"")),
                 "event link is decorated as room pill");
    ok &= expect(out.contains(QStringLiteral("image://mxcImage/server/roomavatar")),
                 "room avatar is resolved from room ID portion");
    return ok;
}

bool
testPlainTextToHtmlPreservesDoubleNewlines()
{
    const QString input = QStringLiteral("Hello!\n\nWorld!\n\nAnother");
    const QString out   = timeline::formattedmessage::plainTextToHtml(input);

    bool ok = true;
    ok &= expect(out == QStringLiteral("<p>Hello!</p><p>World!</p><p>Another</p>"),
                 "double newlines become paragraph breaks");
    return ok;
}

bool
testPlainTextToHtmlPreservesSingleNewlines()
{
    const QString input = QStringLiteral("Line1\nLine2\nLine3");
    const QString out   = timeline::formattedmessage::plainTextToHtml(input);

    bool ok = true;
    ok &= expect(out == QStringLiteral("<p>Line1<br>Line2<br>Line3</p>"),
                 "single newlines become <br> within a paragraph");
    return ok;
}

bool
testPlainTextToHtmlMixedNewlines()
{
    const QString input = QStringLiteral("A\nB\n\nC\nD");
    const QString out   = timeline::formattedmessage::plainTextToHtml(input);

    bool ok = true;
    ok &= expect(out == QStringLiteral("<p>A<br>B</p><p>C<br>D</p>"),
                 "mixed single and double newlines are handled correctly");
    return ok;
}

bool
testPlainTextToHtmlEscapesHtml()
{
    const QString input = QStringLiteral("<b>bold</b> & \"quoted\"");
    const QString out   = timeline::formattedmessage::plainTextToHtml(input);

    bool ok = true;
    ok &= expect(!out.contains(QStringLiteral("<b>")), "HTML tags are escaped");
    ok &= expect(out.contains(QStringLiteral("&amp;")), "ampersand is escaped");
    ok &= expect(out.contains(QStringLiteral("&lt;b&gt;")), "angle brackets are escaped");
    return ok;
}

bool
testPlainTextToHtmlEmpty()
{
    const QString out = timeline::formattedmessage::plainTextToHtml({});

    bool ok = true;
    ok &= expect(out.isEmpty(), "empty input produces empty output");
    return ok;
}

} // namespace

int
main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    bool ok = true;
    ok &= testEscapesDisallowedTags();
    ok &= testStripsMxReplyFallback();
    ok &= testAnchorAttributeFiltering();
    ok &= testHrefSchemeAndRelativeFiltering();
    ok &= testImageSrcFiltering();
    ok &= testCodeClassFiltering();
    ok &= testSpanColorValidation();
    ok &= testFontColorValidation();
    ok &= testDepthLimit();
    ok &= testLinkifyPlainTextAndMatrixUri();
    ok &= testLinkifySkipsAnchorAttributesAndCodeBlocks();
    ok &= testPillDecoratesUserMention();
    ok &= testPillDecoratesRoomMention();
    ok &= testPillDecoratesRoomIdMention();
    ok &= testPillSkipsNonMatrixToLinks();
    ok &= testPillPreservesMultipleLinks();
    ok &= testPillWithNullResolver();
    ok &= testPillWithEventLink();
    ok &= testPlainTextToHtmlPreservesDoubleNewlines();
    ok &= testPlainTextToHtmlPreservesSingleNewlines();
    ok &= testPlainTextToHtmlMixedNewlines();
    ok &= testPlainTextToHtmlEscapesHtml();
    ok &= testPlainTextToHtmlEmpty();

    return ok ? 0 : 1;
}

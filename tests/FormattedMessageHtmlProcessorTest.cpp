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

// --- transformForPresentation tests ---

const timeline::formattedmessage::PresentationColors testColors{
  QStringLiteral("#a6a7a9"), // blockquoteBackground
  QStringLiteral("#ff0000"), // error
  QStringLiteral("#ffaa00"), // attention
  QStringLiteral("#00cc00"), // success
};

bool
testDelToS()
{
    const QString input = QStringLiteral("<del>text</del>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out == QStringLiteral("<s>text</s>"), "del is normalized to s");
    return ok;
}

bool
testStrikeToS()
{
    const QString input = QStringLiteral("<strike>text</strike>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out == QStringLiteral("<s>text</s>"), "strike is normalized to s");
    return ok;
}

bool
testMixedDelStrike()
{
    const QString input = QStringLiteral("<del>a</del> and <strike>b</strike>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out == QStringLiteral("<s>a</s> and <s>b</s>"), "mixed del and strike both become s");
    return ok;
}

bool
testFontColorRed()
{
    const QString input = QStringLiteral("<font color=\"red\">x</font>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("color=\"#ff0000\"")),
                 "red maps to error color");
    return ok;
}

bool
testFontColorError()
{
    const QString input = QStringLiteral("<font color=\"error\">x</font>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("color=\"#ff0000\"")),
                 "error maps to error color");
    return ok;
}

bool
testFontColorSuccess()
{
    const QString input = QStringLiteral("<font color=\"success\">x</font>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("color=\"#00cc00\"")),
                 "success maps to success color");
    return ok;
}

bool
testFontColorGreen()
{
    const QString input = QStringLiteral("<font color=\"green\">x</font>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("color=\"#00cc00\"")),
                 "green maps to success color");
    return ok;
}

bool
testFontColorYellow()
{
    const QString input = QStringLiteral("<font color=\"yellow\">x</font>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("color=\"#ffaa00\"")),
                 "yellow maps to attention color");
    return ok;
}

bool
testFontColorWarning()
{
    const QString input = QStringLiteral("<font color=\"warning\">x</font>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("color=\"#ffaa00\"")),
                 "warning maps to attention color");
    return ok;
}

bool
testFontColorOrange()
{
    const QString input = QStringLiteral("<font color=\"orange\">x</font>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("color=\"#ffaa00\"")),
                 "orange maps to attention color");
    return ok;
}

bool
testFontColorHexPassthrough()
{
    const QString input = QStringLiteral("<font color=\"#112233\">x</font>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("color=\"#112233\"")),
                 "hex color passes through unchanged");
    return ok;
}

bool
testFontColorMultiple()
{
    const QString input = QStringLiteral(
      "<font color=\"red\">a</font> <font color=\"green\">b</font>");
    const QString out = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("color=\"#ff0000\"")),
                 "first font color mapped");
    ok &= expect(out.contains(QStringLiteral("color=\"#00cc00\"")),
                 "second font color mapped");
    return ok;
}

bool
testFontColorWithExtraAttrs()
{
    const QString input =
      QStringLiteral("<font color=\"red\" data-mx-bg-color=\"#aabbcc\">x</font>");
    const QString out = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("color=\"#ff0000\"")),
                 "color value is replaced");
    ok &= expect(out.contains(QStringLiteral("data-mx-bg-color=\"#aabbcc\"")),
                 "other attributes preserved");
    return ok;
}

bool
testSimpleBlockquote()
{
    const QString input = QStringLiteral("<blockquote>hello</blockquote>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("background-color:#a6a7a9")),
                 "blockquote gets background color");
    ok &= expect(out.contains(QStringLiteral("<table")), "blockquote replaced with table");
    ok &= expect(out.contains(QStringLiteral("hello")), "content preserved");
    return ok;
}

bool
testNestedBlockquotes()
{
    const QString input =
      QStringLiteral("<blockquote>outer<blockquote>inner</blockquote></blockquote>");
    const QString out = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(countOccurrences(out, QStringLiteral("background-color:#a6a7a9")) == 2,
                 "both blockquote levels get background color");
    ok &= expect(out.contains(QStringLiteral("outer")), "outer content preserved");
    ok &= expect(out.contains(QStringLiteral("inner")), "inner content preserved");
    return ok;
}

bool
testEmptyBlockquote()
{
    const QString input = QStringLiteral("<blockquote></blockquote>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("background-color:#a6a7a9")),
                 "empty blockquote gets background");
    return ok;
}

bool
testMultipleBlockquotes()
{
    const QString input =
      QStringLiteral("<blockquote>a</blockquote>gap<blockquote>b</blockquote>");
    const QString out = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(countOccurrences(out, QStringLiteral("background-color:#a6a7a9")) == 2,
                 "both blockquotes get background color");
    ok &= expect(out.contains(QStringLiteral("gap")), "gap content preserved");
    return ok;
}

bool
testEscapedBlockquoteNotTransformed()
{
    const QString input = QStringLiteral("&lt;blockquote&gt;not a tag&lt;/blockquote&gt;");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out == input, "HTML-escaped blockquote not transformed");
    return ok;
}

bool
testLiteralColorTextNotModified()
{
    const QString input = QStringLiteral("<p>The attribute is color=\"red\" in the spec.</p>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(!out.contains(testColors.error),
                 "literal color=red outside font tag not modified");
    return ok;
}

bool
testNoTransformationsPassthrough()
{
    const QString input = QStringLiteral("<b>bold</b> and <i>italic</i>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out == input, "input without transformable elements passes through unchanged");
    return ok;
}

bool
testDeeplyNestedBlockquotes()
{
    QString input;
    for (int i = 0; i < 10; ++i)
        input += QStringLiteral("<blockquote>");
    input += QStringLiteral("deep");
    for (int i = 0; i < 10; ++i)
        input += QStringLiteral("</blockquote>");

    const QString out = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(countOccurrences(out, QStringLiteral("background-color:#a6a7a9")) == 10,
                 "10 blockquotes each get background color");
    ok &= expect(out.contains(QStringLiteral("deep")), "deeply nested content preserved");
    return ok;
}

bool
testBlockquoteInsideOtherElements()
{
    const QString input = QStringLiteral("<p><blockquote>quoted</blockquote></p>");
    const QString out   = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("background-color:#a6a7a9")),
                 "blockquote inside p gets background");
    ok &= expect(out.contains(QStringLiteral("quoted")), "content preserved");
    return ok;
}

bool
testBlockquoteMultiParagraph()
{
    const QString input = QStringLiteral(
      "<blockquote>\n<p>Para 1</p>\n<p>Para 2</p>\n</blockquote>");
    const QString out = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("background-color:#a6a7a9")),
                 "multi-paragraph blockquote gets background");
    ok &= expect(!out.contains(QStringLiteral("<p>")), "p tags stripped inside blockquote");
    ok &= expect(out.contains(QStringLiteral("Para 1<br>Para 2")),
                 "paragraphs joined with br");
    return ok;
}

bool
testBlockquoteParagraphsOutsidePreserved()
{
    const QString input = QStringLiteral(
      "<p>Before</p>\n<blockquote>\n<p>Quoted</p>\n</blockquote>\n<p>After</p>");
    const QString out = timeline::formattedmessage::transformForPresentation(input, testColors);

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("<p>Before</p>")),
                 "paragraph before blockquote preserved");
    ok &= expect(out.contains(QStringLiteral("<p>After</p>")),
                 "paragraph after blockquote preserved");
    ok &= expect(out.contains(QStringLiteral("Quoted")), "blockquote content preserved");
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

    // transformForPresentation tests
    ok &= testDelToS();
    ok &= testStrikeToS();
    ok &= testMixedDelStrike();
    ok &= testFontColorRed();
    ok &= testFontColorError();
    ok &= testFontColorSuccess();
    ok &= testFontColorGreen();
    ok &= testFontColorYellow();
    ok &= testFontColorWarning();
    ok &= testFontColorOrange();
    ok &= testFontColorHexPassthrough();
    ok &= testFontColorMultiple();
    ok &= testFontColorWithExtraAttrs();
    ok &= testSimpleBlockquote();
    ok &= testNestedBlockquotes();
    ok &= testEmptyBlockquote();
    ok &= testMultipleBlockquotes();
    ok &= testEscapedBlockquoteNotTransformed();
    ok &= testLiteralColorTextNotModified();
    ok &= testNoTransformationsPassthrough();
    ok &= testDeeplyNestedBlockquotes();
    ok &= testBlockquoteInsideOtherElements();
    ok &= testBlockquoteMultiParagraph();
    ok &= testBlockquoteParagraphsOutsidePreserved();

    return ok ? 0 : 1;
}

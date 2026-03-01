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
    ok &= testDepthLimit();
    ok &= testLinkifyPlainTextAndMatrixUri();
    ok &= testLinkifySkipsAnchorAttributesAndCodeBlocks();

    return ok ? 0 : 1;
}

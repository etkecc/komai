// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string_view>

#include <QGuiApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QString>
#include <QTextDocumentFragment>

#include <KSyntaxHighlighting/repository.h>

#include "timeline/FormattedCodeBlockHighlighter.h"
#include "timeline/formattedcode/LanguageResolver.h"

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
testHighlightsRecognizedLanguage()
{
    const QString input =
      QStringLiteral("<pre><code class=\"language-diff\">+ line\n- line</code></pre>");
    const QString output = timeline::highlightFormattedCodeBlocks(input, QPalette(), true);

    bool ok = true;
    ok &= expect(output.contains(QStringLiteral("<pre><code class=\"language-diff\">")),
                 "preserves code-block wrapper and language class");
    ok &= expect(output.contains(QStringLiteral("<span style=\"")),
                 "adds formatted spans for recognized syntax");
    ok &= expect(output.contains(QStringLiteral("+ line")) && output.contains(QStringLiteral("- line")),
                 "preserves code content");
    return ok;
}

bool
testPreservesCodeBlockNewlines()
{
    const QString input = QStringLiteral(
      "<pre><code class=\"language-php\">&lt;?php\nclass Greeter {\n    public function hello(): string {\n        return \"Hello, world!\";\n    }\n}\n</code></pre>"
      "<p>Hey!</p>");
    const QString output = timeline::highlightFormattedCodeBlocks(input, QPalette(), true);

    const QRegularExpression codeBodyRegex(
      QStringLiteral("<code\\b[^>]*>(.*?)</code>"),
      QRegularExpression::DotMatchesEverythingOption);
    const auto codeBodyMatch = codeBodyRegex.match(output);

    bool ok = true;
    ok &= expect(codeBodyMatch.hasMatch(), "highlighted output still contains a code block body");
    if (!codeBodyMatch.hasMatch())
        return ok;

    const QString codeBody = codeBodyMatch.captured(1);
    const QString reconstructed =
      QTextDocumentFragment::fromHtml(QStringLiteral("<pre>") + codeBody + QStringLiteral("</pre>"))
        .toPlainText();

    ok &= expect(reconstructed.contains(QStringLiteral("\nclass Greeter")),
                 "code block newline before class declaration is preserved");
    ok &= expect(reconstructed.contains(QStringLiteral("\n    public function hello")),
                 "code block indentation and newline are preserved");
    ok &= expect(reconstructed.count(QLatin1Char('\n')) >= 5,
                 "code block keeps multiline structure after highlighting");
    return ok;
}

bool
testUnknownLanguageFallsBackToOriginalBlock()
{
    const QString input = QStringLiteral(
      "<pre><code class=\"language-komai-unknown\">hello()</code></pre>");
    const QString output = timeline::highlightFormattedCodeBlocks(input, QPalette(), true);
    return expect(output == input, "unknown language leaves code block unchanged");
}

bool
testParsesLanguageClassCaseInsensitively()
{
    const QString input =
      QStringLiteral("<pre><code CLASS='LANGUAGE-PHP'>&lt;?php\necho \"Hello, world!\";\n</code></pre>");
    const QString output = timeline::highlightFormattedCodeBlocks(input, QPalette(), true);

    bool ok = true;
    ok &= expect(output.contains(QStringLiteral("<span style=\"")),
                 "case-insensitive language class is detected");
    ok &= expect(output.contains(QStringLiteral("Hello, world!")),
                 "case-insensitive language class keeps code content");
    return ok;
}

bool
testAutoDetectsPhpWhenLanguageIsOmitted()
{
    const QString input = QStringLiteral(
      "<pre><code>&lt;?php\necho \"Hello, world!\";\n</code></pre>");
    const QString output = timeline::highlightFormattedCodeBlocks(input, QPalette(), true);

    bool ok = true;
    ok &= expect(output.contains(QStringLiteral("<span style=\"")),
                 "omitted language can still be highlighted via heuristic detection");
    ok &= expect(output.contains(QStringLiteral("Hello, world!")),
                 "auto-detected highlighting preserves code content");
    return ok;
}

bool
testMimeBasedDetectionFindsDefinition()
{
    KSyntaxHighlighting::Repository repo;
    const QString htmlSnippet =
      QStringLiteral("<!DOCTYPE html><html><body>Hello from MIME path</body></html>");
    const auto definition = timeline::formattedcode::detectDefinitionByMimeType(htmlSnippet, repo);

    bool ok = true;
    ok &= expect(definition.isValid(), "MIME-based detection returns a valid syntax definition");
    ok &= expect(!definition.name().isEmpty(), "MIME-based detection returns a named definition");
    return ok;
}

bool
testAllowsWhitespaceBetweenPreAndCodeTags()
{
    const QString input =
      QStringLiteral("<pre>\n  <code class=\"language-json\">{\"hello\": \"world\"}</code>\n</pre>");
    const QString output = timeline::highlightFormattedCodeBlocks(input, QPalette(), true);

    return expect(output.contains(QStringLiteral("<span style=\"")),
                  "whitespace between pre/code still allows highlighting");
}

bool
testIgnoresNonWhitespaceBetweenPreAndCodeTags()
{
    const QString input = QStringLiteral(
      "<pre><span>prefix</span><code class=\"language-json\">{\"hello\": \"world\"}</code></pre>");
    const QString output = timeline::highlightFormattedCodeBlocks(input, QPalette(), true);
    return expect(output == input, "non-whitespace content between pre/code is left unchanged");
}

bool
testNoLanguageFallsBackToOriginalBlock()
{
    const QString input = QStringLiteral("<pre><code>plain text</code></pre>");
    const QString output = timeline::highlightFormattedCodeBlocks(input, QPalette(), true);
    return expect(output == input, "missing language leaves code block unchanged");
}

bool
testMalformedCodeBlockIsLeftUnchanged()
{
    const QString input = QStringLiteral("<pre><code class=\"language-cpp\">int x = 1;");
    const QString output = timeline::highlightFormattedCodeBlocks(input, QPalette(), true);
    return expect(output == input, "malformed/unclosed code blocks are left unchanged");
}

bool
testDisabledLeavesHtmlUnchanged()
{
    const QString input = QStringLiteral(
      "<pre><code class=\"language-cpp\">int x = 1;</code></pre>");
    const QString output = timeline::highlightFormattedCodeBlocks(input, QPalette(), false);
    return expect(output == input, "disabled mode returns original html");
}

bool
testHighlightingCapsAtConfiguredBlockLimit()
{
    QString input;
    for (int i = 1; i <= 70; ++i) {
        input += QStringLiteral("<pre><code class=\"language-diff\">+ line%1\n- line%1</code></pre>")
                   .arg(i);
    }

    const QString output = timeline::highlightFormattedCodeBlocks(input, QPalette(), true);

    bool ok = true;
    ok &= expect(output.contains(QStringLiteral("<span style=\"")),
                 "at least one block is syntax-highlighted");
    ok &= expect(!output.contains(QStringLiteral("<pre><code class=\"language-diff\">+ line1\n- line1</code></pre>")),
                 "early blocks are transformed");
    ok &= expect(output.contains(QStringLiteral("<pre><code class=\"language-diff\">+ line70\n- line70</code></pre>")),
                 "blocks beyond the cap stay unchanged");
    ok &= expect(countOccurrences(output, QStringLiteral("<span style=\"")) > 0,
                 "highlighting output includes styled ranges");
    return ok;
}

bool
testLargeCodeBlockFallsBackWithoutHighlighting()
{
    const QString largePayload(25000, QLatin1Char('x'));
    const QString input = QStringLiteral("<pre><code class=\"language-cpp\">%1</code></pre>").arg(largePayload);
    const QString output = timeline::highlightFormattedCodeBlocks(input, QPalette(), true);
    return expect(output == input, "large code blocks are left unchanged to avoid expensive highlighting");
}

bool
testOversizedHtmlFallsBackWithoutHighlighting()
{
    const QString prefix(1000005, QLatin1Char('x'));
    const QString input = prefix + QStringLiteral("<pre><code class=\"language-cpp\">int x = 1;</code></pre>");
    const QString output = timeline::highlightFormattedCodeBlocks(input, QPalette(), true);
    return expect(output == input, "oversized HTML payload is left unchanged");
}

bool
testHighlightedCodeEscapesHtmlSpecialChars()
{
    const QString input = QStringLiteral(
      "<pre><code class=\"language-html\">&lt;img src=x onerror=alert(1)&gt;</code></pre>");
    const QString output = timeline::highlightFormattedCodeBlocks(input, QPalette(), true);

    const QRegularExpression codeBodyRegex(
      QStringLiteral("<code\\b[^>]*>(.*?)</code>"),
      QRegularExpression::DotMatchesEverythingOption);
    const auto codeBodyMatch = codeBodyRegex.match(output);

    bool ok = true;
    ok &= expect(codeBodyMatch.hasMatch(), "highlighted output contains code block body");
    if (!codeBodyMatch.hasMatch())
        return ok;

    const QString codeBody = codeBodyMatch.captured(1);
    const QString reconstructed =
      QTextDocumentFragment::fromHtml(QStringLiteral("<pre>") + codeBody + QStringLiteral("</pre>"))
        .toPlainText();

    ok &= expect(reconstructed.contains(QStringLiteral("<img src=x onerror=alert(1)>")),
                 "code HTML stays literal text when reconstructed");
    ok &= expect(!output.contains(QStringLiteral("<img src=x onerror=alert(1)>")),
                 "code HTML does not become an executable tag");
    return ok;
}

} // namespace

int
main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    bool ok = true;
    ok &= testHighlightsRecognizedLanguage();
    ok &= testPreservesCodeBlockNewlines();
    ok &= testUnknownLanguageFallsBackToOriginalBlock();
    ok &= testParsesLanguageClassCaseInsensitively();
    ok &= testAutoDetectsPhpWhenLanguageIsOmitted();
    ok &= testMimeBasedDetectionFindsDefinition();
    ok &= testAllowsWhitespaceBetweenPreAndCodeTags();
    ok &= testIgnoresNonWhitespaceBetweenPreAndCodeTags();
    ok &= testNoLanguageFallsBackToOriginalBlock();
    ok &= testMalformedCodeBlockIsLeftUnchanged();
    ok &= testDisabledLeavesHtmlUnchanged();
    ok &= testHighlightingCapsAtConfiguredBlockLimit();
    ok &= testLargeCodeBlockFallsBackWithoutHighlighting();
    ok &= testOversizedHtmlFallsBackWithoutHighlighting();
    ok &= testHighlightedCodeEscapesHtmlSpecialChars();
    return ok ? 0 : 1;
}

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FormattedCodeBlockHighlighter.h"

#include <algorithm>

#include <QChar>
#include <QTextDocumentFragment>

#include <KSyntaxHighlighting/repository.h>

#include "timeline/formattedcode/HtmlRenderer.h"
#include "timeline/formattedcode/LanguageResolver.h"

namespace timeline {
namespace {

constexpr int kMaxEncodedCodeBlockChars = 120000;
constexpr int kMaxDecodedCodeBlockChars = 20000;
constexpr int kMaxDecodedCodeBlockLines = 800;
constexpr int kMaxHighlightedCodeBlocks = 64;
constexpr int kMaxHtmlChars             = 1000000;
constexpr int kMaxParsedTags            = 50000;

struct ParsedTag
{
    bool valid       = false;
    bool special     = false;
    bool isEnd       = false;
    bool selfClosing = false;
    int start        = 0;
    int end          = 0;
    int nameStart    = 0;
    int nameLength   = 0;
    int attrsStart   = 0;
    int attrsLength  = 0;
};

bool
isTagNameChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_') ||
           c == QLatin1Char(':');
}

bool
isWhitespaceOnly(const QString &text, int start, int end)
{
    for (int i = start; i < end; ++i) {
        if (!text.at(i).isSpace())
            return false;
    }
    return true;
}

QString
attrsFromTag(const QString &html, const ParsedTag &tag)
{
    if (tag.attrsLength <= 0)
        return {};
    return html.mid(tag.attrsStart, tag.attrsLength);
}

ParsedTag
parseTag(const QString &html, int tagStart)
{
    ParsedTag tag;
    tag.start = tagStart;
    tag.end   = html.size();

    if (tagStart < 0 || tagStart >= html.size() || html.at(tagStart) != QLatin1Char('<'))
        return tag;

    int i = tagStart + 1;
    QChar quote;
    for (; i < html.size(); ++i) {
        const auto c = html.at(i);
        if (quote.isNull()) {
            if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
                quote = c;
            } else if (c == QLatin1Char('>')) {
                break;
            }
        } else if (c == quote) {
            quote = QChar();
        }
    }

    if (i >= html.size())
        return tag;

    tag.end = i + 1;

    int contentStart = tagStart + 1;
    int contentEnd   = i;
    while (contentStart < contentEnd && html.at(contentStart).isSpace())
        ++contentStart;
    while (contentEnd > contentStart && html.at(contentEnd - 1).isSpace())
        --contentEnd;
    if (contentStart >= contentEnd)
        return tag;

    if (html.at(contentStart) == QLatin1Char('!') || html.at(contentStart) == QLatin1Char('?')) {
        tag.valid   = true;
        tag.special = true;
        return tag;
    }

    int cursor = contentStart;
    if (html.at(cursor) == QLatin1Char('/')) {
        tag.isEnd = true;
        ++cursor;
        while (cursor < contentEnd && html.at(cursor).isSpace())
            ++cursor;
    }

    const int nameStart = cursor;
    while (cursor < contentEnd && isTagNameChar(html.at(cursor)))
        ++cursor;
    if (cursor == nameStart)
        return tag;

    tag.nameStart  = nameStart;
    tag.nameLength = cursor - nameStart;

    if (!tag.isEnd) {
        int attrsStart = cursor;
        int attrsEnd   = contentEnd;
        while (attrsEnd > attrsStart && html.at(attrsEnd - 1).isSpace())
            --attrsEnd;
        if (attrsEnd > attrsStart && html.at(attrsEnd - 1) == QLatin1Char('/')) {
            tag.selfClosing = true;
            --attrsEnd;
            while (attrsEnd > attrsStart && html.at(attrsEnd - 1).isSpace())
                --attrsEnd;
        }
        tag.attrsStart  = attrsStart;
        tag.attrsLength = std::max(0, attrsEnd - attrsStart);
    }

    tag.valid = true;
    return tag;
}

bool
tagNameEquals(const QString &html, const ParsedTag &tag, QStringView wanted)
{
    if (!tag.valid || tag.nameLength <= 0)
        return false;
    return QStringView(html)
             .mid(tag.nameStart, tag.nameLength)
             .compare(wanted, Qt::CaseInsensitive) == 0;
}

bool
isHighlightEligible(const QString &decodedCode)
{
    if (decodedCode.size() > kMaxDecodedCodeBlockChars)
        return false;

    const int lineCount = decodedCode.count(QLatin1Char('\n')) + 1;
    return lineCount <= kMaxDecodedCodeBlockLines;
}

} // namespace

QString
highlightFormattedCodeBlocks(const QString &html, const QPalette &palette, bool enabled)
{
    if (!enabled || html.isEmpty() || html.size() > kMaxHtmlChars)
        return html;

    static KSyntaxHighlighting::Repository repository;
    auto syntaxTheme = repository.themeForPalette(palette);
    if (!syntaxTheme.isValid()) {
        syntaxTheme = repository.defaultTheme(palette.color(QPalette::Base).lightness() < 128
                                                ? KSyntaxHighlighting::Repository::DarkTheme
                                                : KSyntaxHighlighting::Repository::LightTheme);
    }

    QString transformed;
    transformed.reserve(html.size());
    int flushStart        = 0;
    int parsedTagCount    = 0;
    int highlightedBlocks = 0;
    int pos               = 0;

    enum class State
    {
        Outside,
        AfterPreOpen,
        InCode,
        AfterCodeClose,
    };

    State state   = State::Outside;
    int codeDepth = 0;
    ParsedTag preTag;
    ParsedTag codeTag;
    ParsedTag codeCloseTag;

    while (pos < html.size()) {
        if (html.at(pos) != QLatin1Char('<')) {
            ++pos;
            continue;
        }

        if (parsedTagCount++ > kMaxParsedTags)
            break;

        const auto tag = parseTag(html, pos);
        if (!tag.valid)
            break;

        bool reevaluate = false;
        switch (state) {
        case State::Outside:
            if (!tag.special && !tag.isEnd && !tag.selfClosing &&
                tagNameEquals(html, tag, QStringLiteral("pre"))) {
                preTag = tag;
                state  = State::AfterPreOpen;
            }
            break;
        case State::AfterPreOpen:
            if (!isWhitespaceOnly(html, preTag.end, tag.start)) {
                state      = State::Outside;
                reevaluate = true;
                break;
            }
            if (!tag.special && !tag.isEnd && !tag.selfClosing &&
                tagNameEquals(html, tag, QStringLiteral("code"))) {
                codeTag   = tag;
                codeDepth = 1;
                state     = State::InCode;
            } else {
                state      = State::Outside;
                reevaluate = true;
            }
            break;
        case State::InCode:
            if (!tag.special && tagNameEquals(html, tag, QStringLiteral("code"))) {
                if (!tag.isEnd && !tag.selfClosing) {
                    ++codeDepth;
                } else if (tag.isEnd) {
                    --codeDepth;
                    if (codeDepth == 0) {
                        codeCloseTag = tag;
                        state        = State::AfterCodeClose;
                    }
                }
            }
            break;
        case State::AfterCodeClose:
            if (!isWhitespaceOnly(html, codeCloseTag.end, tag.start)) {
                state      = State::Outside;
                reevaluate = true;
                break;
            }
            if (!tag.special && tag.isEnd && tagNameEquals(html, tag, QStringLiteral("pre"))) {
                transformed += html.mid(flushStart, preTag.start - flushStart);

                const auto originalCodeBlock = html.mid(preTag.start, tag.end - preTag.start);
                const auto preAttrs          = attrsFromTag(html, preTag);
                const auto codeAttrs         = attrsFromTag(html, codeTag);
                const auto codeBody = html.mid(codeTag.end, codeCloseTag.start - codeTag.end);

                bool highlighted = false;
                if (highlightedBlocks < kMaxHighlightedCodeBlocks &&
                    codeBody.size() <= kMaxEncodedCodeBlockChars) {
                    // Wrap in <pre> before HTML decoding to preserve literal newlines and
                    // indentation.
                    const auto decodedCode =
                      QTextDocumentFragment::fromHtml(QStringLiteral("<pre>") + codeBody +
                                                      QStringLiteral("</pre>"))
                        .toPlainText();
                    if (isHighlightEligible(decodedCode)) {
                        const auto languageToken = formattedcode::extractLanguageToken(codeAttrs);
                        const auto definition =
                          languageToken.isEmpty()
                            ? formattedcode::detectDefinitionFromContent(decodedCode, repository)
                            : formattedcode::resolveDefinition(languageToken, repository);
                        if (definition.isValid()) {
                            transformed += QStringLiteral("<pre%1><code%2>%3</code></pre>")
                                             .arg(preAttrs,
                                                  codeAttrs,
                                                  formattedcode::highlightCodeAsHtml(
                                                    decodedCode, definition, syntaxTheme));
                            ++highlightedBlocks;
                            highlighted = true;
                        }
                    }
                }

                if (!highlighted)
                    transformed += originalCodeBlock;

                flushStart = tag.end;
                state      = State::Outside;
            } else {
                state      = State::Outside;
                reevaluate = true;
            }
            break;
        }

        if (reevaluate)
            continue;

        pos = tag.end;
    }

    transformed += html.mid(flushStart);
    return transformed;
}

} // namespace timeline

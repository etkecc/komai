// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/formattedmessage/HtmlProcessor.h"

#include <QRegularExpression>
#include <QSet>
#include <QStringView>
#include <QUrl>
#include <algorithm>

#include "timeline/formattedmessage/LinkPatterns.h"

// matrix.to URL prefix for detecting pill-eligible links.
static const QLatin1String kMatrixToPrefix("https://matrix.to/#/");

namespace timeline::formattedmessage {
namespace {

constexpr int kMaxTagDepth            = 100;
constexpr int kMaxAttributeCount      = 64;
constexpr int kMaxAttributeValueChars = 4096;
constexpr int kMaxSpoilerChars        = 512;
constexpr int kMaxMathChars           = 4096;
constexpr int kMaxTextAttributeChars  = 1024;
constexpr int kMaxTargetChars         = 64;
constexpr int kMaxClassChars          = 512;
constexpr int kMaxUrlChars            = 2048;

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
    int attrsEnd     = 0;
};

struct ParsedAttribute
{
    QString name;
    QString value;
    bool hasValue = false;
};

bool
isTagNameChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_') ||
           c == QLatin1Char(':');
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

        tag.attrsStart = attrsStart;
        tag.attrsEnd   = attrsEnd;
    }

    tag.valid = true;
    return tag;
}

QString
tagNameLower(const QString &html, const ParsedTag &tag)
{
    if (!tag.valid || tag.nameLength <= 0)
        return {};
    return QStringView(html).mid(tag.nameStart, tag.nameLength).toString().toLower();
}

bool
isVoidTag(const QString &tagName)
{
    static const QSet<QString> kVoidTags = {
      QStringLiteral("br"),
      QStringLiteral("hr"),
      QStringLiteral("img"),
    };
    return kVoidTags.contains(tagName);
}

bool
isAllowedTag(const QString &tagName)
{
    static const QSet<QString> kAllowedTags = {
      QStringLiteral("font"),    QStringLiteral("del"),     QStringLiteral("h1"),
      QStringLiteral("h2"),      QStringLiteral("h3"),      QStringLiteral("h4"),
      QStringLiteral("h5"),      QStringLiteral("h6"),      QStringLiteral("blockquote"),
      QStringLiteral("p"),       QStringLiteral("a"),       QStringLiteral("ul"),
      QStringLiteral("ol"),      QStringLiteral("sup"),     QStringLiteral("sub"),
      QStringLiteral("li"),      QStringLiteral("b"),       QStringLiteral("i"),
      QStringLiteral("u"),       QStringLiteral("strong"),  QStringLiteral("em"),
      QStringLiteral("s"),       QStringLiteral("strike"),  QStringLiteral("code"),
      QStringLiteral("hr"),      QStringLiteral("br"),      QStringLiteral("div"),
      QStringLiteral("table"),   QStringLiteral("thead"),   QStringLiteral("tbody"),
      QStringLiteral("tr"),      QStringLiteral("th"),      QStringLiteral("td"),
      QStringLiteral("caption"), QStringLiteral("pre"),     QStringLiteral("span"),
      QStringLiteral("img"),     QStringLiteral("details"), QStringLiteral("summary"),
    };

    return kAllowedTags.contains(tagName);
}

QList<ParsedAttribute>
parseAttributes(const QString &html, const ParsedTag &tag)
{
    QList<ParsedAttribute> attrs;
    if (!tag.valid || tag.isEnd || tag.attrsEnd <= tag.attrsStart)
        return attrs;

    int cursor = tag.attrsStart;
    while (cursor < tag.attrsEnd && attrs.size() < kMaxAttributeCount) {
        while (cursor < tag.attrsEnd && html.at(cursor).isSpace())
            ++cursor;
        if (cursor >= tag.attrsEnd)
            break;

        const int nameStart = cursor;
        while (cursor < tag.attrsEnd && !html.at(cursor).isSpace() &&
               html.at(cursor) != QLatin1Char('='))
            ++cursor;
        if (cursor == nameStart)
            break;

        ParsedAttribute attr;
        attr.name = QStringView(html).mid(nameStart, cursor - nameStart).toString().toLower();

        while (cursor < tag.attrsEnd && html.at(cursor).isSpace())
            ++cursor;

        if (cursor < tag.attrsEnd && html.at(cursor) == QLatin1Char('=')) {
            ++cursor;
            while (cursor < tag.attrsEnd && html.at(cursor).isSpace())
                ++cursor;

            attr.hasValue = true;
            if (cursor < tag.attrsEnd &&
                (html.at(cursor) == QLatin1Char('"') || html.at(cursor) == QLatin1Char('\''))) {
                const auto quote     = html.at(cursor++);
                const int valueStart = cursor;
                while (cursor < tag.attrsEnd && html.at(cursor) != quote)
                    ++cursor;
                attr.value = QStringView(html).mid(valueStart, cursor - valueStart).toString();
                if (cursor < tag.attrsEnd)
                    ++cursor;
            } else {
                const int valueStart = cursor;
                while (cursor < tag.attrsEnd && !html.at(cursor).isSpace())
                    ++cursor;
                attr.value = QStringView(html).mid(valueStart, cursor - valueStart).toString();
            }
        }

        attrs.push_back(attr);
    }

    return attrs;
}

QString
sanitizeHexColor(const QString &raw)
{
    static const QRegularExpression hexColor(QStringLiteral(R"(^#[0-9a-fA-F]{6}$)"));
    const auto value = raw.trimmed();
    if (!hexColor.match(value).hasMatch())
        return {};
    return value;
}

QString
sanitizeFontColor(const QString &raw)
{
    static const QRegularExpression hexColor(QStringLiteral(R"(^#[0-9a-fA-F]{6}$)"));
    const auto value = raw.trimmed();
    if (hexColor.match(value).hasMatch())
        return value;

    static const QSet<QString> allowedColors = {
      QStringLiteral("red"),
      QStringLiteral("orange"),
      QStringLiteral("yellow"),
      QStringLiteral("green"),
      QStringLiteral("warning"),
      QStringLiteral("success"),
      QStringLiteral("error"),
    };
    if (!allowedColors.contains(value.toLower()))
        return {};

    return value.toLower();
}

QString
sanitizeIntegerString(const QString &raw)
{
    static const QRegularExpression integerRegex(QStringLiteral(R"(^\d{1,5}$)"));
    const auto value = raw.trimmed();
    if (!integerRegex.match(value).hasMatch())
        return {};
    return value;
}

bool
isAllowedHrefScheme(QStringView scheme)
{
    const auto lowered                         = scheme.toString().toLower();
    static const QSet<QString> kAllowedSchemes = {
      QStringLiteral("https"),
      QStringLiteral("http"),
      QStringLiteral("ftp"),
      QStringLiteral("mailto"),
      QStringLiteral("magnet"),
    };
    return kAllowedSchemes.contains(lowered);
}

QString
sanitizeHref(const QString &raw)
{
    const auto value = raw.trimmed();
    if (value.isEmpty() || value.size() > kMaxUrlChars)
        return {};

    const auto url = QUrl(value);
    if (!url.isValid() || url.scheme().isEmpty() || url.isRelative())
        return {};
    if (!isAllowedHrefScheme(QStringView{url.scheme()}))
        return {};

    return value;
}

QString
sanitizeMxcUrl(const QString &raw)
{
    const auto value = raw.trimmed();
    if (value.isEmpty() || value.size() > kMaxUrlChars)
        return {};

    const auto url = QUrl(value);
    if (!url.isValid() || url.scheme().compare(QLatin1String("mxc"), Qt::CaseInsensitive) != 0)
        return {};

    const auto afterScheme = value.mid(6);
    if (afterScheme.isEmpty())
        return {};

    return value;
}

QString
sanitizeTarget(const QString &raw)
{
    static const QRegularExpression targetRegex(QStringLiteral(R"(^[A-Za-z0-9_\-]+$)"));
    const auto value = raw.trimmed();
    if (value.isEmpty() || value.size() > kMaxTargetChars)
        return {};
    if (!targetRegex.match(value).hasMatch())
        return {};
    return value;
}

QString
sanitizeCodeClass(const QString &raw)
{
    if (raw.isEmpty() || raw.size() > kMaxClassChars)
        return {};

    static const QRegularExpression tokenRegex(
      QStringLiteral(R"(^language-[A-Za-z0-9#+._-]{1,64}$)"));

    QStringList kept;
    const auto parts = raw.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const auto &part : parts) {
        const auto token = part.trimmed();
        if (tokenRegex.match(token).hasMatch())
            kept.push_back(token);
    }

    return kept.join(QLatin1Char(' '));
}

QString
sanitizeAttributeValue(const QString &tagName, const ParsedAttribute &attr)
{
    if (!attr.hasValue || attr.value.size() > kMaxAttributeValueChars)
        return {};

    const auto value = attr.value;

    if (tagName == QLatin1String("span")) {
        if (attr.name == QLatin1String("data-mx-bg-color"))
            return sanitizeHexColor(value);
        if (attr.name == QLatin1String("data-mx-color"))
            return sanitizeFontColor(value);
        if (attr.name == QLatin1String("data-mx-spoiler"))
            return value.size() <= kMaxSpoilerChars ? value : QString{};
        if (attr.name == QLatin1String("data-mx-maths"))
            return value.size() <= kMaxMathChars ? value : QString{};
        return {};
    }

    if (tagName == QLatin1String("a")) {
        if (attr.name == QLatin1String("href"))
            return sanitizeHref(value);
        if (attr.name == QLatin1String("target"))
            return sanitizeTarget(value);
        return {};
    }

    if (tagName == QLatin1String("img")) {
        if (attr.name == QLatin1String("src"))
            return sanitizeMxcUrl(value);
        if (attr.name == QLatin1String("width") || attr.name == QLatin1String("height"))
            return sanitizeIntegerString(value);
        if (attr.name == QLatin1String("alt") || attr.name == QLatin1String("title"))
            return value.size() <= kMaxTextAttributeChars ? value : QString{};
        return {};
    }

    if (tagName == QLatin1String("ol")) {
        if (attr.name == QLatin1String("start"))
            return sanitizeIntegerString(value);
        return {};
    }

    if (tagName == QLatin1String("code")) {
        if (attr.name == QLatin1String("class"))
            return sanitizeCodeClass(value);
        return {};
    }

    if (tagName == QLatin1String("div")) {
        if (attr.name == QLatin1String("data-mx-maths"))
            return value.size() <= kMaxMathChars ? value : QString{};
        return {};
    }

    if (tagName == QLatin1String("font")) {
        if (attr.name == QLatin1String("color") || attr.name == QLatin1String("data-mx-bg-color") ||
            attr.name == QLatin1String("data-mx-color"))
            return attr.name == QLatin1String("color") ? sanitizeFontColor(value)
                                                       : sanitizeHexColor(value);
        return {};
    }

    return {};
}

QString
sanitizeTagAttributes(const QString &html, const ParsedTag &tag, const QString &tagName)
{
    const auto parsedAttrs = parseAttributes(html, tag);
    if (parsedAttrs.isEmpty())
        return {};

    QString attrs;
    for (const auto &attr : parsedAttrs) {
        const auto sanitized = sanitizeAttributeValue(tagName, attr);
        if (sanitized.isEmpty())
            continue;

        attrs += QLatin1Char(' ');
        attrs += attr.name;
        attrs += QStringLiteral("=\"");
        attrs += sanitized.toHtmlEscaped();
        attrs += QLatin1Char('"');
    }

    return attrs;
}

QString
linkifyTextSegment(const QString &text)
{
    auto out = text;
    out.replace(link_patterns::urlRegex, link_patterns::urlHtml);

    static const QRegularExpression matrixUriRegex(
      QStringLiteral("\\b(?<![\"'])(?>(matrix:[\\S]{5,}))(?![\"'])\\b"));
    out.replace(matrixUriRegex, link_patterns::urlHtml);

    return out;
}

} // namespace

QString
sanitizeHtml(const QString &rawHtml)
{
    if (rawHtml.isEmpty())
        return rawHtml;

    QString out;
    out.reserve(rawHtml.size());

    int pos          = 0;
    int depth        = 0;
    int mxReplyDepth = 0;

    while (pos < rawHtml.size()) {
        const int nextLt = rawHtml.indexOf(QLatin1Char('<'), pos);
        if (nextLt < 0) {
            if (mxReplyDepth == 0)
                out += rawHtml.mid(pos);
            break;
        }

        if (mxReplyDepth == 0)
            out += rawHtml.mid(pos, nextLt - pos);

        const auto tag = parseTag(rawHtml, nextLt);
        if (!tag.valid) {
            if (mxReplyDepth == 0)
                out += QStringLiteral("&lt;");
            pos = nextLt + 1;
            continue;
        }

        const auto tagName = tagNameLower(rawHtml, tag);

        if (tagName == QLatin1String("mx-reply")) {
            if (!tag.isEnd)
                ++mxReplyDepth;
            else if (mxReplyDepth > 0)
                --mxReplyDepth;
            pos = tag.end;
            continue;
        }

        if (mxReplyDepth > 0) {
            pos = tag.end;
            continue;
        }

        if (tag.special || !isAllowedTag(tagName)) {
            out += QStringLiteral("&lt;");
            pos = nextLt + 1;
            continue;
        }

        if (tag.isEnd) {
            out += QStringLiteral("</%1>").arg(tagName);
            if (depth > 0 && !isVoidTag(tagName))
                --depth;
            pos = tag.end;
            continue;
        }

        if (!tag.selfClosing && !isVoidTag(tagName) && depth >= kMaxTagDepth) {
            out += QStringLiteral("&lt;");
            pos = nextLt + 1;
            continue;
        }

        const auto attrs = sanitizeTagAttributes(rawHtml, tag, tagName);
        if (tag.selfClosing || isVoidTag(tagName))
            out += QStringLiteral("<%1%2/>").arg(tagName, attrs);
        else
            out += QStringLiteral("<%1%2>").arg(tagName, attrs);

        if (!tag.selfClosing && !isVoidTag(tagName))
            ++depth;

        pos = tag.end;
    }

    return out;
}

QString
linkifyHtml(const QString &html)
{
    if (html.isEmpty())
        return html;

    QString out;
    out.reserve(html.size() + html.size() / 4);

    int pos         = 0;
    int anchorDepth = 0;
    int preDepth    = 0;
    int codeDepth   = 0;

    while (pos < html.size()) {
        const int nextLt = html.indexOf(QLatin1Char('<'), pos);
        if (nextLt < 0) {
            if (anchorDepth == 0 && preDepth == 0 && codeDepth == 0)
                out += linkifyTextSegment(html.mid(pos));
            else
                out += html.mid(pos);
            break;
        }

        const auto text = html.mid(pos, nextLt - pos);
        if (anchorDepth == 0 && preDepth == 0 && codeDepth == 0)
            out += linkifyTextSegment(text);
        else
            out += text;

        const auto tag = parseTag(html, nextLt);
        if (!tag.valid) {
            out += QLatin1Char('<');
            pos = nextLt + 1;
            continue;
        }

        const auto tagName = tagNameLower(html, tag);
        out += html.mid(tag.start, tag.end - tag.start);

        if (!tag.special) {
            auto updateDepth = [](const ParsedTag &t, const QString &name, int &counter) {
                if (t.selfClosing || isVoidTag(name))
                    return;
                if (t.isEnd) {
                    if (counter > 0)
                        --counter;
                } else {
                    ++counter;
                }
            };

            if (tagName == QLatin1String("a"))
                updateDepth(tag, tagName, anchorDepth);
            else if (tagName == QLatin1String("pre"))
                updateDepth(tag, tagName, preDepth);
            else if (tagName == QLatin1String("code"))
                updateDepth(tag, tagName, codeDepth);
        }

        pos = tag.end;
    }

    return out;
}

namespace {

/// Extract the Matrix ID from a matrix.to href value.
/// Returns the decoded ID (e.g. "@user:server", "#room:server", "!roomid:server")
/// or empty string if the href is not a matrix.to URL.
QString
matrixIdFromHref(const QString &href)
{
    if (!href.startsWith(kMatrixToPrefix))
        return {};

    // The ID is everything after "https://matrix.to/#/", possibly percent-encoded.
    // Strip any query string or extra fragment path segments (e.g. "/$eventid").
    auto fragment = href.mid(kMatrixToPrefix.size());

    // Remove query string if present.
    const int queryPos = fragment.indexOf(QLatin1Char('?'));
    if (queryPos >= 0)
        fragment = fragment.left(queryPos);

    // For event links like "!room:server/$event:server", keep only the first segment.
    const int slashPos = fragment.indexOf(QLatin1Char('/'));
    if (slashPos >= 0)
        fragment = fragment.left(slashPos);

    return QUrl::fromPercentEncoding(fragment.toUtf8());
}

/// Determine the pill CSS class suffix from a Matrix ID sigil.
/// Returns "user", "room", or empty for unrecognized sigils.
QString
pillClassForId(const QString &matrixId)
{
    if (matrixId.isEmpty())
        return {};

    const auto sigil = matrixId.at(0);
    if (sigil == QLatin1Char('@'))
        return QStringLiteral("user");
    if (sigil == QLatin1Char('#') || sigil == QLatin1Char('!'))
        return QStringLiteral("room");

    return {};
}

} // namespace

QString
decorateMatrixPills(const QString &html, const PillAvatarResolver &avatarResolver)
{
    if (html.isEmpty())
        return html;

    QString out;
    out.reserve(html.size() + html.size() / 4);

    int pos = 0;

    while (pos < html.size()) {
        const int nextLt = html.indexOf(QLatin1Char('<'), pos);
        if (nextLt < 0) {
            out += html.mid(pos);
            break;
        }

        out += html.mid(pos, nextLt - pos);

        const auto tag = parseTag(html, nextLt);
        if (!tag.valid) {
            out += QLatin1Char('<');
            pos = nextLt + 1;
            continue;
        }

        const auto tagName = tagNameLower(html, tag);

        // Only process opening <a> tags with matrix.to hrefs.
        if (tagName != QLatin1String("a") || tag.isEnd || tag.selfClosing) {
            out += html.mid(tag.start, tag.end - tag.start);
            pos = tag.end;
            continue;
        }

        const auto attrs = parseAttributes(html, tag);
        QString href;
        for (const auto &attr : attrs) {
            if (attr.name == QLatin1String("href")) {
                href = attr.value;
                break;
            }
        }

        const auto matrixId = matrixIdFromHref(href);
        const auto pillType = pillClassForId(matrixId);

        if (pillType.isEmpty()) {
            // Not a pill-eligible link — emit as-is.
            out += html.mid(tag.start, tag.end - tag.start);
            pos = tag.end;
            continue;
        }

        // Rebuild the <a> tag with pill class.
        out += QStringLiteral("<a href=\"%1\" class=\"pill pill-%2\">")
                 .arg(href.toHtmlEscaped(), pillType);

        // Inject avatar image if the resolver provides one.
        // Display size is controlled by CSS (img.pill-avatar { height: 1.4em }).
        if (avatarResolver) {
            const auto avatarSrc = avatarResolver(matrixId);
            if (!avatarSrc.isEmpty()) {
                out += QStringLiteral("<img class=\"pill-avatar\" src=\"%1\"/>")
                         .arg(avatarSrc.toHtmlEscaped());
            }
        }

        pos = tag.end;
    }

    return out;
}

} // namespace timeline::formattedmessage

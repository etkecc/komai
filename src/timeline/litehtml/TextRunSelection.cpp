// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/litehtml/TextRunSelection.h"

#include <algorithm>

#include <litehtml.h>
#include <litehtml/render_item.h>

namespace timeline::litehtml {

bool
isUrlBreakChar(QChar c)
{
    const ushort u = c.unicode();
    return u == u'/' || u == u'?' || u == u'&' || u == u'=' || u == u'#' || u == u':';
}

bool
isCjkChar(QChar c)
{
    const ushort u = c.unicode();
    return u >= 0x4E00 && u <= 0x9FCC;
}

void
splitText(const char *text,
          const std::function<void(const char *)> &on_word,
          const std::function<void(const char *)> &on_space)
{
    constexpr int kForceBreakAfter = 30;

    const QString str = QString::fromUtf8(text);
    QString word;
    auto flushWord = [&]() {
        if (!word.isEmpty()) {
            on_word(word.toUtf8().constData());
            word.clear();
        }
    };

    for (int i = 0; i < str.size(); ++i) {
        const QChar c  = str[i];
        const ushort u = c.unicode();

        if (u == u' ' || u == u'\t' || u == u'\n' || u == u'\r' || u == u'\f') {
            flushWord();
            const QString sp(c);
            on_space(sp.toUtf8().constData());
        } else if (isCjkChar(c)) {
            // CJK character: each is its own break opportunity.
            flushWord();
            const QString cjk(c);
            on_word(cjk.toUtf8().constData());
        } else {
            word.append(c);
            if (isUrlBreakChar(c)) {
                // Split after the *last* char in a run of URL separators, so
                // "https://" stays as one segment and we break before the next
                // path component instead of between the two slashes.
                const QChar next = (i + 1 < str.size()) ? str[i + 1] : QChar();
                if (next.isNull() || !isUrlBreakChar(next))
                    flushWord();
            } else if (word.size() >= kForceBreakAfter) {
                // Long unbreakable run with no separator chars: force a chunk
                // boundary so e.g. base64 strings or hashes can wrap.
                flushWord();
            }
        }
    }
    flushWord();
}

namespace {

/// Accumulates the logical separator between the last emitted run and the next
/// one while the walk passes through breaks, collapsed spaces, and block
/// boundaries.
struct PendingSeparator
{
    int newlines = 0;
    bool tab     = false;
    bool space   = false;
};

/// The separator an element's boundary (entering or leaving it) contributes
/// between the surrounding runs, mirroring how browsers serialize a selection
/// to plain text: table cells become tabs, paragraph-level blocks get a blank
/// line, any other block a single newline, and inline elements nothing.
PendingSeparator
blockBoundary(const std::shared_ptr<::litehtml::element> &el)
{
    switch (el->css().get_display()) {
    case ::litehtml::display_inline:
    case ::litehtml::display_inline_block:
    case ::litehtml::display_inline_table:
    case ::litehtml::display_inline_text:
    case ::litehtml::display_inline_flex:
        return {};
    case ::litehtml::display_table_cell:
        return {.tab = true};
    default:
        break;
    }

    switch (el->tag()) {
    case ::litehtml::_p_:
    case ::litehtml::_h1_:
    case ::litehtml::_h2_:
    case ::litehtml::_h3_:
    case ::litehtml::_h4_:
    case ::litehtml::_h5_:
    case ::litehtml::_h6_:
    case ::litehtml::_pre_:
        return {.newlines = 2};
    default:
        return {.newlines = 1};
    }
}

void
collectRecurse(const std::shared_ptr<::litehtml::render_item> &ri,
               const QPoint &offset,
               const QFont &fallbackFont,
               QVector<TextRun> &out,
               PendingSeparator &pending)
{
    const auto el = ri->src_el();
    if (!el)
        return;

    const auto &css = el->css();
    if (css.get_display() == ::litehtml::display_none ||
        css.get_visibility() != ::litehtml::visibility_visible)
        return;

    // Hard line breaks: <br> elements and preserved "\n" characters inside
    // white-space:pre/pre-wrap content. They carry no drawable text and are
    // often skip()-marked by line_box, so handle them before any visibility
    // filtering. Each one is a newline of its own — consecutive breaks keep
    // blank lines intact.
    if (el->is_break()) {
        if (!out.isEmpty())
            ++pending.newlines;
        return;
    }

    if (el->is_text()) {
        std::string utf8;
        el->get_text(utf8);
        QString text = QString::fromStdString(utf8);

        const auto place = ri->get_placement();

        if (el->is_space()) {
            // Mirror el_text::compute_styles: collapsible whitespace renders
            // as a single space; a tab preserved by pre-wrap renders as four
            // spaces. The run text must match what is painted so hit-testing
            // by font metrics lines up.
            if (el->is_white_space())
                text = QStringLiteral(" ");
            else if (text == QStringLiteral("\t"))
                text = QStringLiteral("    ");

            if (ri->skip() || qRound(place.width) <= 0 || qRound(place.height) <= 0) {
                // Never painted: either collapsed at a line boundary
                // (skip()-marked) or left without a laid-out size, like the
                // source-formatting whitespace between table rows. Still a
                // word separator logically, but it must not become a run —
                // a zero-size rect would pollute hit-testing.
                pending.space = true;
                return;
            }
        }

        if (text.isEmpty())
            return;

        TextRun run;
        run.text = text;
        run.rect = QRect(qRound(place.x) + offset.x(),
                         qRound(place.y) + offset.y(),
                         qRound(place.width),
                         qRound(place.height));

        auto *font         = reinterpret_cast<QFont *>(css.get_font());
        run.font           = font ? *font : fallbackFont;
        run.newlinesBefore = pending.newlines;
        run.tabBefore      = pending.newlines == 0 && pending.tab;
        run.spaceBefore    = pending.newlines == 0 && !pending.tab && pending.space;
        pending            = {};
        out.append(run);
        return;
    }

    const auto boundary = blockBoundary(el);
    if (!out.isEmpty()) {
        pending.newlines = std::max(pending.newlines, boundary.newlines);
        pending.tab      = pending.tab || boundary.tab;
    }

    for (const auto &child : ri->children())
        collectRecurse(child, offset, fallbackFont, out, pending);

    if (!out.isEmpty()) {
        pending.newlines = std::max(pending.newlines, boundary.newlines);
        pending.tab      = pending.tab || boundary.tab;
    }
}

} // namespace

void
collectTextRuns(const std::shared_ptr<::litehtml::render_item> &root,
                const QPoint &offset,
                const QFont &fallbackFont,
                QVector<TextRun> &out)
{
    out.clear();
    if (!root)
        return;

    PendingSeparator pending;
    collectRecurse(root, offset, fallbackFont, out, pending);
}

QString
extractTextRange(const QVector<TextRun> &runs,
                 const SelectionPoint &start,
                 const SelectionPoint &end)
{
    if (!start.isValid() || !end.isValid())
        return {};
    if (start.runIndex >= runs.size() || end.runIndex >= runs.size())
        return {};
    if (start == end)
        return {};

    QString result;
    for (int i = start.runIndex; i <= end.runIndex; ++i) {
        const auto &run = runs[i];

        // Only logical separators are reproduced; soft wraps introduced by
        // layout contribute nothing, so a wrapped URL comes out in one piece.
        if (i > start.runIndex && !result.isEmpty()) {
            if (run.newlinesBefore > 0)
                result += QString(run.newlinesBefore, u'\n');
            else if (run.tabBefore)
                result += u'\t';
            else if (run.spaceBefore)
                result += u' ';
        }

        // Prepend list marker prefix (e.g. "- ") if this is the first run of a list item.
        if (!run.prefix.isEmpty() && (i != start.runIndex || start.charOffset == 0))
            result += run.prefix;

        if (i == start.runIndex && i == end.runIndex) {
            result += run.text.mid(start.charOffset, end.charOffset - start.charOffset);
        } else if (i == start.runIndex) {
            result += run.text.mid(start.charOffset);
        } else if (i == end.runIndex) {
            result += run.text.left(end.charOffset);
        } else {
            result += run.text;
        }
    }

    return result;
}

} // namespace timeline::litehtml

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Verifies that text-selection extraction reproduces the logical text of a
// rendered message: line breaks introduced by soft wrapping (long URLs split
// to fit a narrow bubble, wraps at spaces) must not leak into the copied text,
// while real line breaks (<br>, paragraphs, newlines in code blocks) must be
// preserved (issue #280).

#include <iostream>
#include <memory>
#include <set>
#include <string_view>

#include <QFont>
#include <QGuiApplication>
#include <QPoint>
#include <QString>

#include <litehtml.h>

#include "timeline/litehtml/TextRunSelection.h"

namespace {

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

/// Minimal document_container with deterministic fixed-width text metrics
/// (every character 10px wide), so wrap points don't depend on the fonts
/// available on the test machine. Uses the real Komai tokenizer, which is the
/// piece that splits long URLs into wrappable segments.
class FixedWidthContainer : public litehtml::document_container
{
public:
    litehtml::uint_ptr create_font(const litehtml::font_description & /*descr*/,
                                   const litehtml::document * /*doc*/,
                                   litehtml::font_metrics *fm) override
    {
        if (fm) {
            fm->font_size   = 10;
            fm->height      = 10;
            fm->ascent      = 8;
            fm->descent     = 2;
            fm->x_height    = 5;
            fm->ch_width    = 10;
            fm->draw_spaces = true;
        }
        return reinterpret_cast<litehtml::uint_ptr>(new QFont());
    }
    void delete_font(litehtml::uint_ptr hFont) override
    {
        delete reinterpret_cast<QFont *>(hFont);
    }
    litehtml::pixel_t text_width(const char *text, litehtml::uint_ptr /*hFont*/) override
    {
        return static_cast<litehtml::pixel_t>(QString::fromUtf8(text).length() * 10);
    }
    void split_text(const char *text,
                    const std::function<void(const char *)> &on_word,
                    const std::function<void(const char *)> &on_space) override
    {
        timeline::litehtml::splitText(text, on_word, on_space);
    }
    void draw_text(litehtml::uint_ptr,
                   const char *,
                   litehtml::uint_ptr,
                   litehtml::web_color,
                   const litehtml::position &) override
    {
    }
    litehtml::pixel_t pt_to_px(float pt) const override
    {
        return static_cast<litehtml::pixel_t>(pt * 96.0 / 72.0);
    }
    litehtml::pixel_t get_default_font_size() const override { return 10; }
    const char *get_default_font_name() const override { return "test"; }
    void draw_list_marker(litehtml::uint_ptr, const litehtml::list_marker &) override {}
    void load_image(const char *, const char *, bool) override {}
    void get_image_size(const char *, const char *, litehtml::size &sz) override
    {
        sz.width  = 0;
        sz.height = 0;
    }
    void draw_image(litehtml::uint_ptr,
                    const litehtml::background_layer &,
                    const std::string &,
                    const std::string &) override
    {
    }
    void draw_solid_fill(litehtml::uint_ptr,
                         const litehtml::background_layer &,
                         const litehtml::web_color &) override
    {
    }
    void draw_linear_gradient(litehtml::uint_ptr,
                              const litehtml::background_layer &,
                              const litehtml::background_layer::linear_gradient &) override
    {
    }
    void draw_radial_gradient(litehtml::uint_ptr,
                              const litehtml::background_layer &,
                              const litehtml::background_layer::radial_gradient &) override
    {
    }
    void draw_conic_gradient(litehtml::uint_ptr,
                             const litehtml::background_layer &,
                             const litehtml::background_layer::conic_gradient &) override
    {
    }
    void
    draw_borders(litehtml::uint_ptr, const litehtml::borders &, const litehtml::position &, bool)
      override
    {
    }
    void set_caption(const char *) override {}
    void set_base_url(const char *) override {}
    void link(const std::shared_ptr<litehtml::document> &, const litehtml::element::ptr &) override
    {
    }
    void on_anchor_click(const char *, const litehtml::element::ptr &) override {}
    void on_mouse_event(const litehtml::element::ptr &, litehtml::mouse_event) override {}
    void set_cursor(const char *) override {}
    void transform_text(litehtml::string &, litehtml::text_transform) override {}
    void import_css(litehtml::string &, const litehtml::string &, litehtml::string &) override {}
    void set_clip(const litehtml::position &, const litehtml::border_radiuses &) override {}
    void del_clip() override {}
    void get_viewport(litehtml::position &viewport) const override
    {
        viewport = litehtml::position(0, 0, 800, 600);
    }
    litehtml::element::ptr create_element(const char *,
                                          const litehtml::string_map &,
                                          const std::shared_ptr<litehtml::document> &) override
    {
        return nullptr;
    }
    void get_media_features(litehtml::media_features &media) const override
    {
        media.type          = litehtml::media_type_screen;
        media.width         = 800;
        media.height        = 600;
        media.device_width  = 800;
        media.device_height = 600;
        media.color         = 8;
        media.resolution    = 96;
    }
    void get_language(litehtml::string &language, litehtml::string &culture) const override
    {
        language = "en";
        culture  = "";
    }
};

QVector<TextRun>
renderRuns(const char *html, int width, const char *userCss = "")
{
    FixedWidthContainer container;
    auto doc = litehtml::document::createFromString(html, &container, litehtml::master_css, userCss);
    doc->render(static_cast<litehtml::pixel_t>(width));

    QVector<TextRun> runs;
    timeline::litehtml::collectTextRuns(doc->root_render(), QPoint(0, 0), QFont(), runs);
    return runs;
}

QString
extractAll(const QVector<TextRun> &runs)
{
    if (runs.isEmpty())
        return {};
    return timeline::litehtml::extractTextRange(
      runs, {0, 0}, {static_cast<int>(runs.size()) - 1, static_cast<int>(runs.last().text.length())});
}

int
distinctLineCount(const QVector<TextRun> &runs)
{
    std::set<int> ys;
    for (const auto &run : runs)
        ys.insert(run.rect.y());
    return static_cast<int>(ys.size());
}

bool
testWrappedUrlCopiesUnbroken()
{
    // 20 characters per line: the URL cannot fit on one line and litehtml
    // wraps it at the segments produced by splitText.
    const auto runs = renderRuns("<p>see https://example.com/aaa/bbb/ccc now</p>", 200);

    bool ok = true;
    ok &= expect(distinctLineCount(runs) > 1, "URL message actually wrapped onto multiple lines");
    ok &= expect(extractAll(runs) == QStringLiteral("see https://example.com/aaa/bbb/ccc now"),
                 "wrapped URL is copied without line breaks");
    return ok;
}

bool
testWordWrapPreservesSpace()
{
    const auto runs = renderRuns("<p>alpha beta gamma delta</p>", 120);

    bool ok = true;
    ok &= expect(distinctLineCount(runs) > 1, "word-wrapped message spans multiple lines");
    ok &= expect(extractAll(runs) == QStringLiteral("alpha beta gamma delta"),
                 "space eaten at a wrap point is restored as a space, not a newline");
    return ok;
}

bool
testForcedBreakInLongTokenCopiesUnbroken()
{
    // 40 unbreakable characters: splitText force-splits every 30 chars and
    // the segments wrap at 20 chars per line.
    const auto runs = renderRuns("<p>abcdefghijklmnopqrstuvwxyz0123456789abcd</p>", 200);

    bool ok = true;
    ok &= expect(distinctLineCount(runs) > 1, "long token wrapped onto multiple lines");
    ok &= expect(extractAll(runs) == QStringLiteral("abcdefghijklmnopqrstuvwxyz0123456789abcd"),
                 "force-split token is copied without line breaks");
    return ok;
}

bool
testBrIsHardNewline()
{
    const auto runs = renderRuns("<p>one<br>two</p>", 800);
    return expect(extractAll(runs) == QStringLiteral("one\ntwo"),
                  "<br> is copied as a newline");
}

bool
testDoubleBrKeepsBlankLine()
{
    const auto runs = renderRuns("<p>one<br><br>two</p>", 800);
    return expect(extractAll(runs) == QStringLiteral("one\n\ntwo"),
                  "consecutive <br>s keep the blank line");
}

bool
testParagraphsJoinWithBlankLine()
{
    const auto runs = renderRuns("<p>one</p><p>two</p>", 800);
    return expect(extractAll(runs) == QStringLiteral("one\n\ntwo"),
                  "paragraph boundary is copied as a blank line, like browsers");
}

bool
testHeadingJoinsWithBlankLine()
{
    const auto runs = renderRuns("<h1>title</h1><p>body</p>", 800);
    return expect(extractAll(runs) == QStringLiteral("title\n\nbody"),
                  "heading boundary is copied as a blank line");
}

bool
testListItemsJoinWithSingleNewline()
{
    const auto runs = renderRuns("<ul><li>one</li><li>two</li></ul>", 800);
    return expect(extractAll(runs) == QStringLiteral("one\ntwo"),
                  "list items are copied one per line without blank lines");
}

bool
testTableCopiesTabsBetweenCellsNewlinesBetweenRows()
{
    const auto runs = renderRuns(
      "<table><tr><td>A1</td><td>B1</td></tr><tr><td>A2</td><td>B2</td></tr></table>", 800);
    return expect(extractAll(runs) == QStringLiteral("A1\tB1\nA2\tB2"),
                  "table cells separate with tabs, rows with newlines");
}

bool
testMarkdownTableHtmlIgnoresFormattingWhitespace()
{
    // The shape ruma's markdown conversion emits: thead/th plus raw newlines
    // between rows. That formatting whitespace never gets laid out and must
    // not leak into runs or the copied text.
    const auto runs =
      renderRuns("<table><thead><tr><th>Column 1</th><th>Column 2</th></tr></thead><tbody>\n"
                 "<tr><td>Data 1</td><td>Data 2</td></tr>\n"
                 "</tbody></table>\n",
                 800);
    return expect(extractAll(runs) == QStringLiteral("Column 1\tColumn 2\nData 1\tData 2"),
                  "markdown-shaped table copies as tab-separated rows");
}

bool
testPreWrapKeepsRealNewlinesOnly()
{
    const auto runs = renderRuns("<pre>short\nabcdefghijklmnopqrstuvwxyz0123456789abcd</pre>",
                                 200,
                                 "pre { white-space: pre-wrap }");

    bool ok = true;
    ok &= expect(distinctLineCount(runs) > 2, "long code line wrapped onto multiple lines");
    ok &= expect(extractAll(runs) ==
                   QStringLiteral("short\nabcdefghijklmnopqrstuvwxyz0123456789abcd"),
                 "code block keeps its real newline but not the soft wraps");
    return ok;
}

bool
testPartialRangeRespectsCharOffsets()
{
    const auto runs = renderRuns("<p>hello world</p>", 800);
    // Runs are ["hello", " ", "world"]; slice from inside the first word to
    // inside the last.
    if (!expect(runs.size() == 3, "simple message produces word, space, word runs"))
        return false;

    const auto text = timeline::litehtml::extractTextRange(runs, {0, 1}, {2, 3});
    return expect(text == QStringLiteral("ello wor"), "partial selection honors char offsets");
}

} // namespace

int
main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    bool ok = true;
    ok &= testWrappedUrlCopiesUnbroken();
    ok &= testWordWrapPreservesSpace();
    ok &= testForcedBreakInLongTokenCopiesUnbroken();
    ok &= testBrIsHardNewline();
    ok &= testDoubleBrKeepsBlankLine();
    ok &= testParagraphsJoinWithBlankLine();
    ok &= testHeadingJoinsWithBlankLine();
    ok &= testListItemsJoinWithSingleNewline();
    ok &= testTableCopiesTabsBetweenCellsNewlinesBetweenRows();
    ok &= testMarkdownTableHtmlIgnoresFormattingWhitespace();
    ok &= testPreWrapKeepsRealNewlinesOnly();
    ok &= testPartialRangeRespectsCharOffsets();

    return ok ? 0 : 1;
}

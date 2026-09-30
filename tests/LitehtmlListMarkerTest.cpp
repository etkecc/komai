// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Verifies that ordered-list markers fit inside the list's left padding.
// litehtml draws "outside" markers right-aligned just left of the item's
// content box, so a marker wider than the padding starts at a negative x and
// its leading digits are clipped by the message item: "10." renders as "0.".

#include <iostream>
#include <memory>
#include <string_view>

#include <QFont>
#include <QGuiApplication>
#include <QPalette>
#include <QString>
#include <QVector>

#include <litehtml.h>

#include "timeline/litehtml/LitehtmlStylesheet.h"
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

struct DrawnText
{
    QString text;
    litehtml::pixel_t x;
};

/// document_container with deterministic proportional text metrics that
/// approximate a typical sans-serif UI font: digits and letters are 0.55em
/// wide, periods and spaces 0.3em. Records every draw_text call so marker
/// positions can be inspected.
class ProportionalContainer : public litehtml::document_container
{
public:
    QVector<DrawnText> drawn;

    litehtml::uint_ptr create_font(const litehtml::font_description &descr,
                                   const litehtml::document * /*doc*/,
                                   litehtml::font_metrics *fm) override
    {
        auto *size = new litehtml::pixel_t(descr.size);
        if (fm) {
            fm->font_size   = descr.size;
            fm->height      = descr.size * 1.2f;
            fm->ascent      = descr.size * 0.95f;
            fm->descent     = descr.size * 0.25f;
            fm->x_height    = descr.size * 0.5f;
            fm->ch_width    = descr.size * 0.55f;
            fm->draw_spaces = true;
        }
        return reinterpret_cast<litehtml::uint_ptr>(size);
    }
    void delete_font(litehtml::uint_ptr hFont) override
    {
        delete reinterpret_cast<litehtml::pixel_t *>(hFont);
    }
    litehtml::pixel_t text_width(const char *text, litehtml::uint_ptr hFont) override
    {
        const auto size        = *reinterpret_cast<litehtml::pixel_t *>(hFont);
        litehtml::pixel_t width = 0;
        for (const QChar ch : QString::fromUtf8(text))
            width += (ch == QLatin1Char('.') || ch == QLatin1Char(' ')) ? size * 0.3f
                                                                          : size * 0.55f;
        return width;
    }
    void split_text(const char *text,
                    const std::function<void(const char *)> &on_word,
                    const std::function<void(const char *)> &on_space) override
    {
        timeline::litehtml::splitText(text, on_word, on_space);
    }
    void draw_text(litehtml::uint_ptr,
                   const char *text,
                   litehtml::uint_ptr,
                   litehtml::web_color,
                   const litehtml::position &pos) override
    {
        drawn.append({QString::fromUtf8(text), pos.x});
    }
    litehtml::pixel_t pt_to_px(float pt) const override
    {
        return static_cast<litehtml::pixel_t>(qRound(pt * 96.0 / 72.0));
    }
    litehtml::pixel_t get_default_font_size() const override { return 15; }
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

/// Renders `html` with Komai's message stylesheet and returns the drawn
/// ordered-list markers (text runs ending in ".") with their left edges.
QVector<DrawnText>
drawnMarkers(const char *html, bool compact)
{
    QFont font(QStringLiteral("test"));
    font.setPointSize(11);
    const auto css = timeline::litehtml::generateMasterStylesheet(QPalette(),
                                                                  font,
                                                                  compact,
                                                                  QStringLiteral("#ff0000"),
                                                                  QStringLiteral("#ffaa00"),
                                                                  QStringLiteral("#00ff00"),
                                                                  QStringLiteral("#ffff00"),
                                                                  QStringLiteral("#000000"),
                                                                  QStringLiteral("#eeeeee"))
                       .toStdString();

    ProportionalContainer container;
    auto doc =
      litehtml::document::createFromString(html, &container, litehtml::master_css, css.c_str());
    doc->render(600);
    doc->draw(0, 0, 0, nullptr);

    QVector<DrawnText> markers;
    for (const auto &text : container.drawn) {
        if (text.text.endsWith(QLatin1Char('.')))
            markers.append(text);
    }
    return markers;
}

bool
testMarkersFitInsidePadding(const char *html, const QStringList &expected, std::string_view name)
{
    bool ok = true;
    for (const bool compact : {false, true}) {
        const auto markers = drawnMarkers(html, compact);

        QStringList texts;
        for (const auto &marker : markers)
            texts.append(marker.text);
        ok &= expect(texts == expected,
                     std::string(name) + ": all markers are drawn with their numbers");

        for (const auto &marker : markers) {
            ok &= expect(marker.x >= 0,
                         std::string(name) + ": marker \"" + marker.text.toStdString() +
                           "\" starts inside the message (x=" + std::to_string(marker.x) +
                           (compact ? ", compact)" : ")"));
        }
    }
    return ok;
}

bool
testSingleDigitMarkers()
{
    return testMarkersFitInsidePadding(
      "<ol><li>apple</li><li>banana</li><li>cherry</li></ol>",
      {QStringLiteral("1."), QStringLiteral("2."), QStringLiteral("3.")},
      "single-digit list");
}

bool
testStartAttributeCrossingIntoTwoDigits()
{
    return testMarkersFitInsidePadding(
      "<ol start=\"9\"><li>apple</li><li>banana</li><li>cherry</li></ol>",
      {QStringLiteral("9."), QStringLiteral("10."), QStringLiteral("11.")},
      "list starting at 9");
}

bool
testLongListReachesTwoDigits()
{
    return testMarkersFitInsidePadding(
      "<ol><li>a</li><li>b</li><li>c</li><li>d</li><li>e</li><li>f</li>"
      "<li>g</li><li>h</li><li>i</li><li>j</li><li>k</li><li>l</li></ol>",
      {QStringLiteral("1."),
       QStringLiteral("2."),
       QStringLiteral("3."),
       QStringLiteral("4."),
       QStringLiteral("5."),
       QStringLiteral("6."),
       QStringLiteral("7."),
       QStringLiteral("8."),
       QStringLiteral("9."),
       QStringLiteral("10."),
       QStringLiteral("11."),
       QStringLiteral("12.")},
      "twelve-item list");
}

bool
testNearlyThreeDigitMarkers()
{
    return testMarkersFitInsidePadding(
      "<ol start=\"98\"><li>apple</li><li>banana</li></ol>",
      {QStringLiteral("98."), QStringLiteral("99.")},
      "list starting at 98");
}

} // namespace

int
main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    bool ok = true;
    ok &= testSingleDigitMarkers();
    ok &= testStartAttributeCrossingIntoTwoDigits();
    ok &= testLongListReachesTwoDigits();
    ok &= testNearlyThreeDigitMarkers();

    return ok ? 0 : 1;
}

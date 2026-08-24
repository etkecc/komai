// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QPainter>
#include <QRect>
#include <QSet>
#include <QString>
#include <QVector>

#include <litehtml.h>

#include "timeline/litehtml/TextRunSelection.h"

/// A list marker position captured during paint.
struct ListMarker
{
    QRect rect;
    QString prefix; ///< e.g. "- " for bullet lists
};

/// QPainter-based implementation of litehtml::document_container.
/// Draws HTML content using QPainter and manages font/image resources.
class LitehtmlContainer
  : public QObject
  , public litehtml::document_container
{
    Q_OBJECT

public:
    explicit LitehtmlContainer(QObject *parent = nullptr);

    void setPainter(QPainter *painter) { m_painter = painter; }
    void setViewportSize(int w, int h)
    {
        m_viewportWidth  = w;
        m_viewportHeight = h;
    }
    void setDefaultFont(const QFont &font) { m_defaultFont = font; }
    void setDefaultColor(const QColor &color) { m_defaultColor = color; }
    void setEmojiFontFamily(const QString &family) { m_emojiFontFamily = family; }
    void clearImageCache()
    {
        m_imageCache.clear();
        m_inFlight.clear();
    }

    // litehtml::document_container interface
    litehtml::uint_ptr create_font(const litehtml::font_description &descr,
                                   const litehtml::document *doc,
                                   litehtml::font_metrics *fm) override;
    void delete_font(litehtml::uint_ptr hFont) override;
    litehtml::pixel_t text_width(const char *text, litehtml::uint_ptr hFont) override;
    void split_text(const char *text,
                    const std::function<void(const char *)> &on_word,
                    const std::function<void(const char *)> &on_space) override;
    void draw_text(litehtml::uint_ptr hdc,
                   const char *text,
                   litehtml::uint_ptr hFont,
                   litehtml::web_color color,
                   const litehtml::position &pos) override;
    litehtml::pixel_t pt_to_px(float pt) const override;
    litehtml::pixel_t get_default_font_size() const override;
    const char *get_default_font_name() const override;
    void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker &marker) override;
    void load_image(const char *src, const char *baseurl, bool redraw_on_ready) override;
    void get_image_size(const char *src, const char *baseurl, litehtml::size &sz) override;
    void draw_image(litehtml::uint_ptr hdc,
                    const litehtml::background_layer &layer,
                    const std::string &url,
                    const std::string &base_url) override;
    void draw_solid_fill(litehtml::uint_ptr hdc,
                         const litehtml::background_layer &layer,
                         const litehtml::web_color &color) override;
    void draw_linear_gradient(litehtml::uint_ptr hdc,
                              const litehtml::background_layer &layer,
                              const litehtml::background_layer::linear_gradient &gradient) override;
    void draw_radial_gradient(litehtml::uint_ptr hdc,
                              const litehtml::background_layer &layer,
                              const litehtml::background_layer::radial_gradient &gradient) override;
    void draw_conic_gradient(litehtml::uint_ptr hdc,
                             const litehtml::background_layer &layer,
                             const litehtml::background_layer::conic_gradient &gradient) override;
    void draw_borders(litehtml::uint_ptr hdc,
                      const litehtml::borders &borders,
                      const litehtml::position &draw_pos,
                      bool root) override;
    void set_caption(const char *caption) override;
    void set_base_url(const char *base_url) override;
    void
    link(const std::shared_ptr<litehtml::document> &doc, const litehtml::element::ptr &el) override;
    void on_anchor_click(const char *url, const litehtml::element::ptr &el) override;
    void on_mouse_event(const litehtml::element::ptr &el, litehtml::mouse_event event) override;
    void set_cursor(const char *cursor) override;
    void transform_text(litehtml::string &text, litehtml::text_transform tt) override;
    void import_css(litehtml::string &text,
                    const litehtml::string &url,
                    litehtml::string &baseurl) override;
    void
    set_clip(const litehtml::position &pos, const litehtml::border_radiuses &bdr_radius) override;
    void del_clip() override;
    void get_viewport(litehtml::position &viewport) const override;
    litehtml::element::ptr create_element(const char *tag_name,
                                          const litehtml::string_map &attributes,
                                          const std::shared_ptr<litehtml::document> &doc) override;
    void get_media_features(litehtml::media_features &media) const override;
    void get_language(litehtml::string &language, litehtml::string &culture) const override;

    /// Enter hover mode: on_anchor_click stores the URL instead of emitting linkClicked.
    void setHoverMode(bool hover)
    {
        m_hoverMode = hover;
        m_lastHoveredUrl.clear();
    }
    QString lastHoveredUrl() const { return m_lastHoveredUrl; }
    bool isPointerCursor() const { return m_pointerCursor; }
    void resetCursorState() { m_pointerCursor = false; }

    /// Text run collection for selection support. begin/end bracket a
    /// document draw: list markers are captured while painting (they are not
    /// part of the render tree's text), then endTextRunCollection() walks the
    /// laid-out render tree to build the text runs in document order and
    /// matches the captured markers onto them. `offset` translates document
    /// coordinates into item coordinates (left padding / top inset).
    void beginTextRunCollection()
    {
        m_textRuns.clear();
        m_listMarkers.clear();
        m_collectingTextRuns = true;
    }
    void
    endTextRunCollection(const std::shared_ptr<litehtml::render_item> &root, const QPoint &offset);
    const QVector<TextRun> &textRuns() const { return m_textRuns; }

signals:
    void linkClicked(const QString &url);
    void imageLoaded();

private:
    static QColor toQColor(const litehtml::web_color &c);
    void drawBorderLine(const QPoint &from, const QPoint &to, const litehtml::border &border);
    void loadMxcImage(const QString &srcUrl);
    /// Render an `image://default-avatar/...` URL via DefaultAvatarRunnable
    /// and insert the result into `m_imageCache` keyed by `cacheKey`. When
    /// `cacheKey` differs from `defaultAvatarUrl`, this is being used as the
    /// pre-emptive fallback for an mxc URL: the mxc download is in flight
    /// in parallel and is allowed to overwrite the cache entry on success.
    void loadDefaultAvatarImage(const QString &defaultAvatarUrl, const QString &cacheKey);

    QPainter *m_painter = nullptr;
    QFont m_defaultFont;
    QColor m_defaultColor;
    QString m_emojiFontFamily;
    QByteArray m_defaultFontNameUtf8;
    int m_viewportWidth  = 0;
    int m_viewportHeight = 0;
    QHash<QString, QImage> m_imageCache;
    /// URLs currently being loaded. Prevents the same image from being
    /// re-fetched on every layout pass while the first request is still in
    /// flight (Avatar.qml has its own dedup; litehtml doesn't, so we add it
    /// here to avoid re-queueing dozens of fetches for the same hung mxc URL
    /// before the first one's deadline expires). Cleared alongside
    /// `m_imageCache` because a cache reset means we want the next
    /// `load_image` call to actually do work.
    QSet<QString> m_inFlight;
    QList<QRect> m_clips;
    bool m_hoverMode          = false;
    bool m_pointerCursor      = false;
    bool m_collectingTextRuns = false;
    QString m_lastHoveredUrl;
    QVector<TextRun> m_textRuns;
    QVector<ListMarker> m_listMarkers;
};

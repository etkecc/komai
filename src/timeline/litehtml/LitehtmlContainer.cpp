// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/litehtml/LitehtmlContainer.h"

#include <algorithm>

#include <QPen>
#include <QUrl>
#include <QtMath>

#include "providers/MxcImageProvider.h"

LitehtmlContainer::LitehtmlContainer(QObject *parent)
  : QObject(parent)
{
}

QColor
LitehtmlContainer::toQColor(const litehtml::web_color &c)
{
    return QColor(c.red, c.green, c.blue, c.alpha);
}

// -- Font management --

litehtml::uint_ptr
LitehtmlContainer::create_font(const char *faceName,
                               int size,
                               int weight,
                               litehtml::font_style italic,
                               unsigned int decoration,
                               litehtml::font_metrics *fm)
{
    auto *font = new QFont();

    if (faceName && *faceName)
        font->setFamily(QString::fromUtf8(faceName));
    else
        font->setFamily(m_defaultFont.family());

    font->setPixelSize(size);

    // Map CSS weight (100-900) to QFont weight.
    // litehtml passes values like 400 (normal), 700 (bold).
    font->setWeight(static_cast<QFont::Weight>(qBound(1, weight, 1000)));

    font->setItalic(italic == litehtml::font_style_italic);
    font->setUnderline(decoration & litehtml::font_decoration_underline);
    font->setStrikeOut(decoration & litehtml::font_decoration_linethrough);
    font->setOverline(decoration & litehtml::font_decoration_overline);

    if (fm) {
        QFontMetrics metrics(*font);
        bool isEmojiFont = !m_emojiFontFamily.isEmpty() && faceName &&
                           QString::fromUtf8(faceName) == m_emojiFontFamily;
        if (isEmojiFont) {
            // For emoji spans, report the default font's metrics so litehtml
            // keeps normal line height and baseline alignment.
            QFontMetrics dfm(m_defaultFont);
            fm->ascent   = dfm.ascent();
            fm->descent  = dfm.descent();
            fm->height   = dfm.height();
            fm->x_height = dfm.xHeight();
        } else {
            fm->ascent   = metrics.ascent();
            fm->descent  = metrics.descent();
            fm->height   = metrics.height();
            fm->x_height = metrics.xHeight();
        }
        fm->draw_spaces = true;
    }

    return reinterpret_cast<litehtml::uint_ptr>(font);
}

void
LitehtmlContainer::delete_font(litehtml::uint_ptr hFont)
{
    delete reinterpret_cast<QFont *>(hFont);
}

int
LitehtmlContainer::text_width(const char *text, litehtml::uint_ptr hFont)
{
    auto *font = reinterpret_cast<QFont *>(hFont);
    if (!font)
        return 0;
    QFontMetrics metrics(*font);
    return metrics.horizontalAdvance(QString::fromUtf8(text));
}

void
LitehtmlContainer::draw_text(litehtml::uint_ptr /*hdc*/,
                             const char *text,
                             litehtml::uint_ptr hFont,
                             litehtml::web_color color,
                             const litehtml::position &pos)
{
    if (!m_painter)
        return;

    auto *font = reinterpret_cast<QFont *>(hFont);
    if (!font)
        return;

    if (m_collectingTextRuns) {
        TextRun run;
        run.text = QString::fromUtf8(text);
        run.rect = QRect(pos.x, pos.y, pos.width, pos.height);
        run.font = *font;
        m_textRuns.append(run);
    }

    m_painter->setFont(*font);
    m_painter->setPen(toQColor(color));
    m_painter->drawText(QRect(pos.x, pos.y, pos.width, pos.height), 0, QString::fromUtf8(text));
}

void
LitehtmlContainer::endTextRunCollection()
{
    m_collectingTextRuns = false;

    // Sort text runs into visual order (top-to-bottom, left-to-right).
    // litehtml draws list markers and some decorations after content text,
    // so the raw draw order doesn't match document reading order.
    std::sort(m_textRuns.begin(), m_textRuns.end(), [](const TextRun &a, const TextRun &b) {
        // Group by line: treat runs as same-line if their Y ranges overlap.
        int aMid      = a.rect.y() + a.rect.height() / 2;
        int bMid      = b.rect.y() + b.rect.height() / 2;
        bool sameLine = (aMid >= b.rect.top() && aMid <= b.rect.bottom()) ||
                        (bMid >= a.rect.top() && bMid <= a.rect.bottom());
        if (!sameLine)
            return a.rect.y() < b.rect.y();
        return a.rect.x() < b.rect.x();
    });

    // Match bullet list markers to text runs by Y-coordinate overlap.
    // Bullet markers are not drawn via draw_text, so they need separate handling.
    for (const auto &marker : m_listMarkers) {
        int markerCenterY = marker.rect.y() + marker.rect.height() / 2;
        for (auto &run : m_textRuns) {
            if (!run.prefix.isEmpty())
                continue;
            if (markerCenterY >= run.rect.top() && markerCenterY <= run.rect.bottom()) {
                run.prefix = marker.prefix;
                break;
            }
        }
    }
}

int
LitehtmlContainer::pt_to_px(int pt) const
{
    // Match Qt's standard conversion factor (11pt -> ~14.67px at 96dpi).
    return qRound(pt * 96.0 / 72.0);
}

int
LitehtmlContainer::get_default_font_size() const
{
    return pt_to_px(qRound(m_defaultFont.pointSizeF()));
}

const char *
LitehtmlContainer::get_default_font_name() const
{
    // Store the UTF8 so the pointer stays valid.
    const_cast<LitehtmlContainer *>(this)->m_defaultFontNameUtf8 = m_defaultFont.family().toUtf8();
    return m_defaultFontNameUtf8.constData();
}

void
LitehtmlContainer::draw_list_marker(litehtml::uint_ptr /*hdc*/, const litehtml::list_marker &marker)
{
    if (!m_painter)
        return;

    // Collect marker positions; matched to text runs in endTextRunCollection().
    if (m_collectingTextRuns) {
        bool isBullet = marker.marker_type == litehtml::list_style_type_disc ||
                        marker.marker_type == litehtml::list_style_type_circle ||
                        marker.marker_type == litehtml::list_style_type_square;
        if (isBullet) {
            ListMarker lm;
            lm.rect   = QRect(marker.pos.x, marker.pos.y, marker.pos.width, marker.pos.height);
            lm.prefix = QStringLiteral("- ");
            m_listMarkers.append(lm);
        }
    }

    if (!marker.image.empty()) {
        const auto key = QString::fromStdString(marker.image);
        if (m_imageCache.contains(key)) {
            m_painter->drawImage(
              QRect(marker.pos.x, marker.pos.y, marker.pos.width, marker.pos.height),
              m_imageCache[key]);
            return;
        }
    }

    switch (marker.marker_type) {
    case litehtml::list_style_type_circle: {
        m_painter->setPen(toQColor(marker.color));
        m_painter->setBrush(Qt::NoBrush);
        m_painter->drawEllipse(marker.pos.x, marker.pos.y, marker.pos.width, marker.pos.height);
        break;
    }
    case litehtml::list_style_type_disc: {
        m_painter->setPen(Qt::NoPen);
        m_painter->setBrush(toQColor(marker.color));
        m_painter->drawEllipse(marker.pos.x, marker.pos.y, marker.pos.width, marker.pos.height);
        break;
    }
    case litehtml::list_style_type_square: {
        m_painter->setPen(Qt::NoPen);
        m_painter->setBrush(toQColor(marker.color));
        m_painter->drawRect(marker.pos.x, marker.pos.y, marker.pos.width, marker.pos.height);
        break;
    }
    default:
        break;
    }
}

// -- Image loading --

void
LitehtmlContainer::load_image(const char *src, const char * /*baseurl*/, bool redraw_on_ready)
{
    const auto srcUrl = QString::fromUtf8(src);

    // Security: only allow image://mxcImage/ URLs (rewritten MXC URLs).
    if (!srcUrl.startsWith(QLatin1String("image://mxcImage/")))
        return;

    if (m_imageCache.contains(srcUrl))
        return;

    // Extract the ID portion after the provider prefix.
    const auto id = srcUrl.mid(QStringLiteral("image://mxcImage/").length());

    MxcImageProvider::download(
      id,
      QSize(),
      [this, srcUrl, redraw_on_ready](
        const QString &, const QSize &, const QImage &image, const QString &error) {
          if (error.isEmpty() && !image.isNull()) {
              m_imageCache.insert(srcUrl, image);
              if (redraw_on_ready)
                  emit imageLoaded();
          }
      },
      false, // crop
      0);    // radius
}

void
LitehtmlContainer::get_image_size(const char *src, const char * /*baseurl*/, litehtml::size &sz)
{
    const auto key = QString::fromUtf8(src);
    if (m_imageCache.contains(key)) {
        const auto &img = m_imageCache[key];
        sz.width        = img.width();
        sz.height       = img.height();
    } else {
        sz.width  = 0;
        sz.height = 0;
    }
}

// -- Drawing --

void
LitehtmlContainer::draw_background(litehtml::uint_ptr /*hdc*/,
                                   const std::vector<litehtml::background_paint> &bgs)
{
    if (!m_painter)
        return;

    // Iterate in reverse: CSS paints farthest background first.
    for (auto it = bgs.rbegin(); it != bgs.rend(); ++it) {
        const auto &bg = *it;

        // Fill with background color if not transparent.
        if (bg.color.alpha > 0) {
            m_painter->fillRect(
              QRect(bg.border_box.x, bg.border_box.y, bg.border_box.width, bg.border_box.height),
              toQColor(bg.color));
        }

        // Draw background image if present.
        if (!bg.image.empty()) {
            const auto key = QString::fromStdString(bg.image);
            if (m_imageCache.contains(key)) {
                m_painter->drawImage(
                  QRect(bg.position_x, bg.position_y, bg.image_size.width, bg.image_size.height),
                  m_imageCache[key]);
            }
        }
    }
}

void
LitehtmlContainer::drawBorderLine(const QPoint &from,
                                  const QPoint &to,
                                  const litehtml::border &border)
{
    if (!m_painter || border.width == 0)
        return;

    QPen pen(toQColor(border.color), border.width);
    switch (border.style) {
    case litehtml::border_style_dotted:
        pen.setStyle(Qt::DotLine);
        break;
    case litehtml::border_style_dashed:
        pen.setStyle(Qt::DashLine);
        break;
    case litehtml::border_style_solid:
    default:
        pen.setStyle(Qt::SolidLine);
        break;
    }
    m_painter->setPen(pen);
    m_painter->drawLine(from, to);
}

void
LitehtmlContainer::draw_borders(litehtml::uint_ptr /*hdc*/,
                                const litehtml::borders &borders,
                                const litehtml::position &draw_pos,
                                bool /*root*/)
{
    if (!m_painter)
        return;

    int x = draw_pos.x;
    int y = draw_pos.y;
    int w = draw_pos.width;
    int h = draw_pos.height;

    // QPen strokes are centered on the line coordinates, so offset each
    // border inward by half its width to keep the stroke inside the box.
    // Top
    if (borders.top.width > 0) {
        int off = borders.top.width / 2;
        drawBorderLine(QPoint(x, y + off), QPoint(x + w, y + off), borders.top);
    }
    // Bottom
    if (borders.bottom.width > 0) {
        int off = borders.bottom.width / 2;
        drawBorderLine(QPoint(x, y + h - off), QPoint(x + w, y + h - off), borders.bottom);
    }
    // Left
    if (borders.left.width > 0) {
        int off = borders.left.width / 2;
        drawBorderLine(QPoint(x + off, y), QPoint(x + off, y + h), borders.left);
    }
    // Right
    if (borders.right.width > 0) {
        int off = borders.right.width / 2;
        drawBorderLine(QPoint(x + w - off, y), QPoint(x + w - off, y + h), borders.right);
    }
}

// -- Clipping --

void
LitehtmlContainer::set_clip(const litehtml::position &pos,
                            const litehtml::border_radiuses & /*bdr_radius*/)
{
    if (!m_painter)
        return;
    QRect r(pos.x, pos.y, pos.width, pos.height);
    m_clips.push_back(r);
    m_painter->save();
    m_painter->setClipRect(r, Qt::IntersectClip);
}

void
LitehtmlContainer::del_clip()
{
    if (!m_painter || m_clips.isEmpty())
        return;
    m_painter->restore();
    m_clips.pop_back();
}

// -- Viewport & metadata --

void
LitehtmlContainer::get_client_rect(litehtml::position &client) const
{
    client.x      = 0;
    client.y      = 0;
    client.width  = m_viewportWidth;
    client.height = m_viewportHeight;
}

void
LitehtmlContainer::get_media_features(litehtml::media_features &media) const
{
    media.type          = litehtml::media_type_screen;
    media.width         = m_viewportWidth;
    media.height        = m_viewportHeight;
    media.device_width  = m_viewportWidth;
    media.device_height = m_viewportHeight;
    media.color         = 8;
    media.monochrome    = 0;
    media.color_index   = 256;
    media.resolution    = 96;
}

void
LitehtmlContainer::get_language(litehtml::string &language, litehtml::string &culture) const
{
    language = "en";
    culture  = "";
}

// -- No-op / pass-through --

void
LitehtmlContainer::set_caption(const char * /*caption*/)
{
}

void
LitehtmlContainer::set_base_url(const char * /*base_url*/)
{
}

void
LitehtmlContainer::link(const std::shared_ptr<litehtml::document> & /*doc*/,
                        const litehtml::element::ptr & /*el*/)
{
}

void
LitehtmlContainer::on_anchor_click(const char *url, const litehtml::element::ptr & /*el*/)
{
    if (!url)
        return;
    if (m_hoverMode)
        m_lastHoveredUrl = QString::fromUtf8(url);
    else
        emit linkClicked(QString::fromUtf8(url));
}

void
LitehtmlContainer::set_cursor(const char *cursor)
{
    m_pointerCursor = cursor && QLatin1String(cursor) == QLatin1String("pointer");
}

void
LitehtmlContainer::transform_text(litehtml::string &text, litehtml::text_transform tt)
{
    auto qtext = QString::fromStdString(text);
    switch (tt) {
    case litehtml::text_transform_capitalize:
        // Capitalize first letter of each word
        if (!qtext.isEmpty()) {
            bool prevSpace = true;
            for (int i = 0; i < qtext.size(); ++i) {
                if (prevSpace && qtext[i].isLetter())
                    qtext[i] = qtext[i].toUpper();
                prevSpace = qtext[i].isSpace();
            }
        }
        break;
    case litehtml::text_transform_uppercase:
        qtext = qtext.toUpper();
        break;
    case litehtml::text_transform_lowercase:
        qtext = qtext.toLower();
        break;
    default:
        return;
    }
    text = qtext.toStdString();
}

void
LitehtmlContainer::import_css(litehtml::string & /*text*/,
                              const litehtml::string & /*url*/,
                              litehtml::string & /*baseurl*/)
{
    // No external CSS loading — security.
}

litehtml::element::ptr
LitehtmlContainer::create_element(const char * /*tag_name*/,
                                  const litehtml::string_map & /*attributes*/,
                                  const std::shared_ptr<litehtml::document> & /*doc*/)
{
    return nullptr;
}

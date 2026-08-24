// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/litehtml/LitehtmlContainer.h"

#include <algorithm>

#include <QGuiApplication>
#include <QPen>
#include <QPointer>
#include <QScreen>
#include <QTextDocumentFragment>
#include <QThreadPool>
#include <QUrl>
#include <QtMath>

#include "avatars/default/DefaultAvatarProvider.h"
#include "providers/MxcImageProvider.h"
#include "timeline/litehtml/LitehtmlStylesheet.h"

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
LitehtmlContainer::create_font(const litehtml::font_description &descr,
                               const litehtml::document * /*doc*/,
                               litehtml::font_metrics *fm)
{
    // litehtml 0.10 bundles all font attributes into font_description; the
    // face name, pixel size, weight, style and text-decoration flags that
    // used to be separate create_font arguments now come from there.
    const char *faceName = descr.family.c_str();
    const int size       = qRound(descr.size);

    auto *font = new QFont();

    if (faceName && *faceName) {
        // litehtml may pass CSS font-family values with surrounding quotes
        // (e.g. "'Noto Sans'" from font-family: 'Noto Sans'). Strip them
        // so Qt can match the actual font name.
        QString family = QString::fromUtf8(faceName);
        if (family.startsWith(QLatin1Char('\'')) && family.endsWith(QLatin1Char('\'')))
            family = family.mid(1, family.size() - 2);
        font->setFamily(family);
    } else {
        font->setFamily(m_defaultFont.family());
    }

    font->setPixelSize(size);

    // Map CSS weight (100-900) to QFont weight.
    // litehtml passes values like 400 (normal), 700 (bold).
    font->setWeight(static_cast<QFont::Weight>(qBound(1, descr.weight, 1000)));

    font->setItalic(descr.style == litehtml::font_style_italic);
    font->setUnderline(descr.decoration_line & litehtml::text_decoration_line_underline);
    font->setStrikeOut(descr.decoration_line & litehtml::text_decoration_line_line_through);
    font->setOverline(descr.decoration_line & litehtml::text_decoration_line_overline);

    if (fm) {
        QFontMetrics metrics(*font);
        QString faceStr = QString::fromUtf8(faceName);
        // litehtml passes CSS font-family values which may include quotes
        // (e.g. "'Noto Color Emoji'" from font-family: 'Noto Color Emoji').
        // Strip surrounding quotes for comparison.
        QString faceStripped = faceStr;
        if (faceStripped.startsWith(QLatin1Char('\'')) && faceStripped.endsWith(QLatin1Char('\'')))
            faceStripped = faceStripped.mid(1, faceStripped.size() - 2);
        bool isEmojiFont = !m_emojiFontFamily.isEmpty() && !faceStripped.isEmpty() &&
                           faceStripped == m_emojiFontFamily;
        if (isEmojiFont) {
            // Report text font metrics instead of the emoji font's inflated
            // metrics.  Body emojis use emojiScaleFactor (CSS font-size), so
            // dividing by that factor recovers the base text size; use default
            // font metrics to keep normal line height.  Heading emojis are 1em,
            // so their size doesn't match the scaled body size; use text font
            // metrics at that size so the line box is tall enough.
            using timeline::litehtml::emojiScaleFactor;
            int defaultPx    = pt_to_px(qRound(m_defaultFont.pointSizeF()));
            int unscaledPx   = qRound(size / emojiScaleFactor);
            bool isBodyEmoji = (unscaledPx == defaultPx);
            QFontMetrics dfm = isBodyEmoji ? QFontMetrics(m_defaultFont) : QFontMetrics([&] {
                QFont f(m_defaultFont);
                f.setPixelSize(size);
                return f;
            }());
            fm->ascent       = dfm.ascent();
            fm->descent      = dfm.descent();
            fm->height       = dfm.height();
            fm->x_height     = dfm.xHeight();
        } else {
            fm->ascent   = metrics.ascent();
            fm->descent  = metrics.descent();
            fm->height   = metrics.height();
            fm->x_height = metrics.xHeight();
        }
        // litehtml 0.10 resolves em lengths as `value * font_metrics::font_size`
        // (document::to_pixels). font_size is a new field that did not exist in
        // 0.9; leaving it at its 0 default collapses every em-based length —
        // including our `0.65em` paragraph/heading margins — to zero, removing
        // all block spacing. It must be the font's pixel size. ch_width and the
        // sub/super shifts are likewise new; mirror the reference containers.
        fm->font_size   = size;
        fm->ch_width    = metrics.horizontalAdvance(QLatin1Char('0'));
        fm->sub_shift   = size / 5;
        fm->super_shift = size / 3;
        fm->draw_spaces = true;
    }

    return reinterpret_cast<litehtml::uint_ptr>(font);
}

void
LitehtmlContainer::delete_font(litehtml::uint_ptr hFont)
{
    delete reinterpret_cast<QFont *>(hFont);
}

litehtml::pixel_t
LitehtmlContainer::text_width(const char *text, litehtml::uint_ptr hFont)
{
    auto *font = reinterpret_cast<QFont *>(hFont);
    if (!font)
        return 0;
    QFontMetrics metrics(*font);
    return metrics.horizontalAdvance(QString::fromUtf8(text));
}

void
LitehtmlContainer::split_text(const char *text,
                              const std::function<void(const char *)> &on_word,
                              const std::function<void(const char *)> &on_space)
{
    // Litehtml's default split_text only breaks on whitespace and CJK chars,
    // so a long URL with no spaces becomes a single unbreakable text element
    // and overflows narrow containers.  Komai's tokenizer splits URL-shaped
    // runs at separator chars and forces a break inside very long unbreakable
    // runs so line_box has wrap opportunities.  Each emitted segment becomes
    // its own el_text; line_box only inserts an actual line break between them
    // when the next segment would overflow, so short text is unaffected
    // visually.
    timeline::litehtml::splitText(text, on_word, on_space);
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

    m_painter->setFont(*font);
    m_painter->setPen(toQColor(color));

    // Emoji fonts need vertical position adjustment because we report
    // text-font metrics in create_font to keep line heights uniform,
    // but the actual emoji glyph uses the emoji font's (larger) metrics.
    // litehtml positions the text run assuming our fake ascent, so the
    // rendered baseline (pos.y + realAscent) ends up wrong.  Shift the
    // draw rect so that the real baseline aligns with the expected one.
    // Use TextDontClip: italic glyphs lean beyond their layout box, so
    // clipping to the litehtml position rect cuts off ascenders (e.g. the
    // top of 'l' in "<em>still</em>").  Element-level clipping is already
    // handled by litehtml's own clip stack (set_clip / del_clip).
    QString str = QString::fromUtf8(text);
    if (!m_emojiFontFamily.isEmpty() && font->family() == m_emojiFontFamily) {
        QFontMetrics realMetrics(*font);

        // Recompute the fake ascent we reported in create_font.
        using timeline::litehtml::emojiScaleFactor;
        int defaultPx            = pt_to_px(qRound(m_defaultFont.pointSizeF()));
        int unscaledPx           = qRound(font->pixelSize() / emojiScaleFactor);
        bool isBodyEmoji         = (unscaledPx == defaultPx);
        QFontMetrics fakeMetrics = isBodyEmoji ? QFontMetrics(m_defaultFont) : QFontMetrics([&] {
            QFont f(m_defaultFont);
            f.setPixelSize(font->pixelSize());
            return f;
        }());

        int yOffset = fakeMetrics.ascent() - realMetrics.ascent();
        m_painter->drawText(
          QRect(pos.x, pos.y + yOffset, pos.width, realMetrics.height()), Qt::TextDontClip, str);
    } else {
        m_painter->drawText(QRect(pos.x, pos.y, pos.width, pos.height), Qt::TextDontClip, str);
    }
}

void
LitehtmlContainer::endTextRunCollection(const std::shared_ptr<litehtml::render_item> &root,
                                        const QPoint &offset)
{
    m_collectingTextRuns = false;

    // Walk the laid-out render tree instead of capturing draw_text calls:
    // the tree carries the logical structure (word adjacency, collapsed
    // spaces, <br>/pre newlines, block boundaries) that selection extraction
    // needs to undo soft line-wrapping, and yields the runs in document
    // (reading) order — no visual re-sorting required.
    timeline::litehtml::collectTextRuns(root, offset, m_defaultFont, m_textRuns);

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

litehtml::pixel_t
LitehtmlContainer::pt_to_px(float pt) const
{
    // Match Qt's standard conversion factor (11pt -> ~14.67px at 96dpi).
    return qRound(pt * 96.0 / 72.0);
}

litehtml::pixel_t
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

namespace {

constexpr auto kMxcImagePrefix      = QLatin1String("image://mxcImage/");
constexpr auto kDefaultAvatarPrefix = QLatin1String("image://default-avatar/");

double
maxScreenDevicePixelRatio()
{
    double dpr = 1.0;
    for (const auto *s : QGuiApplication::screens())
        dpr = qMax(dpr, s->devicePixelRatio());
    return dpr;
}

} // namespace

void
LitehtmlContainer::load_image(const char *src, const char * /*baseurl*/, bool /*redraw_on_ready*/)
{
    const auto srcUrl = QString::fromUtf8(src);

    if (m_imageCache.contains(srcUrl) || m_inFlight.contains(srcUrl))
        return;

    if (srcUrl.startsWith(kMxcImagePrefix)) {
        m_inFlight.insert(srcUrl);
        loadMxcImage(srcUrl);
    } else if (srcUrl.startsWith(kDefaultAvatarPrefix)) {
        m_inFlight.insert(srcUrl);
        loadDefaultAvatarImage(srcUrl, srcUrl);
    }
    // Anything else is rejected for safety — pill HTML only ever points at
    // these two providers, so other schemes are either content from a peer
    // we shouldn't fetch or a packaging mistake.
}

void
LitehtmlContainer::loadMxcImage(const QString &srcUrl)
{
    auto id       = srcUrl.mid(kMxcImagePrefix.size());
    bool crop     = false;
    double radius = 0;
    QSize size;
    QString roomId;
    // Optional `fallback=<percent-encoded image://default-avatar/...>` query —
    // emitted by the Rust pill decorator alongside the mxc URL so we can
    // pre-cache the default avatar under the mxc URL key. Mirrors
    // Avatar.qml's "show default while loading, keep it on failure"
    // behaviour for the timeline body.
    QString fallbackUrl;

    const auto queryStart = id.lastIndexOf(QLatin1Char('?'));
    if (queryStart != -1) {
        const auto query     = QStringView(id).mid(queryStart + 1);
        const auto queryBits = query.split(QLatin1Char('&'));
        id                   = id.left(queryStart);

        for (const auto &b : queryBits) {
            if (b == QStringView(u"scale")) {
                crop = false;
            } else if (b == QStringView(u"crop")) {
                crop = true;
            } else if (b.startsWith(QStringView(u"radius="))) {
                radius = b.mid(7).toDouble();
            } else if (b.startsWith(QStringView(u"avatarSize="))) {
                // Logical avatar size — apply QScreen DPR to get physical size.
                int side = qMax(1, qRound(b.mid(11).toInt() * maxScreenDevicePixelRatio()));
                size     = QSize(side, side);
                crop     = true; // match Avatar.qml's default crop mode
            } else if (b.startsWith(QStringView(u"height="))) {
                size.setHeight(b.mid(7).toInt());
                size.setWidth(0);
            } else if (b.startsWith(QStringView(u"room="))) {
                roomId = b.mid(5).toString();
            } else if (b.startsWith(QStringView(u"fallback="))) {
                fallbackUrl = QUrl::fromPercentEncoding(b.mid(9).toUtf8());
            }
        }
    }

    // Pre-emptively cache the default avatar under the mxc URL key, so the
    // pill renders the default while the mxc fetch is in flight (and stays
    // default if the mxc fetch fails). The mxc download below races this
    // and is allowed to overwrite the entry on success.
    if (!fallbackUrl.isEmpty() && fallbackUrl.startsWith(kDefaultAvatarPrefix))
        loadDefaultAvatarImage(fallbackUrl, srcUrl);

    QPointer<LitehtmlContainer> guard(this);
    MxcImageProvider::download(
      id,
      size,
      [guard, srcUrl](const QString &, const QSize &, const QImage &image, const QString &) {
          // The callback arrives off the main thread; marshal everything
          // (success and null alike) to the main thread so the QPointer
          // check, in-flight bookkeeping, and cache update are all
          // thread-safe.
          QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [guard, srcUrl, image]() {
                if (!guard)
                    return;
                guard->m_inFlight.remove(srcUrl);
                if (image.isNull()) {
                    // Leave whatever's already in the cache (typically the
                    // pre-cached default avatar from the fallback path).
                    return;
                }
                guard->m_imageCache.insert(srcUrl, image);
                emit guard->imageLoaded();
            },
            Qt::QueuedConnection);
      },
      crop,
      radius,
      roomId);
}

void
LitehtmlContainer::loadDefaultAvatarImage(const QString &defaultAvatarUrl, const QString &cacheKey)
{
    // URL shape mirrors what Avatar.qml builds:
    //   image://default-avatar/{userid}?radius=N&displayName=...&color=rrggbb&style=N&_v=N&avatarSize=N
    // Parse manually rather than via QUrl: matrix user IDs contain ':' and '@'
    // which QUrl tries to interpret as authority/userinfo delimiters and
    // mangles.
    auto id    = defaultAvatarUrl.mid(kDefaultAvatarPrefix.size());
    int radius = 0;
    int style  = 0;
    QString displayName;
    QString colorHex;
    QSize size;

    const auto queryStart = id.indexOf(QLatin1Char('?'));
    if (queryStart != -1) {
        const auto query     = QStringView(id).mid(queryStart + 1);
        const auto queryBits = query.split(QLatin1Char('&'));
        id                   = id.left(queryStart);

        for (const auto &b : queryBits) {
            if (b.startsWith(QStringView(u"radius="))) {
                radius = b.mid(7).toInt();
            } else if (b.startsWith(QStringView(u"style="))) {
                style = b.mid(6).toInt();
            } else if (b.startsWith(QStringView(u"displayName="))) {
                displayName = QUrl::fromPercentEncoding(b.mid(12).toUtf8());
            } else if (b.startsWith(QStringView(u"color="))) {
                colorHex = b.mid(6).toString();
            } else if (b.startsWith(QStringView(u"avatarSize="))) {
                int side = qMax(1, qRound(b.mid(11).toInt() * maxScreenDevicePixelRatio()));
                size     = QSize(side, side);
            }
            // `_v` is a cache-buster consumed by the QML side — ignored here
            // because cacheKey is already the full m_imageCache key.
        }
    }

    if (size.isEmpty())
        size = QSize(48, 48);

    // When the caller provides a different cache key, this is the auxiliary
    // pre-cache step run from loadMxcImage: in-flight bookkeeping is owned
    // by the mxc callback, and we must not clobber a real mxc image that
    // happened to land first.
    const bool isPrimary = (defaultAvatarUrl == cacheKey);

    QPointer<LitehtmlContainer> guard(this);
    auto *runnable = new DefaultAvatarRunnable(id, radius, displayName, colorHex, size, style);
    QObject::connect(runnable,
                     &DefaultAvatarRunnable::done,
                     this,
                     [guard, cacheKey, isPrimary](const QImage &image) {
                         if (!guard)
                             return;
                         // DefaultAvatarRunnable signals on the runnable's
                         // thread; marshal back to the main thread before
                         // touching QObject state.
                         QMetaObject::invokeMethod(
                           QCoreApplication::instance(),
                           [guard, cacheKey, isPrimary, image]() {
                               if (!guard)
                                   return;
                               if (isPrimary)
                                   guard->m_inFlight.remove(cacheKey);
                               if (image.isNull())
                                   return;
                               // Auxiliary path: a real mxc image may already
                               // be in the cache from a fast download; don't
                               // overwrite it with the default.
                               if (!isPrimary && guard->m_imageCache.contains(cacheKey))
                                   return;
                               guard->m_imageCache.insert(cacheKey, image);
                               emit guard->imageLoaded();
                           },
                           Qt::QueuedConnection);
                     });
    QThreadPool::globalInstance()->start(runnable);
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
LitehtmlContainer::draw_solid_fill(litehtml::uint_ptr /*hdc*/,
                                   const litehtml::background_layer &layer,
                                   const litehtml::web_color &color)
{
    // litehtml 0.10 splits the old draw_background into one call per layer:
    // solid colours arrive here, images via draw_image, and CSS gradients via
    // the draw_*_gradient hooks.
    if (!m_painter || color.alpha == 0)
        return;

    const auto &b = layer.border_box;
    QRect rect(b.x, b.y, b.width, b.height);
    const auto &r  = layer.border_radius;
    bool hasRadius = r.top_left_x || r.top_right_x || r.bottom_left_x || r.bottom_right_x;
    if (hasRadius) {
        // Use a uniform radius (average of all corners).
        int rx = (r.top_left_x + r.top_right_x + r.bottom_right_x + r.bottom_left_x) / 4;
        int ry = (r.top_left_y + r.top_right_y + r.bottom_right_y + r.bottom_left_y) / 4;
        m_painter->setRenderHint(QPainter::Antialiasing, true);
        m_painter->setPen(Qt::NoPen);
        m_painter->setBrush(toQColor(color));
        m_painter->drawRoundedRect(rect, rx, ry);
        m_painter->setRenderHint(QPainter::Antialiasing, false);
    } else {
        m_painter->fillRect(rect, toQColor(color));
    }
}

void
LitehtmlContainer::draw_image(litehtml::uint_ptr /*hdc*/,
                              const litehtml::background_layer &layer,
                              const std::string &url,
                              const std::string & /*base_url*/)
{
    if (!m_painter || url.empty())
        return;

    const auto key = QString::fromStdString(url);
    if (!m_imageCache.contains(key))
        return;

    // origin_box is where the image's first tile is laid out; for our pill /
    // avatar content (single, non-repeating images) drawing it once there
    // matches the old position_x/position_y + image_size rectangle.
    const auto &o = layer.origin_box;
    m_painter->drawImage(QRect(o.x, o.y, o.width, o.height), m_imageCache[key]);
}

void
LitehtmlContainer::draw_linear_gradient(
  litehtml::uint_ptr /*hdc*/,
  const litehtml::background_layer & /*layer*/,
  const litehtml::background_layer::linear_gradient & /*gradient*/)
{
    // CSS gradients are not used by the HTML we render (pills, formatted
    // message bodies), so these are intentionally no-ops.
}

void
LitehtmlContainer::draw_radial_gradient(
  litehtml::uint_ptr /*hdc*/,
  const litehtml::background_layer & /*layer*/,
  const litehtml::background_layer::radial_gradient & /*gradient*/)
{
}

void
LitehtmlContainer::draw_conic_gradient(
  litehtml::uint_ptr /*hdc*/,
  const litehtml::background_layer & /*layer*/,
  const litehtml::background_layer::conic_gradient & /*gradient*/)
{
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
LitehtmlContainer::get_viewport(litehtml::position &viewport) const
{
    viewport.x      = 0;
    viewport.y      = 0;
    viewport.width  = m_viewportWidth;
    viewport.height = m_viewportHeight;
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
    // litehtml passes the raw href attribute value which may still contain HTML entities.
    // Decode all of them so that URLs work correctly (e.g. &amp; → &).
    auto decoded = QTextDocumentFragment::fromHtml(QString::fromUtf8(url)).toPlainText();

    if (m_hoverMode)
        m_lastHoveredUrl = decoded;
    else
        emit linkClicked(decoded);
}

void
LitehtmlContainer::on_mouse_event(const litehtml::element::ptr & /*el*/,
                                  litehtml::mouse_event /*event*/)
{
    // litehtml 0.10 reports element mouse enter/leave here for :hover-style
    // effects; our rendering recomputes hover via on_mouse_over each frame, so
    // there is nothing extra to track.
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

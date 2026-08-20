// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/litehtml/LitehtmlItem.h"
#include <QRegularExpression>

#include <QClipboard>
#include <QElapsedTimer>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QImage>
#include <QPainterPath>
#include <QPalette>
#include <QQuickWindow>
#include <QtMath>

#include <algorithm>
#include <climits>
#include <cstring>

#include <litehtml/render_item.h>

#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/litehtml/LitehtmlStylesheet.h"
#include "ui/Theme.h"
#include "utils/Utils.h"

namespace {
bool
churnPerfEnabled()
{
    static const bool enabled = [] {
        auto val = qgetenv("KOMAI_PERF_TIMELINE_CHURN").trimmed().toLower();
        return val == "1" || val == "true" || val == "yes" || val == "on";
    }();
    return enabled;
}

// Diagnostic only: KOMAI_SPOILER_DEBUG traces spoiler box collection and the
// paint that consumes it, so a mismatch between the two can be seen directly.
bool
spoilerDebugEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("KOMAI_SPOILER_DEBUG");
    return enabled;
}

// A short, tag-free prefix of the message, so log lines can be matched up to
// the messages they came from.
QString
spoilerDebugTag(const QString &html)
{
    QString plain = html;
    plain.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    return plain.simplified().left(48);
}

// Blur one hidden-spoiler region in place. `itemBoxes` are in item
// coordinates; `buffer` holds device pixels at `dpr` scale. Heavy downscale +
// smooth upscale is cheap and leaves text unreadable at any font size, while
// the smear still hints at the hidden content's shape and colors.
void
blurSpoilerRegion(QImage &buffer, const QVector<QRect> &itemBoxes, qreal dpr)
{
    // One region's line boxes are blurred together rather than one at a time:
    // a wrapped spoiler's lines abut, and clearing/redrawing each separately
    // left the rounded corners notched out along every seam.
    QRegion region;
    for (const QRect &itemBox : itemBoxes) {
        const QRect deviceBox(qFloor(itemBox.x() * dpr),
                              qFloor(itemBox.y() * dpr),
                              qCeil(itemBox.width() * dpr),
                              qCeil(itemBox.height() * dpr));
        region += deviceBox.intersected(QRect(QPoint(0, 0), buffer.size()));
    }
    const QRect r = region.boundingRect();
    if (r.isEmpty())
        return;

    QImage patch = buffer.copy(r);
    patch.setDevicePixelRatio(1.0);
    // Scaled by the device ratio so the blur is as strong on a HiDPI screen as
    // on a 1x one. The patch is in device pixels, so a fixed divisor would
    // leave proportionally more detail the denser the display, and at 2x the
    // glyph shapes were still legible through it.
    constexpr int kBlurFactor = 8;
    const int factor          = qMax(1, qRound(kBlurFactor * dpr));
    const QSize small(qMax(1, patch.width() / factor), qMax(1, patch.height() / factor));
    const QImage blurred = patch.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                             .scaled(patch.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // Everything below is in device pixels. `resetTransform()` is not enough to
    // get there: it clears the world transform, but not the scale QPainter
    // derives from the paint device's devicePixelRatio, which would apply the
    // ratio a second time and push the blur off its text. Drop the ratio for
    // the duration of the composite instead, then restore it so the image still
    // draws at its device-independent size.
    const qreal bufferDpr = buffer.devicePixelRatio();
    buffer.setDevicePixelRatio(1.0);
    QPainter p(&buffer);
    p.setRenderHint(QPainter::Antialiasing, true);
    // Clip to the region first so the clear and the repaint cover exactly the
    // same pixels. Clearing a larger area than gets repainted is what punched
    // transparent bites out of the rounded corners.
    QPainterPath clipPath;
    if (region.rectCount() == 1)
        clipPath.addRoundedRect(QRectF(r), 4.0 * dpr, 4.0 * dpr);
    else
        clipPath.addRegion(region);
    p.setClipPath(clipPath);
    // The sharp original pixels must be fully removed first: the blurred
    // patch is partially transparent around glyph edges, so merely painting
    // it over the original would leave the text readable underneath.
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.fillRect(r, Qt::black);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.drawImage(r.topLeft(), blurred);
    p.end();
    buffer.setDevicePixelRatio(bufferDpr);
}
} // namespace

LitehtmlItem::LitehtmlItem(QQuickItem *parent)
  : QQuickPaintedItem(parent)
  , m_container(new LitehtmlContainer(this))
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setAntialiasing(true);

    connect(m_container, &LitehtmlContainer::linkClicked, this, &LitehtmlItem::linkActivated);
    connect(m_container, &LitehtmlContainer::imageLoaded, this, [this]() {
        if (m_document) {
            relayout();
            update();
        }
    });
    connect(UserSettings::instance().get(), &UserSettings::uiThemeSlugChanged, this, [this]() {
        m_masterCssDirty = true;
        requestDocumentRebuild();
    });
    connect(this, &QQuickItem::activeFocusChanged, this, [this](bool hasFocus) {
        if (!hasFocus) {
            clearSelection();
            update();
        }
    });
}

QString
LitehtmlItem::generateMasterCss()
{
    auto palette = QGuiApplication::palette();
    const Theme theme(UserSettings::instance()->uiThemeSlug());
    // Captured before the surface override below, so code blocks keep the theme's
    // own alternate-base instead of the per-sender bubble tint.
    const auto codeBackground = timeline::litehtml::codeBackgroundColor(palette);
    if (m_color.isValid())
        palette.setColor(QPalette::Text, m_color);
    if (m_linkColor.isValid())
        palette.setColor(QPalette::Link, m_linkColor);
    if (m_surfaceColor.isValid())
        palette.setColor(QPalette::AlternateBase, m_surfaceColor);
    // Search-match highlight uses the theme's `warning` color (semantically
    // "draw attention without alarm" — consistently yellow/orange across
    // themes, unlike `attention` which leans error-red). Themes don't define
    // a paired text color for warning, so derive it from the bg's luminance:
    // pick black on light bg, white on dark bg, then mix 10% of the bg back
    // in so the text picks up the bg's warmth instead of reading clinical.
    const auto searchHighlightBg = theme.warning();
    const QColor contrast =
      utils::luminance(searchHighlightBg) > 0.5 ? QColor(Qt::black) : QColor(Qt::white);
    const QColor searchHighlightFg((contrast.red() * 9 + searchHighlightBg.red()) / 10,
                                   (contrast.green() * 9 + searchHighlightBg.green()) / 10,
                                   (contrast.blue() * 9 + searchHighlightBg.blue()) / 10);
    return timeline::litehtml::generateMasterStylesheet(palette,
                                                        m_font,
                                                        m_compact,
                                                        theme.error().name(),
                                                        theme.attention().name(),
                                                        theme.success().name(),
                                                        searchHighlightBg.name(),
                                                        searchHighlightFg.name(),
                                                        codeBackground);
}

void
LitehtmlItem::setHtml(const QString &html)
{
    if (m_html == html)
        return;
    m_html = html;
    // New message content (also on delegate recycling): spoilers start hidden.
    m_revealedSpoilers.clear();
    m_spoilerRegions.clear();
    setHoveredSpoiler(false);
    emit htmlChanged();
    requestDocumentRebuild();
}

void
LitehtmlItem::setColor(const QColor &color)
{
    if (m_color == color)
        return;
    m_color = color;
    m_container->setDefaultColor(color);
    emit colorChanged();
    m_masterCssDirty = true;
    requestDocumentRebuild();
}

void
LitehtmlItem::setLinkColor(const QColor &color)
{
    if (m_linkColor == color)
        return;
    m_linkColor = color;
    emit linkColorChanged();
    m_masterCssDirty = true;
    requestDocumentRebuild();
}

void
LitehtmlItem::setSurfaceColor(const QColor &color)
{
    if (m_surfaceColor == color)
        return;
    m_surfaceColor = color;
    emit surfaceColorChanged();
    m_masterCssDirty = true;
    requestDocumentRebuild();
}

void
LitehtmlItem::setFont(const QFont &font)
{
    if (m_font == font)
        return;
    m_font = font;
    // QML font inheritance may leave the family empty; resolve to the
    // application font so the litehtml master CSS always has a concrete name.
    if (m_font.family().isEmpty())
        m_font.setFamily(QGuiApplication::font().family());
    m_container->setDefaultFont(m_font);
    emit fontChanged();
    m_masterCssDirty = true;
    requestDocumentRebuild();
}

void
LitehtmlItem::setLeftPadding(qreal padding)
{
    if (qFuzzyCompare(m_leftPadding, padding))
        return;
    m_leftPadding = padding;
    emit leftPaddingChanged();
    if (m_document) {
        relayout();
        update();
    }
}

void
LitehtmlItem::setRightPadding(qreal padding)
{
    if (qFuzzyCompare(m_rightPadding, padding))
        return;
    m_rightPadding = padding;
    emit rightPaddingChanged();
    if (m_document) {
        relayout();
        update();
    }
}

void
LitehtmlItem::setCompact(bool compact)
{
    if (m_compact == compact)
        return;
    m_compact = compact;
    emit compactChanged();
    m_masterCssDirty = true;
    requestDocumentRebuild();
}

void
LitehtmlItem::componentComplete()
{
    QQuickPaintedItem::componentComplete();

    if (m_rebuildPending)
        rebuildDocument();
}

void
LitehtmlItem::requestDocumentRebuild()
{
    m_rebuildPending = true;

    if (!isComponentComplete())
        return;

    rebuildDocument();
}

void
LitehtmlItem::rebuildDocument()
{
    clearSelection();
    // The empty-html branch below returns before relayout() would clear this.
    clearCodeBlock();

    m_container->clearImageCache();

    if (m_html.isEmpty()) {
        m_document.reset();
        setImplicitWidth(0);
        setImplicitHeight(0);
        update();
        return;
    }

    if (m_masterCssDirty) {
        m_masterCss      = generateMasterCss();
        m_masterCssDirty = false;
    }

    m_container->setDefaultFont(m_font);
    m_container->setDefaultColor(m_color);
    m_container->setEmojiFontFamily(utils::effectiveEmojiFontFamily());

    QElapsedTimer timer;
    timer.start();
    m_document            = litehtml::document::createFromString(m_html.toUtf8().constData(),
                                                      m_container,
                                                      litehtml::master_css,
                                                      m_masterCss.toUtf8().constData());
    const qint64 createUs = timer.nsecsElapsed() / 1000;

    ++m_rebuildCount;
    relayout();
    if (roomSwitchPerfEnabled() && m_rebuildCount <= 2) {
        logPerfPhase("litehtml.rebuild",
                     createUs,
                     QString(" rebuild_count=%1 html_len=%2 width=%3")
                       .arg(m_rebuildCount)
                       .arg(m_html.size())
                       .arg(qRound(width())));
    }
    if (churnPerfEnabled()) {
        komai::logging::ui()->info("[churn] rebuild item={} count={} htmlLen={} width={} us={}",
                                   (void *)this,
                                   m_rebuildCount,
                                   m_html.size(),
                                   qRound(width()),
                                   createUs);
    }
    update();
}

void
LitehtmlItem::relayout()
{
    // A reflow moves the <pre> but reuses the element, so the identity
    // throttle in handleHoverMove would keep a stale rect.
    clearCodeBlock();

    if (!m_document)
        return;

    int itemWidth = static_cast<int>(width());

    // Skip layout before the item has a real width — EventDelegateChooser
    // will assign the available width first, which triggers geometryChange.
    if (itemWidth < 1)
        return;

    int w = qMax(1, itemWidth - static_cast<int>(m_leftPadding) - static_cast<int>(m_rightPadding));
    m_container->setViewportSize(w, static_cast<int>(height()));
    QElapsedTimer timer;
    timer.start();
    // litehtml 0.10 dropped document::content_width() (declared but undefined).
    // It used to return the tight content extent — the widest laid-out line,
    // excluding the full-width <html>/<body> boxes. render() returns exactly
    // that intrinsic width (already capped at the render width by wrapping), so
    // we use its return value. document::width() is unusable here: it includes
    // the <html> root, which is display:block and fills the whole render width,
    // so it would size every bubble to the maximum width.
    const litehtml::pixel_t naturalWidth = m_document->render(w);
    const qint64 renderUs                = timer.nsecsElapsed() / 1000;
    // litehtml 0.10's pixel_t is float, so render() can return a fractional
    // natural width (e.g. 473.4). Truncating to int here would feed an
    // implicitWidth a hair narrower than the content back through the bubble's
    // shrink-to-fit binding; litehtml then re-renders at that width and wraps
    // the last element (e.g. a trailing emoji) onto a new line. Ceil so the
    // bubble is always at least as wide as the content litehtml measured.
    int cw =
      qCeil(naturalWidth) + static_cast<int>(m_leftPadding) + static_cast<int>(m_rightPadding);
    // litehtml's content_width is the sum of glyph advance widths. Italic
    // glyphs slant past their advance, so the last character's ink extends a
    // few pixels past content_width and gets clipped at the bubble edge.
    // Reserve a small overhang on the right when the default font is italic
    // (emotes, notices). A fraction of the font ascent scales naturally with
    // font size; clamp to a sensible minimum so tiny fonts still get a buffer.
    if (m_font.italic())
        cw += qMax(2, qRound(QFontMetrics(m_font).ascent() * 0.2));
    setImplicitWidth(cw);

    // Issue #78: when the message contains a `<span class="emoji">`, the emoji
    // glyph (font-size: 1.4em via CSS) renders with the emoji font's larger
    // ascent.  litehtml lays out the line box using the text font's ascent
    // (faked in LitehtmlContainer::create_font to keep line heights uniform),
    // so the emoji's ink top is positioned above the line box on the first
    // line — i.e. above the QQuickPaintedItem's texture top edge — and gets
    // clipped.  Reserve room for the overshoot and shift the document draw
    // down by the same amount in paint().
    const bool htmlHasEmoji = m_html.contains(QStringLiteral("<span class=\"emoji\""));
    int ascentOvershoot     = 0;
    if (htmlHasEmoji) {
        const auto emojiFamily = utils::effectiveEmojiFontFamily();
        if (!emojiFamily.isEmpty()) {
            const int textPx  = qRound(m_font.pointSizeF() * 96.0 / 72.0);
            const int emojiPx = qRound(textPx * timeline::litehtml::emojiScaleFactor);
            QFont emojiFont;
            emojiFont.setFamily(emojiFamily);
            emojiFont.setPixelSize(emojiPx);
            ascentOvershoot = QFontMetrics(emojiFont).ascent() - QFontMetrics(m_font).ascent();
        }
    }
    m_topInset = qMax(0, ascentOvershoot);
    setImplicitHeight(m_document->height() + m_topInset);

    collectSpoilerRegions();

    updateTextureSize();
    ++m_relayoutCount;
    if (roomSwitchPerfEnabled() && m_relayoutCount <= 3) {
        logPerfPhase("litehtml.relayout",
                     renderUs,
                     QString(" relayout_count=%1 width=%2 implicit_height=%3")
                       .arg(m_relayoutCount)
                       .arg(w)
                       .arg(qRound(implicitHeight())));
    }
    if (churnPerfEnabled()) {
        komai::logging::ui()->info("[churn] relayout item={} count={} renderW={} contentW={} "
                                   "implicitH={} us={}",
                                   (void *)this,
                                   m_relayoutCount,
                                   w,
                                   cw,
                                   qRound(implicitHeight()),
                                   renderUs);
    }
}

namespace {
// Document-absolute painted boxes of one element: per-line fragments for
// inline elements, the border box for blocks (get_rendering_boxes handles the
// ancestor-offset accumulation for both).
void
appendElementBoxes(const litehtml::element::ptr &el, QVector<QRect> &out)
{
    el->run_on_renderers([&out](const std::shared_ptr<litehtml::render_item> &ri) {
        litehtml::position::vector boxes;
        ri->get_rendering_boxes(boxes);
        for (const auto &b : boxes) {
            const QRect rect(qFloor(b.x), qFloor(b.y), qCeil(b.width), qCeil(b.height));
            if (!rect.isEmpty())
                out.append(rect);
        }
        return true; // visit every render item of this element
    });
}

// Every painted box in an element's subtree, at every nesting level. Needed
// because an inline element's own boxes are sized to *its* font metrics, so
// taller descendants (scaled emoji, a padded <code> chip, a larger nested
// font, images) paint outside them.
void
appendSubtreeBoxes(const litehtml::element::ptr &el, QVector<QRect> &out)
{
    for (const auto &child : el->children()) {
        appendElementBoxes(child, out);
        appendSubtreeBoxes(child, out);
    }
}

// Groups boxes into one rect per visual line, unioning everything that
// overlaps vertically. Used when a spoiler has no line boxes of its own to
// anchor to (an inline span wrapping block content, e.g. <pre>).
QVector<QRect>
mergeOverlappingRows(QVector<QRect> boxes)
{
    std::sort(boxes.begin(), boxes.end(), [](const QRect &a, const QRect &b) {
        return a.top() != b.top() ? a.top() < b.top() : a.left() < b.left();
    });
    QVector<QRect> rows;
    for (const QRect &box : boxes) {
        auto row = std::find_if(rows.begin(), rows.end(), [&box](const QRect &r) {
            return box.top() < r.bottom() && r.top() < box.bottom();
        });
        if (row != rows.end())
            *row = row->united(box);
        else
            rows.append(box);
    }
    return rows;
}

// Grows each of a spoiler's own line boxes to cover what actually painted on
// that line.
//
// Each subtree box is folded into the single line it overlaps most (nearest
// centre breaks ties), never into every line it touches: a glyph that
// overflows its line must not drag the neighbouring line's rect sideways, or
// the blur would spill onto text outside the spoiler.
void
growLineBoxesToPaintedExtent(QVector<QRect> &lineBoxes, const QVector<QRect> &subtree)
{
    for (const QRect &box : subtree) {
        int best        = -1;
        int bestOverlap = INT_MIN;
        for (int i = 0; i < lineBoxes.size(); ++i) {
            const QRect &line = lineBoxes[i];
            const int overlap = qMin(box.bottom(), line.bottom()) - qMax(box.top(), line.top());
            // Prefer real overlap; for a box that clears every line (rare, but
            // possible with large negative offsets) fall back to proximity,
            // which `overlap` already expresses as a negative distance.
            const int score =
              overlap != bestOverlap ? overlap : -qAbs(box.center().y() - line.center().y());
            if (score > bestOverlap) {
                bestOverlap = score;
                best        = i;
            }
        }
        if (best >= 0)
            lineBoxes[best] = lineBoxes[best].united(box);
    }
}
} // namespace

void
LitehtmlItem::collectSpoilerRegions()
{
    m_spoilerRegions.clear();
    if (!m_document || !m_html.contains(QLatin1String("data-mx-spoiler")))
        return;
    auto root = m_document->root();
    if (!root)
        return;

    // Spoiler content is laid out and painted normally; hiding happens in
    // paint(), which blurs these boxes. Collected after every layout because
    // the boxes are per-line fragments that move with wrapping.
    const auto spoilers = root->select_all("span[data-mx-spoiler]");
    for (const auto &el : spoilers) {
        SpoilerRegion region;
        appendElementBoxes(el, region.boxes);

        QVector<QRect> subtree;
        appendSubtreeBoxes(el, subtree);

        if (region.boxes.isEmpty()) {
            // No line boxes of our own to anchor to (an inline span wrapping
            // block content); derive the rows from what painted instead.
            region.boxes = mergeOverlappingRows(subtree);
        } else {
            growLineBoxesToPaintedExtent(region.boxes, subtree);
        }
        // Append even when still empty so indices keep tracking document
        // order; an empty region is simply never hit-tested or blurred.
        m_spoilerRegions.append(region);
    }
    if (spoilerDebugEnabled()) {
        QString dump;
        for (int i = 0; i < m_spoilerRegions.size(); ++i) {
            dump += QStringLiteral(" r%1=").arg(i);
            if (m_spoilerRegions[i].boxes.isEmpty())
                dump += QStringLiteral("EMPTY");
            for (const QRect &b : m_spoilerRegions[i].boxes)
                dump += QStringLiteral("[%1,%2 %3x%4]")
                          .arg(b.x())
                          .arg(b.y())
                          .arg(b.width())
                          .arg(b.height());
        }
        komai::logging::ui()->warn("[spoiler] collect item={} selected={} regions={} itemW={} "
                                   "itemH={} padL={} inset={}{} :: {}",
                                   (void *)this,
                                   spoilers.size(),
                                   m_spoilerRegions.size(),
                                   qRound(width()),
                                   qRound(height()),
                                   qRound(m_leftPadding),
                                   m_topInset,
                                   dump.toStdString(),
                                   spoilerDebugTag(m_html).toStdString());
    }
}

int
LitehtmlItem::spoilerIndexAt(const QPoint &itemPos) const
{
    const QPoint docPos(itemPos.x() - static_cast<int>(m_leftPadding), itemPos.y() - m_topInset);
    for (int i = 0; i < m_spoilerRegions.size(); ++i) {
        for (const QRect &box : m_spoilerRegions[i].boxes) {
            if (box.contains(docPos))
                return i;
        }
    }
    return -1;
}

bool
LitehtmlItem::hasHiddenSpoilers() const
{
    for (int i = 0; i < m_spoilerRegions.size(); ++i) {
        if (!m_spoilerRegions[i].boxes.isEmpty() && !m_revealedSpoilers.contains(i))
            return true;
    }
    return false;
}

void
LitehtmlItem::setHoveredSpoiler(bool hovered)
{
    if (m_hoveredSpoiler == hovered)
        return;
    m_hoveredSpoiler = hovered;
    emit hoveredSpoilerChanged();
}

void
LitehtmlItem::updateTextureSize()
{
    auto *w = window();
    if (!w)
        return;
    qreal dpr = w->devicePixelRatio();
    if (dpr > 1.0)
        setTextureSize(QSize(qCeil(width() * dpr), qCeil(height() * dpr)));
}

void
LitehtmlItem::paint(QPainter *painter)
{
    if (!m_document || !painter)
        return;

    int padLeft = static_cast<int>(m_leftPadding);
    m_container->setViewportSize(static_cast<int>(width()) - padLeft, static_cast<int>(height()));

    litehtml::position clip;
    clip.x      = 0;
    clip.y      = 0;
    clip.width  = static_cast<int>(width());
    clip.height = static_cast<int>(height());

    // Always collect text runs so they are available for double-click word
    // selection without requiring a prior drag selection paint pass.
    // Pass m_topInset as the y offset so that document y=0 maps to
    // item y=m_topInset, leaving room above for emoji glyph overshoot
    // (issue #78). TextRun rects recorded during draw_text are in
    // painter (item) coords, so selection painting/hit-testing keeps
    // working without further coord translation.
    QElapsedTimer timer;
    timer.start();
    if (spoilerDebugEnabled() && m_html.contains(QLatin1String("data-mx-spoiler"))) {
        QString dump;
        for (int i = 0; i < m_spoilerRegions.size(); ++i) {
            dump += QStringLiteral(" r%1%2=").arg(i).arg(
              m_revealedSpoilers.contains(i) ? QStringLiteral("(revealed)") : QString());
            if (m_spoilerRegions[i].boxes.isEmpty())
                dump += QStringLiteral("EMPTY");
            for (const QRect &b : m_spoilerRegions[i].boxes)
                dump += QStringLiteral("[%1,%2 %3x%4]")
                          .arg(b.x())
                          .arg(b.y())
                          .arg(b.width())
                          .arg(b.height());
        }
        komai::logging::ui()->warn("[spoiler] paint   item={} blurring={} regions={} itemW={} "
                                   "itemH={} padL={} inset={} dpr={}{} :: {}",
                                   (void *)this,
                                   hasHiddenSpoilers(),
                                   m_spoilerRegions.size(),
                                   qRound(width()),
                                   qRound(height()),
                                   padLeft,
                                   m_topInset,
                                   window() ? window()->devicePixelRatio() : -1.0,
                                   dump.toStdString(),
                                   spoilerDebugTag(m_html).toStdString());
    }
    if (!hasHiddenSpoilers()) {
        m_container->setPainter(painter);
        m_container->beginTextRunCollection();
        m_document->draw(reinterpret_cast<litehtml::uint_ptr>(painter), padLeft, m_topInset, &clip);
        m_container->endTextRunCollection();
    } else {
        // Hidden spoilers are blurred from the actual painted pixels, so the
        // document goes through an intermediate image first. Text runs are
        // still recorded in item coordinates — litehtml passes logical
        // positions regardless of the painter's DPR transform.
        const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
        QImage buffer(qMax(1, qCeil(width() * dpr)),
                      qMax(1, qCeil(height() * dpr)),
                      QImage::Format_ARGB32_Premultiplied);
        buffer.setDevicePixelRatio(dpr);
        buffer.fill(Qt::transparent);
        {
            QPainter bufferPainter(&buffer);
            m_container->setPainter(&bufferPainter);
            m_container->beginTextRunCollection();
            m_document->draw(
              reinterpret_cast<litehtml::uint_ptr>(&bufferPainter), padLeft, m_topInset, &clip);
            m_container->endTextRunCollection();
        }
        for (int i = 0; i < m_spoilerRegions.size(); ++i) {
            if (m_revealedSpoilers.contains(i))
                continue;
            QVector<QRect> boxes;
            boxes.reserve(m_spoilerRegions[i].boxes.size());
            for (const QRect &box : m_spoilerRegions[i].boxes)
                boxes.append(box.translated(padLeft, m_topInset));
            blurSpoilerRegion(buffer, boxes, dpr);
        }
        painter->drawImage(QPointF(0, 0), buffer);
        m_container->setPainter(painter);
    }

    if (m_selStart.isValid() && m_selEnd.isValid() && m_selStart != m_selEnd)
        drawSelection(painter);

    m_container->setPainter(nullptr);
    ++m_paintCount;
    if (roomSwitchPerfEnabled() && m_paintCount <= 2) {
        logPerfPhase("litehtml.paint",
                     timer.nsecsElapsed() / 1000,
                     QString(" paint_count=%1 width=%2 height=%3")
                       .arg(m_paintCount)
                       .arg(qRound(width()))
                       .arg(qRound(height())));
    }
}

void
LitehtmlItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);

    if (m_document && qRound(newGeometry.width()) != qRound(oldGeometry.width())) {
        if (churnPerfEnabled()) {
            komai::logging::ui()->info("[churn] geomWidth item={} oldW={} newW={}",
                                       (void *)this,
                                       qRound(oldGeometry.width()),
                                       qRound(newGeometry.width()));
        }
        relayout();
        update();
    } else if (qRound(newGeometry.height()) != qRound(oldGeometry.height())) {
        updateTextureSize();
    }
}

bool
LitehtmlItem::roomSwitchPerfEnabled() const
{
    return qEnvironmentVariableIsSet("KOMAI_ROOM_SWITCH_PERF") ||
           qEnvironmentVariableIsSet("KOMAI_PERF_ROOM_SWITCH");
}

void
LitehtmlItem::logPerfPhase(const char *phase, qint64 elapsedUs, const QString &extra) const
{
    if (!roomSwitchPerfEnabled())
        return;

    QString message =
      QStringLiteral("[room-switch-perf] phase=%1 room='%2' event='%3' elapsed_us=%4 "
                     "elapsed_ms=%5")
        .arg(QString::fromUtf8(phase),
             m_perfRoomId,
             m_perfEventId,
             QString::number(elapsedUs),
             QString::number(double(elapsedUs) / 1000.0, 'f', 3));
    if (!extra.isEmpty())
        message += extra;
    qInfo().noquote() << message;
}

void
LitehtmlItem::handleHoverMove(qreal x, qreal y)
{
    if (!m_document)
        return;

    int padLeft = static_cast<int>(m_leftPadding);
    int docX    = static_cast<int>(x) - padLeft;
    int docY    = static_cast<int>(y) - m_topInset;

    // Skip if the document-space position hasn't moved since the last call.
    QPoint docPos(docX, docY);
    if (docPos == m_lastHoverDocPos)
        return;
    m_lastHoverDocPos = docPos;

    litehtml::position::vector redraw;

    m_container->resetCursorState();
    m_document->on_mouse_over(docX, docY, docX, docY, redraw);

    const int spoilerIdx         = spoilerIndexAt(QPoint(static_cast<int>(x), static_cast<int>(y)));
    const bool overHiddenSpoiler = spoilerIdx >= 0 && !m_revealedSpoilers.contains(spoilerIdx);
    setHoveredSpoiler(overHiddenSpoiler);

    // If litehtml reported a pointer cursor, we're over a link.
    // Simulate a click in hover mode to capture the URL via on_anchor_click.
    QString url;
    if (m_container->isPointerCursor() && !overHiddenSpoiler) {
        m_container->setHoverMode(true);
        litehtml::position::vector dummy;
        m_document->on_lbutton_down(docX, docY, docX, docY, dummy);
        m_document->on_lbutton_up(docX, docY, docX, docY, dummy);
        url = m_container->lastHoveredUrl();
        m_container->setHoverMode(false);
    }

    // A link under a hidden spoiler's blur must not leak its URL through the
    // hover tooltip (hence the !overHiddenSpoiler guard above).
    if (m_hoveredLink != url) {
        m_hoveredLink = url;
        emit hoveredLinkChanged();
    }

    // Walk up to the enclosing <pre>; recompute the rect only when the
    // hovered block's identity changes.
    litehtml::element::const_ptr node = m_document->get_over_element();
    while (node && std::strcmp(node->get_tagName(), "pre") != 0)
        node = node->parent();

    if (node && node != m_codeBlock) {
        const litehtml::position pl = node->get_placement();
        if (pl.width > 0 && pl.height > 0) {
            m_codeBlock = node;
            // doc -> item space adds (padLeft, m_topInset).
            m_codeBlockRect =
              QRectF(pl.left() + padLeft, pl.top() + m_topInset, pl.width, pl.height);
            emit codeBlockRectChanged();
        } else {
            // No usable placement yet: treat as not hovered so the next
            // hover move retries.
            node = nullptr;
        }
    }
    setCodeBlockHovered(node && node == m_codeBlock);

    if (!redraw.empty())
        update();
}

void
LitehtmlItem::handleHoverLeave()
{
    if (!m_document)
        return;

    m_lastHoverDocPos = QPoint(-1, -1);

    // No hover-move signal arrives once the cursor leaves the item. Keep the
    // block and its rect: the pointer may be on the QML copy button overlay,
    // which needs both to stay actionable.
    setCodeBlockHovered(false);
    setHoveredSpoiler(false);

    litehtml::position::vector redraw;
    m_document->on_mouse_leave(redraw);

    if (!m_hoveredLink.isEmpty()) {
        m_hoveredLink.clear();
        emit hoveredLinkChanged();
    }

    if (!redraw.empty())
        update();
}

void
LitehtmlItem::clearCodeBlock()
{
    setCodeBlockHovered(false);
    m_codeBlock = nullptr;
    if (!m_codeBlockRect.isNull()) {
        m_codeBlockRect = QRectF();
        emit codeBlockRectChanged();
    }
}

void
LitehtmlItem::setCodeBlockHovered(bool hovered)
{
    if (m_codeBlockHovered == hovered)
        return;
    m_codeBlockHovered = hovered;
    emit codeBlockHoveredChanged();
}

bool
LitehtmlItem::copyCodeBlockText()
{
    if (!m_codeBlock)
        return false;

    litehtml::string txt;
    m_codeBlock->get_text(txt);
    if (txt.empty())
        return false;

    // Drop one trailing newline; a pasted block otherwise auto-runs its last
    // line in a terminal.
    if (txt.back() == '\n')
        txt.pop_back();
    QGuiApplication::clipboard()->setText(QString::fromStdString(txt));
    return true;
}

void
LitehtmlItem::mousePressEvent(QMouseEvent *event)
{
    if (!m_document || event->button() != Qt::LeftButton) {
        QQuickPaintedItem::mousePressEvent(event);
        return;
    }

    auto pos = event->position().toPoint();

    bool hadSelection = m_selStart.isValid() && m_selEnd.isValid() && m_selStart != m_selEnd;
    clearSelection();
    // Use screen-space coordinates for selection — text runs recorded during
    // paint include the leftPadding offset, so selection must match.
    m_selectStartPos          = pos;
    m_selectEndPos            = m_selectStartPos;
    m_selecting               = true;
    m_textSelectionSuppressed = false;
    m_wordDragExtend          = false;

    emit selectionDragBegan(static_cast<int>(event->modifiers()));

    forceActiveFocus();

    // litehtml needs document-space coordinates for link hit testing.
    int padLeft = static_cast<int>(m_leftPadding);
    int docX    = pos.x() - padLeft;
    int docY    = pos.y() - m_topInset;
    litehtml::position::vector redraw;
    m_document->on_lbutton_down(docX, docY, docX, docY, redraw);

    if (hadSelection || !redraw.empty())
        update();

    event->accept();
}

void
LitehtmlItem::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_document || !m_selecting) {
        QQuickPaintedItem::mouseMoveEvent(event);
        return;
    }

    // Prevent the parent Flickable/ListView from stealing the grab during selection.
    if (!keepMouseGrab())
        setKeepMouseGrab(true);

    // Let the QML drag-select controller decide whether the drag has crossed
    // into another message's row. Scene coords stay valid even after the
    // cursor has left our bounds — Qt keeps delivering moves to the implicit
    // grab holder.
    emit selectionDragMoved(mapToScene(event->position()));

    if (m_textSelectionSuppressed) {
        // QML has taken over the gesture for message-selection. Keep the
        // grab so we keep getting moves, but stop recomputing text-runs.
        event->accept();
        return;
    }

    auto pos       = event->position().toPoint();
    m_selectEndPos = pos;

    if (m_wordDragExtend) {
        // Extend the double-click selection by whole words: union the anchored
        // word with the word currently under the cursor.
        SelectionPoint cursStart;
        SelectionPoint cursEnd;
        if (wordRangeAt(hitTestTextRun(pos), cursStart, cursEnd)) {
            auto before = [](const SelectionPoint &a, const SelectionPoint &b) {
                return a.runIndex < b.runIndex ||
                       (a.runIndex == b.runIndex && a.charOffset < b.charOffset);
            };
            m_selStart = before(m_wordAnchorStart, cursStart) ? m_wordAnchorStart : cursStart;
            m_selEnd   = before(m_wordAnchorEnd, cursEnd) ? cursEnd : m_wordAnchorEnd;
        }
    } else {
        resolveSelection();
    }

    auto text = extractSelectedText();
    if (text != m_selectedText) {
        m_selectedText = text;
        emit selectedTextChanged();
    }

    update();
    event->accept();
}

void
LitehtmlItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_document || event->button() != Qt::LeftButton) {
        QQuickPaintedItem::mouseReleaseEvent(event);
        return;
    }

    bool wasSelecting         = m_selecting;
    bool wasSuppressed        = m_textSelectionSuppressed;
    auto modifiers            = event->modifiers();
    m_selecting               = false;
    m_textSelectionSuppressed = false;
    m_wordDragExtend          = false;
    setKeepMouseGrab(false);

    int padLeft = static_cast<int>(m_leftPadding);
    auto pos    = event->position().toPoint();
    int docX    = pos.x() - padLeft;
    int docY    = pos.y() - m_topInset;

    bool hasSelection = m_selStart.isValid() && m_selEnd.isValid() && m_selStart != m_selEnd;
    // A "click" is a press+release with no significant text-selection drag
    // and no escalation to message-level drag-select.
    bool wasClick = !hasSelection && !wasSuppressed;

    // Drain the drag-select state machine first so any modifier-click handler
    // we trigger below runs against a clean baseline.
    if (wasSelecting)
        emit selectionDragEnded();

    if (wasClick) {
        if (modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::MetaModifier)) {
            emit clickedWithCtrlOrMeta();
        } else if (modifiers.testFlag(Qt::ShiftModifier)) {
            emit clickedWithShift();
        } else if (const int spoilerIdx = spoilerIndexAt(pos);
                   spoilerIdx >= 0 && !m_revealedSpoilers.contains(spoilerIdx)) {
            // Click on a hidden spoiler reveals it — and never falls through
            // to litehtml, so a link under the blur can't be activated blind.
            m_revealedSpoilers.insert(spoilerIdx);
            // Re-evaluate hover state (cursor, link tooltip) for the now
            // visible content without waiting for the pointer to move.
            m_lastHoverDocPos = QPoint(-1, -1);
            handleHoverMove(pos.x(), pos.y());
            update();
        } else if (spoilerIdx >= 0 && m_hoveredLink.isEmpty()) {
            // Click on a revealed spoiler hides it again, unless the click is
            // on a link inside it — then the link wins (handled below).
            m_revealedSpoilers.remove(spoilerIdx);
            m_lastHoverDocPos = QPoint(-1, -1);
            handleHoverMove(pos.x(), pos.y());
            update();
        } else {
            // Forward to litehtml only for plain clicks — link activation
            // under a modifier-click would be incidental, not intended.
            litehtml::position::vector redraw;
            m_document->on_lbutton_up(docX, docY, docX, docY, redraw);
            if (!redraw.empty())
                update();
        }
    }

    event->accept();
}

void
LitehtmlItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_document || event->button() != Qt::LeftButton) {
        QQuickPaintedItem::mouseDoubleClickEvent(event);
        return;
    }

    auto pos = event->position().toPoint();

    SelectionPoint wordStart;
    SelectionPoint wordEnd;
    if (!wordRangeAt(hitTestTextRun(pos), wordStart, wordEnd)) {
        m_selecting = false;
        event->accept();
        return;
    }

    m_selStart = wordStart;
    m_selEnd   = wordEnd;

    // Arm word-granularity drag extension: keep the gesture "selecting" so a
    // drag following the double-click extends the selection word-by-word,
    // anchored on the just-selected word. mousePressEvent emitted
    // selectionDragBegan for this second click already, so the move/release
    // signals to the QML drag-select controller stay balanced.
    m_selecting       = true;
    m_wordDragExtend  = true;
    m_wordAnchorStart = wordStart;
    m_wordAnchorEnd   = wordEnd;
    m_selectStartPos  = pos;
    m_selectEndPos    = pos;

    const auto &runs = m_container->textRuns();
    const auto &text = runs[wordStart.runIndex].text;
    auto selected    = text.mid(wordStart.charOffset, wordEnd.charOffset - wordStart.charOffset);
    if (selected != m_selectedText) {
        m_selectedText = selected;
        emit selectedTextChanged();
    }

    forceActiveFocus();
    update();
    event->accept();
}

bool
LitehtmlItem::wordRangeAt(const SelectionPoint &sp,
                          SelectionPoint &wordStart,
                          SelectionPoint &wordEnd) const
{
    if (!sp.isValid())
        return false;
    const auto &runs = m_container->textRuns();
    if (sp.runIndex >= runs.size())
        return false;

    const auto &text = runs[sp.runIndex].text;
    int ws           = qBound(0, sp.charOffset, static_cast<int>(text.length()));
    int we           = ws;

    while (ws > 0 && !text[ws - 1].isSpace()) {
        // Step back by a whole codepoint (skip low surrogate).
        ws -= (ws >= 2 && text[ws - 1].isLowSurrogate()) ? 2 : 1;
    }
    while (we < text.length() && !text[we].isSpace()) {
        // Step forward by a whole codepoint (skip surrogate pair).
        we += text[we].isHighSurrogate() ? 2 : 1;
    }

    wordStart = {sp.runIndex, ws};
    wordEnd   = {sp.runIndex, we};
    return true;
}

bool
LitehtmlItem::event(QEvent *event)
{
    // Claim Escape at the ShortcutOverride layer so it reaches our
    // keyPressEvent below instead of activating the timeline-window
    // Escape Shortcut. The Shortcut path's handleEscape()/focusTextInput()
    // doesn't reliably move focus off this paint item, leaving the user
    // stuck after a text selection drag pulls focus here.
    if (event->type() == QEvent::ShortcutOverride) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            event->accept();
            return true;
        }
    }
    return QQuickPaintedItem::event(event);
}

void
LitehtmlItem::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy) && !m_selectedText.isEmpty()) {
        QGuiApplication::clipboard()->setText(m_selectedText);
        event->accept();
        return;
    }

    // Clicking inside a message body for text selection grabs focus here
    // (see mousePressEvent). Without explicit handling, Escape and
    // Tab/Backtab leave the user "stuck" — Escape's window-level Shortcut
    // doesn't reliably re-focus the composer from this paint-item focus
    // chain, and Tab has nowhere natural to go in a timeline of
    // non-focusable delegates. Drop our focus and let the QML side route
    // back to the composer.
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Tab ||
        event->key() == Qt::Key_Backtab) {
        clearSelection();
        update();
        setFocus(false);
        emit focusReleaseRequested();
        event->accept();
        return;
    }

    QQuickPaintedItem::keyPressEvent(event);
}

void
LitehtmlItem::clearSelection()
{
    m_selStart = {};
    m_selEnd   = {};
    if (!m_selectedText.isEmpty()) {
        m_selectedText.clear();
        emit selectedTextChanged();
    }
}

void
LitehtmlItem::suppressTextSelection()
{
    if (m_textSelectionSuppressed)
        return;
    m_textSelectionSuppressed = true;
    clearSelection();
    // Repaint to drop any selection-highlight that was already on screen
    // from the moves leading up to the QML controller's escalation.
    update();
}

SelectionPoint
LitehtmlItem::hitTestTextRun(const QPoint &pos) const
{
    const auto &runs = m_container->textRuns();
    for (int i = 0; i < runs.size(); ++i) {
        const auto &run = runs[i];
        if (pos.y() < run.rect.top() || pos.y() > run.rect.bottom())
            continue;
        if (pos.x() < run.rect.left() || pos.x() > run.rect.right())
            continue;

        // Find character offset within the run, stepping by whole codepoints
        // so offsets never land in the middle of a UTF-16 surrogate pair.
        QFontMetrics fm(run.font);
        int localX   = pos.x() - run.rect.x();
        int bestChar = run.text.length();
        for (int c = 0; c < run.text.length();) {
            int len      = run.text[c].isHighSurrogate() ? 2 : 1;
            int charW    = fm.horizontalAdvance(run.text.left(c + len));
            int prevW    = c > 0 ? fm.horizontalAdvance(run.text.left(c)) : 0;
            int midpoint = prevW + (charW - prevW) / 2;
            if (localX < midpoint) {
                bestChar = c;
                break;
            }
            c += len;
        }
        return {i, bestChar};
    }

    // Not inside any run — find the closest one.
    int bestRun   = -1;
    int bestDistY = INT_MAX;
    int bestDistX = INT_MAX;
    for (int i = 0; i < runs.size(); ++i) {
        const auto &run = runs[i];
        int dy          = 0;
        if (pos.y() < run.rect.top())
            dy = run.rect.top() - pos.y();
        else if (pos.y() > run.rect.bottom())
            dy = pos.y() - run.rect.bottom();

        int dx = 0;
        if (pos.x() < run.rect.left())
            dx = run.rect.left() - pos.x();
        else if (pos.x() > run.rect.right())
            dx = pos.x() - run.rect.right();

        if (dy < bestDistY || (dy == bestDistY && dx < bestDistX)) {
            bestDistY = dy;
            bestDistX = dx;
            bestRun   = i;
        }
    }

    if (bestRun < 0)
        return {};

    const auto &run = runs[bestRun];
    int charOff     = pos.x() <= run.rect.left() ? 0 : run.text.length();
    return {bestRun, charOff};
}

void
LitehtmlItem::resolveSelection()
{
    auto sp1 = hitTestTextRun(m_selectStartPos);
    auto sp2 = hitTestTextRun(m_selectEndPos);

    if (!sp1.isValid() || !sp2.isValid()) {
        m_selStart = {};
        m_selEnd   = {};
        return;
    }

    // Normalize so selStart comes before selEnd in document order.
    if (sp1.runIndex > sp2.runIndex ||
        (sp1.runIndex == sp2.runIndex && sp1.charOffset > sp2.charOffset)) {
        m_selStart = sp2;
        m_selEnd   = sp1;
    } else {
        m_selStart = sp1;
        m_selEnd   = sp2;
    }
}

QString
LitehtmlItem::extractSelectedText() const
{
    if (!m_selStart.isValid() || !m_selEnd.isValid())
        return {};

    const auto &runs = m_container->textRuns();
    if (m_selStart.runIndex >= runs.size() || m_selEnd.runIndex >= runs.size())
        return {};

    if (m_selStart == m_selEnd)
        return {};

    QString result;
    for (int i = m_selStart.runIndex; i <= m_selEnd.runIndex; ++i) {
        const auto &run = runs[i];

        // Insert newline between runs on different lines.
        // Same-line runs already contain any necessary whitespace — don't add extra.
        if (i > m_selStart.runIndex && !result.isEmpty()) {
            const auto &prevRun = runs[i - 1];
            if (qAbs(run.rect.y() - prevRun.rect.y()) > prevRun.rect.height() / 2)
                result += u'\n';
        }

        // Prepend list marker prefix (e.g. "- ") if this is the first run of a list item.
        if (!run.prefix.isEmpty() && (i != m_selStart.runIndex || m_selStart.charOffset == 0))
            result += run.prefix;

        if (i == m_selStart.runIndex && i == m_selEnd.runIndex) {
            result +=
              run.text.mid(m_selStart.charOffset, m_selEnd.charOffset - m_selStart.charOffset);
        } else if (i == m_selStart.runIndex) {
            result += run.text.mid(m_selStart.charOffset);
        } else if (i == m_selEnd.runIndex) {
            result += run.text.left(m_selEnd.charOffset);
        } else {
            result += run.text;
        }
    }

    return result;
}

void
LitehtmlItem::drawSelection(QPainter *painter)
{
    if (!m_selStart.isValid() || !m_selEnd.isValid())
        return;

    const auto &runs = m_container->textRuns();
    if (m_selStart.runIndex >= runs.size() || m_selEnd.runIndex >= runs.size())
        return;

    const auto palette = QGuiApplication::palette();
    QColor hlColor     = palette.highlight().color();
    hlColor.setAlpha(128);

    for (int i = m_selStart.runIndex; i <= m_selEnd.runIndex; ++i) {
        const auto &run = runs[i];
        QFontMetrics fm(run.font);
        int startChar = (i == m_selStart.runIndex) ? m_selStart.charOffset : 0;
        int endChar   = (i == m_selEnd.runIndex) ? m_selEnd.charOffset : run.text.length();

        if (startChar >= endChar)
            continue;

        int x1 = run.rect.x() + fm.horizontalAdvance(run.text.left(startChar));
        int x2 = run.rect.x() + fm.horizontalAdvance(run.text.left(endChar));

        QRect hlRect(x1, run.rect.y(), x2 - x1, run.rect.height());
        painter->fillRect(hlRect, hlColor);
    }
}

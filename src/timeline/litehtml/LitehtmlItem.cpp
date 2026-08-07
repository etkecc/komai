// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/litehtml/LitehtmlItem.h"

#include <QClipboard>
#include <QElapsedTimer>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QPalette>
#include <QPixmap>
#include <QQuickWindow>
#include <QSvgRenderer>
#include <QtMath>

#include <climits>
#include <cstring>

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
    clearCodeButton();

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
    // A reflow moves the <pre> but reuses the element, so the throttle keeps a stale rect.
    clearCodeButton();

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
    m_container->setPainter(painter);
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
    m_container->beginTextRunCollection();
    m_document->draw(reinterpret_cast<litehtml::uint_ptr>(painter), padLeft, m_topInset, &clip);
    m_container->endTextRunCollection();

    if (m_selStart.isValid() && m_selEnd.isValid() && m_selStart != m_selEnd)
        drawSelection(painter);

    // Painted last so a selection dragged across the corner can't tint the icon.
    if (m_codeButtonRect.isValid()) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_surfaceColor);
        painter->drawRoundedRect(m_codeButtonRect, 4, 4);

        const QString iconPath = m_codeCopied
                                   ? QStringLiteral(":/icons/icons/ui/checkmark.svg")
                                   : QStringLiteral(":/icons/icons/ui/copy.svg");
        const QRect iconRect = m_codeButtonRect.adjusted(4, 4, -4, -4);
        // Rasterize at device pixels so the icon stays crisp on a HiDPI panel.
        const qreal dpr = painter->device()->devicePixelRatioF();
        QPixmap icon(iconRect.size() * dpr);
        icon.setDevicePixelRatio(dpr);
        icon.fill(Qt::transparent);
        {
            QPainter ip(&icon);
            ip.setRenderHint(QPainter::Antialiasing, true);
            const QRectF logical(QPointF(0, 0), QSizeF(iconRect.size()));
            QSvgRenderer(iconPath).render(&ip, logical);
            // SourceIn recolors the icon ink to the text color so it tracks the theme.
            ip.setCompositionMode(QPainter::CompositionMode_SourceIn);
            ip.fillRect(logical, m_color);
        }
        painter->drawPixmap(iconRect, icon);
        painter->restore();
    }

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

    // If litehtml reported a pointer cursor, we're over a link.
    // Simulate a click in hover mode to capture the URL via on_anchor_click.
    QString url;
    if (m_container->isPointerCursor()) {
        m_container->setHoverMode(true);
        litehtml::position::vector dummy;
        m_document->on_lbutton_down(docX, docY, docX, docY, dummy);
        m_document->on_lbutton_up(docX, docY, docX, docY, dummy);
        url = m_container->lastHoveredUrl();
        m_container->setHoverMode(false);
    }

    if (m_hoveredLink != url) {
        m_hoveredLink = url;
        emit hoveredLinkChanged();
    }

    // Walk up to the enclosing <pre>; throttle recompute on block identity.
    litehtml::element::const_ptr node = m_document->get_over_element();
    while (node && std::strcmp(node->get_tagName(), "pre") != 0)
        node = node->parent();

    if (node != m_codeBlock) {
        // Left the old block: forget its copied state so a new block never shows its checkmark.
        m_codeCopied = false;
        if (m_codeRevertTimer)
            m_codeRevertTimer->stop();
        const litehtml::position pl = node ? node->get_placement() : litehtml::position();
        if (node && pl.width > 0 && pl.height > 0) {
            // doc -> item space adds (padLeft, m_topInset).
            const int box    = 24;
            const int gap    = 6;
            const int right  = static_cast<int>(pl.right()) + padLeft;
            const int top    = static_cast<int>(pl.top()) + m_topInset;
            m_codeBlock      = node;
            m_codeButtonRect = QRect(right - box - gap, top + gap, box, box);
        } else {
            // No usable placement yet: stay null so the next frame retries.
            clearCodeButton();
        }
        update();
        return;
    }

    if (!redraw.empty())
        update();
}

void
LitehtmlItem::handleHoverLeave()
{
    if (!m_document)
        return;

    m_lastHoverDocPos = QPoint(-1, -1);

    // No hover-move signal arrives once the cursor leaves the item, so clear here.
    const bool hadButton = m_codeButtonRect.isValid();
    clearCodeButton();

    litehtml::position::vector redraw;
    m_document->on_mouse_leave(redraw);

    if (!m_hoveredLink.isEmpty()) {
        m_hoveredLink.clear();
        emit hoveredLinkChanged();
    }

    if (hadButton || !redraw.empty())
        update();
}

void
LitehtmlItem::clearCodeButton()
{
    m_codeBlock      = nullptr;
    m_codeButtonRect = QRect();
    m_codeCopied     = false;
    if (m_codeRevertTimer)
        m_codeRevertTimer->stop();
}

void
LitehtmlItem::mousePressEvent(QMouseEvent *event)
{
    if (!m_document || event->button() != Qt::LeftButton) {
        QQuickPaintedItem::mousePressEvent(event);
        return;
    }

    auto pos = event->position().toPoint();

    // Above clearSelection()/on_lbutton_down on purpose: caught any later, the copy click starts a text-drag.
    if (m_codeButtonRect.isValid() && m_codeButtonRect.contains(pos)) {
        Q_ASSERT(m_codeBlock); // a valid rect always carries a resolved block
        litehtml::string txt;
        m_codeBlock->get_text(txt);
        // Drop one trailing newline; a pasted block otherwise auto-runs its last line in a terminal.
        if (!txt.empty() && txt.back() == '\n')
            txt.pop_back();
        QGuiApplication::clipboard()->setText(QString::fromStdString(txt));

        m_codeCopied = true;
        if (!m_codeRevertTimer) {
            m_codeRevertTimer = new QTimer(this);
            m_codeRevertTimer->setSingleShot(true);
            connect(m_codeRevertTimer, &QTimer::timeout, this, [this] {
                m_codeCopied = false;
                update();
            });
        }
        m_codeRevertTimer->start(1000);

        update();
        event->accept();
        return;
    }

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

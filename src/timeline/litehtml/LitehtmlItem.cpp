// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/litehtml/LitehtmlItem.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QPalette>
#include <QQuickWindow>
#include <QtMath>

#include <climits>

#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/litehtml/LitehtmlStylesheet.h"

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
        m_masterCss = generateMasterCss();
        rebuildDocument();
    });
    connect(this, &QQuickItem::activeFocusChanged, this, [this](bool hasFocus) {
        if (!hasFocus) {
            clearSelection();
            update();
        }
    });
    m_masterCss = generateMasterCss();
}

QString
LitehtmlItem::generateMasterCss()
{
    const auto palette = QGuiApplication::palette();
    return timeline::litehtml::generateMasterStylesheet(palette, m_font, m_compact);
}

void
LitehtmlItem::setHtml(const QString &html)
{
    if (m_html == html)
        return;
    m_html = html;
    emit htmlChanged();
    rebuildDocument();
}

void
LitehtmlItem::setColor(const QColor &color)
{
    if (m_color == color)
        return;
    m_color = color;
    m_container->setDefaultColor(color);
    emit colorChanged();
    if (m_document)
        update();
}

void
LitehtmlItem::setFont(const QFont &font)
{
    if (m_font == font)
        return;
    m_font = font;
    m_container->setDefaultFont(font);
    emit fontChanged();
    m_masterCss = generateMasterCss();
    rebuildDocument();
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
LitehtmlItem::setCompact(bool compact)
{
    if (m_compact == compact)
        return;
    m_compact = compact;
    emit compactChanged();
    m_masterCss = generateMasterCss();
    rebuildDocument();
}

void
LitehtmlItem::rebuildDocument()
{
    clearSelection();

    if (m_html.isEmpty()) {
        m_document.reset();
        setImplicitWidth(0);
        setImplicitHeight(0);
        update();
        return;
    }

    m_container->setDefaultFont(m_font);
    m_container->setDefaultColor(m_color);
    m_container->setEmojiFontFamily(UserSettings::instance()->uiFontEmojiFamily());

    m_document = litehtml::document::createFromString(m_html.toUtf8().constData(),
                                                      m_container,
                                                      litehtml::master_css,
                                                      m_masterCss.toUtf8().constData());

    relayout();
    update();
}

void
LitehtmlItem::relayout()
{
    if (!m_document)
        return;

    int itemWidth = static_cast<int>(width());

    // Skip layout before the item has a real width — EventDelegateChooser
    // will assign the available width first, which triggers geometryChange.
    if (itemWidth < 1)
        return;

    int w = qMax(1, itemWidth - static_cast<int>(m_leftPadding));
    m_container->setViewportSize(w, static_cast<int>(height()));
    m_document->render(w);
    int cw = m_document->content_width() + static_cast<int>(m_leftPadding);
    setImplicitWidth(cw);
    setImplicitHeight(m_document->height());
    updateTextureSize();
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

    m_container->beginTextRunCollection();
    m_document->draw(reinterpret_cast<litehtml::uint_ptr>(painter), padLeft, 0, &clip);
    m_container->endTextRunCollection();

    if (m_selStart.isValid() && m_selEnd.isValid() && m_selStart != m_selEnd)
        drawSelection(painter);

    m_container->setPainter(nullptr);
}

void
LitehtmlItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);

    if (m_document && qRound(newGeometry.width()) != qRound(oldGeometry.width())) {
        relayout();
        update();
    }
}

void
LitehtmlItem::handleHoverMove(qreal x, qreal y)
{
    if (!m_document)
        return;

    int padLeft = static_cast<int>(m_leftPadding);
    litehtml::position::vector redraw;
    int docX = static_cast<int>(x) - padLeft;
    int docY = static_cast<int>(y);

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

    if (!redraw.empty())
        update();
}

void
LitehtmlItem::handleHoverLeave()
{
    if (!m_document)
        return;

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
LitehtmlItem::mousePressEvent(QMouseEvent *event)
{
    if (!m_document || event->button() != Qt::LeftButton) {
        QQuickPaintedItem::mousePressEvent(event);
        return;
    }

    int padLeft = static_cast<int>(m_leftPadding);
    auto pos    = event->position().toPoint();
    int docX    = pos.x() - padLeft;
    int docY    = pos.y();

    bool hadSelection = m_selStart.isValid() && m_selEnd.isValid() && m_selStart != m_selEnd;
    clearSelection();
    m_selectStartPos = QPoint(docX, docY);
    m_selectEndPos   = m_selectStartPos;
    m_selecting      = true;

    forceActiveFocus();

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

    int padLeft    = static_cast<int>(m_leftPadding);
    auto pos       = event->position().toPoint();
    m_selectEndPos = QPoint(pos.x() - padLeft, pos.y());

    resolveSelection();

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

    m_selecting = false;
    setKeepMouseGrab(false);

    int padLeft = static_cast<int>(m_leftPadding);
    auto pos    = event->position().toPoint();
    int docX    = pos.x() - padLeft;
    int docY    = pos.y();

    // Only forward to litehtml (link activation) if this was a click, not a drag selection.
    bool hasSelection = m_selStart.isValid() && m_selEnd.isValid() && m_selStart != m_selEnd;
    if (!hasSelection) {
        litehtml::position::vector redraw;
        m_document->on_lbutton_up(docX, docY, docX, docY, redraw);
        if (!redraw.empty())
            update();
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

    m_selecting = false;

    int padLeft = static_cast<int>(m_leftPadding);
    auto pos    = event->position().toPoint();
    QPoint docPos(pos.x() - padLeft, pos.y());

    auto sp = hitTestTextRun(docPos);
    if (!sp.isValid()) {
        event->accept();
        return;
    }

    const auto &runs = m_container->textRuns();
    const auto &run  = runs[sp.runIndex];
    const auto &text = run.text;

    // Expand to word boundaries.
    int wordStart = sp.charOffset;
    int wordEnd   = sp.charOffset;

    while (wordStart > 0 && !text[wordStart - 1].isSpace()) {
        // Step back by a whole codepoint (skip low surrogate).
        wordStart -= (wordStart >= 2 && text[wordStart - 1].isLowSurrogate()) ? 2 : 1;
    }
    while (wordEnd < text.length() && !text[wordEnd].isSpace()) {
        // Step forward by a whole codepoint (skip surrogate pair).
        wordEnd += text[wordEnd].isHighSurrogate() ? 2 : 1;
    }

    m_selStart = {sp.runIndex, wordStart};
    m_selEnd   = {sp.runIndex, wordEnd};

    auto selected = text.mid(wordStart, wordEnd - wordStart);
    if (selected != m_selectedText) {
        m_selectedText = selected;
        emit selectedTextChanged();
    }

    forceActiveFocus();
    update();
    event->accept();
}

void
LitehtmlItem::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy) && !m_selectedText.isEmpty()) {
        QGuiApplication::clipboard()->setText(m_selectedText);
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

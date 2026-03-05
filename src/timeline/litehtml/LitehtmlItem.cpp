// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/litehtml/LitehtmlItem.h"

#include <QGuiApplication>
#include <QPalette>
#include <QQuickWindow>
#include <QtMath>

#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/litehtml/LitehtmlStylesheet.h"

LitehtmlItem::LitehtmlItem(QQuickItem *parent)
  : QQuickPaintedItem(parent)
  , m_container(new LitehtmlContainer(this))
{
    setAcceptHoverEvents(true);
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
    if (m_html.isEmpty()) {
        m_document.reset();
        setImplicitWidth(0);
        setImplicitHeight(0);
        update();
        return;
    }

    m_container->setDefaultFont(m_font);
    m_container->setDefaultColor(m_color);

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

    m_document->draw(reinterpret_cast<litehtml::uint_ptr>(painter), padLeft, 0, &clip);
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
LitehtmlItem::hoverMoveEvent(QHoverEvent *event)
{
    if (!m_document) {
        QQuickPaintedItem::hoverMoveEvent(event);
        return;
    }

    int padLeft = static_cast<int>(m_leftPadding);
    litehtml::position::vector redraw;
    auto pos = event->position().toPoint();
    int docX = pos.x() - padLeft;
    int docY = pos.y();

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
    setCursor(url.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);

    if (!redraw.empty())
        update();
}

void
LitehtmlItem::hoverLeaveEvent(QHoverEvent *event)
{
    if (!m_document) {
        QQuickPaintedItem::hoverLeaveEvent(event);
        return;
    }

    litehtml::position::vector redraw;
    m_document->on_mouse_leave(redraw);

    if (!m_hoveredLink.isEmpty()) {
        m_hoveredLink.clear();
        emit hoveredLinkChanged();
    }
    unsetCursor();

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
    litehtml::position::vector redraw;
    auto pos = event->position().toPoint();
    int docX = pos.x() - padLeft;
    int docY = pos.y();
    m_document->on_lbutton_down(docX, docY, docX, docY, redraw);

    if (!redraw.empty())
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

    int padLeft = static_cast<int>(m_leftPadding);
    litehtml::position::vector redraw;
    auto pos = event->position().toPoint();
    int docX = pos.x() - padLeft;
    int docY = pos.y();
    m_document->on_lbutton_up(docX, docY, docX, docY, redraw);

    if (!redraw.empty())
        update();

    event->accept();
}

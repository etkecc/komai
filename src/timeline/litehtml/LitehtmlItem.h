// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QFont>
#include <QPoint>
#include <QQmlEngine>
#include <QQuickPaintedItem>
#include <QString>
#include <memory>

#include <litehtml.h>

#include "timeline/litehtml/LitehtmlContainer.h"

struct SelectionPoint
{
    int runIndex   = -1;
    int charOffset = 0;

    bool isValid() const { return runIndex >= 0; }
    bool operator==(const SelectionPoint &o) const
    {
        return runIndex == o.runIndex && charOffset == o.charOffset;
    }
    bool operator!=(const SelectionPoint &o) const { return !(*this == o); }
};

class LitehtmlItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString html READ html WRITE setHtml NOTIFY htmlChanged)
    Q_PROPERTY(QString hoveredLink READ hoveredLink NOTIFY hoveredLinkChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY selectedTextChanged)
    Q_PROPERTY(qreal leftPadding READ leftPadding WRITE setLeftPadding NOTIFY leftPaddingChanged)
    Q_PROPERTY(bool compact READ compact WRITE setCompact NOTIFY compactChanged)

public:
    explicit LitehtmlItem(QQuickItem *parent = nullptr);

    QString html() const { return m_html; }
    void setHtml(const QString &html);

    QString hoveredLink() const { return m_hoveredLink; }

    QColor color() const { return m_color; }
    void setColor(const QColor &color);

    QFont font() const { return m_font; }
    void setFont(const QFont &font);

    QString selectedText() const { return m_selectedText; }

    qreal leftPadding() const { return m_leftPadding; }
    void setLeftPadding(qreal padding);

    bool compact() const { return m_compact; }
    void setCompact(bool compact);

    void paint(QPainter *painter) override;

signals:
    void htmlChanged();
    void hoveredLinkChanged();
    void colorChanged();
    void fontChanged();
    void selectedTextChanged();
    void leftPaddingChanged();
    void compactChanged();
    void linkActivated(const QString &link);

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void rebuildDocument();
    void relayout();
    void updateTextureSize();
    QString generateMasterCss();

    SelectionPoint hitTestTextRun(const QPoint &pos) const;
    void resolveSelection();
    QString extractSelectedText() const;
    void drawSelection(QPainter *painter);
    void clearSelection();

    QString m_html;
    QString m_hoveredLink;
    QColor m_color;
    QFont m_font;
    QString m_selectedText;
    qreal m_leftPadding = 0;
    bool m_compact      = false;
    QString m_masterCss;

    LitehtmlContainer *m_container = nullptr;
    litehtml::document::ptr m_document;

    bool m_selecting = false;
    QPoint m_selectStartPos;
    QPoint m_selectEndPos;
    SelectionPoint m_selStart;
    SelectionPoint m_selEnd;
};

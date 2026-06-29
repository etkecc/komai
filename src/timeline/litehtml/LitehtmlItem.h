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
    Q_PROPERTY(QColor linkColor READ linkColor WRITE setLinkColor NOTIFY linkColorChanged)
    Q_PROPERTY(
      QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY surfaceColorChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(QString perfRoomId READ perfRoomId WRITE setPerfRoomId)
    Q_PROPERTY(QString perfEventId READ perfEventId WRITE setPerfEventId)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY selectedTextChanged)
    Q_PROPERTY(qreal leftPadding READ leftPadding WRITE setLeftPadding NOTIFY leftPaddingChanged)
    Q_PROPERTY(
      qreal rightPadding READ rightPadding WRITE setRightPadding NOTIFY rightPaddingChanged)
    Q_PROPERTY(bool compact READ compact WRITE setCompact NOTIFY compactChanged)

public:
    explicit LitehtmlItem(QQuickItem *parent = nullptr);

    QString html() const { return m_html; }
    void setHtml(const QString &html);

    QString hoveredLink() const { return m_hoveredLink; }

    QColor color() const { return m_color; }
    void setColor(const QColor &color);

    QColor linkColor() const { return m_linkColor; }
    void setLinkColor(const QColor &color);

    QColor surfaceColor() const { return m_surfaceColor; }
    void setSurfaceColor(const QColor &color);

    QFont font() const { return m_font; }
    void setFont(const QFont &font);

    QString perfRoomId() const { return m_perfRoomId; }
    void setPerfRoomId(const QString &roomId) { m_perfRoomId = roomId; }

    QString perfEventId() const { return m_perfEventId; }
    void setPerfEventId(const QString &eventId) { m_perfEventId = eventId; }

    QString selectedText() const { return m_selectedText; }

    qreal leftPadding() const { return m_leftPadding; }
    void setLeftPadding(qreal padding);

    qreal rightPadding() const { return m_rightPadding; }
    void setRightPadding(qreal padding);

    bool compact() const { return m_compact; }
    void setCompact(bool compact);

    void paint(QPainter *painter) override;

    Q_INVOKABLE void handleHoverMove(qreal x, qreal y);
    Q_INVOKABLE void handleHoverLeave();
    // Called by the timeline's drag-select controller once it has decided the
    // in-progress text-selection drag has crossed into another row and should
    // escalate into message-level selection. Clears the visual text selection
    // and silences further text-selection updates for the remainder of the
    // gesture, while keeping the mouse grab so move signals keep flowing.
    Q_INVOKABLE void suppressTextSelection();

signals:
    void htmlChanged();
    void hoveredLinkChanged();
    void colorChanged();
    void linkColorChanged();
    void surfaceColorChanged();
    void fontChanged();
    void selectedTextChanged();
    void leftPaddingChanged();
    void rightPaddingChanged();
    void compactChanged();
    void linkActivated(const QString &link);
    // Emitted when the focused item should release focus to whatever the
    // surrounding view considers the natural fallback target. We can't
    // call into a QML singleton from here without coupling, so the QML
    // delegate is expected to wire this to the right action (typically
    // `TimelineManager.requestEscape()`).
    void focusReleaseRequested();
    // Emitted on left-button press inside the item. The QML drag-select
    // controller uses this to reset any latching state from a prior gesture.
    // `modifiers` is the `Qt::KeyboardModifiers` value at press time (passed
    // as `int` for straightforward QML interop); the controller treats any
    // Ctrl/Meta/Shift bit as "additive drag — preserve prior selection".
    void selectionDragBegan(int modifiers);
    // Emitted on every mouse move while a left-button drag is in progress
    // (the implicit grab keeps us receiving these even after the cursor has
    // left our bounds). `scenePos` is the cursor in scene coordinates; the
    // QML controller maps it to the ListView to decide whether the drag has
    // crossed into another message's row.
    void selectionDragMoved(QPointF scenePos);
    // Emitted when the left-button drag completes.
    void selectionDragEnded();
    // Click (no drag) with a Ctrl / Meta modifier — the QML side routes this
    // to the same message-selection toggle as the row-level Ctrl-click. The
    // existing TapHandler on `selectionToggleSurface` is disabled for
    // litehtml-backed rows because it would otherwise grab the press and
    // prevent the litehtml from starting a text-selection drag.
    void clickedWithCtrlOrMeta();
    // Click (no drag) with a Shift modifier — routed to the message range
    // select handler (same as Shift-click on `selectionToggleSurface`).
    void clickedWithShift();

protected:
    void componentComplete() override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;

private:
    void requestDocumentRebuild();
    void rebuildDocument();
    void relayout();
    void updateTextureSize();
    QString generateMasterCss();

    SelectionPoint hitTestTextRun(const QPoint &pos) const;
    // Expand a hit-test point to the boundaries of the word it lands in,
    // within that point's text run. Returns false if the point is not on a
    // valid run. Used for double-click word selection and word-granularity
    // drag extension.
    bool wordRangeAt(const SelectionPoint &sp,
                     SelectionPoint &wordStart,
                     SelectionPoint &wordEnd) const;
    void resolveSelection();
    QString extractSelectedText() const;
    void drawSelection(QPainter *painter);
    void clearSelection();

    bool roomSwitchPerfEnabled() const;
    void logPerfPhase(const char *phase, qint64 elapsedUs, const QString &extra = {}) const;

    QString m_html;
    QString m_hoveredLink;
    QColor m_color;
    QColor m_linkColor;
    QColor m_surfaceColor;
    QFont m_font;
    QString m_selectedText;
    qreal m_leftPadding  = 0;
    qreal m_rightPadding = 0;
    bool m_compact       = false;
    // Top inset reserved for emoji glyph ink that overshoots the text-line
    // ascent (issue #78). Computed during relayout from the diff between
    // the emoji font's ascent at 1.4em and the text font's ascent.
    int m_topInset = 0;
    QString m_masterCss;
    bool m_masterCssDirty = true;
    bool m_rebuildPending = false;
    QString m_perfRoomId;
    QString m_perfEventId;
    int m_rebuildCount  = 0;
    int m_relayoutCount = 0;
    int m_paintCount    = 0;

    LitehtmlContainer *m_container = nullptr;
    litehtml::document::ptr m_document;

    bool m_selecting = false;
    // While true, mouseMoveEvent emits `selectionDragMoved` but skips
    // resolveSelection() / extractSelectedText() — the QML side has taken
    // over the gesture for message selection. Cleared on press / release.
    bool m_textSelectionSuppressed = false;
    // Set by a double-click word selection: a subsequent drag extends the
    // selection by whole words, anchored on the double-clicked word
    // (m_wordAnchorStart/End). Cleared on press / release.
    bool m_wordDragExtend = false;
    SelectionPoint m_wordAnchorStart;
    SelectionPoint m_wordAnchorEnd;
    QPoint m_selectStartPos;
    QPoint m_selectEndPos;
    SelectionPoint m_selStart;
    SelectionPoint m_selEnd;

    // Hover throttling: last document-space position passed to on_mouse_over.
    QPoint m_lastHoverDocPos{-1, -1};
};

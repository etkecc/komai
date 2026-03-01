// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KomaiCursorShape.h"

#include <QCursor>

KomaiCursorShape::KomaiCursorShape(QQuickItem *parent)
  : QQuickItem(parent)
  , currentShape_(Qt::CursorShape::ArrowCursor)
{
}

Qt::CursorShape
KomaiCursorShape::cursorShape() const
{
    return cursor().shape();
}

void
KomaiCursorShape::setCursorShape(Qt::CursorShape cursorShape)
{
    if (currentShape_ == cursorShape)
        return;

    currentShape_ = cursorShape;
    setCursor(cursorShape);
    emit cursorShapeChanged();
}

#include "moc_KomaiCursorShape.cpp"

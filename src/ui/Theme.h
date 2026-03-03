// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QPalette>
#include <QQmlEngine>

#include "ThemeRegistry.h"

class Theme final : public QPalette
{
    Q_GADGET
    QML_ANONYMOUS

    Q_PROPERTY(QColor sidebarBackground READ sidebarBackground CONSTANT)
    Q_PROPERTY(QColor separator READ separator CONSTANT)
    Q_PROPERTY(QColor attention READ attention CONSTANT)
    Q_PROPERTY(QColor success READ success CONSTANT)
    Q_PROPERTY(QColor error READ error CONSTANT)
    Q_PROPERTY(QColor warning READ warning CONSTANT)
    Q_PROPERTY(QColor online READ online CONSTANT)
    Q_PROPERTY(QColor unavailable READ unavailable CONSTANT)
public:
    Theme() {}
    explicit Theme(QStringView theme);
    static QPalette paletteFromTheme(QStringView theme);

    QColor sidebarBackground() const { return sidebarBackground_; }
    QColor separator() const { return separator_; }
    QColor attention() const { return attention_; }
    QColor success() const { return success_; }
    QColor error() const { return error_; }
    QColor warning() const { return warning_; }
    QColor online() const { return QColor(0x00, 0xcc, 0x66); }
    QColor unavailable() const { return QColor(0xff, 0x99, 0x33); }

private:
    QColor sidebarBackground_, separator_, attention_, success_, error_, warning_;
};

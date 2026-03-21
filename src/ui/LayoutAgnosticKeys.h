// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class LayoutAgnosticKeys : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    enum class LatinKey
    {
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        O,
        R,
        T,
        U,
        Count,
    };
    Q_ENUM(LatinKey)

    explicit LayoutAgnosticKeys(QObject *parent = nullptr);

    Q_INVOKABLE bool matchesLatinKey(LatinKey latinKey, int key, quint32 nativeScanCode) const;
};

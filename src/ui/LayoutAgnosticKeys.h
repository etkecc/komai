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
    explicit LayoutAgnosticKeys(QObject *parent = nullptr);

    Q_INVOKABLE bool
    matchesLatinKey(const QString &latinKey, int key, quint32 nativeScanCode) const;
};

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace komai {

//! Describes a single actionable button attached to a Snackbar notification.
//! Passed to QML as a QVariantMap so the payload stays serializable and
//! thread-safe — no raw callbacks cross the C++/QML boundary.
struct NotificationAction
{
    //! What to do when the user clicks the action button.
    enum Kind : int
    {
        //! `target` is opened via QDesktopServices::openUrl (file://, https://, mailto:, ...).
        OpenUrl = 0,
        //! `target` is a file:// URL; the containing folder is opened with the file highlighted.
        RevealInFolder = 1,
    };

    int kind = OpenUrl;
    QString label;
    //! Optional qrc path like ":/icons/icons/ui/open-externally.svg".
    QString iconSource;
    QUrl target;

    QVariantMap toVariantMap() const
    {
        QVariantMap map;
        map.insert(QStringLiteral("kind"), kind);
        map.insert(QStringLiteral("label"), label);
        map.insert(QStringLiteral("iconSource"), iconSource);
        map.insert(QStringLiteral("target"), target);
        return map;
    }
};

inline QVariantList
toVariantList(const QList<NotificationAction> &actions)
{
    QVariantList list;
    list.reserve(actions.size());
    for (const auto &a : actions)
        list.append(a.toVariantMap());
    return list;
}

} // namespace komai

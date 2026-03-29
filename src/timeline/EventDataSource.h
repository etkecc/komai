// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractItemModel>
#include <QModelRoleData>
#include <QString>
#include <QVariant>

/// Minimal interface for models that can feed EventDelegateChooser.
///
/// Both the legacy TimelineModel and the new MatrixTimelineModel implement
/// this so that the delegate chooser can read event data by ID without
/// caring which backend produced the model.
class EventDataSource : public QAbstractListModel
{
    Q_OBJECT

public:
    using QAbstractListModel::QAbstractListModel;

    /// Return the value of a single role for a given event/item ID.
    /// @p relatedTo is the parent event ID when resolving reply context.
    virtual QVariant dataById(const QString &id, int role, const QString &relatedTo) = 0;

    /// Fetch multiple roles for a given event/item ID in one call.
    virtual void multiData(const QString &id,
                           const QString &relatedTo,
                           QModelRoleDataSpan roleDataSpan) const = 0;

    /// Return the model row index for a given event/item ID, or -1 if absent.
    virtual int idToIndex(const QString &id) const = 0;
};

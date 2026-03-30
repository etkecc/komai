// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineFilter.h"

#include "TimelineModel.h"

TimelineFilter::TimelineFilter(QObject *parent)
  : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(false);
}

void
TimelineFilter::startFiltering()
{
    filteringInProgress_ = false;
    cachedCount          = rowCount();
    emit isFilteringChanged();
    invalidateFilter();
}

void
TimelineFilter::continueFiltering()
{
    startFiltering();
}

bool
TimelineFilter::event(QEvent *ev)
{
    return QSortFilterProxyModel::event(ev);
}

void
TimelineFilter::setThreadId(const QString &t)
{
    if (threadId == t)
        return;

    threadId = t;
    emit threadIdChanged();
    startFiltering();
}

void
TimelineFilter::setFilterNotifications(bool filter)
{
    if (filterByNotifications_ == filter)
        return;

    filterByNotifications_ = filter;
    emit filterNotificationsChanged();
    startFiltering();
}

void
TimelineFilter::setContentFilter(const QString &c)
{
    if (contentFilter == c)
        return;

    contentFilter = c;
    emit contentFilterChanged();
    startFiltering();
}

void
TimelineFilter::fetchAgain()
{
    cachedCount = rowCount();
}

void
TimelineFilter::sourceDataChanged(const QModelIndex &topLeft,
                                  const QModelIndex &bottomRight,
                                  const QVector<int> &roles)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);

    if (!roles.contains(TimelineModel::Roles::Body) && !roles.contains(TimelineModel::ThreadId) &&
        !roles.contains(TimelineModel::Notificationlevel)) {
        return;
    }

    invalidateFilter();
}

void
TimelineFilter::setSource(QAbstractItemModel *s)
{
    if (sourceModel() == s)
        return;

    setSourceModel(s);
    cachedCount            = rowCount();
    incrementalSearchIndex = 0;
    emit sourceChanged();
    emit isFilteringChanged();
    invalidateFilter();
}

QAbstractItemModel *
TimelineFilter::source() const
{
    return sourceModel();
}

void
TimelineFilter::setCurrentIndex(int idx)
{
    if (currentIndex_ == idx)
        return;

    currentIndex_ = idx;
    emit currentIndexChanged();
}

int
TimelineFilter::currentIndex() const
{
    return currentIndex_;
}

bool
TimelineFilter::isFiltering() const
{
    return filteringInProgress_;
}

bool
TimelineFilter::filterAcceptsRow(int source_row, const QModelIndex &) const
{
    if (!sourceModel())
        return false;

    if (threadId.isEmpty() && contentFilter.isEmpty() && !filterByNotifications_)
        return true;

    const auto idx = sourceModel()->index(source_row, 0);

    if (!contentFilter.isEmpty() && !sourceModel()
                                       ->data(idx, TimelineModel::Body)
                                       .toString()
                                       .contains(contentFilter, Qt::CaseInsensitive)) {
        return false;
    }

    if (filterByNotifications_ &&
        sourceModel()->data(idx, TimelineModel::Notificationlevel)
            .value<qml_mtx_events::NotificationLevel>() !=
          qml_mtx_events::NotificationLevel::Highlight) {
        return false;
    }

    if (threadId.isEmpty())
        return true;

    return sourceModel()->data(idx, TimelineModel::EventId) == threadId ||
           sourceModel()->data(idx, TimelineModel::ThreadId) == threadId;
}

#include "moc_TimelineFilter.cpp"

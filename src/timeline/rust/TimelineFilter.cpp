// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineFilter.h"

#include <QCoreApplication>
#include <QEvent>

#include "MatrixTimelineModel.h"
#include "timeline/TimelineEventTypes.h"

/// Searching currently can be done incrementally. For that we define a specific role to filter on
/// and then process that role in chunk. This is the `FilterRole`. Of course we need to then also
/// send proper update signals. Filtering then works as follows:
///
/// - At first no range is filtered (incrementalSearchIndex == 0).
/// - Then, when filtering is requested, we start posting events to the
/// event loop with lower than low priority (low prio - 1). The only thing those events do is
/// increment the incrementalSearchIndex and emit a dataChanged for that range of events.
/// - This then causes those events to be reevaluated if they should be visible.

static int FilterRole = Qt::UserRole * 3;

static QEvent::Type
getFilterEventType()
{
    static QEvent::Type t = static_cast<QEvent::Type>(QEvent::registerEventType());
    return t;
}

TimelineFilter::TimelineFilter(QObject *parent)
  : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterRole(FilterRole);
}

bool
TimelineFilter::hasActiveFilter() const
{
    return !threadId.isEmpty() || !contentFilter.isEmpty() || filterByNotifications_;
}

void
TimelineFilter::startFiltering()
{
    incrementalSearchIndex  = 0;
    sourceCountAtLastFetch_ = 0;
    emit isFilteringChanged();

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    endFilterChange();
#else
    invalidateFilter();
#endif

    beginResetModel();
    endResetModel();

    continueFiltering();
}

void
TimelineFilter::continueFiltering()
{
    if (auto s = source(); s) {
        if (s->rowCount() > incrementalSearchIndex) {
            auto ev = new QEvent(getFilterEventType());
            QCoreApplication::postEvent(this, ev, Qt::LowEventPriority - 1);
        } else {
            if (incrementalSearchIndex != std::numeric_limits<int>::max()) {
                incrementalSearchIndex = std::numeric_limits<int>::max();
                emit isFilteringChanged();
            }
            fetchAgain();
        }
    }
}

bool
TimelineFilter::event(QEvent *ev)
{
    if (ev->type() == getFilterEventType()) {
        if (incrementalSearchIndex < std::numeric_limits<int>::max()) {
            int orgIndex = incrementalSearchIndex;
            incrementalSearchIndex += 100;

            if (auto s = source(); s) {
                auto count = s->rowCount();
                if (incrementalSearchIndex >= count) {
                    incrementalSearchIndex = std::numeric_limits<int>::max();
                }
                s->dataChanged(s->index(orgIndex, 0),
                               s->index(std::min(incrementalSearchIndex, count - 1), 0),
                               {FilterRole});

                continueFiltering();
            }
            emit isFilteringChanged();
        }
        return true;
    }
    return QSortFilterProxyModel::event(ev);
}

void
TimelineFilter::setThreadId(const QString &t)
{
    if (this->threadId != t) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        beginFilterChange();
#endif

        this->threadId = t;

        emit threadIdChanged();
        startFiltering();
        fetchMore({});
    }
}

void
TimelineFilter::setFilterNotifications(bool filter)
{
    if (this->filterByNotifications_ != filter) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        beginFilterChange();
#endif
        this->filterByNotifications_ = filter;

        emit filterNotificationsChanged();
        startFiltering();
        fetchMore({});
    }
}

void
TimelineFilter::setContentFilter(const QString &c)
{
    if (this->contentFilter != c) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        beginFilterChange();
#endif
        this->contentFilter = c;

        emit contentFilterChanged();
        startFiltering();
        fetchMore({});
    }
}

void
TimelineFilter::fetchAgain()
{
    if (!hasActiveFilter())
        return;

    if (auto s = source(); s && incrementalSearchIndex == std::numeric_limits<int>::max()) {
        int currentSourceCount = s->rowCount();

        // If the source didn't grow since our last request, pagination is
        // exhausted — stop requesting to avoid an infinite loop.
        if (sourceCountAtLastFetch_ > 0 && currentSourceCount <= sourceCountAtLastFetch_) {
            cachedCount = this->rowCount();
            return;
        }

        sourceCountAtLastFetch_ = currentSourceCount;
        cachedCount             = this->rowCount();
        emit requestMoreData();
    }
}

void
TimelineFilter::onSourceRowsInserted()
{
    if (!hasActiveFilter())
        return;

    // New rows arrived (likely from pagination). If the incremental pass
    // already finished, kick fetchAgain() to check whether we should keep
    // loading more history.
    if (incrementalSearchIndex == std::numeric_limits<int>::max() && sourceCountAtLastFetch_ > 0)
        fetchAgain();
}

void
TimelineFilter::sourceDataChanged(const QModelIndex &topLeft,
                                  const QModelIndex &bottomRight,
                                  const QVector<int> &roles)
{
    using R = komai::MatrixTimelineModel::Roles;
    if (!roles.contains(R::Body) && !roles.contains(R::ThreadId) &&
        !roles.contains(R::Notificationlevel))
        return;

    if (auto s = source()) {
        s->dataChanged(topLeft, bottomRight, {FilterRole});
    }
}

void
TimelineFilter::setSource(QAbstractItemModel *s)
{
    if (auto orig = this->source(); orig != s) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
#endif

        cachedCount             = 0;
        incrementalSearchIndex  = 0;
        sourceCountAtLastFetch_ = 0;

        if (orig) {
            disconnect(
              orig, &QAbstractItemModel::dataChanged, this, &TimelineFilter::sourceDataChanged);
            disconnect(
              orig, &QAbstractItemModel::rowsInserted, this, &TimelineFilter::onSourceRowsInserted);
        }

        this->setSourceModel(s);

        if (s) {
            connect(s,
                    &QAbstractItemModel::dataChanged,
                    this,
                    &TimelineFilter::sourceDataChanged,
                    Qt::QueuedConnection);
            connect(s,
                    &QAbstractItemModel::rowsInserted,
                    this,
                    &TimelineFilter::onSourceRowsInserted,
                    Qt::QueuedConnection);
        }

        incrementalSearchIndex = 0;

        emit sourceChanged();
        emit isFilteringChanged();

        if (s && (!threadId.isEmpty() || !contentFilter.isEmpty() || filterByNotifications_))
            continueFiltering();

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        endFilterChange();
#else
        invalidateFilter();
#endif
    }
}

QAbstractItemModel *
TimelineFilter::source() const
{
    return sourceModel();
}

void
TimelineFilter::setCurrentIndex(int /*idx*/)
{
    // currentIndex tracking is not used in the matrix-sdk path
}

int
TimelineFilter::currentIndex() const
{
    return -1;
}

bool
TimelineFilter::isFiltering() const
{
    return incrementalSearchIndex != std::numeric_limits<int>::max() &&
           !(threadId.isEmpty() && contentFilter.isEmpty());
}

bool
TimelineFilter::filterAcceptsRow(int source_row, const QModelIndex &) const
{
    // this chunk is still unfiltered.
    if (source_row > incrementalSearchIndex)
        return false;

    if (threadId.isEmpty() && contentFilter.isEmpty() && !filterByNotifications_)
        return true;

    if (auto s = sourceModel()) {
        using R  = komai::MatrixTimelineModel::Roles;
        auto idx = s->index(source_row, 0);

        if (!contentFilter.isEmpty() &&
            !s->data(idx, R::Body).toString().contains(contentFilter, Qt::CaseInsensitive)) {
            return false;
        }

        if (filterByNotifications_ &&
            s->data(idx, R::Notificationlevel).value<qml_mtx_events::NotificationLevel>() !=
              qml_mtx_events::Highlight) {
            return false;
        }

        if (threadId.isEmpty()) {
            return true;
        }

        return s->data(idx, R::EventId) == threadId || s->data(idx, R::ThreadId) == threadId;
    } else {
        return true;
    }
}

#include "moc_TimelineFilter.cpp"

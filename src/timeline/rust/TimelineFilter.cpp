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

    // Mirror QML's `count` property change notification.  QSortFilterProxyModel
    // emits the granular rowsInserted/rowsRemoved/modelReset/layoutChanged
    // signals when the filtered view's row count changes; QML bindings on
    // `count` only re-evaluate when `countChanged` fires.
    connect(this, &QAbstractItemModel::rowsInserted, this, &TimelineFilter::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &TimelineFilter::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &TimelineFilter::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &TimelineFilter::countChanged);
}

bool
TimelineFilter::hasActiveFilter() const
{
    return !threadId.isEmpty() || !contentFilter.isEmpty();
}

void
TimelineFilter::startFiltering()
{
    incrementalSearchIndex  = 0;
    sourceCountAtLastFetch_ = 0;
    waitingForData_         = false;
    emit isFilteringChanged();

    // Use a full model reset instead of invalidateFilter() /
    // endFilterChange(). The granular re-filter pass streams
    // rowsRemoved/rowsInserted diffs into the live ListView while delegate
    // bindings (previousMessageUserId etc.) re-enter the proxy via
    // dataByIndex(), rebuilding lazy mappings mid-binding and corrupting
    // row geometry (visual message overlap, #181). The reset rebuilds the
    // mapping atomically (endResetModel() triggers resetInternalData())
    // before any delegate binding evaluates - same rationale as
    // setCollapseThreadReplies().
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
        this->threadId = t;

        emit threadIdChanged();
        startFiltering();
        fetchMore({});
    }
}

void
TimelineFilter::setCollapseThreadReplies(bool collapse)
{
    if (collapseThreadReplies_ == collapse)
        return;

    collapseThreadReplies_ = collapse;

    // Use a full model reset instead of invalidateFilter().
    // The delegate bindings (previousMessageUserId etc.) call
    // dataByIndex() which can trigger lazy proxy mapping rebuilds
    // during evaluation, changing chat.count mid-binding and causing
    // a binding loop + stale grouping data (visual message overlap).
    // A model reset forces the mapping to rebuild atomically before
    // any delegate bindings evaluate.
    beginResetModel();
    endResetModel();

    // Emit after the model is consistent so the ListView model
    // binding (which may switch from perRoomModel to this proxy)
    // sees already-correct rows.
    emit collapseThreadRepliesChanged();
}

void
TimelineFilter::setContentFilter(const QString &c)
{
    if (this->contentFilter != c) {
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
            waitingForData_ = false;
            cachedCount     = this->rowCount();
            emit isFilteringChanged();
            return;
        }

        sourceCountAtLastFetch_ = currentSourceCount;
        cachedCount             = this->rowCount();
        waitingForData_         = true;
        emit isFilteringChanged();
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
TimelineFilter::onSourceModelReset()
{
    // After a source model reset the proxy may not re-apply the collapse
    // filter correctly. Force a full re-evaluation when collapse is active.
    if (collapseThreadReplies_) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        beginFilterChange();
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        endFilterChange();
#else
        invalidateFilter();
#endif
    }
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
        waitingForData_         = false;

        if (orig) {
            disconnect(
              orig, &QAbstractItemModel::dataChanged, this, &TimelineFilter::sourceDataChanged);
            disconnect(
              orig, &QAbstractItemModel::rowsInserted, this, &TimelineFilter::onSourceRowsInserted);
            disconnect(
              orig, &QAbstractItemModel::modelReset, this, &TimelineFilter::onSourceModelReset);
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
            connect(s,
                    &QAbstractItemModel::modelReset,
                    this,
                    &TimelineFilter::onSourceModelReset,
                    Qt::QueuedConnection);
        }

        incrementalSearchIndex = 0;

        emit sourceChanged();
        emit isFilteringChanged();

        if (s && (!threadId.isEmpty() || !contentFilter.isEmpty()))
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

int
TimelineFilter::rowForEventId(const QString &eventId) const
{
    auto *src = qobject_cast<komai::MatrixTimelineModel *>(sourceModel());
    if (!src)
        return -1;

    const int sourceRow = src->rowForEventId(eventId);
    if (sourceRow < 0)
        return -1;

    // mapFromSource() returns an invalid index (row() == -1) when the source
    // row is filtered out, which is exactly the contract callers expect.
    return mapFromSource(src->index(sourceRow, 0)).row();
}

QVariantMap
TimelineFilter::itemAt(int row) const
{
    QVariantMap itemData;
    if (row < 0 || row >= rowCount())
        return itemData;

    const auto idx     = index(row, 0);
    const auto roleMap = roleNames();
    for (auto it = roleMap.cbegin(); it != roleMap.cend(); ++it)
        itemData.insert(QString::fromUtf8(it.value()), data(idx, it.key()));
    return itemData;
}

bool
TimelineFilter::isFiltering() const
{
    if (!hasActiveFilter())
        return false;

    return incrementalSearchIndex != std::numeric_limits<int>::max() || waitingForData_;
}

bool
TimelineFilter::filterAcceptsRow(int source_row, const QModelIndex &) const
{
    // this chunk is still unfiltered (only applies to content/thread search).
    if (!threadId.isEmpty() || !contentFilter.isEmpty()) {
        if (source_row > incrementalSearchIndex)
            return false;
    }

    // Thread collapsing: hide non-root thread replies.
    if (collapseThreadReplies_) {
        if (auto s = sourceModel()) {
            using R  = komai::MatrixTimelineModel::Roles;
            auto idx = s->index(source_row, 0);
            if (!s->data(idx, R::ThreadId).toString().isEmpty() &&
                !s->data(idx, R::IsThreadRoot).toBool()) {
                return false;
            }
        }
    }

    if (threadId.isEmpty() && contentFilter.isEmpty())
        return true;

    if (auto s = sourceModel()) {
        using R  = komai::MatrixTimelineModel::Roles;
        auto idx = s->index(source_row, 0);

        if (!contentFilter.isEmpty() &&
            !s->data(idx, R::Body).toString().contains(contentFilter, Qt::CaseInsensitive)) {
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

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QQmlEngine>
#include <QSortFilterProxyModel>
#include <QString>

namespace komai {
class MatrixTimelineModel;
}

class TimelineFilter : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString filterByThread READ filterByThread WRITE setThreadId NOTIFY threadIdChanged)
    Q_PROPERTY(QString filterByContent READ filterByContent WRITE setContentFilter NOTIFY
                 contentFilterChanged)
    Q_PROPERTY(bool collapseThreadReplies READ collapseThreadReplies WRITE setCollapseThreadReplies
                 NOTIFY collapseThreadRepliesChanged)
    Q_PROPERTY(QAbstractItemModel *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(bool filteringInProgress READ isFiltering NOTIFY isFilteringChanged)

public:
    explicit TimelineFilter(QObject *parent = nullptr);

    QString filterByThread() const { return threadId; }
    QString filterByContent() const { return contentFilter; }
    bool collapseThreadReplies() const { return collapseThreadReplies_; }
    QAbstractItemModel *source() const;
    int currentIndex() const;
    bool isFiltering() const;

    void setThreadId(const QString &t);
    void setContentFilter(const QString &t);
    void setCollapseThreadReplies(bool collapse);
    void setSource(QAbstractItemModel *s);
    void setCurrentIndex(int idx);

    Q_INVOKABLE QVariant dataByIndex(int i, int role = Qt::DisplayRole) const
    {
        return data(index(i, 0), role);
    }

    /// Row index of `eventId` in *this* (proxy) model, or -1 if the event is
    /// not in the backing model or is currently filtered out. Lets QML
    /// position the timeline ListView by event id while it is bound to the
    /// filtered proxy rather than the raw MatrixTimelineModel — the two have
    /// different row spaces, so reusing a source-model row index against the
    /// proxy mispositions the view (issue #139 walk-mode jumps).
    Q_INVOKABLE int rowForEventId(const QString &eventId) const;

    /// Role-name-keyed dump of the proxy row, matching
    /// `MatrixTimelineModel::itemAt`'s contract so callers can query the
    /// ListView's bound model uniformly without caring whether it is the raw
    /// source or this filter.  Without this the drag-select / walk-mode
    /// helpers must keep two model identities in sync — `ListView.indexAt`
    /// returns proxy rows while `itemAt` was only available on the source —
    /// and mismatching the two skips/selects the wrong rows when collapse
    /// hides thread replies.
    Q_INVOKABLE QVariantMap itemAt(int row) const;

    bool event(QEvent *ev) override;

signals:
    void threadIdChanged();
    void contentFilterChanged();
    void collapseThreadRepliesChanged();
    void sourceChanged();
    void currentIndexChanged();
    void isFilteringChanged();
    void requestMoreData();

private slots:
    void fetchAgain();
    void sourceDataChanged(const QModelIndex &topLeft,
                           const QModelIndex &bottomRight,
                           const QVector<int> &roles);
    void onSourceRowsInserted();
    void onSourceModelReset();

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    void startFiltering();
    void continueFiltering();
    bool hasActiveFilter() const;

    QString threadId, contentFilter;
    bool collapseThreadReplies_ = false;
    int cachedCount = 0, incrementalSearchIndex = 0;
    int sourceCountAtLastFetch_ = 0;
    bool waitingForData_        = false;
};

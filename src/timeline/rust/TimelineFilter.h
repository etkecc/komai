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

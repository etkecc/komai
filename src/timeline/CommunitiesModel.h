// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QQmlEngine>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>

#include <array>
#include <unordered_map>

#include "matrix/MatrixStateTypes.h"

class CommunitiesModel;

class FilteredCommunitiesModel final : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use Communities.filtered() to create a FilteredCommunitiesModel")

public:
    explicit FilteredCommunitiesModel(CommunitiesModel *model, QObject *parent = nullptr);
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsRow(int sourceRow, const QModelIndex &) const override;
    Q_INVOKABLE int filterIdToIndex(const QString &filterId) const;
    Q_INVOKABLE QString filterIdAt(int row) const;
};

class CommunitiesModel final : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Communities)
    QML_SINGLETON

    Q_PROPERTY(QString currentFilterId READ currentFilterId WRITE setCurrentFilterId NOTIFY
                 currentFilterIdChanged RESET resetCurrentFilterId)
    Q_PROPERTY(QStringList tags READ tags NOTIFY tagsChanged)
    Q_PROPERTY(QStringList tagsWithDefault READ tagsWithDefault NOTIFY tagsChanged)
    Q_PROPERTY(bool containsSubspaces READ containsSubspaces NOTIFY containsSubspacesChanged)
    Q_PROPERTY(int maxDepth READ maxDepth NOTIFY containsSubspacesChanged)

    struct FixedFilterRow
    {
        QString id;
        QString icon;
        int unreadRoomCount = 0;
        bool hasHighlight   = false;
    };

public:
    // Fixed row indices for the communities sidebar.
    static constexpr int kRowAllRooms   = 0;
    static constexpr int kRowPeople     = 1;
    static constexpr int kRowBots       = 2;
    static constexpr int kRowGroups     = 3;
    static constexpr int kFixedRowCount = 4; // spaces and tags start after this

    enum Roles
    {
        AvatarUrl = Qt::UserRole,
        DisplayName,
        Tooltip,
        Collapsed,
        Collapsible,
        Hidden,
        Parent,
        Depth,
        Id,
        UnreadMessages,
        HasLoudNotification,
        UnreadIndicatorsHidden,
        IsDirect,
        IsEmpty,
    };

    struct FlatTree
    {
        struct Elem
        {
            QString id;
            int depth = 0;

            int unreadRoomCount = 0;
            bool hasHighlight   = false;

            bool collapsed = false;
        };

        std::vector<Elem> tree;

        int size() const { return static_cast<int>(tree.size()); }
        int indexOf(const QString &s) const
        {
            for (int i = 0; i < size(); i++)
                if (tree[i].id == s)
                    return i;
            return -1;
        }
        int lastChild(int index) const
        {
            if (index >= size() || index < 0)
                return index;
            const auto depth = tree[index].depth;
            int i            = index + 1;
            for (; i < size(); i++)
                if (tree[i].depth <= depth)
                    break;
            return i - 1;
        }
        int parent(int index) const
        {
            if (index >= size() || index < 0)
                return -1;
            const auto depth = tree[index].depth;
            if (depth == 0)
                return -1;
            int i = index - 1;
            for (; i >= 0; i--)
                if (tree[i].depth < depth)
                    break;
            return i;
        }

        void storeCollapsed();
        void restoreCollapsed();
    };

    CommunitiesModel(QObject *parent);

    static CommunitiesModel *instance() { return instance_; }

    static CommunitiesModel *create(QQmlEngine *qmlEngine, QJSEngine *)
    {
        // The instance has to exist before it is used. We cannot replace it.
        Q_ASSERT(instance_);

        // The engine has to have the same thread affinity as the singleton.
        Q_ASSERT(qmlEngine->thread() == instance_->thread());

        // There can only be one engine accessing the singleton.
        static QJSEngine *s_engine = nullptr;
        if (s_engine)
            Q_ASSERT(qmlEngine == s_engine);
        else
            s_engine = qmlEngine;

        QJSEngine::setObjectOwnership(instance_, QJSEngine::CppOwnership);
        return instance_;
    }

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        (void)parent;
        return kFixedRowCount + tags_.size() + spaceOrder_.size();
    }
    QVariant data(const QModelIndex &index, int role) const override;
    QString fixedFilterDisplayName(int row) const;
    QString fixedFilterTooltip(int row) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    bool hasRoomsForFixedFilter(const QString &filterId) const;

    bool containsSubspaces() const
    {
        for (const auto &e : spaceOrder_.tree)
            if (e.depth > 0)
                return true;
        return false;
    }

    int maxDepth() const
    {
        int max = 0;
        for (const auto &e : spaceOrder_.tree)
            if (e.depth > max)
                max = e.depth;
        return max;
    }

public slots:
    void initializeSidebar();
    void clear();
    QString currentFilterId() const { return currentFilterId_; }
    void setCurrentFilterId(const QString &filterId);
    bool trySwitchToSpace(const QString &spaceId);
    void resetCurrentFilterId()
    {
        currentFilterId_.clear();
        emit currentFilterIdChanged(currentFilterId_);
    }
    QStringList tags() const { return tags_; }
    QStringList tagsWithDefault() const
    {
        QStringList tagsWD = tags_;
        tagsWD.prepend(QStringLiteral("m.lowpriority"));
        tagsWD.prepend(QStringLiteral("m.favourite"));
        tagsWD.removeOne(QStringLiteral("m.server_notice"));
        tagsWD.removeDuplicates();
        return tagsWD;
    }
    void toggleGlobalExclude(QString filterId);
    void toggleFilterUnreadIndicators(QString filterId);

    Q_INVOKABLE bool areFilterUnreadIndicatorsHidden(const QString &filterId) const;
    Q_INVOKABLE bool isGlobalExcluded(const QString &filterId) const;
    Q_INVOKABLE bool isSpaceHidden(const QString &spaceId) const;
    Q_INVOKABLE void toggleSpaceHidden(const QString &spaceId);
    Q_INVOKABLE QVariantList spaceEntries() const;

    // Cascade-aware queries: a space is "effectively" hidden / excluded if it
    // OR any of its ancestor spaces carries the corresponding flag.
    //
    // For the exclusion query, stopAtBareId acts as a ceiling: when the walk
    // reaches that ancestor it stops without considering it (or anything above).
    // Pass it when the caller is already filtering BY a specific space, so
    // exclusions on that space — or anything above it — don't apply within the
    // user's explicit per-space view.
    bool isSpaceEffectivelyHidden(const QString &spaceId) const;
    bool isSpaceEffectivelyExcludedFromAllRooms(const QString &spaceId,
                                                const QString &stopAtBareId = {}) const;

    FilteredCommunitiesModel *filtered() { return new FilteredCommunitiesModel(this, this); }

signals:
    void currentFilterIdChanged(QString filterId);
    void globalExcludesChanged();
    void unreadIndicatorsHiddenFiltersChanged();
    void tagsChanged();
    void hiddenSpacesChanged();
    void containsSubspacesChanged();

private slots:
    void handleRoomlistDataChanged(const QModelIndex &topLeft,
                                   const QModelIndex &bottomRight,
                                   const QList<int> &roles);
    void handleRoomlistModelReset();
    void handleRoomlistRowsInserted(const QModelIndex &parent, int first, int last);
    void handleRoomlistRowsRemoved(const QModelIndex &parent, int first, int last);

private:
    QStringList tags_;
    QString currentFilterId_;
    QStringList globalExcludedFilterIds_;
    QStringList unreadIndicatorsHiddenFilterIds_;
    QStringList hiddenSpaceIds_;
    FlatTree spaceOrder_;
    std::map<QString, RoomInfo> spaces_;

    struct BadgeCounts
    {
        int unreadRoomCount = 0;
        bool hasHighlight   = false;
    };
    std::unordered_map<QString, BadgeCounts> tagBadgeCache;

    void computeFilterBadges();
    void recomputeFilterBadges();
    std::array<FixedFilterRow, kFixedRowCount> fixedFilters_ = {{
      {"", QStringLiteral(":/icons/icons/ui/world.svg"), {}},
      {"people", QStringLiteral(":/icons/icons/ui/person.svg"), {}},
      {"bot", QStringLiteral(":/icons/icons/ui/robot-sparkle.svg"), {}},
      {"group", QStringLiteral(":/icons/icons/ui/people.svg"), {}},
    }};

    friend class FilteredCommunitiesModel;

    inline static CommunitiesModel *instance_ = nullptr;
};

inline int
FilteredCommunitiesModel::filterIdToIndex(const QString &filterId) const
{
    for (int row = 0; row < rowCount(); row++) {
        if (data(index(row, 0), CommunitiesModel::Roles::Id).toString() == filterId)
            return row;
    }

    return rowCount() > 0 ? 0 : -1;
}

inline QString
FilteredCommunitiesModel::filterIdAt(int row) const
{
    if (row < 0 || row >= rowCount())
        return {};

    return data(index(row, 0), CommunitiesModel::Roles::Id).toString();
}

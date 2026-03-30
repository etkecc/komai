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
#include "matrix/MatrixSyncUpdate.h"

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

class SpaceItem
{
    Q_GADGET

    Q_PROPERTY(QString roomid MEMBER roomid CONSTANT)
    Q_PROPERTY(QString name MEMBER name CONSTANT)
    Q_PROPERTY(int treeIndex MEMBER treeIndex CONSTANT)

    Q_PROPERTY(bool childValid MEMBER childValid CONSTANT)
    Q_PROPERTY(bool parentValid MEMBER parentValid CONSTANT)
    Q_PROPERTY(bool canonical MEMBER canonical CONSTANT)

    Q_PROPERTY(bool canEditParent MEMBER canEditParent CONSTANT)
    Q_PROPERTY(bool canEditChild MEMBER canEditChild CONSTANT)

public:
    SpaceItem() {}
    SpaceItem(QString roomid_,
              QString name_,
              int treeIndex_,
              bool childValid_,
              bool parentValid_,
              bool canonical_,
              bool canEditChild_,
              bool canEditParent_)
      : roomid(std::move(roomid_))
      , name(std::move(name_))
      , treeIndex(treeIndex_)
      , childValid(childValid_)
      , parentValid(parentValid_)
      , canonical(canonical_)
      , canEditParent(canEditParent_)
      , canEditChild(canEditChild_)
    {
    }

    QString roomid, name;
    int treeIndex   = 0;
    bool childValid = false, parentValid = false, canonical = false;
    bool canEditParent = false, canEditChild = false;
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
        BadgesHidden,
        IsDirect,
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

    Q_INVOKABLE QVariantList spaceChildrenListFromIndex(const QString &room, int idx = -1) const;
    Q_INVOKABLE void updateSpaceStatus(QString space,
                                       QString room,
                                       bool setParent,
                                       bool setChild,
                                       bool canonical) const;

public slots:
    void initializeSidebar();
    void sync(const komai::SyncUpdate &sync);
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
    void toggleFilterBadges(QString filterId);

    Q_INVOKABLE bool areFilterBadgesHidden(const QString &filterId) const;
    Q_INVOKABLE bool isGlobalExcluded(const QString &filterId) const;

    FilteredCommunitiesModel *filtered() { return new FilteredCommunitiesModel(this, this); }

signals:
    void currentFilterIdChanged(QString filterId);
    void globalExcludesChanged();
    void badgesHiddenFiltersChanged();
    void tagsChanged();
    void containsSubspacesChanged();

private:
    QStringList tags_;
    QString currentFilterId_;
    QStringList globalExcludedFilterIds_;
    QStringList badgesHiddenFilterIds_;
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
    bool hasPeopleRooms_                                     = false;
    bool hasBotRooms_                                        = false;
    bool hasGroupRooms_                                      = false;

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

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>

class RoomDirectoryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool loadingMoreRooms READ loadingMoreRooms NOTIFY loadingMoreRoomsChanged)
    Q_PROPERTY(
      bool reachedEndOfPagination READ reachedEndOfPagination NOTIFY reachedEndOfPaginationChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QString server READ server NOTIFY serverChanged)
    Q_PROPERTY(bool hasResults READ hasResults NOTIFY hasResultsChanged)
    Q_PROPERTY(
      int totalRoomCountEstimate READ totalRoomCountEstimate NOTIFY totalRoomCountEstimateChanged)
    Q_PROPERTY(int mrsRoomCount READ mrsRoomCount NOTIFY mrsRoomCountChanged)
    Q_PROPERTY(QString mrsLanguageFilter READ mrsLanguageFilter WRITE setMrsLanguageFilter NOTIFY
                 mrsLanguageFilterChanged)
    Q_PROPERTY(int maxMemberFilter READ maxMemberFilter WRITE setMaxMemberFilter NOTIFY
                 maxMemberFilterChanged)

public:
    explicit RoomDirectoryModel(QObject *parent = nullptr, const QString &server = {});

    enum Roles
    {
        Name = Qt::UserRole,
        Id,
        AvatarUrl,
        Topic,
        MemberCount,
        Previewable,
        CanJoin,
        IsSpace,
        Alias,
    };
    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex &index, int role) const override;

    inline int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        (void)parent;
        return static_cast<int>(publicRoomsData_.size());
    }

    bool canFetchMore(const QModelIndex &) const override { return canFetchMore_; }

    bool loadingMoreRooms() const { return loadingMoreRooms_; }

    bool reachedEndOfPagination() const { return reachedEndOfPagination_; }

    QString errorString() const { return errorString_; }

    QString server() const { return server_; }

    bool hasResults() const { return !publicRoomsData_.empty(); }

    int totalRoomCountEstimate() const { return totalRoomCountEstimate_; }

    int mrsRoomCount() const { return mrsRoomCount_; }

    QString mrsLanguageFilter() const { return mrsLanguageFilter_; }
    void setMrsLanguageFilter(const QString &lang);

    int maxMemberFilter() const { return maxMemberFilter_; }
    void setMaxMemberFilter(int max);

    Q_INVOKABLE static QStringList availableLanguages();
    Q_INVOKABLE void clearResults();

    void fetchMore(const QModelIndex &) override;

    Q_INVOKABLE void joinRoom(const int &index = -1);
    Q_INVOKABLE QStringList knownServers(const QString &prefix) const;
    Q_INVOKABLE void fetchMrsRoomCount(const QString &serverName);

signals:
    void loadingMoreRoomsChanged();
    void reachedEndOfPaginationChanged();
    void errorStringChanged();
    void serverChanged();
    void hasResultsChanged();
    void totalRoomCountEstimateChanged();
    void mrsRoomCountChanged();
    void mrsLanguageFilterChanged();
    void maxMemberFilterChanged();

public slots:
    void setMatrixServer(const QString &s = QLatin1String(""));
    void setSearchTerm(const QString &f);

private:
    struct RoomDirectoryEntry
    {
        QString displayName;
        QString roomId;
        QString roomServerName;
        QString avatarUrl;
        QString topic;
        QString canonicalAlias;
        uint64_t memberCount = 0;
        bool canPreview      = false;
        bool isSpace         = false;
    };

private slots:
    void displayRooms(uint64_t generation,
                      QVector<RoomDirectoryEntry> rooms,
                      const QString &next_batch,
                      const QString &search_term,
                      const QString &server,
                      const QString &since,
                      int totalRoomCountEstimate);

    void handleFetchError(uint64_t generation,
                          const QString &errorMessage,
                          const QString &search_term,
                          const QString &server,
                          const QString &since);

private:
    bool canJoinRoom(const QString &room) const;

    static constexpr uint64_t limit_ = 50;

    QString server_;
    QString userSearchString_;
    QString prevBatch_;
    QString nextBatch_;
    bool canFetchMore_{false};
    bool loadingMoreRooms_{false};
    uint64_t fetchGeneration_{0};
    int filterSkipCount_{0};
    static constexpr int maxFilterSkips_ = 10;
    bool reachedEndOfPagination_{false};
    QVector<RoomDirectoryEntry> publicRoomsData_;
    QString errorString_;
    int totalRoomCountEstimate_{-1};
    int mrsRoomCount_{-1};
    bool mrsRoomCountLoading_{false};
    QString mrsLanguageFilter_;
    int maxMemberFilter_{0};
    mutable QStringList cachedKnownServers_;
    mutable bool knownServersCached_{false};

    QStringList getViasForRoom(const RoomDirectoryEntry &room) const;
    void resetDisplayedData();
    void fetchMrsStats(const QString &statsUrl);
};

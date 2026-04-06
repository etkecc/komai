// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <optional>

#include "komai-rust-cxxbridge/ffi.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "timeline/EventDataSource.h"

namespace komai {

class MatrixTimelineModel final : public EventDataSource
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    /// Canonical roles — values 0–45 match TimelineModel::Roles exactly so
    /// that QML references like Room.Day resolve to the correct role number
    /// without a translation layer.  Extra roles live at Qt::UserRole.
    enum Roles
    {
        // --- TimelineModel-compatible block (values must match 1:1) ---
        Type = 0,
        TypeString,              // 1
        IsOnlyEmoji,             // 2
        Body,                    // 3
        FormattedBody,           // 4
        HasFormattedBody,        // 5
        FormattedStateEvent,     // 6
        StateEventIconSource,    // 7
        IsSender,                // 8
        UserId,                  // 9
        UserName,                // 10
        UserPowerlevel,          // 11
        Day,                     // 12
        Timestamp,               // 13
        Url,                     // 14
        ThumbnailUrl,            // 15
        Duration,                // 16
        Blurhash,                // 17
        Filename,                // 18
        Filesize,                // 19
        FilesizeBytes,           // 20
        MimeType,                // 21
        OriginalHeight,          // 22
        OriginalWidth,           // 23
        ProportionalHeight,      // 24
        EventId,                 // 25
        Status,                  // 26 — TimelineModel::State, role name "status"
        IsEdited,                // 27
        IsEditable,              // 28
        IsEncrypted,             // 29
        IsStateEvent,            // 30
        Trustlevel,              // 31
        Notificationlevel,       // 32
        EncryptionError,         // 33
        ReplyTo,                 // 34
        ThreadId,                // 35
        Reactions,               // 36
        Room,                    // 37
        RoomId,                  // 38
        _RoomNameGap  = 39,      // placeholder for TimelineModel::RoomName
        _RoomTopicGap = 40,      // placeholder for TimelineModel::RoomTopic
        CallType,                // 41
        Dump,                    // 42
        RelatedEventCacheBuster, // 43
        IsHiddenEvent,           // 44
        FileTypeIconSource,      // 45

        // --- Extra roles (not in TimelineModel) ---
        ItemId = Qt::UserRole,
        SenderAvatarUrl,
        ReactionsSummary,
        PreviousTimestamp,
        PreviousSenderId,
        PreviousItemKind,
        DeliveryState,
        IsThreadRoot,
    };

    explicit MatrixTimelineModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setRoomId(const QString &roomId);

    // EventDataSource interface
    QVariant dataById(const QString &id, int role, const QString &relatedTo) override;
    void multiData(const QString &id,
                   const QString &relatedTo,
                   QModelRoleDataSpan roleDataSpan) const override;
    int idToIndex(const QString &id) const override;

    Q_INVOKABLE int rowForEventId(const QString &eventId) const;
    Q_INVOKABLE QVariantMap itemAt(int row) const;
    Q_INVOKABLE QVariantMap previewDataForEvent(const QString &eventId,
                                                const QString &relatedTo = QString()) const;
    Q_INVOKABLE QString avatarUrl(const QString &userId) const;
    static ::rust::Vec<::komai::rust::HtmlPillAvatar>
    buildPillAvatars(const QVector<MatrixTimelineItem> &items);
    Q_INVOKABLE QVariant dataByIndex(int row, int role) const { return data(index(row), role); }
    Q_INVOKABLE QString copyTextForEventIds(const QVariantList &eventIds, bool plainText) const;

    /// Quick per-event accessors for toolbar / action bar — O(1) after ID lookup.
    Q_INVOKABLE QString userNameForEvent(const QString &eventId) const;
    Q_INVOKABLE QString userIdForEvent(const QString &eventId) const;
    Q_INVOKABLE QString bodyForEvent(const QString &eventId) const;
    Q_INVOKABLE QString typeStringForEvent(const QString &eventId) const;
    Q_INVOKABLE QString filenameForEvent(const QString &eventId) const;

    std::optional<MatrixTimelineItem> itemByEventId(const QString &eventId) const;
    QVector<MatrixTimelineItem> visibleItemsSnapshot() const { return items_; }

    int count() const { return items_.size(); }
    int hiddenCount() const;
    bool redactItemByEventId(const QString &eventId);
    bool revealOlderItems(int additionalCount);
    void replaceItems(QVector<MatrixTimelineItem> items);
    void clear();

signals:
    void countChanged();
    void specialEffectsTriggered(const QStringList &effectNames);
    void aboutToReplaceContent();
    void contentReplaced();

private:
    QVariant replyData(const MatrixTimelineItem &parentItem, int role) const;
    int
    rowForEventIdInItems(const QVector<MatrixTimelineItem> &items, const QString &eventId) const;
    QVector<MatrixTimelineItem> visibleItemsForRawCount(int rawVisibleCount) const;

    void refreshDerivedFields();
    int previousVisibleRowFrom(int row) const;
    void replaceVisibleItems(QVector<MatrixTimelineItem> items);
    void applyRedactedPresentation(MatrixTimelineItem &item) const;
    void applyOptimisticRedactions(QVector<MatrixTimelineItem> &items);
    void emitEffectsForPrependedItems(const QVector<MatrixTimelineItem> &nextItems);

    QString roomId_;
    QVector<MatrixTimelineItem> allItems_;
    QVector<MatrixTimelineItem> items_;
    int revealedItemCount_ = 0;
    QSet<QString> optimisticRedactedEventIds_;
};

} // namespace komai

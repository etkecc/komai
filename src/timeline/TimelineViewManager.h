// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QPalette>
#include <QQmlEngine>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

#include "NavigationHistory.h"

class QQuickItem;
class QQuickTextDocument;

class UserSettings;
class ChatPage;
class ImagePackListModel;
class CallManager;
class VerificationManager;
class InviteesModel;
class MemberList;
class CommunitiesModel;
class RoomlistModel;
class PresenceEmitter;
class UserProfile;
class RoomSettings;
class FilteredRoomlistModel;
class QAbstractItemModel;

namespace komai {
class MatrixTimelineModel;
}

class MatrixPendingAttachmentUpload final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString thumbnail READ thumbnail CONSTANT)
    Q_PROPERTY(QString body READ body WRITE setBody NOTIFY bodyChanged)
    Q_PROPERTY(QString filename READ filename WRITE setFilename NOTIFY filenameChanged)
    Q_PROPERTY(QString mimetype READ mimetype CONSTANT)
    Q_PROPERTY(QString fileTypeIconSource READ fileTypeIconSource CONSTANT)

public:
    MatrixPendingAttachmentUpload(QString filePath,
                                  QString filename,
                                  QString mimeType,
                                  QString fileTypeIconSource,
                                  QString thumbnail,
                                  QObject *parent = nullptr)
      : QObject(parent)
      , filePath_(std::move(filePath))
      , filename_(std::move(filename))
      , mimeType_(std::move(mimeType))
      , fileTypeIconSource_(std::move(fileTypeIconSource))
      , thumbnail_(std::move(thumbnail))
    {
    }

    QString thumbnail() const { return thumbnail_; }
    QString body() const { return body_; }
    QString filename() const { return filename_; }
    QString mimetype() const { return mimeType_; }
    QString fileTypeIconSource() const { return fileTypeIconSource_; }
    QString filePath() const { return filePath_; }

    void setBody(QString body)
    {
        if (body_ == body)
            return;
        body_ = std::move(body);
        emit bodyChanged();
    }

    void setFilename(QString filename)
    {
        if (filename_ == filename)
            return;
        filename_ = std::move(filename);
        emit filenameChanged();
    }

signals:
    void bodyChanged();
    void filenameChanged();

private:
    QString filePath_;
    QString body_;
    QString filename_;
    QString mimeType_;
    QString fileTypeIconSource_;
    QString thumbnail_;
};

class TimelineViewManager final : public QObject
{
    Q_OBJECT

    QML_NAMED_ELEMENT(TimelineManager)
    QML_SINGLETON

    Q_PROPERTY(bool waitingForFirstSync MEMBER waitingForFirstSync_ READ waitingForFirstSync NOTIFY
                 waitingForFirstSyncChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)
    Q_PROPERTY(QVector<QString> ignoredUsers READ getIgnoredUsers NOTIFY ignoredUsersChanged)
    Q_PROPERTY(int colorRevision READ colorRevision NOTIFY colorRevisionChanged)
    Q_PROPERTY(QAbstractItemModel *matrixTimelineModel READ matrixTimelineModel NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(
      int matrixTimelineItemCount READ matrixTimelineItemCount NOTIFY matrixTimelineStateChanged)
    Q_PROPERTY(
      bool matrixTimelineLoading READ matrixTimelineLoading NOTIFY matrixTimelineStateChanged)
    Q_PROPERTY(bool matrixTimelineAttachmentSending READ matrixTimelineAttachmentSending NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(QVariantList matrixTimelineAttachments READ matrixTimelineAttachments NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(int matrixTimelineAttachmentCount READ matrixTimelineAttachmentCount NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(QString matrixTimelineReplyEventId READ matrixTimelineReplyEventId NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(QString matrixTimelineReplySenderDisplayName READ
                 matrixTimelineReplySenderDisplayName NOTIFY matrixTimelineStateChanged)
    Q_PROPERTY(QString matrixTimelineReplySenderId READ matrixTimelineReplySenderId NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(QString matrixTimelineReplyBody READ matrixTimelineReplyBody NOTIFY
                 matrixTimelineStateChanged)
    // NOTIFY tied to matrixThreadTimelineChanged (the same signal that
    // also drives matrixThreadTimelineModel / matrixThreadTimelineLoading)
    // so that QML's `threadViewActive` and `threadTimelineModel` re-bind
    // from a single notification instead of two back-to-back emits, which
    // would otherwise rebuild the ListView delegate set twice per tab
    // switch.
    Q_PROPERTY(QString matrixTimelineThreadEventId READ matrixTimelineThreadEventId NOTIFY
                 matrixThreadTimelineChanged)
    Q_PROPERTY(QString matrixTimelineEditEventId READ matrixTimelineEditEventId NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(QStringList matrixTimelinePinnedEventIds READ matrixTimelinePinnedEventIds NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(QStringList matrixTimelineFrequentReactions READ matrixTimelineFrequentReactions
                 NOTIFY matrixTimelineStateChanged)
    Q_PROPERTY(bool matrixTimelineCanRedactOwn READ matrixTimelineCanRedactOwn NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(bool matrixTimelineCanRedactOther READ matrixTimelineCanRedactOther NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(QString matrixTimelinePendingJumpEventId READ matrixTimelinePendingJumpEventId NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(QStringList matrixTimelineTypingUsers READ matrixTimelineTypingUsers NOTIFY
                 matrixTimelineTypingUsersChanged)
    Q_PROPERTY(QAbstractItemModel *matrixThreadTimelineModel READ matrixThreadTimelineModel NOTIFY
                 matrixThreadTimelineChanged)
    Q_PROPERTY(bool matrixThreadTimelineLoading READ matrixThreadTimelineLoading NOTIFY
                 matrixThreadTimelineChanged)

    // Homeserver `m.room_versions` capability — exposed for the upgrade-room
    // dialog (version dropdown + default) and the `/upgraderoom` slash
    // command's default. Populated asynchronously after initial sync;
    // matrix-sdk caches the underlying /capabilities response in the state
    // store, so subsequent refreshes are local.
    Q_PROPERTY(bool roomVersionsCapabilityLoaded READ roomVersionsCapabilityLoaded NOTIFY
                 roomVersionsCapabilityChanged)
    Q_PROPERTY(
      QString defaultRoomVersion READ defaultRoomVersion NOTIFY roomVersionsCapabilityChanged)
    Q_PROPERTY(
      QStringList stableRoomVersions READ stableRoomVersions NOTIFY roomVersionsCapabilityChanged)

public:
    TimelineViewManager(CallManager *callManager, ChatPage *parent = nullptr);

    static TimelineViewManager *create(QQmlEngine *qmlEngine, QJSEngine *);

    static TimelineViewManager *instance() { return TimelineViewManager::instance_; }

    QVector<QString> getIgnoredUsers();

    VerificationManager *verificationManager() { return verificationManager_; }

    void clearAll();

    Q_INVOKABLE bool waitingForFirstSync() const { return waitingForFirstSync_; }
    QString activeMatrixTimelineRoomId() const { return activeMatrixTimelineRoomId_; }
    bool isConnected() const { return isConnected_; }
    int colorRevision() const { return colorRevision_; }
    QAbstractItemModel *matrixTimelineModel() const;
    Q_INVOKABLE QAbstractItemModel *ensureModelForRoom(const QString &roomId);
    Q_INVOKABLE void releaseModelForRoom(const QString &roomId);
    Q_INVOKABLE void trimProcessMemory();
    int matrixTimelineItemCount() const;
    bool matrixTimelineLoading() const { return matrixTimelineLoading_; }
    bool matrixTimelineAttachmentSending() const { return matrixAttachmentUploadInFlight_; }
    QVariantList matrixTimelineAttachments() const;
    int matrixTimelineAttachmentCount() const;
    QString matrixTimelineReplyEventId() const { return matrixTimelineReplyEventId_; }
    QString matrixTimelineReplySenderDisplayName() const
    {
        return matrixTimelineReplySenderDisplayName_;
    }
    QString matrixTimelineReplySenderId() const { return matrixTimelineReplySenderId_; }
    QString matrixTimelineReplyBody() const { return matrixTimelineReplyBody_; }
    QString matrixTimelineThreadEventId() const { return matrixTimelineThreadEventId_; }
    QString matrixTimelineEditEventId() const { return matrixTimelineEditEventId_; }
    QStringList matrixTimelinePinnedEventIds() const { return matrixTimelinePinnedEventIds_; }
    QStringList matrixTimelineFrequentReactions() const { return matrixTimelineFrequentReactions_; }
    bool matrixTimelineCanRedactOwn() const { return matrixTimelineCanRedactOwn_; }
    bool matrixTimelineCanRedactOther() const { return matrixTimelineCanRedactOther_; }
    QString matrixTimelinePendingJumpEventId() const { return matrixTimelinePendingJumpEventId_; }
    QStringList matrixTimelineTypingUsers() const { return matrixTimelineTypingUsers_; }
    QAbstractItemModel *matrixThreadTimelineModel() const;
    bool matrixThreadTimelineLoading() const;
    Q_INVOKABLE void
    paginateActiveMatrixThreadTimelineBackwards(int limit                     = 50,
                                                const QString &expectedRoomId = QString());
    Q_INVOKABLE void openMediaOverlay(QObject *room,
                                      const QString &mxcUrl,
                                      const QString &eventId,
                                      double originalWidth,
                                      double proportionalHeight);
    Q_INVOKABLE void openMediaOverlayWithContext(QObject *room,
                                                 const QString &mxcUrl,
                                                 const QString &eventId,
                                                 double originalWidth,
                                                 double proportionalHeight,
                                                 QObject *timeline,
                                                 QObject *timelineView);
    Q_INVOKABLE void openMediaOverlay(QObject *room,
                                      const QString &mxcUrl,
                                      const QString &eventId,
                                      double originalWidth,
                                      double proportionalHeight,
                                      int mediaType,
                                      int duration,
                                      const QString &thumbnailUrl);
    Q_INVOKABLE void openMediaOverlayWithContext(QObject *room,
                                                 const QString &mxcUrl,
                                                 const QString &eventId,
                                                 double originalWidth,
                                                 double proportionalHeight,
                                                 int mediaType,
                                                 int duration,
                                                 const QString &thumbnailUrl,
                                                 QObject *timeline,
                                                 QObject *timelineView);
    Q_INVOKABLE void openImagePackSettings(QString roomid);
    Q_INVOKABLE void openMedia(QString mxcUrl);
    Q_INVOKABLE void saveMedia(QString mxcUrl);
    Q_INVOKABLE void copyImage(const QString &mxcUrl) const;
    Q_INVOKABLE void copyMatrixEventLink(const QString &roomId, const QString &eventId) const;
    Q_INVOKABLE QColor userColor(QString id, QColor background);
    Q_INVOKABLE QVariantMap userBubblePalette(QString id, QColor background);
    Q_INVOKABLE QColor roomUserColor(QString roomId,
                                     QString userId,
                                     QColor background,
                                     int colorCodingPolicy = -1);
    Q_INVOKABLE QColor previewRoomUserColor(QString roomId,
                                            QString userId,
                                            QColor background,
                                            int roomMemberCount,
                                            int colorCodingPolicy = -1);
    Q_INVOKABLE QVariantMap roomUserBubblePalette(QString roomId,
                                                  QString userId,
                                                  QColor background,
                                                  int colorCodingPolicy = -1);
    Q_INVOKABLE QVariantMap previewRoomUserBubblePalette(QString roomId,
                                                         QString userId,
                                                         QColor background,
                                                         int roomMemberCount,
                                                         int colorCodingPolicy = -1);
    Q_INVOKABLE QString escapeEmoji(QString str) const;
    Q_INVOKABLE int emojiOnlyCodepointCount(const QString &body) const;
    Q_INVOKABLE QString htmlEscape(QString str) const { return str.toHtmlEscaped(); }
    Q_INVOKABLE QString formatMatrixMessageHtml(const QString &body) const;
    Q_INVOKABLE QVariantMap inspectActiveMatrixSlashCommand(const QString &text) const;
    Q_INVOKABLE QString activeMatrixCommandCompletionSearchString(const QString &text,
                                                                  int cursorPosition) const;
    Q_INVOKABLE QString activeMatrixApplyCommandCompletion(const QString &text,
                                                           int cursorPosition,
                                                           const QString &completion) const;
    Q_INVOKABLE int activeMatrixCommandCompletionCursorPosition(const QString &text,
                                                                int cursorPosition,
                                                                const QString &completion) const;
    Q_INVOKABLE bool
    activeMatrixCommandExpectsUserIdAt(const QString &text, int cursorPosition) const;
    Q_INVOKABLE bool
    executeActiveMatrixSlashCommand(const QString &text, const QStringList &mentions = {});

    Q_INVOKABLE void openRoomSettings(QString room_id);
    Q_INVOKABLE void
    openRoomInfo(const QString &roomId, const QString &initialTab = QStringLiteral("settings"));
    Q_INVOKABLE void openInviteUsers(QString roomId);
    Q_INVOKABLE void openGlobalUserProfile(QString userId);
    Q_INVOKABLE void openRoomUserProfile(QString roomId, QString userId);
    Q_INVOKABLE UserProfile *getGlobalUserProfile(QString userId);

    Q_INVOKABLE void ignoreUser(const QString &userId);

    Q_INVOKABLE void focusMessageInput();
    Q_INVOKABLE void requestEscape();
    Q_INVOKABLE void markRoomSwitchPhase(const QString &roomId, const QString &phase);
    Q_INVOKABLE bool roomSwitchPerfEnabled() const { return roomSwitchPerfEnabled_; }
    Q_INVOKABLE bool perfUiFlagEnabled(const QString &flag) const;
    Q_INVOKABLE bool
    sendActiveMatrixTextMessage(const QString &body, const QStringList &mentions = {});
    Q_INVOKABLE bool queueActiveMatrixReply(const QString &eventId,
                                            const QString &senderId,
                                            const QString &senderDisplayName,
                                            const QString &body);
    Q_INVOKABLE void clearActiveMatrixReply();
    Q_INVOKABLE bool queueActiveMatrixThread(const QString &threadEventId);
    Q_INVOKABLE void clearActiveMatrixThread();
    Q_INVOKABLE bool
    toggleActiveMatrixTimelineReaction(const QString &eventId, const QString &reactionKey);
    Q_INVOKABLE bool
    redactActiveMatrixTimelineEvent(const QString &eventId, const QString &reason = QString());
    Q_INVOKABLE bool redactActiveMatrixTimelineEvents(const QStringList &eventIds,
                                                      const QString &reason = QString());
    Q_INVOKABLE bool redactActiveMatrixTimelineEventsByUser(const QString &userId,
                                                            const QString &reason = QString());
    Q_INVOKABLE bool cancelActiveMatrixTimelineLocalEcho(const QString &transactionId);
    Q_INVOKABLE bool retryActiveMatrixTimelineLocalEcho(const QString &transactionId);
    Q_INVOKABLE bool markActiveMatrixTimelineEventAsRead(const QString &eventId);
    Q_INVOKABLE bool
    reportActiveMatrixTimelineEvent(const QString &eventId, const QString &reason = QString());
    Q_INVOKABLE bool
    forwardActiveMatrixTimelineEvent(const QString &eventId, const QString &targetRoomId);
    Q_INVOKABLE bool
    forwardActiveMatrixTimelineEvents(const QStringList &eventIds, const QString &targetRoomId);
    Q_INVOKABLE bool pinActiveMatrixTimelineEvent(const QString &eventId);
    Q_INVOKABLE bool unpinActiveMatrixTimelineEvent(const QString &eventId);
    Q_INVOKABLE void fetchActiveMatrixRoomThreadRoots(const QString &include,
                                                      const QString &from = {},
                                                      int limit           = 20);
    Q_INVOKABLE bool requestRawMessageDialogForActiveMatrixTimelineEvent(const QString &eventId);
    Q_INVOKABLE bool requestReadReceiptsModelForActiveMatrixTimelineEvent(const QString &eventId);
    Q_INVOKABLE bool openActiveMatrixAttachmentSelection();
    Q_INVOKABLE bool tryPasteClipboardAttachment(bool strict);
    bool stageMatrixAttachmentsForRoom(const QString &roomId, const QStringList &filePaths);
    Q_INVOKABLE bool sendActiveMatrixAttachments();
    Q_INVOKABLE bool stageVoiceRecording(const QString &filePath);
    Q_INVOKABLE void setActiveAttachmentDurationMs(uint64_t durationMs);
    Q_INVOKABLE void setActiveAttachmentVoiceWaveform(const QList<float> &waveform);
    Q_INVOKABLE bool stageAndSendVoiceRecording(const QString &filePath, int durationMs);
    Q_INVOKABLE void clearActiveMatrixAttachments();
    Q_INVOKABLE void removeActiveMatrixAttachment(int index);
    Q_INVOKABLE bool
    queueActiveMatrixEdit(const QString &eventId, const QString &body, const QString &messageKind);
    Q_INVOKABLE void clearActiveMatrixEdit();
    Q_INVOKABLE bool
    sendActiveMatrixEditMessage(const QString &body, const QStringList &mentions = {});
    Q_INVOKABLE void sendActiveMatrixTypingNotice(bool typing);
    Q_INVOKABLE bool paginateActiveMatrixTimelineBackwards(int pageSize = 0);
    Q_INVOKABLE void setPreferredInitialMatrixTimelinePageSize(int pageSize);
    Q_INVOKABLE bool
    openActiveMatrixTimelineMedia(const QString &itemId, const QString &suggestedFileName = {});
    Q_INVOKABLE bool
    saveActiveMatrixTimelineMedia(const QString &itemId, const QString &suggestedFileName = {});
    Q_INVOKABLE bool copyActiveMatrixTimelineMedia(const QString &itemId);
    Q_INVOKABLE bool resolveActiveMatrixPendingJump();
    Q_INVOKABLE void clearActiveMatrixPendingJump(const QString &eventId = QString());

    Q_INVOKABLE void fixImageRendering(QQuickTextDocument *t, QQuickItem *i);

    Q_INVOKABLE void navigateBack();
    Q_INVOKABLE void navigateForward();

    bool roomVersionsCapabilityLoaded() const { return roomVersionsCapabilityLoaded_; }
    QString defaultRoomVersion() const { return defaultRoomVersion_; }
    QStringList stableRoomVersions() const { return stableRoomVersions_; }
    Q_INVOKABLE void refreshRoomVersionsCapability();

    /// Sends `POST /_matrix/client/v3/rooms/{room_id}/upgrade` (via the
    /// matrix-sdk runtime worker).  On success, switches the active room to
    /// the returned replacement room id; on failure, surfaces the error via
    /// the snackbar.  `additionalCreators` is only honored by the server
    /// from room version 12 onwards.
    Q_INVOKABLE void performRoomUpgrade(const QString &roomId,
                                        const QString &newVersion,
                                        const QStringList &additionalCreators);

signals:
    void activeTimelineChanged(QObject *timeline);
    void waitingForFirstSyncChanged(bool waitingForFirstSync);
    void isConnectedChanged(bool state);
    void replyingEventChanged(QString replyingEvent);
    void replyClosed();
    void activeMatrixTimelineRawMessageDialogReady(QString eventId, QVariantMap payload);
    void activeMatrixTimelineReadReceiptsReady(QString eventId, QObject *readReceipts);
    void inviteUsers(QString roomId, QStringList users);
    void roomMembersChanged(QString roomId);
    void roomMemberPowerLevelChanged(QString roomId, QString userId, int powerLevel);
    void showRoomList();
    void narrowViewChanged();
    void focusInput();
    void escapeRequested();
    void openRoomInfoDialog(RoomSettings *settings,
                            MemberList *members,
                            QObject *room,
                            const QString &initialTab);
    void openInviteUsersDialog(InviteesModel *invitees);
    void openProfile(UserProfile *profile);
    void showImagePackSettings(ImagePackListModel *packlist, bool canCreateRoomPack);
    void openLeaveRoomDialog(QString roomid, QString reason = "");
    void openUpgradeRoomDialog(QString roomid, QString currentVersion);
    /// Fires when the user commits a room upgrade (submit on the Upgrade
    /// dialog or `/upgraderoom`), before the matrix-sdk request runs.
    /// Listened to by the Room Info dialog so it can close itself once the
    /// underlying room is on its way to being tombstoned.
    void roomUpgradeStarted(QString roomId);
    void openInviteResponseDialog(QString roomid);
    void showMediaOverlay(QObject *room,
                          QString eventId,
                          QString url,
                          double originalWidth,
                          double proportionalHeight,
                          int mediaType,
                          int duration,
                          QString thumbnailUrl,
                          QObject *timeline,
                          QObject *timelineView);
    void ignoredUsersChanged(const QVector<QString> &ignoredUsers);
    void colorRevisionChanged();
    void matrixTimelineStateChanged();
    void matrixTimelineTypingUsersChanged();
    void matrixRoomThreadRootsReady(QVariantList items, QString nextBatchToken);
    void matrixThreadTimelineChanged();
    void roomVersionsCapabilityChanged();

public slots:
    void updateReadReceipts(const QString &room_id, const std::vector<QString> &event_ids);
    void initializeRoomlist();
    void handleMatrixBackendInitialSyncReady(std::uint64_t handleId);
    void handleMatrixBackendSyncConnectionStateChanged(std::uint64_t handleId, bool isConnected);
    void handleMatrixBackendIgnoredUsersUpdated(std::uint64_t handleId,
                                                const QVector<QString> &ignoredUsers);
    void handleMatrixBackendRoomListSnapshotUpdated(std::uint64_t handleId);
    void handleMatrixBackendNotificationReceived(std::uint64_t handleId,
                                                 const QString &roomId,
                                                 const QString &eventId);
    void
    handleMatrixBackendRoomTimelineSnapshotUpdated(std::uint64_t handleId, const QString &roomId);
    void handleMatrixBackendRoomTimelinePaginationStateChanged(std::uint64_t handleId,
                                                               const QString &roomId,
                                                               bool inProgress);
    void handleMatrixBackendRoomPinnedEventsChanged(std::uint64_t handleId,
                                                    const QString &roomId,
                                                    const QStringList &eventIds);
    void handleMatrixBackendThreadTimelineSnapshotUpdated(std::uint64_t handleId,
                                                          const QString &roomId,
                                                          const QString &threadRootId);
    void
    handleMatrixBackendSyncStopped(std::uint64_t handleId, const QString &reason, bool isAuthError);
    void handleMatrixBackendTypingUsersUpdated(std::uint64_t handleId,
                                               const QString &roomId,
                                               const QStringList &displayNames);

    void showEvent(const QString &room_id, const QString &event_id);

    void updateColorPalette();
    void queueReply(const QString &roomid, const QString &repliedToEvent, const QString &replyBody);

    void setVideoCallItem();

    QAbstractItemModel *completerFor(const QString &completerName,
                                     const QString &roomId = QLatin1String(QLatin1String("")));
    RoomlistModel *rooms() { return rooms_; }
    void primeCurrentMatrixTimelineSelection();
    void suppressAutoTimelineSelection(bool suppress)
    {
        matrixTimelineAutoSelectionSuppressed_ = suppress;
    }
    void markRoomSwitchRequested(const QString &roomId, const QString &reason);
    void markRoomSwitchPhaseCpp(const QString &roomId, const QString &phase);

private:
    static void saveMxcMediaToFile(const QString &mxcUrl, const QString &filename);
    void scheduleMatrixSidebarRefresh();
    // Emit isConnectedChanged only when the value actually changes, with a log line
    // so connectivity transitions are observable. Every source funnels through here.
    void updateConnectedState();
    bool setIgnoredUsers(QVector<QString> ignoredUsers);
    void queueMatrixRoomReadMarker(uint64_t handleId,
                                   const QString &roomId,
                                   const QString &eventId,
                                   bool publicReceipt);
    void dispatchPendingMatrixReadMarker(const QString &roomId);
    void clearMatrixReadMarkerQueue();
    void queueActiveMatrixPendingJump(const QString &roomId, const QString &eventId);

    bool waitingForFirstSync_  = true;
    bool isConnected_          = true; // app/sync-level connectivity
    bool lastConnectedEmitted_ = true; // last value emitted via isConnectedChanged
    QVector<QString> ignoredUsers_;

    bool roomVersionsCapabilityLoaded_   = false;
    bool roomVersionsCapabilityInFlight_ = false;
    QString defaultRoomVersion_;
    QStringList stableRoomVersions_;

    RoomlistModel *rooms_          = nullptr;
    FilteredRoomlistModel *frooms_ = nullptr;
    CommunitiesModel *communities_ = nullptr;

    // don't move this above the rooms_
    VerificationManager *verificationManager_ = nullptr;
    PresenceEmitter *presenceEmitter          = nullptr;

    QHash<std::pair<QString, quint64>, QColor> userColors;

    // Per-room color cache: (roomId, userId) -> QColor
    // Invalidated when theme changes or room membership changes.
    QHash<std::pair<QString, QString>, QColor> roomUserColors_;
    // Per-room slot assignment cache: (roomId, userId) -> slot index into the
    // theme's `userColors.others` list.
    QHash<std::pair<QString, QString>, int> roomUserColorSlots_;
    // Cached sorted member lists per room (excluding self) for palette slot assignment.
    QHash<QString, std::vector<std::string>> roomMemberCache_;
    int colorRevision_ = 0;

    // 16 maximally-spaced hues for small-room palette assignment.

    bool roomSwitchPerfEnabled_     = false;
    quint64 roomSwitchPerfSwitchId_ = 0;
    QString roomSwitchPerfActiveRoomId_;
    QElapsedTimer roomSwitchPerfTimer_;

    inline static TimelineViewManager *instance_ = nullptr;

    NavigationHistory navHistory_;
    bool navigating_                                 = false;
    komai::MatrixTimelineModel *matrixTimelineModel_ = nullptr;
    QHash<QString, komai::MatrixTimelineModel *> perRoomModels_;
    QString activeMatrixTimelineRoomId_;
    bool matrixTimelineLoading_                 = false;
    bool matrixSidebarRefreshQueued_            = false;
    bool matrixRoomListRefreshQueued_           = false;
    bool matrixRoomListRefreshPending_          = false;
    bool matrixTimelineSelectionUpdateQueued_   = false;
    bool matrixTimelineAutoSelectionSuppressed_ = false;
    bool matrixTimelineRefreshQueued_           = false;
    QSet<QString> matrixTimelineRefreshPendingRoomIds_;
    quint64 matrixTimelineRefreshRequestId_         = 0;
    quint64 matrixTimelineRefreshInFlightRequestId_ = 0;
    bool matrixTimelineRoomStateRefreshPending_     = false;
    QString matrixTimelineRoomStateRefreshPendingRoomId_;
    quint64 matrixTimelineRoomStateRequestId_         = 0;
    quint64 matrixTimelineRoomStateInFlightRequestId_ = 0;
    quint64 matrixTimelineWarmupGuardGeneration_      = 0;
    bool matrixTimelineWarmupGuardActive_             = false;
    QString matrixTimelineRefreshInFlightRoomId_;
    QString matrixTimelineRoomStateInFlightRoomId_;
    QHash<QString, uint64_t> matrixReadMarkerPendingHandlesByRoom_;
    QHash<QString, QString> matrixReadMarkerPendingEventIdsByRoom_;
    QHash<QString, bool> matrixReadMarkerPendingPublicByRoom_;
    QHash<QString, QString> matrixReadMarkerInFlightEventIdsByRoom_;
    int preferredInitialMatrixTimelinePageSize_  = 0;
    bool matrixTimelineInitialPrefetchAttempted_ = false;
    QStringList matrixTimelinePinnedEventIds_;
    QStringList matrixTimelineFrequentReactions_;
    bool matrixTimelineCanRedactOwn_   = false;
    bool matrixTimelineCanRedactOther_ = false;
    QString matrixTimelinePendingJumpRoomId_;
    QString matrixTimelinePendingJumpEventId_;
    int matrixTimelinePendingJumpPaginationAttempts_ = 0;
    bool matrixTimelinePendingJumpAwaitingSnapshot_  = false;
    bool matrixTimelinePendingJumpExhaustedLogged_   = false;
    struct PendingMatrixAttachment
    {
        uint64_t handleId = 0;
        QString roomId;
        QString filePath;
        QString filename;
        QString body;
        QString replyEventId;
        QString threadId;
        QString mimeType;
        uint64_t durationMs = 0;
        bool isVoice        = false;
        QList<float> waveform;
    };
    std::deque<PendingMatrixAttachment> pendingMatrixAttachments_;
    QList<MatrixPendingAttachmentUpload *> matrixPendingAttachmentItems_;
    bool matrixAttachmentUploadInFlight_ = false;
    QString matrixTimelineReplyEventId_;
    QString matrixTimelineReplySenderDisplayName_;
    QString matrixTimelineReplySenderId_;
    QString matrixTimelineReplyBody_;
    QString matrixTimelineThreadEventId_;
    // One thread timeline model per (roomId, threadEventId) the user has
    // opened in this session. Switching back to a previously-viewed
    // thread rebinds QML to the cached model and shows its last snapshot
    // instantly; the re-subscribe then refreshes it in the background.
    // Eviction happens on backend disconnect via clearCurrentMatrixTimeline.
    struct ThreadTimelineEntry
    {
        komai::MatrixTimelineModel *model = nullptr;
        bool loading                      = false;
    };
    QHash<QPair<QString, QString>, ThreadTimelineEntry> matrixThreadTimelineEntries_;
    QString matrixTimelineEditEventId_;
    QString matrixTimelineEditMessageKind_;

    struct PerRoomInteractionState
    {
        QString replyEventId;
        QString replySenderDisplayName;
        QString replySenderId;
        QString replyBody;
        QString threadEventId;
        QString editEventId;
        QString editMessageKind;
        std::vector<PendingMatrixAttachment> attachments;

        bool isEmpty() const
        {
            return replyEventId.isEmpty() && threadEventId.isEmpty() && editEventId.isEmpty() &&
                   attachments.empty();
        }
    };
    QHash<QString, PerRoomInteractionState> perRoomInteractionState_;
    QStringList matrixTimelineTypingUsers_;
    struct MatrixTimelineFrequentReactionsCacheEntry
    {
        QStringList reactions;
        qint64 timestampMs = 0;
    };
    QHash<QString, MatrixTimelineFrequentReactionsCacheEntry> matrixTimelineFrequentReactionsCache_;

    void logRoomSwitchPhase(const QString &roomId, const QString &phase, const QString &source);
    void scheduleCurrentMatrixTimelineSelectionUpdate();
    void scheduleCurrentMatrixTimelineRefresh();
    void updateCurrentMatrixTimelineSelection();
    void refreshActiveMatrixTimelineRoomStateAsync();
    bool applyActiveMatrixTimelineRoomState(QStringList frequentReactions,
                                            bool canRedactOwn,
                                            bool canRedactOther);
    void invalidateMatrixTimelineFrequentReactionsCache(const QString &roomId);
    void refreshCurrentMatrixTimeline(const QString &roomId);
    void clearCurrentMatrixTimeline(bool stopBackendTask = true);
    void startNextPendingMatrixAttachment();
    void finishPendingMatrixAttachment(bool ok,
                                       const PendingMatrixAttachment &attachment,
                                       QString error = {});
    bool setActiveMatrixReplyState(const QString &eventId,
                                   const QString &senderId,
                                   const QString &senderDisplayName,
                                   const QString &body);
    bool clearActiveMatrixReplyState();
    bool setActiveMatrixThreadState(const QString &threadEventId);
    bool clearActiveMatrixThreadState();
    ThreadTimelineEntry *
    ensureThreadTimelineEntry(const QString &roomId, const QString &threadEventId);
    ThreadTimelineEntry *activeThreadTimelineEntry();
    const ThreadTimelineEntry *activeThreadTimelineEntry() const;
    void destroyAllThreadTimelineEntries();
    bool setActiveMatrixEditState(const QString &eventId, const QString &messageKind);
    bool clearActiveMatrixEditState();
    void fetchActiveMatrixTimelineMediaToFile(const QString &itemId,
                                              const QString &outputPath,
                                              const QString &userVisibleName,
                                              bool openAfterSave);

    QString normalizedMatrixMessageKind(const QString &messageKind) const;
};

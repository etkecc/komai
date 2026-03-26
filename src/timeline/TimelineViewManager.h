// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QPalette>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

#include "NavigationHistory.h"
#include "matrix/MatrixSyncUpdate.h"

class QQuickItem;
class QQuickTextDocument;

class UserSettings;
class ChatPage;
class ImagePackListModel;
class TimelineModel;
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

namespace mtx::events::voip {
struct CallInvite;
struct CallCandidates;
struct CallAnswer;
struct CallHangUp;
struct CallSelectAnswer;
struct CallReject;
struct CallNegotiate;
}
namespace mtx::events::collections {
struct TimelineEvents;
}

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
    Q_PROPERTY(QString matrixTimelineReplyBody READ matrixTimelineReplyBody NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(QString matrixTimelineEditEventId READ matrixTimelineEditEventId NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(QStringList matrixTimelinePinnedEventIds READ matrixTimelinePinnedEventIds NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(bool matrixTimelineCanRedactOwn READ matrixTimelineCanRedactOwn NOTIFY
                 matrixTimelineStateChanged)
    Q_PROPERTY(bool matrixTimelineCanRedactOther READ matrixTimelineCanRedactOther NOTIFY
                 matrixTimelineStateChanged)

public:
    TimelineViewManager(CallManager *callManager, ChatPage *parent = nullptr);

    static TimelineViewManager *create(QQmlEngine *qmlEngine, QJSEngine *);

    static TimelineViewManager *instance() { return TimelineViewManager::instance_; }

    QVector<QString> getIgnoredUsers();

    void sync(const komai::SyncUpdate &sync);

    VerificationManager *verificationManager() { return verificationManager_; }

    void clearAll();

    Q_INVOKABLE bool waitingForFirstSync() const { return waitingForFirstSync_; }
    bool isConnected() const { return isConnected_; }
    int colorRevision() const { return colorRevision_; }
    QAbstractItemModel *matrixTimelineModel() const;
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
    QString matrixTimelineReplyBody() const { return matrixTimelineReplyBody_; }
    QString matrixTimelineEditEventId() const { return matrixTimelineEditEventId_; }
    QStringList matrixTimelinePinnedEventIds() const { return matrixTimelinePinnedEventIds_; }
    bool matrixTimelineCanRedactOwn() const { return matrixTimelineCanRedactOwn_; }
    bool matrixTimelineCanRedactOther() const { return matrixTimelineCanRedactOther_; }
    Q_INVOKABLE void openMediaOverlay(TimelineModel *room,
                                      const QString &mxcUrl,
                                      const QString &eventId,
                                      double originalWidth,
                                      double proportionalHeight);
    Q_INVOKABLE void openMediaOverlayWithContext(TimelineModel *room,
                                                 const QString &mxcUrl,
                                                 const QString &eventId,
                                                 double originalWidth,
                                                 double proportionalHeight,
                                                 QObject *timeline,
                                                 QObject *timelineView);
    Q_INVOKABLE void openMediaOverlay(TimelineModel *room,
                                      const QString &mxcUrl,
                                      const QString &eventId,
                                      double originalWidth,
                                      double proportionalHeight,
                                      int mediaType,
                                      int duration,
                                      const QString &thumbnailUrl);
    Q_INVOKABLE void openMediaOverlayWithContext(TimelineModel *room,
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
    Q_INVOKABLE QString htmlEscape(QString str) const { return str.toHtmlEscaped(); }
    Q_INVOKABLE QString formatMatrixMessageHtml(const QString &body) const;

    Q_INVOKABLE void openRoomMembers(TimelineModel *room);
    Q_INVOKABLE void openRoomSettings(QString room_id);
    Q_INVOKABLE void
    openRoomInfo(const QString &roomId, const QString &initialTab = QStringLiteral("settings"));
    Q_INVOKABLE void openInviteUsers(QString roomId);
    Q_INVOKABLE void openGlobalUserProfile(QString userId);
    Q_INVOKABLE UserProfile *getGlobalUserProfile(QString userId);

    Q_INVOKABLE void focusMessageInput();
    Q_INVOKABLE void markRoomSwitchPhase(const QString &roomId, const QString &phase);
    Q_INVOKABLE bool roomSwitchPerfEnabled() const { return roomSwitchPerfEnabled_; }
    Q_INVOKABLE bool perfUiFlagEnabled(const QString &flag) const;
    Q_INVOKABLE bool sendActiveMatrixTextMessage(const QString &body);
    Q_INVOKABLE bool queueActiveMatrixReply(const QString &eventId,
                                            const QString &senderDisplayName,
                                            const QString &body);
    Q_INVOKABLE void clearActiveMatrixReply();
    Q_INVOKABLE bool
    toggleActiveMatrixTimelineReaction(const QString &eventId, const QString &reactionKey);
    Q_INVOKABLE bool
    redactActiveMatrixTimelineEvent(const QString &eventId, const QString &reason = QString());
    Q_INVOKABLE bool markActiveMatrixTimelineEventAsRead(const QString &eventId);
    Q_INVOKABLE bool reportActiveMatrixTimelineEvent(const QString &eventId,
                                                     const QString &reason = QString(),
                                                     int score             = -50);
    Q_INVOKABLE bool
    forwardActiveMatrixTimelineEvent(const QString &eventId, const QString &targetRoomId);
    Q_INVOKABLE bool pinActiveMatrixTimelineEvent(const QString &eventId);
    Q_INVOKABLE bool unpinActiveMatrixTimelineEvent(const QString &eventId);
    Q_INVOKABLE QVariantMap
    rawMessageDialogForActiveMatrixTimelineEvent(const QString &eventId) const;
    Q_INVOKABLE QObject *
    readReceiptsModelForActiveMatrixTimelineEvent(const QString &eventId) const;
    Q_INVOKABLE bool openActiveMatrixAttachmentSelection();
    Q_INVOKABLE bool sendActiveMatrixAttachments();
    Q_INVOKABLE void clearActiveMatrixAttachments();
    Q_INVOKABLE void removeActiveMatrixAttachment(int index);
    Q_INVOKABLE bool
    queueActiveMatrixEdit(const QString &eventId, const QString &body, const QString &messageKind);
    Q_INVOKABLE void clearActiveMatrixEdit();
    Q_INVOKABLE bool sendActiveMatrixEditMessage(const QString &body);
    Q_INVOKABLE bool paginateActiveMatrixTimelineBackwards(int pageSize = 0);
    Q_INVOKABLE bool
    openActiveMatrixTimelineMedia(const QString &itemId, const QString &suggestedFileName = {});
    Q_INVOKABLE bool
    saveActiveMatrixTimelineMedia(const QString &itemId, const QString &suggestedFileName = {});

    Q_INVOKABLE void fixImageRendering(QQuickTextDocument *t, QQuickItem *i);

    Q_INVOKABLE void navigateBack();
    Q_INVOKABLE void navigateForward();

signals:
    void activeTimelineChanged(TimelineModel *timeline);
    void waitingForFirstSyncChanged(bool waitingForFirstSync);
    void isConnectedChanged(bool state);
    void replyingEventChanged(QString replyingEvent);
    void replyClosed();
    void inviteUsers(QString roomId, QStringList users);
    void showRoomList();
    void narrowViewChanged();
    void focusInput();
    void openRoomInfoDialog(RoomSettings *settings,
                            MemberList *members,
                            TimelineModel *room,
                            const QString &initialTab);
    void openInviteUsersDialog(InviteesModel *invitees);
    void openProfile(UserProfile *profile);
    void showImagePackSettings(TimelineModel *room, ImagePackListModel *packlist);
    void openLeaveRoomDialog(QString roomid, QString reason = "");
    void showMediaOverlay(TimelineModel *room,
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

public slots:
    void updateReadReceipts(const QString &room_id, const std::vector<QString> &event_ids);
    void receivedSessionKey(const std::string &room_id, const std::string &session_id);
    void clearDecryptionErrors();
    void initializeRoomlist();
    void handleMatrixBackendInitialSyncReady(std::uint64_t handleId);
    void handleMatrixBackendRoomListSnapshotUpdated(std::uint64_t handleId);
    void
    handleMatrixBackendRoomTimelineSnapshotUpdated(std::uint64_t handleId, const QString &roomId);

    void showEvent(const QString &room_id, const QString &event_id);

    void updateColorPalette();
    void queueReply(const QString &roomid, const QString &repliedToEvent, const QString &replyBody);
    void queueCallMessage(const QString &roomid, const mtx::events::voip::CallInvite &);
    void queueCallMessage(const QString &roomid, const mtx::events::voip::CallCandidates &);
    void queueCallMessage(const QString &roomid, const mtx::events::voip::CallAnswer &);
    void queueCallMessage(const QString &roomid, const mtx::events::voip::CallHangUp &);
    void queueCallMessage(const QString &roomid, const mtx::events::voip::CallSelectAnswer &);
    void queueCallMessage(const QString &roomid, const mtx::events::voip::CallReject &);
    void queueCallMessage(const QString &roomid, const mtx::events::voip::CallNegotiate &);

    void setVideoCallItem();

    QAbstractItemModel *completerFor(const QString &completerName,
                                     const QString &roomId = QLatin1String(QLatin1String("")));
    void forwardMessageToRoom(mtx::events::collections::TimelineEvents const *e, QString roomId);

    RoomlistModel *rooms() { return rooms_; }
    void markRoomSwitchRequested(const QString &roomId, const QString &reason);
    void markRoomSwitchPhaseCpp(const QString &roomId, const QString &phase);

private:
    bool waitingForFirstSync_ = true;
    bool isConnected_         = true;

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
    QString activeMatrixTimelineRoomId_;
    bool matrixTimelineLoading_ = false;
    QStringList matrixTimelinePinnedEventIds_;
    bool matrixTimelineCanRedactOwn_   = false;
    bool matrixTimelineCanRedactOther_ = false;
    struct PendingMatrixAttachment
    {
        uint64_t handleId = 0;
        QString roomId;
        QString filePath;
        QString filename;
        QString body;
        QString replyEventId;
        QString mimeType;
    };
    std::deque<PendingMatrixAttachment> pendingMatrixAttachments_;
    QList<MatrixPendingAttachmentUpload *> matrixPendingAttachmentItems_;
    bool matrixAttachmentUploadInFlight_ = false;
    QString matrixTimelineReplyEventId_;
    QString matrixTimelineReplySenderDisplayName_;
    QString matrixTimelineReplyBody_;
    QString matrixTimelineEditEventId_;
    QString matrixTimelineEditMessageKind_;

    void processIgnoredUsers(const std::optional<QVector<QString>> &ignoredUsers);
    void logRoomSwitchPhase(const QString &roomId, const QString &phase, const QString &source);
    void updateCurrentMatrixTimelineSelection();
    bool refreshActiveMatrixTimelinePinnedEventIds();
    bool refreshActiveMatrixTimelineRedactionPermissions();
    void refreshCurrentMatrixTimeline();
    void clearCurrentMatrixTimeline(bool stopBackendTask = true);
    void startNextPendingMatrixAttachment();
    void finishPendingMatrixAttachment(bool ok,
                                       const PendingMatrixAttachment &attachment,
                                       QString error = {});
    bool setActiveMatrixReplyState(const QString &eventId,
                                   const QString &senderDisplayName,
                                   const QString &body);
    bool clearActiveMatrixReplyState();
    bool setActiveMatrixEditState(const QString &eventId, const QString &messageKind);
    bool clearActiveMatrixEditState();
    void fetchActiveMatrixTimelineMediaToFile(const QString &itemId,
                                              const QString &outputPath,
                                              const QString &userVisibleName,
                                              bool openAfterSave);

    QString normalizedMatrixMessageKind(const QString &messageKind) const;
};

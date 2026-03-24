// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QPalette>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>
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
    QTimer *matrixTimelineRefreshTimer_              = nullptr;
    QString activeMatrixTimelineRoomId_;
    bool matrixTimelineLoading_ = false;

    void processIgnoredUsers(const std::optional<QVector<QString>> &ignoredUsers);
    void logRoomSwitchPhase(const QString &roomId, const QString &phase, const QString &source);
    void updateCurrentMatrixTimelineSelection();
    void refreshCurrentMatrixTimeline();
    void clearCurrentMatrixTimeline(bool stopBackendTask = true);
};

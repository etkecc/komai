// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QMap>
#include <QProcessEnvironment>
#include <QQmlEngine>
#include <QSharedPointer>

#include <optional>
#include <string>

#include "settings/core/SettingsStore.h"

namespace YAML {
class Node;
}

class QSortFilterProxyModel;

namespace settings {
class SettingsController;
}

/**
 * UserSettings is the runtime settings model exposed to QML.
 *
 * It owns the in-memory state and emits change notifications for UI components.
 * It does not handle policy/transport details (for example profile path
 * resolution, staged loads, or secure storage layout); those concerns are now
 * orchestrated through settings::SettingsController.
 */
class UserSettings final : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Settings)
    QML_SINGLETON

    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool messageHoverHighlight READ messageHoverHighlight WRITE setMessageHoverHighlight
                 NOTIFY messageHoverHighlightChanged)
    Q_PROPERTY(bool enlargeEmojiOnlyMessages READ enlargeEmojiOnlyMessages WRITE
                 setEnlargeEmojiOnlyMessages NOTIFY enlargeEmojiOnlyMessagesChanged)
    Q_PROPERTY(bool tray READ tray WRITE setTray NOTIFY trayChanged)
    Q_PROPERTY(bool startInTray READ startInTray WRITE setStartInTray NOTIFY startInTrayChanged)
    Q_PROPERTY(bool showCommunitiesSidebar READ showCommunitiesSidebar WRITE
                 setShowCommunitiesSidebar NOTIFY showCommunitiesSidebarChanged)
    Q_PROPERTY(bool scrollbarsInRoomlist READ scrollbarsInRoomlist WRITE setScrollbarsInRoomlist
                 NOTIFY scrollbarsInRoomlistChanged)
    Q_PROPERTY(bool markdown READ markdown WRITE setMarkdown NOTIFY markdownChanged)
    Q_PROPERTY(SendMessageKey sendMessageKey READ sendMessageKey WRITE setSendMessageKey NOTIFY
                 sendMessageKeyChanged)
    Q_PROPERTY(AutoReplaceEmoji autoReplaceEmoji READ autoReplaceEmoji WRITE setAutoReplaceEmoji
                 NOTIFY autoReplaceEmojiChanged)
    Q_PROPERTY(bool bubbles READ bubbles WRITE setBubbles NOTIFY bubblesChanged)
    Q_PROPERTY(bool smallAvatars READ smallAvatars WRITE setSmallAvatars NOTIFY smallAvatarsChanged)
    Q_PROPERTY(
      bool enableStickers READ enableStickers WRITE setEnableStickers NOTIFY enableStickersChanged)
    Q_PROPERTY(bool showOwnAvatarInBubbleLayout READ showOwnAvatarInBubbleLayout WRITE
                 setShowOwnAvatarInBubbleLayout NOTIFY showOwnAvatarInBubbleLayoutChanged)
    Q_PROPERTY(QString pinnedReactions READ pinnedReactions WRITE setPinnedReactions NOTIFY
                 pinnedReactionsChanged)
    Q_PROPERTY(ShowSenderUsername showSenderUsername READ showSenderUsername WRITE
                 setShowSenderUsername NOTIFY showSenderUsernameChanged)
    Q_PROPERTY(
      int showSenderUsernameLargeRoomThreshold READ showSenderUsernameLargeRoomThreshold CONSTANT)
    Q_PROPERTY(bool animateImagesOnHover READ animateImagesOnHover WRITE setAnimateImagesOnHover
                 NOTIFY animateImagesOnHoverChanged)
    Q_PROPERTY(bool typingNotifications READ typingNotifications WRITE setTypingNotifications NOTIFY
                 typingNotificationsChanged)
    Q_PROPERTY(RoomSortOrder roomSortOrder READ roomSortOrder WRITE setRoomSortOrder NOTIFY
                 roomSortOrderChanged)
    Q_PROPERTY(bool showActionButtons READ showActionButtons WRITE setShowActionButtons NOTIFY
                 showActionButtonsChanged)
    Q_PROPERTY(bool readReceipts READ readReceipts WRITE setReadReceipts NOTIFY readReceiptsChanged)
    Q_PROPERTY(bool desktopNotifications READ hasDesktopNotifications WRITE setDesktopNotifications
                 NOTIFY desktopNotificationsChanged)
    Q_PROPERTY(bool alertOnIncomingMessages READ alertOnIncomingMessages WRITE
                 setAlertOnIncomingMessages NOTIFY alertOnIncomingMessagesChanged)
    Q_PROPERTY(bool useCircularAvatars READ useCircularAvatars WRITE setUseCircularAvatars NOTIFY
                 useCircularAvatarsChanged)
    Q_PROPERTY(bool decryptNotifications READ decryptNotifications WRITE setDecryptNotifications
                 NOTIFY decryptNotificationsChanged)
    Q_PROPERTY(bool showCommunityNotificationCounts READ showCommunityNotificationCounts WRITE
                 setShowCommunityNotificationCounts NOTIFY showCommunityNotificationCountsChanged)
    Q_PROPERTY(bool compactRoomList READ compactRoomList WRITE setCompactRoomList NOTIFY
                 compactRoomListChanged)
    Q_PROPERTY(bool showRoomListTime READ showRoomListTime WRITE setShowRoomListTime NOTIFY
                 showRoomListTimeChanged)
    Q_PROPERTY(LastMessagePreview showLastMessagePreview READ showLastMessagePreview WRITE
                 setShowLastMessagePreview NOTIFY showLastMessagePreviewChanged)
    Q_PROPERTY(bool fancyEffects READ fancyEffects WRITE setFancyEffects NOTIFY fancyEffectsChanged)
    Q_PROPERTY(
      bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(
      bool privacyScreen READ privacyScreen WRITE setPrivacyScreen NOTIFY privacyScreenChanged)
    Q_PROPERTY(int privacyScreenTimeoutSeconds READ privacyScreenTimeoutSeconds WRITE
                 setPrivacyScreenTimeoutSeconds NOTIFY privacyScreenTimeoutSecondsChanged)
    Q_PROPERTY(int maxTimelineWidth READ maxTimelineWidth WRITE setMaxTimelineWidth NOTIFY
                 maxTimelineWidthChanged)
    Q_PROPERTY(
      int roomListWidth READ roomListWidth WRITE setRoomListWidth NOTIFY roomListWidthChanged)
    Q_PROPERTY(int communityListWidth READ communityListWidth WRITE setCommunityListWidth NOTIFY
                 communityListWidthChanged)
    Q_PROPERTY(bool mobileMode READ mobileMode WRITE setMobileMode NOTIFY mobileModeChanged)
    Q_PROPERTY(bool enableSwipeGestures READ enableSwipeGestures WRITE setEnableSwipeGestures NOTIFY
                 enableSwipeGesturesChanged)
    Q_PROPERTY(double scaleFactor READ scaleFactor WRITE setScaleFactor NOTIFY scaleFactorChanged)
    Q_PROPERTY(double fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(QString font READ font WRITE setFontFamily NOTIFY fontChanged)
    Q_PROPERTY(QString emojiFont READ emojiFont WRITE setEmojiFontFamily NOTIFY emojiFontChanged)
    Q_PROPERTY(Presence presence READ presence WRITE setPresence NOTIFY presenceChanged)
    Q_PROPERTY(ShowImage showImage READ showImage WRITE setShowImage NOTIFY showImageChanged)
    Q_PROPERTY(QString ringtone READ ringtone WRITE setRingtone NOTIFY ringtoneChanged)
    Q_PROPERTY(QString microphone READ microphone WRITE setMicrophone NOTIFY microphoneChanged)
    Q_PROPERTY(QString camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(QString cameraResolution READ cameraResolution WRITE setCameraResolution NOTIFY
                 cameraResolutionChanged)
    Q_PROPERTY(QString cameraFrameRate READ cameraFrameRate WRITE setCameraFrameRate NOTIFY
                 cameraFrameRateChanged)
    Q_PROPERTY(int screenShareFrameRate READ screenShareFrameRate WRITE setScreenShareFrameRate
                 NOTIFY screenShareFrameRateChanged)
    Q_PROPERTY(
      bool screenSharePiP READ screenSharePiP WRITE setScreenSharePiP NOTIFY screenSharePiPChanged)
    Q_PROPERTY(bool screenShareRemoteVideo READ screenShareRemoteVideo WRITE
                 setScreenShareRemoteVideo NOTIFY screenShareRemoteVideoChanged)
    Q_PROPERTY(bool screenShareHideCursor READ screenShareHideCursor WRITE setScreenShareHideCursor
                 NOTIFY screenShareHideCursorChanged)
    Q_PROPERTY(bool useFallbackCallRelayServer READ useFallbackCallRelayServer WRITE
                 setUseFallbackCallRelayServer NOTIFY useFallbackCallRelayServerChanged)
    Q_PROPERTY(bool enableLegacyCalls READ enableLegacyCalls WRITE setEnableLegacyCalls NOTIFY
                 enableLegacyCallsChanged)
    Q_PROPERTY(bool onlyShareKeysWithVerifiedUsers READ onlyShareKeysWithVerifiedUsers WRITE
                 setOnlyShareKeysWithVerifiedUsers NOTIFY onlyShareKeysWithVerifiedUsersChanged)
    Q_PROPERTY(bool shareKeysWithTrustedUsers READ shareKeysWithTrustedUsers WRITE
                 setShareKeysWithTrustedUsers NOTIFY shareKeysWithTrustedUsersChanged)
    Q_PROPERTY(bool useOnlineKeyBackup READ useOnlineKeyBackup WRITE setUseOnlineKeyBackup NOTIFY
                 useOnlineKeyBackupChanged)
    Q_PROPERTY(QString profile READ profile WRITE setProfile NOTIFY profileChanged)
    Q_PROPERTY(QString userId READ userId WRITE setUserId NOTIFY userIdChanged)
    Q_PROPERTY(QString accessToken READ accessToken WRITE setAccessToken NOTIFY accessTokenChanged)
    Q_PROPERTY(QString deviceId READ deviceId WRITE setDeviceId NOTIFY deviceIdChanged)
    Q_PROPERTY(QString homeserver READ homeserver WRITE setHomeserver NOTIFY homeserverChanged)
    Q_PROPERTY(bool disableCertificateValidation READ disableCertificateValidation WRITE
                 setDisableCertificateValidation NOTIFY disableCertificateValidationChanged)
    Q_PROPERTY(bool useIdenticon READ useIdenticon WRITE setUseIdenticon NOTIFY useIdenticonChanged)
    Q_PROPERTY(bool openImagesInExternalApp READ openImagesInExternalApp WRITE
                 setOpenImagesInExternalApp NOTIFY openImagesInExternalAppChanged)
    Q_PROPERTY(bool openVideosInExternalApp READ openVideosInExternalApp WRITE
                 setOpenVideosInExternalApp NOTIFY openVideosInExternalAppChanged)
    Q_PROPERTY(QString integrationsLinksBrowserCommand READ integrationsLinksBrowserCommand WRITE
                 setIntegrationsLinksBrowserCommand NOTIFY integrationsLinksBrowserCommandChanged)
    Q_PROPERTY(int integrationsDbusApiAccess READ integrationsDbusApiAccess WRITE
                 setIntegrationsDbusApiAccess NOTIFY integrationsDbusApiAccessChanged)

    Q_PROPERTY(QStringList hiddenPins READ hiddenPins WRITE setHiddenPins NOTIFY hiddenPinsChanged)
    Q_PROPERTY(QStringList recentReactions READ recentReactions WRITE setRecentReactions NOTIFY
                 recentReactionsChanged)
    Q_PROPERTY(QStringList hiddenWidgets READ hiddenWidgets WRITE setHiddenWidgets NOTIFY
                 hiddenWidgetsChanged)
    Q_PROPERTY(bool updateSpaceVias READ updateSpaceVias WRITE setUpdateSpaceVias NOTIFY
                 updateSpaceViasChanged)
    Q_PROPERTY(bool expireEvents READ expireEvents WRITE setExpireEvents NOTIFY expireEventsChanged)

    // Window geometry (not exposed to QML, used internally)
    Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowWidthChanged)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowHeightChanged)

    // Database settings (internal, auto-adjusted)
    Q_PROPERTY(qulonglong maxDbSize READ maxDbSize WRITE setMaxDbSize NOTIFY maxDbSizeChanged)
    Q_PROPERTY(uint maxDbs READ maxDbs WRITE setMaxDbs NOTIFY maxDbsChanged)

    // Secrets storage fallback
    Q_PROPERTY(bool runWithoutSecureSecretsService READ runWithoutSecureSecretsService WRITE
                 setRunWithoutSecureSecretsService NOTIFY runWithoutSecureSecretsServiceChanged)

    // Experimental features
    Q_PROPERTY(bool enableHttp3 READ enableHttp3 WRITE setEnableHttp3 NOTIFY enableHttp3Changed)

    UserSettings();

public:
    friend class settings::SettingsController;

    static QSharedPointer<UserSettings> instance();
    static void initialize(std::optional<QString> profile);
    static void initialize(std::optional<QString> profile, const YAML::Node &configRoot);
    static UserSettings *create(QQmlEngine *qmlEngine, QJSEngine *)
    {
        // The instance has to exist before it is used. We cannot replace it.
        Q_ASSERT(instance());

        // The engine has to have the same thread affinity as the singleton.
        Q_ASSERT(qmlEngine->thread() == instance()->thread());

        // There can only be one engine accessing the singleton.
        static QJSEngine *s_engine = nullptr;
        if (s_engine)
            Q_ASSERT(qmlEngine == s_engine);
        else
            s_engine = qmlEngine;

        QJSEngine::setObjectOwnership(instance().get(), QJSEngine::CppOwnership);
        return instance().get();
    }

    enum class Presence
    {
        AutomaticPresence,
        Online,
        Unavailable,
        Offline,
    };
    Q_ENUM(Presence)

    enum class ShowImage
    {
        Always,
        OnlyPrivate,
        Never,
    };
    Q_ENUM(ShowImage)

    enum class ShowSenderUsername
    {
        Always,
        OnlyInLargeRooms,
        Never,
    };
    Q_ENUM(ShowSenderUsername)

    enum class AutoReplaceEmoji
    {
        Always,
        OnlyAtEnd,
        Never,
    };
    Q_ENUM(AutoReplaceEmoji)

    enum class SendMessageKey
    {
        Enter,
        ShiftEnter,
        CtrlEnter,
    };
    Q_ENUM(SendMessageKey)

    enum class RoomSortOrder
    {
        UnreadFirst_Recent, // Unread first, then by recent activity
        UnreadFirst_Alpha,  // Unread first, then alphabetically
        Recent,             // By recent activity only
        Alphabetical,       // Alphabetically only
    };
    Q_ENUM(RoomSortOrder)

    enum class LastMessagePreview
    {
        Always,          // Always show message previews
        OnlyUnencrypted, // Only show in unencrypted rooms
        Never,           // Never show message previews
    };
    Q_ENUM(LastMessagePreview)

    struct SessionSnapshot
    {
        QString userId;
        QString accessToken;
        QString deviceId;
        QString homeserver;
    };

    void save();
    void load(std::optional<QString> profile);
    void load(std::optional<QString> profile, const YAML::Node &configRoot);
    void applyTheme();
    void setTheme(QString theme);
    void setMessageHoverHighlight(bool state);
    void setEnlargeEmojiOnlyMessages(bool state);
    void setTray(bool state);
    void setStartInTray(bool state);
    void setMobileMode(bool mode);
    void setEnableSwipeGestures(bool mode);
    void setScaleFactor(double factor);
    void setFontSize(double size);
    void setFontFamily(QString family);
    void setEmojiFontFamily(QString family);
    void setShowCommunitiesSidebar(bool state);
    void setScrollbarsInRoomlist(bool state);
    void setMarkdown(bool state);
    void setSendMessageKey(SendMessageKey key);
    void setAutoReplaceEmoji(AutoReplaceEmoji state);
    void setBubbles(bool state);
    void setSmallAvatars(bool state);
    void setEnableStickers(bool state);
    void setShowOwnAvatarInBubbleLayout(bool state);
    void setPinnedReactions(QString value);
    void setShowSenderUsername(ShowSenderUsername state);
    void setAnimateImagesOnHover(bool state);
    void setReadReceipts(bool state);
    void setTypingNotifications(bool state);
    void setRoomSortOrder(RoomSortOrder order);
    void setShowActionButtons(bool state);
    void setMaxTimelineWidth(int state);
    void setCommunityListWidth(int state);
    void setRoomListWidth(int state);
    void setDesktopNotifications(bool state);
    void setAlertOnIncomingMessages(bool state);
    void setUseCircularAvatars(bool state);
    void setDecryptNotifications(bool state);
    void setShowCommunityNotificationCounts(bool state);
    void setCompactRoomList(bool state);
    void setShowRoomListTime(bool state);
    void setShowLastMessagePreview(LastMessagePreview style);
    void setFancyEffects(bool state);
    void setReducedMotion(bool state);
    void setPrivacyScreen(bool state);
    void setPrivacyScreenTimeoutSeconds(int state);
    void setPresence(Presence state);
    void setShowImage(ShowImage state);
    void setRingtone(QString ringtone);
    void setMicrophone(QString microphone);
    void setCamera(QString camera);
    void setCameraResolution(QString resolution);
    void setCameraFrameRate(QString frameRate);
    void setScreenShareFrameRate(int frameRate);
    void setScreenSharePiP(bool state);
    void setScreenShareRemoteVideo(bool state);
    void setScreenShareHideCursor(bool state);
    void setUseFallbackCallRelayServer(bool state);
    void setEnableLegacyCalls(bool state);
    void setOnlyShareKeysWithVerifiedUsers(bool state);
    void setShareKeysWithTrustedUsers(bool state);
    void setUseOnlineKeyBackup(bool state);
    void setUseOnlineKeyBackupFromConfig(bool state);
    void setProfile(QString profile);
    void setUserId(QString userId);
    void setAccessToken(QString accessToken);
    void setDeviceId(QString deviceId);
    void setCurrentTagId(QString currentTagId);
    void setHomeserver(QString homeserver);
    void setDisableCertificateValidation(bool disabled);
    void setHiddenTags(const QStringList &hiddenTags);
    void setMutedTags(const QStringList &mutedTags);
    void setHiddenPins(const QStringList &hiddenTags);
    void setHiddenWidgets(const QStringList &hiddenTags);
    void setRecentReactions(QStringList recent);
    void setUseIdenticon(bool state);
    void setOpenImagesInExternalApp(bool state);
    void setOpenVideosInExternalApp(bool state);
    void setIntegrationsLinksBrowserCommand(QString command);
    void setCollapsedSpaces(QList<QStringList> spaces);
    void setIntegrationsDbusApiAccess(int access);
    void setUpdateSpaceVias(bool state);
    void setExpireEvents(bool state);
    void setWindowWidth(int width);
    void setWindowHeight(int height);
    void setMaxDbSize(qulonglong size);
    void setMaxDbs(uint count);
    void setRunWithoutSecureSecretsService(bool state);
    void setEnableHttp3(bool state);
    void clearAuth();
    bool hasPersistedSessionIdentity() const;
    bool hasActiveSession() const;
    SessionSnapshot sessionSnapshot() const;
    // Persist full auth/session material even if fields are unchanged in memory.
    // This keeps file/keychain storage repaired after partial deletion/corruption.
    bool persistSessionSnapshot(const SessionSnapshot &snapshot);
    // Load session identity fields from persisted storage without triggering settings save.
    void setSessionSnapshot(const SessionSnapshot &snapshot);

    // Secrets storage helpers (for fallback mode)
    QString secret(const QString &name) const;
    void setSecret(const QString &name, const QString &value);
    void removeSecret(const QString &name);
    void setPersistenceSuspended(bool suspended);

    // Theme helpers for QML (used on the Welcome page)
    Q_INVOKABLE int themeVariantIndex() const;
    Q_INVOKABLE void setThemeVariantByIndex(int index);
    Q_INVOKABLE QStringList themeNamesForCurrentVariant() const;
    Q_INVOKABLE int themeIndexInCurrentVariant() const;
    Q_INVOKABLE void setThemeByVariantIndex(int index);

    QString theme() const
    {
        if (const auto value =
              coreStore_.valueAs<std::string>(settings::core::SettingId::UiThemeSlug);
            value.has_value() && !value->empty())
            return QString::fromStdString(*value);
        return !theme_.isEmpty() ? theme_ : defaultTheme_;
    }
    bool messageHoverHighlight() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesHoverHighlight);
            value.has_value())
            return *value;
        return messageHoverHighlight_;
    }
    bool enlargeEmojiOnlyMessages() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesEmojiOnlyEnlarge);
            value.has_value())
            return *value;
        return enlargeEmojiOnlyMessages_;
    }
    bool tray() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::IntegrationsSystemTrayEnabled);
            value.has_value())
            return *value;
        return tray_;
    }
    bool startInTray() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::IntegrationsSystemTrayAutostart);
            value.has_value())
            return *value;
        return startInTray_;
    }
    bool showCommunitiesSidebar() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsCommunitiesVisible);
            value.has_value())
            return *value;
        return showCommunitiesSidebar_;
    }
    bool scrollbarsInRoomlist() const
    {
        if (const auto value = coreStore_.valueAs<bool>(
              settings::core::SettingId::SidebarsRoomListScrollbarsEnabled);
            value.has_value())
            return *value;
        return scrollbarsInRoomlist_;
    }
    bool useCircularAvatars() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::UiAvatarsCircular);
            value.has_value())
            return *value;
        return useCircularAvatars_;
    }
    bool decryptNotifications() const
    {
        if (const auto value = coreStore_.valueAs<bool>(
              settings::core::SettingId::NotificationsDesktopDecryptMessages);
            value.has_value())
            return *value;
        return decryptNotifications_;
    }
    bool showCommunityNotificationCounts() const
    {
        if (const auto value = coreStore_.valueAs<bool>(
              settings::core::SettingId::SidebarsRoomListShowCommunityCounts);
            value.has_value())
            return *value;
        return showCommunityNotificationCounts_;
    }
    bool compactRoomList() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsRoomListCompact);
            value.has_value())
            return *value;
        return compactRoomList_;
    }
    bool showRoomListTime() const
    {
        if (const auto value = coreStore_.valueAs<bool>(
              settings::core::SettingId::SidebarsRoomListShowLastMessageTime);
            value.has_value())
            return *value;
        return showRoomListTime_;
    }
    LastMessagePreview showLastMessagePreview() const
    {
        if (const auto value = coreStore_.valueAs<int>(
              settings::core::SettingId::SidebarsRoomListLastMessagePreview);
            value.has_value() && *value >= static_cast<int>(LastMessagePreview::Always) &&
            *value <= static_cast<int>(LastMessagePreview::Never))
            return static_cast<LastMessagePreview>(*value);
        return showLastMessagePreview_;
    }
    bool fancyEffects() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaEffectsEnabled);
            value.has_value())
            return *value;
        return fancyEffects_;
    }
    bool reducedMotion() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::UiMotionAnimationsEnabled);
            value.has_value())
            return !*value;
        return reducedMotion_;
    }
    bool privacyScreen() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::PrivacyScreenLockEnabled);
            value.has_value())
            return *value;
        return privacyScreen_;
    }
    int privacyScreenTimeoutSeconds() const
    {
        if (const auto value =
              coreStore_.valueAs<int>(settings::core::SettingId::PrivacyScreenLockTimeoutSeconds);
            value.has_value())
            return *value;
        return privacyScreenTimeoutSeconds_;
    }
    bool markdown() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::ComposerInputMarkdownEnabled);
            value.has_value())
            return *value;
        return markdown_;
    }
    SendMessageKey sendMessageKey() const
    {
        if (const auto value =
              coreStore_.valueAs<int>(settings::core::SettingId::ComposerInputSendKey);
            value.has_value() && *value >= static_cast<int>(SendMessageKey::Enter) &&
            *value <= static_cast<int>(SendMessageKey::CtrlEnter))
            return static_cast<SendMessageKey>(*value);
        return sendMessageKey_;
    }
    AutoReplaceEmoji autoReplaceEmoji() const
    {
        if (const auto value =
              coreStore_.valueAs<int>(settings::core::SettingId::ComposerInputAutoReplaceEmoji);
            value.has_value() && *value >= static_cast<int>(AutoReplaceEmoji::Always) &&
            *value <= static_cast<int>(AutoReplaceEmoji::Never))
            return static_cast<AutoReplaceEmoji>(*value);
        return autoReplaceEmoji_;
    }
    bool bubbles() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesLayoutBubbles);
            value.has_value())
            return *value;
        return bubbles_;
    }
    bool smallAvatars() const
    {
        if (const auto value = coreStore_.valueAs<bool>(
              settings::core::SettingId::TimelineMessagesLayoutSmallAvatars);
            value.has_value())
            return *value;
        return smallAvatars_;
    }
    bool enableStickers() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::ComposerExtrasStickersEnabled);
            value.has_value())
            return *value;
        return enableStickers_;
    }
    bool showOwnAvatarInBubbleLayout() const
    {
        if (const auto value = coreStore_.valueAs<bool>(
              settings::core::SettingId::TimelineMessagesLayoutShowOwnAvatar);
            value.has_value())
            return *value;
        return showOwnAvatarInBubbleLayout_;
    }
    QString pinnedReactions() const
    {
        if (const auto value = coreStore_.valueAs<std::string>(
              settings::core::SettingId::TimelineMessageActionsPinnedReactions);
            value.has_value())
            return QString::fromStdString(*value);
        return pinnedReactions_;
    }
    ShowSenderUsername showSenderUsername() const
    {
        if (const auto value =
              coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesSenderUsername);
            value.has_value() && *value >= static_cast<int>(ShowSenderUsername::Always) &&
            *value <= static_cast<int>(ShowSenderUsername::Never))
            return static_cast<ShowSenderUsername>(*value);
        return showSenderUsername_;
    }
    int showSenderUsernameLargeRoomThreshold() const { return 16; }
    bool animateImagesOnHover() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaAnimateOnHover);
            value.has_value())
            return *value;
        return animateImagesOnHover_;
    }
    bool typingNotifications() const
    {
        if (const auto value = coreStore_.valueAs<bool>(
              settings::core::SettingId::ComposerFeedbackTypingNotifications);
            value.has_value())
            return *value;
        return typingNotifications_;
    }
    RoomSortOrder roomSortOrder() const
    {
        if (const auto value =
              coreStore_.valueAs<int>(settings::core::SettingId::SidebarsRoomListSort);
            value.has_value() && *value >= static_cast<int>(RoomSortOrder::UnreadFirst_Recent) &&
            *value <= static_cast<int>(RoomSortOrder::Alphabetical))
            return static_cast<RoomSortOrder>(*value);
        return roomSortOrder_;
    }
    bool showActionButtons() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessageActionsEnabled);
            value.has_value())
            return *value;
        return showActionButtons_;
    }
    bool mobileMode() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::UiInputEnableTextSelection);
            value.has_value())
            return !*value;
        return mobileMode_;
    }
    bool enableSwipeGestures() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::UiInputSwipeGestures);
            value.has_value())
            return *value;
        return enableSwipeGestures_;
    }
    bool readReceipts() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::ComposerFeedbackReadReceipts);
            value.has_value())
            return *value;
        return readReceipts_;
    }
    bool hasDesktopNotifications() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::NotificationsDesktopEnabled);
            value.has_value())
            return *value;
        return hasDesktopNotifications_;
    }
    bool alertOnIncomingMessages() const
    {
        if (const auto value = coreStore_.valueAs<bool>(
              settings::core::SettingId::NotificationsDesktopAlertOnIncoming);
            value.has_value())
            return *value;
        return alertOnIncomingMessages_;
    }
    bool hasNotifications() const { return hasDesktopNotifications() || alertOnIncomingMessages(); }
    int maxTimelineWidth() const
    {
        if (const auto value =
              coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesMaxWidthPx);
            value.has_value())
            return *value;
        return maxTimelineWidth_;
    }
    int communityListWidth() const { return communityListWidth_; }
    int roomListWidth() const { return roomListWidth_; }
    double scaleFactor() const { return scaleFactor_ > 0.0 ? scaleFactor_ : 1.0; }
    double fontSize() const
    {
        if (const auto value = coreStore_.valueAs<double>(settings::core::SettingId::UiFontSizePt);
            value.has_value())
            return *value;
        return baseFontSize_;
    }
    QString font() const
    {
        if (const auto value =
              coreStore_.valueAs<std::string>(settings::core::SettingId::UiFontFamily);
            value.has_value())
            return QString::fromStdString(*value);
        return font_;
    }
    QString emojiFont() const;
    QString emojiFontFamily() const
    {
        if (const auto value =
              coreStore_.valueAs<std::string>(settings::core::SettingId::UiFontEmojiFamily);
            value.has_value())
            return QString::fromStdString(*value);
        return emojiFont_;
    }
    Presence presence() const
    {
        if (const auto value =
              coreStore_.valueAs<int>(settings::core::SettingId::NetworkPresenceStatusPolicy);
            value.has_value() && *value >= static_cast<int>(Presence::AutomaticPresence) &&
            *value <= static_cast<int>(Presence::Offline))
            return static_cast<Presence>(*value);
        return presence_;
    }
    ShowImage showImage() const
    {
        if (const auto value =
              coreStore_.valueAs<int>(settings::core::SettingId::TimelineMediaImageDisplay);
            value.has_value() && *value >= static_cast<int>(ShowImage::Always) &&
            *value <= static_cast<int>(ShowImage::Never))
            return static_cast<ShowImage>(*value);
        return showImage_;
    }
    QString ringtone() const
    {
        if (const auto value =
              coreStore_.valueAs<std::string>(settings::core::SettingId::CallsAudioRingtone);
            value.has_value())
            return QString::fromStdString(*value);
        return ringtone_;
    }
    QString microphone() const
    {
        if (const auto value =
              coreStore_.valueAs<std::string>(settings::core::SettingId::CallsDevicesMicrophone);
            value.has_value())
            return QString::fromStdString(*value);
        return microphone_;
    }
    QString camera() const
    {
        if (const auto value =
              coreStore_.valueAs<std::string>(settings::core::SettingId::CallsDevicesCamera);
            value.has_value())
            return QString::fromStdString(*value);
        return camera_;
    }
    QString cameraResolution() const
    {
        if (const auto value = coreStore_.valueAs<std::string>(
              settings::core::SettingId::CallsDevicesCameraResolution);
            value.has_value())
            return QString::fromStdString(*value);
        return cameraResolution_;
    }
    QString cameraFrameRate() const
    {
        if (const auto value = coreStore_.valueAs<std::string>(
              settings::core::SettingId::CallsDevicesCameraFrameRate);
            value.has_value())
            return QString::fromStdString(*value);
        return cameraFrameRate_;
    }
    int screenShareFrameRate() const { return screenShareFrameRate_; }
    bool screenSharePiP() const { return screenSharePiP_; }
    bool screenShareRemoteVideo() const { return screenShareRemoteVideo_; }
    bool screenShareHideCursor() const { return screenShareHideCursor_; }
    bool useFallbackCallRelayServer() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::CallsRelayUseFallbackServer);
            value.has_value())
            return *value;
        return useFallbackCallRelayServer_;
    }
    bool enableLegacyCalls() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::CallsLegacyEnabled);
            value.has_value())
            return *value;
        return enableLegacyCalls_;
    }
    bool shareKeysWithTrustedUsers() const
    {
        if (const auto value = coreStore_.valueAs<bool>(
              settings::core::SettingId::EncryptionKeySharingShareWithTrusted);
            value.has_value())
            return *value;
        return shareKeysWithTrustedUsers_;
    }
    bool onlyShareKeysWithVerifiedUsers() const
    {
        if (const auto value = coreStore_.valueAs<bool>(
              settings::core::SettingId::EncryptionKeySharingOnlyVerifiedUsers);
            value.has_value())
            return *value;
        return onlyShareKeysWithVerifiedUsers_;
    }
    bool useOnlineKeyBackup() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::EncryptionBackupOnlineEnabled);
            value.has_value())
            return *value;
        return useOnlineKeyBackup_;
    }
    QString profile() const { return profile_; }
    QString userId() const { return userId_; }
    QString accessToken() const { return accessToken_; }
    QString deviceId() const { return deviceId_; }
    QString currentTagId() const { return currentTagId_; }
    QString homeserver() const { return homeserver_; }
    bool disableCertificateValidation() const { return disableCertificateValidation_; }
    QStringList hiddenTags() const { return hiddenTags_; }
    QStringList mutedTags() const { return mutedTags_; }
    QStringList hiddenPins() const { return hiddenPins_; }
    QStringList hiddenWidgets() const { return hiddenWidgets_; }
    QStringList recentReactions() const { return recentReactions_; }
    bool useIdenticon() const;
    bool openImagesInExternalApp() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaOpenImagesExternal);
            value.has_value())
            return *value;
        return openImagesInExternalApp_;
    }
    bool openVideosInExternalApp() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaOpenVideosExternal);
            value.has_value())
            return *value;
        return openVideosInExternalApp_;
    }
    QList<QStringList> collapsedSpaces() const { return collapsedSpaces_; }
    int integrationsDbusApiAccess() const
    {
        if (const auto value =
              coreStore_.valueAs<int>(settings::core::SettingId::IntegrationsDbusApiAccess);
            value.has_value())
            return *value;
        return integrationsDbusApiAccess_;
    }
    QString integrationsLinksBrowserCommand() const { return integrationsLinksBrowserCommand_; }
    bool updateSpaceVias() const
    {
        if (const auto value = coreStore_.valueAs<bool>(
              settings::core::SettingId::PrivacyMaintenanceUpdateSpaceVias);
            value.has_value())
            return *value;
        return updateSpaceVias_;
    }
    bool expireEvents() const
    {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::PrivacyMaintenanceExpireEvents);
            value.has_value())
            return *value;
        return expireEvents_;
    }
    int windowWidth() const { return windowWidth_; }
    int windowHeight() const { return windowHeight_; }
    qulonglong maxDbSize() const { return maxDbSize_; }
    uint maxDbs() const { return maxDbs_; }
    bool runWithoutSecureSecretsService() const { return runWithoutSecureSecretsService_; }
    bool enableHttp3() const { return enableHttp3_; }
    settings::core::SettingsStore &mutableCoreStore() { return coreStore_; }
    const settings::core::SettingsStore &coreStore() const { return coreStore_; }

signals:
    void showCommunitiesSidebarChanged(bool state);
    void scrollbarsInRoomlistChanged(bool state);
    void roomSortOrderChanged(RoomSortOrder order);
    void themeChanged(QString state);
    void messageHoverHighlightChanged(bool state);
    void enlargeEmojiOnlyMessagesChanged(bool state);
    void trayChanged(bool state);
    void startInTrayChanged(bool state);
    void markdownChanged(bool state);
    void sendMessageKeyChanged(SendMessageKey key);
    void autoReplaceEmojiChanged(AutoReplaceEmoji state);
    void bubblesChanged(bool state);
    void smallAvatarsChanged(bool state);
    void enableStickersChanged(bool state);
    void showOwnAvatarInBubbleLayoutChanged(bool state);
    void pinnedReactionsChanged(const QString &value);
    void showSenderUsernameChanged(ShowSenderUsername state);
    void animateImagesOnHoverChanged(bool state);
    void typingNotificationsChanged(bool state);
    void showActionButtonsChanged(bool state);
    void readReceiptsChanged(bool state);
    void desktopNotificationsChanged(bool state);
    void alertOnIncomingMessagesChanged(bool state);
    void useCircularAvatarsChanged(bool state);
    void decryptNotificationsChanged(bool state);
    void showCommunityNotificationCountsChanged(bool state);
    void compactRoomListChanged(bool state);
    void showRoomListTimeChanged(bool state);
    void showLastMessagePreviewChanged(LastMessagePreview style);
    void fancyEffectsChanged(bool state);
    void reducedMotionChanged(bool state);
    void privacyScreenChanged(bool state);
    void privacyScreenTimeoutSecondsChanged(int state);
    void maxTimelineWidthChanged(int state);
    void roomListWidthChanged(int state);
    void communityListWidthChanged(int state);
    void mobileModeChanged(bool mode);
    void enableSwipeGesturesChanged(bool state);
    void scaleFactorChanged(double factor);
    void fontSizeChanged(double state);
    void fontChanged(QString state);
    void emojiFontChanged(QString state);
    void presenceChanged(Presence state);
    void showImageChanged(ShowImage state);
    void ringtoneChanged(QString ringtone);
    void microphoneChanged(QString microphone);
    void cameraChanged(QString camera);
    void cameraResolutionChanged(QString resolution);
    void cameraFrameRateChanged(QString frameRate);
    void screenShareFrameRateChanged(int frameRate);
    void screenSharePiPChanged(bool state);
    void screenShareRemoteVideoChanged(bool state);
    void screenShareHideCursorChanged(bool state);
    void useFallbackCallRelayServerChanged(bool state);
    void enableLegacyCallsChanged(bool state);
    void onlyShareKeysWithVerifiedUsersChanged(bool state);
    void shareKeysWithTrustedUsersChanged(bool state);
    void useOnlineKeyBackupChanged(bool state);
    void profileChanged(QString profile);
    void userIdChanged(QString userId);
    void accessTokenChanged(QString accessToken);
    void deviceIdChanged(QString deviceId);
    void homeserverChanged(QString homeserver);
    void disableCertificateValidationChanged(bool disabled);
    void useIdenticonChanged(bool state);
    void openImagesInExternalAppChanged(bool state);
    void openVideosInExternalAppChanged(bool state);
    void hiddenPinsChanged();
    void hiddenWidgetsChanged();
    void recentReactionsChanged();
    void integrationsDbusApiAccessChanged(int state);
    void integrationsLinksBrowserCommandChanged(QString command);
    void updateSpaceViasChanged(bool state);
    void expireEventsChanged(bool state);
    void windowWidthChanged(int width);
    void windowHeightChanged(int height);
    void maxDbSizeChanged(qulonglong size);
    void maxDbsChanged(uint count);
    void runWithoutSecureSecretsServiceChanged(bool state);
    void enableHttp3Changed(bool state);

private:
    template<typename T, typename Signal>
    void setSetting(T &member, const T &value, Signal signal);

    void loadConfigYaml(const YAML::Node &root);
    void loadSessionYaml(const YAML::Node &root);
    void loadStateYaml(const YAML::Node &root);

    void saveConfigYaml() const;
    void saveSessionYaml() const;
    void saveSecretsYaml() const;
    void saveStateYaml() const;

    // Default to system theme if QT_QPA_PLATFORMTHEME var is set.
    QString defaultTheme_ = QProcessEnvironment::systemEnvironment()
                                .value(QStringLiteral("QT_QPA_PLATFORMTHEME"), QLatin1String(""))
                                .isEmpty()
                              ? "komai-light"
                              : "komai-light";
    QString theme_;
    bool messageHoverHighlight_;
    bool enlargeEmojiOnlyMessages_;
    bool tray_;
    bool startInTray_;
    bool showCommunitiesSidebar_;
    bool scrollbarsInRoomlist_;
    bool markdown_;
    SendMessageKey sendMessageKey_;
    AutoReplaceEmoji autoReplaceEmoji_;
    bool bubbles_;
    bool smallAvatars_;
    bool enableStickers_;
    bool showOwnAvatarInBubbleLayout_;
    QString pinnedReactions_;
    ShowSenderUsername showSenderUsername_;
    bool animateImagesOnHover_;
    bool typingNotifications_;
    RoomSortOrder roomSortOrder_;
    bool showActionButtons_;
    bool readReceipts_;
    bool hasDesktopNotifications_;
    bool alertOnIncomingMessages_;
    bool useCircularAvatars_;
    bool decryptNotifications_;
    bool showCommunityNotificationCounts_;
    bool compactRoomList_;
    bool showRoomListTime_;
    LastMessagePreview showLastMessagePreview_;
    bool fancyEffects_;
    bool reducedMotion_;
    bool privacyScreen_;
    int privacyScreenTimeoutSeconds_;
    bool shareKeysWithTrustedUsers_;
    bool onlyShareKeysWithVerifiedUsers_;
    bool useOnlineKeyBackup_;
    bool mobileMode_;
    bool enableSwipeGestures_;
    int maxTimelineWidth_;
    int roomListWidth_      = -1;
    int communityListWidth_ = 200;
    double scaleFactor_     = -1.0;
    double baseFontSize_    = 13.0;
    QString font_;
    QString emojiFont_;
    Presence presence_;
    ShowImage showImage_;
    QString ringtone_;
    QString microphone_;
    QString camera_;
    QString cameraResolution_;
    QString cameraFrameRate_;
    int screenShareFrameRate_;
    bool screenSharePiP_;
    bool screenShareRemoteVideo_;
    bool screenShareHideCursor_;
    bool useFallbackCallRelayServer_;
    bool enableLegacyCalls_;
    bool disableCertificateValidation_ = false;
    QString profile_;
    QString userId_;
    QString accessToken_;
    QString deviceId_;
    QString currentTagId_;
    QString homeserver_;
    QStringList hiddenTags_;
    QStringList mutedTags_;
    QStringList hiddenPins_;
    QStringList hiddenWidgets_;
    QStringList recentReactions_;
    QList<QStringList> collapsedSpaces_;
    bool useIdenticon_;
    bool openImagesInExternalApp_;
    bool openVideosInExternalApp_;
    int integrationsDbusApiAccess_ = 0;
    QString integrationsLinksBrowserCommand_;
    bool updateSpaceVias_;
    bool expireEvents_;
    int windowWidth_                     = 0;
    int windowHeight_                    = 0;
    qulonglong maxDbSize_                = 0;
    uint maxDbs_                         = 0;
    bool runWithoutSecureSecretsService_ = false;
    bool enableHttp3_                    = false;
    QMap<QString, QString> secrets_;
    settings::core::SettingsStore coreStore_;
    bool suppressSettingsSave_ = false;

    enum class StartupPersistenceScope
    {
        ConfigOnly,
        Full,
    };

    void setPersistenceScopeReadyForAuth(bool ready);
    StartupPersistenceScope startupPersistenceScope_ = StartupPersistenceScope::ConfigOnly;

    // Paths to the per-profile settings directory and files.
    QString profileDirPath_;
    QString configFilePath_;
    QString stateFilePath_;
    QString sessionFilePath_;
    QString secretsFilePath_;

    static QSharedPointer<UserSettings> instance_;
};

class UserSettingsModel : public QAbstractListModel
{
    /**
     * UserSettingsModel adapts runtime setting metadata for QML presentation.
     *
     * It renders sections + setting rows as a list model, provides role-based values
     * for delegates, and forwards edits back to the singleton `UserSettings`.
     */
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    enum SettingsTab
    {
        TabLookFeel,
        TabSidebars,
        TabTimeline,
        TabComposer,
        TabNotifications,
        TabCalls,
        TabPrivacy,
        TabEncryption,
        TabSession,
        TabIntegrations,
        TabAbout,
    };
    Q_ENUM(SettingsTab);

private:
public:
    enum Types
    {
        Toggle,
        ReadOnlyText,
        Options,
        OptionsWithDescription,
        PresenceStatusMessageField,
        Integer,
        Double,
        SectionTitle,
        SectionBar,
        KeyStatus,
        SessionKeyImportExport,
        XSignKeysRequestDownload,
        ConfigureHiddenEvents,
        ManageIgnoredUsers,
        Link,
        ThemeSelector,
        TextInput,
        LogoutButton,
        ProfileButton,
        AccessTokenField,
    };
    Q_ENUM(Types);

    enum Roles
    {
        Name,
        Description,
        Value,
        Type,
        ValueLowerBound,
        ValueUpperBound,
        ValueStep,
        Values,
        Good,
        Enabled,
        ThemeVariantValue,
        ThemeVariantValues,
        SettingImage,
        Tab,
    };

    UserSettingsModel(QObject *parent = nullptr);
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    Q_INVOKABLE QObject *modelForTab(int tab) const;
    Q_INVOKABLE void importSessionKeys();
    Q_INVOKABLE void exportSessionKeys();
    Q_INVOKABLE void requestCrossSigningSecrets();
    Q_INVOKABLE void downloadCrossSigningSecrets();

private:
    mutable QHash<int, QSortFilterProxyModel *> filteredModels_;
};

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
    Q_PROPERTY(bool groupView READ groupView WRITE setGroupView NOTIFY groupViewStateChanged)
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
    Q_PROPERTY(bool showOwnAvatarNextToOwnMessages READ showOwnAvatarNextToOwnMessages WRITE
                 setShowOwnAvatarNextToOwnMessages NOTIFY showOwnAvatarNextToOwnMessagesChanged)
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
    Q_PROPERTY(bool sortByImportance READ sortByImportance WRITE setSortByImportance NOTIFY
                 roomSortingChangedImportance)
    Q_PROPERTY(bool sortByAlphabet READ sortByAlphabet WRITE setSortByAlphabet NOTIFY
                 roomSortingChangedAlphabetical)
    Q_PROPERTY(bool buttonsInTimeline READ buttonsInTimeline WRITE setButtonsInTimeline NOTIFY
                 buttonInTimelineChanged)
    Q_PROPERTY(bool readReceipts READ readReceipts WRITE setReadReceipts NOTIFY readReceiptsChanged)
    Q_PROPERTY(bool desktopNotifications READ hasDesktopNotifications WRITE setDesktopNotifications
                 NOTIFY desktopNotificationsChanged)
    Q_PROPERTY(bool alertOnNotification READ hasAlertOnNotification WRITE setAlertOnNotification
                 NOTIFY alertOnNotificationChanged)
    Q_PROPERTY(
      bool avatarCircles READ avatarCircles WRITE setAvatarCircles NOTIFY avatarCirclesChanged)
    Q_PROPERTY(
      bool decryptSidebar READ decryptSidebar WRITE setDecryptSidebar NOTIFY decryptSidebarChanged)
    Q_PROPERTY(bool decryptNotifications READ decryptNotifications WRITE setDecryptNotifications
                 NOTIFY decryptNotificationsChanged)
    Q_PROPERTY(bool spaceNotifications READ spaceNotifications WRITE setSpaceNotifications NOTIFY
                 spaceNotificationsChanged)
    Q_PROPERTY(bool fancyEffects READ fancyEffects WRITE setFancyEffects NOTIFY fancyEffectsChanged)
    Q_PROPERTY(
      bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(
      bool privacyScreen READ privacyScreen WRITE setPrivacyScreen NOTIFY privacyScreenChanged)
    Q_PROPERTY(int privacyScreenTimeout READ privacyScreenTimeout WRITE setPrivacyScreenTimeout
                 NOTIFY privacyScreenTimeoutChanged)
    Q_PROPERTY(int timelineMaxWidth READ timelineMaxWidth WRITE setTimelineMaxWidth NOTIFY
                 timelineMaxWidthChanged)
    Q_PROPERTY(
      int roomListWidth READ roomListWidth WRITE setRoomListWidth NOTIFY roomListWidthChanged)
    Q_PROPERTY(int communityListWidth READ communityListWidth WRITE setCommunityListWidth NOTIFY
                 communityListWidthChanged)
    Q_PROPERTY(bool mobileMode READ mobileMode WRITE setMobileMode NOTIFY mobileModeChanged)
    Q_PROPERTY(bool disableSwipe READ disableSwipe WRITE setDisableSwipe NOTIFY disableSwipeChanged)
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
    Q_PROPERTY(
      bool useStunServer READ useStunServer WRITE setUseStunServer NOTIFY useStunServerChanged)
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
    Q_PROPERTY(bool openImageExternal READ openImageExternal WRITE setOpenImageExternal NOTIFY
                 openImageExternalChanged)
    Q_PROPERTY(bool openVideoExternal READ openVideoExternal WRITE setOpenVideoExternal NOTIFY
                 openVideoExternalChanged)

    Q_PROPERTY(QStringList hiddenPins READ hiddenPins WRITE setHiddenPins NOTIFY hiddenPinsChanged)
    Q_PROPERTY(QStringList recentReactions READ recentReactions WRITE setRecentReactions NOTIFY
                 recentReactionsChanged)
    Q_PROPERTY(QStringList hiddenWidgets READ hiddenWidgets WRITE setHiddenWidgets NOTIFY
                 hiddenWidgetsChanged)
    Q_PROPERTY(
      bool exposeDBusApi READ exposeDBusApi WRITE setExposeDBusApi NOTIFY exposeDBusApiChanged)
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
    static QSharedPointer<UserSettings> instance();
    static void initialize(std::optional<QString> profile);
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

    void save();
    void load(std::optional<QString> profile);
    void applyTheme();
    void setTheme(QString theme);
    void setMessageHoverHighlight(bool state);
    void setEnlargeEmojiOnlyMessages(bool state);
    void setTray(bool state);
    void setStartInTray(bool state);
    void setMobileMode(bool mode);
    void setDisableSwipe(bool mode);
    void setFontSize(double size);
    void setFontFamily(QString family);
    void setEmojiFontFamily(QString family);
    void setGroupView(bool state);
    void setScrollbarsInRoomlist(bool state);
    void setMarkdown(bool state);
    void setSendMessageKey(SendMessageKey key);
    void setAutoReplaceEmoji(AutoReplaceEmoji state);
    void setBubbles(bool state);
    void setSmallAvatars(bool state);
    void setEnableStickers(bool state);
    void setShowOwnAvatarNextToOwnMessages(bool state);
    void setPinnedReactions(const QString &value);
    void setShowSenderUsername(ShowSenderUsername state);
    void setAnimateImagesOnHover(bool state);
    void setReadReceipts(bool state);
    void setTypingNotifications(bool state);
    void setSortByImportance(bool state);
    void setSortByAlphabet(bool state);
    void setButtonsInTimeline(bool state);
    void setTimelineMaxWidth(int state);
    void setCommunityListWidth(int state);
    void setRoomListWidth(int state);
    void setDesktopNotifications(bool state);
    void setAlertOnNotification(bool state);
    void setAvatarCircles(bool state);
    void setDecryptSidebar(bool state);
    void setDecryptNotifications(bool state);
    void setSpaceNotifications(bool state);
    void setFancyEffects(bool state);
    void setReducedMotion(bool state);
    void setPrivacyScreen(bool state);
    void setPrivacyScreenTimeout(int state);
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
    void setUseStunServer(bool state);
    void setEnableLegacyCalls(bool state);
    void setOnlyShareKeysWithVerifiedUsers(bool state);
    void setShareKeysWithTrustedUsers(bool state);
    void setUseOnlineKeyBackup(bool state);
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
    void setOpenImageExternal(bool state);
    void setOpenVideoExternal(bool state);
    void setCollapsedSpaces(QList<QStringList> spaces);
    void setExposeDBusApi(bool state);
    void setUpdateSpaceVias(bool state);
    void setExpireEvents(bool state);
    void setWindowWidth(int width);
    void setWindowHeight(int height);
    void setMaxDbSize(qulonglong size);
    void setMaxDbs(uint count);
    void setRunWithoutSecureSecretsService(bool state);
    void setEnableHttp3(bool state);
    void clearAuth();

    // Secrets storage helpers (for fallback mode)
    QString secret(const QString &name) const;
    void setSecret(const QString &name, const QString &value);
    void removeSecret(const QString &name);

    // Theme helpers for QML (used on the Welcome page)
    Q_INVOKABLE int themeVariantIndex() const;
    Q_INVOKABLE void setThemeVariantByIndex(int index);
    Q_INVOKABLE QStringList themeNamesForCurrentVariant() const;
    Q_INVOKABLE int themeIndexInCurrentVariant() const;
    Q_INVOKABLE void setThemeByVariantIndex(int index);

    QString theme() const { return !theme_.isEmpty() ? theme_ : defaultTheme_; }
    bool messageHoverHighlight() const { return messageHoverHighlight_; }
    bool enlargeEmojiOnlyMessages() const { return enlargeEmojiOnlyMessages_; }
    bool tray() const { return tray_; }
    bool startInTray() const { return startInTray_; }
    bool groupView() const { return groupView_; }
    bool scrollbarsInRoomlist() const { return scrollbarsInRoomlist_; }
    bool avatarCircles() const { return avatarCircles_; }
    bool decryptSidebar() const { return decryptSidebar_; }
    bool decryptNotifications() const { return decryptNotifications_; }
    bool spaceNotifications() const { return spaceNotifications_; }
    bool fancyEffects() const { return fancyEffects_; }
    bool reducedMotion() const { return reducedMotion_; }
    bool privacyScreen() const { return privacyScreen_; }
    int privacyScreenTimeout() const { return privacyScreenTimeout_; }
    bool markdown() const { return markdown_; }
    SendMessageKey sendMessageKey() const { return sendMessageKey_; }
    AutoReplaceEmoji autoReplaceEmoji() const { return autoReplaceEmoji_; }
    bool bubbles() const { return bubbles_; }
    bool smallAvatars() const { return smallAvatars_; }
    bool enableStickers() const { return enableStickers_; }
    bool showOwnAvatarNextToOwnMessages() const { return showOwnAvatarNextToOwnMessages_; }
    QString pinnedReactions() const { return pinnedReactions_; }
    ShowSenderUsername showSenderUsername() const { return showSenderUsername_; }
    int showSenderUsernameLargeRoomThreshold() const { return 16; }
    bool animateImagesOnHover() const { return animateImagesOnHover_; }
    bool typingNotifications() const { return typingNotifications_; }
    bool sortByImportance() const { return sortByImportance_; }
    bool sortByAlphabet() const { return sortByAlphabet_; }
    bool buttonsInTimeline() const { return buttonsInTimeline_; }
    bool mobileMode() const { return mobileMode_; }
    bool disableSwipe() const { return disableSwipe_; }
    bool readReceipts() const { return readReceipts_; }
    bool hasDesktopNotifications() const { return hasDesktopNotifications_; }
    bool hasAlertOnNotification() const { return hasAlertOnNotification_; }
    bool hasNotifications() const { return hasDesktopNotifications() || hasAlertOnNotification(); }
    int timelineMaxWidth() const { return timelineMaxWidth_; }
    int communityListWidth() const { return communityListWidth_; }
    int roomListWidth() const { return roomListWidth_; }
    double fontSize() const { return baseFontSize_; }
    QString font() const { return font_; }
    QString emojiFont() const;
    QString emojiFontFamily() const { return emojiFont_; }
    Presence presence() const { return presence_; }
    ShowImage showImage() const { return showImage_; }
    QString ringtone() const { return ringtone_; }
    QString microphone() const { return microphone_; }
    QString camera() const { return camera_; }
    QString cameraResolution() const { return cameraResolution_; }
    QString cameraFrameRate() const { return cameraFrameRate_; }
    int screenShareFrameRate() const { return screenShareFrameRate_; }
    bool screenSharePiP() const { return screenSharePiP_; }
    bool screenShareRemoteVideo() const { return screenShareRemoteVideo_; }
    bool screenShareHideCursor() const { return screenShareHideCursor_; }
    bool useStunServer() const { return useStunServer_; }
    bool enableLegacyCalls() const { return enableLegacyCalls_; }
    bool shareKeysWithTrustedUsers() const { return shareKeysWithTrustedUsers_; }
    bool onlyShareKeysWithVerifiedUsers() const { return onlyShareKeysWithVerifiedUsers_; }
    bool useOnlineKeyBackup() const { return useOnlineKeyBackup_; }
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
    bool openImageExternal() const { return openImageExternal_; }
    bool openVideoExternal() const { return openVideoExternal_; }
    QList<QStringList> collapsedSpaces() const { return collapsedSpaces_; }
    bool exposeDBusApi() const { return exposeDBusApi_; }
    bool updateSpaceVias() const { return updateSpaceVias_; }
    bool expireEvents() const { return expireEvents_; }
    int windowWidth() const { return windowWidth_; }
    int windowHeight() const { return windowHeight_; }
    qulonglong maxDbSize() const { return maxDbSize_; }
    uint maxDbs() const { return maxDbs_; }
    bool runWithoutSecureSecretsService() const { return runWithoutSecureSecretsService_; }
    bool enableHttp3() const { return enableHttp3_; }

signals:
    void groupViewStateChanged(bool state);
    void scrollbarsInRoomlistChanged(bool state);
    void roomSortingChangedImportance(bool state);
    void roomSortingChangedAlphabetical(bool state);
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
    void showOwnAvatarNextToOwnMessagesChanged(bool state);
    void pinnedReactionsChanged(const QString &value);
    void showSenderUsernameChanged(ShowSenderUsername state);
    void animateImagesOnHoverChanged(bool state);
    void typingNotificationsChanged(bool state);
    void buttonInTimelineChanged(bool state);
    void readReceiptsChanged(bool state);
    void desktopNotificationsChanged(bool state);
    void alertOnNotificationChanged(bool state);
    void avatarCirclesChanged(bool state);
    void decryptSidebarChanged(bool state);
    void decryptNotificationsChanged(bool state);
    void spaceNotificationsChanged(bool state);
    void fancyEffectsChanged(bool state);
    void reducedMotionChanged(bool state);
    void privacyScreenChanged(bool state);
    void privacyScreenTimeoutChanged(int state);
    void timelineMaxWidthChanged(int state);
    void roomListWidthChanged(int state);
    void communityListWidthChanged(int state);
    void mobileModeChanged(bool mode);
    void disableSwipeChanged(bool state);
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
    void useStunServerChanged(bool state);
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
    void openImageExternalChanged(bool state);
    void openVideoExternalChanged(bool state);
    void hiddenPinsChanged();
    void hiddenWidgetsChanged();
    void recentReactionsChanged();
    void exposeDBusApiChanged(bool state);
    void updateSpaceViasChanged(bool state);
    void expireEventsChanged(bool state);
    void windowWidthChanged(int width);
    void windowHeightChanged(int height);
    void maxDbSizeChanged(qulonglong size);
    void maxDbsChanged(uint count);
    void runWithoutSecureSecretsServiceChanged(bool state);
    void enableHttp3Changed(bool state);

private:
    // Default to system theme if QT_QPA_PLATFORMTHEME var is set.
    QString defaultTheme_ = QProcessEnvironment::systemEnvironment()
                                .value(QStringLiteral("QT_QPA_PLATFORMTHEME"), QLatin1String(""))
                                .isEmpty()
                              ? "komai"
                              : "komai";
    QString theme_;
    bool messageHoverHighlight_;
    bool enlargeEmojiOnlyMessages_;
    bool tray_;
    bool startInTray_;
    bool groupView_;
    bool scrollbarsInRoomlist_;
    bool markdown_;
    SendMessageKey sendMessageKey_;
    AutoReplaceEmoji autoReplaceEmoji_;
    bool bubbles_;
    bool smallAvatars_;
    bool enableStickers_;
    bool showOwnAvatarNextToOwnMessages_;
    QString pinnedReactions_;
    ShowSenderUsername showSenderUsername_;
    bool animateImagesOnHover_;
    bool typingNotifications_;
    bool sortByImportance_;
    bool sortByAlphabet_;
    bool buttonsInTimeline_;
    bool readReceipts_;
    bool hasDesktopNotifications_;
    bool hasAlertOnNotification_;
    bool avatarCircles_;
    bool decryptSidebar_;
    bool decryptNotifications_;
    bool spaceNotifications_;
    bool fancyEffects_;
    bool reducedMotion_;
    bool privacyScreen_;
    int privacyScreenTimeout_;
    bool shareKeysWithTrustedUsers_;
    bool onlyShareKeysWithVerifiedUsers_;
    bool useOnlineKeyBackup_;
    bool mobileMode_;
    bool disableSwipe_;
    int timelineMaxWidth_;
    int roomListWidth_;
    int communityListWidth_;
    double baseFontSize_;
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
    bool useStunServer_;
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
    bool openImageExternal_;
    bool openVideoExternal_;
    bool exposeDBusApi_;
    bool updateSpaceVias_;
    bool expireEvents_;
    int windowWidth_                     = 0;
    int windowHeight_                    = 0;
    qulonglong maxDbSize_                = 0;
    uint maxDbs_                         = 0;
    bool runWithoutSecureSecretsService_ = false;
    bool enableHttp3_                    = false;
    QMap<QString, QString> secrets_;

    // Path to the per-profile YAML config file
    QString configFilePath_;

    static QSharedPointer<UserSettings> instance_;
};

class UserSettingsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    enum SettingsTab
    {
        TabLookFeel,
        TabTimeline,
        TabComposer,
        TabNotifications,
        TabCalls,
        TabPrivacy,
        TabEncryption,
        TabSession,
        TabAbout,
    };
    Q_ENUM(SettingsTab);

private:
    enum Indices
    {
        // == Look & Feel Tab ==
        // Theme section
        LookFeelThemeSection,
        Theme,
        // Fonts section
        LookFeelFontsSection,
        Font,
        FontSize,
        EmojiFont,
#ifndef Q_OS_MACOS
        ScaleFactor,
#endif
        // Effects section
        LookFeelEffectsSection,
        ReducedMotion,
        // Room List section
        LookFeelRoomListSection,
        AvatarCircles,
        UseIdenticon,
        ScrollbarsInRoomlist,
        GroupView,
        SortByImportance,
        SortByAlphabet,
        DecryptSidebar,
        SpaceNotifications,
        // System Tray section
        LookFeelTraySection,
        Tray,
        StartInTray,
#ifdef NHEKO_DBUS_SYS
        ExposeDBusApi,
#endif
        // Mobile section
        LookFeelMobileSection,
        MobileMode,
        DisableSwipe,

        // == Timeline Tab ==
        // Messages section
        TimelineMessagesSection,
        Bubbles,
        SmallAvatars,
        ShowOwnAvatarNextToOwnMessages,
        ShowSenderUsername,
        TimelineMaxWidth,
        EnlargeEmojiOnlyMessages,
        MessageHoverHighlight,
        ButtonsInTimeline,
        // Media section
        TimelineMediaSection,
        FancyEffects,
        AnimateImagesOnHover,
        ShowImage,
        OpenImageExternal,
        OpenVideoExternal,

        // == Composer Tab ==
        // Input section
        ComposerInputSection,
        Markdown,
        SendMessageKey,
        AutoReplaceEmoji,
        // Feedback section
        ComposerFeedbackSection,
        TypingNotifications,
        ReadReceipts,
        // Extras section
        ComposerExtrasSection,
        PinnedReactions,
        EnableStickers,

        // == Notifications Tab ==
        // Desktop section
        NotificationsDesktopSection,
        DesktopNotifications,
        AlertOnNotification,
        DecryptNotifications,

        // == Calls Tab ==
        // General section
        CallsGeneralSection,
        EnableLegacyCalls,
        UseStunServer,
        // Devices section
        CallsDevicesSection,
        Microphone,
        Camera,
        CameraResolution,
        CameraFrameRate,
        Ringtone,

        // == Privacy Tab ==
        // Screen Lock section
        PrivacyScreenLockSection,
        PrivacyScreen,
        PrivacyScreenTimeout,
        // Data & Maintenance section
        PrivacyDataSection,
        ExpireEvents,
        HiddenTimelineEvents,
        UpdateSpaceVias,
        // Users section
        PrivacyUsersSection,
        IgnoredUsers,

        // == Encryption Tab ==
        // Key Sharing section
        EncryptionKeySharingSection,
        OnlyShareKeysWithVerifiedUsers,
        ShareKeysWithTrustedUsers,
        // Backup section
        EncryptionBackupSection,
        UseOnlineKeyBackup,
        SessionKeys,
        // Cross-Signing section
        EncryptionCrossSigningSection,
        OnlineBackupKey,
        SelfSigningKey,
        UserSigningKey,
        MasterKey,
        CrossSigningSecrets,

        // == Session Tab ==
        // Account section
        SessionAccountSection,
        UserId,
        Homeserver,
        Profile,
        // Device section
        SessionDeviceSection,
        DeviceId,
        DeviceFingerprint,
        AccessToken,
        // Actions section
        SessionActionsSection,
        Logout,

        // == About Tab ==
        // Application section
        AboutApplicationSection,
        Version,
        Platform,
        BasedOn,
        ForkBy,

        COUNT,

    // hidden
#ifdef Q_OS_MACOS
        ScaleFactor,
#endif
#ifndef NHEKO_DBUS_SYS
        ExposeDBusApi,
#endif
    };

public:
    enum Types
    {
        Toggle,
        ReadOnlyText,
        Options,
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
    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        (void)parent;
        return (int)COUNT;
    }
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    Q_INVOKABLE void importSessionKeys();
    Q_INVOKABLE void exportSessionKeys();
    Q_INVOKABLE void requestCrossSigningSecrets();
    Q_INVOKABLE void downloadCrossSigningSecrets();
};

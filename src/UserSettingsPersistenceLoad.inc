void
UserSettings::loadConfigYaml(const YAML::Node &root)
{
    tray_        = readScalar<bool>(root, SettingKey::AppWindowTrayEnabled, false);
    startInTray_ = readScalar<bool>(root, SettingKey::AppStartupStartInTray, false);
    hasDesktopNotifications_ =
      readScalar<bool>(root, SettingKey::NotificationsDesktopEnabled, true);
    alertOnIncomingMessages_ =
      readScalar<bool>(root, SettingKey::NotificationsDesktopAlertOnIncoming, false);
    showCommunitiesSidebar_ = readScalar<bool>(root, SettingKey::SidebarsCommunitiesVisible, true);
    scrollbarsInRoomlist_ =
      readScalar<bool>(root, SettingKey::SidebarsRoomListScrollbarsEnabled, true);
    showActionButtons_ = readScalar<bool>(root, SettingKey::TimelineMessageActionsEnabled, true);
    maxTimelineWidth_  = readScalar<int>(root, SettingKey::TimelineMessagesMaxWidthPx, 0);
    messageHoverHighlight_ =
      readScalar<bool>(root, SettingKey::TimelineMessagesHoverHighlight, false);
    enlargeEmojiOnlyMessages_ =
      readScalar<bool>(root, SettingKey::TimelineMessagesEmojiOnlyEnlarge, true);
    markdown_       = readScalar<bool>(root, SettingKey::ComposerInputMarkdownEnabled, true);
    sendMessageKey_ = sendMessageKeyFromStorage(
      readString(root, SettingKey::ComposerInputSendKey, QStringLiteral("enter")),
      SendMessageKey::Enter);
    autoReplaceEmoji_ = autoReplaceEmojiFromStorage(
      readString(root, SettingKey::ComposerInputAutoReplaceEmoji, QStringLiteral("always")),
      AutoReplaceEmoji::Always);
    bubbles_        = readScalar<bool>(root, SettingKey::TimelineMessagesLayoutBubbles, true);
    smallAvatars_   = readScalar<bool>(root, SettingKey::TimelineMessagesLayoutSmallAvatars, false);
    enableStickers_ = readScalar<bool>(root, SettingKey::ComposerExtrasStickersEnabled, false);
    showOwnAvatarInBubbleLayout_ =
      readScalar<bool>(root, SettingKey::TimelineMessagesLayoutShowOwnAvatar, true);
    const auto pinnedReactionsDefault       = QStringLiteral("👍️,👎️,😀,🤣,❤️");
    const auto legacyPinnedReactionsDefault = QStringLiteral(":thumbsup:,:thumbsdown:,:smile:");
    pinnedReactions_ =
      readString(root, SettingKey::TimelineMessageActionsPinnedReactions, pinnedReactionsDefault);
    if (pinnedReactions_ == legacyPinnedReactionsDefault)
        pinnedReactions_ = pinnedReactionsDefault;
    animateImagesOnHover_ = readScalar<bool>(root, SettingKey::TimelineMediaAnimateOnHover, false);
    typingNotifications_ =
      readScalar<bool>(root, SettingKey::ComposerFeedbackTypingNotifications, true);
    roomSortOrder_ = roomSortOrderFromStorage(
      readString(root, SettingKey::SidebarsRoomListSort, QStringLiteral("unread_first_recent")),
      RoomSortOrder::UnreadFirst_Recent);
    readReceipts_       = readScalar<bool>(root, SettingKey::ComposerFeedbackReadReceipts, true);
    theme_              = readString(root, SettingKey::UiThemeSlug, defaultTheme_);
    font_               = readString(root, SettingKey::UiFontFamily, QString());
    emojiFont_          = readString(root, SettingKey::UiFontEmojiFamily, QString());
    useCircularAvatars_ = readScalar<bool>(root, SettingKey::UiAvatarsCircular, false);
    useIdenticon_       = readScalar<bool>(root, SettingKey::UiAvatarsIdenticonFallback, true);
    openImagesInExternalApp_ =
      readScalar<bool>(root, SettingKey::TimelineMediaOpenImagesExternal, false);
    openVideosInExternalApp_ =
      readScalar<bool>(root, SettingKey::TimelineMediaOpenVideosExternal, false);
    decryptNotifications_ =
      readScalar<bool>(root, SettingKey::NotificationsDesktopDecryptMessages, true);
    showCommunityNotificationCounts_ =
      readScalar<bool>(root, SettingKey::SidebarsRoomListShowCommunityCounts, true);
    compactRoomList_ = readScalar<bool>(root, SettingKey::SidebarsRoomListCompact, false);
    showRoomListTime_ =
      readScalar<bool>(root, SettingKey::SidebarsRoomListShowLastMessageTime, true);
    showLastMessagePreview_ = lastMessagePreviewFromStorage(
      readString(root, SettingKey::SidebarsRoomListLastMessagePreview, QStringLiteral("always")),
      LastMessagePreview::Always);
    fancyEffects_  = readScalar<bool>(root, SettingKey::TimelineMediaEffectsEnabled, true);
    reducedMotion_ = !readScalar<bool>(root, SettingKey::UiMotionAnimationsEnabled, true);
    privacyScreen_ = readScalar<bool>(root, SettingKey::PrivacyScreenLockEnabled, false);
    privacyScreenTimeoutSeconds_ =
      readScalar<int>(root, SettingKey::PrivacyScreenLockTimeoutSeconds, 0);
    integrationsDbusApiAccess_ =
      readScalar<int>(root, SettingKey::IntegrationsDbusApiAccess, IntegrationsDbusAccessNone);
    if (integrationsDbusApiAccess_ < IntegrationsDbusAccessNone ||
        integrationsDbusApiAccess_ > IntegrationsDbusAccessReadWrite)
        integrationsDbusApiAccess_ = IntegrationsDbusAccessNone;
    integrationsLinksBrowserCommand_ =
      readString(root, SettingKey::IntegrationsBrowserCommand, QString());
    updateSpaceVias_ = readScalar<bool>(root, SettingKey::PrivacyMaintenanceUpdateSpaceVias, true);
    expireEvents_    = readScalar<bool>(root, SettingKey::PrivacyMaintenanceExpireEvents, false);
    presence_        = presenceFromStorage(
      readString(root, SettingKey::NetworkPresenceDefault, QStringLiteral("automatic_presence")),
      Presence::AutomaticPresence);
    mobileMode_          = !readScalar<bool>(root, SettingKey::UiInputEnableTextSelection, true);
    enableSwipeGestures_ = readScalar<bool>(root, SettingKey::UiInputSwipeGestures, false);

    if (!emojiFont_.isEmpty())
        nhlog::ui()->info("Emoji font: \"{}\" (from settings)", emojiFont_.toStdString());

    scaleFactor_      = readScalar<double>(root, SettingKey::UiScaleFactor, -1.0);
    baseFontSize_     = readScalar<double>(root, SettingKey::UiFontSizePt, 13.0);
    ringtone_         = readString(root, SettingKey::CallsAudioRingtone, QStringLiteral("Default"));
    microphone_       = readString(root, SettingKey::CallsDevicesMicrophone, QString());
    camera_           = readString(root, SettingKey::CallsDevicesCamera, QString());
    cameraResolution_ = readString(root, SettingKey::CallsDevicesCameraResolution, QString());
    cameraFrameRate_  = readString(root, SettingKey::CallsDevicesCameraFrameRate, QString());
    screenShareFrameRate_ = readScalar<int>(root, SettingKey::CallsScreenshareFrameRate, 5);
    screenSharePiP_ = readScalar<bool>(root, SettingKey::CallsScreensharePictureInPicture, true);
    screenShareRemoteVideo_ =
      readScalar<bool>(root, SettingKey::CallsScreenshareIncludeRemoteVideo, false);
    screenShareHideCursor_ = readScalar<bool>(root, SettingKey::CallsScreenshareHideCursor, false);
    useFallbackCallRelayServer_ =
      readScalar<bool>(root, SettingKey::CallsRelayUseFallbackServer, false);
    enableLegacyCalls_ = readScalar<bool>(root, SettingKey::CallsLegacyEnabled, false);
    showImage_         = showImageFromStorage(
      readString(root, SettingKey::TimelineMediaImageDisplay, QStringLiteral("always")),
      ShowImage::Always);
    showSenderUsername_ = showSenderUsernameFromStorage(
      readString(
        root, SettingKey::TimelineMessagesSenderUsername, QStringLiteral("only_in_large_rooms")),
      ShowSenderUsername::OnlyInLargeRooms);

    shareKeysWithTrustedUsers_ =
      readScalar<bool>(root, SettingKey::EncryptionKeySharingShareWithTrusted, false);
    onlyShareKeysWithVerifiedUsers_ =
      readScalar<bool>(root, SettingKey::EncryptionKeySharingOnlyVerifiedUsers, false);
    useOnlineKeyBackup_ = readScalar<bool>(root, SettingKey::EncryptionBackupOnlineEnabled, true);

    disableCertificateValidation_ =
      readScalar<bool>(root, SettingKey::NetworkTlsDisableCertificateValidation, false);
    maxDbSize_   = readScalar<qulonglong>(root, SettingKey::DbMaxSizeBytes, 0);
    maxDbs_      = readScalar<uint>(root, SettingKey::DbMaxFiles, 0);
    enableHttp3_ = readScalar<bool>(root, SettingKey::NetworkHttp3Enabled, false);

    if (scaleFactor_ < 1.0 || scaleFactor_ > 3.0)
        scaleFactor_ = -1.0;
}

void
UserSettings::loadSessionYaml(const YAML::Node &root)
{
    homeserver_ = readString(root, SettingKey::SessionAccountHomeserver, QString());
    userId_     = readString(root, SettingKey::SessionAccountUserId, QString());
    deviceId_   = readString(root, SettingKey::SessionDeviceId, QString());

    nhlog::ui()->info(
      "Loaded session identity (has_user_id={}, has_device_id={}, has_homeserver={}, "
      "user_id='{}', device_id='{}', homeserver='{}')",
      !userId_.trimmed().isEmpty(),
      !deviceId_.trimmed().isEmpty(),
      !homeserver_.trimmed().isEmpty(),
      userId_.toStdString(),
      deviceId_.toStdString(),
      homeserver_.toStdString());
}

void
UserSettings::loadStateYaml(const YAML::Node &root)
{
    windowWidth_        = readScalar<int>(root, SettingKey::AppWindowSizeWidth, 0);
    windowHeight_       = readScalar<int>(root, SettingKey::AppWindowSizeHeight, 0);
    roomListWidth_      = readScalar<int>(root, SettingKey::SidebarsRoomListWidthPx, -1);
    communityListWidth_ = readScalar<int>(root, SettingKey::SidebarsCommunitiesWidthPx, 200);
    currentTagId_       = readString(root, SettingKey::SessionNavigationCurrentTagId, QString());
    hiddenTags_         = readStringList(root, SettingKey::SidebarsCommunitiesHiddenTags);
    mutedTags_          = readStringList(
      root, SettingKey::SidebarsCommunitiesMutedTags, QStringList{QStringLiteral("global")});
    hiddenPins_      = readStringList(root, SettingKey::TimelinePinsHidden);
    hiddenWidgets_   = readStringList(root, SettingKey::TimelineWidgetsHidden);
    recentReactions_ = readStringList(root, SettingKey::ComposerReactionsRecent);
    collapsedSpaces_ = readNestedStringLists(root, SettingKey::SidebarsCommunitiesCollapsedSpaces);
}

void
UserSettings::saveConfigYaml() const
{
    YAML::Node root(YAML::NodeType::Map);

    setNode(root, SettingKey::AppWindowTrayEnabled, tray_);
    setNode(root, SettingKey::AppStartupStartInTray, startInTray_);
    setNode(root, SettingKey::UiThemeSlug, theme().toStdString());
    setNode(root, SettingKey::UiFontFamily, font_.toStdString());
    setNode(root, SettingKey::UiFontEmojiFamily, emojiFont_.toStdString());
    if (scaleFactor_ >= 1.0 && scaleFactor_ <= 3.0)
        setNode(root, SettingKey::UiScaleFactor, scaleFactor_);
    setNode(root, SettingKey::UiFontSizePt, baseFontSize_);
    setNode(root, SettingKey::UiMotionAnimationsEnabled, !reducedMotion_);
    setNode(root, SettingKey::UiInputEnableTextSelection, !mobileMode_);
    setNode(root, SettingKey::UiInputSwipeGestures, enableSwipeGestures_);
    setNode(root, SettingKey::UiAvatarsCircular, useCircularAvatars_);
    setNode(root, SettingKey::UiAvatarsIdenticonFallback, useIdenticon_);
    setNode(root, SettingKey::SidebarsRoomListCompact, compactRoomList_);
    setNode(root, SettingKey::SidebarsRoomListShowLastMessageTime, showRoomListTime_);
    setNode(root,
            SettingKey::SidebarsRoomListLastMessagePreview,
            toStorageValue(showLastMessagePreview_).toStdString());
    setNode(
      root, SettingKey::SidebarsRoomListShowCommunityCounts, showCommunityNotificationCounts_);
    setNode(root, SettingKey::SidebarsRoomListScrollbarsEnabled, scrollbarsInRoomlist_);
    setNode(root, SettingKey::SidebarsRoomListSort, toStorageValue(roomSortOrder_).toStdString());
    setNode(root, SettingKey::SidebarsCommunitiesVisible, showCommunitiesSidebar_);
    setNode(root, SettingKey::TimelineMessagesLayoutBubbles, bubbles_);
    setNode(root, SettingKey::TimelineMessagesLayoutSmallAvatars, smallAvatars_);
    setNode(root, SettingKey::TimelineMessagesLayoutShowOwnAvatar, showOwnAvatarInBubbleLayout_);
    setNode(root,
            SettingKey::TimelineMessagesSenderUsername,
            toStorageValue(showSenderUsername_).toStdString());
    setNode(root, SettingKey::TimelineMessagesMaxWidthPx, maxTimelineWidth_);
    setNode(root, SettingKey::TimelineMessagesEmojiOnlyEnlarge, enlargeEmojiOnlyMessages_);
    setNode(root, SettingKey::TimelineMessagesHoverHighlight, messageHoverHighlight_);
    setNode(root, SettingKey::TimelineMessageActionsEnabled, showActionButtons_);
    setNode(
      root, SettingKey::TimelineMessageActionsPinnedReactions, pinnedReactions_.toStdString());
    setNode(root, SettingKey::TimelineMediaEffectsEnabled, fancyEffects_);
    setNode(root, SettingKey::TimelineMediaAnimateOnHover, animateImagesOnHover_);
    setNode(root, SettingKey::TimelineMediaImageDisplay, toStorageValue(showImage_).toStdString());
    setNode(root, SettingKey::TimelineMediaOpenImagesExternal, openImagesInExternalApp_);
    setNode(root, SettingKey::TimelineMediaOpenVideosExternal, openVideosInExternalApp_);
    setNode(root, SettingKey::ComposerInputMarkdownEnabled, markdown_);
    setNode(root, SettingKey::ComposerInputSendKey, toStorageValue(sendMessageKey_).toStdString());
    setNode(root,
            SettingKey::ComposerInputAutoReplaceEmoji,
            toStorageValue(autoReplaceEmoji_).toStdString());
    setNode(root, SettingKey::ComposerFeedbackTypingNotifications, typingNotifications_);
    setNode(root, SettingKey::ComposerFeedbackReadReceipts, readReceipts_);
    setNode(root, SettingKey::ComposerExtrasStickersEnabled, enableStickers_);
    setNode(root, SettingKey::NotificationsDesktopEnabled, hasDesktopNotifications_);
    setNode(root, SettingKey::NotificationsDesktopAlertOnIncoming, alertOnIncomingMessages_);
    setNode(root, SettingKey::NotificationsDesktopDecryptMessages, decryptNotifications_);
    setNode(root, SettingKey::CallsLegacyEnabled, enableLegacyCalls_);
    setNode(root, SettingKey::CallsRelayUseFallbackServer, useFallbackCallRelayServer_);
    setNode(root, SettingKey::CallsDevicesMicrophone, microphone_.toStdString());
    setNode(root, SettingKey::CallsDevicesCamera, camera_.toStdString());
    setNode(root, SettingKey::CallsDevicesCameraResolution, cameraResolution_.toStdString());
    setNode(root, SettingKey::CallsDevicesCameraFrameRate, cameraFrameRate_.toStdString());
    setNode(root, SettingKey::CallsAudioRingtone, ringtone_.toStdString());
    setNode(root, SettingKey::CallsScreenshareFrameRate, screenShareFrameRate_);
    setNode(root, SettingKey::CallsScreensharePictureInPicture, screenSharePiP_);
    setNode(root, SettingKey::CallsScreenshareIncludeRemoteVideo, screenShareRemoteVideo_);
    setNode(root, SettingKey::CallsScreenshareHideCursor, screenShareHideCursor_);
    setNode(root, SettingKey::PrivacyScreenLockEnabled, privacyScreen_);
    setNode(root, SettingKey::PrivacyScreenLockTimeoutSeconds, privacyScreenTimeoutSeconds_);
    setNode(root, SettingKey::PrivacyMaintenanceExpireEvents, expireEvents_);
    setNode(root, SettingKey::PrivacyMaintenanceUpdateSpaceVias, updateSpaceVias_);
    setNode(
      root, SettingKey::EncryptionKeySharingOnlyVerifiedUsers, onlyShareKeysWithVerifiedUsers_);
    setNode(root, SettingKey::EncryptionKeySharingShareWithTrusted, shareKeysWithTrustedUsers_);
    setNode(root, SettingKey::EncryptionBackupOnlineEnabled, useOnlineKeyBackup_);
    setNode(
      root, SettingKey::NetworkTlsDisableCertificateValidation, disableCertificateValidation_);
    setNode(root, SettingKey::NetworkPresenceDefault, toStorageValue(presence_).toStdString());
    setNode(root, SettingKey::NetworkHttp3Enabled, enableHttp3_);
    setNode(root, SettingKey::DbMaxSizeBytes, maxDbSize_);
    setNode(root, SettingKey::DbMaxFiles, maxDbs_);
    setNode(root, SettingKey::IntegrationsDbusApiAccess, integrationsDbusApiAccess_);
    setNode(
      root, SettingKey::IntegrationsBrowserCommand, integrationsLinksBrowserCommand_.toStdString());
    setNode(root,
            SettingKey::SecretsProvider,
            (runWithoutSecureSecretsService_
               ? QString::fromLatin1(staged_load_plan::ProviderFileValue)
               : QString::fromLatin1(staged_load_plan::ProviderSecretServiceValue))
              .toStdString());

    if (writeYamlFile(configFilePath_, root, false))
        nhlog::ui()->debug("Saved config to: {}", configFilePath_.toStdString());
}

void
UserSettings::saveSessionYaml() const
{
    const bool hasAccessToken = hasSessionValue(accessToken_);
    const bool hasUserId      = hasSessionValue(userId_);
    const bool hasDeviceId    = hasSessionValue(deviceId_);

    if (hasAccessToken && (!hasUserId || !hasDeviceId)) {
        nhlog::ui()->warn(
          "Skipping session.yml write for profile '{}' because session identity is incomplete "
          "(has_user_id={}, has_device_id={}, has_access_token=true)",
          app_paths::normalizedProfileId(profile_).toStdString(),
          hasUserId,
          hasDeviceId);
        return;
    }

    YAML::Node root(YAML::NodeType::Map);
    setNode(root, SettingKey::SessionAccountUserId, userId_.toStdString());
    setNode(root, SettingKey::SessionAccountHomeserver, homeserver_.toStdString());
    setNode(root, SettingKey::SessionDeviceId, deviceId_.toStdString());

    if (writeYamlFile(sessionFilePath_, root, false))
        nhlog::ui()->debug("Saved session to: {}", sessionFilePath_.toStdString());
}

void
UserSettings::saveSecretsYaml() const
{
    settings::persistence::saveProfileSecrets(
      profile_, runWithoutSecureSecretsService_, secretsFilePath_, accessToken_, secrets_);
}

void
UserSettings::saveStateYaml() const
{
    YAML::Node root(YAML::NodeType::Map);

    setNode(root, SettingKey::AppWindowSizeWidth, windowWidth_);
    setNode(root, SettingKey::AppWindowSizeHeight, windowHeight_);
    setNode(root, SettingKey::SidebarsRoomListWidthPx, roomListWidth_);
    setNode(root, SettingKey::SidebarsCommunitiesWidthPx, communityListWidth_);
    setNode(root, SettingKey::SessionNavigationCurrentTagId, currentTagId_.toStdString());
    writeStringList(root, SettingKey::SidebarsCommunitiesHiddenTags, hiddenTags_);
    writeStringList(root, SettingKey::SidebarsCommunitiesMutedTags, mutedTags_);
    writeNestedStringLists(root, SettingKey::SidebarsCommunitiesCollapsedSpaces, collapsedSpaces_);
    writeStringList(root, SettingKey::TimelinePinsHidden, hiddenPins_);
    writeStringList(root, SettingKey::TimelineWidgetsHidden, hiddenWidgets_);
    writeStringList(root, SettingKey::ComposerReactionsRecent, recentReactions_);

    if (writeYamlFile(stateFilePath_, root, false))
        nhlog::ui()->debug("Saved state to: {}", stateFilePath_.toStdString());
}

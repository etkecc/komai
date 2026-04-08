QString
showImageToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.timelineMediaImageDisplay());
}

void
applyShowImageFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setTimelineMediaImageDisplay(
      showImageFromStorage(rawToken, UserSettings::ShowImage::Always));
}

QString
avatarSizeToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.timelineMessagesLayoutAvatarSize());
}

void
applyAvatarSizeFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setTimelineMessagesLayoutAvatarSize(
      avatarSizeFromStorage(rawToken, UserSettings::AvatarSize::Regular));
}

QString
showSenderUsernameToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.timelineMessagesSenderUsername());
}

void
applyShowSenderUsernameFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setTimelineMessagesSenderUsername(
      showSenderUsernameFromStorage(rawToken, UserSettings::ShowSenderUsername::OnlyInLargeRooms));
}

QString
timelineActionsPolicyToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.timelineMessageActionsActivationPolicy());
}

void
applyTimelineActionsPolicyFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setTimelineMessageActionsActivationPolicy(
      timelineMessageActionsActivationPolicyFromStorage(
        rawToken, UserSettings::TimelineMessageActionsActivationPolicy::ActionsButton));
}

QString
timelineStyleToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.timelineMessagesStyle());
}

void
applyTimelineStyleFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setTimelineMessagesStyle(timelineMessagesStyleFromStorage(
      rawToken, UserSettings::TimelineMessagesStyle::Bubbles));
}

QString
timelinePositioningToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.timelineMessagesLayoutPositioning());
}

void
applyTimelinePositioningFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setTimelineMessagesLayoutPositioning(timelineMessagesLayoutPositioningFromStorage(
      rawToken, UserSettings::TimelineMessagesLayoutPositioning::OpposingBySender));
}

QString
timelineUserColorCodingPolicyToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.timelineUserColorCodingPolicy());
}

void
applyTimelineUserColorCodingPolicyFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setTimelineUserColorCodingPolicy(timelineUserColorCodingPolicyFromStorage(
      rawToken, UserSettings::TimelineUserColorCodingPolicy::AdaptiveByRoomSize));
}

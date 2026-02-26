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

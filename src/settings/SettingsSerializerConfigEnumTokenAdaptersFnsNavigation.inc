QString
roomSortOrderToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.navigationRoomListSort());
}

void
applyRoomSortOrderFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setNavigationRoomListSort(
      roomSortOrderFromStorage(rawToken, UserSettings::RoomSortOrder::UnreadFirst_Recent));
}

QString
lastMessagePreviewToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.navigationRoomListLastMessagePreview());
}

void
applyLastMessagePreviewFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setNavigationRoomListLastMessagePreview(
      lastMessagePreviewFromStorage(rawToken, UserSettings::LastMessagePreview::Always));
}

QString
unreadDetectionPolicyToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.navigationRoomListUnreadDetectionPolicy());
}

void
applyUnreadDetectionPolicyFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setNavigationRoomListUnreadDetectionPolicy(
      unreadDetectionPolicyFromStorage(rawToken, UserSettings::UnreadDetectionPolicy::AnyEvent));
}

QString
tabPinButtonVisibilityToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.navigationTabsShowPinButton());
}

void
applyTabPinButtonVisibilityFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setNavigationTabsShowPinButton(
      tabPinButtonVisibilityFromStorage(rawToken, UserSettings::TabPinButtonVisibility::Never));
}

QString
tabPinnedTabLabelToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.navigationTabsPinnedTabLabel());
}

void
applyTabPinnedTabLabelFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setNavigationTabsPinnedTabLabel(
      tabLabelDisplayFromStorage(rawToken, UserSettings::TabLabelDisplay::AvatarAndLabel));
}

QString
tabTabLabelToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.navigationTabsTabLabel());
}

void
applyTabTabLabelFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setNavigationTabsTabLabel(
      tabLabelDisplayFromStorage(rawToken, UserSettings::TabLabelDisplay::AvatarAndLabel));
}

QString
roomSortOrderToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.sidebarsRoomListSort());
}

void
applyRoomSortOrderFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setSidebarsRoomListSort(
      roomSortOrderFromStorage(rawToken, UserSettings::RoomSortOrder::UnreadFirst_Recent));
}

QString
lastMessagePreviewToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.sidebarsRoomListLastMessagePreview());
}

void
applyLastMessagePreviewFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setSidebarsRoomListLastMessagePreview(
      lastMessagePreviewFromStorage(rawToken, UserSettings::LastMessagePreview::Always));
}

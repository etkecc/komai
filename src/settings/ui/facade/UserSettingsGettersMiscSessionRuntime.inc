QString
UserSettings::profile() const
{
    return profile_;
}
QString
UserSettings::userId() const
{
    return userId_;
}
QString
UserSettings::accessToken() const
{
    return accessToken_;
}
QString
UserSettings::deviceId() const
{
    return deviceId_;
}
QString
UserSettings::currentTagId() const
{
    return currentTagId_;
}
QString
UserSettings::homeserver() const
{
    return homeserver_;
}
QStringList
UserSettings::hiddenTags() const
{
    return hiddenTags_;
}
QStringList
UserSettings::mutedTags() const
{
    return mutedTags_;
}
QStringList
UserSettings::hiddenPins() const
{
    return hiddenPins_;
}
QStringList
UserSettings::hiddenWidgets() const
{
    return hiddenWidgets_;
}
QStringList
UserSettings::recentReactions() const
{
    return recentReactions_;
}
QList<QStringList>
UserSettings::collapsedSpaces() const
{
    return collapsedSpaces_;
}
int
UserSettings::windowWidth() const
{
    return windowWidth_;
}
int
UserSettings::windowHeight() const
{
    return windowHeight_;
}
qulonglong
UserSettings::dbMaxSizeBytes() const
{
    return dbMaxSizeBytes_;
}
uint
UserSettings::dbMaxStores() const
{
    return dbMaxStores_;
}

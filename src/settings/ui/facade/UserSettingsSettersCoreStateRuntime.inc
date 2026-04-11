namespace {

QStringList
normalizeHiddenTimelineEventTypes(const QStringList &eventTypes)
{
    QStringList normalized;

    for (const auto &eventType : eventTypes) {
        const auto trimmed = eventType.trimmed();
        if (trimmed.isEmpty() || normalized.contains(trimmed))
            continue;

        normalized.push_back(trimmed);
    }

    normalized.sort();
    return normalized;
}

QMap<QString, QStringList>
normalizeHiddenTimelineEventTypesByRoom(const QMap<QString, QStringList> &eventTypesByRoom,
                                        const QStringList &globalEventTypes)
{
    QMap<QString, QStringList> normalized;

    for (auto it = eventTypesByRoom.constBegin(); it != eventTypesByRoom.constEnd(); ++it) {
        const auto roomId = it.key().trimmed();
        if (roomId.isEmpty())
            continue;

        const auto roomEventTypes = normalizeHiddenTimelineEventTypes(it.value());
        if (roomEventTypes.isEmpty() || roomEventTypes == globalEventTypes)
            continue;

        normalized.insert(roomId, roomEventTypes);
    }

    return normalized;
}

} // namespace

void
UserSettings::setGlobalExcludes(const QStringList &globalExcludes)
{
    if (globalExcludes_ == globalExcludes)
        return;

    globalExcludes_ = globalExcludes;
    save();
    emit globalExcludesChanged();
}

void
UserSettings::setBadgesHiddenFilters(const QStringList &badgesHiddenFilters)
{
    badgesHiddenFilters_ = badgesHiddenFilters;
    save();
}

void
UserSettings::setHiddenPins(const QStringList &hiddenTags)
{
    hiddenPins_ = hiddenTags;
    save();
    emit hiddenPinsChanged();
}

void
UserSettings::setOpenTabs(const QStringList &openTabs)
{
    openTabs_ = openTabs;
    save();
    emit openTabsChanged();
}

void
UserSettings::setHiddenWidgets(const QStringList &hiddenTags)
{
    hiddenWidgets_ = hiddenTags;
    save();
    emit hiddenWidgetsChanged();
}

void
UserSettings::setHiddenSpaces(const QStringList &hiddenSpaces)
{
    if (hiddenSpaces_ == hiddenSpaces)
        return;

    hiddenSpaces_ = hiddenSpaces;
    save();
    emit hiddenSpacesChanged();
}

void
UserSettings::setHiddenTimelineEventTypes(const QStringList &eventTypes)
{
    const auto normalizedGlobal = normalizeHiddenTimelineEventTypes(eventTypes);
    const auto normalizedByRoom =
      normalizeHiddenTimelineEventTypesByRoom(hiddenTimelineEventTypesByRoom_, normalizedGlobal);

    if (hiddenTimelineEventTypes_ == normalizedGlobal &&
        hiddenTimelineEventTypesByRoom_ == normalizedByRoom) {
        return;
    }

    hiddenTimelineEventTypes_       = normalizedGlobal;
    hiddenTimelineEventTypesByRoom_ = normalizedByRoom;
    save();
    emit hiddenTimelineEventTypesChanged();
}

void
UserSettings::setHiddenTimelineEventTypesByRoom(const QMap<QString, QStringList> &eventTypesByRoom)
{
    const auto normalized =
      normalizeHiddenTimelineEventTypesByRoom(eventTypesByRoom, hiddenTimelineEventTypes_);
    if (hiddenTimelineEventTypesByRoom_ == normalized)
        return;

    hiddenTimelineEventTypesByRoom_ = normalized;
    save();
    emit hiddenTimelineEventTypesChanged();
}

void
UserSettings::setHiddenTimelineEventTypesForRoom(const QString &roomId,
                                                 const QStringList &eventTypes)
{
    const auto trimmedRoomId = roomId.trimmed();
    if (trimmedRoomId.isEmpty()) {
        setHiddenTimelineEventTypes(eventTypes);
        return;
    }

    const auto normalizedEventTypes = normalizeHiddenTimelineEventTypes(eventTypes);
    auto nextByRoom                 = hiddenTimelineEventTypesByRoom_;

    if (normalizedEventTypes.isEmpty() || normalizedEventTypes == hiddenTimelineEventTypes_)
        nextByRoom.remove(trimmedRoomId);
    else
        nextByRoom.insert(trimmedRoomId, normalizedEventTypes);

    if (nextByRoom == hiddenTimelineEventTypesByRoom_)
        return;

    hiddenTimelineEventTypesByRoom_ = nextByRoom;
    save();
    emit hiddenTimelineEventTypesChanged();
}

void
UserSettings::setComposerDraftsByRoom(const QMap<QString, QString> &draftsByRoom)
{
    QMap<QString, QString> normalizedDraftsByRoom;
    for (auto it = draftsByRoom.constBegin(); it != draftsByRoom.constEnd(); ++it) {
        if (it.key().isEmpty() || it.value().trimmed().isEmpty())
            continue;
        normalizedDraftsByRoom.insert(it.key(), it.value());
    }

    if (composerDraftsByRoom_ == normalizedDraftsByRoom)
        return;

    composerDraftsByRoom_ = std::move(normalizedDraftsByRoom);
    save();
    emit composerDraftsByRoomChanged();
}

void
UserSettings::setComposerDraftForRoom(const QString &roomId, const QString &draftText)
{
    if (roomId.isEmpty())
        return;

    const auto trimmed = draftText.trimmed();
    if (trimmed.isEmpty()) {
        clearComposerDraftForRoom(roomId);
        return;
    }

    const auto existing = composerDraftsByRoom_.value(roomId);
    if (existing == draftText)
        return;

    composerDraftsByRoom_.insert(roomId, draftText);
    emit composerDraftsByRoomChanged();
    if (suppressSettingsSave_)
        return;

    settings::SettingsController controller;
    controller.save(*this, settings::SettingsController::SavePolicy::StateOnly);
}

void
UserSettings::clearComposerDraftForRoom(const QString &roomId)
{
    if (roomId.isEmpty())
        return;

    if (composerDraftsByRoom_.remove(roomId) == 0)
        return;

    emit composerDraftsByRoomChanged();
    if (suppressSettingsSave_)
        return;

    settings::SettingsController controller;
    controller.save(*this, settings::SettingsController::SavePolicy::StateOnly);
}

void
UserSettings::clearAllComposerDrafts()
{
    if (composerDraftsByRoom_.isEmpty())
        return;

    composerDraftsByRoom_.clear();
    emit composerDraftsByRoomChanged();
    if (suppressSettingsSave_)
        return;

    settings::SettingsController controller;
    controller.save(*this, settings::SettingsController::SavePolicy::StateOnly);
}

void
UserSettings::setCollapsedSpaces(QStringList spaces)
{
    collapsedSpaces_ = spaces;
    save();
}

void
UserSettings::setDonationStatus(QString s)
{
    setSetting(donationStatus_, s, &UserSettings::donationStatusChanged);
}
void
UserSettings::setWindowWidth(int s)
{
    setSetting(windowWidth_, s, &UserSettings::windowWidthChanged);
}
void
UserSettings::setWindowHeight(int s)
{
    setSetting(windowHeight_, s, &UserSettings::windowHeightChanged);
}

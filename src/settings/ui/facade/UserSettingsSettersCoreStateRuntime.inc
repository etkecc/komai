void
UserSettings::setGlobalExcludes(const QStringList &globalExcludes)
{
    globalExcludes_ = globalExcludes;
    save();
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
UserSettings::setHiddenWidgets(const QStringList &hiddenTags)
{
    hiddenWidgets_ = hiddenTags;
    save();
    emit hiddenWidgetsChanged();
}

void
UserSettings::setRecentReactions(QStringList recent)
{
    recentReactions_ = recent;
    save();
    emit recentReactionsChanged();
}

void
UserSettings::setComposerDraftsByRoom(const QMap<QString, QString> &draftsByRoom)
{
    composerDraftsByRoom_.clear();
    for (auto it = draftsByRoom.constBegin(); it != draftsByRoom.constEnd(); ++it) {
        if (it.key().isEmpty() || it.value().trimmed().isEmpty())
            continue;
        composerDraftsByRoom_.insert(it.key(), it.value());
    }
    save();
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
UserSettings::setWindowWidth(int s)
{
    setSetting(windowWidth_, s, &UserSettings::windowWidthChanged);
}
void
UserSettings::setWindowHeight(int s)
{
    setSetting(windowHeight_, s, &UserSettings::windowHeightChanged);
}

void
UserSettings::setDbMaxSizeBytes(qulonglong s)
{
    setSetting(dbMaxSizeBytes_, s, &UserSettings::dbMaxSizeBytesChanged);
}
void
UserSettings::setDbMaxStores(uint s)
{
    setSetting(dbMaxStores_, s, &UserSettings::dbMaxStoresChanged);
}

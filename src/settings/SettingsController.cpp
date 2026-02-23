// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <yaml-cpp/yaml.h>

#include "SettingsController.h"
#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>
#include <utility>

#include "Paths.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsPersistence.h"
#include "settings/SettingsStorage.h"
#include "settings/StagedLoadPlan.h"
#include "settings/YamlSettings.h"
#include "settings/core/SettingDefinition.h"
#include "settings/core/SettingsConstraints.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/core/SettingsSerializer.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include <string>
#include <string_view>

namespace {

using settings::storage::configFilePathForProfile;
using settings::storage::createDir;
using settings::storage::loadYamlFile;
using settings::storage::pathExists;
using settings::storage::profileDirPath;
using settings::storage::removePath;
using settings::storage::secretsFilePathForProfile;
using settings::storage::sessionFilePathForProfile;
using settings::storage::stateFilePathForProfile;
using settings::storage::writeYamlFile;

using settings::persistence::providerFromConfig;

} // namespace

namespace {

std::shared_ptr<spdlog::logger>
nullLogger(std::string_view name)
{
    static auto sink   = std::make_shared<spdlog::sinks::null_sink_mt>();
    static auto logger = std::make_shared<spdlog::logger>(std::string(name), sink);
    return logger;
}

settings::ControllerLoggers
defaultLoggers()
{
    return {.ui = nullLogger("settings-controller-ui")};
}

settings::ControllerLoggers &
currentLoggers()
{
    static settings::ControllerLoggers loggers = defaultLoggers();
    return loggers;
}

void
syncCoreStoreFromSettings(UserSettings &settings)
{
    auto &store = settings.mutableCoreStore();
    store.clear();
    settings::core::constraints::applyDefaultConstraints(store);

    const auto set = [&store](settings::core::SettingId id,
                              settings::core::SettingsStore::Value value) {
        (void)store.setValue(id, std::move(value));
    };

    set(settings::core::SettingId::UiThemeSlug, settings.theme().toStdString());
    set(settings::core::SettingId::UiFontFamily, settings.font().toStdString());
    set(settings::core::SettingId::UiFontSizePt, settings.fontSize());
    set(settings::core::SettingId::UiFontEmojiFamily, settings.emojiFontFamily().toStdString());
    set(settings::core::SettingId::UiMotionAnimationsEnabled, settings.uiAnimationsEnabled());
    set(settings::core::SettingId::UiInputEnableTextSelection, settings.textSelectionEnabled());
    set(settings::core::SettingId::UiInputSwipeGestures, settings.swipeGesturesEnabled());
    set(settings::core::SettingId::UiAvatarsCircular, settings.circularAvatarsEnabled());
    set(settings::core::SettingId::UiAvatarsIdenticonFallback, settings.identiconFallbackEnabled());
    set(settings::core::SettingId::SidebarsRoomListCompact, settings.compactRoomList());
    set(settings::core::SettingId::SidebarsRoomListShowLastMessageTime,
        settings.roomListShowLastMessageTime());
    set(settings::core::SettingId::SidebarsRoomListLastMessagePreview,
        static_cast<int>(settings.showLastMessagePreview()));
    set(settings::core::SettingId::SidebarsRoomListShowCommunityCounts,
        settings.communityNotificationCountsVisible());
    set(settings::core::SettingId::SidebarsRoomListScrollbarsEnabled,
        settings.roomListScrollbarsVisible());
    set(settings::core::SettingId::SidebarsRoomListSort,
        static_cast<int>(settings.roomSortOrder()));
    set(settings::core::SettingId::SidebarsCommunitiesVisible,
        settings.communitiesSidebarVisible());
    set(settings::core::SettingId::NetworkPresenceStatusPolicy,
        static_cast<int>(settings.presence()));
    set(settings::core::SettingId::PrivacyMaintenanceExpireEvents, settings.expireEvents());
    set(settings::core::SettingId::PrivacyMaintenanceUpdateSpaceVias, settings.updateSpaceVias());
    set(settings::core::SettingId::PrivacyScreenLockEnabled, settings.privacyScreen());
    set(settings::core::SettingId::PrivacyScreenLockTimeoutSeconds,
        settings.privacyScreenTimeoutSeconds());
    set(settings::core::SettingId::IntegrationsSystemTrayEnabled, settings.systemTrayEnabled());
    set(settings::core::SettingId::IntegrationsSystemTrayAutostart, settings.systemTrayAutostart());
    set(settings::core::SettingId::IntegrationsDbusApiAccess, settings.integrationsDbusApiAccess());
    set(settings::core::SettingId::IntegrationsBrowserCommand,
        settings.integrationsLinksBrowserCommand().toStdString());
    set(settings::core::SettingId::ComposerInputMarkdownEnabled, settings.markdownEnabled());
    set(settings::core::SettingId::ComposerInputSendKey,
        static_cast<int>(settings.sendMessageKey()));
    set(settings::core::SettingId::ComposerInputAutoReplaceEmoji,
        static_cast<int>(settings.autoReplaceEmoji()));
    set(settings::core::SettingId::ComposerFeedbackTypingNotifications,
        settings.typingNotificationsEnabled());
    set(settings::core::SettingId::ComposerFeedbackReadReceipts, settings.readReceiptsEnabled());
    set(settings::core::SettingId::ComposerExtrasStickersEnabled, settings.stickersEnabled());
    set(settings::core::SettingId::NotificationsDesktopEnabled,
        settings.desktopNotificationsEnabled());
    set(settings::core::SettingId::NotificationsDesktopAlertOnIncoming,
        settings.alertOnIncomingMessages());
    set(settings::core::SettingId::NotificationsDesktopDecryptMessages,
        settings.decryptNotifications());
    set(settings::core::SettingId::CallsLegacyEnabled, settings.legacyCallsEnabled());
    set(settings::core::SettingId::CallsRelayUseFallbackServer,
        settings.fallbackCallRelayServerEnabled());
    set(settings::core::SettingId::CallsDevicesMicrophone, settings.microphone().toStdString());
    set(settings::core::SettingId::CallsDevicesCamera, settings.camera().toStdString());
    set(settings::core::SettingId::CallsDevicesCameraResolution,
        settings.cameraResolution().toStdString());
    set(settings::core::SettingId::CallsDevicesCameraFrameRate,
        settings.cameraFrameRate().toStdString());
    set(settings::core::SettingId::CallsAudioRingtone, settings.ringtone().toStdString());
    set(settings::core::SettingId::TimelineMessagesLayoutBubbles,
        settings.timelineBubblesEnabled());
    set(settings::core::SettingId::TimelineMessagesLayoutSmallAvatars,
        settings.timelineSmallAvatarsEnabled());
    set(settings::core::SettingId::TimelineMessagesLayoutShowOwnAvatar,
        settings.timelineShowOwnAvatarInBubbleLayout());
    set(settings::core::SettingId::TimelineMessagesSenderUsername,
        static_cast<int>(settings.showSenderUsername()));
    set(settings::core::SettingId::TimelineMessagesMaxWidthPx, settings.maxTimelineWidth());
    set(settings::core::SettingId::TimelineMessagesEmojiOnlyEnlarge,
        settings.enlargeEmojiOnlyMessages());
    set(settings::core::SettingId::TimelineMessagesHoverHighlight,
        settings.messageHoverHighlight());
    set(settings::core::SettingId::TimelineMessageActionsEnabled,
        settings.timelineMessageActionsEnabled());
    set(settings::core::SettingId::TimelineMessageActionsPinnedReactions,
        settings.pinnedReactions().toStdString());
    set(settings::core::SettingId::TimelineMediaEffectsEnabled,
        settings.timelineMediaEffectsEnabled());
    set(settings::core::SettingId::TimelineMediaAnimateOnHover, settings.animateImagesOnHover());
    set(settings::core::SettingId::TimelineMediaImageDisplay,
        static_cast<int>(settings.showImage()));
    set(settings::core::SettingId::TimelineMediaOpenImagesExternal,
        settings.openImagesInExternalApp());
    set(settings::core::SettingId::TimelineMediaOpenVideosExternal,
        settings.openVideosInExternalApp());
    set(settings::core::SettingId::EncryptionKeySharingOnlyVerifiedUsers,
        settings.onlyShareKeysWithVerifiedUsers());
    set(settings::core::SettingId::EncryptionKeySharingShareWithTrusted,
        settings.shareKeysWithTrustedUsers());
    set(settings::core::SettingId::EncryptionBackupOnlineEnabled,
        settings.onlineKeyBackupEnabled());
}

const YAML::Node *
rootNodeForScope(settings::core::SettingScope scope,
                 const YAML::Node &configRoot,
                 const YAML::Node &stateRoot,
                 const YAML::Node &sessionRoot)
{
    switch (scope) {
    case settings::core::SettingScope::Config:
        return &configRoot;
    case settings::core::SettingScope::State:
        return &stateRoot;
    case settings::core::SettingScope::Session:
        return &sessionRoot;
    case settings::core::SettingScope::Runtime:
    case settings::core::SettingScope::Secrets:
        return nullptr;
    }

    return nullptr;
}

void
syncCoreStoreFromPersistence(UserSettings &settings,
                             const YAML::Node &configRoot,
                             const YAML::Node &stateRoot,
                             const YAML::Node &sessionRoot)
{
    syncCoreStoreFromSettings(settings);

    auto &store = settings.mutableCoreStore();
    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (definition.id == settings::core::SettingId::Unknown || !definition.persistedKey)
            continue;

        const auto *root = rootNodeForScope(definition.scope, configRoot, stateRoot, sessionRoot);
        if (!root)
            continue;

        const auto defaultValue = store.value(definition.id);
        if (!defaultValue.has_value())
            continue;

        const auto node = yaml_settings::getNode(*root, definition.persistedKey);
        (void)settings::core::serializer::setFromYamlNodeOrDefault(
          store, definition.id, node, *defaultValue);
    }
}

void
syncConfigYamlFromCoreStore(const QString &configFilePath,
                            const settings::core::SettingsStore &store)
{
    YAML::Node configRoot = loadYamlFile(configFilePath, "config");
    bool changed          = false;

    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (definition.id == settings::core::SettingId::Unknown || !definition.persistedKey ||
            definition.scope != settings::core::SettingScope::Config)
            continue;

        const auto value = store.value(definition.id);
        if (!value.has_value())
            continue;

        yaml_settings::setNode(
          configRoot, definition.persistedKey, settings::core::serializer::toYamlNode(*value));
        changed = true;
    }

    if (changed)
        writeYamlFile(configFilePath, configRoot, true);
}

} // namespace

void
settings::setLoggers(settings::ControllerLoggers loggers)
{
    const auto &defaults = defaultLoggers();
    if (!loggers.ui)
        loggers.ui = defaults.ui;
    currentLoggers() = std::move(loggers);
}

const settings::ControllerLoggers &
settings::activeLoggers()
{
    return currentLoggers();
}

void
settings::SettingsController::load(UserSettings &settings, std::optional<QString> profile)
{
    load(settings, profile, YAML::Node());
}

void
settings::SettingsController::load(UserSettings &settings,
                                   std::optional<QString> profile,
                                   const YAML::Node &configRoot)
{
    if (profile)
        settings.profile_ = (*profile == QLatin1String("default")) ? QLatin1String("") : *profile;
    else
        settings.profile_ = QLatin1String("");

    settings.profileDirPath_  = profileDirPath(settings.profile_);
    settings.configFilePath_  = configFilePathForProfile(settings.profile_);
    settings.stateFilePath_   = stateFilePathForProfile(settings.profile_);
    settings.sessionFilePath_ = sessionFilePathForProfile(settings.profile_);
    settings.secretsFilePath_ = secretsFilePathForProfile(settings.profile_);
    createDir(settings.profileDirPath_);

    settings.setPersistenceSuspended(true);

    const auto effectiveConfig =
      configRoot.IsDefined() ? configRoot : loadYamlFile(settings.configFilePath_, "config");
    settings.loadConfigYaml(effectiveConfig);

    const auto provider               = providerFromConfig(effectiveConfig);
    settings.usesFileSecretsProvider_ = provider == staged_load_plan::SecretsProvider::File;
    YAML::Node sessionRoot;
    YAML::Node stateRoot;

    for (const auto stage : staged_load_plan::stagesForProvider(provider)) {
        switch (stage) {
        case staged_load_plan::Stage::Config:
            break;
        case staged_load_plan::Stage::Session: {
            sessionRoot = loadYamlFile(settings.sessionFilePath_, "session");
            settings.loadSessionYaml(sessionRoot);
            break;
        }
        case staged_load_plan::Stage::SecretsSecureBackend:
        case staged_load_plan::Stage::SecretsFile: {
            const auto payload = settings::persistence::loadProfileSecrets(
              settings.profile_, settings.usesFileSecretsProvider_, settings.secretsFilePath_);
            settings.accessToken_ = payload.accessToken;
            settings.secrets_     = payload.secrets;

            const auto snapshot = settings.sessionSnapshot();
            if ((snapshot.userId.isEmpty() || snapshot.deviceId.isEmpty() ||
                 snapshot.homeserver.isEmpty()) &&
                !payload.sessionUserId.isEmpty() && !payload.sessionDeviceId.isEmpty() &&
                !payload.sessionHomeserver.isEmpty()) {
                settings.setSessionSnapshot(
                  UserSettings::SessionSnapshot{.userId      = payload.sessionUserId,
                                                .accessToken = payload.accessToken,
                                                .deviceId    = payload.sessionDeviceId,
                                                .homeserver  = payload.sessionHomeserver});
            }
            break;
        }
        case staged_load_plan::Stage::State: {
            stateRoot = loadYamlFile(settings.stateFilePath_, "state");
            settings.loadStateYaml(stateRoot);
            break;
        }
        }
    }

    settings.applyTheme();
    syncCoreStoreFromPersistence(settings, effectiveConfig, stateRoot, sessionRoot);
    settings.setPersistenceScopeReadyForAuth(settings.hasActiveSession());
    // Keep persistence intentionally paused until the UI startup sequence completes.
    // This avoids incidental `save()` calls from initialization code paths.
    settings.setPersistenceSuspended(true);

    if (profile)
        emit settings.profileChanged(settings.profile_);
}

void
settings::SettingsController::save(UserSettings &settings, SavePolicy policy)
{
    if (settings.profileDirPath_.isEmpty()) {
        settings.profileDirPath_  = profileDirPath(settings.profile_);
        settings.configFilePath_  = configFilePathForProfile(settings.profile_);
        settings.stateFilePath_   = stateFilePathForProfile(settings.profile_);
        settings.sessionFilePath_ = sessionFilePathForProfile(settings.profile_);
        settings.secretsFilePath_ = secretsFilePathForProfile(settings.profile_);
        createDir(settings.profileDirPath_);
    }

    syncCoreStoreFromSettings(settings);

    settings.saveConfigYaml();
    syncConfigYamlFromCoreStore(settings.configFilePath_, settings.coreStore());

    if (policy == SavePolicy::Full) {
        settings.saveSessionYaml();
        settings.saveSecretsYaml();
        settings.saveStateYaml();
    }
    syncCoreStoreFromSettings(settings);
}

void
settings::SettingsController::clearAuth(UserSettings &settings)
{
    activeLoggers().ui->info("Clearing persisted session auth/identity for profile '{}'",
                             app_paths::normalizedProfileId(settings.profile_).toStdString());

    settings.accessToken_ = QString();
    settings.homeserver_  = QString();
    settings.userId_      = QString();
    settings.deviceId_    = QString();
    settings.secrets_.clear();

    if (pathExists(settings.sessionFilePath_) && !removePath(settings.sessionFilePath_)) {
        activeLoggers().ui->warn("Failed to remove session file '{}', keeping file to avoid "
                                 "accidental data loss",
                                 settings.sessionFilePath_.toStdString());
    }

    const auto provider = providerFromConfig(loadYamlFile(settings.configFilePath_, "config"));
    settings::persistence::clearProfileSecrets(settings.profile_,
                                               provider == staged_load_plan::SecretsProvider::File,
                                               settings.secretsFilePath_);
    settings.saveStateYaml();
    syncCoreStoreFromSettings(settings);
}

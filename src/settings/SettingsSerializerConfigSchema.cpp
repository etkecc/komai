// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerConfigSchema.h"

#include <array>
#include <mutex>
#include <string_view>
#include <type_traits>
#include <unordered_set>

#include <QString>

#include "SettingsSerializerConfigConverters.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::serializer::config {

namespace {

const std::array<BoolSettingDescriptor, 38> BoolSettings{
  BoolSettingDescriptor{SettingKey::IntegrationsSystemTrayEnabled,
                        false,
                        &UserSettings::integrationsSystemTrayEnabled,
                        &UserSettings::setIntegrationsSystemTrayEnabled},
  BoolSettingDescriptor{SettingKey::IntegrationsSystemTrayAutostart,
                        false,
                        &UserSettings::integrationsSystemTrayAutostart,
                        &UserSettings::setIntegrationsSystemTrayAutostart},
  BoolSettingDescriptor{SettingKey::NotificationsEnabled,
                        true,
                        &UserSettings::notificationsEnabled,
                        &UserSettings::setNotificationsEnabled},
  BoolSettingDescriptor{SettingKey::NotificationsAttentionOnIncoming,
                        false,
                        &UserSettings::notificationsAttentionOnIncoming,
                        &UserSettings::setNotificationsAttentionOnIncoming},
  BoolSettingDescriptor{SettingKey::SidebarsCommunitiesVisible,
                        true,
                        &UserSettings::sidebarsCommunitiesVisible,
                        &UserSettings::setSidebarsCommunitiesVisible},
  BoolSettingDescriptor{SettingKey::SidebarsRoomListScrollbarsEnabled,
                        true,
                        &UserSettings::sidebarsRoomListScrollbarsVisible,
                        &UserSettings::setSidebarsRoomListScrollbarsVisible},
  BoolSettingDescriptor{SettingKey::TimelineMessagesHoverHighlight,
                        false,
                        &UserSettings::timelineMessagesHoverHighlight,
                        &UserSettings::setTimelineMessagesHoverHighlight},
  BoolSettingDescriptor{SettingKey::TimelineMessagesEmojiOnlyEnlarge,
                        true,
                        &UserSettings::timelineMessagesEmojiOnlyEnlarge,
                        &UserSettings::setTimelineMessagesEmojiOnlyEnlarge},
  BoolSettingDescriptor{SettingKey::TimelineMediaAnimateOnHover,
                        false,
                        &UserSettings::timelineMediaAnimateOnHover,
                        &UserSettings::setTimelineMediaAnimateOnHover},
  BoolSettingDescriptor{SettingKey::ComposerTypingSendEnabled,
                        true,
                        &UserSettings::composerTypingSendEnabled,
                        &UserSettings::setComposerTypingSendEnabled},
  BoolSettingDescriptor{SettingKey::TimelineTypingShowEnabled,
                        true,
                        &UserSettings::timelineTypingShowEnabled,
                        &UserSettings::setTimelineTypingShowEnabled},
  BoolSettingDescriptor{SettingKey::TimelineReadReceiptsEnabled,
                        true,
                        &UserSettings::timelineReadReceiptsEnabled,
                        &UserSettings::setTimelineReadReceiptsEnabled},
  BoolSettingDescriptor{SettingKey::ComposerExtrasStickersEnabled,
                        false,
                        &UserSettings::stickersEnabled,
                        &UserSettings::setStickersEnabled},
  BoolSettingDescriptor{SettingKey::TimelineMediaEffectsEnabled,
                        true,
                        &UserSettings::timelineMediaEffectsEnabled,
                        &UserSettings::setTimelineMediaEffectsEnabled},
  BoolSettingDescriptor{SettingKey::UiAvatarsCircular,
                        false,
                        &UserSettings::circularAvatarsEnabled,
                        &UserSettings::setCircularAvatarsEnabled},
  BoolSettingDescriptor{SettingKey::UiAvatarsIdenticonFallback,
                        true,
                        &UserSettings::identiconFallbackEnabled,
                        &UserSettings::setIdenticonFallbackEnabled},
  BoolSettingDescriptor{SettingKey::SidebarsRoomListShowCommunityCounts,
                        true,
                        &UserSettings::sidebarsRoomListShowCommunityCounts,
                        &UserSettings::setSidebarsRoomListShowCommunityCounts},
  BoolSettingDescriptor{SettingKey::SidebarsRoomListCompact,
                        false,
                        &UserSettings::sidebarsRoomListCompact,
                        &UserSettings::setSidebarsRoomListCompact},
  BoolSettingDescriptor{SettingKey::SidebarsRoomListShowLastMessageTime,
                        true,
                        &UserSettings::sidebarsRoomListShowLastMessageTime,
                        &UserSettings::setSidebarsRoomListShowLastMessageTime},
  BoolSettingDescriptor{SettingKey::PrivacyWindowFocusBlurEnabled,
                        false,
                        &UserSettings::windowFocusBlurEnabled,
                        &UserSettings::setWindowFocusBlurEnabled},
  BoolSettingDescriptor{SettingKey::EncryptionKeySharingOnlyVerifiedUsers,
                        false,
                        &UserSettings::encryptionKeySharingOnlyVerifiedUsers,
                        &UserSettings::setEncryptionKeySharingOnlyVerifiedUsers},
  BoolSettingDescriptor{SettingKey::EncryptionKeySharingShareWithTrusted,
                        false,
                        &UserSettings::encryptionKeySharingShareWithTrusted,
                        &UserSettings::setEncryptionKeySharingShareWithTrusted},
  BoolSettingDescriptor{SettingKey::EncryptionBackupOnlineEnabled,
                        true,
                        &UserSettings::encryptionBackupOnlineEnabled,
                        &UserSettings::setEncryptionBackupOnlineEnabledFromConfig},
  BoolSettingDescriptor{SettingKey::NetworkTlsEnableCertificateValidation,
                        kDefaultCertificateValidationEnabled,
                        &UserSettings::certificateValidationEnabled,
                        &UserSettings::setCertificateValidationEnabled},
  BoolSettingDescriptor{SettingKey::NetworkHttp3Enabled,
                        kDefaultNetworkHttp3Enabled,
                        &UserSettings::http3Enabled,
                        &UserSettings::setHttp3Enabled},
  BoolSettingDescriptor{SettingKey::UiInputTouchSwipeGesturesEnabled,
                        false,
                        &UserSettings::uiInputTouchSwipeGesturesEnabled,
                        &UserSettings::setUiInputTouchSwipeGesturesEnabled},
  BoolSettingDescriptor{SettingKey::TimelineMessagesLayoutSmallAvatars,
                        false,
                        &UserSettings::timelineSmallAvatarsEnabled,
                        &UserSettings::setTimelineSmallAvatarsEnabled},
  BoolSettingDescriptor{SettingKey::TimelineMessagesLayoutShowOwnAvatar,
                        true,
                        &UserSettings::timelineShowOwnAvatarInBubbleLayout,
                        &UserSettings::setTimelineShowOwnAvatarInBubbleLayout},
  BoolSettingDescriptor{SettingKey::CallsLegacyEnabled,
                        false,
                        &UserSettings::callsLegacyEnabled,
                        &UserSettings::setCallsLegacyEnabled},
  BoolSettingDescriptor{SettingKey::CallsScreensharePictureInPicture,
                        true,
                        &UserSettings::screenSharePiP,
                        &UserSettings::setScreenSharePiP},
  BoolSettingDescriptor{SettingKey::CallsScreenshareIncludeRemoteVideo,
                        false,
                        &UserSettings::screenShareRemoteVideo,
                        &UserSettings::setScreenShareRemoteVideo},
  BoolSettingDescriptor{SettingKey::CallsScreenshareShowCursor,
                        settings::core::definitions::kDefaultScreenShareShowCursor,
                        &UserSettings::screenShareShowCursor,
                        &UserSettings::setScreenShareShowCursor},
  BoolSettingDescriptor{SettingKey::CallsRelayUseFallbackServer,
                        false,
                        &UserSettings::callsRelayUseFallbackServer,
                        &UserSettings::setCallsRelayUseFallbackServer},
  BoolSettingDescriptor{SettingKey::ComposerInputMarkdownEnabled,
                        true,
                        &UserSettings::markdownEnabled,
                        &UserSettings::setMarkdownEnabled},
  BoolSettingDescriptor{SettingKey::TimelineMediaOpenImagesExternal,
                        false,
                        &UserSettings::openImagesInExternalApp,
                        &UserSettings::setOpenImagesInExternalApp},
  BoolSettingDescriptor{SettingKey::TimelineMediaOpenVideosExternal,
                        false,
                        &UserSettings::openVideosInExternalApp,
                        &UserSettings::setOpenVideosInExternalApp},
  BoolSettingDescriptor{SettingKey::PrivacyMaintenanceUpdateSpaceVias,
                        true,
                        &UserSettings::updateSpaceVias,
                        &UserSettings::setUpdateSpaceVias},
  BoolSettingDescriptor{SettingKey::PrivacyMaintenanceExpireEvents,
                        false,
                        &UserSettings::expireEvents,
                        &UserSettings::setExpireEvents},
};

const std::array<IntSettingDescriptor, 4> IntSettings{
  IntSettingDescriptor{SettingKey::UiLayoutContentMaxWidthPx,
                       kDefaultUiLayoutContentMaxWidthPx,
                       &UserSettings::maxContentWidth,
                       &UserSettings::setMaxContentWidth},
  IntSettingDescriptor{SettingKey::TimelineMessagesMaxWidthPx,
                       kDefaultTimelineMaxWidthPx,
                       &UserSettings::maxTimelineWidth,
                       &UserSettings::setMaxTimelineWidth},
  IntSettingDescriptor{SettingKey::PrivacyWindowFocusBlurDelaySeconds,
                       kDefaultPrivacyWindowFocusBlurDelaySeconds,
                       &UserSettings::windowFocusBlurDelaySeconds,
                       &UserSettings::setWindowFocusBlurDelaySeconds},
  IntSettingDescriptor{SettingKey::CallsScreenshareFrameRate,
                       kDefaultScreenShareFrameRate,
                       &UserSettings::screenShareFrameRate,
                       &UserSettings::setScreenShareFrameRate},
};

const std::array<UintSettingDescriptor, 1> UintSettings{
  UintSettingDescriptor{SettingKey::DbMaxStores,
                        kDefaultMaxStores,
                        &UserSettings::maxStores,
                        &UserSettings::setMaxStores},
};

const std::array<ULongLongSettingDescriptor, 1> ULongLongSettings{
  ULongLongSettingDescriptor{SettingKey::DbMaxSizeBytes,
                             kDefaultMaxDbSizeBytes,
                             &UserSettings::maxDbSize,
                             &UserSettings::setMaxDbSize},
};

const std::array<DoubleSettingDescriptor, 1> DoubleSettings{
  DoubleSettingDescriptor{SettingKey::UiFontSizePt,
                          kDefaultFontSizePt,
                          &UserSettings::fontSize,
                          &UserSettings::setFontSize},
};

const std::array<StringSettingDescriptor, 9> StringSettings{
  StringSettingDescriptor{SettingKey::UiFontFamily,
                          QString(),
                          &UserSettings::font,
                          &UserSettings::setFontFamily},
  StringSettingDescriptor{SettingKey::UiFontEmojiFamily,
                          QString(),
                          &UserSettings::emojiFontFamily,
                          &UserSettings::setEmojiFontFamily},
  StringSettingDescriptor{SettingKey::TimelineMessageActionsPinnedReactions,
                          QString::fromUtf8(settings::core::definitions::kDefaultPinnedReactions),
                          &UserSettings::pinnedReactions,
                          &UserSettings::setPinnedReactions},
  StringSettingDescriptor{
    SettingKey::CallsAudioRingtone,
    QString::fromLatin1(settings::core::definitions::kDefaultCallsAudioRingtone),
    &UserSettings::ringtone,
    &UserSettings::setRingtone},
  StringSettingDescriptor{SettingKey::CallsDevicesMicrophone,
                          QString(),
                          &UserSettings::microphone,
                          &UserSettings::setMicrophone},
  StringSettingDescriptor{SettingKey::CallsDevicesCamera,
                          QString(),
                          &UserSettings::camera,
                          &UserSettings::setCamera},
  StringSettingDescriptor{SettingKey::CallsDevicesCameraResolution,
                          QString(),
                          &UserSettings::cameraResolution,
                          &UserSettings::setCameraResolution},
  StringSettingDescriptor{SettingKey::CallsDevicesCameraFrameRate,
                          QString(),
                          &UserSettings::cameraFrameRate,
                          &UserSettings::setCameraFrameRate},
  StringSettingDescriptor{SettingKey::IntegrationsBrowserCommand,
                          QString(),
                          &UserSettings::integrationsLinksBrowserCommand,
                          &UserSettings::setIntegrationsLinksBrowserCommand},
};

template<typename DescriptorT>
void
registerDescriptorKeys(std::string_view descriptorSetName,
                       std::span<const DescriptorT> descriptors,
                       std::unordered_set<std::string_view> &allTypedKeys)
{
    for (const auto &descriptor : descriptors) {
        Q_ASSERT_X(descriptor.key != nullptr && descriptor.key[0] != '\0',
                   "settings::serializer::config::validateConfigSchemaDescriptors",
                   "config descriptor key must not be empty");

        const std::string_view key{descriptor.key};
        const bool inserted = allTypedKeys.insert(key).second;
        Q_ASSERT_X(inserted,
                   "settings::serializer::config::validateConfigSchemaDescriptors",
                   descriptorSetName.data());
    }
}

bool
isSchemaOnlyConfigKey(std::string_view key)
{
    return key == std::string_view{SettingKey::DbMaxStores} ||
           key == std::string_view{SettingKey::DbMaxSizeBytes};
}

bool
hasPersistedConfigDefinition(std::string_view key)
{
    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (definition.scope != settings::core::SettingScope::Config ||
            definition.persistedKey == nullptr)
            continue;

        if (key == std::string_view{definition.persistedKey})
            return true;
    }

    return false;
}

struct SettingIdHash
{
    std::size_t operator()(settings::core::SettingId id) const
    {
        using Underlying = std::underlying_type_t<settings::core::SettingId>;
        return static_cast<std::size_t>(static_cast<Underlying>(id));
    }
};

} // namespace

std::span<const BoolSettingDescriptor>
boolConfigSettings()
{
    return BoolSettings;
}

std::span<const IntSettingDescriptor>
intConfigSettings()
{
    return IntSettings;
}

std::span<const UintSettingDescriptor>
uintConfigSettings()
{
    return UintSettings;
}

std::span<const ULongLongSettingDescriptor>
ulonglongConfigSettings()
{
    return ULongLongSettings;
}

std::span<const DoubleSettingDescriptor>
doubleConfigSettings()
{
    return DoubleSettings;
}

std::span<const StringSettingDescriptor>
stringConfigSettings()
{
    return StringSettings;
}

void
validateConfigSchemaDescriptors()
{
    static std::once_flag validationOnce;
    std::call_once(validationOnce, []() {
        std::unordered_set<std::string_view> typedKeys;
        typedKeys.reserve(boolConfigSettings().size() + intConfigSettings().size() +
                          uintConfigSettings().size() + ulonglongConfigSettings().size() +
                          doubleConfigSettings().size() + stringConfigSettings().size());

        registerDescriptorKeys(
          "duplicate key in boolConfigSettings", boolConfigSettings(), typedKeys);
        registerDescriptorKeys(
          "duplicate key in intConfigSettings", intConfigSettings(), typedKeys);
        registerDescriptorKeys(
          "duplicate key in uintConfigSettings", uintConfigSettings(), typedKeys);
        registerDescriptorKeys(
          "duplicate key in ulonglongConfigSettings", ulonglongConfigSettings(), typedKeys);
        registerDescriptorKeys(
          "duplicate key in doubleConfigSettings", doubleConfigSettings(), typedKeys);
        registerDescriptorKeys(
          "duplicate key in stringConfigSettings", stringConfigSettings(), typedKeys);

        for (const auto key : typedKeys) {
            Q_ASSERT_X(hasPersistedConfigDefinition(key) || isSchemaOnlyConfigKey(key),
                       "settings::serializer::config::validateConfigSchemaDescriptors",
                       "typed config descriptor key missing persisted definition");
        }

        std::unordered_set<settings::core::SettingId, SettingIdHash> enumAdapterIds;
        std::unordered_set<std::string_view> enumAdapterKeys;
        enumAdapterIds.reserve(enumTokenAdapters().size());
        enumAdapterKeys.reserve(enumTokenAdapters().size());

        for (const auto &adapter : enumTokenAdapters()) {
            Q_ASSERT_X(adapter.key != nullptr && adapter.key[0] != '\0',
                       "settings::serializer::config::validateConfigSchemaDescriptors",
                       "enum token adapter key must not be empty");
            Q_ASSERT_X(adapter.defaultToken != nullptr && adapter.defaultToken[0] != '\0',
                       "settings::serializer::config::validateConfigSchemaDescriptors",
                       "enum token adapter default token must not be empty");

            const bool idInserted  = enumAdapterIds.insert(adapter.id).second;
            const bool keyInserted = enumAdapterKeys.insert(std::string_view{adapter.key}).second;
            Q_ASSERT_X(idInserted,
                       "settings::serializer::config::validateConfigSchemaDescriptors",
                       "duplicate enum token adapter SettingId");
            Q_ASSERT_X(keyInserted,
                       "settings::serializer::config::validateConfigSchemaDescriptors",
                       "duplicate enum token adapter key");
            Q_ASSERT_X(typedKeys.find(std::string_view{adapter.key}) == typedKeys.end(),
                       "settings::serializer::config::validateConfigSchemaDescriptors",
                       "enum token adapter key overlaps typed config descriptor key");
            Q_ASSERT_X(hasPersistedConfigDefinition(std::string_view{adapter.key}),
                       "settings::serializer::config::validateConfigSchemaDescriptors",
                       "enum token adapter key missing persisted definition");
        }
    });
}

} // namespace settings::serializer::config

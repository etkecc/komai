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
#include "SettingsSerializerConfigSchemaBoolCalls.inc"
#include "SettingsSerializerConfigSchemaBoolComposer.inc"
#include "SettingsSerializerConfigSchemaBoolEncryption.inc"
#include "SettingsSerializerConfigSchemaBoolIntegrations.inc"
#include "SettingsSerializerConfigSchemaBoolLookFeel.inc"
#include "SettingsSerializerConfigSchemaBoolNetwork.inc"
#include "SettingsSerializerConfigSchemaBoolNotifications.inc"
#include "SettingsSerializerConfigSchemaBoolPrivacy.inc"
#include "SettingsSerializerConfigSchemaBoolSidebars.inc"
#include "SettingsSerializerConfigSchemaBoolTimeline.inc"
};

const std::array<IntSettingDescriptor, 4> IntSettings{
#include "SettingsSerializerConfigSchemaIntCalls.inc"
#include "SettingsSerializerConfigSchemaIntLookFeel.inc"
#include "SettingsSerializerConfigSchemaIntPrivacy.inc"
#include "SettingsSerializerConfigSchemaIntTimeline.inc"
};

const std::array<UintSettingDescriptor, 1> UintSettings{
  UintSettingDescriptor{SettingKey::DbMaxStores,
                        kDefaultMaxStores,
                        &UserSettings::dbMaxStores,
                        &UserSettings::setDbMaxStores},
};

const std::array<ULongLongSettingDescriptor, 1> ULongLongSettings{
  ULongLongSettingDescriptor{SettingKey::DbMaxSizeBytes,
                             kDefaultMaxDbSizeBytes,
                             &UserSettings::dbMaxSizeBytes,
                             &UserSettings::setDbMaxSizeBytes},
};

const std::array<DoubleSettingDescriptor, 1> DoubleSettings{
  DoubleSettingDescriptor{SettingKey::UiFontSizePt,
                          kDefaultFontSizePt,
                          &UserSettings::uiFontSizePt,
                          &UserSettings::setUiFontSizePt},
};

const std::array<StringSettingDescriptor, 9> StringSettings{
#include "SettingsSerializerConfigSchemaStringCalls.inc"
#include "SettingsSerializerConfigSchemaStringIntegrations.inc"
#include "SettingsSerializerConfigSchemaStringLookFeel.inc"
#include "SettingsSerializerConfigSchemaStringTimeline.inc"
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

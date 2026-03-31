// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerConfigSchema.h"

#include <mutex>
#include <string_view>
#include <type_traits>
#include <unordered_set>

#include "SettingsSerializerConfigConverters.h"
#include "settings/core/SettingsDefinitions.h"

namespace settings::serializer::config {

namespace {

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
    Q_UNUSED(key);
    return false;
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

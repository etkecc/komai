// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerConfigConverters.h"

#include "settings/SettingKeys.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::serializer::config {

namespace {

#include "SettingsSerializerConfigEnumTokenAdaptersFnsComposer.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersFnsIntegrations.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersFnsLookFeel.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersFnsNetwork.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersFnsNotifications.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersFnsSidebars.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersFnsTimeline.inc"

constexpr EnumTokenAdapter kEnumTokenAdapters[] = {
#include "SettingsSerializerConfigEnumTokenAdaptersComposer.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersIntegrations.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersLookFeel.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersNetwork.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersNotifications.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersSidebars.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersTimeline.inc"
};

constexpr bool
hasUniqueEnumTokenAdapterIds()
{
    for (std::size_t i = 0; i < std::size(kEnumTokenAdapters); ++i) {
        for (std::size_t j = i + 1; j < std::size(kEnumTokenAdapters); ++j) {
            if (kEnumTokenAdapters[i].id == kEnumTokenAdapters[j].id)
                return false;
        }
    }

    return true;
}

constexpr bool
hasCompleteEnumTokenAdapterCoverage()
{
    for (const auto id : settings::core::definitions::enumTokenConfigSettingIds()) {
        bool found = false;
        for (const auto &adapter : kEnumTokenAdapters) {
            if (adapter.id == id) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }

    for (const auto &adapter : kEnumTokenAdapters) {
        if (!settings::core::definitions::isEnumTokenConfigSettingId(adapter.id))
            return false;
    }

    return true;
}

static_assert(hasUniqueEnumTokenAdapterIds(),
              "enum token adapters must not contain duplicate SettingIds");
static_assert(hasCompleteEnumTokenAdapterCoverage(),
              "enum token adapters must match core enum-token setting definitions");

} // namespace

std::span<const EnumTokenAdapter>
enumTokenAdapters()
{
    return kEnumTokenAdapters;
}

} // namespace settings::serializer::config

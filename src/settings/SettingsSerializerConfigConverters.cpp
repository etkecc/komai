// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerConfigConverters.h"

#include <array>

#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::serializer::config {

namespace {

template<typename ValueT, std::size_t N>
QString
valueToStorageToken(ValueT value,
                    const std::array<std::pair<ValueT, const char *>, N> &tokenMap,
                    const char *fallbackToken)
{
    for (const auto &[candidate, token] : tokenMap) {
        if (candidate == value)
            return QString::fromLatin1(token);
    }

    return QString::fromLatin1(fallbackToken);
}

template<typename ValueT, std::size_t N>
ValueT
valueFromStorageToken(const QString &value,
                      ValueT fallback,
                      const std::array<std::pair<ValueT, const char *>, N> &tokenMap)
{
    for (const auto &[candidate, token] : tokenMap) {
        if (value == QLatin1String(token))
            return candidate;
    }

    return fallback;
}
} // namespace

#include "SettingsSerializerConfigConvertersCalls.inc"
#include "SettingsSerializerConfigConvertersComposer.inc"
#include "SettingsSerializerConfigConvertersIntegrations.inc"
#include "SettingsSerializerConfigConvertersLookFeel.inc"
#include "SettingsSerializerConfigConvertersNavigation.inc"
#include "SettingsSerializerConfigConvertersNetwork.inc"
#include "SettingsSerializerConfigConvertersNotifications.inc"
#include "SettingsSerializerConfigConvertersTimeline.inc"

} // namespace settings::serializer::config

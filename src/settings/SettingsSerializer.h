// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

class QString;
class UserSettings;
namespace komai::logging {
class Logger;
}
namespace komai::rust {
struct SettingsProfileHandle;
}

namespace settings::serializer {

struct SerializerLoggers
{
    std::shared_ptr<komai::logging::Logger> ui;
};

void
setLoggers(SerializerLoggers loggers);
const SerializerLoggers &
activeLoggers();

/**
 * YAML serialization/deserialization helpers for UserSettings persistence.
 *
 * This module owns enum/string normalization and read/write mapping for the
 * settings model. It remains a pure settings utility with no QML lifecycle
 * responsibilities.
 */
void
stageConfig(const UserSettings &settings,
            bool usesFileSecretsProvider,
            ::komai::rust::SettingsProfileHandle &profileHandle);
void
stageSession(const UserSettings &settings, ::komai::rust::SettingsProfileHandle &profileHandle);
void
stageState(const UserSettings &settings, ::komai::rust::SettingsProfileHandle &profileHandle);

} // namespace settings::serializer

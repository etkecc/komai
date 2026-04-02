// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

class QString;
class UserSettings;
namespace nhlog {
class Logger;
}
namespace komai::rust {
struct SettingsProfileHandle;
}

namespace settings::serializer {

struct SerializerLoggers
{
    std::shared_ptr<nhlog::Logger> ui;
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
saveConfig(const UserSettings &settings,
           const QString &configFilePath,
           bool usesFileSecretsProvider,
           ::komai::rust::SettingsProfileHandle *profileHandle = nullptr);
void
saveSession(const UserSettings &settings,
            const QString &sessionFilePath,
            ::komai::rust::SettingsProfileHandle *profileHandle = nullptr);
void
saveState(const UserSettings &settings,
          const QString &stateFilePath,
          ::komai::rust::SettingsProfileHandle *profileHandle = nullptr);

} // namespace settings::serializer

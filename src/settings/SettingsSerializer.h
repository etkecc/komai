// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace YAML {
class Node;
}

class QString;
class UserSettings;

namespace settings::serializer {

/**
 * YAML serialization/deserialization helpers for UserSettings persistence.
 *
 * This module owns enum/string normalization and read/write mapping for the
 * settings model. It remains a pure settings utility with no QML lifecycle
 * responsibilities.
 */
void loadConfig(UserSettings &settings, const YAML::Node &root);
void loadSession(UserSettings &settings, const YAML::Node &root);
void loadState(UserSettings &settings, const YAML::Node &root);

void saveConfig(const UserSettings &settings, const QString &configFilePath);
void saveSession(const UserSettings &settings, const QString &sessionFilePath);
void saveState(const UserSettings &settings, const QString &stateFilePath);

} // namespace settings::serializer


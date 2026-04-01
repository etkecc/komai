// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace YAML {
class Node;
}

class UserSettings;

namespace settings::serializer {

/**
 * YAML-backed load helpers used by the staged settings controller.
 *
 * These remain separate from the broader serializer facade so raw YAML types
 * do not leak into unrelated startup/application code.
 */
void
loadConfig(UserSettings &settings, const YAML::Node &root);
void
loadSession(UserSettings &settings, const YAML::Node &root);
void
loadState(UserSettings &settings, const YAML::Node &root);

} // namespace settings::serializer

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <yaml-cpp/yaml.h>

namespace settings::storage {

YAML::Node
loadYamlFile(const QString &path, const char *label);

bool
writeYamlFile(const QString &path, const YAML::Node &root, bool ownerReadWriteOnly);

} // namespace settings::storage

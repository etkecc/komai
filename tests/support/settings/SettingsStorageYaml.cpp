// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/SettingsStorageYaml.h"

#include "logging/Logging.h"
#include "settings/SettingsStorage.h"

namespace settings::storage {

YAML::Node
loadYamlFile(const QString &path, const char *label)
{
    const char *safeLabel = label ? label : "settings";
    const auto text       = readTextFile(path, safeLabel);

    if (text.isEmpty() && !pathExists(path))
        return YAML::Node(YAML::NodeType::Map);

    try {
        const auto root = YAML::Load(text.toStdString());
        return root.IsMap() ? root : YAML::Node(YAML::NodeType::Map);
    } catch (const YAML::Exception &e) {
        activeLoggers().ui->error(
          "Failed to parse {} file {}: {}", safeLabel, path.toStdString(), e.what());
        return YAML::Node(YAML::NodeType::Map);
    }
}

bool
writeYamlFile(const QString &path, const YAML::Node &root, bool ownerReadWriteOnly)
{
    YAML::Emitter out;
    out.SetIndent(2);
    out << (root && root.IsMap() ? root : YAML::Node(YAML::NodeType::Map));
    return writeTextFile(path, QString::fromUtf8(out.c_str()), ownerReadWriteOnly);
}

} // namespace settings::storage

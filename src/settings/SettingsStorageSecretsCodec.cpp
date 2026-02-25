// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsStorage.h"

#include <string>

namespace settings::storage {

QString
encodeSecretsMap(const QMap<QString, QString> &secrets)
{
    YAML::Node root(YAML::NodeType::Map);
    for (auto it = secrets.constBegin(); it != secrets.constEnd(); ++it)
        root[it.key().toStdString()] = it.value().toStdString();

    YAML::Emitter out;
    out << root;
    return QString::fromStdString(out.c_str());
}

QMap<QString, QString>
decodeSecretsMap(const QString &serialized)
{
    if (serialized.trimmed().isEmpty())
        return {};

    try {
        YAML::Node root = YAML::Load(serialized.toStdString());
        if (!root.IsMap())
            return {};

        QMap<QString, QString> result;
        for (const auto &item : root) {
            if (!item.first.IsScalar() || !item.second.IsScalar())
                continue;
            result[QString::fromStdString(item.first.as<std::string>())] =
              QString::fromStdString(item.second.as<std::string>());
        }
        return result;
    } catch (const YAML::Exception &) {
        return {};
    }
}

} // namespace settings::storage

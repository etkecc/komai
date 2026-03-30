// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include <functional>

#include <yaml-cpp/yaml.h>

namespace yaml_settings {

inline QStringList
splitDottedKey(const char *key)
{
    return QString::fromLatin1(key).split(u'.', Qt::SkipEmptyParts);
}

inline YAML::Node
getNode(const YAML::Node &root, const char *dottedKey)
{
    auto parts = splitDottedKey(dottedKey);
    QList<YAML::Node> chain;
    chain.reserve(parts.size() + 1);
    chain.push_back(root);

    for (const auto &part : parts) {
        const auto &node = chain.back();
        if (!node || !node.IsMap())
            return YAML::Node();
        const auto child = node[part.toStdString()];
        if (!child)
            return YAML::Node();
        chain.push_back(child);
    }

    return chain.back();
}

template<typename T>
inline void
setNode(YAML::Node &root, const char *dottedKey, const T &value)
{
    auto parts = splitDottedKey(dottedKey);
    if (parts.isEmpty())
        return;

    std::function<void(YAML::Node, int)> writeRecursive = [&](YAML::Node node, int index) {
        const auto key = parts.at(index).toStdString();

        if (index == parts.size() - 1) {
            node[key] = value;
            return;
        }

        if (!node[key] || !node[key].IsMap())
            node[key] = YAML::Node(YAML::NodeType::Map);

        writeRecursive(node[key], index + 1);
    };

    writeRecursive(root, 0);
}

template<typename T>
inline T
readScalar(const YAML::Node &root, const char *dottedKey, const T &defaultValue)
{
    try {
        const auto node = getNode(root, dottedKey);
        if (!node || !node.IsScalar())
            return defaultValue;
        return node.as<T>();
    } catch (const YAML::Exception &) {
        return defaultValue;
    }
}

inline QString
readString(const YAML::Node &root, const char *dottedKey, const QString &defaultValue)
{
    return QString::fromStdString(
      readScalar<std::string>(root, dottedKey, defaultValue.toStdString()));
}

inline QStringList
readStringList(const YAML::Node &root, const char *dottedKey, const QStringList &defaultValue = {})
{
    const auto node = getNode(root, dottedKey);
    if (!node || !node.IsSequence())
        return defaultValue;

    QStringList result;
    for (const auto &item : node) {
        if (item.IsScalar()) {
            try {
                result.append(QString::fromStdString(item.as<std::string>()));
            } catch (const YAML::Exception &) {
            }
        }
    }
    return result;
}

inline QList<QStringList>
readNestedStringLists(const YAML::Node &root,
                      const char *dottedKey,
                      const QList<QStringList> &defaultValue = {})
{
    const auto node = getNode(root, dottedKey);
    if (!node || !node.IsSequence())
        return defaultValue;

    QList<QStringList> result;
    for (const auto &entry : node) {
        if (!entry.IsSequence())
            continue;
        QStringList list;
        for (const auto &item : entry) {
            if (item.IsScalar()) {
                try {
                    list.append(QString::fromStdString(item.as<std::string>()));
                } catch (const YAML::Exception &) {
                }
            }
        }
        result.push_back(list);
    }
    return result;
}

inline QMap<QString, QString>
readStringMap(const YAML::Node &root, const char *dottedKey)
{
    const auto node = getNode(root, dottedKey);
    if (!node || !node.IsMap())
        return {};

    QMap<QString, QString> result;
    for (const auto &item : node) {
        if (!item.first.IsScalar() || !item.second.IsScalar())
            continue;
        try {
            result[QString::fromStdString(item.first.as<std::string>())] =
              QString::fromStdString(item.second.as<std::string>());
        } catch (const YAML::Exception &) {
        }
    }
    return result;
}

inline QMap<QString, QStringList>
readStringListMap(const YAML::Node &root, const char *dottedKey)
{
    const auto node = getNode(root, dottedKey);
    if (!node || !node.IsMap())
        return {};

    QMap<QString, QStringList> result;
    for (const auto &item : node) {
        if (!item.first.IsScalar() || !item.second.IsSequence())
            continue;

        QStringList values;
        for (const auto &entry : item.second) {
            if (!entry.IsScalar())
                continue;
            try {
                values.push_back(QString::fromStdString(entry.as<std::string>()));
            } catch (const YAML::Exception &) {
            }
        }

        try {
            result.insert(QString::fromStdString(item.first.as<std::string>()), values);
        } catch (const YAML::Exception &) {
        }
    }

    return result;
}

inline void
writeStringList(YAML::Node &root, const char *dottedKey, const QStringList &value)
{
    YAML::Node list(YAML::NodeType::Sequence);
    for (const auto &entry : value)
        list.push_back(entry.toStdString());
    setNode(root, dottedKey, list);
}

inline void
writeNestedStringLists(YAML::Node &root, const char *dottedKey, const QList<QStringList> &value)
{
    YAML::Node outer(YAML::NodeType::Sequence);
    for (const auto &entry : value) {
        YAML::Node inner(YAML::NodeType::Sequence);
        for (const auto &item : entry)
            inner.push_back(item.toStdString());
        outer.push_back(inner);
    }
    setNode(root, dottedKey, outer);
}

inline void
writeStringMap(YAML::Node &root, const char *dottedKey, const QMap<QString, QString> &value)
{
    YAML::Node map(YAML::NodeType::Map);
    for (auto it = value.constBegin(); it != value.constEnd(); ++it)
        map[it.key().toStdString()] = it.value().toStdString();
    setNode(root, dottedKey, map);
}

inline void
writeStringListMap(YAML::Node &root, const char *dottedKey, const QMap<QString, QStringList> &value)
{
    YAML::Node map(YAML::NodeType::Map);
    for (auto it = value.constBegin(); it != value.constEnd(); ++it) {
        YAML::Node list(YAML::NodeType::Sequence);
        for (const auto &entry : it.value())
            list.push_back(entry.toStdString());
        map[it.key().toStdString()] = list;
    }
    setNode(root, dottedKey, map);
}

} // namespace yaml_settings

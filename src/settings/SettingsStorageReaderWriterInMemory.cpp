// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsStorage.h"
#include "SettingsStorageInternal.h"

#include <QHash>
#include <QString>

#include "logging/Logging.h"

#include "profile/Paths.h"

namespace settings::storage::detail {

namespace {

class InMemoryReaderWriter final : public ReaderWriter
{
public:
    explicit InMemoryReaderWriter(QStringView baseDir)
      : baseDir_(QString::fromUtf8(baseDir.toUtf8()))
    {
    }

    QString profileDirPath(const QString &profile) const override
    {
        return profileBasePath(profile);
    }

    QString configFilePathForProfile(const QString &profile) const override
    {
        return profileBasePath(profile) + QStringLiteral("/config.yml");
    }

    QString stateFilePathForProfile(const QString &profile) const override
    {
        return profileBasePath(profile) + QStringLiteral("/state.yml");
    }

    QString sessionFilePathForProfile(const QString &profile) const override
    {
        return profileBasePath(profile) + QStringLiteral("/session.yml");
    }

    QString secretsFilePathForProfile(const QString &profile) const override
    {
        return profileBasePath(profile) + QStringLiteral("/secrets.yml");
    }

    QString readTextFile(const QString &path, const char *label) const override
    {
        const auto textIt = texts_.find(path);
        if (textIt != texts_.end())
            return textIt.value();

        const auto it = nodes_.find(path);
        if (it == nodes_.end()) {
            const char *safeLabel = label ? label : "settings";
            activeLoggers().ui->info(
              "{} file does not exist, using defaults: {}", safeLabel, path.toStdString());
            return {};
        }

        YAML::Emitter out;
        out.SetIndent(2);
        out << it.value();
        return QString::fromUtf8(out.c_str());
    }

    bool writeTextFile(const QString &path, const QString &content, bool) const override
    {
        texts_[path] = content;
        try {
            nodes_[path] = YAML::Load(content.toStdString());
        } catch (const YAML::Exception &) {
            nodes_[path] = YAML::Node(YAML::NodeType::Map);
        }
        return true;
    }

    YAML::Node loadYamlFile(const QString &path, const char *label) const override
    {
        const auto textIt = texts_.find(path);
        if (textIt != texts_.end()) {
            try {
                const auto root = YAML::Load(textIt.value().toStdString());
                return root.IsMap() ? root : YAML::Node(YAML::NodeType::Map);
            } catch (const YAML::Exception &) {
                return YAML::Node(YAML::NodeType::Map);
            }
        }

        const auto it = nodes_.find(path);
        if (it == nodes_.end()) {
            const char *safeLabel = label ? label : "settings";
            activeLoggers().ui->info(
              "{} file does not exist, using defaults: {}", safeLabel, path.toStdString());
            return YAML::Node(YAML::NodeType::Map);
        }
        return it.value();
    }

    bool writeYamlFile(const QString &path, const YAML::Node &root, bool) const override
    {
        nodes_[path] = root;
        YAML::Emitter out;
        out.SetIndent(2);
        out << root;
        texts_[path] = QString::fromUtf8(out.c_str());
        return true;
    }

    bool pathExists(const QString &path) const override
    {
        return nodes_.contains(path) || texts_.contains(path);
    }

    bool createDir(const QString &path) const override
    {
        Q_UNUSED(path)
        return true;
    }

    bool removePath(const QString &path) const override
    {
        const bool removedNode = nodes_.remove(path) > 0;
        const bool removedText = texts_.remove(path) > 0;
        return removedNode || removedText;
    }

private:
    QString profileBasePath(const QString &profile) const
    {
        return baseDir_ + QStringLiteral("/profiles/") + app_paths::normalizedProfileId(profile);
    }

    QString baseDir_;
    mutable QHash<QString, YAML::Node> nodes_;
    mutable QHash<QString, QString> texts_;
};

} // namespace

ReaderWriterPtr
makeInMemoryReaderWriter(QStringView baseDir)
{
    return std::make_shared<InMemoryReaderWriter>(baseDir);
}

} // namespace settings::storage::detail

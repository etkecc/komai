// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include <QByteArray>
#include <QDir>
#include <QTemporaryDir>

namespace test_env {

class EnvVarOverride
{
public:
    EnvVarOverride(const char *name, const QByteArray &value)
      : name_{name}
      , original_{qgetenv(name)}
      , hadOriginal_{qEnvironmentVariableIsSet(name)}
    {
        qputenv(name_, value);
    }

    ~EnvVarOverride()
    {
        if (hadOriginal_)
            qputenv(name_, original_);
        else
            qunsetenv(name_);
    }

    EnvVarOverride(const EnvVarOverride &)            = delete;
    EnvVarOverride &operator=(const EnvVarOverride &) = delete;

private:
    const char *name_;
    QByteArray original_;
    bool hadOriginal_;
};

class ScopedTestHome
{
public:
    explicit ScopedTestHome(const QString &prefix)
      : baseDir_{QStringLiteral("/tmp/%1-XXXXXX").arg(prefix)}
    {
        if (!baseDir_.isValid())
            return;

        const QString rootPath = baseDir_.path();
        const QString homePath = rootPath + QStringLiteral("/home");
        const QString configPath = rootPath + QStringLiteral("/config");
        const QString statePath = rootPath + QStringLiteral("/state");
        const QString dataPath = rootPath + QStringLiteral("/data");
        const QString cachePath = rootPath + QStringLiteral("/cache");

        QDir().mkpath(homePath);
        QDir().mkpath(configPath);
        QDir().mkpath(statePath);
        QDir().mkpath(dataPath);
        QDir().mkpath(cachePath);

        homeOverride_ = std::make_unique<EnvVarOverride>("HOME", homePath.toUtf8());
        xdgConfigOverride_ =
          std::make_unique<EnvVarOverride>("XDG_CONFIG_HOME", configPath.toUtf8());
        xdgStateOverride_ =
          std::make_unique<EnvVarOverride>("XDG_STATE_HOME", statePath.toUtf8());
        xdgDataOverride_ = std::make_unique<EnvVarOverride>("XDG_DATA_HOME", dataPath.toUtf8());
        xdgCacheOverride_ =
          std::make_unique<EnvVarOverride>("XDG_CACHE_HOME", cachePath.toUtf8());
    }

    bool isValid() const { return baseDir_.isValid(); }

    QString rootPath() const { return baseDir_.path(); }

    ScopedTestHome(const ScopedTestHome &)            = delete;
    ScopedTestHome &operator=(const ScopedTestHome &) = delete;

private:
    QTemporaryDir baseDir_;
    std::unique_ptr<EnvVarOverride> homeOverride_;
    std::unique_ptr<EnvVarOverride> xdgConfigOverride_;
    std::unique_ptr<EnvVarOverride> xdgStateOverride_;
    std::unique_ptr<EnvVarOverride> xdgDataOverride_;
    std::unique_ptr<EnvVarOverride> xdgCacheOverride_;
};

} // namespace test_env

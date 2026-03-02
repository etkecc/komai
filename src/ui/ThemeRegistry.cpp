// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThemeRegistry.h"

#include <algorithm>

#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

#include <yaml-cpp/yaml.h>

#include "Logging.h"
#include "Paths.h"

static ThemeRegistry *s_instance = nullptr;

static const QStringList paletteKeys = {
  QStringLiteral("window"),
  QStringLiteral("windowText"),
  QStringLiteral("base"),
  QStringLiteral("alternateBase"),
  QStringLiteral("text"),
  QStringLiteral("brightText"),
  QStringLiteral("button"),
  QStringLiteral("buttonText"),
  QStringLiteral("light"),
  QStringLiteral("mid"),
  QStringLiteral("dark"),
  QStringLiteral("highlight"),
  QStringLiteral("highlightedText"),
  QStringLiteral("link"),
  QStringLiteral("toolTipBase"),
  QStringLiteral("toolTipText"),
  QStringLiteral("red"),
  QStringLiteral("green"),
  QStringLiteral("orange"),
  QStringLiteral("error"),
};

void
ThemeRegistry::initialize()
{
    if (!s_instance)
        s_instance = new ThemeRegistry();
}

ThemeRegistry &
ThemeRegistry::instance()
{
    return *s_instance;
}

ThemeRegistry::ThemeRegistry()
{
    // Copy built-in themes
    const auto &builtins = themeDefinitions();
    allThemes_.assign(builtins.begin(), builtins.end());

    loadExternalThemes();

    // Sort by (sortOrder, name)
    std::sort(allThemes_.begin(), allThemes_.end(), [](const ThemeDef &a, const ThemeDef &b) {
        if (a.sortOrder != b.sortOrder)
            return a.sortOrder < b.sortOrder;
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
}

void
ThemeRegistry::loadExternalThemes()
{
    // Collect slugs we already have (built-in themes)
    QSet<QString> seenSlugs;
    for (const auto &t : allThemes_)
        seenSlugs.insert(t.slug);

    const auto locations = app_paths::data::themeSearchDirectories();
    for (const auto &location : locations) {
        QDir dir(location);
        if (!dir.exists())
            continue;

        const auto entries =
          dir.entryList({QStringLiteral("*.yml"), QStringLiteral("*.yaml")}, QDir::Files);
        QMap<QString, QString> chosen;
        for (const auto &filename : entries) {
            const QFileInfo info(filename);
            const QString slug   = info.completeBaseName();
            const QString suffix = info.suffix().toLower();

            if (!chosen.contains(slug) || suffix == QStringLiteral("yml"))
                chosen.insert(slug, filename);
        }

        for (auto it = chosen.cbegin(); it != chosen.cend(); ++it) {
            const QString &filename = it.value();
            const QString &slug     = it.key();
            if (seenSlugs.contains(slug)) {
                nhlog::ui()->info("Theme '{}' from {} skipped (already loaded)",
                                  slug.toStdString(),
                                  dir.path().toStdString());
                continue;
            }

            auto theme = parseThemeFile(dir.filePath(filename), slug);
            if (theme) {
                nhlog::ui()->info("Loaded external theme '{}' from {}",
                                  slug.toStdString(),
                                  dir.path().toStdString());
                allThemes_.push_back(std::move(*theme));
                seenSlugs.insert(slug);
            }
        }
    }
}

std::optional<ThemeDef>
ThemeRegistry::parseThemeFile(const QString &path, const QString &slug)
{
    YAML::Node root;
    try {
        root = YAML::LoadFile(path.toStdString());
    } catch (const YAML::Exception &e) {
        nhlog::ui()->warn("Failed to parse theme file {}: {}", path.toStdString(), e.what());
        return std::nullopt;
    }

    // Validate name
    if (!root["name"] || !root["name"].IsScalar() || root["name"].as<std::string>().empty()) {
        nhlog::ui()->warn("Theme file {} missing or empty 'name'", path.toStdString());
        return std::nullopt;
    }

    // Validate variant
    if (!root["variant"] || !root["variant"].IsScalar()) {
        nhlog::ui()->warn("Theme file {} missing 'variant'", path.toStdString());
        return std::nullopt;
    }
    auto variant = root["variant"].as<std::string>();
    if (variant != "light" && variant != "dark") {
        nhlog::ui()->warn("Theme file {} has invalid variant '{}' (expected 'light' or 'dark')",
                          path.toStdString(),
                          variant);
        return std::nullopt;
    }

    // Validate palette
    if (!root["palette"] || !root["palette"].IsMap()) {
        nhlog::ui()->warn("Theme file {} missing or invalid 'palette' map", path.toStdString());
        return std::nullopt;
    }
    auto palette = root["palette"];

    // Validate all 20 keys are present and are valid 6-char hex
    static const QRegularExpression hexRe(QStringLiteral("^[0-9a-fA-F]{6}$"));
    QMap<QString, QColor> colors;
    for (const auto &key : paletteKeys) {
        auto keyStd = key.toStdString();
        if (!palette[keyStd] || !palette[keyStd].IsScalar()) {
            nhlog::ui()->warn("Theme file {} missing palette key '{}'", path.toStdString(), keyStd);
            return std::nullopt;
        }
        auto hexStr = QString::fromStdString(palette[keyStd].as<std::string>());
        if (!hexRe.match(hexStr).hasMatch()) {
            nhlog::ui()->warn("Theme file {} has invalid hex '{}' for key '{}'",
                              path.toStdString(),
                              hexStr.toStdString(),
                              keyStd);
            return std::nullopt;
        }
        colors[key] = QColor(QStringLiteral("#") + hexStr);
    }

    ThemeDef def;
    def.slug      = slug;
    def.name      = QString::fromStdString(root["name"].as<std::string>());
    def.variant   = QString::fromStdString(variant);
    def.sortOrder = 300;
    def.source    = path;

    def.window          = colors[QStringLiteral("window")];
    def.windowText      = colors[QStringLiteral("windowText")];
    def.base            = colors[QStringLiteral("base")];
    def.alternateBase   = colors[QStringLiteral("alternateBase")];
    def.text            = colors[QStringLiteral("text")];
    def.brightText      = colors[QStringLiteral("brightText")];
    def.button          = colors[QStringLiteral("button")];
    def.buttonText      = colors[QStringLiteral("buttonText")];
    def.light           = colors[QStringLiteral("light")];
    def.mid             = colors[QStringLiteral("mid")];
    def.dark            = colors[QStringLiteral("dark")];
    def.highlight       = colors[QStringLiteral("highlight")];
    def.highlightedText = colors[QStringLiteral("highlightedText")];
    def.link            = colors[QStringLiteral("link")];
    def.toolTipBase     = colors[QStringLiteral("toolTipBase")];
    def.toolTipText     = colors[QStringLiteral("toolTipText")];
    def.red             = colors[QStringLiteral("red")];
    def.green           = colors[QStringLiteral("green")];
    def.orange          = colors[QStringLiteral("orange")];
    def.error           = colors[QStringLiteral("error")];

    return def;
}

const ThemeDef *
ThemeRegistry::findTheme(QStringView slug) const
{
    for (const auto &t : allThemes_) {
        if (t.slug == slug)
            return &t;
    }
    return nullptr;
}

QStringList
ThemeRegistry::themeNames(const QString &variant) const
{
    QStringList names;
    for (const auto &t : allThemes_) {
        if (variant.isEmpty() || t.variant == variant)
            names.append(t.name);
    }
    return names;
}

QStringList
ThemeRegistry::themeSlugs(const QString &variant) const
{
    QStringList slugs;
    for (const auto &t : allThemes_) {
        if (variant.isEmpty() || t.variant == variant)
            slugs.append(t.slug);
    }
    return slugs;
}

QString
ThemeRegistry::themeVariant(QStringView slug) const
{
    if (slug == u"system")
        return QStringLiteral("system");
    const auto *def = findTheme(slug);
    return def ? def->variant : QStringLiteral("light");
}

QString
ThemeRegistry::defaultThemeSlug(QStringView variant) const
{
    if (variant == u"system")
        return QStringLiteral("system");
    for (const auto &t : allThemes_) {
        if (t.variant == variant)
            return t.slug;
    }
    return QStringLiteral("komai-light");
}

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

#include "logging/Logging.h"
#include "profile/Paths.h"

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
  QStringLiteral("attention"),
  QStringLiteral("success"),
  QStringLiteral("warning"),
  QStringLiteral("error"),
};

static std::optional<ThemeUserColorSlot>
parseUserColorSlot(const YAML::Node &slotNode,
                   const QString &path,
                   QStringView label,
                   const QRegularExpression &hexRe)
{
    const auto labelString = label.toString();

    if (!slotNode || !slotNode.IsMap()) {
        nhlog::ui()->warn(
          "Theme file {} {} must be a mapping", path.toStdString(), labelString.toStdString());
        return std::nullopt;
    }

    ThemeUserColorSlot slot;
    const auto readOptionalColor = [&](QStringView key, QColor &target) -> bool {
        const auto keyStd = key.toString().toStdString();
        if (!slotNode[keyStd])
            return true;
        if (!slotNode[keyStd].IsScalar()) {
            nhlog::ui()->warn("Theme file {} {}.{} must be a string",
                              path.toStdString(),
                              labelString.toStdString(),
                              key.toString().toStdString());
            return false;
        }

        const auto hex = QString::fromStdString(slotNode[keyStd].as<std::string>());
        if (!hexRe.match(hex).hasMatch()) {
            nhlog::ui()->warn("Theme file {} has invalid hex '{}' for {}.{}",
                              path.toStdString(),
                              hex.toStdString(),
                              labelString.toStdString(),
                              key.toString().toStdString());
            return false;
        }

        target = QColor(hex);
        return true;
    };

    if (!slotNode["background"] || !slotNode["background"].IsScalar()) {
        nhlog::ui()->warn(
          "Theme file {} missing {}.background", path.toStdString(), labelString.toStdString());
        return std::nullopt;
    }

    const auto backgroundHex = QString::fromStdString(slotNode["background"].as<std::string>());
    if (!hexRe.match(backgroundHex).hasMatch()) {
        nhlog::ui()->warn("Theme file {} has invalid hex '{}' for {}.background",
                          path.toStdString(),
                          backgroundHex.toStdString(),
                          labelString.toStdString());
        return std::nullopt;
    }
    slot.background = QColor(backgroundHex);

    if (!readOptionalColor(QStringLiteral("text"), slot.text) ||
        !readOptionalColor(QStringLiteral("secondaryText"), slot.secondaryText) ||
        !readOptionalColor(QStringLiteral("link"), slot.link)) {
        return std::nullopt;
    }

    for (auto it = slotNode.begin(); it != slotNode.end(); ++it) {
        const auto key = QString::fromStdString(it->first.as<std::string>());
        if (key != QLatin1String("background") && key != QLatin1String("text") &&
            key != QLatin1String("secondaryText") && key != QLatin1String("link")) {
            nhlog::ui()->warn("Theme file {} has unexpected key '{}' in {}",
                              path.toStdString(),
                              key.toStdString(),
                              labelString.toStdString());
            return std::nullopt;
        }
    }

    return slot;
}

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

    // Validate all 20 keys are present and are valid #-prefixed hex
    static const QRegularExpression hexRe(QStringLiteral("^#[0-9a-fA-F]{6}$"));
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
        colors[key] = QColor(hexStr);
    }

    // Validate userColors section (required)
    if (!root["userColors"] || !root["userColors"].IsMap()) {
        nhlog::ui()->warn("Theme file {} missing or invalid 'userColors' map", path.toStdString());
        return std::nullopt;
    }
    auto userColorsNode = root["userColors"];

    auto selfSlot =
      parseUserColorSlot(userColorsNode["self"], path, QStringLiteral("userColors.self"), hexRe);
    if (!selfSlot)
        return std::nullopt;

    if (!userColorsNode["others"] || !userColorsNode["others"].IsSequence()) {
        nhlog::ui()->warn("Theme file {} missing or invalid userColors.others list",
                          path.toStdString());
        return std::nullopt;
    }
    auto othersNode = userColorsNode["others"];
    if (othersNode.size() < 1) {
        nhlog::ui()->warn("Theme file {} userColors.others must have at least 1 entry",
                          path.toStdString());
        return std::nullopt;
    }

    std::vector<ThemeUserColorSlot> userColorOthers;
    userColorOthers.reserve(othersNode.size());
    for (std::size_t i = 0; i < othersNode.size(); ++i) {
        auto otherSlot = parseUserColorSlot(
          othersNode[i], path, QStringLiteral("userColors.others[%1]").arg(i), hexRe);
        if (!otherSlot)
            return std::nullopt;
        userColorOthers.push_back(*otherSlot);
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
    def.attention       = colors[QStringLiteral("attention")];
    def.success         = colors[QStringLiteral("success")];
    def.warning         = colors[QStringLiteral("warning")];
    def.error           = colors[QStringLiteral("error")];

    def.userColorSelf   = *selfSlot;
    def.userColorOthers = std::move(userColorOthers);

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
    const auto *def = findTheme(slug);
    return def ? def->variant : QStringLiteral("light");
}

QString
ThemeRegistry::defaultThemeSlug(QStringView variant) const
{
    for (const auto &t : allThemes_) {
        if (t.variant == variant)
            return t.slug;
    }
    return QStringLiteral("komai-light");
}

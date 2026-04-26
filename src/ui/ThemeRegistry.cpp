// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThemeRegistry.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSet>

#include "komai-rust-cxxbridge/ffi.h"

#include "logging/Logging.h"
#include "profile/Paths.h"

static ThemeRegistry *s_instance = nullptr;

static int
variantRank(QStringView variant)
{
    if (variant == QLatin1String("light"))
        return 0;
    if (variant == QLatin1String("dark"))
        return 1;
    return 2;
}

static bool
isPinnedKomaiTheme(const ThemeDef &theme)
{
    return theme.slug.endsWith(QLatin1String("-komai"));
}

static bool
themeDisplayLess(const ThemeDef &a, const ThemeDef &b)
{
    const auto aVariantRank = variantRank(a.variant);
    const auto bVariantRank = variantRank(b.variant);
    if (aVariantRank != bVariantRank)
        return aVariantRank < bVariantRank;

    const bool aPinned = isPinnedKomaiTheme(a);
    const bool bPinned = isPinnedKomaiTheme(b);
    if (aPinned != bPinned)
        return aPinned;

    const int nameCompare = QString::compare(a.name, b.name, Qt::CaseInsensitive);
    if (nameCompare != 0)
        return nameCompare < 0;

    return QString::compare(a.slug, b.slug, Qt::CaseInsensitive) < 0;
}

static QString
fromRustString(const ::rust::String &value)
{
    return QString::fromStdString(static_cast<std::string>(value));
}

static QColor
parseColor(const ::rust::String &value)
{
    return QColor(fromRustString(value));
}

static QColor
parseOptionalColor(const ::rust::String &value)
{
    const auto hex = fromRustString(value);
    return hex.isEmpty() ? QColor() : QColor(hex);
}

static ThemeUserColorSlot
toThemeUserColorSlot(const ::komai::rust::ThemeUserColorSlotData &slot)
{
    ThemeUserColorSlot converted;
    converted.background    = parseColor(slot.background);
    converted.text          = parseOptionalColor(slot.text);
    converted.secondaryText = parseOptionalColor(slot.secondary_text);
    converted.link          = parseOptionalColor(slot.link);
    return converted;
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

static ThemeDef
convertFfiTheme(const ::komai::rust::ThemeExternalDefinition &ffi,
                const QString &slug,
                int sortOrder,
                const QString &source)
{
    ThemeDef def;
    def.slug      = slug;
    def.name      = fromRustString(ffi.name);
    def.variant   = fromRustString(ffi.variant);
    def.sortOrder = sortOrder;
    def.source    = source;

    const auto &palette = ffi.palette;
    def.window          = parseColor(palette.window);
    def.windowText      = parseColor(palette.window_text);
    def.base            = parseColor(palette.base);
    def.alternateBase   = parseColor(palette.alternate_base);
    def.text            = parseColor(palette.text);
    def.brightText      = parseColor(palette.bright_text);
    def.button          = parseColor(palette.button);
    def.buttonText      = parseColor(palette.button_text);
    def.light           = parseColor(palette.light);
    def.mid             = parseColor(palette.mid);
    def.dark            = parseColor(palette.dark);
    def.highlight       = parseColor(palette.highlight);
    def.highlightedText = parseColor(palette.highlighted_text);
    def.link            = parseColor(palette.link);
    def.toolTipBase     = parseColor(palette.tool_tip_base);
    def.toolTipText     = parseColor(palette.tool_tip_text);
    def.attention       = parseColor(palette.attention);
    def.attentionText   = parseColor(palette.attention_text);
    def.success         = parseColor(palette.success);
    def.warning         = parseColor(palette.warning);
    def.error           = parseColor(palette.error);

    def.userColorSelf = toThemeUserColorSlot(ffi.user_color_self);
    def.userColorOthers.reserve(ffi.user_color_others.size());
    for (const auto &slot : ffi.user_color_others)
        def.userColorOthers.push_back(toThemeUserColorSlot(slot));

    return def;
}

ThemeRegistry::ThemeRegistry()
{
    const auto builtins = ::komai::rust::theme_builtin_themes();
    for (const auto &entry : builtins.themes) {
        allThemes_.push_back(convertFfiTheme(
          entry.theme, fromRustString(entry.slug), entry.sort_order, QStringLiteral("builtin")));
    }
    for (const auto &err : builtins.errors)
        komai::logging::ui()->error("Built-in theme error: {}", static_cast<std::string>(err));

    loadExternalThemes();

    std::sort(allThemes_.begin(), allThemes_.end(), themeDisplayLess);
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
                komai::logging::ui()->info("Theme '{}' from {} skipped (already loaded)",
                                           slug.toStdString(),
                                           dir.path().toStdString());
                continue;
            }

            auto theme = parseThemeFile(dir.filePath(filename), slug);
            if (theme) {
                komai::logging::ui()->info("Loaded external theme '{}' from {}",
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
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        komai::logging::ui()->warn(
          "Failed to read theme file {}: {}", path.toStdString(), file.errorString().toStdString());
        return std::nullopt;
    }

    const auto parsed = ::komai::rust::theme_parse_external_theme(file.readAll().toStdString());
    if (!parsed.has_theme) {
        komai::logging::ui()->warn("Failed to parse theme file {}: {}",
                                   path.toStdString(),
                                   static_cast<std::string>(parsed.error_message));
        return std::nullopt;
    }

    return convertFfiTheme(parsed.theme, slug, 300, path);
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
    return QStringLiteral("light-komai");
}

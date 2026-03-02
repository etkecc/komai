// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThemeCommands.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <string>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QTimer>

#include <yaml-cpp/yaml.h>

#include "Logging.h"
#include "ThemeColorUtils.h"
#include "ui/ThemeRegistry.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static QString
userThemesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           QStringLiteral("/komai/themes");
}

static QByteArray
httpGet(QNetworkAccessManager &nam, const QUrl &url, int timeoutMs = 15000)
{
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "komai-theme-cli/1.0");

    auto *reply = nam.get(req);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        delete reply;
        return {};
    }

    auto data = reply->readAll();
    auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    delete reply;

    if (code < 200 || code >= 300)
        return {};

    return data;
}

// Write theme YAML in canonical key order (matches bin/theme/colors.py:write_theme_yaml)
static bool
writeThemeYaml(const QString &path,
               const std::string &name,
               const std::string &author,
               const std::string &variant,
               const theme_color::Palette &palette,
               const theme_color::Palette *sourceBase16 = nullptr)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "name: \"" << QString::fromStdString(name) << "\"\n";
    if (!author.empty())
        out << "author: \"" << QString::fromStdString(author) << "\"\n";
    out << "variant: \"" << QString::fromStdString(variant) << "\"\n";
    out << "palette:\n";

    for (const auto *key : theme_color::PALETTE_KEYS) {
        auto it = palette.find(key);
        if (it != palette.end())
            out << "  " << key << ": \"" << QString::fromStdString(it->second) << "\"\n";
    }
    for (const auto *key : theme_color::CUSTOM_KEYS) {
        auto it = palette.find(key);
        if (it != palette.end())
            out << "  " << key << ": \"" << QString::fromStdString(it->second) << "\"\n";
    }

    if (sourceBase16) {
        out << "source_base16:\n";
        for (int i = 0; i < 16; ++i) {
            char slot[8];
            std::snprintf(slot, sizeof(slot), "base%02X", i);
            auto it = sourceBase16->find(slot);
            if (it != sourceBase16->end())
                out << "  " << slot << ": \"" << QString::fromStdString(it->second) << "\"\n";
        }
    }

    return true;
}

// Parse raw Base16 YAML from tinted-theming
static bool
parseBase16Yaml(const QByteArray &content,
                std::string &outName,
                std::string &outAuthor,
                theme_color::Palette &outPalette)
{
    try {
        auto root = YAML::Load(content.toStdString());
        if (root["name"] && root["name"].IsScalar())
            outName = root["name"].as<std::string>();
        if (root["author"] && root["author"].IsScalar())
            outAuthor = root["author"].as<std::string>();

        YAML::Node pal;
        if (root["palette"] && root["palette"].IsMap()) {
            pal = root["palette"];
        } else {
            // Older format: base00-base0F at top level
            pal = root;
        }

        for (int i = 0; i < 16; ++i) {
            char slot[8];
            std::snprintf(slot, sizeof(slot), "base%02X", i);
            if (pal[slot] && pal[slot].IsScalar()) {
                auto val = pal[slot].as<std::string>();
                // Strip leading # if present
                if (!val.empty() && val[0] == '#')
                    val = val.substr(1);
                outPalette[slot] = val;
            }
        }
    } catch (const YAML::Exception &) {
        return false;
    }
    return true;
}

// Validate that all 16 base16 slots are present and valid hex
static bool
validateBase16Palette(const theme_color::Palette &palette)
{
    for (int i = 0; i < 16; ++i) {
        char slot[8];
        std::snprintf(slot, sizeof(slot), "base%02X", i);
        auto it = palette.find(slot);
        if (it == palette.end())
            return false;
        const auto &val = it->second;
        if (val.size() != 6)
            return false;
        for (char c : val) {
            if (!std::isxdigit(static_cast<unsigned char>(c)))
                return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Subcommand handlers
// ---------------------------------------------------------------------------

static const char *SCHEMES_URL =
  "https://raw.githubusercontent.com/tinted-theming/schemes/refs/heads/spec-0.11/base16";

static int
handleTintedImport(int argc, char *argv[], QCoreApplication & /*app*/)
{
    // Parse args: komai theme tinted-import <slug> [name] [--force] [--variant light|dark]
    QString slug;
    QString customName;
    bool force = false;
    QString variantFlag;

    // Find positional args after "tinted-import"
    bool pastSubcmd = false;
    std::vector<QString> positionals;
    for (int i = 1; i < argc; ++i) {
        QString arg{argv[i]};
        if (arg == QLatin1String("theme"))
            continue;
        if (arg == QLatin1String("tinted-import")) {
            pastSubcmd = true;
            continue;
        }
        if (!pastSubcmd)
            continue;

        if (arg == QLatin1String("--force")) {
            force = true;
        } else if (arg == QLatin1String("--variant")) {
            if (i + 1 < argc)
                variantFlag = QString{argv[++i]};
        } else if (!arg.startsWith(QLatin1Char('-'))) {
            positionals.push_back(arg);
        }
    }

    if (positionals.empty()) {
        std::cerr
          << "Usage: komai theme tinted-import <slug> [name] [--force] [--variant light|dark]\n";
        return 1;
    }
    slug = positionals[0];
    if (positionals.size() > 1)
        customName = positionals[1];

    QNetworkAccessManager nam;

    auto url = QStringLiteral("%1/%2.yaml").arg(QLatin1String(SCHEMES_URL), slug);
    std::cout << "Downloading " << url.toStdString() << "...\n";

    auto data = httpGet(nam, QUrl(url));
    if (data.isEmpty()) {
        std::cerr << "ERROR: Theme '" << slug.toStdString()
                  << "' not found in tinted-theming/schemes\n"
                  << "Use 'komai theme tinted-search' to see available themes.\n";
        return 1;
    }

    std::string themeName, themeAuthor;
    theme_color::Palette rawPalette;
    if (!parseBase16Yaml(data, themeName, themeAuthor, rawPalette)) {
        std::cerr << "ERROR: Failed to parse theme YAML\n";
        return 1;
    }

    if (!validateBase16Palette(rawPalette)) {
        std::cerr << "ERROR: Downloaded theme is missing required Base16 slots\n";
        return 1;
    }

    if (themeName.empty())
        themeName = slug.toStdString();
    themeName = theme_color::stripVariantSuffix(themeName);

    std::string variant;
    if (!variantFlag.isEmpty()) {
        variant = variantFlag.toStdString();
        if (variant != "light" && variant != "dark") {
            std::cerr << "ERROR: --variant must be 'light' or 'dark'\n";
            return 1;
        }
    } else {
        variant = theme_color::detectVariant(rawPalette);
    }

    // Determine output name
    auto outputBase = customName.isEmpty() ? slug : customName;
    auto outputFile = QStringLiteral("%1/%2-%3.yml")
                        .arg(userThemesDir(), outputBase, QString::fromStdString(variant));

    if (QFile::exists(outputFile) && !force) {
        std::cerr << "Theme already exists: " << outputFile.toStdString() << "\n"
                  << "Use --force to overwrite.\n";
        return 1;
    }

    // Apply Base16 → QPalette mapping
    auto mapped = theme_color::base16ToPalette(rawPalette, variant);
    auto custom = theme_color::base16ToCustom(rawPalette);
    theme_color::Palette finalPalette;
    finalPalette.insert(mapped.begin(), mapped.end());
    finalPalette.insert(custom.begin(), custom.end());

    if (!writeThemeYaml(outputFile, themeName, themeAuthor, variant, finalPalette, &rawPalette)) {
        std::cerr << "ERROR: Failed to write " << outputFile.toStdString() << "\n";
        return 1;
    }

    std::cout << "\xe2\x9c\x85 Theme saved to: " << outputFile.toStdString() << "\n";
    return 0;
}

static int
handleTintedSearch(int argc, char *argv[], QCoreApplication & /*app*/)
{
    // Parse optional query
    QString query;
    bool pastSubcmd = false;
    for (int i = 1; i < argc; ++i) {
        QString arg{argv[i]};
        if (arg == QLatin1String("theme"))
            continue;
        if (arg == QLatin1String("tinted-search")) {
            pastSubcmd = true;
            continue;
        }
        if (!pastSubcmd)
            continue;
        if (!arg.startsWith(QLatin1Char('-'))) {
            query = arg;
            break;
        }
    }

    std::cout << "Fetching theme list from tinted-theming/schemes...\n";

    QNetworkAccessManager nam;
    QUrl apiUrl(QStringLiteral(
      "https://api.github.com/repos/tinted-theming/schemes/contents/base16?ref=spec-0.11"));

    QNetworkRequest req(apiUrl);
    req.setRawHeader("Accept", "application/vnd.github.v3+json");
    req.setRawHeader("User-Agent", "komai-theme-cli/1.0");

    auto *reply = nam.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(15000);
    loop.exec();

    if (!reply->isFinished() || reply->error() != QNetworkReply::NoError) {
        std::cerr << "ERROR: Failed to fetch theme list\n";
        if (reply->isFinished())
            std::cerr << reply->errorString().toStdString() << "\n";
        delete reply;
        return 1;
    }

    auto json = QJsonDocument::fromJson(reply->readAll());
    delete reply;
    auto entries = json.array();

    QStringList themes;
    for (const auto &entry : entries) {
        auto name = entry.toObject()[QStringLiteral("name")].toString();
        if (name.endsWith(QStringLiteral(".yaml"))) {
            auto slug = name.chopped(5); // remove .yaml
            if (query.isEmpty() || slug.contains(query, Qt::CaseInsensitive))
                themes.append(slug);
        }
    }
    themes.sort(Qt::CaseInsensitive);

    if (query.isEmpty())
        std::cout << "\nAvailable themes (" << themes.size() << "):\n\n";
    else
        std::cout << "\nThemes matching '" << query.toStdString() << "' (" << themes.size()
                  << "):\n\n";

    for (const auto &t : themes)
        std::cout << "  " << t.toStdString() << "\n";

    return 0;
}

static int
handleList(int /*argc*/, char * /*argv*/[], QCoreApplication & /*app*/)
{
    // ThemeRegistry uses nhlog for diagnostics; initialize a minimal stderr logger.
    nhlog::init(QStringLiteral("off"), {}, false);

    ThemeRegistry::initialize();
    const auto &themes = ThemeRegistry::instance().allThemes();

    // Find max slug/name widths for alignment
    int maxSlug = 4, maxName = 4;
    for (const auto &t : themes) {
        maxSlug = std::max(maxSlug, static_cast<int>(t.slug.size()));
        maxName = std::max(maxName, static_cast<int>(t.name.size()));
    }

    for (const auto &t : themes) {
        std::cout << qPrintable(t.slug.leftJustified(maxSlug + 2))
                  << qPrintable(t.name.leftJustified(maxName + 2))
                  << qPrintable(t.variant.leftJustified(7)) << qPrintable(t.source) << "\n";
    }

    return 0;
}

static int
handleCreateSample(int argc, char *argv[], QCoreApplication & /*app*/)
{
    // Parse args: komai theme create-sample <light|dark> <name> [--force]
    bool force      = false;
    bool pastSubcmd = false;
    std::vector<QString> positionals;

    for (int i = 1; i < argc; ++i) {
        QString arg{argv[i]};
        if (arg == QLatin1String("theme"))
            continue;
        if (arg == QLatin1String("create-sample")) {
            pastSubcmd = true;
            continue;
        }
        if (!pastSubcmd)
            continue;

        if (arg == QLatin1String("--force")) {
            force = true;
        } else if (!arg.startsWith(QLatin1Char('-'))) {
            positionals.push_back(arg);
        }
    }

    if (positionals.size() < 2) {
        std::cerr << "Usage: komai theme create-sample <light|dark> <name> [--force]\n";
        return 1;
    }

    auto variant = positionals[0].toStdString();
    auto name    = positionals[1].toStdString();

    if (variant != "light" && variant != "dark") {
        std::cerr << "ERROR: variant must be 'light' or 'dark'\n";
        return 1;
    }

    auto outputFile =
      QStringLiteral("%1/%2-%3.yml")
        .arg(userThemesDir(), QString::fromStdString(name), QString::fromStdString(variant));

    if (QFile::exists(outputFile) && !force) {
        std::cerr << "Theme already exists: " << outputFile.toStdString() << "\n"
                  << "Use --force to overwrite.\n";
        return 1;
    }

    // Distinct sample palettes (intentionally different from komai-light/komai-dark)
    theme_color::Palette palette;
    if (variant == "light") {
        // Warm off-white + slate + teal (distinct from komai-light: pure white + orange)
        palette = {
          {"window", "f8f6f2"},
          {"windowText", "3b4252"},
          {"base", "f0ede8"},
          {"alternateBase", "e8e4de"},
          {"text", "3b4252"},
          {"brightText", "d8dee9"},
          {"button", "f0ede8"},
          {"buttonText", "5e6779"},
          {"light", "ebe8e2"},
          {"mid", "d5d0c8"},
          {"dark", "4c566a"},
          {"highlight", "2b9ea0"},
          {"highlightedText", "ffffff"},
          {"link", "238a8c"},
          {"toolTipBase", "f0ede8"},
          {"toolTipText", "3b4252"},
          {"red", "bf616a"},
          {"green", "a3be8c"},
          {"orange", "d08770"},
          {"error", "bf616a"},
        };
    } else {
        // Neutral dark + muted purple accent (distinct from komai-dark: orange accent)
        palette = {
          {"window", "2e3440"},
          {"windowText", "d8dee9"},
          {"base", "272c36"},
          {"alternateBase", "3b4252"},
          {"text", "d8dee9"},
          {"brightText", "eceff4"},
          {"button", "272c36"},
          {"buttonText", "8892a4"},
          {"light", "d8dee9"},
          {"mid", "3b4252"},
          {"dark", "3e4556"},
          {"highlight", "8b7ec8"},
          {"highlightedText", "eceff4"},
          {"link", "a098d8"},
          {"toolTipBase", "272c36"},
          {"toolTipText", "d8dee9"},
          {"red", "bf616a"},
          {"green", "a3be8c"},
          {"orange", "d08770"},
          {"error", "bf616a"},
        };
    }

    if (!writeThemeYaml(outputFile, name, "", variant, palette)) {
        std::cerr << "ERROR: Failed to write " << outputFile.toStdString() << "\n";
        return 1;
    }

    std::cout << "\xe2\x9c\x85 Theme saved to: " << outputFile.toStdString() << "\n";
    return 0;
}

// ---------------------------------------------------------------------------
// Subcommand dispatch
// ---------------------------------------------------------------------------

using SubcommandHandler = std::function<int(int, char *[], QCoreApplication &)>;

static const std::map<QString, SubcommandHandler> &
subcommands()
{
    static const std::map<QString, SubcommandHandler> table = {
      {QStringLiteral("tinted-import"), handleTintedImport},
      {QStringLiteral("tinted-search"), handleTintedSearch},
      {QStringLiteral("list"), handleList},
      {QStringLiteral("create-sample"), handleCreateSample},
    };
    return table;
}

int
runThemeCommand(int argc, char *argv[], QCoreApplication &app)
{
    // Find the subcommand (first positional after "theme")
    QString subcmd;
    bool pastTheme = false;
    for (int i = 1; i < argc; ++i) {
        QString arg{argv[i]};
        if (arg == QLatin1String("theme")) {
            pastTheme = true;
            continue;
        }
        if (!pastTheme)
            continue;
        if (!arg.startsWith(QLatin1Char('-'))) {
            subcmd = arg;
            break;
        }
    }

    const auto &table = subcommands();

    if (subcmd.isEmpty() || subcmd == QLatin1String("--help") || subcmd == QLatin1String("-h")) {
        std::cout << "Usage: komai theme <subcommand> [args...]\n\n"
                  << "Subcommands:\n"
                  << "  tinted-import <slug> [name]   Import a Base16 theme from tinted-theming\n"
                  << "  tinted-search [query]         Search available Base16 themes\n"
                  << "  list                          List all loaded themes\n"
                  << "  create-sample <variant> <name> Create a starter theme YAML\n";
        return subcmd.isEmpty() ? 1 : 0;
    }

    auto it = table.find(subcmd);
    if (it == table.end()) {
        std::cerr << "Unknown subcommand: " << subcmd.toStdString() << "\n"
                  << "Run 'komai theme --help' for a list of subcommands.\n";
        return 1;
    }

    return it->second(argc, argv, app);
}

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThemeCommandHandlers.h"

#include <iostream>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include "ThemeColorUtils.h"
#include "ThemeCommandUtils.h"

namespace theme_command {

static const char *SCHEMES_URL =
  "https://raw.githubusercontent.com/tinted-theming/schemes/refs/heads/spec-0.11/base16";

int
handleTintedImport(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    const bool force       = parsed.hasFlag(QStringLiteral("--force"));
    const auto variantFlag = parsed.flagOr(QStringLiteral("--variant"));
    const auto slug        = parsed.positionals.value(0);
    const auto customName  = parsed.positionals.value(1);

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
                        .arg(userThemesDir(), QString::fromStdString(variant), outputBase);

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

    // Generate user colors from the highlight (accent) color
    auto userColors =
      theme_color::generateUserColors(finalPalette["highlight"], finalPalette["base"], variant);
    std::vector<std::string> linkBackgrounds = {
      finalPalette["window"],
      finalPalette["base"],
      finalPalette["alternateBase"],
      userColors.self.background,
    };
    for (const auto &slot : userColors.others)
        linkBackgrounds.push_back(slot.background);
    finalPalette["link"] =
      theme_color::adjustFgForBackgrounds(finalPalette["link"], linkBackgrounds, variant, 4.5);
    theme_color::populateUserColorForegrounds(userColors, finalPalette, variant);

    if (!writeThemeYaml(
          outputFile, themeName, themeAuthor, variant, finalPalette, userColors, &rawPalette)) {
        std::cerr << "ERROR: Failed to write " << outputFile.toStdString() << "\n";
        return 1;
    }

    std::cout << "\xe2\x9c\x85 Theme saved to: " << outputFile.toStdString() << "\n";
    return 0;
}

int
handleTintedSearch(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    const auto query = parsed.positionals.value(0);

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

} // namespace theme_command

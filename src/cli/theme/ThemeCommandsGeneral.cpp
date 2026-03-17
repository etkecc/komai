// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThemeCommandHandlers.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QFile>

#include "ThemeColorUtils.h"
#include "ThemeCommandUtils.h"
#include "logging/Logging.h"
#include "ui/ThemeRegistry.h"

namespace theme_command {

int
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

int
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
        // Warm off-white + slate + teal (distinct from komai-light: pure white + warning)
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
          {"attention", "bf616a"},
          {"success", "a3be8c"},
          {"warning", "d08770"},
          {"error", "bf616a"},
        };
    } else {
        // Neutral dark + muted purple accent (distinct from komai-dark: warning accent)
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
          {"attention", "bf616a"},
          {"success", "a3be8c"},
          {"warning", "d08770"},
          {"error", "bf616a"},
        };
    }

    auto userColors =
      theme_color::generateUserColors(palette["highlight"], palette["base"], variant);
    std::vector<std::string> linkBackgrounds = {
      palette["window"],
      palette["base"],
      palette["alternateBase"],
      userColors.self,
    };
    linkBackgrounds.insert(
      linkBackgrounds.end(), userColors.others.begin(), userColors.others.end());
    palette["link"] =
      theme_color::adjustFgForBackgrounds(palette["link"], linkBackgrounds, variant, 4.5);

    if (!writeThemeYaml(outputFile, name, "", variant, palette, userColors)) {
        std::cerr << "ERROR: Failed to write " << outputFile.toStdString() << "\n";
        return 1;
    }

    std::cout << "\xe2\x9c\x85 Theme saved to: " << outputFile.toStdString() << "\n";
    return 0;
}

} // namespace theme_command

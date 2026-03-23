// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThemeCommands.h"

#include <functional>
#include <iostream>
#include <map>

#include <QCoreApplication>

#include "theme/ThemeCommandHandlers.h"

using SubcommandHandler = std::function<int(int, char *[], QCoreApplication &)>;

static const std::map<QString, SubcommandHandler> &
subcommands()
{
    static const std::map<QString, SubcommandHandler> table = {
      {QStringLiteral("tinted-import"), theme_command::handleTintedImport},
      {QStringLiteral("tinted-search"), theme_command::handleTintedSearch},
      {QStringLiteral("list"), theme_command::handleList},
      {QStringLiteral("create-sample"), theme_command::handleCreateSample},
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
                  << "  list                          List all available themes\n"
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

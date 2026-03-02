// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CliDispatch.h"

#include <functional>
#include <iostream>
#include <map>

#include <QCoreApplication>
#include <QString>

#include "ThemeCommands.h"

using HandlerFn = std::function<int(int argc, char *argv[], QCoreApplication &app)>;

static const std::map<QString, HandlerFn> &
commandGroups()
{
    static const std::map<QString, HandlerFn> groups = {
      {QStringLiteral("theme"), runThemeCommand},
    };
    return groups;
}

// Find the first positional argument, skipping known option+value pairs.
static QString
findCommandGroup(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        QString arg{argv[i]};

        // Skip known option+value pairs that consume the next arg
        if (arg == QLatin1String("-p") || arg == QLatin1String("--profile") ||
            arg == QLatin1String("-l") || arg == QLatin1String("--log-level") ||
            arg == QLatin1String("-L") || arg == QLatin1String("--log-type")) {
            ++i; // skip the value
            continue;
        }

        // Skip flags (start with -)
        if (arg.startsWith(QLatin1Char('-')))
            continue;

        // Skip matrix: URIs
        if (arg.startsWith(QLatin1String("matrix:")))
            continue;

        return arg;
    }
    return {};
}

int
dispatchCliCommand(int argc, char *argv[])
{
    auto group = findCommandGroup(argc, argv);
    if (group.isEmpty())
        return -1;

    const auto &groups = commandGroups();
    auto it            = groups.find(group);
    if (it == groups.end())
        return -1;

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("komai"));

    return it->second(argc, argv, app);
}

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <iostream>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QString>
#include <QStringList>

#include "app/MainApplicationSupport.h"
#include "dbus/Api.h"

namespace cli_dbus {

/// Returns the profile ID from the -p / --profile flag, or empty for default.
inline QString
profileFromArgs(int argc, char *argv[])
{
    return app::support::selectedProfileFromArgs(argc, argv).value;
}

/// Initializes D-Bus metatypes and checks that the target profile has a running
/// Komai instance on the session bus.  Returns true on success; prints an error
/// to stderr and returns false otherwise.
inline bool
ensureConnected(const QString &profileId)
{
    komai::dbus::init();

    auto *iface = QDBusConnection::sessionBus().interface();
    if (!iface) {
        std::cerr << "Error: cannot connect to the session D-Bus\n";
        return false;
    }

    auto service = komai::dbus::serviceName(profileId);
    if (!iface->isServiceRegistered(service)) {
        auto display = profileId.isEmpty() ? QStringLiteral("default") : profileId;
        std::cerr << "Error: no running Komai instance for profile '" << display.toStdString()
                  << "'\n"
                  << "Start Komai first: komai";
        if (!profileId.isEmpty() && profileId != QLatin1String("default"))
            std::cerr << " -p " << profileId.toStdString();
        std::cerr << "\n";
        return false;
    }

    return true;
}

/// Collects positional arguments after the given keyword in argv,
/// skipping known option+value pairs and flags.
inline QStringList
positionalsAfter(int argc, char *argv[], const QString &keyword)
{
    QStringList result;
    bool pastKeyword = false;
    for (int i = 1; i < argc; ++i) {
        QString arg{argv[i]};

        // Skip known option+value pairs
        if (arg == QLatin1String("-p") || arg == QLatin1String("--profile") ||
            arg == QLatin1String("-l") || arg == QLatin1String("--log-level") ||
            arg == QLatin1String("-L") || arg == QLatin1String("--log-type")) {
            ++i;
            continue;
        }
        // Skip flags (including --profile=value, -pvalue, etc.)
        if (arg.startsWith(QLatin1Char('-')))
            continue;

        if (!pastKeyword) {
            if (arg == keyword)
                pastKeyword = true;
            continue;
        }

        result.append(arg);
    }
    return result;
}

/// Returns true if --help or -h appears anywhere in argv.
inline bool
hasHelpFlag(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        QString arg{argv[i]};
        if (arg == QLatin1String("--help") || arg == QLatin1String("-h"))
            return true;
    }
    return false;
}

} // namespace cli_dbus

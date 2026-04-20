// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

class QCoreApplication;

namespace cli_schema {

struct FlagDef
{
    QString longName;         // e.g. "--msgtype" (includes the dashes)
    QString shortName;        // e.g. "-p" (empty if none)
    bool takesValue = false;  // false = boolean flag, true = option+value
    QStringList valueEnum;    // non-empty => only these values accepted
    QString valuePlaceholder; // shown in help: "<n>", "<mxc-uri>", ...
    QString help;             // one-line description
    QString defaultValue;     // optional; shown in help only when non-empty
};

struct PositionalDef
{
    QString name; // shown in help as <name>
    QString help;
    bool optional = false;
    bool variadic = false; // final positional greedily captures the rest
};

struct ParsedArgs
{
    QStringList positionals;            // positionals for the leaf subcommand
    QHash<QString, QString> flagValues; // long-name -> value (takesValue=true)
    QSet<QString> flagsPresent;         // long-names of boolean flags that appeared
    QStringList subcommandPath;         // e.g. {"settings", "ui", "set-theme"}
    QString profileId;                  // normalised via profile_id; empty = default

    // Returns the value of a takesValue=true flag, or defaultValue if absent.
    QString flagOr(const QString &longName, const QString &defaultValue = {}) const
    {
        return flagValues.value(longName, defaultValue);
    }

    bool hasFlag(const QString &longName) const { return flagsPresent.contains(longName); }
};

struct SubcommandDef
{
    QString name;     // leaf name, e.g. "timeline" or "set-theme"
    QString help;     // one-line summary shown in the parent listing
    QString longHelp; // optional paragraph shown with `... <sub> --help`

    QList<PositionalDef> positionals;
    QList<FlagDef> flags;
    QList<SubcommandDef> subcommands; // recursion (e.g. `settings ui`)

    // If requiresProfile is true, the dispatcher calls cli_ipc::ensureConnected
    // before invoking the handler. Set to false for offline subcommands
    // (profiles launcher, theme *, completions *).
    bool requiresProfile = true;

    // Handler invoked after the dispatcher has parsed argv into ParsedArgs.
    // Mutually exclusive with rawHandler.
    std::function<int(const ParsedArgs &, QCoreApplication &)> handler;

    // Escape hatch: the dispatcher does not parse argv, simply forwards argc/argv
    // to rawHandler. Used where the handler has non-standard needs (e.g. mcp
    // serve exec-ing into komai-mcp).
    std::function<int(int, char *[], QCoreApplication &)> rawHandler;
};

struct GroupDef
{
    QString name;     // top-level group name, e.g. "app", "rooms"
    QString help;     // one-line summary
    QString longHelp; // optional paragraph for `komai <group> --help`
    QList<SubcommandDef> subcommands;
};

} // namespace cli_schema

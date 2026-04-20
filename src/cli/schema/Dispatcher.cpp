// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Dispatcher.h"

#include <iostream>

#include <QCoreApplication>
#include <QHash>
#include <QString>
#include <QStringList>

#include "../IpcClient.h"
#include "HelpRenderer.h"

namespace cli_schema {

namespace {

const FlagDef *
findFlag(const QList<FlagDef> &flags, const QString &name)
{
    for (const auto &flag : flags) {
        if (flag.longName == name || (!flag.shortName.isEmpty() && flag.shortName == name))
            return &flag;
    }
    return nullptr;
}

const SubcommandDef *
findSubcommand(const QList<SubcommandDef> &subs, const QString &name)
{
    for (const auto &sub : subs) {
        if (sub.name == name)
            return &sub;
    }
    return nullptr;
}

bool
looksLikeFlag(const QString &arg)
{
    return arg.startsWith(QLatin1Char('-'));
}

// Returns true if `arg` is a top-level option that consumes the next argv
// element as its value (these are managed by MainApplication.cpp's QCommandLineParser
// but still appear in argv when the dispatcher scans).
bool
isTopLevelValueOption(const QString &arg)
{
    return arg == QLatin1String("-p") || arg == QLatin1String("--profile") ||
           arg == QLatin1String("-l") || arg == QLatin1String("--log-level") ||
           arg == QLatin1String("-L") || arg == QLatin1String("--log-type");
}

bool
isHelpToken(const QString &arg)
{
    return arg == QLatin1String("--help") || arg == QLatin1String("-h");
}

// Find the position (0-based index into argv) of the group name. The caller
// already knows which group we're dispatching, so scanning for the literal
// group name is safe: subcommands and positionals can't be named the same as
// top-level command groups.
int
findGroupIndex(int argc, char *argv[], const QString &groupName)
{
    for (int i = 1; i < argc; ++i) {
        if (isTopLevelValueOption(QString{argv[i]})) {
            ++i;
            continue;
        }
        if (QString{argv[i]} == groupName)
            return i;
    }
    return -1;
}

struct WalkResult
{
    bool ok                     = true;
    bool helpRequested          = false;
    const SubcommandDef *target = nullptr;
    QStringList subcommandPath;
    ParsedArgs parsed;
    QString errorMessage;
};

WalkResult
walkAndParse(const GroupDef &group, int startIndex, int argc, char *argv[])
{
    WalkResult r;
    r.subcommandPath.append(group.name);

    const QList<SubcommandDef> *currentSubcommands = &group.subcommands;
    const SubcommandDef *current                   = nullptr; // leaf tracker

    int i = startIndex + 1;
    while (i < argc) {
        QString arg{argv[i]};

        if (isHelpToken(arg)) {
            r.helpRequested = true;
            return r;
        }

        // Top-level options that might appear after the group name.
        if (isTopLevelValueOption(arg)) {
            i += 2;
            continue;
        }

        if (looksLikeFlag(arg))
            break; // flags belong to the leaf subcommand; parse below

        // Positional: first try to descend into a nested subcommand.
        if (!currentSubcommands->isEmpty()) {
            if (const auto *sub = findSubcommand(*currentSubcommands, arg)) {
                current = sub;
                r.subcommandPath.append(sub->name);
                currentSubcommands = &sub->subcommands;
                ++i;
                continue;
            }

            // At a level that expects a subcommand but the positional doesn't
            // match any known one. If we haven't chosen a subcommand yet, or
            // the current subcommand also expects a nested subcommand (and has
            // no positionals of its own), this is an unknown-subcommand error.
            const bool expectsSubcommand =
              !current || (current->positionals.isEmpty() && !current->subcommands.isEmpty());
            if (expectsSubcommand) {
                r.ok           = false;
                r.errorMessage = QStringLiteral("Unknown subcommand: ") + arg;
                // Surface `current` as the target so the error message renders
                // the *nested* subcommand listing (e.g. `settings ui <sub>`),
                // not the top-level group listing.
                r.target = current;
                return r;
            }
        }

        // Not a subcommand — must be a positional for the current target.
        break;
    }

    if (!current) {
        // No subcommand selected; caller will print group help or flag an error.
        return r;
    }

    r.target                = current;
    r.parsed.subcommandPath = r.subcommandPath;

    // Now parse flags and positionals for the leaf subcommand.
    while (i < argc) {
        QString arg{argv[i]};

        if (isHelpToken(arg)) {
            r.helpRequested = true;
            return r;
        }

        if (isTopLevelValueOption(arg)) {
            i += 2;
            continue;
        }

        if (looksLikeFlag(arg)) {
            // Split --flag=value early.
            QString flagName = arg;
            QString inlineVal;
            bool haveInline = false;
            int eq          = arg.indexOf(QLatin1Char('='));
            if (eq > 0) {
                flagName   = arg.left(eq);
                inlineVal  = arg.mid(eq + 1);
                haveInline = true;
            }

            const auto *flag = findFlag(current->flags, flagName);
            if (!flag) {
                r.ok           = false;
                r.errorMessage = QStringLiteral("Unknown flag: ") + arg;
                return r;
            }

            if (!flag->takesValue) {
                if (haveInline) {
                    r.ok           = false;
                    r.errorMessage = QStringLiteral("Flag %1 takes no value").arg(flag->longName);
                    return r;
                }
                r.parsed.flagsPresent.insert(flag->longName);
                ++i;
                continue;
            }

            QString value;
            if (haveInline) {
                value = inlineVal;
            } else {
                if (i + 1 >= argc) {
                    r.ok           = false;
                    r.errorMessage = QStringLiteral("Flag %1 requires a value").arg(flag->longName);
                    return r;
                }
                value = QString{argv[i + 1]};
                ++i;
            }

            if (!flag->valueEnum.isEmpty() && !flag->valueEnum.contains(value)) {
                r.ok = false;
                r.errorMessage =
                  QStringLiteral("Invalid value for %1: %2 (expected one of: %3)")
                    .arg(flag->longName, value, flag->valueEnum.join(QStringLiteral(", ")));
                return r;
            }

            r.parsed.flagValues.insert(flag->longName, value);
            ++i;
            continue;
        }

        // Positional.
        r.parsed.positionals.append(arg);
        ++i;
    }

    // Validate positional count. For a variadic final positional we demand at
    // least min(required) args (variadic implies at least one match unless
    // also optional).
    int required = 0;
    for (const auto &pos : current->positionals)
        if (!pos.optional)
            ++required;

    if (r.parsed.positionals.size() < required) {
        r.ok           = false;
        r.errorMessage = QStringLiteral("Missing required arguments for: komai %1")
                           .arg(r.subcommandPath.join(QLatin1Char(' ')));
        return r;
    }

    // Reject trailing extras when the final positional isn't variadic. Without
    // this, `komai foo subcmd extra-arg` would silently accept `extra-arg`
    // when `subcmd` has no (or fewer) declared positionals — masking user typos.
    const bool hasVariadic =
      !current->positionals.isEmpty() && current->positionals.last().variadic;
    if (!hasVariadic && r.parsed.positionals.size() > current->positionals.size()) {
        r.ok           = false;
        r.errorMessage = QStringLiteral("Unexpected positional argument for: komai %1")
                           .arg(r.subcommandPath.join(QLatin1Char(' ')));
        r.target = current;
        return r;
    }

    return r;
}

int
runSubcommand(const SubcommandDef &sub,
              const ParsedArgs &parsed,
              int argc,
              char *argv[],
              QCoreApplication &app)
{
    if (sub.rawHandler)
        return sub.rawHandler(argc, argv, app);

    if (!sub.handler) {
        std::cerr << "Error: subcommand '" << sub.name.toStdString()
                  << "' has no handler (internal bug)\n";
        return 1;
    }

    if (sub.requiresProfile) {
        if (!cli_ipc::ensureConnected(parsed.profileId))
            return 1;
    }

    return sub.handler(parsed, app);
}

} // namespace

int
dispatchGroup(const GroupDef &group, int argc, char *argv[], QCoreApplication &app)
{
    const int groupIndex = findGroupIndex(argc, argv, group.name);
    if (groupIndex < 0) {
        // CliDispatch wouldn't have called us if the group wasn't in argv,
        // but guard anyway.
        std::cerr << "Error: internal dispatch mismatch for group '" << group.name.toStdString()
                  << "'\n";
        return 1;
    }

    auto walk = walkAndParse(group, groupIndex, argc, argv);

    if (!walk.ok) {
        std::cerr << "Error: " << walk.errorMessage.toStdString() << "\n\n";
        if (walk.target) {
            std::cerr << renderSubcommandHelp(group, walk.subcommandPath).toStdString();
        } else {
            std::cerr << renderGroupHelp(group).toStdString();
        }
        return 1;
    }

    if (walk.helpRequested) {
        if (walk.subcommandPath.size() > 1)
            std::cout << renderSubcommandHelp(group, walk.subcommandPath).toStdString();
        else
            std::cout << renderGroupHelp(group).toStdString();
        return 0;
    }

    if (!walk.target) {
        // No subcommand specified — print group help, exit 1 (mirrors current behaviour).
        std::cout << renderGroupHelp(group).toStdString();
        return 1;
    }

    // If the matched subcommand has nested subcommands but the user didn't
    // descend into one, treat as "no subcommand given" for that level.
    if (!walk.target->subcommands.isEmpty() && !walk.target->handler && !walk.target->rawHandler) {
        std::cout << renderSubcommandHelp(group, walk.subcommandPath).toStdString();
        return 1;
    }

    walk.parsed.profileId = cli_ipc::profileFromArgs(argc, argv);
    return runSubcommand(*walk.target, walk.parsed, argc, argv, app);
}

} // namespace cli_schema

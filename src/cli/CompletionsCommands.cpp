// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CompletionsCommands.h"

#include <iostream>

#include <QCoreApplication>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include "AppCommands.h"
#include "McpCommands.h"
#include "MediaCommands.h"
#include "ProfileCommands.h"
#include "RoomCommands.h"
#include "SettingsCommands.h"
#include "ThemeCommands.h"
#include "UserCommands.h"
#include "schema/Dispatcher.h"

namespace {

using cli_schema::FlagDef;
using cli_schema::GroupDef;
using cli_schema::ParsedArgs;
using cli_schema::SubcommandDef;

// Walk a subcommand tree and collect every flag whose `takesValue` is true.
// Used by shell snippets so they know which flags consume the next token.
void
collectValueFlagsFromSub(const SubcommandDef &sub, QSet<QString> &out)
{
    for (const auto &f : sub.flags) {
        if (f.takesValue) {
            out.insert(f.longName);
            if (!f.shortName.isEmpty())
                out.insert(f.shortName);
        }
    }
    for (const auto &s : sub.subcommands)
        collectValueFlagsFromSub(s, out);
}

QStringList
valueTakingFlags(const QList<GroupDef> &groups)
{
    QSet<QString> set;
    // Top-level Qt options (these still consume a following value).
    set << QStringLiteral("-p") << QStringLiteral("--profile") << QStringLiteral("-l")
        << QStringLiteral("--log-level") << QStringLiteral("-L") << QStringLiteral("--log-type");

    for (const auto &g : groups)
        for (const auto &s : g.subcommands)
            collectValueFlagsFromSub(s, set);

    QStringList list(set.begin(), set.end());
    list.sort();
    return list;
}

// Returns all positional "path" strings that lead to actionable subcommands,
// paired with the flag/subcommand tokens to offer as completions at that path.
// Example entries:
//   {path: ""}              -> offers top-level groups + top-level options
//   {path: "app"}           -> offers "version api-version"
//   {path: "settings ui"}   -> offers "theme set-theme"
//   {path: "rooms send"}    -> offers "--msgtype --format"
struct PathOffer
{
    QStringList path;       // positional tokens from argv[1..] that lead here
    QStringList candidates; // tokens to offer (subcommand names and/or flag names)
    bool isLeaf = false;    // true iff the node at `path` has no nested subcommands
};

// Build one offer row per node in the tree, rooted at `group`.
void
buildPathOffers(const GroupDef &group, QList<PathOffer> &out)
{
    // Root-of-group offer: inside "<group> ..."
    // For a group with only nested subcommands (no direct handlers), we also
    // offer its subcommand names here. That's fine — the dispatcher treats
    // no-subcommand-given as help.
    const std::function<void(const SubcommandDef &, QStringList)> visit =
      [&](const SubcommandDef &sub, QStringList path) {
          path.append(sub.name);

          QStringList candidates;
          for (const auto &s : sub.subcommands)
              candidates.append(s.name);
          for (const auto &f : sub.flags) {
              candidates.append(f.longName);
              if (!f.shortName.isEmpty())
                  candidates.append(f.shortName);
          }
          candidates.append(QStringLiteral("--help"));
          candidates.append(QStringLiteral("-h"));
          candidates.removeDuplicates();
          candidates.sort();

          PathOffer offer;
          offer.path       = path;
          offer.candidates = candidates;
          offer.isLeaf     = sub.subcommands.isEmpty();
          out.append(offer);

          for (const auto &s : sub.subcommands)
              visit(s, path);
      };

    // Group-level offer (positional path = [group.name])
    QStringList groupCandidates;
    for (const auto &s : group.subcommands)
        groupCandidates.append(s.name);
    groupCandidates.append(QStringLiteral("--help"));
    groupCandidates.append(QStringLiteral("-h"));
    groupCandidates.sort();

    PathOffer groupOffer;
    groupOffer.path       = {group.name};
    groupOffer.candidates = groupCandidates;
    groupOffer.isLeaf     = false; // groups always have children
    out.append(groupOffer);

    for (const auto &s : group.subcommands)
        visit(s, {group.name});
}

// Offers at the top level (before any group name is typed).
PathOffer
topLevelOffer(const QList<GroupDef> &groups)
{
    PathOffer offer;
    offer.path = {};
    for (const auto &g : groups)
        offer.candidates.append(g.name);
    offer.candidates.append(QStringLiteral("--help"));
    offer.candidates.append(QStringLiteral("-h"));
    offer.candidates.append(QStringLiteral("--version"));
    offer.candidates.append(QStringLiteral("-v"));
    offer.candidates.append(QStringLiteral("--debug"));
    offer.candidates.append(QStringLiteral("-p"));
    offer.candidates.append(QStringLiteral("--profile"));
    offer.candidates.append(QStringLiteral("-l"));
    offer.candidates.append(QStringLiteral("--log-level"));
    offer.candidates.append(QStringLiteral("-L"));
    offer.candidates.append(QStringLiteral("--log-type"));
    offer.candidates.sort();
    return offer;
}

QList<PathOffer>
buildAllOffers(const QList<GroupDef> &groups)
{
    QList<PathOffer> offers;
    offers.append(topLevelOffer(groups));
    for (const auto &g : groups)
        buildPathOffers(g, offers);
    return offers;
}

// Collect enum flags (name -> allowed values) across the tree so shells can
// offer value candidates after `--flag `.
using EnumFlagMap = QHash<QString, QStringList>;

void
collectEnumFlagsFromSub(const SubcommandDef &sub, EnumFlagMap &out)
{
    for (const auto &f : sub.flags) {
        if (f.takesValue && !f.valueEnum.isEmpty()) {
            out.insert(f.longName, f.valueEnum);
            if (!f.shortName.isEmpty())
                out.insert(f.shortName, f.valueEnum);
        }
    }
    for (const auto &s : sub.subcommands)
        collectEnumFlagsFromSub(s, out);
}

EnumFlagMap
allEnumFlags(const QList<GroupDef> &groups)
{
    EnumFlagMap out;
    for (const auto &g : groups)
        for (const auto &s : g.subcommands)
            collectEnumFlagsFromSub(s, out);
    return out;
}

int
handleBash(const ParsedArgs & /*parsed*/, QCoreApplication & /*app*/)
{
    std::cout << completions::renderBash(allCliGroupDefs()).toStdString();
    return 0;
}

int
handleZsh(const ParsedArgs & /*parsed*/, QCoreApplication & /*app*/)
{
    std::cout << completions::renderZsh(allCliGroupDefs()).toStdString();
    return 0;
}

int
handleFish(const ParsedArgs & /*parsed*/, QCoreApplication & /*app*/)
{
    std::cout << completions::renderFish(allCliGroupDefs()).toStdString();
    return 0;
}

} // namespace

// ---- Group-def registry & schema ------------------------------------------------

QList<GroupDef>
allCliGroupDefs()
{
    return {
      appGroupDef(),
      completionsGroupDef(),
      mediaGroupDef(),
      mcpGroupDef(),
      profilesGroupDef(),
      roomsGroupDef(),
      settingsGroupDef(),
      themeGroupDef(),
      userGroupDef(),
    };
}

GroupDef
completionsGroupDef()
{
    GroupDef group;
    group.name = QStringLiteral("completions");
    group.help = QStringLiteral("Generate shell completion scripts (offline)");
    group.longHelp =
      QStringLiteral("Emit a completion script for the requested shell on standard output.");

    auto makeSub = [](const QString &name,
                      const QString &help,
                      std::function<int(const ParsedArgs &, QCoreApplication &)> h) {
        SubcommandDef sub;
        sub.name            = name;
        sub.help            = help;
        sub.requiresProfile = false;
        sub.handler         = std::move(h);
        return sub;
    };

    group.subcommands.append(makeSub(
      QStringLiteral("bash"), QStringLiteral("Print a bash completion script"), handleBash));
    group.subcommands.append(
      makeSub(QStringLiteral("zsh"), QStringLiteral("Print a zsh completion script"), handleZsh));
    group.subcommands.append(makeSub(
      QStringLiteral("fish"), QStringLiteral("Print a fish completion script"), handleFish));

    return group;
}

int
runCompletionsCommand(int argc, char *argv[], QCoreApplication &app)
{
    return cli_schema::dispatchGroup(completionsGroupDef(), argc, argv, app);
}

// ---- Bash renderer --------------------------------------------------------------

namespace completions {

QString
renderBash(const QList<GroupDef> &groups)
{
    const auto offers   = buildAllOffers(groups);
    const auto valFlags = valueTakingFlags(groups);
    const auto enums    = allEnumFlags(groups);

    QString out;
    QTextStream s(&out);

    s << "# bash completion for komai (auto-generated; do not edit)\n";
    s << "_komai() {\n";
    s << "    local cur prev\n";
    s << "    cur=\"${COMP_WORDS[COMP_CWORD]}\"\n";
    s << "    prev=\"${COMP_WORDS[COMP_CWORD-1]}\"\n";
    s << "\n";

    // Enum-value completion when previous token is an enum flag.
    if (!enums.isEmpty()) {
        s << "    case \"${prev}\" in\n";
        QStringList keys(enums.keys());
        keys.sort();
        for (const auto &k : keys) {
            s << "        " << k << ") COMPREPLY=($(compgen -W \""
              << enums.value(k).join(QLatin1Char(' ')) << "\" -- \"${cur}\")); return ;;\n";
        }
        s << "    esac\n\n";
    }

    // For non-enum value-taking flags, fall through to default bash completion
    // (filename) by just returning empty COMPREPLY. We'll rely on bash's
    // default behaviour by clearing completion context.
    s << "    case \"${prev}\" in\n";
    for (const auto &flag : valFlags) {
        if (!enums.contains(flag))
            s << "        " << flag << ") return ;;\n";
    }
    s << "    esac\n\n";

    // Walk COMP_WORDS to find the positional path (skipping flags and flag values).
    s << "    local -a _komai_value_flags=(" << valFlags.join(QLatin1Char(' ')) << ")\n";
    s << "    local -a _komai_path=()\n";
    s << "    local _skip=0 _i\n";
    s << "    for ((_i=1; _i<COMP_CWORD; _i++)); do\n";
    s << "        local _w=\"${COMP_WORDS[_i]}\"\n";
    s << "        if (( _skip )); then _skip=0; continue; fi\n";
    s << "        local _f\n";
    s << "        for _f in \"${_komai_value_flags[@]}\"; do\n";
    s << "            if [[ \"${_w}\" == \"${_f}\" ]]; then _skip=1; break; fi\n";
    s << "        done\n";
    s << "        (( _skip )) && continue\n";
    s << "        [[ \"${_w}\" == -* ]] && continue\n";
    s << "        _komai_path+=(\"${_w}\")\n";
    s << "    done\n\n";

    s << "    local _key=\"${_komai_path[*]}\"\n";
    s << "    case \"${_key}\" in\n";
    // Emit deepest paths first. Each non-empty path uses a double pattern
    // "<path>"|"<path> "* so trailing positional arguments (`rooms timeline !x:y`)
    // still match the `rooms timeline` entry.
    auto sortedOffers = offers;
    std::sort(sortedOffers.begin(), sortedOffers.end(), [](const PathOffer &a, const PathOffer &b) {
        return a.path.size() > b.path.size();
    });
    for (const auto &offer : sortedOffers) {
        if (offer.path.isEmpty()) {
            s << "        \"\") COMPREPLY=($(compgen -W \""
              << offer.candidates.join(QLatin1Char(' ')) << "\" -- \"${cur}\")) ;;\n";
        } else {
            const auto key = offer.path.join(QLatin1Char(' '));
            s << "        \"" << key << "\"|\"" << key << " \"*) COMPREPLY=($(compgen -W \""
              << offer.candidates.join(QLatin1Char(' ')) << "\" -- \"${cur}\")) ;;\n";
        }
    }
    s << "    esac\n";
    s << "}\n";
    s << "complete -F _komai komai\n";

    return out;
}

// ---- Zsh renderer ---------------------------------------------------------------

static QString
zshQuote(const QString &s)
{
    QString out = s;
    out.replace(QLatin1String("\\"), QLatin1String("\\\\"));
    out.replace(QLatin1String("'"), QLatin1String("'\\''"));
    return QStringLiteral("'") + out + QStringLiteral("'");
}

QString
renderZsh(const QList<GroupDef> &groups)
{
    const auto offers   = buildAllOffers(groups);
    const auto valFlags = valueTakingFlags(groups);
    const auto enums    = allEnumFlags(groups);

    QString out;
    QTextStream s(&out);

    s << "#compdef komai\n";
    s << "# zsh completion for komai (auto-generated; do not edit)\n";
    s << "\n";
    s << "_komai() {\n";
    s << "    local -a _komai_value_flags=(" << valFlags.join(QLatin1Char(' ')) << ")\n";
    s << "    local cur prev\n";
    s << "    cur=\"${words[CURRENT]}\"\n";
    s << "    prev=\"${words[CURRENT-1]}\"\n";
    s << "\n";

    // Enum-value completion
    if (!enums.isEmpty()) {
        s << "    case \"${prev}\" in\n";
        QStringList keys(enums.keys());
        keys.sort();
        for (const auto &k : keys) {
            s << "        " << k << ") _values \"" << k.mid(2) << "\" "
              << enums.value(k).join(QLatin1Char(' ')) << "; return ;;\n";
        }
        s << "    esac\n\n";
    }

    // Walk for positional path
    s << "    local -a _komai_path=()\n";
    s << "    local _skip=0 _i\n";
    s << "    for ((_i=2; _i<CURRENT; _i++)); do\n";
    s << "        local _w=\"${words[_i]}\"\n";
    s << "        if [[ $_skip -eq 1 ]]; then _skip=0; continue; fi\n";
    s << "        local _is_value_flag=0 _f\n";
    s << "        for _f in \"${_komai_value_flags[@]}\"; do\n";
    s << "            if [[ \"${_w}\" == \"${_f}\" ]]; then _is_value_flag=1; break; fi\n";
    s << "        done\n";
    s << "        if [[ $_is_value_flag -eq 1 ]]; then _skip=1; continue; fi\n";
    s << "        [[ \"${_w}\" == -* ]] && continue\n";
    s << "        _komai_path+=(\"${_w}\")\n";
    s << "    done\n\n";

    s << "    local _key=\"${_komai_path[*]}\"\n";
    s << "    case \"${_key}\" in\n";
    auto zshSortedOffers = offers;
    std::sort(zshSortedOffers.begin(),
              zshSortedOffers.end(),
              [](const PathOffer &a, const PathOffer &b) { return a.path.size() > b.path.size(); });
    for (const auto &offer : zshSortedOffers) {
        if (offer.path.isEmpty()) {
            s << "        '') compadd -- " << offer.candidates.join(QLatin1Char(' ')) << " ;;\n";
        } else {
            const auto key = offer.path.join(QLatin1Char(' '));
            s << "        " << zshQuote(key) << " | " << zshQuote(key + QStringLiteral(" *"))
              << ") compadd -- " << offer.candidates.join(QLatin1Char(' ')) << " ;;\n";
        }
    }
    s << "    esac\n";
    s << "}\n";
    s << "\n";
    s << "_komai \"$@\"\n";

    return out;
}

// ---- Fish renderer --------------------------------------------------------------

static QString
fishEscape(const QString &s)
{
    QString out = s;
    out.replace(QLatin1String("\\"), QLatin1String("\\\\"));
    out.replace(QLatin1String("'"), QLatin1String("\\'"));
    return out;
}

QString
renderFish(const QList<GroupDef> &groups)
{
    const auto offers   = buildAllOffers(groups);
    const auto valFlags = valueTakingFlags(groups);
    const auto enums    = allEnumFlags(groups);

    QString out;
    QTextStream s(&out);

    s << "# fish completion for komai (auto-generated; do not edit)\n";
    s << "\n";

    // Helper: emit positional tokens (one per line) from the current command
    // line, skipping flags and the values of value-taking flags. Fish's
    // `(cmd)` substitution splits on newlines, so emitting one-per-line is
    // what makes `count (__komai_positionals)` match the declared path length.
    s << "function __komai_positionals\n";
    s << "    set -l value_flags " << valFlags.join(QLatin1Char(' ')) << "\n";
    s << "    set -l tokens (commandline -opc)\n";
    s << "    set -l skip_next 0\n";
    s << "    set -l first 1\n";
    s << "    for token in $tokens\n";
    s << "        if test $first -eq 1\n";
    s << "            set first 0\n";
    s << "            continue\n";
    s << "        end\n";
    s << "        if test $skip_next -eq 1\n";
    s << "            set skip_next 0\n";
    s << "            continue\n";
    s << "        end\n";
    s << "        if contains -- $token $value_flags\n";
    s << "            set skip_next 1\n";
    s << "            continue\n";
    s << "        end\n";
    s << "        if string match -q -- '-*' $token\n";
    s << "            continue\n";
    s << "        end\n";
    s << "        echo $token\n";
    s << "    end\n";
    s << "end\n";
    s << "\n";
    s << "function __komai_at_path\n";
    s << "    # Exact positional match: used for subcommand-name completion at a\n";
    s << "    # given level. Fires only when the user has typed exactly `$argv`.\n";
    s << "    set -l got (__komai_positionals)\n";
    s << "    set -l want $argv\n";
    s << "    if test (count $got) -ne (count $want)\n";
    s << "        return 1\n";
    s << "    end\n";
    s << "    for i in (seq (count $want))\n";
    s << "        if test \"$got[$i]\" != \"$want[$i]\"\n";
    s << "            return 1\n";
    s << "        end\n";
    s << "    end\n";
    s << "    return 0\n";
    s << "end\n";
    s << "\n";
    s << "function __komai_within\n";
    s << "    # Prefix match: used for flag completion inside a leaf subcommand.\n";
    s << "    # Fires whenever the positional path starts with `$argv` (inclusive).\n";
    s << "    set -l got (__komai_positionals)\n";
    s << "    set -l want $argv\n";
    s << "    if test (count $got) -lt (count $want)\n";
    s << "        return 1\n";
    s << "    end\n";
    s << "    for i in (seq (count $want))\n";
    s << "        if test \"$got[$i]\" != \"$want[$i]\"\n";
    s << "            return 1\n";
    s << "        end\n";
    s << "    end\n";
    s << "    return 0\n";
    s << "end\n";
    s << "\n";

    // Disable default (filename) completion globally; value-taking flags with
    // no enum will fall through to fish's default behaviour (files).
    s << "complete -c komai -f\n";
    s << "\n";

    // Path-contextual subcommand/flag completions.
    for (const auto &offer : offers) {
        QString cond;
        const auto pathTail = offer.path.join(QLatin1Char(' '));
        if (offer.isLeaf) {
            // Leaf: fire for any positional path that starts with this leaf's path.
            cond = offer.path.isEmpty() ? QStringLiteral("__komai_within")
                                        : QStringLiteral("__komai_within ") + pathTail;
        } else {
            // Non-leaf: fire only when the user is exactly at this level.
            cond = offer.path.isEmpty() ? QStringLiteral("__komai_at_path")
                                        : QStringLiteral("__komai_at_path ") + pathTail;
        }

        for (const auto &cand : offer.candidates) {
            if (cand.isEmpty())
                continue;
            if (cand.startsWith(QLatin1String("--"))) {
                const auto name = cand.mid(2);
                s << "complete -c komai -n '" << fishEscape(cond) << "' -l " << fishEscape(name);
                if (enums.contains(cand)) {
                    // -xa combines "takes a required non-file value" with
                    // "here's the allowed set". With `-x` alone fish treats the
                    // value slot as private and ignores subsequent `-a` rules,
                    // so enum values must be on the flag's own `complete` line.
                    s << " -xa " << zshQuote(enums.value(cand).join(QLatin1Char(' ')));
                }
                s << "\n";
            } else if (cand.startsWith(QLatin1Char('-')) && cand.size() == 2) {
                const auto name = cand.mid(1);
                s << "complete -c komai -n '" << fishEscape(cond) << "' -s " << fishEscape(name);
                if (enums.contains(cand))
                    s << " -xa " << zshQuote(enums.value(cand).join(QLatin1Char(' ')));
                s << "\n";
            } else {
                s << "complete -c komai -n '" << fishEscape(cond) << "' -a " << zshQuote(cand)
                  << "\n";
            }
        }
    }

    return out;
}

} // namespace completions

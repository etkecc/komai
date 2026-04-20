// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "HelpRenderer.h"

#include <algorithm>

#include <QStringList>
#include <QTextStream>

namespace cli_schema {

namespace {

QString
formatUsagePositional(const PositionalDef &pos)
{
    QString token = QStringLiteral("<") + pos.name + QStringLiteral(">");
    if (pos.variadic)
        token.append(QStringLiteral("..."));
    if (pos.optional)
        return QStringLiteral("[") + token + QStringLiteral("]");
    return token;
}

QString
formatFlagValueHint(const FlagDef &flag)
{
    if (!flag.takesValue)
        return {};
    if (!flag.valueEnum.isEmpty())
        return flag.valueEnum.join(QLatin1Char('|'));
    if (!flag.valuePlaceholder.isEmpty())
        return flag.valuePlaceholder;
    return QStringLiteral("<value>");
}

QString
formatUsageFlag(const FlagDef &flag)
{
    if (!flag.takesValue)
        return QStringLiteral("[") + flag.longName + QStringLiteral("]");
    return QStringLiteral("[") + flag.longName + QLatin1Char(' ') + formatFlagValueHint(flag) +
           QStringLiteral("]");
}

QString
usageLine(const QStringList &pathTokens,
          const QList<PositionalDef> &positionals,
          const QList<FlagDef> &flags,
          bool hasNestedSubcommands)
{
    QString usage = QStringLiteral("Usage: komai");
    for (const auto &tok : pathTokens)
        usage += QLatin1Char(' ') + tok;

    for (const auto &pos : positionals)
        usage += QLatin1Char(' ') + formatUsagePositional(pos);

    for (const auto &flag : flags)
        usage += QLatin1Char(' ') + formatUsageFlag(flag);

    if (hasNestedSubcommands)
        usage += QStringLiteral(" <subcommand> [args...]");

    return usage;
}

QString
formatSubcommandSummary(const SubcommandDef &sub)
{
    QString head = sub.name;
    for (const auto &pos : sub.positionals)
        head += QLatin1Char(' ') + formatUsagePositional(pos);
    return head;
}

void
appendSubcommandListing(QTextStream &out, const QList<SubcommandDef> &subs)
{
    out << "Subcommands:\n";
    int headColWidth = 0;
    for (const auto &sub : subs) {
        const auto head = formatSubcommandSummary(sub);
        headColWidth    = std::max(headColWidth, static_cast<int>(head.size()));
    }
    headColWidth += 2; // padding between head and help

    for (const auto &sub : subs) {
        const auto head = formatSubcommandSummary(sub);
        out << "  " << head;
        if (!sub.help.isEmpty()) {
            const int pad = headColWidth - static_cast<int>(head.size());
            out << QString(pad, QLatin1Char(' ')) << sub.help;
        }
        out << "\n";
    }
}

void
appendFlagListing(QTextStream &out, const QList<FlagDef> &flags)
{
    if (flags.isEmpty())
        return;
    out << "\nOptions:\n";
    for (const auto &flag : flags) {
        QString head;
        if (!flag.shortName.isEmpty())
            head = flag.shortName + QStringLiteral(", ") + flag.longName;
        else
            head = QStringLiteral("    ") + flag.longName;
        if (flag.takesValue) {
            head += QLatin1Char(' ');
            head += formatFlagValueHint(flag);
        }
        out << "  " << head;
        if (!flag.help.isEmpty()) {
            out << "\n        " << flag.help;
            if (!flag.defaultValue.isEmpty())
                out << " (default: " << flag.defaultValue << ")";
        }
        out << "\n";
    }
}

} // namespace

QString
renderGroupHelp(const GroupDef &group)
{
    QString out;
    QTextStream s(&out);

    s << usageLine({QStringLiteral("[-p <profile>]"), group.name}, {}, {}, true) << "\n";

    if (!group.longHelp.isEmpty())
        s << "\n" << group.longHelp << "\n";

    s << "\n";
    appendSubcommandListing(s, group.subcommands);
    return out;
}

QString
renderSubcommandHelp(const GroupDef &group, const QStringList &subcommandPath)
{
    QString out;
    QTextStream s(&out);

    // Walk to the target subcommand while recording path tokens for Usage:.
    QStringList pathTokens{QStringLiteral("[-p <profile>]"), group.name};
    const QList<SubcommandDef> *subs = &group.subcommands;
    const SubcommandDef *target      = nullptr;

    for (int i = 1; i < subcommandPath.size(); ++i) {
        const auto &name           = subcommandPath.at(i);
        const SubcommandDef *found = nullptr;
        for (const auto &sub : *subs) {
            if (sub.name == name) {
                found = &sub;
                break;
            }
        }
        if (!found) {
            // Shouldn't happen if the caller honours the contract; fall back to
            // the group help.
            return renderGroupHelp(group);
        }
        pathTokens.append(found->name);
        target = found;
        subs   = &found->subcommands;
    }

    if (!target) {
        return renderGroupHelp(group);
    }

    s << usageLine(pathTokens, target->positionals, target->flags, !target->subcommands.isEmpty())
      << "\n";

    if (!target->longHelp.isEmpty())
        s << "\n" << target->longHelp << "\n";

    if (!target->subcommands.isEmpty()) {
        s << "\n";
        appendSubcommandListing(s, target->subcommands);
    }

    appendFlagListing(s, target->flags);

    return out;
}

} // namespace cli_schema

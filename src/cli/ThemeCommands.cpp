// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThemeCommands.h"

#include <QCoreApplication>

#include "schema/Dispatcher.h"
#include "schema/SchemaTypes.h"
#include "theme/ThemeCommandHandlers.h"

namespace {

cli_schema::GroupDef
themeGroup()
{
    cli_schema::GroupDef group;
    group.name = QStringLiteral("theme");
    group.help = QStringLiteral("Theme file management (offline)");

    // list
    cli_schema::SubcommandDef list;
    list.name            = QStringLiteral("list");
    list.help            = QStringLiteral("List all available themes");
    list.requiresProfile = false;
    list.handler         = theme_command::handleList;
    group.subcommands.append(list);

    // tinted-search [query]
    cli_schema::SubcommandDef tintedSearch;
    tintedSearch.name            = QStringLiteral("tinted-search");
    tintedSearch.help            = QStringLiteral("Search available Base16 themes");
    tintedSearch.requiresProfile = false;
    cli_schema::PositionalDef query;
    query.name     = QStringLiteral("query");
    query.help     = QStringLiteral("Case-insensitive substring match.");
    query.optional = true;
    tintedSearch.positionals.append(query);
    tintedSearch.handler = theme_command::handleTintedSearch;
    group.subcommands.append(tintedSearch);

    // tinted-import <slug> [name] [--force] [--variant light|dark]
    cli_schema::SubcommandDef tintedImport;
    tintedImport.name            = QStringLiteral("tinted-import");
    tintedImport.help            = QStringLiteral("Import a Base16 theme from tinted-theming");
    tintedImport.requiresProfile = false;

    cli_schema::PositionalDef slug;
    slug.name = QStringLiteral("slug");
    tintedImport.positionals.append(slug);

    cli_schema::PositionalDef name;
    name.name     = QStringLiteral("name");
    name.help     = QStringLiteral("Optional filename override (default: slug).");
    name.optional = true;
    tintedImport.positionals.append(name);

    cli_schema::FlagDef force;
    force.longName = QStringLiteral("--force");
    force.help     = QStringLiteral("Overwrite an existing theme file.");
    tintedImport.flags.append(force);

    cli_schema::FlagDef variant;
    variant.longName   = QStringLiteral("--variant");
    variant.takesValue = true;
    variant.valueEnum  = {QStringLiteral("light"), QStringLiteral("dark")};
    variant.help       = QStringLiteral("Override the automatic light/dark detection.");
    tintedImport.flags.append(variant);

    tintedImport.handler = theme_command::handleTintedImport;
    group.subcommands.append(tintedImport);

    // create-sample <variant> <name> [--force]
    cli_schema::SubcommandDef createSample;
    createSample.name            = QStringLiteral("create-sample");
    createSample.help            = QStringLiteral("Create a starter theme YAML");
    createSample.requiresProfile = false;

    cli_schema::PositionalDef variantPos;
    variantPos.name = QStringLiteral("variant");
    variantPos.help = QStringLiteral("One of: light, dark.");
    createSample.positionals.append(variantPos);

    cli_schema::PositionalDef samplePath;
    samplePath.name = QStringLiteral("name");
    createSample.positionals.append(samplePath);

    cli_schema::FlagDef forceSample;
    forceSample.longName = QStringLiteral("--force");
    forceSample.help     = QStringLiteral("Overwrite an existing sample theme file.");
    createSample.flags.append(forceSample);

    createSample.handler = theme_command::handleCreateSample;
    group.subcommands.append(createSample);

    return group;
}

} // namespace

int
runThemeCommand(int argc, char *argv[], QCoreApplication &app)
{
    return cli_schema::dispatchGroup(themeGroup(), argc, argv, app);
}

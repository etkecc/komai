// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string>

#include "cli/McpCommands.h"
#include "cli/schema/SchemaTypes.h"

static int failures = 0;

static bool
expect(bool condition, const char *message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    ++failures;
    return false;
}

static void
testBuildPopulatesDefaultAccessMode()
{
    cli_schema::ParsedArgs parsed;
    parsed.profileId = QStringLiteral("work");

    const auto command = mcp_commands::buildServeCommand(parsed);

    expect(command.profileId == QLatin1String("work"),
           "provided profile id round-trips into ServeCommand");
    expect(command.accessMode == QLatin1String("read_only"),
           "default access mode is read_only when --access is absent");
}

static void
testBuildReadsAccessFlag()
{
    cli_schema::ParsedArgs parsed;
    parsed.profileId = QStringLiteral("work");
    parsed.flagValues.insert(QStringLiteral("--access"), QStringLiteral("read_write"));

    const auto command = mcp_commands::buildServeCommand(parsed);

    expect(command.accessMode == QLatin1String("read_write"),
           "--access read_write flows through to ServeCommand");
}

static void
testBuildNormalizesEmptyProfileToDefault()
{
    cli_schema::ParsedArgs parsed;
    // profileId left empty — matches what the dispatcher hands us for `komai mcp serve`
    // without `-p`.

    const auto command = mcp_commands::buildServeCommand(parsed);

    expect(command.profileId == QLatin1String("default"),
           "empty profile id is normalised to 'default'");
}

static void
testChildArgumentsWireFormat()
{
    mcp_commands::ServeCommand command;
    command.profileId  = QStringLiteral("work");
    command.accessMode = QStringLiteral("read_write");

    const auto child = command.childArguments();
    expect(child == QStringList({QStringLiteral("serve"),
                                 QStringLiteral("--profile"),
                                 QStringLiteral("work"),
                                 QStringLiteral("--access"),
                                 QStringLiteral("read_write")}),
           "childArguments() emits the expected komai-mcp argv");
}

int
main()
{
    testBuildPopulatesDefaultAccessMode();
    testBuildReadsAccessFlag();
    testBuildNormalizesEmptyProfileToDefault();
    testChildArgumentsWireFormat();

    if (failures != 0)
        std::cerr << failures << " test(s) failed.\n";
    return failures == 0 ? 0 : 1;
}

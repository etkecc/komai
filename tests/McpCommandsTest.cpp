// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string>
#include <vector>

#include "cli/McpCommands.h"

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

struct ArgvBuilder
{
    std::vector<std::string> storage;
    std::vector<char *> argv;

    explicit ArgvBuilder(std::initializer_list<const char *> args)
    {
        storage.reserve(args.size());
        argv.reserve(args.size());
        for (const auto *arg : args)
            storage.emplace_back(arg);
        for (auto &arg : storage)
            argv.push_back(arg.data());
    }

    int argc() const { return static_cast<int>(argv.size()); }
};

static void
testProfileBeforeGroupIsForwarded()
{
    ArgvBuilder args{"komai", "-p", "work", "mcp", "serve"};
    const auto command = mcp_commands::parseServeCommand(args.argc(), args.argv.data());

    expect(command.status == mcp_commands::ParseStatus::Ready,
           "profile before group parses successfully");
    expect(command.profileId == "work", "profile before group forwards work");
    expect(command.accessMode == "read_only", "default access mode is read_only");
    expect(command.childArguments() ==
             QStringList{
               "serve",
               "--profile",
               "work",
               "--access",
               "read_only",
             },
           "profile before group child args");
}

static void
testProfileAfterServeIsForwarded()
{
    ArgvBuilder args{"komai", "mcp", "serve", "--profile", "work"};
    const auto command = mcp_commands::parseServeCommand(args.argc(), args.argv.data());

    expect(command.status == mcp_commands::ParseStatus::Ready,
           "profile after serve parses successfully");
    expect(command.profileId == "work", "profile after serve forwards work");
}

static void
testDefaultProfileIsNormalized()
{
    ArgvBuilder args{"komai", "mcp", "serve"};
    const auto command = mcp_commands::parseServeCommand(args.argc(), args.argv.data());

    expect(command.status == mcp_commands::ParseStatus::Ready,
           "default profile parses successfully");
    expect(command.profileId == "default", "default profile forwards default");
}

static void
testAccessFlagIsParsed()
{
    ArgvBuilder args{"komai", "mcp", "serve", "--access", "read_write"};
    const auto command = mcp_commands::parseServeCommand(args.argc(), args.argv.data());

    expect(command.status == mcp_commands::ParseStatus::Ready, "read_write access parses");
    expect(command.accessMode == "read_write", "read_write access forwards correctly");
}

static void
testInvalidAccessFails()
{
    ArgvBuilder args{"komai", "mcp", "serve", "--access", "invalid"};
    const auto command = mcp_commands::parseServeCommand(args.argc(), args.argv.data());

    expect(command.status == mcp_commands::ParseStatus::Error, "invalid access is rejected");
}

int
main()
{
    testProfileBeforeGroupIsForwarded();
    testProfileAfterServeIsForwarded();
    testDefaultProfileIsNormalized();
    testAccessFlagIsParsed();
    testInvalidAccessFails();

    if (failures != 0)
        std::cerr << failures << " test(s) failed.\n";
    return failures == 0 ? 0 : 1;
}

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string>
#include <vector>

#include "cli/ProfileCommands.h"

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
testLauncherCreateParses()
{
    ArgvBuilder args{"komai", "profiles", "launcher", "create", "work_2"};
    const auto command = profile_commands::parseLauncherCommand(args.argc(), args.argv.data());

    expect(command.status == profile_commands::ParseStatus::Ready,
           "launcher create parses successfully");
    expect(command.action == profile_commands::LauncherAction::Create,
           "launcher create uses create action");
    expect(command.profileId == "work_2", "launcher create forwards profile id");
}

static void
testLauncherRemoveParses()
{
    ArgvBuilder args{"komai", "profiles", "launcher", "remove", "work_2"};
    const auto command = profile_commands::parseLauncherCommand(args.argc(), args.argv.data());

    expect(command.status == profile_commands::ParseStatus::Ready,
           "launcher remove parses successfully");
    expect(command.action == profile_commands::LauncherAction::Remove,
           "launcher remove uses remove action");
}

static void
testDefaultProfileIsRejected()
{
    ArgvBuilder args{"komai", "profiles", "launcher", "create", "default"};
    const auto command = profile_commands::parseLauncherCommand(args.argc(), args.argv.data());

    expect(command.status == profile_commands::ParseStatus::Error,
           "default profile launcher is rejected");
}

static void
testInvalidProfileIsRejected()
{
    ArgvBuilder args{"komai", "profiles", "launcher", "create", "test8.5"};
    const auto command = profile_commands::parseLauncherCommand(args.argc(), args.argv.data());

    expect(command.status == profile_commands::ParseStatus::Error,
           "invalid profile ids are rejected");
}

int
main()
{
    testLauncherCreateParses();
    testLauncherRemoveParses();
    testDefaultProfileIsRejected();
    testInvalidProfileIsRejected();

    if (failures != 0)
        std::cerr << failures << " test(s) failed.\n";
    return failures == 0 ? 0 : 1;
}

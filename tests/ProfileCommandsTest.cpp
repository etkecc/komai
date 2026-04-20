// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string>

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

static void
testValidIdIsAccepted()
{
    const auto error = profile_commands::validateLauncherProfileId(QStringLiteral("work_2"));
    expect(!error.has_value(), "well-formed profile id is accepted");
}

static void
testDefaultProfileIsRejected()
{
    const auto error = profile_commands::validateLauncherProfileId(QStringLiteral("default"));
    expect(error.has_value(), "default profile is rejected");
}

static void
testInvalidProfileIsRejected()
{
    // '.' is not in the allowed [A-Za-z0-9_-] set.
    const auto error = profile_commands::validateLauncherProfileId(QStringLiteral("test8.5"));
    expect(error.has_value(), "ids with disallowed characters are rejected");
}

int
main()
{
    testValidIdIsAccepted();
    testDefaultProfileIsRejected();
    testInvalidProfileIsRejected();

    if (failures != 0)
        std::cerr << failures << " test(s) failed.\n";
    return failures == 0 ? 0 : 1;
}

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <QCoreApplication>

#include "cli/schema/Dispatcher.h"
#include "cli/schema/SchemaTypes.h"

using cli_schema::FlagDef;
using cli_schema::GroupDef;
using cli_schema::ParsedArgs;
using cli_schema::PositionalDef;
using cli_schema::SubcommandDef;

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

// Captures what the handler saw, so tests can inspect ParsedArgs after
// dispatchGroup returns.
struct HandlerCapture
{
    bool invoked = false;
    ParsedArgs parsed;
    int returnCode = 0;
};

// Build a small three-layer schema covering the features we want to exercise:
//   testgroup sub1                              -- boolean flags, enum flag
//     sub1 [--flag-a] [--flag-b value]
//     sub1 --flag-c one|two|three
//   testgroup sub2 <required-pos> [<opt>...]    -- required + variadic positional
//   testgroup nested                            -- nested subgroup
//     nested leaf                               -- bottom-of-tree handler
static GroupDef
buildTestGroup(HandlerCapture &sub1Capture,
               HandlerCapture &sub2Capture,
               HandlerCapture &leafCapture)
{
    GroupDef group;
    group.name = QStringLiteral("testgroup");
    group.help = QStringLiteral("test group");

    // sub1 — boolean, value, enum flags
    SubcommandDef sub1;
    sub1.name            = QStringLiteral("sub1");
    sub1.help            = QStringLiteral("one");
    sub1.requiresProfile = false;

    FlagDef flagA;
    flagA.longName = QStringLiteral("--flag-a");
    flagA.help     = QStringLiteral("boolean flag");
    sub1.flags.append(flagA);

    FlagDef flagB;
    flagB.longName         = QStringLiteral("--flag-b");
    flagB.takesValue       = true;
    flagB.valuePlaceholder = QStringLiteral("<v>");
    sub1.flags.append(flagB);

    FlagDef flagC;
    flagC.longName   = QStringLiteral("--flag-c");
    flagC.takesValue = true;
    flagC.valueEnum  = {QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("three")};
    sub1.flags.append(flagC);

    sub1.handler = [&sub1Capture](const ParsedArgs &p, QCoreApplication &) {
        sub1Capture.invoked = true;
        sub1Capture.parsed  = p;
        return sub1Capture.returnCode;
    };
    group.subcommands.append(sub1);

    // sub2 — required positional + variadic
    SubcommandDef sub2;
    sub2.name            = QStringLiteral("sub2");
    sub2.help            = QStringLiteral("two");
    sub2.requiresProfile = false;

    PositionalDef first;
    first.name = QStringLiteral("first");
    sub2.positionals.append(first);

    PositionalDef rest;
    rest.name     = QStringLiteral("rest");
    rest.variadic = true;
    rest.optional = true;
    sub2.positionals.append(rest);

    sub2.handler = [&sub2Capture](const ParsedArgs &p, QCoreApplication &) {
        sub2Capture.invoked = true;
        sub2Capture.parsed  = p;
        return sub2Capture.returnCode;
    };
    group.subcommands.append(sub2);

    // nested leaf
    SubcommandDef nested;
    nested.name            = QStringLiteral("nested");
    nested.help            = QStringLiteral("nested");
    nested.requiresProfile = false;

    SubcommandDef leaf;
    leaf.name            = QStringLiteral("leaf");
    leaf.help            = QStringLiteral("leaf handler");
    leaf.requiresProfile = false;
    leaf.handler         = [&leafCapture](const ParsedArgs &p, QCoreApplication &) {
        leafCapture.invoked = true;
        leafCapture.parsed  = p;
        return leafCapture.returnCode;
    };
    nested.subcommands.append(leaf);
    group.subcommands.append(nested);

    return group;
}

static void
testHelpAtGroupLevel(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "--help"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 0, "--help at group level exits 0");
    expect(!s1.invoked && !s2.invoked && !leaf.invoked,
           "--help at group level does not call any handler");
}

static void
testHelpAtSubcommandLevel(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "sub1", "--help"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 0, "subcommand --help exits 0");
    expect(!s1.invoked, "subcommand --help does not invoke handler");
}

static void
testHelpAtNestedLevel(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "nested", "leaf", "--help"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 0, "nested-leaf --help exits 0");
    expect(!leaf.invoked, "nested-leaf --help does not invoke handler");
}

static void
testGroupHelpWhenNoSubcommand(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 1, "group with no subcommand exits 1");
    expect(!s1.invoked && !s2.invoked && !leaf.invoked,
           "no subcommand invokes no handler");
}

static void
testUnknownSubcommandIsRejected(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "bogus"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 1, "unknown subcommand exits 1");
    expect(!s1.invoked && !s2.invoked, "unknown subcommand invokes no handler");
}

static void
testUnknownNestedSubcommandIsRejected(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "nested", "bogus"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 1, "unknown nested subcommand exits 1");
    expect(!leaf.invoked, "unknown nested subcommand invokes no handler");
}

// Regression guard: an unknown subcommand *below* a valid subgroup must print
// the nested subgroup's listing, not the top-level group listing. Captured
// here by redirecting stderr to a temp file and grepping for the nested leaf
// name.
static void
testUnknownNestedSubcommandErrorShowsNestedListing(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    std::string captured;
    {
        // Redirect stderr into a memory-backed streambuf.
        std::stringstream buffer;
        auto *saved = std::cerr.rdbuf(buffer.rdbuf());
        ArgvBuilder args{"komai", "testgroup", "nested", "bogus"};
        cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);
        std::cerr.rdbuf(saved);
        captured = buffer.str();
    }

    expect(captured.find("leaf") != std::string::npos,
           "nested-unknown error lists 'leaf' from the nested subgroup");
    expect(captured.find("sub1") == std::string::npos,
           "nested-unknown error does NOT list top-level 'sub1'");
}

static void
testUnknownFlagIsRejected(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "sub1", "--nope"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 1, "unknown flag exits 1");
    expect(!s1.invoked, "unknown flag does not invoke handler");
}

static void
testBooleanFlagRecorded(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "sub1", "--flag-a"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 0, "boolean flag reaches handler");
    expect(s1.invoked, "handler invoked");
    expect(s1.parsed.hasFlag(QStringLiteral("--flag-a")), "flag-a recorded in flagsPresent");
    expect(!s1.parsed.hasFlag(QStringLiteral("--flag-b")),
           "unspecified flag-b not in flagsPresent");
}

static void
testValueFlagSpaceSeparated(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "sub1", "--flag-b", "xyz"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 0, "value flag with space reaches handler");
    expect(s1.parsed.flagOr(QStringLiteral("--flag-b")) == QLatin1String("xyz"),
           "space-separated value captured");
}

static void
testValueFlagInlineEquals(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "sub1", "--flag-b=xyz"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 0, "--flag=value inline form reaches handler");
    expect(s1.parsed.flagOr(QStringLiteral("--flag-b")) == QLatin1String("xyz"),
           "inline-equals value captured");
}

static void
testValueFlagMissingValueFails(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "sub1", "--flag-b"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 1, "value flag without value exits 1");
    expect(!s1.invoked, "value flag without value does not invoke handler");
}

static void
testEnumFlagAcceptsValidValue(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "sub1", "--flag-c", "two"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 0, "valid enum value accepted");
    expect(s1.parsed.flagOr(QStringLiteral("--flag-c")) == QLatin1String("two"),
           "enum value captured");
}

static void
testEnumFlagRejectsInvalidValue(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "sub1", "--flag-c", "four"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 1, "invalid enum value exits 1");
    expect(!s1.invoked, "invalid enum value does not invoke handler");
}

static void
testMissingRequiredPositionalFails(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "sub2"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 1, "missing required positional exits 1");
    expect(!s2.invoked, "missing required positional does not invoke handler");
}

static void
testVariadicPositionalCaptures(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "sub2", "first", "second", "third"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 0, "variadic positional reaches handler");
    expect(s2.parsed.positionals.size() == 3, "three positionals captured");
    if (s2.parsed.positionals.size() == 3) {
        expect(s2.parsed.positionals.at(0) == QLatin1String("first"),
               "first positional correct");
        expect(s2.parsed.positionals.at(1) == QLatin1String("second"),
               "second positional correct");
        expect(s2.parsed.positionals.at(2) == QLatin1String("third"),
               "third positional correct");
    }
}

static void
testNestedSubcommandReachesLeaf(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "nested", "leaf"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 0, "nested leaf reached");
    expect(leaf.invoked, "leaf handler invoked");
    expect(leaf.parsed.subcommandPath ==
             QStringList{QStringLiteral("testgroup"),
                         QStringLiteral("nested"),
                         QStringLiteral("leaf")},
           "subcommandPath contains the full walk");
}

static void
testNestedGroupWithoutLeafShowsHelp(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "nested"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 1, "nested group without leaf exits 1");
    expect(!leaf.invoked, "nested-no-leaf does not invoke leaf handler");
}

static void
testProfileFlagBeforeGroupIsCaptured(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "-p", "work", "testgroup", "sub1"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 0, "-p before group still dispatches");
    expect(s1.invoked, "handler invoked with -p before group");
    expect(s1.parsed.profileId == QLatin1String("work"),
           "profileId populated from -p");
}

static void
testProfileFlagAfterGroupIsCaptured(QCoreApplication &app)
{
    HandlerCapture s1, s2, leaf;
    auto group = buildTestGroup(s1, s2, leaf);

    ArgvBuilder args{"komai", "testgroup", "sub1", "--profile", "work"};
    const int rc = cli_schema::dispatchGroup(group, args.argc(), args.argv.data(), app);

    expect(rc == 0, "--profile after group still dispatches");
    expect(s1.invoked, "handler invoked with --profile after group");
    expect(s1.parsed.profileId == QLatin1String("work"),
           "profileId populated from --profile after group");
}

int
main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testHelpAtGroupLevel(app);
    testHelpAtSubcommandLevel(app);
    testHelpAtNestedLevel(app);
    testGroupHelpWhenNoSubcommand(app);

    testUnknownSubcommandIsRejected(app);
    testUnknownNestedSubcommandIsRejected(app);
    testUnknownNestedSubcommandErrorShowsNestedListing(app);
    testUnknownFlagIsRejected(app);

    testBooleanFlagRecorded(app);
    testValueFlagSpaceSeparated(app);
    testValueFlagInlineEquals(app);
    testValueFlagMissingValueFails(app);
    testEnumFlagAcceptsValidValue(app);
    testEnumFlagRejectsInvalidValue(app);

    testMissingRequiredPositionalFails(app);
    testVariadicPositionalCaptures(app);

    testNestedSubcommandReachesLeaf(app);
    testNestedGroupWithoutLeafShowsHelp(app);

    testProfileFlagBeforeGroupIsCaptured(app);
    testProfileFlagAfterGroupIsCaptured(app);

    if (failures != 0)
        std::cerr << failures << " test(s) failed.\n";
    return failures == 0 ? 0 : 1;
}

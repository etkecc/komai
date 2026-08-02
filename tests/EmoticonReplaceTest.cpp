// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Tests for emoji::emoticonForShortcut() and emoji::isEmoticonShortcut().
//
// These test the single-token exact-match lookup used by the composer's
// live emoticon replacement (typing `:)` + space -> inline emoji), as
// distinct from emoji::replaceEmoticons()'s whole-message scan used at
// send time.

#include <iostream>

#include <QString>

#include "emoji/EmoticonReplace.h"

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
testExactMatchesReturnEmoji()
{
    // 🙂 = U+1F642 slightly smiling face
    expect(emoji::emoticonForShortcut(QStringLiteral(":)")) == QStringLiteral("\U0001F642"),
           ":) -> U+1F642");
    // 😀 = U+1F600 grinning face
    expect(emoji::emoticonForShortcut(QStringLiteral(":D")) == QStringLiteral("\U0001F600"),
           ":D -> U+1F600");
    // ❤ = U+2764 red heart
    expect(emoji::emoticonForShortcut(QStringLiteral("<3")) == QStringLiteral("❤"),
           "<3 -> U+2764");
    // 💔 = U+1F494 broken heart
    expect(emoji::emoticonForShortcut(QStringLiteral("</3")) == QStringLiteral("\U0001F494"),
           "</3 -> U+1F494 (not confused with <3)");
    // 🙂 with-nose variant, same emoji as :)
    expect(emoji::emoticonForShortcut(QStringLiteral(":-)")) == QStringLiteral("\U0001F642"),
           ":-) -> U+1F642");
}

static void
testCaseInsensitive()
{
    expect(emoji::emoticonForShortcut(QStringLiteral(":d")) == QStringLiteral("\U0001F600"),
           ":d (lowercase) -> U+1F600, same as :D");
    expect(emoji::emoticonForShortcut(QStringLiteral(":O")) == QStringLiteral("\U0001F62E"),
           ":O -> U+1F62E");
    expect(emoji::emoticonForShortcut(QStringLiteral(":o")) == QStringLiteral("\U0001F62E"),
           ":o (lowercase) -> U+1F62E, same as :O");
}

static void
testNonMatchesReturnEmpty()
{
    expect(emoji::emoticonForShortcut(QString{}).isEmpty(), "empty input -> empty");
    expect(emoji::emoticonForShortcut(QStringLiteral("hello")).isEmpty(), "plain word -> empty");
    // Not an exact match -- the composer must isolate the token itself;
    // this function does no partial/substring matching.
    expect(emoji::emoticonForShortcut(QStringLiteral(":Dog")).isEmpty(),
           ":Dog is not an exact shortcut -> empty");
    expect(emoji::emoticonForShortcut(QStringLiteral(" :) ")).isEmpty(),
           "shortcut with surrounding spaces is not an exact match -> empty");
    expect(emoji::emoticonForShortcut(QStringLiteral(":))")).isEmpty(),
           ":)) is not an exact shortcut -> empty");
}

static void
testReplaceLeadingEmoticonBareShortcut()
{
    // No trailing characters -- same result as a full-token exact match.
    expect(emoji::replaceLeadingEmoticon(QStringLiteral(":)")) == QStringLiteral("\U0001F642"),
           "replaceLeadingEmoticon(:)) -> U+1F642");
    expect(emoji::replaceLeadingEmoticon(QStringLiteral(";)")) == QStringLiteral("\U0001F609"),
           "replaceLeadingEmoticon(;)) -> U+1F609");
}

static void
testReplaceLeadingEmoticonPreservesTrailingPunctuation()
{
    // Regression: "Who are you :)?" + space must still convert the
    // shortcut even though a "?" sits between it and the space.
    expect(emoji::replaceLeadingEmoticon(QStringLiteral(":)?")) == QStringLiteral("\U0001F642?"),
           ":)? -> U+1F642 + ?  (trailing punctuation preserved)");
    expect(emoji::replaceLeadingEmoticon(QStringLiteral("<3!!!")) == QStringLiteral("❤!!!"),
           "<3!!! -> U+2764 + !!!  (trailing punctuation preserved)");
    expect(emoji::replaceLeadingEmoticon(QStringLiteral(":D,")) == QStringLiteral("\U0001F600,"),
           ":D, -> U+1F600 + ,");
}

static void
testReplaceLeadingEmoticonRejectsWordContinuation()
{
    // ":Dog" must NOT become "\U0001F600og" -- the shortcut is a prefix of
    // a longer word, not a standalone emoticon.
    expect(emoji::replaceLeadingEmoticon(QStringLiteral(":Dog")).isEmpty(),
           ":Dog -> empty (letter right after the shortcut)");
    expect(emoji::replaceLeadingEmoticon(QStringLiteral(":)0")).isEmpty(),
           ":)0 -> empty (digit right after the shortcut)");
}

static void
testReplaceLeadingEmoticonCaseInsensitiveAndNoMatch()
{
    expect(emoji::replaceLeadingEmoticon(QStringLiteral(":d?")) == QStringLiteral("\U0001F600?"),
           ":d? (lowercase) -> U+1F600 + ?");
    expect(emoji::replaceLeadingEmoticon(QStringLiteral("hello")).isEmpty(),
           "hello -> empty (no shortcut prefix)");
    expect(emoji::replaceLeadingEmoticon(QString{}).isEmpty(), "empty input -> empty");
}

static void
testIsEmoticonShortcutStillWorks()
{
    // isEmoticonShortcut() now delegates to emoticonForShortcut() -- verify
    // its existing bool contract (used by the composer's emoji-picker
    // suppression logic) is unchanged.
    expect(emoji::isEmoticonShortcut(QStringLiteral(":)")), "isEmoticonShortcut(:)) -> true");
    expect(emoji::isEmoticonShortcut(QStringLiteral(":d")),
           "isEmoticonShortcut(:d) -> true (case-insensitive)");
    expect(!emoji::isEmoticonShortcut(QStringLiteral(":Dog")), "isEmoticonShortcut(:Dog) -> false");
    expect(!emoji::isEmoticonShortcut(QString{}), "isEmoticonShortcut(empty) -> false");
}

int
main()
{
    testExactMatchesReturnEmoji();
    testCaseInsensitive();
    testNonMatchesReturnEmpty();
    testReplaceLeadingEmoticonBareShortcut();
    testReplaceLeadingEmoticonPreservesTrailingPunctuation();
    testReplaceLeadingEmoticonRejectsWordContinuation();
    testReplaceLeadingEmoticonCaseInsensitiveAndNoMatch();
    testIsEmoticonShortcutStillWorks();

    if (failures) {
        std::cerr << failures << " test(s) FAILED\n";
        return 1;
    }
    std::cout << "All emoticon replace tests passed.\n";
    return 0;
}

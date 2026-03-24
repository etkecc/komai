// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Tests for emoji::normalizeForComparison().
//
// Each test case documents the exact Unicode codepoints involved, because many
// emoji look identical on screen despite having different byte representations.
// The most common difference is the presence or absence of U+FE0F (Variation
// Selector 16), which requests emoji-style presentation but is visually
// redundant for codepoints that already default to emoji presentation.

#include <iostream>
#include <string>

#include <QString>

#include "emoji/EmojiNormalize.h"

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

// ---------------------------------------------------------------------------
// QString overload
// ---------------------------------------------------------------------------

static void
testQStringStripVS16()
{
    // 👍️  = U+1F44D  U+FE0F   (thumbs up + Variation Selector 16)
    // 👍   = U+1F44D            (thumbs up, no VS)
    const QString withVS16    = QStringLiteral("\U0001F44D\uFE0F");
    const QString withoutVS16 = QStringLiteral("\U0001F44D");
    expect(emoji::normalizeForComparison(withVS16) == withoutVS16,
           "QString: thumbs up U+1F44D U+FE0F → U+1F44D");

    // Already-simple form stays unchanged.
    expect(emoji::normalizeForComparison(withoutVS16) == withoutVS16,
           "QString: thumbs up U+1F44D unchanged");
}

static void
testQStringStripVS15()
{
    // U+FE0E (Variation Selector 15 — text presentation) should also be stripped.
    // ❤︎ = U+2764 U+FE0E  (heart, text presentation)
    // ❤  = U+2764          (heart, no VS)
    const QString heartText = QStringLiteral("\u2764\uFE0E");
    const QString heartBare = QStringLiteral("\u2764");
    expect(emoji::normalizeForComparison(heartText) == heartBare,
           "QString: heart U+2764 U+FE0E → U+2764");
}

static void
testQStringHeartWithVS16()
{
    // ❤️ = U+2764 U+FE0F  (heart, emoji presentation)
    // ❤  = U+2764          (heart, no VS)
    const QString heartEmoji = QStringLiteral("\u2764\uFE0F");
    const QString heartBare  = QStringLiteral("\u2764");
    expect(emoji::normalizeForComparison(heartEmoji) == heartBare,
           "QString: heart U+2764 U+FE0F → U+2764");
}

static void
testQStringThumbsDownWithVS16()
{
    // 👎️ = U+1F44E U+FE0F  (thumbs down + VS16)
    // 👎  = U+1F44E          (thumbs down)
    const QString with    = QStringLiteral("\U0001F44E\uFE0F");
    const QString without = QStringLiteral("\U0001F44E");
    expect(emoji::normalizeForComparison(with) == without,
           "QString: thumbs down U+1F44E U+FE0F → U+1F44E");
}

static void
testQStringSkinTonePreserved()
{
    // 👍🏿 = U+1F44D U+1F3FF  (thumbs up + dark skin tone modifier)
    // Skin tone modifiers (U+1F3FB–U+1F3FF) are NOT variation selectors and
    // produce visually distinct emojis — they must be preserved.
    const QString darkThumbsUp = QStringLiteral("\U0001F44D\U0001F3FF");
    expect(emoji::normalizeForComparison(darkThumbsUp) == darkThumbsUp,
           "QString: dark skin thumbs up U+1F44D U+1F3FF preserved");

    // Must not match bare thumbs up.
    const QString bareThumbsUp = QStringLiteral("\U0001F44D");
    expect(emoji::normalizeForComparison(darkThumbsUp) != bareThumbsUp,
           "QString: dark skin thumbs up != bare thumbs up");
}

static void
testQStringSkinToneWithVS16Preserved()
{
    // Some clients send: U+1F44D U+FE0F U+1F3FF (thumbs up + VS16 + dark skin tone).
    // Stripping VS16 should give U+1F44D U+1F3FF, preserving the skin tone.
    const QString withVS16  = QStringLiteral("\U0001F44D\uFE0F\U0001F3FF");
    const QString expected  = QStringLiteral("\U0001F44D\U0001F3FF");
    expect(emoji::normalizeForComparison(withVS16) == expected,
           "QString: U+1F44D U+FE0F U+1F3FF → U+1F44D U+1F3FF (skin tone kept, VS16 stripped)");
}

static void
testQStringZWJSequencePreserved()
{
    // 👨‍👩‍👧 = U+1F468 U+200D U+1F469 U+200D U+1F467  (family: man, woman, girl)
    // ZWJ (U+200D) joins the sequence — must be preserved.
    const QString family = QStringLiteral("\U0001F468\u200D\U0001F469\u200D\U0001F467");
    expect(emoji::normalizeForComparison(family) == family,
           "QString: ZWJ family sequence preserved");
}

static void
testQStringFlagPreserved()
{
    // 🇩🇪 = U+1F1E9 U+1F1EA  (flag: Germany, regional indicator D + E)
    const QString flag = QStringLiteral("\U0001F1E9\U0001F1EA");
    expect(emoji::normalizeForComparison(flag) == flag,
           "QString: flag sequence U+1F1E9 U+1F1EA preserved");
}

static void
testQStringAlreadySimple()
{
    // 🚀 = U+1F680  (rocket, no VS — emoji-default codepoint)
    const QString rocket = QStringLiteral("\U0001F680");
    expect(emoji::normalizeForComparison(rocket) == rocket,
           "QString: rocket U+1F680 unchanged");

    // 😀 = U+1F600  (grinning face, no VS)
    const QString grin = QStringLiteral("\U0001F600");
    expect(emoji::normalizeForComparison(grin) == grin,
           "QString: grinning face U+1F600 unchanged");
}

static void
testQStringEmptyAndPlainText()
{
    expect(emoji::normalizeForComparison(QString{}).isEmpty(),
           "QString: empty string stays empty");

    // Plain ASCII text has no variation selectors.
    const QString ascii = QStringLiteral("hello");
    expect(emoji::normalizeForComparison(ascii) == ascii,
           "QString: plain ASCII unchanged");
}

// ---------------------------------------------------------------------------
// std::string (UTF-8) overload
// ---------------------------------------------------------------------------

static void
testStdStringStripVS16()
{
    // 👍️ in UTF-8: F0 9F 91 8D  EF B8 8F   (U+1F44D + U+FE0F)
    // 👍  in UTF-8: F0 9F 91 8D              (U+1F44D)
    const std::string withVS16    = "\xF0\x9F\x91\x8D\xEF\xB8\x8F";
    const std::string withoutVS16 = "\xF0\x9F\x91\x8D";
    expect(emoji::normalizeForComparison(withVS16) == withoutVS16,
           "std::string: thumbs up F0 9F 91 8D + EF B8 8F → F0 9F 91 8D");
}

static void
testStdStringStripVS15()
{
    // ❤︎ in UTF-8: E2 9D A4  EF B8 8E   (U+2764 + U+FE0E)
    // ❤  in UTF-8: E2 9D A4              (U+2764)
    const std::string heartText = "\xE2\x9D\xA4\xEF\xB8\x8E";
    const std::string heartBare = "\xE2\x9D\xA4";
    expect(emoji::normalizeForComparison(heartText) == heartBare,
           "std::string: heart E2 9D A4 + EF B8 8E → E2 9D A4");
}

static void
testStdStringHeartWithVS16()
{
    // ❤️ in UTF-8: E2 9D A4  EF B8 8F   (U+2764 + U+FE0F)
    const std::string heartEmoji = "\xE2\x9D\xA4\xEF\xB8\x8F";
    const std::string heartBare  = "\xE2\x9D\xA4";
    expect(emoji::normalizeForComparison(heartEmoji) == heartBare,
           "std::string: heart E2 9D A4 + EF B8 8F → E2 9D A4");
}

static void
testStdStringSkinTonePreserved()
{
    // 👍🏿 in UTF-8: F0 9F 91 8D  F0 9F 8F BF  (U+1F44D + U+1F3FF)
    const std::string dark = "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBF";
    expect(emoji::normalizeForComparison(dark) == dark,
           "std::string: dark skin thumbs up preserved");

    const std::string bare = "\xF0\x9F\x91\x8D";
    expect(emoji::normalizeForComparison(dark) != bare,
           "std::string: dark skin thumbs up != bare thumbs up");
}

static void
testStdStringEmpty()
{
    expect(emoji::normalizeForComparison(std::string{}).empty(),
           "std::string: empty string stays empty");
}

// ---------------------------------------------------------------------------
// Dedup scenario: pinned vs. frequent reactions
// ---------------------------------------------------------------------------

static void
testDedupScenario()
{
    // Simulates the actual bug: pinned has U+1F44D U+FE0F, frequent has U+1F44D.
    // After normalization, both should compare equal.
    const QString pinned   = QStringLiteral("\U0001F44D\uFE0F"); // from settings
    const QString frequent = QStringLiteral("\U0001F44D");        // from room history

    expect(emoji::normalizeForComparison(pinned) == emoji::normalizeForComparison(frequent),
           "dedup: pinned U+1F44D U+FE0F matches frequent U+1F44D after normalization");

    // Skin-toned variant should NOT match the base form.
    const QString frequentDark = QStringLiteral("\U0001F44D\U0001F3FF");
    expect(emoji::normalizeForComparison(pinned) != emoji::normalizeForComparison(frequentDark),
           "dedup: pinned U+1F44D U+FE0F does not match dark-skin U+1F44D U+1F3FF");
}

int
main()
{
    // QString overload
    testQStringStripVS16();
    testQStringStripVS15();
    testQStringHeartWithVS16();
    testQStringThumbsDownWithVS16();
    testQStringSkinTonePreserved();
    testQStringSkinToneWithVS16Preserved();
    testQStringZWJSequencePreserved();
    testQStringFlagPreserved();
    testQStringAlreadySimple();
    testQStringEmptyAndPlainText();

    // std::string overload
    testStdStringStripVS16();
    testStdStringStripVS15();
    testStdStringHeartWithVS16();
    testStdStringSkinTonePreserved();
    testStdStringEmpty();

    // Dedup scenario
    testDedupScenario();

    if (failures) {
        std::cerr << failures << " test(s) FAILED\n";
        return 1;
    }
    std::cout << "All emoji normalization tests passed.\n";
    return 0;
}

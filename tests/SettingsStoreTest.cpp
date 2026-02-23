// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>

#include "TestEnvironment.h"
#include "settings/core/SettingsStore.h"

namespace {

bool
expect(bool condition, const char *message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

bool
testBasicSetGet()
{
    settings::core::SettingsStore store;
    const auto result = store.set(settings::core::SettingId::UiFontSizePt, 14);

    bool ok = true;
    ok &= expect(result.success, "set(int) succeeds");
    ok &= expect(result.changed, "set(int) reports changed");
    ok &= expect(store.hasValue(settings::core::SettingId::UiFontSizePt), "store has value after set");
    ok &= expect(store.size() == 1, "store tracks value count");
    const auto asInt = store.valueAs<int>(settings::core::SettingId::UiFontSizePt);
    ok &= expect(asInt.has_value() && *asInt == 14, "valueAs<int> reads stored value");

    return ok;
}

bool
testSetIdUnknownFails()
{
    settings::core::SettingsStore store;
    const auto result = store.set(settings::core::SettingId::Unknown, true);

    return expect(!result.success, "set(Unknown) fails") &&
           expect(!result.changed, "set(Unknown) does not mark changed") &&
           expect(!result.validationError.empty(), "set(Unknown) returns validation error");
}

bool
testIdempotentSet()
{
    settings::core::SettingsStore store;
    const auto first = store.set(settings::core::SettingId::UiMotionAnimationsEnabled, true);
    const auto second = store.set(settings::core::SettingId::UiMotionAnimationsEnabled, true);

    return expect(first.success && first.changed, "first set changes value") &&
           expect(second.success && !second.changed, "second set is idempotent");
}

bool
testTypeMismatchRead()
{
    settings::core::SettingsStore store;
    (void)store.set(settings::core::SettingId::TimelineMessagesMaxWidthPx, 1200);

    const auto asDouble = store.valueAs<double>(settings::core::SettingId::TimelineMessagesMaxWidthPx);
    const auto asString = store.valueAs<std::string>(
      settings::core::SettingId::TimelineMessagesMaxWidthPx);

    return expect(!asDouble.has_value(), "valueAs<double> rejects int value") &&
           expect(!asString.has_value(), "valueAs<string> rejects int value");
}

bool
testEraseAndClear()
{
    settings::core::SettingsStore store;
    (void)store.set(settings::core::SettingId::UiFontFamily, std::string("Noto Sans"));
    (void)store.set(settings::core::SettingId::UiFontEmojiFamily, std::string("Noto Color Emoji"));

    bool ok = true;
    ok &= expect(store.size() == 2, "two values inserted");
    ok &= expect(store.erase(settings::core::SettingId::UiFontFamily), "erase existing value returns true");
    ok &= expect(!store.erase(settings::core::SettingId::UiFontFamily),
                 "erase missing value returns false");
    ok &= expect(store.size() == 1, "size decreases after erase");
    store.clear();
    ok &= expect(store.size() == 0, "clear removes all values");
    ok &= expect(!store.hasValue(settings::core::SettingId::UiFontEmojiFamily),
                 "clear removes remaining value");

    return ok;
}

bool
testStringListValue()
{
    settings::core::SettingsStore store;
    settings::core::SettingsStore::StringList values{"one", "two", "three"};
    const auto setResult =
      store.set(settings::core::SettingId::TimelineMessageActionsPinnedReactions, values);
    const auto read =
      store.valueAs<settings::core::SettingsStore::StringList>(
        settings::core::SettingId::TimelineMessageActionsPinnedReactions);

    return expect(setResult.success && setResult.changed, "set string list succeeds") &&
           expect(read.has_value(), "string list can be read back") &&
           expect(read->size() == 3 && read->at(1) == "two",
                  "string list preserves ordering and values");
}

} // namespace

int
main()
{
    test_env::ScopedTestHome testHome{QStringLiteral("komai-settings-store-test")};
    if (!testHome.isValid()) {
        std::cerr << "FAILED: test home environment can be created\n";
        return 1;
    }
    if (!testHome.isIsolated()) {
        std::cerr << "FAILED: test home environment is isolated\n";
        return 1;
    }

    bool ok = true;
    ok &= testBasicSetGet();
    ok &= testSetIdUnknownFails();
    ok &= testIdempotentSet();
    ok &= testTypeMismatchRead();
    ok &= testEraseAndClear();
    ok &= testStringListValue();
    return ok ? 0 : 1;
}

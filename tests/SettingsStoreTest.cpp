// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <iostream>
#include <limits>
#include <string_view>
#include <unordered_set>

#include "TestEnvironment.h"
#include "settings/core/SettingsConstraints.h"
#include "settings/core/SettingsDefinitions.h"
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

constexpr settings::core::SettingId kExpectedConstrainedIds[] = {
  settings::core::SettingId::UiThemeMode,
  settings::core::SettingId::UiScrollbarPolicy,
  settings::core::SettingId::UiAvatarsDefaultAvatarStyle,
  settings::core::SettingId::UiLayoutDensity,
  settings::core::SettingId::TimelineMessagesLayoutMaxWidthPercent,
  settings::core::SettingId::TimelineMessagesLayoutAdaptivePositioningBreakpointPx,
  settings::core::SettingId::IntegrationsDbusApiAccess,
  settings::core::SettingId::NetworkPresenceStatusPolicy,
  settings::core::SettingId::CallsScreenshareFrameRate,
  settings::core::SettingId::DesktopNotificationsMessageContentPolicy,
  settings::core::SettingId::ComposerInputSendKey,
  settings::core::SettingId::ComposerInputAutoReplaceEmoji,
  settings::core::SettingId::ComposerInputEmojiPreferredGender,
  settings::core::SettingId::ComposerInputEmojiPreferredSkinTone,
  settings::core::SettingId::NavigationRoomListSort,
  settings::core::SettingId::NavigationRoomListLastMessagePreview,
  settings::core::SettingId::NavigationRoomListOpeningPolicy,
  settings::core::SettingId::NavigationTabsShowPinButton,
  settings::core::SettingId::NavigationTabsPinnedTabLabel,
  settings::core::SettingId::NavigationTabsTabLabel,
  settings::core::SettingId::NavigationTabsPreferredWidthPx,
  settings::core::SettingId::NavigationTabsMinimumWidthPx,
  settings::core::SettingId::NavigationTabsMaxRecentlyClosedTimelines,
  settings::core::SettingId::TimelineMessagesStyle,
  settings::core::SettingId::TimelineMessagesLayoutPositioning,
  settings::core::SettingId::TimelineUserColorCodingPolicy,
  settings::core::SettingId::TimelineMessagesLayoutAvatarSize,
  settings::core::SettingId::TimelineMessagesSenderUsername,
  settings::core::SettingId::TimelineMediaImageDisplay,
  settings::core::SettingId::TimelineMessageActionsActivationPolicy,
  settings::core::SettingId::TimelineRoomHeaderButtonLabels,
  settings::core::SettingId::DesktopSystemTrayIconStyle,
  settings::core::SettingId::DesktopWindowFocusBlurDelaySeconds,
};

constexpr std::string_view kLegacyEnabledSuffix{"_enabled"};

bool
testBasicSetGet()
{
    settings::core::SettingsStore store;
    const auto result = store.set(settings::core::SettingId::UiFontSizePt, 14);

    bool ok = true;
    ok &= expect(result.success, "set(int) succeeds");
    ok &= expect(result.changed, "set(int) reports changed");
    ok &=
      expect(store.hasValue(settings::core::SettingId::UiFontSizePt), "store has value after set");
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
    const auto first  = store.set(settings::core::SettingId::UiMotionAnimationsEnabled, true);
    const auto second = store.set(settings::core::SettingId::UiMotionAnimationsEnabled, true);

    return expect(first.success && first.changed, "first set changes value") &&
           expect(second.success && !second.changed, "second set is idempotent");
}

bool
testTypeMismatchRead()
{
    settings::core::SettingsStore store;
    (void)store.set(settings::core::SettingId::TimelineMessagesLayoutMaxWidthPercent, 1200);

    const auto asDouble =
      store.valueAs<double>(settings::core::SettingId::TimelineMessagesLayoutMaxWidthPercent);
    const auto asString =
      store.valueAs<std::string>(settings::core::SettingId::TimelineMessagesLayoutMaxWidthPercent);

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
    ok &= expect(store.erase(settings::core::SettingId::UiFontFamily),
                 "erase existing value returns true");
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
    const auto read = store.valueAs<settings::core::SettingsStore::StringList>(
      settings::core::SettingId::TimelineMessageActionsPinnedReactions);

    return expect(setResult.success && setResult.changed, "set string list succeeds") &&
           expect(read.has_value(), "string list can be read back") &&
           expect(read->size() == 3 && read->at(1) == "two",
                  "string list preserves ordering and values");
}

bool
testEnumValidationRejectsOutOfRange()
{
    settings::core::SettingsStore store;
    settings::core::constraints::applyDefaultConstraints(store);
    const auto invalid = store.set(settings::core::SettingId::IntegrationsDbusApiAccess, 99);
    const bool hasRejectedValue =
      store.hasValue(settings::core::SettingId::IntegrationsDbusApiAccess);
    const auto valid = store.set(settings::core::SettingId::IntegrationsDbusApiAccess, 2);

    bool ok = true;
    ok &= expect(!invalid.success, "enum/int validation rejects out-of-range values");
    ok &= expect(!invalid.changed, "invalid enum/int value does not mark changed");
    ok &= expect(!invalid.validationError.empty(), "invalid enum/int value reports error");
    ok &= expect(!hasRejectedValue, "rejected enum/int value is not stored");
    ok &= expect(valid.success && valid.changed, "valid enum/int value is accepted");

    return ok;
}

bool
testEnumValidationRejectsWrongType()
{
    settings::core::SettingsStore store;
    settings::core::constraints::applyDefaultConstraints(store);
    const auto invalid = store.set(settings::core::SettingId::NetworkPresenceStatusPolicy, true);

    bool ok = true;
    ok &= expect(!invalid.success, "enum/int validation rejects wrong value type");
    ok &= expect(!invalid.changed, "wrong enum/int type does not mark changed");
    ok &= expect(!invalid.validationError.empty(), "wrong enum/int type reports error");
    ok &= expect(!store.hasValue(settings::core::SettingId::NetworkPresenceStatusPolicy),
                 "wrong enum/int type is not stored");

    return ok;
}

bool
testConstraintSchemaCoverage()
{
    bool ok = true;
    ok &= expect(settings::core::constraints::intRangeConstraintCount() ==
                   std::size(kExpectedConstrainedIds),
                 "constraint schema size matches expected constrained setting list");
    for (const auto id : kExpectedConstrainedIds) {
        if (!settings::core::constraints::hasIntRangeConstraint(id)) {
            std::cerr << "FAILED: missing constraint schema for SettingId " << static_cast<int>(id)
                      << '\n';
            ok = false;
        }
    }

    return ok;
}

bool
testConstrainedDefinitionsEnforceRanges()
{
    settings::core::SettingsStore store;
    settings::core::constraints::applyDefaultConstraints(store);

    bool ok = true;
    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (!definition.hasIntRangeConstraint)
            continue;

        const int minValue = definition.intRangeConstraintMin;
        const int maxValue = definition.intRangeConstraintMax;
        const auto setMin  = store.set(definition.id, minValue);
        const auto setMax  = store.set(definition.id, maxValue);
        const auto stored  = store.valueAs<int>(definition.id);

        if (!setMin.success || !setMax.success) {
            std::cerr << "FAILED: constrained setting rejects declared bounds for SettingId "
                      << static_cast<int>(definition.id) << '\n';
            ok = false;
        }

        if (!stored.has_value() || *stored != maxValue) {
            std::cerr
              << "FAILED: constrained setting does not persist declared max bound for SettingId "
              << static_cast<int>(definition.id) << '\n';
            ok = false;
        }

        if (minValue > std::numeric_limits<int>::min()) {
            const auto belowMin = store.set(definition.id, minValue - 1);
            if (belowMin.success) {
                std::cerr << "FAILED: constrained setting accepts below-min value for SettingId "
                          << static_cast<int>(definition.id) << '\n';
                ok = false;
            }
        }

        if (maxValue < std::numeric_limits<int>::max()) {
            const auto aboveMax = store.set(definition.id, maxValue + 1);
            if (aboveMax.success) {
                std::cerr << "FAILED: constrained setting accepts above-max value for SettingId "
                          << static_cast<int>(definition.id) << '\n';
                ok = false;
            }
        }
    }

    return ok;
}

bool
testPersistedDefinitionCoverage()
{
    constexpr std::size_t expectedPersistedDefinitionCount = 108;
    const auto definitions = settings::core::definitions::persistedDefinitions();

    bool ok = true;
    ok &= expect(definitions.size() == expectedPersistedDefinitionCount,
                 "persisted definition schema size matches expected setting list");

    for (const auto &definition : definitions) {
        if (definition.id == settings::core::SettingId::Unknown) {
            std::cerr << "FAILED: persisted definition uses SettingId::Unknown\n";
            ok = false;
        }

        if (definition.scope == settings::core::SettingScope::Runtime) {
            std::cerr << "FAILED: runtime-scoped setting in persisted definitions for SettingId "
                      << static_cast<int>(definition.id) << '\n';
            ok = false;
        }

        if (definition.persistedKey == nullptr || definition.persistedKey[0] == '\0') {
            std::cerr << "FAILED: persisted definition missing key for SettingId "
                      << static_cast<int>(definition.id) << '\n';
            ok = false;
        }

        const std::string_view persistedKey{definition.persistedKey};
        if (persistedKey.ends_with(kLegacyEnabledSuffix)) {
            std::cerr << "FAILED: persisted definition key still uses legacy '_enabled' suffix: '"
                      << persistedKey << "' for SettingId " << static_cast<int>(definition.id)
                      << '\n';
            ok = false;
        }

        if (definition.hasIntRangeConstraint &&
            definition.intRangeConstraintMin > definition.intRangeConstraintMax) {
            std::cerr << "FAILED: invalid int range in persisted definition for SettingId "
                      << static_cast<int>(definition.id) << '\n';
            ok = false;
        }
    }

    return ok;
}

bool
testPersistedDefinitionUniqueness()
{
    std::unordered_set<int> seen;
    bool ok = true;

    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        const int idValue = static_cast<int>(definition.id);
        if (!seen.insert(idValue).second) {
            std::cerr << "FAILED: duplicate persisted definition for SettingId " << idValue << '\n';
            ok = false;
        }
    }

    return ok;
}

bool
testConstrainedSettingsArePersisted()
{
    std::unordered_set<int> expectedConstrainedIds;
    for (const auto id : kExpectedConstrainedIds)
        expectedConstrainedIds.insert(static_cast<int>(id));

    bool ok = true;
    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (!definition.hasIntRangeConstraint)
            continue;

        if (expectedConstrainedIds.erase(static_cast<int>(definition.id)) == 0) {
            std::cerr << "FAILED: unexpected constrained setting in persisted schema for SettingId "
                      << static_cast<int>(definition.id) << '\n';
            ok = false;
        }
    }

    for (const auto id : expectedConstrainedIds) {
        std::cerr
          << "FAILED: expected constrained setting missing from persisted schema for SettingId "
          << id << '\n';
        ok = false;
    }

    return ok;
}

bool
testUnconstrainedSettingsAcceptAnyInt()
{
    settings::core::SettingsStore store;
    const auto result = store.set(settings::core::SettingId::IntegrationsDbusApiAccess, 99);
    const auto value  = store.valueAs<int>(settings::core::SettingId::IntegrationsDbusApiAccess);

    return expect(result.success && result.changed, "unconstrained setting accepts raw int") &&
           expect(value.has_value() && *value == 99, "unconstrained setting stores raw int value");
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
    ok &= testEnumValidationRejectsOutOfRange();
    ok &= testEnumValidationRejectsWrongType();
    ok &= testConstraintSchemaCoverage();
    ok &= testConstrainedDefinitionsEnforceRanges();
    ok &= testPersistedDefinitionCoverage();
    ok &= testPersistedDefinitionUniqueness();
    ok &= testConstrainedSettingsArePersisted();
    ok &= testUnconstrainedSettingsAcceptAnyInt();
    return ok ? 0 : 1;
}

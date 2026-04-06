// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <iostream>
#include <string>
#include <string_view>

#include "ui/LayoutAgnosticKeys.h"

namespace {

using LatinKey = LayoutAgnosticKeys::LatinKey;

struct KeyExpectation
{
    LatinKey latinKey;
    int logicalQtKey;
    // Qt reports XKB keycodes (evdev + 8) as nativeScanCode on all Linux backends.
    quint32 linuxXkbScanCode;
    quint32 windowsScanCode;
    const char *label;
};

constexpr auto kKeyExpectations = std::to_array<KeyExpectation>({
  {LatinKey::D, Qt::Key_D, 40, 32, "D"},
  {LatinKey::E, Qt::Key_E, 26, 18, "E"},
  {LatinKey::F, Qt::Key_F, 41, 33, "F"},
  {LatinKey::G, Qt::Key_G, 42, 34, "G"},
  {LatinKey::H, Qt::Key_H, 43, 35, "H"},
  {LatinKey::I, Qt::Key_I, 31, 23, "I"},
  {LatinKey::J, Qt::Key_J, 44, 36, "J"},
  {LatinKey::K, Qt::Key_K, 45, 37, "K"},
  {LatinKey::L, Qt::Key_L, 46, 38, "L"},
  {LatinKey::O, Qt::Key_O, 32, 24, "O"},
  {LatinKey::R, Qt::Key_R, 27, 19, "R"},
  {LatinKey::T, Qt::Key_T, 28, 20, "T"},
  {LatinKey::U, Qt::Key_U, 30, 22, "U"},
});

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

bool
testLogicalQtKeys()
{
    LayoutAgnosticKeys matcher;

    bool ok = true;
    for (const auto &expectation : kKeyExpectations) {
        const bool matched = matcher.matchesLatinKey(expectation.latinKey,
                                                     expectation.logicalQtKey,
                                                     0);
        ok &= expect(matched,
                     std::string("logical Qt key matches LatinKey::") + expectation.label);
    }

    return ok;
}

bool
testLinuxScanCodes()
{
    LayoutAgnosticKeys matcher;

    bool ok = true;
    for (const auto &expectation : kKeyExpectations) {
#if defined(Q_OS_WIN)
        const bool matched =
          matcher.matchesLatinKey(expectation.latinKey, 0, expectation.windowsScanCode);
        ok &= expect(matched,
                     std::string("Windows scan code matches LatinKey::") + expectation.label);
#elif defined(Q_OS_MACOS)
        const bool matched =
          matcher.matchesLatinKey(expectation.latinKey, 0, expectation.windowsScanCode);
        ok &= expect(!matched,
                     std::string("macOS does not claim native scan support for LatinKey::") +
                       expectation.label);
#else
        const bool matched =
          matcher.matchesLatinKey(expectation.latinKey, 0, expectation.linuxXkbScanCode);
        ok &= expect(matched,
                     std::string("Linux XKB scan code matches LatinKey::") +
                       expectation.label);
#endif
    }

    return ok;
}

bool
testInvalidEnumValueReturnsFalse()
{
    LayoutAgnosticKeys matcher;
    const auto invalidLatinKey = static_cast<LatinKey>(static_cast<int>(LatinKey::Count));
    return expect(!matcher.matchesLatinKey(invalidLatinKey, Qt::Key_A, 0),
                  "invalid LatinKey enum value returns false");
}

bool
testSpecialKeysDoNotUseScanCodeFallback()
{
    LayoutAgnosticKeys matcher;

    bool ok = true;
    ok &= expect(!matcher.matchesLatinKey(LatinKey::U, Qt::Key_Backspace, 22),
                 "Backspace does not match LatinKey::U via native scan code");
    ok &= expect(!matcher.matchesLatinKey(LatinKey::O, Qt::Key_Backspace, 24),
                 "Backspace does not match LatinKey::O via native scan code");

    return ok;
}

bool
testLatinKeysSkipScanCodeFallback()
{
    LayoutAgnosticKeys matcher;

    bool ok = true;

    // When event.key is already a Latin letter (A-Z), the physical position is
    // unambiguous. Scan-code fallback must not cause cross-key collisions.
    // Key_O (XKB 32) vs Key_D (XKB 40): O's XKB keycode must not match D.
    ok &= expect(!matcher.matchesLatinKey(LatinKey::D, Qt::Key_O, 32),
                 "Key_O does not match LatinKey::D via scan code when logical key is Latin");
    ok &= expect(!matcher.matchesLatinKey(LatinKey::O, Qt::Key_D, 40),
                 "Key_D does not match LatinKey::O via scan code when logical key is Latin");

    return ok;
}

bool
testNoCrossKeyScanCodeCollisions()
{
    LayoutAgnosticKeys matcher;

    bool ok = true;

    // On a non-Latin layout, each key's XKB scan code must only match itself.
    for (const auto &outer : kKeyExpectations) {
        for (const auto &inner : kKeyExpectations) {
            if (outer.latinKey == inner.latinKey)
                continue;

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
            const bool crossMatched =
              matcher.matchesLatinKey(outer.latinKey, 0, inner.linuxXkbScanCode);
            ok &= expect(!crossMatched,
                         std::string("LatinKey::") + outer.label +
                           " must not match scan code of LatinKey::" + inner.label);
#endif
        }
    }

    return ok;
}

} // namespace

int
main()
{
    bool ok = true;
    ok &= testLogicalQtKeys();
    ok &= testLinuxScanCodes();
    ok &= testInvalidEnumValueReturnsFalse();
    ok &= testSpecialKeysDoNotUseScanCodeFallback();
    ok &= testLatinKeysSkipScanCodeFallback();
    ok &= testNoCrossKeyScanCodeCollisions();

    if (!ok)
        return 1;

    std::cout << "All layout-agnostic key tests passed\n";
    return 0;
}

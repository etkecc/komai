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
    quint32 linuxPrimaryScanCode;
    quint32 linuxAlternateScanCode;
    quint32 windowsScanCode;
    const char *label;
};

constexpr auto kKeyExpectations = std::to_array<KeyExpectation>({
  {LatinKey::D, Qt::Key_D, 40, 32, 32, "D"},
  {LatinKey::E, Qt::Key_E, 26, 18, 18, "E"},
  {LatinKey::F, Qt::Key_F, 41, 33, 33, "F"},
  {LatinKey::G, Qt::Key_G, 42, 34, 34, "G"},
  {LatinKey::H, Qt::Key_H, 43, 35, 35, "H"},
  {LatinKey::I, Qt::Key_I, 31, 23, 23, "I"},
  {LatinKey::J, Qt::Key_J, 44, 36, 36, "J"},
  {LatinKey::K, Qt::Key_K, 45, 37, 37, "K"},
  {LatinKey::L, Qt::Key_L, 46, 38, 38, "L"},
  {LatinKey::O, Qt::Key_O, 32, 24, 24, "O"},
  {LatinKey::R, Qt::Key_R, 27, 19, 19, "R"},
  {LatinKey::T, Qt::Key_T, 28, 20, 20, "T"},
  {LatinKey::U, Qt::Key_U, 30, 22, 22, "U"},
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
        const bool primaryMatched =
          matcher.matchesLatinKey(expectation.latinKey, 0, expectation.linuxPrimaryScanCode);
        const bool alternateMatched =
          matcher.matchesLatinKey(expectation.latinKey, 0, expectation.linuxAlternateScanCode);
        ok &= expect(primaryMatched,
                     std::string("primary Linux scan code matches LatinKey::") +
                       expectation.label);
        ok &= expect(alternateMatched,
                     std::string("alternate Linux scan code matches LatinKey::") +
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

} // namespace

int
main()
{
    bool ok = true;
    ok &= testLogicalQtKeys();
    ok &= testLinuxScanCodes();
    ok &= testInvalidEnumValueReturnsFalse();
    ok &= testSpecialKeysDoNotUseScanCodeFallback();

    if (!ok)
        return 1;

    std::cout << "All layout-agnostic key tests passed\n";
    return 0;
}

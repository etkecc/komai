// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/LayoutAgnosticKeys.h"

#include <array>
#include <cstddef>

#include <QtCore/qglobal.h>
#include <QtCore/qnamespace.h>

#include "logging/Logging.h"

namespace {

using LatinKey = LayoutAgnosticKeys::LatinKey;

struct NativeLatinKeyDefinition
{
    int logicalQtKey;
    // evdev hardware scan code for this key position. On X11 (XKB), Qt reports
    // evdev + 8 as the native scan code, so both conventions are derived from
    // this single value to avoid cross-key collisions.
    quint32 linuxEvdevScanCode;
    quint32 windowsScanCode;
};

constexpr auto kNativeLatinKeys = std::to_array<NativeLatinKeyDefinition>({
  {.logicalQtKey = Qt::Key_D, .linuxEvdevScanCode = 32, .windowsScanCode = 32},
  {.logicalQtKey = Qt::Key_E, .linuxEvdevScanCode = 18, .windowsScanCode = 18},
  {.logicalQtKey = Qt::Key_F, .linuxEvdevScanCode = 33, .windowsScanCode = 33},
  {.logicalQtKey = Qt::Key_G, .linuxEvdevScanCode = 34, .windowsScanCode = 34},
  {.logicalQtKey = Qt::Key_H, .linuxEvdevScanCode = 35, .windowsScanCode = 35},
  {.logicalQtKey = Qt::Key_I, .linuxEvdevScanCode = 23, .windowsScanCode = 23},
  {.logicalQtKey = Qt::Key_J, .linuxEvdevScanCode = 36, .windowsScanCode = 36},
  {.logicalQtKey = Qt::Key_K, .linuxEvdevScanCode = 37, .windowsScanCode = 37},
  {.logicalQtKey = Qt::Key_L, .linuxEvdevScanCode = 38, .windowsScanCode = 38},
  {.logicalQtKey = Qt::Key_O, .linuxEvdevScanCode = 24, .windowsScanCode = 24},
  {.logicalQtKey = Qt::Key_R, .linuxEvdevScanCode = 19, .windowsScanCode = 19},
  {.logicalQtKey = Qt::Key_T, .linuxEvdevScanCode = 20, .windowsScanCode = 20},
  {.logicalQtKey = Qt::Key_U, .linuxEvdevScanCode = 22, .windowsScanCode = 22},
});

constexpr std::size_t kLatinKeyCount = static_cast<std::size_t>(LatinKey::Count);

// XKB keycodes are evdev scan codes offset by 8. Qt reports XKB keycodes as
// nativeScanCode on both X11 and Wayland, so we always apply this offset.
constexpr quint32 kXkbEvdevOffset = 8;

static_assert(kNativeLatinKeys.size() == kLatinKeyCount);

bool
isValidLatinKey(LatinKey latinKey)
{
    const auto index = static_cast<int>(latinKey);
    return index >= 0 && static_cast<std::size_t>(index) < kLatinKeyCount;
}

std::size_t
latinKeyIndex(LatinKey latinKey)
{
    return static_cast<std::size_t>(static_cast<int>(latinKey));
}

const NativeLatinKeyDefinition &
latinKeyDefinition(LatinKey latinKey)
{
    return kNativeLatinKeys[latinKeyIndex(latinKey)];
}

bool
matchesLinuxScanCode(quint32 nativeScanCode, const NativeLatinKeyDefinition &definition)
{
    if (nativeScanCode == 0)
        return false;

    return nativeScanCode == definition.linuxEvdevScanCode + kXkbEvdevOffset;
}

bool
canUseNativeScanCodeFallback(int key)
{
    if (key == 0 || key == Qt::Key_unknown)
        return true;

    // If the key already resolved to a Latin letter, the physical position is
    // unambiguous — scan-code fallback would only risk cross-key collisions.
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return false;

    // Native scan-code matching is only safe for printable text keys. Special keys like
    // Backspace can share platform keycodes with letter positions on some backends.
    return key >= Qt::Key_Space && key < Qt::Key_Escape;
}

}

LayoutAgnosticKeys::LayoutAgnosticKeys(QObject *parent)
  : QObject(parent)
{
}

bool
LayoutAgnosticKeys::matchesLatinKey(LatinKey latinKey, int key, quint32 nativeScanCode) const
{
    if (!isValidLatinKey(latinKey)) {
        nhlog::qml()->warn("LayoutAgnosticKeys called with invalid LatinKey value {}",
                           static_cast<int>(latinKey));
        return false;
    }

    const auto &definition = latinKeyDefinition(latinKey);

    if (key == definition.logicalQtKey)
        return true;

    if (!canUseNativeScanCodeFallback(key))
        return false;

#if defined(Q_OS_WIN)
    return nativeScanCode != 0 && nativeScanCode == definition.windowsScanCode;
#elif defined(Q_OS_MACOS)
    Q_UNUSED(nativeScanCode)
    Q_UNUSED(definition)
    return false;
#else
    return matchesLinuxScanCode(nativeScanCode, definition);
#endif
}

#include "moc_LayoutAgnosticKeys.cpp"

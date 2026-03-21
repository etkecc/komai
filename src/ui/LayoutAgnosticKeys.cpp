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
    std::array<quint32, 2> linuxScanCodes;
    quint32 windowsScanCode;
};

constexpr auto kNativeLatinKeys = std::to_array<NativeLatinKeyDefinition>({
  {.logicalQtKey = Qt::Key_D, .linuxScanCodes = {40, 32}, .windowsScanCode = 32},
  {.logicalQtKey = Qt::Key_E, .linuxScanCodes = {26, 18}, .windowsScanCode = 18},
  {.logicalQtKey = Qt::Key_F, .linuxScanCodes = {41, 33}, .windowsScanCode = 33},
  {.logicalQtKey = Qt::Key_G, .linuxScanCodes = {42, 34}, .windowsScanCode = 34},
  {.logicalQtKey = Qt::Key_H, .linuxScanCodes = {43, 35}, .windowsScanCode = 35},
  {.logicalQtKey = Qt::Key_I, .linuxScanCodes = {31, 23}, .windowsScanCode = 23},
  {.logicalQtKey = Qt::Key_J, .linuxScanCodes = {44, 36}, .windowsScanCode = 36},
  {.logicalQtKey = Qt::Key_K, .linuxScanCodes = {45, 37}, .windowsScanCode = 37},
  {.logicalQtKey = Qt::Key_L, .linuxScanCodes = {46, 38}, .windowsScanCode = 38},
  {.logicalQtKey = Qt::Key_O, .linuxScanCodes = {32, 24}, .windowsScanCode = 24},
  {.logicalQtKey = Qt::Key_R, .linuxScanCodes = {27, 19}, .windowsScanCode = 19},
  {.logicalQtKey = Qt::Key_T, .linuxScanCodes = {28, 20}, .windowsScanCode = 20},
  {.logicalQtKey = Qt::Key_U, .linuxScanCodes = {30, 22}, .windowsScanCode = 22},
});

constexpr std::size_t kLatinKeyCount = static_cast<std::size_t>(LatinKey::Count);

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
    return nativeScanCode != 0 && (nativeScanCode == definition.linuxScanCodes[0] ||
                                   nativeScanCode == definition.linuxScanCodes[1]);
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

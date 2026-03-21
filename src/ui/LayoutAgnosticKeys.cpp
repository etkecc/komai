// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/LayoutAgnosticKeys.h"

#include <array>

#include <QtCore/qglobal.h>
#include <QtCore/qnamespace.h>

namespace {

struct NativeLatinKeyDefinition
{
    QChar latinKey;
    int logicalQtKey;
    std::array<quint32, 2> linuxScanCodes;
    quint32 windowsScanCode;
};

constexpr auto kNativeLatinKeys = std::to_array<NativeLatinKeyDefinition>({
  {.latinKey        = QLatin1Char('D'),
   .logicalQtKey    = Qt::Key_D,
   .linuxScanCodes  = {40, 32},
   .windowsScanCode = 32},
  {.latinKey        = QLatin1Char('E'),
   .logicalQtKey    = Qt::Key_E,
   .linuxScanCodes  = {26, 18},
   .windowsScanCode = 18},
  {.latinKey        = QLatin1Char('F'),
   .logicalQtKey    = Qt::Key_F,
   .linuxScanCodes  = {41, 33},
   .windowsScanCode = 33},
  {.latinKey        = QLatin1Char('G'),
   .logicalQtKey    = Qt::Key_G,
   .linuxScanCodes  = {42, 34},
   .windowsScanCode = 34},
  {.latinKey        = QLatin1Char('I'),
   .logicalQtKey    = Qt::Key_I,
   .linuxScanCodes  = {31, 23},
   .windowsScanCode = 23},
  {.latinKey        = QLatin1Char('J'),
   .logicalQtKey    = Qt::Key_J,
   .linuxScanCodes  = {44, 36},
   .windowsScanCode = 36},
  {.latinKey        = QLatin1Char('K'),
   .logicalQtKey    = Qt::Key_K,
   .linuxScanCodes  = {45, 37},
   .windowsScanCode = 37},
  {.latinKey        = QLatin1Char('O'),
   .logicalQtKey    = Qt::Key_O,
   .linuxScanCodes  = {32, 24},
   .windowsScanCode = 24},
  {.latinKey        = QLatin1Char('R'),
   .logicalQtKey    = Qt::Key_R,
   .linuxScanCodes  = {27, 19},
   .windowsScanCode = 19},
  {.latinKey        = QLatin1Char('T'),
   .logicalQtKey    = Qt::Key_T,
   .linuxScanCodes  = {28, 20},
   .windowsScanCode = 20},
  {.latinKey        = QLatin1Char('U'),
   .logicalQtKey    = Qt::Key_U,
   .linuxScanCodes  = {30, 22},
   .windowsScanCode = 22},
});

const NativeLatinKeyDefinition *
findLatinKeyDefinition(const QString &latinKey)
{
    const QString trimmed = latinKey.trimmed().toUpper();
    if (trimmed.size() != 1)
        return nullptr;

    const QChar requestedKey = trimmed.front();
    for (const auto &definition : kNativeLatinKeys) {
        if (definition.latinKey == requestedKey)
            return &definition;
    }

    return nullptr;
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
LayoutAgnosticKeys::matchesLatinKey(const QString &latinKey, int key, quint32 nativeScanCode) const
{
    const auto *definition = findLatinKeyDefinition(latinKey);
    if (!definition)
        return false;

    if (key == definition->logicalQtKey)
        return true;

#if defined(Q_OS_WIN)
    return nativeScanCode != 0 && nativeScanCode == definition->windowsScanCode;
#elif defined(Q_OS_MACOS)
    return false;
#else
    return matchesLinuxScanCode(nativeScanCode, *definition);
#endif
}

#include "moc_LayoutAgnosticKeys.cpp"

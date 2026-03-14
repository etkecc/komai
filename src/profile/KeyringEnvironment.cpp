// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "profile/KeyringEnvironment.h"

#include <QCryptographicHash>
#include <QStandardPaths>

namespace keyring_environment {

/// The Flatpak-sandboxed config root suffix, e.g. ".../.var/app/cc.etke.komai/config/komai".
constexpr auto kFlatpakConfigSuffix = "/.var/app/cc.etke.komai/config/komai";

/// Snap sandboxes place config under ~/snap/<name>/<rev>/.config/komai.
constexpr auto kSnapMarker = "/snap/";

/// Native (non-sandboxed) config root suffixes per platform.
constexpr auto kLinuxNativeConfigSuffix   = "/.config/komai";
constexpr auto kMacOSNativeConfigSuffix   = "/Library/Preferences/komai";
constexpr auto kWindowsNativeConfigSuffix = "/AppData/Local/komai";

namespace {

QString
resolveTag()
{
    const auto configRoot =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
      QStringLiteral("/komai");

    return tagForConfigRoot(configRoot);
}

} // namespace

QString
tagForConfigRoot(QStringView configRoot)
{
    if (configRoot.endsWith(QLatin1String(kFlatpakConfigSuffix)))
        return QStringLiteral("flatpak");

    // Snap paths contain "/snap/" and end with the native Linux suffix.
    // Check before the native suffix to avoid misclassifying Snap as native.
    if (configRoot.contains(QLatin1String(kSnapMarker)) &&
        configRoot.endsWith(QLatin1String(kLinuxNativeConfigSuffix)))
        return QStringLiteral("snap");

    if (configRoot.endsWith(QLatin1String(kLinuxNativeConfigSuffix)) ||
        configRoot.endsWith(QLatin1String(kMacOSNativeConfigSuffix)) ||
        configRoot.endsWith(QLatin1String(kWindowsNativeConfigSuffix)))
        return QStringLiteral("native");

    // Unknown environment — use a short hash of the config root path.
    const auto hash =
      QCryptographicHash::hash(configRoot.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(hash.left(6));
}

const QString &
tag()
{
    static const QString value = resolveTag();
    return value;
}

const QString &
prefix()
{
    static const QString value = QStringLiteral("komai.") + tag() + QStringLiteral(".");
    return value;
}

} // namespace keyring_environment

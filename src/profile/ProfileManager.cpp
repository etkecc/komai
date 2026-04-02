// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "profile/ProfileManager.h"
#include "komai-rust-cxxbridge/ffi.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <algorithm>

#include "logging/Logging.h"
#include "profile/Paths.h"
#include "profile/ProfileId.h"
#include "settings/SettingsPersistence.h"
#include "settings/core/SettingsDefinitions.h"
#include "ui/Theme.h"

namespace {

void
setError(QString *errorOut, const QString &message)
{
    if (errorOut)
        *errorOut = message;
}

QString
profilesRoot(QStandardPaths::StandardLocation location)
{
    return QStandardPaths::writableLocation(location) + QStringLiteral("/komai/profiles");
}

QStringList
profileIdsFromRoot(QStandardPaths::StandardLocation location)
{
    const QDir dir(profilesRoot(location));
    if (!dir.exists())
        return {};

    return dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
}

void
sortProfileIds(QStringList &profileIds)
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);

    std::sort(
      profileIds.begin(), profileIds.end(), [&collator](const QString &lhs, const QString &rhs) {
          return collator.compare(lhs, rhs) < 0;
      });
}

bool
removeDirectoryRecursively(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists())
        return true;

    QDir dir(path);
    return dir.removeRecursively();
}

bool
launchDetached(const QStringList &arguments, QString *errorOut)
{
    const QString executablePath = QCoreApplication::applicationFilePath();
    if (executablePath.isEmpty()) {
        setError(errorOut,
                 QObject::tr("Unable to determine current executable path for profile launch."));
        return false;
    }

    const QString workingDirectory = QFileInfo(executablePath).absolutePath();
    QProcess detachedProcess;
    detachedProcess.setProgram(executablePath);
    detachedProcess.setArguments(arguments);
    detachedProcess.setWorkingDirectory(workingDirectory);
    // Fully detach launched profile/selector windows from the caller terminal.
    // Logs still go to profile log files via the app logger's file sink.
    const QString nullDevice = QProcess::nullDevice();
    detachedProcess.setStandardInputFile(nullDevice);
    detachedProcess.setStandardOutputFile(nullDevice);
    detachedProcess.setStandardErrorFile(nullDevice);
    if (!detachedProcess.startDetached()) {
        setError(
          errorOut,
          QObject::tr("Failed to launch a detached Komai process for the selected profile."));
        return false;
    }

    return true;
}

} // namespace

namespace profile_manager {

QStringList
listProfileIds()
{
    // Treat profiles as explicitly config-backed identities only.
    // We intentionally do not infer profiles from data/cache roots, so stale
    // leftovers under ~/.local/share or ~/.cache do not resurrect deleted/moved
    // profiles in the UI/startup selector.
    QStringList profileIds;
    for (const auto &profileId : profileIdsFromRoot(QStandardPaths::GenericConfigLocation)) {
        if (profile_id::validate(profileId).has_value())
            continue;
        profileIds.push_back(profileId);
    }

    sortProfileIds(profileIds);
    return profileIds;
}

QVector<ProfileSummary>
listProfiles(QStringView currentProfile)
{
    const auto profileIds      = listProfileIds();
    const auto currentProfile_ = app_paths::normalizedProfileId(currentProfile);

    QVector<ProfileSummary> summaries;
    summaries.reserve(profileIds.size());

    for (const auto &profileId : profileIds) {
        ProfileSummary summary;
        summary.id        = profileId;
        summary.isDefault = (profileId == QLatin1String("default"));
        summary.isCurrent = (profileId == currentProfile_);

        const auto config =
          ::komai::rust::settings_load_config_overview_for_profile(profileId.toStdString());
        const auto session =
          ::komai::rust::settings_load_session_snapshot_for_profile(profileId.toStdString());

        summary.themeSlug =
          QString::fromStdString(static_cast<std::string>(config.theme_slug)).trimmed();
        if (summary.themeSlug.isEmpty())
            summary.themeSlug =
              QString::fromLatin1(settings::core::definitions::kDefaultUiThemeSlug);
        if (summary.themeSlug.trimmed().isEmpty())
            summary.themeSlug =
              QString::fromLatin1(settings::core::definitions::kDefaultUiThemeSlug);

        summary.secretsProvider =
          QString::fromStdString(static_cast<std::string>(config.secrets_provider));
        if (summary.secretsProvider.isEmpty())
            summary.secretsProvider = QStringLiteral("unknown");

        summary.userId     = QString::fromStdString(static_cast<std::string>(session.user_id));
        summary.homeserver = QString::fromStdString(static_cast<std::string>(session.homeserver));

        const auto palette      = Theme::paletteFromTheme(summary.themeSlug);
        summary.accentColor     = palette.color(QPalette::Highlight);
        summary.windowColor     = palette.color(QPalette::Window);
        summary.darkColor       = palette.color(QPalette::Dark);
        summary.textColor       = palette.color(QPalette::Text);
        summary.brightTextColor = palette.color(QPalette::BrightText);

        summaries.push_back(summary);
    }

    return summaries;
}

bool
shouldShowStartupSelector(bool profileArgumentProvided, QStringView selectedProfile)
{
    if (profileArgumentProvided && !selectedProfile.isEmpty())
        return false;

    const auto profileIds = listProfileIds();
    if (profileIds.isEmpty())
        return false;

    return !(profileIds.size() == 1 && profileIds.first() == QLatin1String("default"));
}

std::optional<QString>
validateNewProfileId(QStringView profileId)
{
    const auto trimmed = profileId.toString().trimmed();
    if (trimmed.isEmpty())
        return QObject::tr("Profile name is required.");

    if (const auto validationError = profile_id::validate(trimmed); validationError)
        return *validationError;

    return std::nullopt;
}

bool
launchProfileDetached(QStringView profileId, QString *errorOut)
{
    const auto normalized = profile_id::normalized(profileId);

    if (const auto validationError = profile_id::validate(normalized); validationError) {
        setError(errorOut, QObject::tr("Invalid profile name: %1").arg(*validationError));
        return false;
    }

    return launchDetached({QStringLiteral("-p"), normalized}, errorOut);
}

bool
launchStartupSelectorDetached(QString *errorOut)
{
    return launchDetached({QStringLiteral("--profile=")}, errorOut);
}

bool
deleteProfile(QStringView profileId,
              QStringView currentProfileId,
              QString *errorOut,
              bool protectCurrentProfile)
{
    const auto normalizedTargetProfile  = profile_id::normalized(profileId);
    const auto normalizedCurrentProfile = profile_id::normalized(currentProfileId);

    if (const auto validationError = profile_id::validate(normalizedTargetProfile);
        validationError) {
        setError(errorOut, QObject::tr("Invalid profile name: %1").arg(*validationError));
        return false;
    }

    if (protectCurrentProfile && normalizedTargetProfile == normalizedCurrentProfile) {
        setError(errorOut,
                 QObject::tr("Cannot delete the currently active profile from this instance."));
        return false;
    }

    const auto config = ::komai::rust::settings_load_config_overview_for_profile(
      normalizedTargetProfile.toStdString());
    const auto secretsProvider = settings::persistence::providerFromConfigValue(
      QString::fromStdString(static_cast<std::string>(config.secrets_provider)));

    const auto secretsFilePath = app_paths::config::profileSecretsFile(normalizedTargetProfile);
    const bool secretsRemoved  = settings::persistence::clearProfileSecrets(
      normalizedTargetProfile,
      secretsProvider == staged_load_plan::SecretsProvider::File,
      secretsFilePath);

    const auto configProfileDir =
      QFileInfo(app_paths::config::profileConfigFile(normalizedTargetProfile)).absolutePath();
    const auto dataProfileDir  = app_paths::data::profileDirectory(normalizedTargetProfile);
    const auto cacheProfileDir = app_paths::cache::profileDirectory(normalizedTargetProfile);

    const bool configRemoved = removeDirectoryRecursively(configProfileDir);
    const bool dataRemoved   = removeDirectoryRecursively(dataProfileDir);
    const bool cacheRemoved  = removeDirectoryRecursively(cacheProfileDir);

    if (!configRemoved || !dataRemoved || !cacheRemoved) {
        setError(errorOut,
                 QObject::tr("Failed to remove one or more profile directories for '%1'.")
                   .arg(normalizedTargetProfile));
        return false;
    }

    if (!secretsRemoved) {
        setError(
          errorOut,
          QObject::tr(
            "Profile files were deleted, but secure-store secret cleanup was incomplete for '%1'.")
            .arg(normalizedTargetProfile));
        return false;
    }

    return true;
}

} // namespace profile_manager

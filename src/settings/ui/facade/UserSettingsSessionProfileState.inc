// Include here because this inline unit is compiled into multiple translation units.
#include "ProfileId.h"

void
UserSettings::setUsesFileSecretsProvider(bool usesFileSecretsProvider)
{
    usesFileSecretsProvider_ = usesFileSecretsProvider;
}

void
UserSettings::setSecretsProviderFallbackWarningVisible(bool visible)
{
    if (secretsProviderFallbackWarningVisible_ == visible)
        return;

    secretsProviderFallbackWarningVisible_ = visible;
    emit secretsProviderFallbackWarningVisibleChanged(visible);
}

bool
UserSettings::hasResolvedProfilePaths() const
{
    return !profileDirPath_.isEmpty();
}

const QString &
UserSettings::profileId() const
{
    return profile_;
}

const QString &
UserSettings::profileDirPath() const
{
    return profileDirPath_;
}

const QString &
UserSettings::configFilePath() const
{
    return configFilePath_;
}

const QString &
UserSettings::stateFilePath() const
{
    return stateFilePath_;
}

const QString &
UserSettings::sessionFilePath() const
{
    return sessionFilePath_;
}

const QString &
UserSettings::secretsFilePath() const
{
    return secretsFilePath_;
}

const QMap<QString, QString> &
UserSettings::secretsMap() const
{
    return secrets_;
}

void
UserSettings::notifyProfileChanged()
{
    emit profileChanged(profile_);
}

void
UserSettings::applyProfilePathState(const QString &profile)
{
    profile_         = profile;
    profileDirPath_  = settings::storage::profileDirPath(profile_);
    configFilePath_  = configFilePathForProfile(profile_);
    stateFilePath_   = stateFilePathForProfile(profile_);
    sessionFilePath_ = sessionFilePathForProfile(profile_);
    secretsFilePath_ = secretsFilePathForProfile(profile_);
}

void
UserSettings::setProfile(QString profile)
{
    if (const auto validationError = profile_id::validate(profile); validationError) {
        nhlog::ui()->warn("Rejected invalid profile id '{}': {}",
                          profile.toStdString(),
                          validationError->toStdString());
        return;
    }

    // always set this to allow setting this when loading and it is overwritten on the cli
    applyProfilePathState(profile);
    emit profileChanged(profile_);
    save();
}

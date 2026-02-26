// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QGuiApplication>

#include <stdexcept>
#include <yaml-cpp/yaml.h>

#include "JdenticonProvider.h"
#include "Logging.h"
#include "ProfileId.h"
#include "settings/SettingsController.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/Theme.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
// Resolve the fontconfig generic "emoji" alias to an actual font family name.
// Qt < 6.9 can't resolve fontconfig generic aliases in <font face=""> or QML font.family,
// so we pick the best available emoji font from QFontDatabase.
static QString
resolveEmojiFontFamily()
{
    // Well-known emoji fonts in preference order (matching fontconfig's 60-generic.conf).
    static const QStringList preferredEmojiFonts = {
      QStringLiteral("Noto Color Emoji"),
      QStringLiteral("Apple Color Emoji"),
      QStringLiteral("Segoe UI Emoji"),
      QStringLiteral("Twitter Color Emoji"),
      QStringLiteral("JoyPixels"),
      QStringLiteral("Emoji One"),
    };

    const auto available = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);

    for (const auto &preferred : preferredEmojiFonts) {
        if (available.contains(preferred)) {
            nhlog::ui()->info("Emoji font: using \"{}\"", preferred.toStdString());
            return preferred;
        }
    }

    nhlog::ui()->warn(
      "Emoji font: no suitable font found (install e.g. Noto Color Emoji for emoji support)");
    return {};
}
#endif

QSharedPointer<UserSettings> UserSettings::instance_;

UserSettings::UserSettings()
{
    connect(
      QCoreApplication::instance(), &QCoreApplication::aboutToQuit, []() { instance_.clear(); });
}

QSharedPointer<UserSettings>
UserSettings::instance()
{
    return instance_;
}

void
UserSettings::initialize(std::optional<QString> profile)
{
    instance_.reset(new UserSettings());
    instance_->load(profile);
}

void
UserSettings::initialize(std::optional<QString> profile, const YAML::Node &configRoot)
{
    instance_.reset(new UserSettings());
    instance_->load(profile, configRoot);
}

void
UserSettings::load(std::optional<QString> profile)
{
    if (profile) {
        if (const auto validationError = profile_id::validate(*profile); validationError) {
            throw std::runtime_error(
              QStringLiteral("Invalid profile id: %1").arg(*validationError).toStdString());
        }
    }

    settings::SettingsController controller;
    controller.loadAndMigrate(*this, profile);
}

void
UserSettings::load(std::optional<QString> profile, const YAML::Node &configRoot)
{
    if (profile) {
        if (const auto validationError = profile_id::validate(*profile); validationError) {
            throw std::runtime_error(
              QStringLiteral("Invalid profile id: %1").arg(*validationError).toStdString());
        }
    }

    settings::SettingsController controller;
    controller.loadAndMigrate(*this, profile, configRoot);
}

void
UserSettings::setPersistenceSuspended(bool suspended)
{
    suppressSettingsSave_ = suspended;
}

void
UserSettings::setPersistenceScopeReadyForAuth(bool ready)
{
    startupPersistenceScope_ =
      ready ? StartupPersistenceScope::Full : StartupPersistenceScope::ConfigOnly;
}

QString
UserSettings::uiFontEmojiFamily() const
{
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    // Qt < 6.9 needs a real font family name for <font face=""> and QML font.family.
    // Cache the resolved value so we don't scan QFontDatabase on every call.
    if (uiFontEmojiFamily_.isEmpty()) {
        static const QString resolved = resolveEmojiFontFamily();
        return resolved;
    }
#endif
    return uiFontEmojiFamily_;
}

bool
UserSettings::uiAvatarsIdenticonFallback() const
{
    const auto enabled = [this]() {
        if (const auto value =
              coreStore_.valueAs<bool>(settings::core::SettingId::UiAvatarsIdenticonFallback);
            value.has_value())
            return *value;
        return uiAvatarsIdenticonFallback_;
    }();
    return enabled && JdenticonProvider::isAvailable();
}

void
UserSettings::applyTheme()
{
    QGuiApplication::setPalette(Theme::paletteFromTheme(this->uiThemeSlug()));
    QApplication::setPalette(Theme::paletteFromTheme(this->uiThemeSlug()));
}

void
UserSettings::save()
{
    if (suppressSettingsSave_)
        return;

    settings::SettingsController controller;
    if (startupPersistenceScope_ == StartupPersistenceScope::ConfigOnly) {
        nhlog::ui()->debug("Startup settings persistence in config-only mode; skipping "
                           "state/session/secrets writes");
        controller.save(*this, settings::SettingsController::SavePolicy::ConfigOnly);
    } else {
        if (!hasActiveSession()) {
            startupPersistenceScope_ = StartupPersistenceScope::ConfigOnly;
            nhlog::ui()->warn(
              "Startup settings persistence requested with incomplete session identity "
              "(has_user_id={}, "
              "has_access_token={}, has_device_id={}, has_homeserver={}); forcing config-only save",
              userId_.trimmed().isEmpty() ? "false" : "true",
              accessToken_.trimmed().isEmpty() ? "false" : "true",
              deviceId_.trimmed().isEmpty() ? "false" : "true",
              homeserver_.trimmed().isEmpty() ? "false" : "true");
            controller.save(*this, settings::SettingsController::SavePolicy::ConfigOnly);
        } else {
            controller.save(*this, settings::SettingsController::SavePolicy::Full);
        }
    }
}

#include "moc_UserSettingsPage.cpp"

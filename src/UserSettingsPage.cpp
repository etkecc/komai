// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QGuiApplication>

#include <yaml-cpp/yaml.h>

#include "JdenticonProvider.h"
#include "UserSettingsPage.h"
#include "settings/SettingsController.h"
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
    settings::SettingsController controller;
    controller.load(*this, profile);
}

void
UserSettings::load(std::optional<QString> profile, const YAML::Node &configRoot)
{
    settings::SettingsController controller;
    controller.load(*this, profile, configRoot);
}

void
UserSettings::setPersistenceSuspended(bool suspended)
{
    suppressSettingsSave_ = suspended;
}

QString
UserSettings::emojiFont() const
{
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    // Qt < 6.9 needs a real font family name for <font face=""> and QML font.family.
    // Cache the resolved value so we don't scan QFontDatabase on every call.
    if (emojiFont_.isEmpty()) {
        static const QString resolved = resolveEmojiFontFamily();
        return resolved;
    }
#endif
    return emojiFont_;
}

bool
UserSettings::useIdenticon() const
{
    return useIdenticon_ && JdenticonProvider::isAvailable();
}

void
UserSettings::applyTheme()
{
    QGuiApplication::setPalette(Theme::paletteFromTheme(this->theme()));
    QApplication::setPalette(Theme::paletteFromTheme(this->theme()));
}

void
UserSettings::save()
{
    if (suppressSettingsSave_)
        return;

    settings::SettingsController controller;
    controller.save(*this);
}

#include "moc_UserSettingsPage.cpp"

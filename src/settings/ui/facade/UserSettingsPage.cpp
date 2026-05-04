// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QGuiApplication>

#include <stdexcept>

#include "logging/Logging.h"
#include "profile/ProfileId.h"
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
            komai::logging::ui()->info("Emoji font: using \"{}\"", preferred.toStdString());
            return preferred;
        }
    }

    komai::logging::ui()->warn(
      "Emoji font: no suitable font found (install e.g. Noto Color Emoji for emoji support)");
    return {};
}
#endif

QSharedPointer<UserSettings> UserSettings::instance_;

UserSettings::UserSettings()
{
    deferredStateSaveTimer_.setSingleShot(true);
    deferredStateSaveTimer_.setInterval(150);
    connect(
      &deferredStateSaveTimer_, &QTimer::timeout, this, [this]() { flushDeferredStateSave(); });

    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this]() {
        flushDeferredStateSave();
        instance_.clear();
    });

    // composerInputSendKeyLabel returns a tr()-translated string derived from
    // composerInputSendKey, so its NOTIFY needs to fire on both the underlying
    // setting change and on language switches. Queued on uiLanguageChanged so
    // bindings re-evaluate AFTER MainApplication has installed the new
    // translators (mirrors KomaiGlobalObject's localizedStringsChanged wiring).
    connect(this, &UserSettings::composerInputSendKeyChanged, this, [this](SendMessageKey) {
        emit composerInputSendKeyLabelChanged();
    });
    connect(
      this,
      &UserSettings::uiLanguageChanged,
      this,
      [this](QString) { emit composerInputSendKeyLabelChanged(); },
      Qt::QueuedConnection);
}

QSharedPointer<UserSettings>
UserSettings::instance()
{
    return instance_;
}

void
UserSettings::setNotificationsAccountRuntimeHooks(NotificationsAccountHandleProvider handleProvider,
                                                  NotificationsAccountFetchFn fetchFn,
                                                  NotificationsAccountSetFn setFn)
{
    notificationsAccountHandleProvider_ = std::move(handleProvider);
    notificationsAccountFetchFn_        = std::move(fetchFn);
    notificationsAccountSetFn_          = std::move(setFn);
}

void
UserSettings::initialize(std::optional<QString> profile, LoadPolicy loadPolicy)
{
    instance_.reset(new UserSettings());
    instance_->load(profile, loadPolicy);
}

void
UserSettings::load(std::optional<QString> profile, LoadPolicy loadPolicy)
{
    if (profile) {
        if (const auto validationError = profile_id::validate(*profile); validationError) {
            throw std::runtime_error(
              QStringLiteral("Invalid profile id: %1").arg(*validationError).toStdString());
        }
    }

    settings::SettingsController controller;
    const auto controllerPolicy = loadPolicy == LoadPolicy::ConfigAndStateOnly
                                    ? settings::SettingsController::LoadPolicy::ConfigAndStateOnly
                                    : settings::SettingsController::LoadPolicy::Full;
    controller.loadAndMigrate(*this, profile, controllerPolicy);
}

void
UserSettings::setPersistenceSuspended(bool suspended)
{
    suppressSettingsSave_ = suspended;

    if (suspended) {
        deferredStateSaveTimer_.stop();
    } else if (deferredStateSavePending_) {
        deferredStateSaveTimer_.start();
    }
}

void
UserSettings::setPersistenceScopeReadyForAuth(bool ready)
{
    startupPersistenceScope_ =
      ready ? StartupPersistenceScope::Full : StartupPersistenceScope::ConfigOnly;
}

void
UserSettings::scheduleDeferredStateSave()
{
    deferredStateSavePending_ = true;

    if (suppressSettingsSave_)
        return;

    deferredStateSaveTimer_.start();
}

void
UserSettings::flushDeferredStateSave()
{
    if (!deferredStateSavePending_)
        return;

    if (suppressSettingsSave_)
        return;

    deferredStateSavePending_ = false;
    settings::SettingsController controller;
    controller.save(*this, settings::SettingsController::SavePolicy::StateOnly);
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

UserSettings::DefaultAvatarStyle
UserSettings::uiAvatarsDefaultAvatarStyle() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::UiAvatarsDefaultAvatarStyle);
        value.has_value() && *value >= static_cast<int>(DefaultAvatarStyle::LetterInitial) &&
        *value <= static_cast<int>(DefaultAvatarStyle::BoringAvatarsBauhaus))
        return static_cast<DefaultAvatarStyle>(*value);
    return uiAvatarsDefaultAvatarStyle_;
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
        komai::logging::ui()->debug("Startup settings persistence in config-only mode; skipping "
                                    "state/session/secrets writes");
        controller.save(*this, settings::SettingsController::SavePolicy::ConfigOnly);
    } else {
        if (!hasActiveSession()) {
            startupPersistenceScope_ = StartupPersistenceScope::ConfigOnly;
            komai::logging::ui()->warn(
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

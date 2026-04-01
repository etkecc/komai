// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsController.h"
#include "SettingsControllerInternal.h"

#include "logging/Logging.h"
#include <utility>

#include "settings/core/SettingsConstraints.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/facade/UserSettingsCoreStoreBridge.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include <string>
#include <string_view>

namespace {

settings::ControllerLoggers
defaultLoggers()
{
    return {.ui = std::make_shared<nhlog::Logger>("settings-controller-ui")};
}

settings::ControllerLoggers &
currentLoggers()
{
    static settings::ControllerLoggers loggers = defaultLoggers();
    return loggers;
}

} // namespace

void
settings::syncCoreStoreFromSettings(UserSettings &settings)
{
    auto &store = settings.mutableCoreStore();
    store.clear();
    settings::core::constraints::applyDefaultConstraints(store);

    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        const auto value =
          settings::ui::facade::coreStoreValueForSettingId(settings, definition.id);
        if (!value.has_value()) {
            currentLoggers().ui->warn("No core-store mapping for setting id {}",
                                      static_cast<int>(definition.id));
            continue;
        }

        const auto result = store.setValue(definition.id, *value);
        if (!result.success) {
            currentLoggers().ui->warn("Invalid value for setting id {} ignored: {}",
                                      static_cast<int>(definition.id),
                                      result.validationError);
        }
    }
}

void
settings::setLoggers(settings::ControllerLoggers loggers)
{
    const auto &defaults = defaultLoggers();
    if (!loggers.ui)
        loggers.ui = defaults.ui;
    currentLoggers() = std::move(loggers);
}

const settings::ControllerLoggers &
settings::activeLoggers()
{
    return currentLoggers();
}

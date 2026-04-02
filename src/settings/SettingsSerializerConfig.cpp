// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include <QString>

#include <utility>

#include "logging/Logging.h"

#include "SettingsSerializerConfigInternal.h"

namespace settings::serializer {

namespace {

SerializerLoggers
defaultLoggers()
{
    return {.ui = std::make_shared<nhlog::Logger>("settings-serializer-ui")};
}

SerializerLoggers &
currentLoggers()
{
    static SerializerLoggers loggers = defaultLoggers();
    return loggers;
}

} // namespace

void
setLoggers(SerializerLoggers loggers)
{
    const auto &defaults = defaultLoggers();
    if (!loggers.ui)
        loggers.ui = defaults.ui;
    currentLoggers() = std::move(loggers);
}

const SerializerLoggers &
activeLoggers()
{
    return currentLoggers();
}

namespace detail {

namespace {

constexpr auto kUiInputModeDesktop = "desktop";
constexpr auto kUiInputModeTouch   = "touch";

} // namespace

QString
toStorageUiInputMode(bool uiInputMode)
{
    return QString::fromLatin1(uiInputMode ? kUiInputModeTouch : kUiInputModeDesktop);
}

bool
fromStorageUiInputMode(const QString &value)
{
    if (value.trimmed().compare(QLatin1String(kUiInputModeTouch), Qt::CaseInsensitive) == 0)
        return true;
    return false;
}

} // namespace detail

} // namespace settings::serializer

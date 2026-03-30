// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventExpiry.h"

#include "logging/Logging.h"
#include "ui/MainWindow.h"

namespace {
void
notifyEventExpirySaveUnavailable()
{
    nhlog::ui()->warn("Event expiry persistence is not migrated to matrix-sdk yet");
    MainWindow::instance()->showNotification(
      EventExpiry::tr("Saving event expiry is not available yet during the matrix-sdk migration."));
}
} // namespace

void
EventExpiry::load()
{
    this->event = {};
    emit expireEventsAfterDaysChanged();
    emit expireEventsAfterCountChanged();
    emit protectLatestEventsChanged();
    emit expireStateEventsChanged();
}

void
EventExpiry::save()
{
    notifyEventExpirySaveUnavailable();
}

int
EventExpiry::expireEventsAfterDays() const
{
    return event.expire_after_ms / (1000 * 60 * 60 * 24);
}

int
EventExpiry::expireEventsAfterCount() const
{
    return event.keep_only_latest;
}

int
EventExpiry::protectLatestEvents() const
{
    return event.protect_latest;
}

bool
EventExpiry::expireStateEvents() const
{
    return !event.exclude_state_events;
}

void
EventExpiry::setExpireEventsAfterDays(int val)
{
    if (val > 0)
        this->event.expire_after_ms = std::uint64_t(val) * (1000 * 60 * 60 * 24);
    else
        this->event.expire_after_ms = 0;
    emit expireEventsAfterDaysChanged();
}

void
EventExpiry::setProtectLatestEvents(int val)
{
    if (val > 0)
        this->event.protect_latest = std::uint64_t(val);
    else
        this->event.protect_latest = 0;
    emit protectLatestEventsChanged();
}

void
EventExpiry::setExpireEventsAfterCount(int val)
{
    if (val > 0)
        this->event.keep_only_latest = std::uint64_t(val);
    else
        this->event.keep_only_latest = 0;
    emit expireEventsAfterCountChanged();
}

void
EventExpiry::setExpireStateEvents(bool val)
{
    this->event.exclude_state_events = !val;
    emit expireStateEventsChanged();
}

#include "moc_EventExpiry.cpp"

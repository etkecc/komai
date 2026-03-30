// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "HiddenEvents.h"

#include "logging/Logging.h"
#include "timeline/TimelineEventTypes.h"
#include "ui/MainWindow.h"

namespace {
void
notifyHiddenEventsSaveUnavailable()
{
    nhlog::ui()->warn("Hidden event persistence is not migrated to matrix-sdk yet");
    MainWindow::instance()->showNotification(HiddenEvents::tr(
      "Saving hidden events is not available yet during the matrix-sdk migration."));
}
} // namespace

void
HiddenEvents::load()
{
    hiddenEvents_.clear();
    for (const auto &eventType : qml_mtx_events::defaultHiddenEventTypes()) {
        hiddenEvents_.push_back(int(qml_mtx_events::toRoomEventType(eventType)));
    }
    emit hiddenEventsChanged();
}

Q_INVOKABLE void
HiddenEvents::toggle(int type)
{
    if (auto it = std::find(begin(hiddenEvents_), end(hiddenEvents_), type);
        it != end(hiddenEvents_))
        hiddenEvents_.erase(it);
    else
        hiddenEvents_.push_back(type);
    emit hiddenEventsChanged();
}

QVariantList
HiddenEvents::hiddenEvents() const
{
    QVariantList l;
    for (const auto &e : hiddenEvents_) {
        l.push_back(e);
    }

    return l;
}

void
HiddenEvents::save()
{
    notifyHiddenEventsSaveUnavailable();
}

#include "moc_HiddenEvents.cpp"

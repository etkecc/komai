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
    hiddenEvents_ = qml_mtx_events::defaultHiddenEventTypes();
    emit hiddenEventsChanged();
}

Q_INVOKABLE void
HiddenEvents::toggle(int type)
{
    auto t = qml_mtx_events::fromRoomEventType(static_cast<qml_mtx_events::EventType>(type));
    if (auto it = std::find(begin(hiddenEvents_), end(hiddenEvents_), t); it != end(hiddenEvents_))
        hiddenEvents_.erase(it);
    else
        hiddenEvents_.push_back(t);
    emit hiddenEventsChanged();
}

QVariantList
HiddenEvents::hiddenEvents() const
{
    QVariantList l;
    for (const auto &e : hiddenEvents_) {
        l.push_back(qml_mtx_events::toRoomEventType(e));
    }

    return l;
}

void
HiddenEvents::save()
{
    notifyHiddenEventsSaveUnavailable();
}

#include "moc_HiddenEvents.cpp"

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "HiddenEvents.h"

#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/TimelineEventTypes.h"

namespace {
std::vector<int>
hiddenEventTypesFromKeys(const QStringList &keys)
{
    std::vector<int> hiddenEventTypes;
    hiddenEventTypes.reserve(static_cast<size_t>(keys.size()));

    for (const auto &key : keys) {
        const auto eventType = qml_mtx_events::localTimelineEventTypeFromKey(key);
        if (!eventType)
            continue;
        hiddenEventTypes.push_back(int(*eventType));
    }

    return hiddenEventTypes;
}

QStringList
hiddenEventKeysFromTypes(const std::vector<int> &hiddenEventTypes)
{
    QStringList keys;
    keys.reserve(static_cast<qsizetype>(hiddenEventTypes.size()));

    for (const auto eventType : hiddenEventTypes) {
        const auto key =
          qml_mtx_events::localTimelineEventTypeKey(qml_mtx_events::EventType(eventType));
        if (!key.isEmpty())
            keys.push_back(key);
    }

    return keys;
}
} // namespace

void
HiddenEvents::load()
{
    const auto settings = UserSettings::instance();
    hiddenEvents_ =
      settings ? hiddenEventTypesFromKeys(settings->hiddenTimelineEventTypesForRoom(roomid_))
               : hiddenEventTypesFromKeys(qml_mtx_events::defaultHiddenTimelineEventTypeKeys());
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
    if (const auto settings = UserSettings::instance()) {
        settings->setHiddenTimelineEventTypesForRoom(roomid_,
                                                     hiddenEventKeysFromTypes(hiddenEvents_));
    }
}

#include "moc_HiddenEvents.cpp"

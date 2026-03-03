// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventStore.h"

#include <algorithm>
#include <map>
#include <set>

#include "Reaction.h"
#include "cache/Cache.h"
#include "events/EventAccessors.h"
#include "utils/Utils.h"

std::vector<mtx::events::collections::TimelineEvents>
EventStore::edits(const std::string &event_id)
{
    auto event_ids = cache::relatedEvents(room_id_, event_id);

    auto original_event = get(event_id, "", false, false);
    if (!original_event ||
        std::holds_alternative<mtx::events::RoomEvent<mtx::events::msg::Redacted>>(*original_event))
        return {};

    const auto &original_sender    = mtx::accessors::sender(*original_event);
    const auto &original_relations = mtx::accessors::relations(*original_event);

    std::vector<mtx::events::collections::TimelineEvents> edits;
    for (const auto &id : event_ids) {
        auto related_event = get(id, event_id, false, false);
        if (!related_event)
            continue;

        const auto &edit_rel = mtx::accessors::relations(*related_event);
        if (edit_rel.replaces() == event_id &&
            original_sender == mtx::accessors::sender(*related_event)) {
            auto related_ev = *related_event;

            // Spec does not allow changing relations in an edit. So if we are not using the legacy
            // multi-relation format, just use the original relations + the edit...
            if (edit_rel.synthesized) {
                auto merged_relations        = original_relations;
                merged_relations.synthesized = true;
                merged_relations.relations.push_back(
                  {mtx::common::RelationType::Replace, event_id});
                mtx::accessors::set_relations(related_ev, std::move(merged_relations));
            }
            edits.push_back(std::move(related_ev));
        }
    }

    std::sort(edits.begin(),
              edits.end(),
              [this](const mtx::events::collections::TimelineEvents &a,
                     const mtx::events::collections::TimelineEvents &b) {
                  return cache::getEventIndex(this->room_id_, mtx::accessors::event_id(a)) <
                         cache::getEventIndex(this->room_id_, mtx::accessors::event_id(b));
              });

    return edits;
}

QVariantList
EventStore::reactions(const std::string &event_id)
{
    auto event_ids = cache::relatedEvents(room_id_, event_id);

    struct TempReaction
    {
        std::set<std::string> users;
        std::string reactedBySelf;
    };
    std::map<std::string, TempReaction> aggregation;
    std::vector<Reaction> reactions;

    auto self = utils::localUser().toStdString();
    for (const auto &id : event_ids) {
        auto related_event = get(id, event_id);
        if (!related_event)
            continue;

        if (auto reaction =
              std::get_if<mtx::events::RoomEvent<mtx::events::msg::Reaction>>(related_event);
            reaction && reaction->content.relations.annotates() &&
            reaction->content.relations.annotates()->key) {
            auto key  = reaction->content.relations.annotates()->key.value();
            auto &agg = aggregation[key];

            if (agg.users.empty()) {
                Reaction temp{};
                temp.key_ = QString::fromStdString(key);
                reactions.push_back(temp);
            }

            agg.users.insert(cache::displayName(room_id_, reaction->sender));
            if (reaction->sender == self)
                agg.reactedBySelf = reaction->event_id;
        }
    }

    QVariantList temp;
    temp.reserve(static_cast<int>(reactions.size()));
    for (auto &reaction : reactions) {
        const auto &agg            = aggregation[reaction.key_.toStdString()];
        reaction.count_            = agg.users.size();
        reaction.selfReactedEvent_ = QString::fromStdString(agg.reactedBySelf);

        bool firstReaction = true;
        for (const auto &user : agg.users) {
            if (firstReaction)
                firstReaction = false;
            else
                reaction.users_ += QLatin1String(", ");

            reaction.users_ += QString::fromStdString(user);
        }

        temp.append(QVariant::fromValue(reaction));
    }

    return temp;
}

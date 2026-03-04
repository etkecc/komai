// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/roomlist/RoomlistPreviewSelection.h"

#include <algorithm>

namespace timeline::roomlist {

MaterializedPreviewFields
selectMaterializedPreviewFields(const DescInfo &liveDescription,
                                quint64 liveTimestamp,
                                bool hasLiveMessagePreview,
                                const std::optional<DescInfo> &cachedDescription,
                                quint64 approximateLastModificationTs)
{
    MaterializedPreviewFields fields;

    // Keep room-list preview stable while a prewarmed timeline is not yet resolved
    // to a real message event (e.g. state-heavy recent history).
    if (hasLiveMessagePreview) {
        fields.lastMessage     = liveDescription.body;
        fields.descriptiveTime = liveDescription.descriptiveTime;
    } else {
        if (cachedDescription.has_value()) {
            if (!cachedDescription->body.isEmpty())
                fields.lastMessage = cachedDescription->body;
            if (!cachedDescription->descriptiveTime.isEmpty())
                fields.descriptiveTime = cachedDescription->descriptiveTime;
        }

        if (fields.lastMessage.isEmpty())
            fields.lastMessage = liveDescription.body;
        if (fields.descriptiveTime.isEmpty())
            fields.descriptiveTime = liveDescription.descriptiveTime;
    }

    fields.timestamp = liveTimestamp;
    if (!hasLiveMessagePreview && cachedDescription.has_value()) {
        fields.timestamp =
          std::max(fields.timestamp, static_cast<quint64>(cachedDescription->timestamp));
    }
    fields.timestamp = std::max(fields.timestamp, approximateLastModificationTs);

    return fields;
}

} // namespace timeline::roomlist

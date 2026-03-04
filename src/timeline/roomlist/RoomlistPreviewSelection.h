// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include <QString>

#include "matrix/MatrixStateTypes.h"

namespace timeline::roomlist {

struct MaterializedPreviewFields
{
    QString lastMessage;
    QString descriptiveTime;
    quint64 timestamp = 0;
};

MaterializedPreviewFields
selectMaterializedPreviewFields(const DescInfo &liveDescription,
                                quint64 liveTimestamp,
                                bool hasLiveMessagePreview,
                                const std::optional<DescInfo> &cachedDescription,
                                quint64 approximateLastModificationTs);

} // namespace timeline::roomlist

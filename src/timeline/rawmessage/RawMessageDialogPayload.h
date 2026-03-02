// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPalette>
#include <QString>
#include <nlohmann/json_fwd.hpp>

namespace timeline::rawmessage {

struct RawMessageDialogPayload
{
    QString renderedRawMessage;
    QString rawMessageJson;
    QString rawMessageBody;
    QString rawMessageFormattedBody;
};

RawMessageDialogPayload
buildRawMessageDialogPayload(const nlohmann::json &eventJson, const QPalette &palette);

} // namespace timeline::rawmessage

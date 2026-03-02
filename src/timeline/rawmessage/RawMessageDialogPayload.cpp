// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/rawmessage/RawMessageDialogPayload.h"

#include <string_view>

#include <nlohmann/json.hpp>

#include "timeline/formattedcode/RawJsonFormatter.h"

namespace timeline::rawmessage {

namespace {
QString
extractContentStringField(const nlohmann::json &eventJson, std::string_view fieldName)
{
    const auto contentIt = eventJson.find("content");
    if (contentIt == eventJson.end() || !contentIt->is_object())
        return {};

    const auto fieldIt = contentIt->find(std::string(fieldName));
    if (fieldIt == contentIt->end() || !fieldIt->is_string())
        return {};

    return QString::fromStdString(fieldIt->get<std::string>());
}
} // namespace

RawMessageDialogPayload
buildRawMessageDialogPayload(const nlohmann::json &eventJson, const QPalette &palette)
{
    RawMessageDialogPayload dialogPayload;
    dialogPayload.rawMessageJson = QString::fromStdString(eventJson.dump(4));
    dialogPayload.renderedRawMessage =
      timeline::formattedcode::formatRawJsonForDialog(dialogPayload.rawMessageJson, palette);
    dialogPayload.rawMessageBody          = extractContentStringField(eventJson, "body");
    dialogPayload.rawMessageFormattedBody = extractContentStringField(eventJson, "formatted_body");
    return dialogPayload;
}

} // namespace timeline::rawmessage

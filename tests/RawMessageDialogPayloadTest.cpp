// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string_view>

#include <QGuiApplication>
#include <QPalette>
#include <QString>

#include <nlohmann/json.hpp>

#include "timeline/rawmessage/RawMessageDialogPayload.h"

namespace {

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

bool
testBuildsDialogPayloadWithCopyFields()
{
    nlohmann::json eventJson = {
      {"type", "m.room.message"},
      {"content",
       {{"msgtype", "m.text"},
        {"body", "Hello from body"},
        {"formatted_body", "<strong>Hello from formatted</strong>"}}}
    };

    const auto out = timeline::rawmessage::buildRawMessageDialogPayload(eventJson, QPalette());

    bool ok = true;
    ok &= expect(!out.renderedRawMessage.isEmpty(), "payload includes highlighted html");
    ok &= expect(out.rawMessageJson.contains(QStringLiteral("\"m.room.message\"")),
                 "payload includes serialized event json");
    ok &= expect(out.rawMessageBody == QStringLiteral("Hello from body"),
                 "payload extracts content.body");
    ok &= expect(out.rawMessageFormattedBody == QStringLiteral("<strong>Hello from formatted</strong>"),
                 "payload extracts content.formatted_body");
    return ok;
}

bool
testMissingCopyFieldsStayEmpty()
{
    nlohmann::json eventJson = {
      {"type", "m.reaction"},
      {"content", {{"m.relates_to", {{"event_id", "$event"}}}}}
    };

    const auto out = timeline::rawmessage::buildRawMessageDialogPayload(eventJson, QPalette());

    bool ok = true;
    ok &= expect(out.rawMessageBody.isEmpty(), "missing content.body stays empty");
    ok &= expect(out.rawMessageFormattedBody.isEmpty(),
                 "missing content.formatted_body stays empty");
    return ok;
}

} // namespace

int
main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    bool ok = true;
    ok &= testBuildsDialogPayloadWithCopyFields();
    ok &= testMissingCopyFieldsStayEmpty();

    return ok ? 0 : 1;
}

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomCommands.h"

#include <iostream>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "IpcClient.h"
#include "schema/Dispatcher.h"
#include "schema/SchemaTypes.h"

namespace {

bool
requireNonEmptyValue(const QString &value, const char *label)
{
    if (!value.trimmed().isEmpty())
        return true;
    std::cerr << "Error: " << label << " must not be empty\n";
    return false;
}

bool
handleIpcError(const QJsonObject &response)
{
    if (!response.contains(QStringLiteral("error")))
        return false;
    std::cerr << "Error: " << response.value(QStringLiteral("error")).toString().toStdString()
              << "\n";
    return true;
}

/// Splits a comma-separated flag into a JSON array, skipping empty entries.
QJsonArray
commaSeparatedArray(const QString &value)
{
    QJsonArray array;
    for (const auto &entry : value.split(QLatin1Char(','), Qt::SkipEmptyParts))
        array.append(entry.trimmed());
    return array;
}

int
handleList(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    QJsonObject params;

    const std::pair<const char *, const char *> stringFlags[] = {
      {"--query", "query"},
      {"--parent-space", "parentSpace"},
      {"--tag", "tag"},
    };
    for (const auto &[flag, key] : stringFlags) {
        const auto value = parsed.flagOr(QString::fromLatin1(flag));
        if (!value.isEmpty())
            params.insert(QString::fromLatin1(key), value);
    }

    const std::pair<const char *, const char *> listFlags[] = {
      {"--ids", "ids"},
      {"--fields", "fields"},
    };
    for (const auto &[flag, key] : listFlags) {
        const auto value = parsed.flagOr(QString::fromLatin1(flag));
        if (!value.isEmpty())
            params.insert(QString::fromLatin1(key), commaSeparatedArray(value));
    }

    // Absent means "do not filter on this", which a bare boolean flag could
    // not express, so these take an explicit true/false.
    const std::pair<const char *, const char *> tristateFlags[] = {
      {"--is-dm", "isDm"},
      {"--encrypted", "encrypted"},
    };
    for (const auto &[flag, key] : tristateFlags) {
        const auto value = parsed.flagOr(QString::fromLatin1(flag));
        if (!value.isEmpty())
            params.insert(QString::fromLatin1(key), value == QLatin1String("true"));
    }

    const std::pair<const char *, const char *> intFlags[] = {
      {"--min-member-count", "minMemberCount"},
      {"--limit", "limit"},
      {"--offset", "offset"},
    };
    for (const auto &[flag, key] : intFlags) {
        const auto value = parsed.flagOr(QString::fromLatin1(flag));
        if (value.isEmpty())
            continue;

        bool ok     = false;
        const int n = value.toInt(&ok);
        if (!ok) {
            std::cerr << "Error: " << flag << " must be an integer\n";
            return 1;
        }
        params.insert(QString::fromLatin1(key), n);
    }

    const auto response = cli_ipc::call(parsed.profileId, QStringLiteral("rooms.list"), params);
    if (handleIpcError(response))
        return 1;

    const auto result = response.value(QStringLiteral("result")).toObject();
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << "\n";
    return 0;
}

int
handleTimeline(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    const auto room = parsed.positionals.value(0);
    if (!requireNonEmptyValue(room, "room-id-or-alias"))
        return 1;

    QJsonObject params{{QStringLiteral("roomIdOrAlias"), room}};

    const auto limitFlag = parsed.flagOr(QStringLiteral("--limit"));
    if (!limitFlag.isEmpty()) {
        bool ok     = false;
        const int n = limitFlag.toInt(&ok);
        if (!ok) {
            std::cerr << "Error: --limit must be an integer\n";
            return 1;
        }
        params.insert(QStringLiteral("limit"), n);
    }

    const auto beforeEventId = parsed.flagOr(QStringLiteral("--before-event-id"));
    if (!beforeEventId.isEmpty())
        params.insert(QStringLiteral("beforeEventId"), beforeEventId);

    if (parsed.hasFlag(QStringLiteral("--include-unsigned-fields")))
        params.insert(QStringLiteral("includeUnsignedFields"), true);

    const auto fetchMode = parsed.flagOr(QStringLiteral("--fetch-mode"));
    if (!fetchMode.isEmpty())
        params.insert(QStringLiteral("fetchMode"), fetchMode);

    const auto response = cli_ipc::call(parsed.profileId, QStringLiteral("rooms.timeline"), params);
    if (handleIpcError(response))
        return 1;

    const auto result = response.value(QStringLiteral("result")).toObject();
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << "\n";
    return 0;
}

int
handleJoin(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    const auto room = parsed.positionals.value(0);
    if (!requireNonEmptyValue(room, "room-id-or-alias"))
        return 1;
    auto response = cli_ipc::call(
      parsed.profileId, QStringLiteral("rooms.join"), {{QStringLiteral("roomIdOrAlias"), room}});
    if (handleIpcError(response))
        return 1;
    return 0;
}

int
handleNewDirectChat(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    const auto userId = parsed.positionals.value(0);
    if (!requireNonEmptyValue(userId, "user-id"))
        return 1;
    auto response = cli_ipc::call(parsed.profileId,
                                  QStringLiteral("rooms.newDirectChat"),
                                  {{QStringLiteral("userId"), userId}});
    if (handleIpcError(response))
        return 1;
    return 0;
}

/// Parses a --flag that carries raw JSON straight through to the homeserver.
/// `expectArray` picks which of the two shapes is acceptable.
bool
jsonFlagValue(const cli_schema::ParsedArgs &parsed,
              const QString &flag,
              bool expectArray,
              QJsonObject *params,
              const QString &paramKey)
{
    const auto raw = parsed.flagOr(flag);
    if (raw.isEmpty())
        return true;

    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(raw.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        std::cerr << "Error: " << flag.toStdString()
                  << " is not valid JSON: " << parseError.errorString().toStdString() << "\n";
        return false;
    }

    if (expectArray) {
        if (!doc.isArray()) {
            std::cerr << "Error: " << flag.toStdString() << " must be a JSON array\n";
            return false;
        }
        params->insert(paramKey, doc.array());
    } else {
        if (!doc.isObject()) {
            std::cerr << "Error: " << flag.toStdString() << " must be a JSON object\n";
            return false;
        }
        params->insert(paramKey, doc.object());
    }

    return true;
}

int
handleCreate(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    QJsonObject params;

    const std::pair<const char *, const char *> stringFlags[] = {
      {"--name", "name"},
      {"--topic", "topic"},
      {"--alias-localpart", "aliasLocalpart"},
      {"--preset", "preset"},
      {"--room-version", "roomVersion"},
    };
    for (const auto &[flag, key] : stringFlags) {
        const auto value = parsed.flagOr(QString::fromLatin1(flag));
        if (!value.isEmpty())
            params.insert(QString::fromLatin1(key), value);
    }

    const std::pair<const char *, const char *> boolFlags[] = {
      {"--direct", "isDirect"},
      {"--encrypted", "isEncrypted"},
      {"--space", "isSpace"},
      {"--public", "isPublic"},
    };
    for (const auto &[flag, key] : boolFlags) {
        if (parsed.hasFlag(QString::fromLatin1(flag)))
            params.insert(QString::fromLatin1(key), true);
    }

    const auto invite = parsed.flagOr(QStringLiteral("--invite"));
    if (!invite.isEmpty()) {
        QJsonArray inviteArray;
        for (const auto &userId : invite.split(QLatin1Char(','), Qt::SkipEmptyParts))
            inviteArray.append(userId.trimmed());
        params.insert(QStringLiteral("invite"), inviteArray);
    }

    if (!jsonFlagValue(parsed,
                       QStringLiteral("--power-levels"),
                       false,
                       &params,
                       QStringLiteral("powerLevelContentOverride")) ||
        !jsonFlagValue(parsed,
                       QStringLiteral("--creation-content"),
                       false,
                       &params,
                       QStringLiteral("creationContent")) ||
        !jsonFlagValue(parsed,
                       QStringLiteral("--initial-state"),
                       true,
                       &params,
                       QStringLiteral("initialState"))) {
        return 1;
    }

    const auto response = cli_ipc::call(parsed.profileId, QStringLiteral("rooms.create"), params);
    if (handleIpcError(response))
        return 1;

    const auto result = response.value(QStringLiteral("result")).toObject();
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << "\n";
    return 0;
}

int
handleGetState(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    const auto room = parsed.positionals.value(0);
    if (!requireNonEmptyValue(room, "room-id-or-alias"))
        return 1;
    const auto eventType = parsed.positionals.value(1);
    if (!requireNonEmptyValue(eventType, "event-type"))
        return 1;

    QJsonObject params{
      {QStringLiteral("roomIdOrAlias"), room},
      {QStringLiteral("eventType"), eventType},
      {QStringLiteral("stateKey"), parsed.positionals.value(2)},
    };

    const auto response = cli_ipc::call(parsed.profileId, QStringLiteral("rooms.getState"), params);
    if (handleIpcError(response))
        return 1;

    const auto result = response.value(QStringLiteral("result")).toObject();
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << "\n";
    // A room that simply has no such state is an answer, but scripts want to
    // branch on it without parsing, so it also shows up in the exit code.
    return result.value(QStringLiteral("exists")).toBool() ? 0 : 2;
}

int
handleSetState(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    const auto room = parsed.positionals.value(0);
    if (!requireNonEmptyValue(room, "room-id-or-alias"))
        return 1;
    const auto eventType = parsed.positionals.value(1);
    if (!requireNonEmptyValue(eventType, "event-type"))
        return 1;
    const auto rawContent = parsed.positionals.value(2);
    if (!requireNonEmptyValue(rawContent, "content-json"))
        return 1;

    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(rawContent.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        std::cerr << "Error: content-json is not valid JSON: "
                  << parseError.errorString().toStdString() << "\n";
        return 1;
    }
    if (!doc.isObject()) {
        std::cerr << "Error: content-json must be a JSON object\n";
        return 1;
    }

    QJsonObject params{
      {QStringLiteral("roomIdOrAlias"), room},
      {QStringLiteral("eventType"), eventType},
      {QStringLiteral("stateKey"), parsed.flagOr(QStringLiteral("--state-key"))},
      {QStringLiteral("content"), doc.object()},
    };

    const auto response = cli_ipc::call(parsed.profileId, QStringLiteral("rooms.setState"), params);
    if (handleIpcError(response))
        return 1;

    const auto result = response.value(QStringLiteral("result")).toObject();
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << "\n";
    return 0;
}

/// set-name and set-topic differ only in the method and the parameter name.
int
handleSetRoomText(const cli_schema::ParsedArgs &parsed,
                  const QString &method,
                  const QString &paramKey)
{
    const auto room = parsed.positionals.value(0);
    if (!requireNonEmptyValue(room, "room-id-or-alias"))
        return 1;

    // Remaining positionals join with spaces so unquoted multi-word values
    // work, and an omitted value clears the field.
    QStringList parts;
    for (int i = 1; i < parsed.positionals.size(); ++i)
        parts.append(parsed.positionals.at(i));

    const auto response = cli_ipc::call(
      parsed.profileId,
      method,
      {{QStringLiteral("roomIdOrAlias"), room}, {paramKey, parts.join(QLatin1Char(' '))}});
    return handleIpcError(response) ? 1 : 0;
}

int
handleSetPowerLevel(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    const auto room = parsed.positionals.value(0);
    if (!requireNonEmptyValue(room, "room-id-or-alias"))
        return 1;
    const auto userId = parsed.positionals.value(1);
    if (!requireNonEmptyValue(userId, "user-id"))
        return 1;

    bool ok              = false;
    const int powerLevel = parsed.positionals.value(2).toInt(&ok);
    if (!ok) {
        std::cerr << "Error: power-level must be an integer\n";
        return 1;
    }

    const auto response = cli_ipc::call(parsed.profileId,
                                        QStringLiteral("rooms.setPowerLevel"),
                                        {{QStringLiteral("roomIdOrAlias"), room},
                                         {QStringLiteral("userId"), userId},
                                         {QStringLiteral("powerLevel"), powerLevel}});
    return handleIpcError(response) ? 1 : 0;
}

/// The four user-targeting membership subcommands differ only in the IPC
/// method they call, so they share one handler.
int
handleMembershipAction(const cli_schema::ParsedArgs &parsed, const QString &method)
{
    const auto room = parsed.positionals.value(0);
    if (!requireNonEmptyValue(room, "room-id-or-alias"))
        return 1;

    const auto userId = parsed.positionals.value(1);
    if (!requireNonEmptyValue(userId, "user-id"))
        return 1;

    QJsonObject params{
      {QStringLiteral("roomIdOrAlias"), room},
      {QStringLiteral("userId"), userId},
    };

    const auto reason = parsed.flagOr(QStringLiteral("--reason"));
    if (!reason.isEmpty())
        params.insert(QStringLiteral("reason"), reason);

    if (handleIpcError(cli_ipc::call(parsed.profileId, method, params)))
        return 1;
    return 0;
}

int
handleLeave(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    const auto room = parsed.positionals.value(0);
    if (!requireNonEmptyValue(room, "room-id-or-alias"))
        return 1;

    QJsonObject params{{QStringLiteral("roomIdOrAlias"), room}};

    const auto reason = parsed.flagOr(QStringLiteral("--reason"));
    if (!reason.isEmpty())
        params.insert(QStringLiteral("reason"), reason);

    if (handleIpcError(cli_ipc::call(parsed.profileId, QStringLiteral("rooms.leave"), params)))
        return 1;
    return 0;
}

int
handleSend(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    if (parsed.positionals.size() < 2) {
        std::cerr << "Error: send requires <room-id-or-alias> <message>\n";
        return 1;
    }
    const auto room = parsed.positionals.value(0);
    // Join remaining positionals into the message body so unquoted multi-word
    // messages work.
    QStringList bodyParts;
    for (int i = 1; i < parsed.positionals.size(); ++i)
        bodyParts.append(parsed.positionals.at(i));
    const auto body = bodyParts.join(QLatin1Char(' '));

    const auto msgtypeFlag = parsed.flagOr(QStringLiteral("--msgtype"), QStringLiteral("text"));
    const auto msgtype     = (msgtypeFlag == QLatin1String("notice")) ? QStringLiteral("m.notice")
                                                                      : QStringLiteral("m.text");
    const auto format      = parsed.flagOr(QStringLiteral("--format"), QStringLiteral("auto"));

    auto response = cli_ipc::call(parsed.profileId,
                                  QStringLiteral("rooms.send"),
                                  {{QStringLiteral("roomIdOrAlias"), room},
                                   {QStringLiteral("body"), body},
                                   {QStringLiteral("msgtype"), msgtype},
                                   {QStringLiteral("format"), format}});
    if (handleIpcError(response))
        return 1;
    auto result = response.value(QStringLiteral("result")).toObject();
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << "\n";
    return 0;
}

int
handleSendImage(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    if (parsed.positionals.size() < 2) {
        std::cerr << "Error: send-image requires <room-id-or-alias> <path-or-mxc>\n";
        return 1;
    }
    const auto room         = parsed.positionals.value(0);
    const auto pathOrMxc    = parsed.positionals.value(1);
    const auto caption      = parsed.flagOr(QStringLiteral("--caption"));
    const auto filenameFlag = parsed.flagOr(QStringLiteral("--filename"));

    QJsonObject response;
    if (pathOrMxc.startsWith(QLatin1String("mxc://"))) {
        QJsonObject params{
          {QStringLiteral("roomIdOrAlias"), room},
          {QStringLiteral("mxcUri"), pathOrMxc},
        };
        if (!caption.isEmpty())
            params.insert(QStringLiteral("body"), caption);
        if (!filenameFlag.isEmpty())
            params.insert(QStringLiteral("filename"), filenameFlag);
        response = cli_ipc::call(parsed.profileId, QStringLiteral("rooms.sendImage"), params);
    } else {
        QJsonObject params{
          {QStringLiteral("roomIdOrAlias"), room},
          {QStringLiteral("path"), pathOrMxc},
        };
        if (!caption.isEmpty())
            params.insert(QStringLiteral("body"), caption);
        response = cli_ipc::call(parsed.profileId, QStringLiteral("rooms.sendImageFile"), params);
    }

    if (handleIpcError(response))
        return 1;
    auto result = response.value(QStringLiteral("result")).toObject();
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << "\n";
    return 0;
}

} // namespace

cli_schema::GroupDef
roomsGroupDef()
{
    cli_schema::GroupDef group;
    group.name = QStringLiteral("rooms");
    group.help = QStringLiteral("Room discovery and navigation (JSON)");

    // list [filters]
    cli_schema::SubcommandDef list;
    list.name     = QStringLiteral("list");
    list.help     = QStringLiteral("List joined rooms, with filters (JSON)");
    list.longHelp = QStringLiteral(
      "Prints {\"rooms\": [...], \"matchCount\": n}, where matchCount is how many rooms\n"
      "matched the filters before --limit and --offset were applied -- not how many rooms\n"
      "you are joined to, unless you passed no filters.\n\n"
      "Rooms come back in the room list's own order, which is by recent activity, so a\n"
      "paged walk over a busy account is a snapshot rather than a stable cursor.");

    struct ListFlag
    {
        const char *longName;
        const char *placeholder;
        const char *help;
        bool boolEnum;
    };
    static constexpr ListFlag listFlags[] = {
      {"--ids", "<id,...>", "Room IDs or aliases to restrict the result to.", false},
      {"--query", "<text>", "Case-insensitive substring matched on room name and alias.", false},
      {"--is-dm", "<bool>", "Keep only direct chats, or only non-direct chats.", true},
      {"--encrypted", "<bool>", "Keep only encrypted rooms, or only unencrypted ones.", true},
      {"--tag", "<tag>", "Keep only rooms carrying this Matrix room tag.", false},
      {"--parent-space", "<room-id>", "Keep only children of this space.", false},
      {"--min-member-count", "<n>", "Keep only rooms with at least this many members.", false},
      {"--limit", "<n>", "Maximum rooms to return; all of them if unset.", false},
      {"--offset", "<n>", "Skip this many matching rooms before the page.", false},
      {"--fields", "<key,...>", "Keys to keep on each room.", false},
    };
    for (const auto &definition : listFlags) {
        cli_schema::FlagDef flag;
        flag.longName         = QString::fromLatin1(definition.longName);
        flag.takesValue       = true;
        flag.valuePlaceholder = QString::fromLatin1(definition.placeholder);
        flag.help             = QString::fromLatin1(definition.help);
        if (definition.boolEnum)
            flag.valueEnum = {QStringLiteral("true"), QStringLiteral("false")};
        list.flags.append(flag);
    }
    list.handler = handleList;
    group.subcommands.append(list);

    // timeline <room> [flags]
    cli_schema::SubcommandDef timeline;
    timeline.name = QStringLiteral("timeline");
    timeline.help = QStringLiteral("Read visible timeline events (JSON)");
    cli_schema::PositionalDef timelineRoom;
    timelineRoom.name = QStringLiteral("room-id-or-alias");
    timeline.positionals.append(timelineRoom);

    cli_schema::FlagDef limit;
    limit.longName         = QStringLiteral("--limit");
    limit.takesValue       = true;
    limit.valuePlaceholder = QStringLiteral("<n>");
    limit.help             = QStringLiteral("Max events to return (default: 10, max: 500).");
    timeline.flags.append(limit);

    cli_schema::FlagDef beforeEventId;
    beforeEventId.longName         = QStringLiteral("--before-event-id");
    beforeEventId.takesValue       = true;
    beforeEventId.valuePlaceholder = QStringLiteral("<id>");
    beforeEventId.help             = QStringLiteral("Return events older than this event ID.");
    timeline.flags.append(beforeEventId);

    cli_schema::FlagDef includeUnsigned;
    includeUnsigned.longName = QStringLiteral("--include-unsigned-fields");
    includeUnsigned.help     = QStringLiteral("Include Matrix 'unsigned' event fields.");
    timeline.flags.append(includeUnsigned);

    cli_schema::FlagDef fetchMode;
    fetchMode.longName   = QStringLiteral("--fetch-mode");
    fetchMode.takesValue = true;
    fetchMode.valueEnum = {QStringLiteral("cached_only"), QStringLiteral("server_fetch_if_needed")};
    fetchMode.help      = QStringLiteral("Fetch older history from the server when needed.");
    timeline.flags.append(fetchMode);

    timeline.handler = handleTimeline;
    group.subcommands.append(timeline);

    // join <room>
    cli_schema::SubcommandDef join;
    join.name = QStringLiteral("join");
    join.help = QStringLiteral("Join a room");
    cli_schema::PositionalDef joinRoom;
    joinRoom.name = QStringLiteral("room-id-or-alias");
    join.positionals.append(joinRoom);
    join.handler = handleJoin;
    group.subcommands.append(join);

    // new-direct-chat <user-id>
    cli_schema::SubcommandDef ndc;
    ndc.name = QStringLiteral("new-direct-chat");
    ndc.help = QStringLiteral("Start or open a direct chat");
    cli_schema::PositionalDef userId;
    userId.name = QStringLiteral("user-id");
    ndc.positionals.append(userId);
    ndc.handler = handleNewDirectChat;
    group.subcommands.append(ndc);

    // create [flags]
    cli_schema::SubcommandDef create;
    create.name     = QStringLiteral("create");
    create.help     = QStringLiteral("Create a room or space (JSON)");
    create.longHelp = QStringLiteral(
      "Creates a room and prints its room ID.\n\n"
      "--power-levels, --creation-content and --initial-state take raw JSON that is passed\n"
      "to the homeserver untouched, so anything Matrix defines for them is accepted.");

    struct CreateFlag
    {
        const char *longName;
        bool takesValue;
        const char *placeholder;
        const char *help;
    };
    static constexpr CreateFlag createFlags[] = {
      {"--name", true, "<text>", "Room name."},
      {"--topic", true, "<text>", "Room topic."},
      {"--alias-localpart", true, "<text>", "Local part of the room alias, without '#' or ':'."},
      {"--preset", true, "<preset>", "Matrix createRoom preset."},
      {"--invite", true, "<user-id,...>", "Matrix user IDs to invite."},
      {"--room-version", true, "<version>", "Room version to request; server default if unset."},
      {"--power-levels", true, "<json>", "m.room.power_levels content override, as JSON."},
      {"--initial-state", true, "<json>", "JSON array of state events to set at creation."},
      {"--creation-content", true, "<json>", "m.room.create content additions, as JSON."},
      {"--direct", false, nullptr, "Mark the room as a direct chat."},
      {"--encrypted", false, nullptr, "Enable end-to-end encryption at creation."},
      {"--space", false, nullptr, "Create a space instead of a room."},
      {"--public", false, nullptr, "Publish the room in the server's room directory."},
    };
    for (const auto &definition : createFlags) {
        cli_schema::FlagDef flag;
        flag.longName   = QString::fromLatin1(definition.longName);
        flag.takesValue = definition.takesValue;
        if (definition.placeholder)
            flag.valuePlaceholder = QString::fromLatin1(definition.placeholder);
        flag.help = QString::fromLatin1(definition.help);
        if (flag.longName == QLatin1String("--preset")) {
            flag.valueEnum    = {QStringLiteral("private_chat"),
                                 QStringLiteral("public_chat"),
                                 QStringLiteral("trusted_private_chat")};
            flag.defaultValue = QStringLiteral("private_chat");
        }
        create.flags.append(flag);
    }
    create.handler = handleCreate;
    group.subcommands.append(create);

    // get-state <room> <event-type> [state-key]
    cli_schema::SubcommandDef getState;
    getState.name     = QStringLiteral("get-state");
    getState.help     = QStringLiteral("Read one room state event's content (JSON)");
    getState.longHelp = QStringLiteral(
      "Prints {\"exists\": bool, \"content\": {...}}, read from the homeserver rather than\n"
      "Komai's local cache, which only holds the state types sliding sync asked for.\n\n"
      "Exits 2 when the room has no such state event, so scripts can branch without parsing.");
    for (const auto *name : {"room-id-or-alias", "event-type"}) {
        cli_schema::PositionalDef positional;
        positional.name = QString::fromLatin1(name);
        getState.positionals.append(positional);
    }
    cli_schema::PositionalDef getStateKey;
    getStateKey.name     = QStringLiteral("state-key");
    getStateKey.optional = true;
    getStateKey.help     = QStringLiteral("Defaults to the empty string, which most state uses.");
    getState.positionals.append(getStateKey);
    getState.handler = handleGetState;
    group.subcommands.append(getState);

    // set-state <room> <event-type> <content-json> [--state-key]
    cli_schema::SubcommandDef setState;
    setState.name     = QStringLiteral("set-state");
    setState.help     = QStringLiteral("Send a room state event with raw JSON content");
    setState.longHelp = QStringLiteral(
      "The content replaces the state event wholesale; it is not merged into what is\n"
      "already there. For m.room.power_levels that means an object listing one user\n"
      "drops every other level in the room -- use set-power-level instead.");
    for (const auto *name : {"room-id-or-alias", "event-type", "content-json"}) {
        cli_schema::PositionalDef positional;
        positional.name = QString::fromLatin1(name);
        setState.positionals.append(positional);
    }
    cli_schema::FlagDef stateKeyFlag;
    stateKeyFlag.longName         = QStringLiteral("--state-key");
    stateKeyFlag.takesValue       = true;
    stateKeyFlag.valuePlaceholder = QStringLiteral("<key>");
    stateKeyFlag.help             = QStringLiteral("State key; defaults to the empty string.");
    setState.flags.append(stateKeyFlag);
    setState.handler = handleSetState;
    group.subcommands.append(setState);

    // set-name / set-topic <room> [value...]
    const std::pair<const char *, const char *> roomTextSubcommands[] = {
      {"set-name", "name"},
      {"set-topic", "topic"},
    };
    for (const auto &[name, paramKey] : roomTextSubcommands) {
        cli_schema::SubcommandDef subcommand;
        subcommand.name = QString::fromLatin1(name);
        subcommand.help =
          QStringLiteral("Set the room %1 (omit to clear it)").arg(QString::fromLatin1(paramKey));

        cli_schema::PositionalDef room;
        room.name = QStringLiteral("room-id-or-alias");
        subcommand.positionals.append(room);

        cli_schema::PositionalDef value;
        value.name     = QString::fromLatin1(paramKey);
        value.optional = true;
        value.variadic = true;
        value.help     = QStringLiteral("Multiple words are joined with spaces.");
        subcommand.positionals.append(value);

        const auto method = QStringLiteral("rooms.set%1%2")
                              .arg(QString::fromLatin1(paramKey).left(1).toUpper(),
                                   QString::fromLatin1(paramKey).mid(1));
        const auto key     = QString::fromLatin1(paramKey);
        subcommand.handler = [method, key](const cli_schema::ParsedArgs &parsed,
                                           QCoreApplication & /*app*/) {
            return handleSetRoomText(parsed, method, key);
        };
        group.subcommands.append(subcommand);
    }

    // set-power-level <room> <user> <level>
    cli_schema::SubcommandDef setPowerLevel;
    setPowerLevel.name     = QStringLiteral("set-power-level");
    setPowerLevel.help     = QStringLiteral("Set one user's power level in a room");
    setPowerLevel.longHelp = QStringLiteral(
      "Reads m.room.power_levels, changes this one user, and writes it back, so every\n"
      "other level in the room is preserved.");
    for (const auto *name : {"room-id-or-alias", "user-id", "power-level"}) {
        cli_schema::PositionalDef positional;
        positional.name = QString::fromLatin1(name);
        setPowerLevel.positionals.append(positional);
    }
    setPowerLevel.handler = handleSetPowerLevel;
    group.subcommands.append(setPowerLevel);

    // invite / kick / ban / unban <room> <user> [--reason]
    struct MembershipSubcommand
    {
        const char *name;
        const char *help;
        const char *method;
        const char *reasonHelp;
    };
    static constexpr MembershipSubcommand membershipSubcommands[] = {
      {"invite", "Invite a user to a room", "rooms.invite", "Reason shown to the invitee."},
      {"kick", "Remove a user from a room", "rooms.kick", "Reason recorded on the kick."},
      {"ban", "Ban a user from a room", "rooms.ban", "Reason recorded on the ban."},
      {"unban", "Lift a user's ban from a room", "rooms.unban", "Reason recorded on the unban."},
    };

    for (const auto &definition : membershipSubcommands) {
        cli_schema::SubcommandDef membership;
        membership.name = QString::fromLatin1(definition.name);
        membership.help = QString::fromLatin1(definition.help);

        cli_schema::PositionalDef membershipRoom;
        membershipRoom.name = QStringLiteral("room-id-or-alias");
        membership.positionals.append(membershipRoom);

        cli_schema::PositionalDef membershipUser;
        membershipUser.name = QStringLiteral("user-id");
        membership.positionals.append(membershipUser);

        cli_schema::FlagDef reason;
        reason.longName         = QStringLiteral("--reason");
        reason.takesValue       = true;
        reason.valuePlaceholder = QStringLiteral("<text>");
        reason.help             = QString::fromLatin1(definition.reasonHelp);
        membership.flags.append(reason);

        const auto method  = QString::fromLatin1(definition.method);
        membership.handler = [method](const cli_schema::ParsedArgs &parsed,
                                      QCoreApplication & /*app*/) {
            return handleMembershipAction(parsed, method);
        };
        group.subcommands.append(membership);
    }

    // leave <room> [--reason]
    cli_schema::SubcommandDef leave;
    leave.name = QStringLiteral("leave");
    leave.help = QStringLiteral("Leave a room, or reject a pending invite");
    cli_schema::PositionalDef leaveRoom;
    leaveRoom.name = QStringLiteral("room-id-or-alias");
    leave.positionals.append(leaveRoom);
    cli_schema::FlagDef leaveReason;
    leaveReason.longName         = QStringLiteral("--reason");
    leaveReason.takesValue       = true;
    leaveReason.valuePlaceholder = QStringLiteral("<text>");
    leaveReason.help             = QStringLiteral("Reason recorded on the leave.");
    leave.flags.append(leaveReason);
    leave.handler = handleLeave;
    group.subcommands.append(leave);

    // send <room> <message>...
    cli_schema::SubcommandDef send;
    send.name = QStringLiteral("send");
    send.help = QStringLiteral("Send a message to a room");
    cli_schema::PositionalDef sendRoom;
    sendRoom.name = QStringLiteral("room-id-or-alias");
    send.positionals.append(sendRoom);
    cli_schema::PositionalDef message;
    message.name     = QStringLiteral("message");
    message.variadic = true;
    message.help     = QStringLiteral("Message body; multiple words are joined with spaces.");
    send.positionals.append(message);

    cli_schema::FlagDef msgtype;
    msgtype.longName     = QStringLiteral("--msgtype");
    msgtype.takesValue   = true;
    msgtype.valueEnum    = {QStringLiteral("text"), QStringLiteral("notice")};
    msgtype.defaultValue = QStringLiteral("text");
    msgtype.help         = QStringLiteral("Matrix msgtype.");
    send.flags.append(msgtype);

    cli_schema::FlagDef format;
    format.longName     = QStringLiteral("--format");
    format.takesValue   = true;
    format.valueEnum    = {QStringLiteral("auto"), QStringLiteral("plain"), QStringLiteral("html")};
    format.defaultValue = QStringLiteral("auto");
    format.help         = QStringLiteral("Formatting mode (auto honours the Composer setting).");
    send.flags.append(format);

    send.handler = handleSend;
    group.subcommands.append(send);

    // send-image <room> <path-or-mxc> [--caption <text>] [--filename <name>]
    cli_schema::SubcommandDef sendImage;
    sendImage.name     = QStringLiteral("send-image");
    sendImage.help     = QStringLiteral("Upload and send an image, or send a pre-uploaded mxc://");
    sendImage.longHelp = QStringLiteral(
      "Pass a local <path> to upload and send (encryption-aware), or an 'mxc://' URI to\n"
      "send a pre-uploaded image (unencrypted rooms only; --filename is required).");

    cli_schema::PositionalDef imgRoom;
    imgRoom.name = QStringLiteral("room-id-or-alias");
    sendImage.positionals.append(imgRoom);
    cli_schema::PositionalDef pathOrMxc;
    pathOrMxc.name = QStringLiteral("path-or-mxc");
    sendImage.positionals.append(pathOrMxc);

    cli_schema::FlagDef caption;
    caption.longName         = QStringLiteral("--caption");
    caption.takesValue       = true;
    caption.valuePlaceholder = QStringLiteral("<text>");
    caption.help             = QStringLiteral("Image caption.");
    sendImage.flags.append(caption);

    cli_schema::FlagDef imgFilename;
    imgFilename.longName         = QStringLiteral("--filename");
    imgFilename.takesValue       = true;
    imgFilename.valuePlaceholder = QStringLiteral("<name>");
    imgFilename.help             = QStringLiteral("Filename (required with an mxc:// URI).");
    sendImage.flags.append(imgFilename);

    sendImage.handler = handleSendImage;
    group.subcommands.append(sendImage);

    return group;
}

int
runRoomsCommand(int argc, char *argv[], QCoreApplication &app)
{
    return cli_schema::dispatchGroup(roomsGroupDef(), argc, argv, app);
}

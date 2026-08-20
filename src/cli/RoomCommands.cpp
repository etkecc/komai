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

int
handleList(const cli_schema::ParsedArgs &parsed, QCoreApplication & /*app*/)
{
    auto response = cli_ipc::call(parsed.profileId, QStringLiteral("rooms.list"));
    auto arr      = response.value(QStringLiteral("result")).toArray();
    std::cout << QJsonDocument(arr).toJson(QJsonDocument::Compact).toStdString() << "\n";
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

    // list
    cli_schema::SubcommandDef list;
    list.name    = QStringLiteral("list");
    list.help    = QStringLiteral("List all joined rooms (JSON)");
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

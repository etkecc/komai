// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/SlashCommands.h"

#include <QCoreApplication>

#include <algorithm>
#include <array>

#include "matrix/MatrixIdentifiers.h"
#include "utils/Utils.h"

namespace timeline::slash_commands {
namespace {

constexpr auto kCommandTranslationContext  = "CommandCompleter";
constexpr auto kInputBarTranslationContext = "InputBar";

QString
translateCommandText(const char *source)
{
    return QCoreApplication::translate(kCommandTranslationContext, source);
}

QString
translateInputBarText(const char *source)
{
    return QCoreApplication::translate(kInputBarTranslationContext, source);
}

QString
trimmedArguments(const ParsedCommand &parsed)
{
    return parsed.arguments.trimmed();
}

QString
firstArgument(const ParsedCommand &parsed)
{
    const auto trimmed = trimmedArguments(parsed);
    if (trimmed.isEmpty())
        return {};

    int end = 0;
    while (end < trimmed.size() && !trimmed.at(end).isSpace())
        ++end;

    return trimmed.left(end);
}

ValidationResult
makeValidationResult(ValidationState state, const char *message = nullptr)
{
    ValidationResult result;
    result.state = state;
    if (message)
        result.message = translateInputBarText(message);
    return result;
}

ValidationResult
valid()
{
    return makeValidationResult(ValidationState::Valid);
}

ValidationResult
validateOptionalArguments(const ParsedCommand &, const CommandContext &)
{
    return valid();
}

ValidationResult
validateNoArguments(const ParsedCommand &parsed, const CommandContext &)
{
    if (trimmedArguments(parsed).isEmpty())
        return valid();

    return makeValidationResult(ValidationState::Invalid,
                                "This command does not take any arguments.");
}

int
adjustedCompletionReplacementEnd(const ParsedCommand &parsed, QStringView completion)
{
    if (!parsed.hasLeadingSlash || parsed.tokenStart != 0 || parsed.tokenEnd < 0)
        return -1;

    int replaceEnd = parsed.tokenEnd;
    if (replaceEnd < parsed.inputText.size() && !completion.isEmpty() &&
        parsed.inputText.at(replaceEnd) == completion.back()) {
        ++replaceEnd;
    }

    return replaceEnd;
}

bool
looksLikeCompleteUserId(QStringView userId)
{
    return komai::parseMatrixUserId(userId).has_value();
}

ValidationResult
incompleteUserIdResult()
{
    return makeValidationResult(ValidationState::Incomplete,
                                "Finish the Matrix user ID, e.g. @alice:example.org.");
}

ValidationResult
validateRequiredMessage(const ParsedCommand &parsed, const CommandContext &)
{
    if (!trimmedArguments(parsed).isEmpty())
        return valid();

    return makeValidationResult(ValidationState::Incomplete, "Enter a message after this command.");
}

ValidationResult
validateReact(const ParsedCommand &parsed, const CommandContext &context)
{
    if (trimmedArguments(parsed).isEmpty())
        return makeValidationResult(ValidationState::Incomplete,
                                    "Enter reaction text after /react.");

    if (context.replyEventId.isEmpty())
        return makeValidationResult(ValidationState::ContextRejected,
                                    "Reply to a message before using /react.");

    return valid();
}

ValidationResult
validateJoin(const ParsedCommand &parsed, const CommandContext &)
{
    if (firstArgument(parsed).isEmpty())
        return makeValidationResult(ValidationState::Incomplete,
                                    "Enter a room ID or room alias after this command.");

    return valid();
}

ValidationResult
validateInvite(const ParsedCommand &parsed, const CommandContext &)
{
    const auto target = firstArgument(parsed);
    if (target.isEmpty())
        return makeValidationResult(ValidationState::Incomplete,
                                    "Enter a Matrix user ID after this command.");

    if (!target.startsWith(u"@"))
        return makeValidationResult(
          ValidationState::Invalid,
          "Use a Matrix user ID like @alice:example.org after this command.");

    if (!looksLikeCompleteUserId(target))
        return incompleteUserIdResult();

    return valid();
}

ValidationResult
validateKickLike(const ParsedCommand &parsed, const CommandContext &context)
{
    const auto trimmed = trimmedArguments(parsed);
    if (trimmed.isEmpty()) {
        if (!context.replySenderId.isEmpty())
            return valid();

        return makeValidationResult(
          ValidationState::ContextRejected,
          "Reply to a message or pass a Matrix user ID after this command.");
    }

    const auto target = firstArgument(parsed);
    if (target.startsWith(u"@")) {
        if (!looksLikeCompleteUserId(target))
            return incompleteUserIdResult();
        return valid();
    }

    if (!context.replySenderId.isEmpty())
        return valid();

    return makeValidationResult(ValidationState::ContextRejected,
                                "Reply to a message or pass a Matrix user ID after this command.");
}

ValidationResult
validateRedact(const ParsedCommand &parsed, const CommandContext &context)
{
    const auto trimmed = trimmedArguments(parsed);
    if (trimmed.isEmpty()) {
        if (!context.replyEventId.isEmpty())
            return valid();

        return makeValidationResult(
          ValidationState::ContextRejected,
          "Reply to a message or pass an event ID or Matrix user ID after /redact.");
    }

    const auto target = firstArgument(parsed);
    if (target.startsWith(u"@")) {
        if (!looksLikeCompleteUserId(target))
            return incompleteUserIdResult();
        return valid();
    }

    if (target.startsWith(u"$"))
        return valid();

    if (!context.replyEventId.isEmpty())
        return valid();

    return makeValidationResult(
      ValidationState::ContextRejected,
      "Reply to a message or pass an event ID or Matrix user ID after /redact.");
}

ValidationResult
validateMsgtype(const ParsedCommand &parsed, const CommandContext &)
{
    const auto first = firstArgument(parsed);
    if (first.isEmpty())
        return makeValidationResult(ValidationState::Incomplete,
                                    "Enter a message type after /msgtype.");

    const auto trimmed = trimmedArguments(parsed);
    if (trimmed.size() <= first.size() || trimmed.mid(first.size()).trimmed().isEmpty())
        return makeValidationResult(ValidationState::Incomplete,
                                    "Enter a message after the message type.");

    return valid();
}

bool
isGotoTargetValid(const QString &target)
{
    if (target.isEmpty())
        return false;

    if (target.startsWith(u"$"))
        return target.size() > 1;

    bool allDigits = true;
    for (const QChar ch : target) {
        if (!ch.isDigit()) {
            allDigits = false;
            break;
        }
    }
    if (allDigits)
        return true;

    return utils::parseMatrixUri(target).has_value();
}

ValidationResult
validateGoto(const ParsedCommand &parsed, const CommandContext &)
{
    const auto target = trimmedArguments(parsed);
    if (target.isEmpty())
        return makeValidationResult(
          ValidationState::Incomplete,
          "Enter an event ID, numeric message index, or Matrix link after /goto.");

    if (isGotoTargetValid(target))
        return valid();

    return makeValidationResult(ValidationState::Invalid,
                                "Use an event ID, numeric message index, or Matrix link after "
                                "/goto.");
}

ValidationResult
validateIgnoreUser(const ParsedCommand &parsed, const CommandContext &)
{
    const auto userId = trimmedArguments(parsed);
    if (userId.isEmpty())
        return makeValidationResult(ValidationState::Incomplete,
                                    "Enter a Matrix user ID after this command.");

    if (!userId.startsWith(u"@"))
        return makeValidationResult(
          ValidationState::Invalid,
          "Use a Matrix user ID like @alice:example.org after this command.");

    if (!looksLikeCompleteUserId(userId))
        return incompleteUserIdResult();

    return valid();
}

#define CMD_TR(text) QT_TRANSLATE_NOOP("CommandCompleter", text)

const std::array<CommandDefinition, 21> kCommands{{
  {CommandId::Me,
   "me",
   "/me ",
   CMD_TR("/me <message>"),
   CMD_TR("Send a message expressing an action."),
   "me action emote",
   validateRequiredMessage},
  {CommandId::React,
   "react",
   "/react ",
   CMD_TR("/react <text>"),
   CMD_TR("Send <text> as a reaction when you’re replying to a message."),
   "react reaction reply",
   validateReact},
  {CommandId::Join,
   "join",
   "/join ",
   CMD_TR("/join <!roomid|#alias> [reason]"),
   CMD_TR("Join a room. Reason is optional."),
   "join room alias roomid",
   validateJoin},
  {CommandId::Knock,
   "knock",
   "/knock ",
   CMD_TR("/knock <!roomid|#alias> [reason]"),
   CMD_TR("Ask to join a room. Reason is optional."),
   "knock room alias roomid",
   validateJoin},
  {CommandId::Leave,
   "leave",
   "/leave ",
   CMD_TR("/leave [reason]"),
   CMD_TR("Leave a room. Reason is optional."),
   "leave",
   validateOptionalArguments},
  {CommandId::Invite,
   "invite",
   "/invite @",
   CMD_TR("/invite <@userid> [reason]"),
   CMD_TR("Invite a user into the current room. Reason is optional."),
   "invite user",
   validateInvite,
   true},
  {CommandId::Kick,
   "kick",
   "/kick @",
   CMD_TR("/kick <@userid> [reason]"),
   CMD_TR("Kick a user from the current room. Reason is optional. If user is left out, will try "
          "to kick the sender you are replying to."),
   "kick moderation user",
   validateKickLike,
   true},
  {CommandId::Ban,
   "ban",
   "/ban @",
   CMD_TR("/ban <@userid> [reason]"),
   CMD_TR("Ban a user from the current room. Reason is optional. If user is left out, will try "
          "to ban the sender you are replying to."),
   "ban moderation user",
   validateKickLike,
   true},
  {CommandId::Unban,
   "unban",
   "/unban @",
   CMD_TR("/unban <@userid> [reason]"),
   CMD_TR("Unban a user in the current room. Reason is optional. If user is left out, will try "
          "to unban the sender you are replying to."),
   "unban moderation user",
   validateKickLike,
   true},
  {CommandId::Redact,
   "redact",
   "/redact ",
   CMD_TR("/redact <$eventid|@userid> [reason]"),
   CMD_TR("Redact an event by event id or that you are replying to, or all locally cached "
          "messages of a user. An optional reason can be provided."),
   "redact moderation event user",
   validateRedact,
   true},
  {CommandId::Roomnick,
   "roomnick",
   "/roomnick ",
   CMD_TR("/roomnick <displayname>"),
   CMD_TR("Change your displayname in this room."),
   "roomnick displayname nickname",
   validateOptionalArguments},
  {CommandId::Shrug,
   "shrug",
   "/shrug ",
   CMD_TR("/shrug [message]"),
   CMD_TR("¯\\_(ツ)_/¯ with an optional message."),
   "shrug macro",
   validateOptionalArguments},
  {CommandId::Markdown,
   "markdown",
   "/markdown ",
   CMD_TR("/markdown <message>"),
   CMD_TR("Try Markdown formatting for this message, even if you have it disabled in "
          "Settings -> Composer."),
   "markdown html formatted message",
   validateRequiredMessage},
  {CommandId::Plain,
   "plain",
   "/plain ",
   CMD_TR("/plain <message>"),
   CMD_TR("Send a plain message without Markdown formatting."),
   "plain unformatted markdown html conversion message",
   validateRequiredMessage},
  {CommandId::Notice,
   "notice",
   "/notice ",
   CMD_TR("/notice <message>"),
   CMD_TR("Send a bot message."),
   "notice bot message",
   validateRequiredMessage},
  {CommandId::Msgtype,
   "msgtype",
   "/msgtype ",
   CMD_TR("/msgtype <msgtype> <message>"),
   CMD_TR("Send a message with a custom Matrix msgtype."),
   "msgtype custom message type",
   validateMsgtype},
  {CommandId::Goto,
   "goto",
   "/goto ",
   CMD_TR("/goto <message reference>"),
   CMD_TR("Jump to an event ID, numeric message index, or Matrix link."),
   "goto jump event link index matrix.to",
   validateGoto},
  {CommandId::ConvertToDm,
   "converttodm",
   "/converttodm",
   CMD_TR("/converttodm"),
   CMD_TR("Mark this room as a direct message."),
   "convert dm direct message",
   validateNoArguments},
  {CommandId::ConvertToRoom,
   "converttoroom",
   "/converttoroom",
   CMD_TR("/converttoroom"),
   CMD_TR("Remove the direct-message marker from this room."),
   "convert room direct message",
   validateNoArguments},
  {CommandId::Ignore,
   "ignore",
   "/ignore @",
   CMD_TR("/ignore <@userid>"),
   CMD_TR("Ignore a Matrix user."),
   "ignore user",
   validateIgnoreUser,
   true},
  {CommandId::Unignore,
   "unignore",
   "/unignore @",
   CMD_TR("/unignore <@userid>"),
   CMD_TR("Stop ignoring a Matrix user."),
   "unignore user",
   validateIgnoreUser,
   true},
}};

#undef CMD_TR

} // namespace

CommandResult
CommandResult::dispatched(const QString &feedback)
{
    return {.kind = ResultKind::Dispatched, .feedback = feedback};
}

CommandResult
CommandResult::rejected(const QString &feedback)
{
    return {.kind = ResultKind::Rejected, .feedback = feedback};
}

std::span<const CommandDefinition>
all()
{
    return std::span<const CommandDefinition>(kCommands);
}

const CommandDefinition *
find(QStringView name)
{
    for (const auto &command : kCommands) {
        if (name == QLatin1String(command.name))
            return &command;
    }

    return nullptr;
}

const CommandDefinition *
find(CommandId id)
{
    for (const auto &command : kCommands) {
        if (command.id == id)
            return &command;
    }

    return nullptr;
}

QString
syntaxText(const CommandDefinition &definition)
{
    return translateCommandText(definition.syntax);
}

QString
descriptionText(const CommandDefinition &definition)
{
    return translateCommandText(definition.description);
}

QString
searchText(const CommandDefinition &definition)
{
    return QString::fromLatin1(definition.search);
}

QString
completionText(const CommandDefinition &definition)
{
    return QString::fromLatin1(definition.completion);
}

QString
commandText(CommandId id, const QString &arguments)
{
    const auto *command = find(id);
    if (!command)
        return arguments;

    QString text = QStringLiteral("/") + QString::fromLatin1(command->name);
    if (!arguments.isEmpty())
        text += QStringLiteral(" ") + arguments;

    return text;
}

QString
validationStateName(ValidationState state)
{
    switch (state) {
    case ValidationState::None:
        return QStringLiteral("none");
    case ValidationState::Unrecognized:
        return QStringLiteral("unrecognized");
    case ValidationState::Incomplete:
        return QStringLiteral("incomplete");
    case ValidationState::Valid:
        return QStringLiteral("valid");
    case ValidationState::Invalid:
        return QStringLiteral("invalid");
    case ValidationState::ContextRejected:
        return QStringLiteral("contextRejected");
    }

    return QStringLiteral("none");
}

ParsedCommand
parse(const QString &text)
{
    ParsedCommand parsed;
    parsed.inputText = text;

    if (!text.startsWith(QLatin1Char('/')))
        return parsed;

    parsed.hasLeadingSlash = true;
    parsed.tokenStart      = 0;

    int tokenEnd = 1;
    while (tokenEnd < text.size() && !text.at(tokenEnd).isSpace())
        ++tokenEnd;

    parsed.tokenEnd = tokenEnd;
    parsed.name     = text.mid(1, tokenEnd - 1);

    if (tokenEnd < text.size())
        parsed.arguments = text.mid(tokenEnd + 1);

    if (!parsed.name.isEmpty())
        parsed.definition = find(QStringView{parsed.name});

    return parsed;
}

Inspection
inspect(const QString &text, const CommandContext &context)
{
    Inspection inspection;
    inspection.parsed = parse(text);

    if (!inspection.parsed.isSlashCommandCandidate()) {
        inspection.validation.state = ValidationState::None;
        inspection.submitAction     = SubmitAction::SendPlainText;
        return inspection;
    }

    if (!inspection.parsed.definition) {
        inspection.validation.state = ValidationState::Unrecognized;
        if (inspection.parsed.isBareCommandToken()) {
            inspection.validation.message =
              translateInputBarText("/%1 is not recognized. To send it anyway, add a space to "
                                    "the end of your message.")
                .arg(inspection.parsed.name);
            inspection.submitAction = SubmitAction::PreserveComposer;
        } else {
            inspection.validation.message =
              translateInputBarText(
                "The command /%1 is not recognized and will be sent as part of your message")
                .arg(inspection.parsed.name);
            inspection.submitAction = SubmitAction::SendPlainText;
        }
        return inspection;
    }

    inspection.validation   = inspection.parsed.definition->validate(inspection.parsed, context);
    inspection.submitAction = inspection.validation.state == ValidationState::Valid
                                ? SubmitAction::ExecuteCommand
                                : SubmitAction::PreserveComposer;

    return inspection;
}

QString
completionSearchString(const QString &text, int cursorPosition)
{
    const int clampedCursor = std::clamp(cursorPosition, 0, static_cast<int>(text.size()));
    const QString prefix    = text.left(clampedCursor);
    const auto parsed       = parse(prefix);

    if (!parsed.hasLeadingSlash || parsed.tokenStart != 0 || parsed.tokenEnd < 0)
        return prefix;

    return prefix.left(parsed.tokenEnd);
}

QString
applyCompletion(const QString &text, int cursorPosition, QStringView completion)
{
    (void)cursorPosition;

    const auto parsed    = parse(text);
    const int replaceEnd = adjustedCompletionReplacementEnd(parsed, completion);
    if (replaceEnd < 0)
        return text;

    QString updated = text;
    updated.remove(parsed.tokenStart, replaceEnd - parsed.tokenStart);
    updated.insert(parsed.tokenStart, completion.toString());
    return updated;
}

int
completionCursorPosition(const QString &text, int cursorPosition, QStringView completion)
{
    const int clampedCursor = std::clamp(cursorPosition, 0, static_cast<int>(text.size()));
    const auto parsed       = parse(text);
    const int replaceEnd    = adjustedCompletionReplacementEnd(parsed, completion);

    if (replaceEnd < 0)
        return clampedCursor;

    const int trailingOffset = std::max(0, clampedCursor - replaceEnd);
    return parsed.tokenStart + static_cast<int>(completion.size()) + trailingOffset;
}

bool
argumentExpectsUserId(const QString &text, int cursorPosition)
{
    const int clampedCursor = std::clamp(cursorPosition, 0, static_cast<int>(text.size()));
    const auto parsed       = parse(text);
    if (!parsed.hasLeadingSlash || !parsed.definition ||
        !parsed.definition->expectsUserIdFirstArg) {
        return false;
    }

    const int firstArgStart = parsed.tokenEnd + 1;
    if (clampedCursor < firstArgStart)
        return false;

    for (int i = firstArgStart; i < clampedCursor; ++i) {
        if (text.at(i).isSpace())
            return false;
    }

    return true;
}

} // namespace timeline::slash_commands

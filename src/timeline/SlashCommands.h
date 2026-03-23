// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringView>

#include <span>

class InputBar;

namespace timeline::slash_commands {

enum class CommandId
{
    Me,
    React,
    Join,
    Knock,
    Part,
    Leave,
    Invite,
    Kick,
    Ban,
    Unban,
    Redact,
    Roomnick,
    Shrug,
    ClearTimeline,
    ResetState,
    RotateMegolmSession,
    Md,
    Cmark,
    Plain,
    Rainbow,
    RainbowMe,
    Notice,
    RainbowNotice,
    Confetti,
    RainbowConfetti,
    Msgtype,
    Glitch,
    GradualGlitch,
    Goto,
    ConvertToDm,
    ConvertToRoom,
    Ignore,
    Unignore,
    BlockInvites,
    AllowInvites,
};

enum class ValidationState
{
    None,
    Unrecognized,
    Incomplete,
    Valid,
    Invalid,
    ContextRejected,
};

enum class SubmitAction
{
    None,
    SendPlainText,
    ExecuteCommand,
    PreserveComposer,
};

enum class ResultKind
{
    Dispatched,
    Rejected,
};

struct CommandContext
{
    QString replyEventId;
    QString replySenderId;
};

struct ParsedCommand;
struct ValidationResult;

using Validator = ValidationResult (*)(const ParsedCommand &, const CommandContext &);

struct CommandDefinition
{
    CommandId id;
    const char *name;
    const char *completion;
    const char *syntax;
    const char *description;
    const char *search;
    Validator validate;
};

struct ParsedCommand
{
    QString inputText;
    QString name;
    QString arguments;
    int tokenStart                      = -1;
    int tokenEnd                        = -1;
    bool hasLeadingSlash                = false;
    const CommandDefinition *definition = nullptr;

    [[nodiscard]] bool isSlashCommandCandidate() const
    {
        return hasLeadingSlash && tokenStart == 0 && !name.isEmpty();
    }

    [[nodiscard]] bool isBareCommandToken() const
    {
        return isSlashCommandCandidate() && inputText.size() == tokenEnd;
    }
};

struct ValidationResult
{
    ValidationState state = ValidationState::None;
    QString message;
};

struct Inspection
{
    ParsedCommand parsed;
    ValidationResult validation;
    SubmitAction submitAction = SubmitAction::None;
};

struct CommandResult
{
    ResultKind kind = ResultKind::Rejected;
    QString feedback;

    [[nodiscard]] bool clearsComposer() const { return kind == ResultKind::Dispatched; }

    static CommandResult dispatched(const QString &feedback = {});
    static CommandResult rejected(const QString &feedback = {});
};

std::span<const CommandDefinition>
all();
const CommandDefinition *
find(QStringView name);
const CommandDefinition *
find(CommandId id);

QString
syntaxText(const CommandDefinition &definition);
QString
descriptionText(const CommandDefinition &definition);
QString
searchText(const CommandDefinition &definition);
QString
completionText(const CommandDefinition &definition);
QString
commandText(CommandId id, const QString &arguments = {});
QString
validationStateName(ValidationState state);

ParsedCommand
parse(const QString &text);
Inspection
inspect(const QString &text, const CommandContext &context);
QString
completionSearchString(const QString &text, int cursorPosition);
QString
applyCompletion(const QString &text, int cursorPosition, QStringView completion);
int
completionCursorPosition(const QString &text, int cursorPosition, QStringView completion);

CommandResult
execute(InputBar &inputBar, const ParsedCommand &parsed);

} // namespace timeline::slash_commands

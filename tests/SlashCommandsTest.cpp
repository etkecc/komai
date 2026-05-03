// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string_view>

#include <QCoreApplication>

#include "models/CommandCompleter.h"
#include "models/CompletionModelRoles.h"
#include "timeline/SlashCommands.h"

namespace {

using timeline::slash_commands::CommandContext;
using timeline::slash_commands::CommandId;
using timeline::slash_commands::SubmitAction;
using timeline::slash_commands::ValidationState;

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

int
commandRowByName(std::string_view name)
{
    const auto commands = timeline::slash_commands::all();
    for (std::size_t i = 0; i < commands.size(); ++i) {
        if (name == commands[i].name)
            return static_cast<int>(i);
    }

    return -1;
}

bool
expectInspection(const QString &text,
                 const CommandContext &context,
                 ValidationState expectedState,
                 SubmitAction expectedAction,
                 std::string_view message)
{
    const auto inspection = timeline::slash_commands::inspect(text, context);
    if (inspection.validation.state == expectedState &&
        inspection.submitAction == expectedAction) {
        return true;
    }

    std::cerr << "FAILED: " << message << "\n"
              << "  text: " << text.toStdString() << "\n"
              << "  state: "
              << timeline::slash_commands::validationStateName(inspection.validation.state)
                   .toStdString()
              << "\n"
              << "  action: " << static_cast<int>(inspection.submitAction) << '\n';
    return false;
}

bool
expectInspectionMessageContains(const QString &text,
                                const CommandContext &context,
                                std::string_view needle,
                                std::string_view message)
{
    const auto inspection = timeline::slash_commands::inspect(text, context);
    if (inspection.validation.message.contains(
          QString::fromLatin1(needle.data(), static_cast<qsizetype>(needle.size())))) {
        return true;
    }

    std::cerr << "FAILED: " << message << "\n"
              << "  text: " << text.toStdString() << "\n"
              << "  validation message: " << inspection.validation.message.toStdString() << '\n';
    return false;
}

bool
testRegistryInventory()
{
    bool ok          = true;
    const auto cmds  = timeline::slash_commands::all();
    const auto *gotoCmd = timeline::slash_commands::find(QStringLiteral("goto"));
    const auto *ignore  = timeline::slash_commands::find(CommandId::Ignore);

    if (cmds.size() != 21) {
        std::cerr << "FAILED: registry contains all currently listed slash commands\n"
                  << "  actual count: " << cmds.size() << '\n';
        ok = false;
    }
    ok &= expect(gotoCmd != nullptr && gotoCmd->id == CommandId::Goto,
                 "registry can look up /goto by name");
    ok &= expect(ignore != nullptr && QString::fromLatin1(ignore->name) == QStringLiteral("ignore"),
                 "registry can look up /ignore by id");
    ok &= expect(timeline::slash_commands::find(QStringLiteral("markdown")) != nullptr,
                 "registry exposes /markdown");
    ok &= expect(timeline::slash_commands::find(QStringLiteral("confetti")) == nullptr,
                 "registry no longer exposes /confetti");
    ok &= expect(timeline::slash_commands::find(QStringLiteral("part")) == nullptr,
                 "registry no longer exposes /part");
    ok &= expect(timeline::slash_commands::find(QStringLiteral("rotate-megolm-session")) ==
                   nullptr,
                 "registry no longer exposes /rotate-megolm-session");
    ok &= expect(timeline::slash_commands::find(QStringLiteral("clear-timeline")) == nullptr,
                 "registry no longer exposes /clear-timeline");
    ok &= expect(timeline::slash_commands::find(QStringLiteral("reset-state")) == nullptr,
                 "registry no longer exposes /reset-state");
    ok &= expect(timeline::slash_commands::find(QStringLiteral("cmark")) == nullptr,
                 "registry no longer exposes /cmark");
    ok &= expect(timeline::slash_commands::find(QStringLiteral("md")) == nullptr,
                 "registry no longer exposes /md");
    return ok;
}

bool
testParserAndCompletionRanges()
{
    bool ok             = true;
    const auto parsed   = timeline::slash_commands::parse(QStringLiteral("/goto 123"));
    const auto bareGoto = timeline::slash_commands::parse(QStringLiteral("/goto"));

    ok &= expect(parsed.name == QStringLiteral("goto"), "parser extracts the command name");
    ok &= expect(parsed.arguments == QStringLiteral("123"), "parser extracts command arguments");
    ok &= expect(parsed.definition && parsed.definition->id == CommandId::Goto,
                 "parser resolves registry definitions");
    ok &= expect(bareGoto.isBareCommandToken(), "bare command token is recognized");
    ok &= expect(
      timeline::slash_commands::completionSearchString(QStringLiteral("/goto 123"), 9) ==
        QStringLiteral("/goto"),
      "command picker search string stays on the command token once arguments exist");
    ok &= expect(
      timeline::slash_commands::completionSearchString(QStringLiteral("/"), 1) == QStringLiteral("/"),
      "command picker still shows all commands for a bare slash");
    const auto replacedWithArgs =
      timeline::slash_commands::applyCompletion(QStringLiteral("/me something"),
                                                13,
                                                QStringLiteral("/notice "));
    ok &= expect(replacedWithArgs == QStringLiteral("/notice something"),
                 "command completion preserves already-typed arguments");
    ok &= expect(timeline::slash_commands::completionCursorPosition(
                   QStringLiteral("/me something"), 13, QStringLiteral("/notice ")) ==
                   replacedWithArgs.size(),
                 "command completion keeps the cursor at the end of preserved arguments");
    ok &= expect(timeline::slash_commands::applyCompletion(QStringLiteral("/goto 123"),
                                                           9,
                                                           QStringLiteral("/join ")) ==
                   QStringLiteral("/join 123"),
                 "command completion avoids duplicating separator spaces");
    return ok;
}

bool
testArgumentExpectsUserId()
{
    bool ok = true;
    using timeline::slash_commands::argumentExpectsUserId;

    ok &= expect(argumentExpectsUserId(QStringLiteral("/kick "), 6),
                 "/kick first argument slot expects a user id");
    ok &= expect(argumentExpectsUserId(QStringLiteral("/kick @"), 6),
                 "/kick treats the position right before @ as a user id slot");
    ok &= expect(argumentExpectsUserId(QStringLiteral("/ban @alice:example.org"), 5),
                 "/ban first argument slot expects a user id");
    ok &= expect(argumentExpectsUserId(QStringLiteral("/invite @"), 8),
                 "/invite first argument slot expects a user id");
    ok &= expect(argumentExpectsUserId(QStringLiteral("/ignore @"), 8),
                 "/ignore first argument slot expects a user id");
    ok &= expect(argumentExpectsUserId(QStringLiteral("/redact @"), 8),
                 "/redact first argument slot expects a user id");
    ok &= expect(!argumentExpectsUserId(QStringLiteral("/kick @alice:example.org reason"), 30),
                 "second argument of /kick is not a user id slot");
    ok &= expect(!argumentExpectsUserId(QStringLiteral("/notice @"), 8),
                 "/notice does not expect a user id argument");
    ok &= expect(!argumentExpectsUserId(QStringLiteral("/me @"), 4),
                 "/me does not expect a user id argument");
    ok &= expect(!argumentExpectsUserId(QStringLiteral("@alice:example.org"), 0),
                 "plain @ outside a slash command does not expect a user id");
    ok &= expect(!argumentExpectsUserId(QStringLiteral("/kick"), 3),
                 "cursor still inside the command name does not expect a user id");
    ok &= expect(!argumentExpectsUserId(QStringLiteral("/foo @"), 5),
                 "unknown slash commands do not expect a user id");
    return ok;
}

bool
testCompleterTemplates()
{
    bool ok               = true;
    CommandCompleter model;
    const int ignoreRow   = commandRowByName("ignore");
    const int gotoRow     = commandRowByName("goto");
    const int inviteRow   = commandRowByName("invite");
    const int markdownRow = commandRowByName("markdown");
    const int plainRow    = commandRowByName("plain");

    ok &= expect(model.rowCount() == static_cast<int>(timeline::slash_commands::all().size()),
                 "command completer row count matches registry size");
    ok &= expect(ignoreRow >= 0 &&
                   model.data(model.index(ignoreRow, 0), CompletionModel::CompletionRole)
                       .toString() == QStringLiteral("/ignore @"),
                 "/ignore completion template includes the required user-id prefix");
    ok &= expect(gotoRow >= 0 &&
                   model.data(model.index(gotoRow, 0), CompletionModel::SearchRole)
                       .toString() == QStringLiteral("/goto <message reference>"),
                 "/goto syntax is exposed through the picker");
    ok &= expect(inviteRow >= 0 &&
                   model.data(model.index(inviteRow, 0), CompletionModel::CompletionRole)
                       .toString() == QStringLiteral("/invite @"),
                 "/invite completion template keeps the user-id affordance");
    ok &= expect(markdownRow >= 0 &&
                   model.data(model.index(markdownRow, 0), CompletionModel::CompletionRole)
                       .toString() == QStringLiteral("/markdown "),
                 "/markdown completion template uses the clearer command name");
    ok &= expect(markdownRow >= 0 &&
                   model.data(model.index(markdownRow, 0), CommandCompleter::Roles::Description)
                       .toString() ==
                     QStringLiteral("Try Markdown formatting for this message, even if you have "
                                    "it disabled in Settings -> Composer."),
                 "/markdown help text explains the markdown override");
    ok &= expect(plainRow >= 0 &&
                   model.data(model.index(plainRow, 0), CommandCompleter::Roles::Description)
                       .toString() ==
                     QStringLiteral("Send a plain message without Markdown formatting."),
                 "/plain help text explains the plain-text override");
    return ok;
}

bool
testValidationAndSendBehavior()
{
    bool ok = true;

    ok &= expectInspection(QStringLiteral("/ "),
                           {},
                           ValidationState::None,
                           SubmitAction::SendPlainText,
                           "a bare slash followed by a separator stays plain text");
    ok &= expectInspection(QStringLiteral("hello /foo"),
                           {},
                           ValidationState::None,
                           SubmitAction::SendPlainText,
                           "non-leading slash text is never treated as a command");
    ok &= expectInspection(QStringLiteral("hello world"),
                           {},
                           ValidationState::None,
                           SubmitAction::SendPlainText,
                           "plain text keeps the normal send path");
    ok &= expectInspection(QStringLiteral("/foo"),
                           {},
                           ValidationState::Unrecognized,
                           SubmitAction::PreserveComposer,
                           "bare unknown slash text preserves the composer");
    ok &= expectInspection(QStringLiteral("/foo "),
                           {},
                           ValidationState::Unrecognized,
                           SubmitAction::SendPlainText,
                           "unknown slash text with a separator still sends literally");
    ok &= expectInspection(QStringLiteral("/goto"),
                           {},
                           ValidationState::Incomplete,
                           SubmitAction::PreserveComposer,
                           "/goto without a target stays in the composer");
    ok &= expectInspection(QStringLiteral("/goto abc"),
                           {},
                           ValidationState::Invalid,
                           SubmitAction::PreserveComposer,
                           "/goto rejects syntactically invalid targets without falling back");
    ok &= expectInspection(QStringLiteral("/goto 123"),
                           {},
                           ValidationState::Valid,
                           SubmitAction::ExecuteCommand,
                           "/goto with a numeric index is executable");
    ok &= expectInspection(QStringLiteral("/react fire"),
                           {},
                           ValidationState::ContextRejected,
                           SubmitAction::PreserveComposer,
                           "/react without a reply target is blocked");
    ok &= expectInspection(QStringLiteral("/react fire"),
                           {.replyEventId = QStringLiteral("$event"), .replySenderId = {}},
                           ValidationState::Valid,
                           SubmitAction::ExecuteCommand,
                           "/react with a reply target is executable");
    ok &= expectInspection(QStringLiteral("/kick"),
                           {},
                           ValidationState::ContextRejected,
                           SubmitAction::PreserveComposer,
                           "/kick without a reply target or userid is blocked");
    ok &= expectInspection(QStringLiteral("/kick reason"),
                           {.replyEventId = {}, .replySenderId = QStringLiteral("@alice:example.org")},
                           ValidationState::Valid,
                           SubmitAction::ExecuteCommand,
                           "/kick with reply context can treat arguments as a reason");
    ok &= expectInspection(QStringLiteral("/kick @alice"),
                           {},
                           ValidationState::Incomplete,
                           SubmitAction::PreserveComposer,
                           "/kick rejects a user id without a :server suffix");
    ok &= expectInspection(QStringLiteral("/kick @alice:example.org"),
                           {},
                           ValidationState::Valid,
                           SubmitAction::ExecuteCommand,
                           "/kick accepts a complete Matrix user id");
    ok &= expectInspection(QStringLiteral("/kick @alice:example.org rude"),
                           {},
                           ValidationState::Valid,
                           SubmitAction::ExecuteCommand,
                           "/kick accepts a complete Matrix user id with a trailing reason");
    ok &= expectInspection(QStringLiteral("/ban @alice"),
                           {},
                           ValidationState::Incomplete,
                           SubmitAction::PreserveComposer,
                           "/ban rejects a user id without a :server suffix");
    ok &= expectInspection(QStringLiteral("/unban @alice"),
                           {},
                           ValidationState::Incomplete,
                           SubmitAction::PreserveComposer,
                           "/unban rejects a user id without a :server suffix");
    ok &= expectInspection(QStringLiteral("/invite @alice"),
                           {},
                           ValidationState::Incomplete,
                           SubmitAction::PreserveComposer,
                           "/invite rejects a user id without a :server suffix");
    ok &= expectInspection(QStringLiteral("/invite @alice:example.org"),
                           {},
                           ValidationState::Valid,
                           SubmitAction::ExecuteCommand,
                           "/invite accepts a complete Matrix user id");
    ok &= expectInspection(QStringLiteral("/redact @alice"),
                           {},
                           ValidationState::Incomplete,
                           SubmitAction::PreserveComposer,
                           "/redact rejects a user id without a :server suffix");
    ok &= expectInspection(QStringLiteral("/redact @alice:example.org"),
                           {},
                           ValidationState::Valid,
                           SubmitAction::ExecuteCommand,
                           "/redact accepts a complete Matrix user id for bulk redaction");
    ok &= expectInspection(QStringLiteral("/ignore bob"),
                           {},
                           ValidationState::Invalid,
                           SubmitAction::PreserveComposer,
                           "/ignore rejects non-mxid input");
    ok &= expectInspection(QStringLiteral("/ignore @alice"),
                           {},
                           ValidationState::Incomplete,
                           SubmitAction::PreserveComposer,
                           "/ignore rejects a user id without a :server suffix");
    ok &= expectInspection(QStringLiteral("/ignore @alice:example.org"),
                           {},
                           ValidationState::Valid,
                           SubmitAction::ExecuteCommand,
                           "/ignore accepts a Matrix user id");
    ok &= expectInspection(QStringLiteral("/converttodm extra"),
                           {},
                           ValidationState::Invalid,
                           SubmitAction::PreserveComposer,
                           "no-argument commands reject accidental trailing text");
    ok &= expectInspection(QStringLiteral("/msgtype"),
                           {},
                           ValidationState::Incomplete,
                           SubmitAction::PreserveComposer,
                           "/msgtype needs a msgtype before submit");
    ok &= expectInspection(QStringLiteral("/msgtype foo"),
                           {},
                           ValidationState::Incomplete,
                           SubmitAction::PreserveComposer,
                           "/msgtype with a type but no body cannot submit");
    ok &= expectInspection(QStringLiteral("/msgtype foo  "),
                           {},
                           ValidationState::Incomplete,
                           SubmitAction::PreserveComposer,
                           "/msgtype trailing whitespace does not count as a body");
    ok &= expectInspection(QStringLiteral("/msgtype foo hello"),
                           {},
                           ValidationState::Valid,
                           SubmitAction::ExecuteCommand,
                           "/msgtype accepts a type plus body");
    return ok;
}

bool
testValidationMessages()
{
    bool ok = true;

    ok &= expectInspectionMessageContains(QStringLiteral("/foo"),
                                          {},
                                          "not recognized",
                                          "bare unknown slash text explains that it is not recognized");
    ok &= expectInspectionMessageContains(
      QStringLiteral("/foo "),
      {},
      "will be sent as part of your message",
      "unknown slash text with a separator explains that it will send literally");
    ok &= expectInspectionMessageContains(
      QStringLiteral("/goto"),
      {},
      "Enter an event ID",
      "/goto without arguments shows a targeted incomplete-message hint");
    ok &= expectInspectionMessageContains(
      QStringLiteral("/react fire"),
      {},
      "Reply to a message",
      "/react without reply context explains the rejection");
    ok &= expectInspectionMessageContains(QStringLiteral("/msgtype foo"),
                                          {},
                                          "Enter a message after the message type",
                                          "/msgtype with type but no body explains the missing body");
    ok &= expectInspectionMessageContains(QStringLiteral("/kick @alice"),
                                          {},
                                          "Finish the Matrix user ID",
                                          "/kick with a partial user id nudges the user to finish it");
    ok &= expect(timeline::slash_commands::inspect(QStringLiteral("/notice hello"), {})
                     .validation.message.isEmpty(),
                 "valid commands keep the validation surface quiet");
    return ok;
}

bool
testCommandHelpers()
{
    bool ok = true;

    ok &= expect(timeline::slash_commands::commandText(CommandId::Me, QStringLiteral("waves")) ==
                   QStringLiteral("/me waves"),
                 "commandText uses registry names for /me");
    ok &= expect(
      timeline::slash_commands::commandText(CommandId::Notice) == QStringLiteral("/notice"),
      "commandText omits a trailing space when there are no arguments");
    ok &= expect(timeline::slash_commands::CommandResult::rejected(QStringLiteral("nope"))
                     .clearsComposer() == false,
                 "rejected command results preserve the composer");
    return ok;
}

} // namespace

int
main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    bool ok = true;
    ok &= testRegistryInventory();
    ok &= testParserAndCompletionRanges();
    ok &= testArgumentExpectsUserId();
    ok &= testCompleterTemplates();
    ok &= testValidationAndSendBehavior();
    ok &= testValidationMessages();
    ok &= testCommandHelpers();
    return ok ? 0 : 1;
}

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

    ok &= expect(cmds.size() == 28, "registry contains all currently listed slash commands");
    ok &= expect(gotoCmd != nullptr && gotoCmd->id == CommandId::Goto,
                 "registry can look up /goto by name");
    ok &= expect(ignore != nullptr && QString::fromLatin1(ignore->name) == QStringLiteral("ignore"),
                 "registry can look up /ignore by id");
    ok &= expect(timeline::slash_commands::find(QStringLiteral("confetti")) == nullptr,
                 "registry no longer exposes /confetti");
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
testCompleterTemplates()
{
    bool ok               = true;
    CommandCompleter model;
    const int ignoreRow   = commandRowByName("ignore");
    const int blockRow    = commandRowByName("blockinvites");
    const int gotoRow     = commandRowByName("goto");
    const int inviteRow   = commandRowByName("invite");

    ok &= expect(model.rowCount() == static_cast<int>(timeline::slash_commands::all().size()),
                 "command completer row count matches registry size");
    ok &= expect(ignoreRow >= 0 &&
                   model.data(model.index(ignoreRow, 0), CompletionModel::CompletionRole)
                       .toString() == QStringLiteral("/ignore @"),
                 "/ignore completion template includes the required user-id prefix");
    ok &= expect(blockRow >= 0 &&
                   model.data(model.index(blockRow, 0), CompletionModel::CompletionRole)
                       .toString() == QStringLiteral("/blockinvites "),
                 "/blockinvites completion template includes the required argument slot");
    ok &= expect(gotoRow >= 0 &&
                   model.data(model.index(gotoRow, 0), CompletionModel::SearchRole)
                       .toString() == QStringLiteral("/goto <message reference>"),
                 "/goto syntax is exposed through the picker");
    ok &= expect(inviteRow >= 0 &&
                   model.data(model.index(inviteRow, 0), CompletionModel::CompletionRole)
                       .toString() == QStringLiteral("/invite @"),
                 "/invite completion template keeps the user-id affordance");
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
    ok &= expectInspection(QStringLiteral("/ignore bob"),
                           {},
                           ValidationState::Invalid,
                           SubmitAction::PreserveComposer,
                           "/ignore rejects non-mxid input");
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
    ok &= testCompleterTemplates();
    ok &= testValidationAndSendBehavior();
    ok &= testValidationMessages();
    ok &= testCommandHelpers();
    return ok ? 0 : 1;
}

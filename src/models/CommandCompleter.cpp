// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/CommandCompleter.h"

#include "models/CompletionModelRoles.h"
#include "timeline/SlashCommands.h"

CommandCompleter::CommandCompleter(QObject *parent)
  : QAbstractListModel(parent)
  , commandCount_(timeline::slash_commands::all().size())
{
}

QHash<int, QByteArray>
CommandCompleter::roleNames() const
{
    return {
      {CompletionModel::CompletionRole, "completionRole"},
      {CompletionModel::SearchRole, "searchRole"},
      {CompletionModel::SearchRole2, "searchRole2"},
      {CompletionModel::SearchRole3, "searchRole3"},
      {Roles::Name, "name"},
      {Roles::Description, "description"},
    };
}

QVariant
CommandCompleter::data(const QModelIndex &index, int role) const
{
    const auto commands = timeline::slash_commands::all();
    if (!index.isValid() || index.row() < 0 ||
        static_cast<std::size_t>(index.row()) >= commands.size()) {
        return {};
    }

    const auto &command = commands[static_cast<std::size_t>(index.row())];

    switch (role) {
    case CompletionModel::CompletionRole:
        return timeline::slash_commands::completionText(command);
    case CompletionModel::SearchRole:
    case Qt::DisplayRole:
    case Roles::Name:
        return timeline::slash_commands::syntaxText(command);
    case CompletionModel::SearchRole2:
    case Roles::Description:
        return timeline::slash_commands::descriptionText(command);
    case CompletionModel::SearchRole3:
        return timeline::slash_commands::searchText(command);
    default:
        return {};
    }
}

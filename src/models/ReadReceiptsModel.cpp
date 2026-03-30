// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/ReadReceiptsModel.h"

#include <QLocale>

#include "utils/Utils.h"

ReadReceiptsModel::ReadReceiptsModel(QString event_id, QString room_id, QObject *parent)
  : QAbstractListModel{parent}
  , event_id_{event_id}
  , room_id_{room_id}
{
}

ReadReceiptsModel::ReadReceiptsModel(QVector<ReadReceiptEntry> entries,
                                     QString room_id,
                                     QObject *parent)
  : QAbstractListModel{parent}
  , room_id_{std::move(room_id)}
{
    setEntries(std::move(entries));
}

void
ReadReceiptsModel::update()
{
    // The cache-backed receipt lookup path is intentionally removed.
}

QHash<int, QByteArray>
ReadReceiptsModel::roleNames() const
{
    // Note: RawTimestamp is purposely not included here
    return {
      {Mxid, "mxid"},
      {DisplayName, "displayName"},
      {AvatarUrl, "avatarUrl"},
      {Timestamp, "timestamp"},
    };
}

QVariant
ReadReceiptsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= (int)readReceipts_.size() || index.row() < 0)
        return {};

    switch (role) {
    case Mxid:
        return readReceipts_[index.row()].mxid;
    case DisplayName:
        if (!readReceipts_[index.row()].displayName.isEmpty())
            return readReceipts_[index.row()].displayName;
        return readReceipts_[index.row()].mxid;
    case AvatarUrl:
        if (!readReceipts_[index.row()].avatarUrl.isEmpty())
            return readReceipts_[index.row()].avatarUrl;
        return {};
    case Timestamp:
        return dateFormat(readReceipts_[index.row()].rawTimestamp);
    case RawTimestamp:
        return readReceipts_[index.row()].rawTimestamp;
    default:
        return {};
    }
}

void
ReadReceiptsModel::setUsers(
  const std::multimap<uint64_t, std::string, std::greater<uint64_t>> &users)
{
    QVector<ReadReceiptEntry> updatedReceipts;
    updatedReceipts.reserve(static_cast<qsizetype>(users.size()));

    for (const auto &user : users)
        updatedReceipts.push_back(ReadReceiptEntry{
          .mxid         = QString::fromStdString(user.second),
          .displayName  = {},
          .avatarUrl    = {},
          .rawTimestamp = QDateTime::fromMSecsSinceEpoch(user.first),
        });

    setEntries(std::move(updatedReceipts));
}

void
ReadReceiptsModel::setEntries(QVector<ReadReceiptEntry> entries)
{
    if (entries == readReceipts_)
        return;

    beginResetModel();
    readReceipts_ = std::move(entries);
    endResetModel();
}

QString
ReadReceiptsModel::dateFormat(const QDateTime &then) const
{
    if (!then.isValid())
        return {};

    auto now  = QDateTime::currentDateTime();
    auto days = then.daysTo(now);

    if (days == 0)
        return QLocale::system().toString(then.time(), QLocale::ShortFormat);
    else if (days < 2)
        return tr("Yesterday, %1")
          .arg(QLocale::system().toString(then.time(), QLocale::ShortFormat));
    else if (days < 7)
        //: %1 is the name of the current day, %2 is the time the read receipt was read. The
        //: result may look like this: Monday, 7:15
        return QStringLiteral("%1, %2").arg(
          then.toString(QStringLiteral("dddd")),
          QLocale::system().toString(then.time(), QLocale::ShortFormat));

    return QLocale::system().toString(then.time(), QLocale::ShortFormat);
}

ReadReceiptsProxy::ReadReceiptsProxy(QString event_id, QString room_id, QObject *parent)
  : QSortFilterProxyModel{parent}
  , model_{event_id, room_id, this}
{
    setSourceModel(&model_);
    setSortRole(ReadReceiptsModel::RawTimestamp);
    sort(0, Qt::DescendingOrder);
    setDynamicSortFilter(true);
}

ReadReceiptsProxy::ReadReceiptsProxy(QVector<ReadReceiptEntry> entries,
                                     QString room_id,
                                     QObject *parent)
  : QSortFilterProxyModel{parent}
  , room_id_{room_id}
  , model_{std::move(entries), room_id, this}
{
    setSourceModel(&model_);
    setSortRole(ReadReceiptsModel::RawTimestamp);
    sort(0, Qt::DescendingOrder);
    setDynamicSortFilter(true);
}

#include "moc_ReadReceiptsModel.cpp"

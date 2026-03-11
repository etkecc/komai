// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/ReadReceiptsModel.h"

#include <QLocale>

#include "cache/Cache.h"
#include "logging/Logging.h"
#include "utils/Utils.h"

ReadReceiptsModel::ReadReceiptsModel(QString event_id, QString room_id, QObject *parent)
  : QAbstractListModel{parent}
  , event_id_{event_id}
  , room_id_{room_id}
{
    try {
        setUsers(cache::readReceipts(event_id_, room_id_));
    } catch (const std::exception &) {
        nhlog::db()->warn("failed to retrieve read receipts for {} {}",
                          event_id_.toStdString(),
                          room_id_.toStdString());

        return;
    }

    cache::onReadReceiptsChanged(this, [this] { update(); });
}

void
ReadReceiptsModel::update()
{
    try {
        setUsers(cache::readReceipts(event_id_, room_id_));
    } catch (const std::exception &) {
        nhlog::db()->warn("failed to retrieve read receipts for {} {}",
                          event_id_.toStdString(),
                          room_id_.toStdString());

        return;
    }
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
        return readReceipts_[index.row()].first;
    case DisplayName:
        return cache::displayName(room_id_, readReceipts_[index.row()].first);
    case AvatarUrl:
        return cache::avatarUrl(room_id_, readReceipts_[index.row()].first);
    case Timestamp:
        return dateFormat(readReceipts_[index.row()].second);
    case RawTimestamp:
        return readReceipts_[index.row()].second;
    default:
        return {};
    }
}

void
ReadReceiptsModel::setUsers(
  const std::multimap<uint64_t, std::string, std::greater<uint64_t>> &users)
{
    QVector<QPair<QString, QDateTime>> updatedReceipts;
    updatedReceipts.reserve(static_cast<qsizetype>(users.size()));

    for (const auto &user : users)
        updatedReceipts.push_back(
          {QString::fromStdString(user.second), QDateTime::fromMSecsSinceEpoch(user.first)});

    if (updatedReceipts == readReceipts_)
        return;

    beginResetModel();
    readReceipts_ = std::move(updatedReceipts);
    endResetModel();
}

QString
ReadReceiptsModel::dateFormat(const QDateTime &then) const
{
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

#include "moc_ReadReceiptsModel.cpp"

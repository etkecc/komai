// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHostInfo>
#include <QRandomGenerator>
#include <QString>
#include <QStringList>

#include <utility>

namespace komai::device_name {

inline QString
os()
{
#if defined(Q_OS_MAC)
    return QStringLiteral("Komai-macOS");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("Komai-Linux");
#elif defined(Q_OS_WIN)
    return QStringLiteral("Komai-Windows");
#elif defined(Q_OS_FREEBSD)
    return QStringLiteral("Komai-FreeBSD");
#elif defined(Q_OS_OPENBSD)
    return QStringLiteral("Komai-OpenBSD");
#else
    return QStringLiteral("Komai");
#endif
}

inline std::string
initial()
{
#if defined(Q_OS_MAC)
    return "Komai on macOS";
#elif defined(Q_OS_LINUX)
    return "Komai on Linux";
#elif defined(Q_OS_WIN)
    return "Komai on Windows";
#elif defined(Q_OS_FREEBSD)
    return "Komai on FreeBSD";
#elif defined(Q_OS_OPENBSD)
    return "Komai on OpenBSD";
#else
    return "Komai";
#endif
}

inline QString
hostname()
{
    return QStringLiteral("Komai-%1").arg(QHostInfo::localHostName());
}

inline const std::pair<QStringList, QStringList> &
wordLists()
{
    static const std::pair<QStringList, QStringList> lists = {
      {
        QStringLiteral("swift"),  QStringLiteral("calm"),   QStringLiteral("bright"),
        QStringLiteral("quiet"),  QStringLiteral("bold"),   QStringLiteral("keen"),
        QStringLiteral("warm"),   QStringLiteral("cool"),   QStringLiteral("wild"),
        QStringLiteral("brave"),  QStringLiteral("clever"), QStringLiteral("gentle"),
        QStringLiteral("noble"),  QStringLiteral("vivid"),  QStringLiteral("steady"),
        QStringLiteral("lucky"),  QStringLiteral("nimble"), QStringLiteral("witty"),
        QStringLiteral("merry"),  QStringLiteral("cosmic"), QStringLiteral("snowy"),
        QStringLiteral("golden"), QStringLiteral("silver"), QStringLiteral("rustic"),
        QStringLiteral("misty"),  QStringLiteral("starry"), QStringLiteral("sandy"),
        QStringLiteral("mossy"),  QStringLiteral("coral"),  QStringLiteral("velvet"),
        QStringLiteral("amber"),  QStringLiteral("ivory"),  QStringLiteral("cedar"),
        QStringLiteral("maple"),  QStringLiteral("polar"),  QStringLiteral("lunar"),
        QStringLiteral("solar"),  QStringLiteral("alpine"), QStringLiteral("rusty"),
        QStringLiteral("azure"),  QStringLiteral("jade"),   QStringLiteral("copper"),
        QStringLiteral("sage"),   QStringLiteral("plum"),   QStringLiteral("olive"),
        QStringLiteral("dusky"),  QStringLiteral("frosty"), QStringLiteral("lively"),
        QStringLiteral("crisp"),  QStringLiteral("rosy"),
      },
      {
        QStringLiteral("owl"),    QStringLiteral("river"),  QStringLiteral("summit"),
        QStringLiteral("harbor"), QStringLiteral("fox"),    QStringLiteral("grove"),
        QStringLiteral("peak"),   QStringLiteral("brook"),  QStringLiteral("cliff"),
        QStringLiteral("reef"),   QStringLiteral("meadow"), QStringLiteral("falcon"),
        QStringLiteral("otter"),  QStringLiteral("cedar"),  QStringLiteral("maple"),
        QStringLiteral("canyon"), QStringLiteral("ridge"),  QStringLiteral("dune"),
        QStringLiteral("cove"),   QStringLiteral("trail"),  QStringLiteral("pine"),
        QStringLiteral("wolf"),   QStringLiteral("heron"),  QStringLiteral("pebble"),
        QStringLiteral("orchid"), QStringLiteral("ember"),  QStringLiteral("breeze"),
        QStringLiteral("moss"),   QStringLiteral("coral"),  QStringLiteral("stone"),
        QStringLiteral("fern"),   QStringLiteral("lark"),   QStringLiteral("wren"),
        QStringLiteral("crane"),  QStringLiteral("lotus"),  QStringLiteral("spark"),
        QStringLiteral("flint"),  QStringLiteral("drift"),  QStringLiteral("vale"),
        QStringLiteral("marsh"),  QStringLiteral("cloud"),  QStringLiteral("bloom"),
        QStringLiteral("shell"),  QStringLiteral("leaf"),   QStringLiteral("bay"),
        QStringLiteral("coast"),  QStringLiteral("tide"),   QStringLiteral("gale"),
      },
    };
    return lists;
}

inline QString
random()
{
    const auto &[adjectives, nouns] = wordLists();
    auto *rng                       = QRandomGenerator::global();
    const auto adj                  = adjectives[rng->bounded(adjectives.size())];
    const auto noun                 = nouns[rng->bounded(nouns.size())];
    return QStringLiteral("Komai-%1-%2").arg(adj, noun);
}

inline QString
randomMax()
{
    const auto &[adjectives, nouns] = wordLists();
    int maxAdj                      = 0;
    int maxNoun                     = 0;
    QString longestAdj, longestNoun;
    for (const auto &a : adjectives) {
        if (a.size() > maxAdj) {
            maxAdj     = a.size();
            longestAdj = a;
        }
    }
    for (const auto &n : nouns) {
        if (n.size() > maxNoun) {
            maxNoun     = n.size();
            longestNoun = n;
        }
    }
    return QStringLiteral("Komai-%1-%2").arg(longestAdj, longestNoun);
}

} // namespace komai::device_name

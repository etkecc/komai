// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Utils.h"

#include <QFile>
#include <QIODevice>
#include <QRandomGenerator64>
#include <QUrl>

#include <optional>
#include <utility>

#include "Logging.h"

namespace {
static const QList<QChar> diacritics = []() {
    QList<QChar> ret;
    for (wchar_t c = u'\u0300'; c <= u'\u036f'; ++c)
        ret.append(QChar(c));
    return ret;
}();

QString
mxidFromSegments(QStringView sigil, QStringView mxid)
{
    if (mxid.isEmpty())
        return QString();

    auto mxid_ = QUrl::fromPercentEncoding(mxid.toUtf8());

    if (sigil == u"u") {
        return "@" + mxid_;
    } else if (sigil == u"roomid") {
        return "!" + mxid_;
    } else if (sigil == u"r") {
        return "#" + mxid_;
        //} else if (sigil == "group") {
        //        return "+" + mxid_;
    } else {
        return QString();
    }
}
} // namespace

QString
utils::glitchText(const QString &text)
{
    QString result;
    for (int i = 0; i < text.size(); ++i) {
        result.append(text.at(i));
        if (QRandomGenerator64::global()->bounded(0, 100) >= 25)
            result.append(
              diacritics.at(QRandomGenerator64::global()->bounded(0, diacritics.size())));
    }
    return result;
}

QString
utils::graduallyGlitchText(const QString &text)
{
    QString result;

    const int noGlitch     = text.size() * 0.5;
    const int someGlitch   = text.size() * 0.8;
    const int lotsOfGlitch = text.size() * 0.95;

    for (int i = 0; i < text.size(); ++i) {
        result.append(text.at(i));

        if (i < noGlitch) // first 40% of text is normal
            continue;
        else if (i < someGlitch) // next 25% is progressively glitchier
        {
            if (QRandomGenerator64::global()->bounded(noGlitch, someGlitch) <
                noGlitch + (i - noGlitch) * 0.05)
                result.append(
                  diacritics.at(QRandomGenerator64::global()->bounded(0, diacritics.size())));
        } else if (i < lotsOfGlitch) { // oh no, it's spreading!
            if (QRandomGenerator64::global()->bounded(someGlitch, lotsOfGlitch) < i)
                result.append(
                  diacritics.at(QRandomGenerator64::global()->bounded(0, diacritics.size())));
        } else { // just give up, your computer is cursed now
            do {
                if (QRandomGenerator64::global()->bounded(text.size() / 5, text.size()) < i)
                    result.append(
                      diacritics.at(QRandomGenerator64::global()->bounded(0, diacritics.size())));
            } while (QRandomGenerator64::global()->bounded(0, 100) < 35);
        }
    }

    return result;
}

std::optional<utils::MatrixUriParseResult>
utils::parseMatrixUri(QString uri)
{
    QUrl uri_{uri};

    // Convert matrix.to URIs to proper format
    if (uri_.scheme() == QLatin1String("https") && uri_.host() == QLatin1String("matrix.to")) {
        QString p = uri_.fragment(QUrl::FullyEncoded);
        if (p.startsWith(QLatin1String("/")))
            p.remove(0, 1);

        auto temp = p.split(QStringLiteral("?"));
        QString query;
        if (temp.size() >= 2)
            query = QUrl::fromPercentEncoding(temp.takeAt(1).toUtf8());

        temp            = temp.first().split(QStringLiteral("/"));
        auto identifier = QUrl::fromPercentEncoding(temp.takeFirst().toUtf8());
        QString eventId = QUrl::fromPercentEncoding(temp.join('/').toUtf8());
        if (!identifier.isEmpty()) {
            if (identifier.startsWith(QLatin1String("@"))) {
                QByteArray newUri = "matrix:u/" + QUrl::toPercentEncoding(identifier.remove(0, 1));
                if (!query.isEmpty())
                    newUri.append("?" + query.toUtf8());
                uri_ = QUrl::fromEncoded(newUri);
            } else if (identifier.startsWith(QLatin1String("#"))) {
                QByteArray newUri = "matrix:r/" + QUrl::toPercentEncoding(identifier.remove(0, 1));
                if (!eventId.isEmpty())
                    newUri.append("/e/" + QUrl::toPercentEncoding(eventId.remove(0, 1)));
                if (!query.isEmpty())
                    newUri.append("?" + query.toUtf8());
                uri_ = QUrl::fromEncoded(newUri);
            } else if (identifier.startsWith(QLatin1String("!"))) {
                QByteArray newUri =
                  "matrix:roomid/" + QUrl::toPercentEncoding(identifier.remove(0, 1));
                if (!eventId.isEmpty())
                    newUri.append("/e/" + QUrl::toPercentEncoding(eventId.remove(0, 1)));
                if (!query.isEmpty())
                    newUri.append("?" + query.toUtf8());
                uri_ = QUrl::fromEncoded(newUri);
            }
        }
    }

    // non-matrix URIs are not handled by us, return false
    if (uri_.scheme() != QLatin1String("matrix"))
        return {};

    auto tempPath = uri_.path(QUrl::ComponentFormattingOption::FullyEncoded);
    if (tempPath.startsWith('/'))
        tempPath.remove(0, 1);
    auto segments = QStringView(tempPath).split('/');

    if (segments.size() != 2 && segments.size() != 4)
        return {};

    auto sigil1 = segments[0];
    auto mxid1  = mxidFromSegments(sigil1, segments[1]);
    if (mxid1.isEmpty())
        return {};

    QString mxid2;
    QString sigil2;
    if (segments.size() == 4 && segments[2] == QStringView(u"e")) {
        if (segments[3].isEmpty())
            return {};
        else
            mxid2 = "$" + QUrl::fromPercentEncoding(segments[3].toUtf8());
        sigil2 = "$";
    }

    std::vector<std::string> vias;
    QString action;

    auto items =
      uri_.query(QUrl::ComponentFormattingOption::FullyEncoded).split('&', Qt::SkipEmptyParts);
    for (QString item : std::as_const(items)) {
        if (item.startsWith(QLatin1String("action="))) {
            action = item.remove(QStringLiteral("action="));
        } else if (item.startsWith(QLatin1String("via="))) {
            vias.push_back(QUrl::fromPercentEncoding(item.remove(QStringLiteral("via=")).toUtf8())
                             .toStdString());
        }
    }

    return MatrixUriParseResult{
      .sigil1 = sigil1.toString(),
      .mxid1  = std::move(mxid1),
      .sigil2 = std::move(sigil2),
      .mxid2  = std::move(mxid2),
      .action = std::move(action),
      .vias   = std::move(vias),
    };
}

void
utils::markFileAsFromWeb(const QString &path [[maybe_unused]])
{
#ifdef Q_OS_WINDOWS
    QFile file(path + ":Zone.Identifier");
    if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
        nhlog::net()->error("Failed to open alternate stream for {}", path.toStdString());
        return;
    }

    file.write("[ZoneTransfer]\nZoneId=3");
    file.close();
#endif
}

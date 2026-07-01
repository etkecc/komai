// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/Utils.h"

#include <array>
#include <cmath>
#include <unordered_set>
#include <variant>

#include <QApplication>
#include <QBuffer>
#include <QColorSpace>
#include <QComboBox>
#include <QCryptographicHash>
#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QProcessEnvironment>
#include <QRandomGenerator64>
#include <QScreen>
#include <QStringBuilder>
#include <QTextDocument>
#include <QTimer>
#include <QWindow>
#include <QXmlStreamReader>

#include "chat/ChatPage.h"
#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"

QString
utils::firstChar(const QString &input)
{
    if (input.isEmpty())
        return input;

    for (auto const &c : input.toStdU32String()) {
        if (QString::fromUcs4(&c, 1) != QStringLiteral("#"))
            return QString::fromUcs4(&c, 1).toUpper();
    }

    return QString::fromUcs4(&input.toStdU32String().at(0), 1).toUpper();
}

QString
utils::humanReadableFileSize(uint64_t bytes)
{
    constexpr static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    constexpr static const int length    = sizeof(units) / sizeof(units[0]);

    int u       = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && u < length) {
        ++u;
        size /= 1024.0;
    }

    return QString::number(size, 'g', 4) + ' ' + units[u];
}

int
utils::levenshtein_distance(const std::string &s1, const std::string &s2)
{
    const auto nlen = s1.size();
    const auto hlen = s2.size();

    if (hlen == 0)
        return -1;
    if (nlen == 1)
        return (int)s2.find(s1);

    std::vector<int> row1(hlen + 1, 0);

    for (size_t i = 0; i < nlen; ++i) {
        std::vector<int> row2(1, (int)i + 1);

        for (size_t j = 0; j < hlen; ++j) {
            const int cost = s1[i] != s2[j];
            row2.push_back(std::min(row1[j + 1] + 1, std::min(row2[j] + 1, row1[j] + cost)));
        }

        row1.swap(row2);
    }

    return *std::min_element(row1.begin(), row1.end());
}

QPixmap
utils::scaleImageToPixmap(const QImage &img, int size)
{
    if (img.isNull())
        return QPixmap();

    // Deprecated in 5.13: const double sz =
    //  std::ceil(QApplication::desktop()->screen()->devicePixelRatioF() * (double)size);
    const int sz = static_cast<int>(
      std::ceil(QGuiApplication::primaryScreen()->devicePixelRatio() * (double)size));
    return QPixmap::fromImage(img.scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

QPixmap
utils::scaleDown(uint64_t maxWidth, uint64_t maxHeight, const QPixmap &source)
{
    if (source.isNull())
        return QPixmap();

    const double widthRatio     = (double)maxWidth / (double)source.width();
    const double heightRatio    = (double)maxHeight / (double)source.height();
    const double minAspectRatio = std::min(widthRatio, heightRatio);

    // Size of the output image.
    int w, h = 0;

    if (minAspectRatio > 1) {
        w = source.width();
        h = source.height();
    } else {
        w = static_cast<int>(static_cast<double>(source.width()) * minAspectRatio);
        h = static_cast<int>(static_cast<double>(source.height()) * minAspectRatio);
    }

    return source.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QString
utils::mxcToHttp(const QUrl &url, const QString &server, int port)
{
    const QString urlString = url.toString();
    const QString prefix    = QStringLiteral("mxc://");
    if (!urlString.startsWith(prefix))
        return {};

    const QString path = urlString.mid(prefix.size());
    const int slashIdx = path.indexOf(u'/');
    if (slashIdx <= 0 || slashIdx + 1 >= path.size())
        return {};

    const QString originServer = path.left(slashIdx);
    const QString mediaId      = path.mid(slashIdx + 1);

    return QStringLiteral("https://%1:%2/_matrix/media/r0/download/%3/%4")
      .arg(server)
      .arg(port)
      .arg(originServer, mediaId);
}

QString
utils::humanReadableFingerprint(const std::string &ed25519_)
{
    auto ed25519 = QString::fromStdString(ed25519_);
    QString fingerprint;
    for (int i = 0; i < ed25519.length(); i = i + 4) {
        fingerprint.append(QStringView(ed25519).mid(i, 4));
        if (i > 0 && i == 20)
            fingerprint.append('\n');
        else if (i < ed25519.length())
            fingerprint.append(' ');
    }
    return fingerprint;
}

QString
utils::linkifyMessage(const QString &body)
{
    const auto bodyStd = body.toStdString();
    return QString::fromStdString(
      std::string(komai::rust::html_linkify(::rust::Str(bodyStd.data(), bodyStd.size()))));
}

QString
utils::escapeMentionMarkdown(QString input)
{
    input = input.toHtmlEscaped();

    constexpr std::array<char, 10> markdownChars = {
      '\\',
      '`',
      '*',
      '_',
      /*'{', '}',*/ '[',
      ']',
      '<',
      '>',
      /* '(', ')',  '#', '-', '+', '.', '!', */ '~',
      '|',
    };

    QByteArray replacement = "\\\\";

    for (char c : markdownChars) {
        replacement[1] = c;
        input.replace(QChar::fromLatin1(c), QLatin1StringView(replacement));
    }

    return input;
}

QString
utils::escapeBlacklistedHtml(const QString &rawStr)
{
    const auto rawStd = rawStr.toStdString();
    return QString::fromStdString(
      std::string(komai::rust::html_sanitize(::rust::Str(rawStd.data(), rawStd.size()))));
}

void
utils::centerWidget(QWidget *widget, QWindow *parent)
{
    if (parent) {
        widget->window()->windowHandle()->setTransientParent(parent);
        return;
    }

    auto findCenter = [childRect = widget->rect()](QRect hostRect) -> QPoint {
        return QPoint(static_cast<int>(hostRect.center().x() - (childRect.width() * 0.5)),
                      static_cast<int>(hostRect.center().y() - (childRect.height() * 0.5)));
    };
    widget->move(findCenter(QGuiApplication::primaryScreen()->geometry()));
}

void
utils::restoreCombobox(QComboBox *combo, const QString &value)
{
    for (auto i = 0; i < combo->count(); ++i) {
        if (value == combo->itemText(i)) {
            combo->setCurrentIndex(i);
            break;
        }
    }
}

namespace {
// Qt Quick's scene graph renders in sRGB but does not colour-manage image
// textures: it uploads decoded pixels verbatim. Wide-gamut images (iPhone
// photos are Display P3, plus Adobe RGB screenshots, etc.) carry an embedded
// ICC profile that QImageReader parses into QImage::colorSpace() but never
// applies, so their P3 values get shown as-if-sRGB and look desaturated/flat
// compared with colour-managed apps like Element. Bake the profile into sRGB
// so the values match what a colour-managed renderer would display.
void
normalizeToSRgb(QImage &image)
{
    if (image.isNull())
        return;

    const QColorSpace srgb(QColorSpace::SRgb);
    const QColorSpace cs = image.colorSpace();
    if (cs.isValid() && cs != srgb)
        image.convertToColorSpace(srgb);
}
}

QImage
utils::readImageFromFile(const QString &filename)
{
    QImageReader reader(filename);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    normalizeToSRgb(image);
    return image;
}
QImage
utils::readImage(const QByteArray &data)
{
    QBuffer buf;
    buf.setData(data);
    QImageReader reader(&buf);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    normalizeToSRgb(image);
    return image;
}

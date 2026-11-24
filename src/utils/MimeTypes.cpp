// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/MimeTypes.h"

#include <QFile>
#include <QFileInfo>
#include <QLatin1String>

#include <algorithm>
#include <array>
#include <cstring>
#include <initializer_list>

namespace {

constexpr qint64 HEADER_BYTES = 1024;

constexpr QLatin1String OCTET_STREAM{"application/octet-stream"};

struct SuffixEntry
{
    const char *suffix;
    const char *mimeType;
};

// Several suffixes may map to one MIME type; the first one listed is the preferred suffix.
// Later MIME types sharing a suffix act as aliases: they resolve to that suffix, while the suffix
// itself resolves to the first MIME type listed for it.
constexpr std::array SUFFIXES{
  // Images
  SuffixEntry{"png", "image/png"},
  SuffixEntry{"jpg", "image/jpeg"},
  SuffixEntry{"jpeg", "image/jpeg"},
  SuffixEntry{"jpe", "image/jpeg"},
  SuffixEntry{"jpg", "image/jpg"},
  SuffixEntry{"gif", "image/gif"},
  SuffixEntry{"webp", "image/webp"},
  SuffixEntry{"svg", "image/svg+xml"},
  SuffixEntry{"bmp", "image/bmp"},
  SuffixEntry{"bmp", "image/x-bmp"},
  SuffixEntry{"bmp", "image/x-ms-bmp"},
  SuffixEntry{"tif", "image/tiff"},
  SuffixEntry{"tiff", "image/tiff"},
  SuffixEntry{"ico", "image/vnd.microsoft.icon"},
  SuffixEntry{"ico", "image/x-icon"},
  SuffixEntry{"avif", "image/avif"},
  SuffixEntry{"heic", "image/heic"},
  SuffixEntry{"heif", "image/heif"},
  SuffixEntry{"jxl", "image/jxl"},

  // Video
  SuffixEntry{"mp4", "video/mp4"},
  SuffixEntry{"m4v", "video/mp4"},
  SuffixEntry{"webm", "video/webm"},
  SuffixEntry{"mkv", "video/x-matroska"},
  SuffixEntry{"mov", "video/quicktime"},
  SuffixEntry{"avi", "video/x-msvideo"},
  SuffixEntry{"avi", "video/vnd.avi"},
  SuffixEntry{"ogv", "video/ogg"},
  SuffixEntry{"mpg", "video/mpeg"},
  SuffixEntry{"mpeg", "video/mpeg"},
  SuffixEntry{"3gp", "video/3gpp"},

  // Audio
  SuffixEntry{"mp3", "audio/mpeg"},
  SuffixEntry{"ogg", "audio/ogg"},
  SuffixEntry{"oga", "audio/ogg"},
  SuffixEntry{"opus", "audio/ogg"},
  SuffixEntry{"opus", "audio/opus"},
  SuffixEntry{"opus", "audio/x-opus+ogg"},
  SuffixEntry{"flac", "audio/flac"},
  SuffixEntry{"flac", "audio/x-flac"},
  SuffixEntry{"wav", "audio/x-wav"},
  SuffixEntry{"wav", "audio/wav"},
  SuffixEntry{"wav", "audio/vnd.wave"},
  SuffixEntry{"m4a", "audio/mp4"},
  SuffixEntry{"aac", "audio/aac"},
  SuffixEntry{"weba", "audio/webm"},

  // Documents and archives
  SuffixEntry{"pdf", "application/pdf"},
  SuffixEntry{"zip", "application/zip"},
  SuffixEntry{"gz", "application/gzip"},
  SuffixEntry{"7z", "application/x-7z-compressed"},
  SuffixEntry{"txt", "text/plain"},
  SuffixEntry{"log", "text/plain"},
  SuffixEntry{"md", "text/markdown"},
  SuffixEntry{"html", "text/html"},
  SuffixEntry{"csv", "text/csv"},
  SuffixEntry{"json", "application/json"},
  SuffixEntry{"xml", "application/xml"},
};

bool
hasBytesAt(const QByteArray &header, qsizetype offset, QByteArrayView bytes)
{
    return header.size() >= offset + bytes.size() &&
           std::memcmp(header.constData() + offset,
                       bytes.data(),
                       static_cast<std::size_t>(bytes.size())) == 0;
}

// ISO base media files (MP4 and friends) all share one container; the brands tell them apart.
QString
isoBaseMediaType(const QByteArray &header)
{
    const auto brand   = header.mid(8, 4);
    const auto isAnyOf = [&brand](std::initializer_list<QByteArrayView> candidates) {
        return std::find(candidates.begin(), candidates.end(), QByteArrayView(brand)) !=
               candidates.end();
    };

    if (isAnyOf({"avif", "avis"}))
        return QStringLiteral("image/avif");
    if (isAnyOf({"heic", "heix", "heim", "heis", "hevc", "hevx"}))
        return QStringLiteral("image/heic");
    if (isAnyOf({"mif1", "msf1"})) {
        // A generic HEIF brand; the compatible brands that follow name the actual codec.
        const auto boxSize = static_cast<qsizetype>(static_cast<uchar>(header[0])) << 24 |
                             static_cast<qsizetype>(static_cast<uchar>(header[1])) << 16 |
                             static_cast<qsizetype>(static_cast<uchar>(header[2])) << 8 |
                             static_cast<qsizetype>(static_cast<uchar>(header[3]));
        const auto end = std::min(boxSize, header.size());
        for (qsizetype offset = 16; offset + 4 <= end; offset += 4) {
            if (hasBytesAt(header, offset, "avif"))
                return QStringLiteral("image/avif");
            if (hasBytesAt(header, offset, "heic"))
                return QStringLiteral("image/heic");
        }
        return QStringLiteral("image/heif");
    }
    if (brand == "qt  ")
        return QStringLiteral("video/quicktime");
    if (isAnyOf({"M4A ", "M4B "}))
        return QStringLiteral("audio/mp4");
    if (brand.startsWith("3g"))
        return QStringLiteral("video/3gpp");
    if (brand.startsWith("iso") || brand.startsWith("mp4") ||
        isAnyOf({"avc1", "dash", "M4V ", "MSNV", "f4v "}))
        return QStringLiteral("video/mp4");

    // Other users of the container, such as camera raw formats.
    return {};
}

bool
looksLikeSvg(const QByteArray &header)
{
    auto text = header;
    if (text.startsWith("\xEF\xBB\xBF"))
        text.remove(0, 3);
    text = text.trimmed();

    if (text.startsWith("<svg"))
        return true;

    // An XML or doctype prologue may precede the root element. Requiring it keeps HTML pages with
    // an inline <svg> from matching.
    const auto hasPrologue = text.startsWith("<?xml") || text.startsWith("<!DOCTYPE svg");
    return hasPrologue && text.contains("<svg") && !text.contains("<html");
}

QString
meaningfulName(const QMimeType &mimeType)
{
    const auto name = mimeType.name().trimmed();
    return name == OCTET_STREAM ? QString{} : name;
}

}

namespace komai::mime_types {

QString
fallbackForData(const QByteArray &header)
{
    if (hasBytesAt(header, 0, "\x89PNG\r\n\x1a\n"))
        return QStringLiteral("image/png");
    if (hasBytesAt(header, 0, "\xFF\xD8\xFF"))
        return QStringLiteral("image/jpeg");
    if (hasBytesAt(header, 0, "GIF87a") || hasBytesAt(header, 0, "GIF89a"))
        return QStringLiteral("image/gif");
    if (hasBytesAt(header, 0, "RIFF")) {
        if (hasBytesAt(header, 8, "WEBP"))
            return QStringLiteral("image/webp");
        if (hasBytesAt(header, 8, "WAVE"))
            return QStringLiteral("audio/x-wav");
        if (hasBytesAt(header, 8, "AVI "))
            return QStringLiteral("video/x-msvideo");
    }
    if (hasBytesAt(header, 0, QByteArrayView("II*\0", 4)) ||
        hasBytesAt(header, 0, QByteArrayView("MM\0*", 4)))
        return QStringLiteral("image/tiff");
    // The four reserved bytes are always zero, which a text file starting with "BM" cannot match.
    if (hasBytesAt(header, 0, "BM") && hasBytesAt(header, 6, QByteArrayView("\0\0\0\0", 4)))
        return QStringLiteral("image/bmp");
    if (hasBytesAt(header, 0, QByteArrayView("\0\0\0\x0CJXL \r\n\x87\n", 12)))
        return QStringLiteral("image/jxl");
    if (hasBytesAt(header, 4, "ftyp") && header.size() >= 12)
        return isoBaseMediaType(header);
    if (hasBytesAt(header, 0, "\x1A\x45\xDF\xA3"))
        return header.contains("webm") ? QStringLiteral("video/webm")
                                       : QStringLiteral("video/x-matroska");
    if (hasBytesAt(header, 0, "OggS"))
        return header.contains("theora") ? QStringLiteral("video/ogg")
                                         : QStringLiteral("audio/ogg");
    if (hasBytesAt(header, 0, "fLaC"))
        return QStringLiteral("audio/flac");
    if (hasBytesAt(header, 0, "ID3"))
        return QStringLiteral("audio/mpeg");
    if (hasBytesAt(header, 0, "%PDF-"))
        return QStringLiteral("application/pdf");
    if (looksLikeSvg(header))
        return QStringLiteral("image/svg+xml");

    return {};
}

QString
fallbackForFileName(const QString &fileName)
{
    const auto suffix = QFileInfo(fileName).suffix().toLower();
    if (suffix.isEmpty())
        return {};

    const auto entry = std::find_if(SUFFIXES.begin(), SUFFIXES.end(), [&suffix](const auto &item) {
        return suffix == QLatin1String(item.suffix);
    });
    return entry == SUFFIXES.end() ? QString{} : QString::fromLatin1(entry->mimeType);
}

QString
fallbackSuffix(const QString &mimeType)
{
    const auto name = mimeType.section(QLatin1Char(';'), 0, 0).trimmed().toLower();

    const auto entry = std::find_if(SUFFIXES.begin(), SUFFIXES.end(), [&name](const auto &item) {
        return name == QLatin1String(item.mimeType);
    });
    return entry == SUFFIXES.end() ? QString{} : QString::fromLatin1(entry->suffix);
}

QString
forFile(const QString &filePath, QMimeDatabase::MatchMode mode)
{
    if (const auto name = meaningfulName(QMimeDatabase().mimeTypeForFile(filePath, mode));
        !name.isEmpty())
        return name;

    QString result;

    if (mode != QMimeDatabase::MatchExtension) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly))
            result = fallbackForData(file.read(HEADER_BYTES));
    }

    // Callers asking for content matching do so to not be misled by the file name.
    if (result.isEmpty() && mode != QMimeDatabase::MatchContent)
        result = fallbackForFileName(filePath);

    return result.isEmpty() ? QString(OCTET_STREAM) : result;
}

QString
preferredSuffix(const QString &mimeType)
{
    const auto suffix = QMimeDatabase().mimeTypeForName(mimeType).preferredSuffix();
    return suffix.isEmpty() ? fallbackSuffix(mimeType) : suffix;
}

}

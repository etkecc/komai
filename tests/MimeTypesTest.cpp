// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>

#include <QFile>
#include <QTemporaryDir>

#include "TestEnvironment.h"
#include "utils/MimeTypes.h"

namespace mime_types = komai::mime_types;

static int failures = 0;

static void
expectEqual(const QString &actual, const QString &expected, const char *label)
{
    if (actual != expected) {
        std::cerr << "FAILED: " << label << "  actual='" << actual.toStdString() << "'  expected='"
                  << expected.toStdString() << "'\n";
        ++failures;
    }
}

template<std::size_t N>
static QByteArray
bytes(const char (&literal)[N])
{
    return QByteArray(literal, static_cast<qsizetype>(N - 1));
}

static void
testFallbackForData()
{
    const auto expectData = [](const QByteArray &data, const char *expected) {
        expectEqual(mime_types::fallbackForData(data), QString::fromLatin1(expected), expected);
    };

    expectData(bytes("\x89PNG\r\n\x1a\n\0\0\0\rIHDR"), "image/png");
    expectData(bytes("\xFF\xD8\xFF\xE0"), "image/jpeg");
    expectData(bytes("GIF89a"), "image/gif");
    expectData(bytes("RIFF\1\2\3\4WEBPVP8 "), "image/webp");
    expectData(bytes("RIFF\1\2\3\4WAVEfmt "), "audio/x-wav");
    expectData(bytes("II*\0\x08\0\0\0"), "image/tiff");
    expectData(bytes("BM\x10\x20\0\0\0\0\0\0\x36"), "image/bmp");
    expectData(bytes("\0\0\0\x0CJXL \r\n\x87\n"), "image/jxl");
    expectData(bytes("\0\0\0\x1C" "ftypavif\0\0\0\0mif1"), "image/avif");
    expectData(bytes("\0\0\0\x18" "ftypheic\0\0\0\0mif1"), "image/heic");
    expectData(bytes("\0\0\0\x14" "ftypmif1\0\0\0\0mif1avif"), "image/heif");
    expectData(bytes("\0\0\0\x1C" "ftypmif1\0\0\0\0mif1avif"), "image/avif");
    expectData(bytes("\0\0\0\x18" "ftypisom\0\0\2\0"), "video/mp4");
    expectData(bytes("\0\0\0\x14" "ftypqt  \0\0\0\0"), "video/quicktime");
    expectData(bytes("\0\0\0\x14" "ftypM4A \0\0\0\0"), "audio/mp4");
    expectData(bytes("\0\0\0\x14" "ftypcrx \0\0\0\0"), "");
    expectData(bytes("\x1A\x45\xDF\xA3\x42\x82\x84webm"), "video/webm");
    expectData(bytes("\x1A\x45\xDF\xA3\x42\x82\x88matroska"), "video/x-matroska");
    expectData(bytes("OggS\0\2OpusHead"), "audio/ogg");
    expectData(bytes("fLaC\0"), "audio/flac");
    expectData(bytes("ID3\4\0"), "audio/mpeg");
    expectData(bytes("%PDF-1.7"), "application/pdf");
    expectData(bytes("<svg xmlns='http://www.w3.org/2000/svg'/>"), "image/svg+xml");
    expectData(bytes("<?xml version='1.0'?>\n<svg/>"), "image/svg+xml");

    expectData(bytes("<!DOCTYPE html><html><body><svg/></body></html>"), "");
    expectData(bytes("BM is how this plain text file starts"), "");
    expectData(bytes("plain text"), "");
    expectData(bytes("RIFF"), "");
    expectData(bytes("\0\0\0\x18" "ftyp"), "");
    expectData({}, "");
}

static void
testFallbackForFileName()
{
    expectEqual(mime_types::fallbackForFileName(QStringLiteral("/tmp/a.PNG")),
                QStringLiteral("image/png"),
                "suffix matching ignores case");
    expectEqual(mime_types::fallbackForFileName(QStringLiteral("a.b.jpeg")),
                QStringLiteral("image/jpeg"),
                "last suffix wins");
    expectEqual(
      mime_types::fallbackForFileName(QStringLiteral("png")), {}, "a bare name is not a suffix");
    expectEqual(
      mime_types::fallbackForFileName(QStringLiteral("a.unknown-suffix")), {}, "unknown suffix");
}

static void
testFallbackSuffix()
{
    const auto expectSuffix = [](const char *mimeType, const char *expected) {
        expectEqual(mime_types::fallbackSuffix(QString::fromLatin1(mimeType)),
                    QString::fromLatin1(expected),
                    mimeType);
    };

    expectSuffix("image/png", "png");
    expectSuffix("image/jpeg", "jpg");
    expectSuffix("IMAGE/JPEG", "jpg");
    expectSuffix("image/webp", "webp");
    expectSuffix("image/svg+xml", "svg");
    expectSuffix("audio/ogg; codecs=opus", "ogg");
    expectSuffix("video/mp4", "mp4");
    expectSuffix("application/x-unknown", "");
    expectSuffix("", "");
}

// With the freedesktop search path emptied, Qt builds without an embedded MIME database know no
// types at all. Builds that do embed one must come to the same conclusions.
static void
testForFileWithoutSystemDatabase()
{
    QTemporaryDir dir;

    const auto write = [&dir](const QString &name, const QByteArray &data) {
        const auto path = dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
            std::cerr << "FAILED: cannot write " << path.toStdString() << '\n';
            ++failures;
        }
        return path;
    };

    const auto png = bytes("\x89PNG\r\n\x1a\n\0\0\0\rIHDR");

    expectEqual(mime_types::forFile(write(QStringLiteral("pasted"), png)),
                QStringLiteral("image/png"),
                "content without a suffix");
    expectEqual(mime_types::forFile(write(QStringLiteral("pasted.png"), png)),
                QStringLiteral("image/png"),
                "content with a suffix");
    expectEqual(mime_types::forFile(write(QStringLiteral("renamed.txt"), png),
                                    QMimeDatabase::MatchContent),
                QStringLiteral("image/png"),
                "content matching ignores the suffix");
    expectEqual(mime_types::forFile(write(QStringLiteral("notes.txt"), "hello")),
                QStringLiteral("text/plain"),
                "suffix when content says nothing");
    expectEqual(mime_types::forFile(write(QStringLiteral("fake.png"), bytes("\x01\x02\x03\xFE")),
                                    QMimeDatabase::MatchContent),
                QStringLiteral("application/octet-stream"),
                "content matching does not trust the suffix");
    expectEqual(mime_types::forFile(write(QStringLiteral("blob"), bytes("\x01\x02\x03\xFE"))),
                QStringLiteral("application/octet-stream"),
                "unknown content");
    expectEqual(mime_types::preferredSuffix(QStringLiteral("image/png")),
                QStringLiteral("png"),
                "preferred suffix");
}

int
main()
{
    test_env::EnvVarOverride dataHome("XDG_DATA_HOME", "/nonexistent");
    test_env::EnvVarOverride dataDirs("XDG_DATA_DIRS", "/nonexistent");

    testFallbackForData();
    testFallbackForFileName();
    testFallbackSuffix();
    testForFileWithoutSystemDatabase();

    if (failures > 0) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    return 0;
}

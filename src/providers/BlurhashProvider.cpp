// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "providers/BlurhashProvider.h"

#include <QUrl>

#include "rust/cxx.h"

namespace komai::rust {
// Must match the cxx-generated declaration exactly, including noexcept;
// toolchains reject the mismatch when a TU sees both declarations.
::rust::Vec<::std::uint8_t>
blurhash_decode(::rust::Str hash, ::std::uint32_t width, ::std::uint32_t height) noexcept;
}

void
BlurhashRunnable::run()
{
    if (m_requestedSize.width() < 0 || m_requestedSize.height() < 0) {
        emit error(QStringLiteral("Blurhash needs size request"));
        return;
    }
    if (m_requestedSize.width() == 0 || m_requestedSize.height() == 0) {
        auto image = QImage(m_requestedSize, QImage::Format_RGB32);
        image.fill(QColor(0, 0, 0));
        emit done(image);
        return;
    }

    auto decodeSize = m_requestedSize;
    if (decodeSize.height() > 100 && decodeSize.width() > 100) {
        decodeSize.scale(100, 100, Qt::AspectRatioMode::KeepAspectRatio);
    }

    auto hash = QUrl::fromPercentEncoding(m_id.toUtf8()).toStdString();
    auto w    = static_cast<uint32_t>(decodeSize.width());
    auto h    = static_cast<uint32_t>(decodeSize.height());

    rust::Vec<uint8_t> pixels;
    try {
        pixels = komai::rust::blurhash_decode(rust::Str(hash.data(), hash.size()), w, h);
    } catch (...) {
        emit error(QStringLiteral("Failed decode!"));
        return;
    }

    if (pixels.empty()) {
        emit error(QStringLiteral("Failed decode!"));
        return;
    }

    const auto expectedPixelBytes = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
    if (pixels.size() != expectedPixelBytes) {
        emit error(QStringLiteral("Unexpected blurhash pixel buffer size"));
        return;
    }

    QImage image(pixels.data(),
                 static_cast<int>(w),
                 static_cast<int>(h),
                 static_cast<int>(w) * 4,
                 QImage::Format_RGBA8888);

    image = image.scaled(m_requestedSize);

    emit done(image.convertToFormat(QImage::Format_RGB32));
}

#include "moc_BlurhashProvider.cpp"

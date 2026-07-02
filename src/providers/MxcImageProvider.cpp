// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "providers/MxcImageProvider.h"

#include <QByteArray>
#include <QCache>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QMutex>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QScreen>
#include <QThreadPool>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <vector>

#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "profile/Paths.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

#include "rust/cxx.h"

namespace komai::rust {
// Must match the cxx-generated declaration exactly, including noexcept:
// this TU also sees the generated header, and MSVC/clang reject the
// mismatch as a redefinition.
::rust::Vec<::std::uint8_t>
lanczos_resize_rgba(::rust::Slice<const ::std::uint8_t> pixels,
                    ::std::uint32_t src_w,
                    ::std::uint32_t src_h,
                    ::std::uint32_t dst_w,
                    ::std::uint32_t dst_h) noexcept;
}

static QImage
clipRadius(QImage img, double radius);

namespace {
QString
currentProfileId()
{
    auto settings = UserSettings::instance();
    return settings ? settings->profile() : QStringLiteral("default");
}

QString
providerIdToMxcUri(const QString &id)
{
    return komai::matrix::normalizeMxcUri(id);
}

bool
isMatrixTimelineProviderId(const QString &id)
{
    return id.startsWith(QStringLiteral("matrix-timeline:"));
}

QString
providerIdToMatrixTimelineItemId(const QString &id)
{
    return isMatrixTimelineProviderId(id) ? id.mid(QStringLiteral("matrix-timeline:").size())
                                          : QString();
}

void
purgeFilesInDir(const QString &dirPath)
{
    QDir dir(dirPath,
             "",
             QDir::SortFlags(QDir::Name | QDir::IgnoreCase),
             QDir::Filter::Writable | QDir::Filter::NoDotAndDotDot | QDir::Filter::Files);

    for (const auto &fileInfo : dir.entryInfoList()) {
        if (fileInfo.fileTime(QFile::FileTime::FileAccessTime)
              .daysTo(QDateTime::currentDateTime()) > app_paths::cache::mediaPurgeAgeDays) {
            if (QFile::remove(fileInfo.absoluteFilePath()))
                komai::logging::net()->info("Deleted stale media '{}'",
                                            fileInfo.absoluteFilePath().toStdString());
            else
                komai::logging::net()->warn("Failed to delete stale media '{}'",
                                            fileInfo.absoluteFilePath().toStdString());
        }
    }
}

std::optional<uint64_t>
activeMatrixBackendHandleId()
{
    const auto *window = MainWindow::instance();
    if (!window || window->matrixBackendHandleId() == 0)
        return std::nullopt;

    return window->matrixBackendHandleId();
}

QImage
prepareThumbnailImage(QByteArray data,
                      const QSize &requestedSize,
                      bool cropLocally,
                      double radius,
                      const QString &id)
{
    QImage image = utils::readImage(data);
    if (!image.isNull()) {
        if (requestedSize.width() <= 0) {
            image = image.scaledToHeight(requestedSize.height(), Qt::SmoothTransformation);
        } else {
            image = image.scaled(requestedSize,
                                 cropLocally ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
            if (cropLocally) {
                image = image.copy((image.width() - requestedSize.width()) / 2,
                                   (image.height() - requestedSize.height()) / 2,
                                   requestedSize.width(),
                                   requestedSize.height());
            }
        }

        if (radius != 0) {
            image = clipRadius(std::move(image), radius);
        }
    }

    if (!isMatrixTimelineProviderId(id))
        image.setText(QStringLiteral("mxc url"), providerIdToMxcUri(id));
    return image;
}

// Downscale `src` to fit within `box` (aspect-preserving, never upscaling) via a
// Lanczos3 resample on the Rust side. Qt Quick's GPU minification is soft; a
// Lanczos downscale recovers noticeably more edge detail for the full-view media
// overlay. `src` must already be colour-managed to sRGB. Falls back to `src`
// unchanged when no downscale is needed or the resample fails.
static QImage
resizeToFitLanczos(const QImage &src, const QSize &box)
{
    if (src.isNull() || !box.isValid() || box.isEmpty())
        return src;

    // Fit inside the box preserving aspect; never upscale (that only softens).
    const QSize target = src.size().scaled(box, Qt::KeepAspectRatio);
    if (target.width() <= 0 || target.height() <= 0)
        return src;
    if (target.width() >= src.width() && target.height() >= src.height())
        return src;

    // Format_RGBA8888 is tightly packed (bytesPerLine == width * 4, no padding),
    // so constBits() is exactly the buffer Rust expects.
    const QImage rgba = src.convertToFormat(QImage::Format_RGBA8888);
    const std::size_t n =
      static_cast<std::size_t>(target.width()) * static_cast<std::size_t>(target.height()) * 4;

    rust::Vec<std::uint8_t> out;
    try {
        out = komai::rust::lanczos_resize_rgba(
          ::rust::Slice<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t *>(rgba.constBits()),
            static_cast<std::size_t>(rgba.sizeInBytes())),
          static_cast<std::uint32_t>(rgba.width()),
          static_cast<std::uint32_t>(rgba.height()),
          static_cast<std::uint32_t>(target.width()),
          static_cast<std::uint32_t>(target.height()));
    } catch (const std::exception &e) {
        komai::logging::ui()->warn("Lanczos resize failed: {}", e.what());
        return src;
    }

    if (out.size() != n)
        return src; // Rust rejected the input; keep the full-resolution image.

    QImage result(target.width(), target.height(), QImage::Format_RGBA8888);
    std::memcpy(result.bits(), out.data(), n);
    return result;
}

// Negative cache with exponential backoff for media fetches that fail (dead
// homeserver, broken federation media, fetch timeouts). Without it, QML
// re-requests a broken avatar/thumbnail on every layout pass, and each request
// spawns a thread-pool worker that blocks on a doomed network fetch, pinning the
// CPU and draining the battery. Keyed by the provider id (which is
// size-independent): a homeserver that cannot serve a piece of media cannot
// serve it at any size, so all size variants share one backoff window.
class MediaFetchBackoff
{
public:
    // Outcome of a fetch-gate check for a given key.
    struct Decision
    {
        bool fetch   = true;  // whether the caller should hit the network now
        bool isRetry = false; // true when fetching again after a prior failure's window elapsed
        qint64 secondsRemaining = 0; // when !fetch, whole seconds left in the backoff window
    };

    // Decides whether a fetch should proceed now. Returns fetch=false (with
    // secondsRemaining) while a backoff window is still open; fetch=true with
    // isRetry=true when a previously-failed key's window has elapsed (a genuine
    // retry); fetch=true with isRetry=false for a key we've never seen fail.
    Decision shouldFetch(const QString &key)
    {
        QMutexLocker locker(&mutex_);
        const auto it = entries_.constFind(key);
        if (it == entries_.constEnd())
            return Decision{true, false, 0};

        const auto now = std::chrono::steady_clock::now();
        if (now >= it->nextAttempt)
            return Decision{true, true, 0};

        const auto remaining =
          std::chrono::duration_cast<std::chrono::seconds>(it->nextAttempt - now).count();
        return Decision{false, false, remaining};
    }

    // Records a failed fetch and returns the backoff (in seconds) that must
    // elapse before the next attempt for this key is allowed.
    qint64 recordFailure(const QString &key)
    {
        QMutexLocker locker(&mutex_);
        auto &entry = entries_[key];
        entry.failureCount += 1;

        const qint64 base           = baseBackoffSeconds();
        const qint64 cap            = std::max<qint64>(kMaxBackoffSeconds, base);
        const int shift             = std::min(entry.failureCount - 1, 16);
        const qint64 backoffSeconds = std::min<qint64>(cap, base * (qint64(1) << shift));
        entry.nextAttempt = std::chrono::steady_clock::now() + std::chrono::seconds(backoffSeconds);
        return backoffSeconds;
    }

    void recordSuccess(const QString &key)
    {
        QMutexLocker locker(&mutex_);
        entries_.remove(key);
    }

    // Drops all backoff state so every media is retried on its next request.
    void reset()
    {
        QMutexLocker locker(&mutex_);
        const auto cleared = entries_.size();
        entries_.clear();
        locker.unlock();

        if (cleared > 0)
            komai::logging::net()->info(
              "Cleared media-fetch backoff for {} item(s); previously failed media will be retried",
              cleared);
    }

private:
    static constexpr qint64 kMaxBackoffSeconds = 30 * 60; // 30 minutes

    // Base (first-failure) backoff in seconds. Overridable via
    // KOMAI_DEBUG_MEDIA_BACKOFF_BASE_SECONDS to make recovery observable during
    // testing without waiting the full default window. Parsed once.
    static qint64 baseBackoffSeconds()
    {
        static const qint64 value = [] {
            const char *raw = std::getenv("KOMAI_DEBUG_MEDIA_BACKOFF_BASE_SECONDS");
            if (raw && *raw) {
                bool ok                = false;
                const qlonglong parsed = QString::fromUtf8(raw).toLongLong(&ok);
                if (ok && parsed > 0)
                    return qint64(parsed);
            }
            return qint64(30);
        }();
        return value;
    }

    struct Entry
    {
        int failureCount = 0;
        std::chrono::steady_clock::time_point nextAttempt;
    };

    QMutex mutex_;
    QHash<QString, Entry> entries_;
};

MediaFetchBackoff &
mediaFetchBackoff()
{
    static MediaFetchBackoff instance;
    return instance;
}

// Coalesces concurrent identical byte-level media fetches. The full-screen media
// overlay shows two Image elements for the same media at once (imgRest at display
// size, imgFull at native size); on cold start both request the original before
// the SDK store cache populates, so without coalescing each spawns its own
// multi-second download of the same bytes. Requests sharing a fetch key attach to
// the first (the "leader"); the leader performs the single network fetch and hands
// the resulting bytes to every waiter, each of which decodes/resizes independently
// for its own display size.
class InflightByteFetches
{
public:
    using Waiter = std::function<void(const std::optional<QByteArray> &, const QString &)>;

    // Returns true if the caller is the leader and must perform the fetch (then
    // call finish()); returns false if an identical fetch is already in flight, in
    // which case `w` has been queued to receive that fetch's result.
    bool join(const QString &key, Waiter w)
    {
        QMutexLocker locker(&mutex_);
        auto it = entries_.find(key);
        if (it != entries_.end()) {
            it->waiters.push_back(std::move(w));
            return false;
        }
        entries_.insert(key, Entry{});
        return true;
    }

    // Called by the leader once its fetch resolves. Removes the in-flight entry and
    // returns the queued waiters so the leader can deliver the result to each.
    std::vector<Waiter> finish(const QString &key)
    {
        QMutexLocker locker(&mutex_);
        auto it = entries_.find(key);
        if (it == entries_.end())
            return {};
        auto waiters = std::move(it->waiters);
        entries_.erase(it);
        return waiters;
    }

private:
    struct Entry
    {
        std::vector<Waiter> waiters;
    };

    QMutex mutex_;
    QHash<QString, Entry> entries_;
};

InflightByteFetches &
inflightByteFetches()
{
    static InflightByteFetches instance;
    return instance;
}

// Test hook: when KOMAI_DEBUG_MEDIA_FAIL_RATE holds a probability in (0, 1],
// that fraction of thumbnail/timeline media fetches are treated as failures
// without touching the network. Lets us exercise the backoff path without a
// homeserver that actually has broken media. Parsed once.
bool
shouldSimulateMediaFailure()
{
    static const double rate = [] {
        const char *raw = std::getenv("KOMAI_DEBUG_MEDIA_FAIL_RATE");
        if (!raw || !*raw)
            return 0.0;

        bool ok             = false;
        const double parsed = QString::fromUtf8(raw).toDouble(&ok);
        if (!ok)
            return 0.0;

        return std::clamp(parsed, 0.0, 1.0);
    }();

    if (rate <= 0.0)
        return false;

    return QRandomGenerator::global()->generateDouble() < rate;
}
}

void
MxcImageProvider::resetFetchBackoff()
{
    mediaFetchBackoff().reset();
}

MxcImageProvider::MxcImageProvider()
  : QQuickAsyncImageProvider()
{
    auto timer = new QTimer(this);
    timer->setInterval(std::chrono::hours(1));
    connect(timer, &QTimer::timeout, this, [] {
        QThreadPool::globalInstance()->start([] {
            komai::logging::net()->info("Running media purge");
            const auto profile = currentProfileId();

            auto purgeMediaDir = [](const QString &baseDir) {
                purgeFilesInDir(baseDir + QStringLiteral("/thumbnails"));
                purgeFilesInDir(baseDir + QStringLiteral("/full"));
            };

            // Purge shared media
            purgeMediaDir(app_paths::cache::sharedMediaDirectory(profile));

            // Purge per-room media and remove empty room directories
            const auto roomsRoot = app_paths::cache::mediaRoot(profile) + QStringLiteral("/rooms");
            QDir roomsDir(roomsRoot,
                          "",
                          QDir::SortFlags(QDir::Name | QDir::IgnoreCase),
                          QDir::Filter::NoDotAndDotDot | QDir::Filter::Dirs);
            for (const auto &roomDirInfo : roomsDir.entryInfoList()) {
                purgeMediaDir(roomDirInfo.absoluteFilePath());
                QDir roomDir(roomDirInfo.absoluteFilePath());
                if (roomDir.isEmpty())
                    roomDir.removeRecursively();
            }
        });
    });
    timer->start();
}

QQuickImageResponse *
MxcImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    auto id_      = id;
    bool crop     = true;
    double radius = 0;
    auto size     = requestedSize;
    QString roomId;
    bool fullQuality = false;

    if (requestedSize.width() == 0 && requestedSize.height() == 0)
        size = QSize();

    auto queryStart = id.lastIndexOf('?');
    if (queryStart != -1) {
        id_            = id.left(queryStart);
        auto query     = QStringView(id).mid(queryStart + 1);
        auto queryBits = query.split('&');

        for (auto b : std::as_const(queryBits)) {
            if (b == QStringView(u"scale")) {
                crop = false;
            } else if (b == QStringView(u"full")) {
                // Full-quality mode: fetch the original media and downscale it
                // locally with Lanczos (crisp), rather than pulling a low-res
                // server thumbnail. Used by the full-screen media overlay.
                crop        = false;
                fullQuality = true;
            } else if (b.startsWith(QStringView(u"radius="))) {
                radius = b.mid(7).toDouble();
            } else if (b.startsWith(u"avatarSize=")) {
                // Logical avatar size from QML.  Apply QScreen DPR to get the
                // physical thumbnail size.  This avoids per-surface DPR variance
                // on Wayland fractional scaling (Qt may scale sourceSize by
                // different DPR values for the main window vs overlay dialogs).
                double dpr = 1.0;
                for (const auto *s : QGuiApplication::screens())
                    dpr = qMax(dpr, s->devicePixelRatio());
                int side = qMax(1, qRound(b.mid(11).toInt() * dpr));
                size     = QSize(side, side);
            } else if (b.startsWith(u"height=")) {
                size.setHeight(b.mid(7).toInt());
                size.setWidth(0);
            } else if (b.startsWith(QStringView(u"room="))) {
                roomId = b.mid(5).toString();
            }
        }
    }

    return new MxcImageResponse(id_, crop, radius, size, roomId, fullQuality);
}

void
MxcImageRunnable::run()
{
    MxcImageProvider::download(
      m_id,
      m_requestedSize,
      [this](QString id, QSize, QImage image, QString) {
          if (image.isNull()) {
              emit error(QStringLiteral("Failed to download image: %1").arg(id));
          } else {
              emit done(image);
          }
          this->deleteLater();
      },
      m_crop,
      m_radius,
      m_roomId,
      m_fullQuality);
}

static QImage
clipRadius(QImage img, double radius)
{
    QImage out(img.size(), QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);

    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath ppath;
    ppath.addRoundedRect(img.rect(), radius, radius, Qt::SizeMode::RelativeSize);

    painter.setClipPath(ppath);
    painter.drawImage(img.rect(), img);

    return out;
}

static void
possiblyUpdateAccessTime(const QFileInfo &fileInfo)
{
    if (fileInfo.fileTime(QFile::FileTime::FileAccessTime).daysTo(QDateTime::currentDateTime()) >
        7) {
        komai::logging::net()->debug("Updating file time for '{}'",
                                     fileInfo.absoluteFilePath().toStdString());

        QFile f(fileInfo.absoluteFilePath());

        if (!f.open(QIODevice::ReadWrite) ||
            !f.setFileTime(QDateTime::currentDateTime(), QFile::FileTime::FileAccessTime)) {
            komai::logging::net()->warn("Failed to update filetime for '{}'",
                                        fileInfo.absoluteFilePath().toStdString());
        }
    }
}

void
MxcImageProvider::download(const QString &id,
                           const QSize &requestedSize,
                           std::function<void(QString, QSize, QImage, QString)> then,
                           bool crop,
                           double radius,
                           const QString &roomId,
                           bool fullQuality)
{
    if (id.isEmpty()) {
        komai::logging::net()->warn("Attempted to download image with empty ID");
        then(id, QSize{}, QImage{}, QString{});
        return;
    }

    if (isMatrixTimelineProviderId(id)) {
        if (const auto handleId = activeMatrixBackendHandleId()) {
            const auto gate = mediaFetchBackoff().shouldFetch(id);
            if (!gate.fetch) {
                // A recent fetch for this media failed; skip the network round-trip
                // until the backoff window elapses. Behaves like a cached failure.
                then(id, QSize(), {}, QLatin1String(""));
                return;
            }
            if (gate.isRetry)
                komai::logging::net()->info(
                  "Retrying matrix-sdk active timeline media {} after backoff", id.toStdString());

            const auto requestedWidth  = requestedSize.width() > 0 ? requestedSize.width() : 0;
            const auto requestedHeight = requestedSize.height() > 0 ? requestedSize.height() : 0;
            // Full-quality mode fetches the original media (0x0 = no server
            // thumbnail) and downscales it locally with Lanczos below.
            const auto fetchWidth  = fullQuality ? 0 : requestedWidth;
            const auto fetchHeight = fullQuality ? 0 : requestedHeight;
            const auto itemId      = providerIdToMatrixTimelineItemId(id);

            // Byte-fetch coalescing key: identical (handle, item, fetch-size, crop)
            // fetches share one download. The overlay's imgRest and imgFull both
            // fetch 0x0 here, so they collapse onto a single network round-trip.
            const QString fetchKey = QStringLiteral("%1|%2|%3x%4|%5")
                                       .arg(static_cast<qulonglong>(*handleId))
                                       .arg(itemId)
                                       .arg(fetchWidth)
                                       .arg(fetchHeight)
                                       .arg(crop ? 1 : 0);

            // Per-request post-fetch processing (decode + colour-manage + optional
            // Lanczos downscale + optional corner clip), shared by the fetch leader
            // and every coalesced waiter. `fetchMs` is the network time (0 when the
            // bytes were coalesced from another request's fetch). Never mutates
            // backoff state — that is the leader's job.
            auto deliver = [requestedSize, radius, then, id, itemId, fullQuality](
                             const std::optional<QByteArray> &data, qint64 fetchMs) {
                if (!data || data->isEmpty()) {
                    then(id, QSize(), {}, QLatin1String(""));
                    return;
                }

                using clk         = std::chrono::steady_clock;
                const auto tStart = clk::now();
                QImage image;
                auto tDecoded = tStart;
                auto tResized = tStart;
                if (fullQuality) {
                    // Decode + colour-manage the full media, then Lanczos-downscale
                    // to the requested display size for a crisp full-view image.
                    image    = utils::readImage(*data);
                    tDecoded = clk::now();
                    if (requestedSize.isValid() && !image.isNull())
                        image = resizeToFitLanczos(image, requestedSize);
                    tResized = clk::now();
                    if (radius != 0 && !image.isNull())
                        image = clipRadius(std::move(image), radius);
                } else if (requestedSize.isValid()) {
                    image    = prepareThumbnailImage(*data, requestedSize, false, radius, id);
                    tDecoded = clk::now();
                    tResized = tDecoded;
                } else {
                    image    = utils::readImage(*data);
                    tDecoded = clk::now();
                    tResized = tDecoded;
                }

                // Always-on, low-noise media-load timing: overlay full-quality
                // loads (infrequent) always log; frequent thumbnails only when
                // slow. Answers "download vs decode vs resize" without a rebuild.
                const auto ms = [](clk::time_point a, clk::time_point b) {
                    return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
                };
                const auto decodeMs = ms(tStart, tDecoded);
                const auto resizeMs = ms(tDecoded, tResized);
                const auto totalMs  = fetchMs + decodeMs + resizeMs;
                if (fullQuality || totalMs > 250)
                    komai::logging::net()->warn(
                      "media timing {}: fetch={}ms decode={}ms resize={}ms total={}ms "
                      "({} KiB in, {}x{} out{}{})",
                      itemId.toStdString(),
                      fetchMs,
                      decodeMs,
                      resizeMs,
                      totalMs,
                      static_cast<long long>(data->size() / 1024),
                      image.width(),
                      image.height(),
                      fullQuality ? ", full-quality" : "",
                      fetchMs == 0 ? ", coalesced" : "");

                then(id, requestedSize, image, QLatin1String(""));
            };

            // Attach to an identical in-flight fetch when one exists; otherwise
            // become the leader and perform the single network fetch below.
            const bool isLeader = inflightByteFetches().join(
              fetchKey, [deliver](const std::optional<QByteArray> &data, const QString &) {
                  // Decode on the waiter's own thread-pool task so a slow decode of
                  // one display size never blocks the leader or the other waiters.
                  QThreadPool::globalInstance()->start([deliver, data] { deliver(data, 0); });
              });

            if (!isLeader)
                return;

            QThreadPool::globalInstance()->start(
              [fetchKey, deliver, id, handleId, fetchWidth, fetchHeight, crop, itemId] {
                  using clk          = std::chrono::steady_clock;
                  const auto tStart  = clk::now();
                  const auto context = komai::matrix_backend::blockingCallContext();
                  QString error;
                  std::optional<QByteArray> data;
                  if (shouldSimulateMediaFailure())
                      error =
                        QStringLiteral("simulated media failure (KOMAI_DEBUG_MEDIA_FAIL_RATE)");
                  else if (fetchWidth <= 0 && fetchHeight <= 0)
                      // Full-file fetch (the overlay's full-quality path):
                      // fetches the same MediaFormat::File request but reports
                      // download progress, so the overlay can show a real
                      // percentage via MediaDownloadProgressWatcher.
                      data = komai::MatrixBackendRuntimeService::
                        fetchActiveRoomTimelineMediaContentWithProgress(
                          context, *handleId, itemId, &error);
                  else
                      data =
                        komai::MatrixBackendRuntimeService::fetchActiveRoomTimelineMediaContent(
                          context, *handleId, itemId, fetchWidth, fetchHeight, crop, &error);
                  const auto fetchMs =
                    std::chrono::duration_cast<std::chrono::milliseconds>(clk::now() - tStart)
                      .count();

                  if (!data || data->isEmpty()) {
                      const auto backoffSeconds = mediaFetchBackoff().recordFailure(id);
                      komai::logging::net()->warn(
                        "Failed to fetch matrix-sdk active timeline media {} via backend handle "
                        "{}: {} — retrying in no less than {}s",
                        itemId.toStdString(),
                        *handleId,
                        error.toStdString(),
                        backoffSeconds);
                  } else {
                      mediaFetchBackoff().recordSuccess(id);
                  }

                  // Hand the fetched bytes (or the failure) to every coalesced
                  // waiter — each on its own decode task — then do our own.
                  const auto waiters = inflightByteFetches().finish(fetchKey);
                  for (const auto &w : waiters)
                      w(data, error);

                  deliver(data, fetchMs);
              });
            return;
        }

        komai::logging::net()->warn(
          "Refusing matrix-sdk active-timeline media fetch for '{}' without an "
          "active runtime handle",
          id.toStdString());
        then(id, QSize{}, QImage{}, QString{});
        return;
    }

    bool cropLocally = false;
    if (crop && requestedSize.width() > 96) {
        crop        = false;
        cropLocally = true;
    }

    // Full-quality requests (the media overlay) never take the server-thumbnail
    // shortcut: they fetch the original and Lanczos-downscale it locally in the
    // full-media branch below, matching the active-timeline path.
    if (!fullQuality && requestedSize.isValid() &&
        // Protect against synapse not following the spec:
        // https://github.com/matrix-org/synapse/issues/5302
        requestedSize.height() <= 600 && requestedSize.width() <= 800) {
        QFileInfo fileInfo(app_paths::cache::mediaThumbnailFileForMxc(
          currentProfileId(), id, requestedSize, crop, radius, roomId));
        QDir().mkpath(fileInfo.absolutePath());

        if (fileInfo.exists()) {
            QImage image = utils::readImageFromFile(fileInfo.absoluteFilePath());
            if (!image.isNull()) {
                possiblyUpdateAccessTime(fileInfo);

                if (requestedSize.width() <= 0) {
                    image = image.scaledToHeight(requestedSize.height(), Qt::SmoothTransformation);
                } else {
                    image = image.scaled(requestedSize,
                                         cropLocally ? Qt::KeepAspectRatioByExpanding
                                                     : Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
                    if (cropLocally) {
                        image = image.copy((image.width() - requestedSize.width()) / 2,
                                           (image.height() - requestedSize.height()) / 2,
                                           requestedSize.width(),
                                           requestedSize.height());
                    }
                }

                if (radius != 0) {
                    image = clipRadius(std::move(image), radius);
                }

                if (!image.isNull()) {
                    then(id, requestedSize, image, fileInfo.absoluteFilePath());
                    return;
                }
            }
        }

        if (const auto handleId = activeMatrixBackendHandleId()) {
            const auto gate = mediaFetchBackoff().shouldFetch(id);
            if (!gate.fetch) {
                // A recent fetch for this media failed; skip the network round-trip
                // until the backoff window elapses. Behaves like a cached failure.
                then(id, QSize(), {}, QLatin1String(""));
                return;
            }
            if (gate.isRetry)
                komai::logging::net()->info("Retrying matrix-sdk thumbnail {} after backoff",
                                            id.toStdString());

            const auto requestedWidth  = requestedSize.width() > 0 ? requestedSize.width() : 0;
            const auto requestedHeight = requestedSize.height() > 0 ? requestedSize.height() : 0;

            QThreadPool::globalInstance()->start([fileInfo,
                                                  requestedSize,
                                                  radius,
                                                  then,
                                                  id,
                                                  cropLocally,
                                                  handleId,
                                                  requestedWidth,
                                                  requestedHeight] {
                const auto context = komai::matrix_backend::blockingCallContext();
                QString error;
                std::optional<QByteArray> data;
                if (shouldSimulateMediaFailure())
                    error = QStringLiteral("simulated media failure (KOMAI_DEBUG_MEDIA_FAIL_RATE)");
                else
                    data =
                      komai::MatrixBackendRuntimeService::fetchMediaContent(context,
                                                                            *handleId,
                                                                            providerIdToMxcUri(id),
                                                                            requestedWidth,
                                                                            requestedHeight,
                                                                            !cropLocally,
                                                                            &error);
                if (!data || data->isEmpty()) {
                    const auto backoffSeconds = mediaFetchBackoff().recordFailure(id);
                    komai::logging::net()->warn(
                      "Failed to fetch matrix-sdk thumbnail {} via backend handle {}: {} — "
                      "retrying in no less than {}s",
                      id.toStdString(),
                      *handleId,
                      error.toStdString(),
                      backoffSeconds);
                    then(id, QSize(), {}, QLatin1String(""));
                    return;
                }
                mediaFetchBackoff().recordSuccess(id);

                auto image = prepareThumbnailImage(*data, requestedSize, cropLocally, radius, id);
                if (image.save(fileInfo.absoluteFilePath(), "png")) {
                    utils::markFileAsFromWeb(fileInfo.absoluteFilePath());
                    komai::logging::ui()->debug("Wrote: {}",
                                                fileInfo.absoluteFilePath().toStdString());
                } else {
                    komai::logging::ui()->debug("Failed to write: {}",
                                                fileInfo.absoluteFilePath().toStdString());
                }

                then(id, requestedSize, image, fileInfo.absoluteFilePath());
            });
            return;
        }

        komai::logging::net()->warn(
          "Cannot fetch matrix-sdk thumbnail '{}' without an active runtime "
          "handle",
          id.toStdString());
        then(id, QSize(), {}, QLatin1String(""));
    } else {
        try {
            QFileInfo fileInfo(
              app_paths::cache::mediaFullFileForMxc(currentProfileId(), id, roomId));
            QDir().mkpath(fileInfo.absolutePath());
            QFile f(fileInfo.absoluteFilePath());

            if (fileInfo.exists() && f.open(QIODevice::ReadOnly)) {
                QImage image = utils::readImageFromFile(fileInfo.absoluteFilePath());
                if (!image.isNull()) {
                    possiblyUpdateAccessTime(fileInfo);
                    // Full-quality overlay view: crisp Lanczos downscale of the
                    // full-res cached original to the on-screen size. The cache file
                    // stays full-res so other sizes and zoom reuse it.
                    if (fullQuality && requestedSize.isValid())
                        image = resizeToFitLanczos(image, requestedSize);
                    if (radius != 0) {
                        image = clipRadius(std::move(image), radius);
                    }

                    then(id, requestedSize, image, fileInfo.absoluteFilePath());
                    return;
                }
            }

            if (const auto handleId = activeMatrixBackendHandleId()) {
                QThreadPool::globalInstance()->start(
                  [fileInfo, requestedSize, then, id, radius, handleId, fullQuality] {
                      const auto context = komai::matrix_backend::blockingCallContext();
                      QString error;
                      const auto data = komai::MatrixBackendRuntimeService::fetchMediaContent(
                        context, *handleId, providerIdToMxcUri(id), 0, 0, false, &error);
                      if (!data || data->isEmpty()) {
                          komai::logging::net()->warn(
                            "Failed to fetch matrix-sdk media {} via backend handle {}: {}",
                            id.toStdString(),
                            *handleId,
                            error.toStdString());
                          then(id, QSize(), {}, QLatin1String(""));
                          return;
                      }

                      QFile f(fileInfo.absoluteFilePath());
                      if (!f.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
                          komai::logging::net()->error("Failed to write {}: {}",
                                                       id.toStdString(),
                                                       f.errorString().toStdString());
                          then(id, QSize(), {}, QLatin1String(""));
                          return;
                      }
                      f.write(*data);
                      f.close();
                      utils::markFileAsFromWeb(fileInfo.absoluteFilePath());

                      QImage image = utils::readImageFromFile(fileInfo.absoluteFilePath());
                      // Full-quality overlay view: Lanczos downscale to the on-screen
                      // size (the just-written cache file keeps the full original).
                      if (fullQuality && requestedSize.isValid() && !image.isNull())
                          image = resizeToFitLanczos(image, requestedSize);
                      if (radius != 0)
                          image = clipRadius(std::move(image), radius);
                      image.setText(QStringLiteral("mxc url"), "mxc://" + id);

                      then(id, requestedSize, image, fileInfo.absoluteFilePath());
                  });
                return;
            }

            komai::logging::net()->warn(
              "Cannot fetch matrix-sdk full media '{}' without an active runtime "
              "handle",
              id.toStdString());
            then(id, QSize(), {}, QLatin1String(""));
        } catch (std::exception &e) {
            komai::logging::net()->error("Exception while downloading media: {}", e.what());
        }
    }
}

#include "moc_MxcImageProvider.cpp"

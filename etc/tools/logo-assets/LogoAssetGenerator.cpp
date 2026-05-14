// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Build-only helper that generates raster/logo container assets from
// resources/komai.svg.
//
// We intentionally keep this inside the existing Qt toolchain instead of
// shelling out to external tools like ImageMagick or rsvg-convert. That keeps
// Linux packaging, CI, AppImage, and Flatpak builds from needing extra raster
// conversion dependencies just to materialize derived logo files.

#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QMap>
#include <QPainter>
#include <QSaveFile>
#include <QSvgRenderer>
#include <QTextStream>

#include <array>
#include <cstdio>

namespace {

struct PngTarget
{
    int size;
    const char *fileName;
};

constexpr std::array<PngTarget, 9> kPngTargets = {
  PngTarget{16, "komai-16.png"},
  PngTarget{32, "komai-32.png"},
  PngTarget{44, "komai-44.png"},
  PngTarget{48, "komai-48.png"},
  PngTarget{64, "komai-64.png"},
  PngTarget{128, "komai-128.png"},
  PngTarget{150, "komai-150.png"},
  PngTarget{256, "komai-256.png"},
  PngTarget{512, "komai-512.png"},
};

struct DmgBackgroundTarget
{
    int width;
    int height;
    const char *fileName;
};

// 1x then @2x retina. Window dims (540x380) match the create-dmg invocation in
// the macOS release composite action; icon positions baked into that config
// land at the icon-row y the arrow is drawn at below.
constexpr std::array<DmgBackgroundTarget, 2> kDmgBackgroundTargets = {
  DmgBackgroundTarget{540, 380, "komai-dmg-background.png"},
  DmgBackgroundTarget{1080, 760, "komai-dmg-background@2x.png"},
};

void
printError(const QString &message)
{
    QTextStream(stderr) << message << Qt::endl;
}

bool
writeBytes(const QString &path, const QByteArray &data, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = QStringLiteral("Failed to open '%1' for writing: %2").arg(path, file.errorString());
        return false;
    }

    if (file.write(data) != data.size()) {
        *error =
          QStringLiteral("Failed to write '%1': %2").arg(path, file.errorString());
        return false;
    }

    if (!file.commit()) {
        *error = QStringLiteral("Failed to commit '%1': %2").arg(path, file.errorString());
        return false;
    }

    return true;
}

bool
saveImage(const QImage &image, const QString &path, QString *error)
{
    QImageWriter writer(path);
    if (!writer.write(image)) {
        *error =
          QStringLiteral("Failed to write '%1': %2").arg(path, writer.errorString());
        return false;
    }

    return true;
}

QImage
renderSvg(QSvgRenderer &renderer, int size)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter);

    return image;
}

// Composes the chrome behind the two real icons Finder floats in the DMG
// window: subtle vertical light-grey gradient, komai mark top-centred, a soft
// grey arrow at the icon row pointing toward the Applications symlink. Icon
// positions are baked into create-dmg metadata (not drawn here) — drawing
// icon placeholders into the background would create double-icons.
QImage
renderDmgBackground(QSvgRenderer &renderer, int width, int height)
{
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QLinearGradient gradient(0, 0, 0, height);
    gradient.setColorAt(0.0, QColor(0xfa, 0xfa, 0xfa));
    gradient.setColorAt(1.0, QColor(0xe4, 0xe4, 0xe4));
    painter.fillRect(image.rect(), gradient);

    // All positions are expressed in the 540x380 reference window; @2x just
    // doubles the scale.
    const qreal scale       = qreal(width) / 540.0;
    const qreal logoSize    = 100.0 * scale;
    const qreal logoX       = (width - logoSize) / 2.0;
    const qreal logoY       = 25.0 * scale;
    renderer.render(&painter, QRectF(logoX, logoY, logoSize, logoSize));

    // Icons are 128 px centred at y=225 (so they span y=161..289). The arrow
    // spans the gap between them — left icon's right edge is ~x=204, right
    // icon's left edge is ~x=336, so the shaft sits at x=215..320 with a small
    // arrowhead beyond.
    const qreal arrowY        = 225.0 * scale;
    const qreal arrowStartX   = 215.0 * scale;
    const qreal arrowEndX     = 320.0 * scale;
    const qreal arrowStroke   = 2.5 * scale;
    const qreal arrowHeadLen  = 12.0 * scale;
    const qreal arrowHeadHalf = 8.0 * scale;
    const QColor arrowColor(0xa0, 0xa0, 0xa0);

    QPen arrowPen(arrowColor, arrowStroke);
    arrowPen.setCapStyle(Qt::FlatCap);
    painter.setPen(arrowPen);
    painter.drawLine(QPointF(arrowStartX, arrowY), QPointF(arrowEndX, arrowY));

    QPolygonF arrowHead;
    arrowHead << QPointF(arrowEndX + arrowHeadLen, arrowY)
              << QPointF(arrowEndX, arrowY - arrowHeadHalf)
              << QPointF(arrowEndX, arrowY + arrowHeadHalf);
    painter.setPen(Qt::NoPen);
    painter.setBrush(arrowColor);
    painter.drawPolygon(arrowHead);

    return image;
}

QStringList
requiredOutputs(bool withIco, bool withIcns, bool withDmgBackground)
{
    QStringList outputs;
    outputs.reserve(int(kPngTargets.size()) + int(kDmgBackgroundTargets.size()) + 4);

    for (const auto &target : kPngTargets)
        outputs.append(QString::fromUtf8(target.fileName));

    outputs.append(QStringLiteral("komai.png"));

    if (withIco)
        outputs.append(QStringLiteral("komai.ico"));
    if (withIcns)
        outputs.append(QStringLiteral("komai.icns"));
    if (withDmgBackground) {
        for (const auto &target : kDmgBackgroundTargets)
            outputs.append(QString::fromUtf8(target.fileName));
    }

    return outputs;
}

bool
allOutputsExist(const QDir &outputDir, const QStringList &outputs)
{
    for (const auto &output : outputs) {
        if (!QFile::exists(outputDir.filePath(output)))
            return false;
    }

    return true;
}

bool
generateAssets(const QByteArray &svgData,
               const QDir &outputDir,
               bool withIco,
               bool withIcns,
               bool withDmgBackground,
               QString *error)
{
    QSvgRenderer renderer(svgData);
    if (!renderer.isValid()) {
        *error = QStringLiteral("Failed to parse SVG input.");
        return false;
    }

    QMap<int, QImage> rasterCache;
    auto imageForSize = [&](int size) -> const QImage & {
        if (!rasterCache.contains(size))
            rasterCache.insert(size, renderSvg(renderer, size));
        return rasterCache[size];
    };

    for (const auto &target : kPngTargets) {
        if (!saveImage(imageForSize(target.size), outputDir.filePath(QString::fromUtf8(target.fileName)), error))
            return false;
    }

    if (!saveImage(imageForSize(512), outputDir.filePath(QStringLiteral("komai.png")), error))
        return false;

    if (withIco && !saveImage(imageForSize(256), outputDir.filePath(QStringLiteral("komai.ico")), error))
        return false;

    if (withIcns && !saveImage(imageForSize(512), outputDir.filePath(QStringLiteral("komai.icns")), error))
        return false;

    if (withDmgBackground) {
        for (const auto &target : kDmgBackgroundTargets) {
            const QImage background = renderDmgBackground(renderer, target.width, target.height);
            if (!saveImage(background, outputDir.filePath(QString::fromUtf8(target.fileName)), error))
                return false;
        }
    }

    return true;
}

} // namespace

int
main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("komai_logo_asset_generator"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
      QStringLiteral("Generate derived logo assets from resources/komai.svg."));
    parser.addHelpOption();

    QCommandLineOption inputOption(QStringLiteral("input"),
                                   QStringLiteral("Path to the source SVG."),
                                   QStringLiteral("path"));
    QCommandLineOption outputDirOption(QStringLiteral("output-dir"),
                                       QStringLiteral("Directory for generated assets."),
                                       QStringLiteral("path"));
    QCommandLineOption withIcoOption(QStringLiteral("with-ico"),
                                     QStringLiteral("Generate a Windows .ico file."));
    QCommandLineOption withIcnsOption(QStringLiteral("with-icns"),
                                      QStringLiteral("Generate a macOS .icns file."));
    QCommandLineOption withDmgBackgroundOption(
      QStringLiteral("with-dmg-background"),
      QStringLiteral("Generate background PNGs (1x and @2x) for the macOS release DMG."));

    parser.addOption(inputOption);
    parser.addOption(outputDirOption);
    parser.addOption(withIcoOption);
    parser.addOption(withIcnsOption);
    parser.addOption(withDmgBackgroundOption);
    parser.process(app);

    const QString inputPath      = parser.value(inputOption);
    const QString outputPath     = parser.value(outputDirOption);
    const bool withIco           = parser.isSet(withIcoOption);
    const bool withIcns          = parser.isSet(withIcnsOption);
    const bool withDmgBackground = parser.isSet(withDmgBackgroundOption);
    const QString checksumRel    = QStringLiteral("komai.svg.sha256");

    if (inputPath.isEmpty() || outputPath.isEmpty())
        parser.showHelp(2);

    QFile svgFile(inputPath);
    if (!svgFile.open(QIODevice::ReadOnly)) {
        printError(QStringLiteral("Failed to open '%1': %2").arg(inputPath, svgFile.errorString()));
        return 1;
    }

    const QByteArray svgData = svgFile.readAll();
    const QByteArray checksum =
      QCryptographicHash::hash(svgData, QCryptographicHash::Sha256).toHex();

    QDir outputDir(outputPath);
    if (!outputDir.exists() && !QDir().mkpath(outputPath)) {
        printError(QStringLiteral("Failed to create output directory '%1'.").arg(outputPath));
        return 1;
    }

    const QString checksumPath = outputDir.filePath(checksumRel);
    QFile checksumFile(checksumPath);
    const auto outputs = requiredOutputs(withIco, withIcns, withDmgBackground);

    if (checksumFile.exists() && checksumFile.open(QIODevice::ReadOnly) &&
        checksumFile.readAll().trimmed() == checksum && allOutputsExist(outputDir, outputs)) {
        return 0;
    }

    QString error;
    if (!generateAssets(svgData, outputDir, withIco, withIcns, withDmgBackground, &error)) {
        printError(error);
        return 1;
    }

    if (!writeBytes(checksumPath, checksum + '\n', &error)) {
        printError(error);
        return 1;
    }

    return 0;
}

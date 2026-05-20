// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/view/TimelineViewManagerMatrixTimelineInternal.h"

#include <cmath>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

#include "chat/ChatPage.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

namespace komai::timeline::view::internal {

bool
matrixMessageUsesMarkdownFormatting()
{
    auto *chatPage       = ChatPage::instance();
    const auto *settings = chatPage ? chatPage->userSettings().get() : nullptr;
    return settings && settings->composerInputMarkdownToHtmlEnabled();
}

QString
renderPlainMatrixMessageHtml(const QString &body)
{
    auto html = body.toHtmlEscaped().replace(u'\n', QStringLiteral("<br>"));
    html      = utils::escapeBlacklistedHtml(html);
    html      = utils::linkifyMessage(html);
    return utils::replaceEmoji(html);
}

QString
matrixPendingAttachmentThumbnail(const QString &filePath, const QString &mimeType)
{
    if (!mimeType.startsWith(QStringLiteral("image/"), Qt::CaseInsensitive))
        return {};

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile())
        return {};

    if (mimeType.compare(QStringLiteral("image/svg+xml"), Qt::CaseInsensitive) == 0)
        return QUrl::fromLocalFile(fileInfo.absoluteFilePath()).toString();

    if (utils::readImageFromFile(fileInfo.absoluteFilePath()).isNull())
        return {};

    return QUrl::fromLocalFile(fileInfo.absoluteFilePath()).toString();
}

QString
matrixTimelineAttachmentFileName(const QString &suggestedFileName, const QString &itemId)
{
    const auto fileName = QFileInfo(suggestedFileName).fileName().trimmed();
    if (!fileName.isEmpty())
        return fileName;

    const auto trimmedItemId = itemId.trimmed();
    if (!trimmedItemId.isEmpty())
        return trimmedItemId + QStringLiteral(".bin");

    return QStringLiteral("matrix-attachment.bin");
}

QString
matrixTimelineMediaCachePath(const QString &fileName)
{
    auto baseDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (baseDir.isEmpty())
        baseDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (baseDir.isEmpty())
        baseDir = QDir::tempPath();

    const auto cacheDir = QDir(baseDir).filePath(QStringLiteral("matrix-runtime-media"));
    QDir().mkpath(cacheDir);
    return QDir(cacheDir).filePath(fileName);
}

bool
isForwardableActiveMatrixTimelineTextKind(const QString &itemKind)
{
    return itemKind == QStringLiteral("message") || itemKind == QStringLiteral("notice") ||
           itemKind == QStringLiteral("emote");
}

int
estimatedInitialMatrixTimelinePageSize(double viewportHeight)
{
    if (viewportHeight <= 0)
        return 0;

    const auto *settings             = UserSettings::instance().get();
    const auto fontSizePt            = settings ? settings->uiFontSizePt() : 13.0;
    const auto bufferedHeadroom      = std::min(viewportHeight * 0.15, fontSizePt * 16.0);
    const auto desiredBufferedHeight = viewportHeight + bufferedHeadroom;
    const auto averageRowHeight      = std::max(56.0, std::round(fontSizePt * 5.25));

    return std::clamp(
      static_cast<int>(std::ceil(desiredBufferedHeight / averageRowHeight)), 15, 40);
}

int
fallbackInitialMatrixTimelinePageSize()
{
    const auto *mainWindow = MainWindow::instance();
    if (!mainWindow)
        return 24;

    const auto *settings       = UserSettings::instance().get();
    const auto fontSizePt      = settings ? settings->uiFontSizePt() : 13.0;
    const auto chromeAllowance = std::max(180.0, std::round(fontSizePt * 14.0));
    const auto approximateViewportHeight =
      std::max(0.0, static_cast<double>(mainWindow->height()) - chromeAllowance);

    return estimatedInitialMatrixTimelinePageSize(approximateViewportHeight);
}

bool
shouldIgnoreMatrixTimelineWarmupShrink(int currentCount, int nextCount)
{
    if (currentCount <= 0 || nextCount >= currentCount)
        return false;

    const auto minimumAcceptedCount =
      std::max(1, static_cast<int>(std::floor(static_cast<double>(currentCount) * 0.8)));
    return nextCount < minimumAcceptedCount;
}

} // namespace komai::timeline::view::internal

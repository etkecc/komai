// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KomaiGlobalObject.h"

#include <algorithm>

#include "emoji/EmojiNormalize.h"
#include "emoji/EmoticonReplace.h"
#include "komai-rust-cxxbridge/ffi.h"
#include "utils/Utils.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QProcess>
#include <QQuickTextDocument>
#include <QScreen>
#include <QStyle>
#include <QTextBoundaryFinder>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>
#include <QVariantMap>
#include <QWindow>
#include <QtMath>

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "matrix/backend/MatrixSdkPaths.h"
#include "profile/Paths.h"
#include "profile/ProfileManager.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"
#include "utils/MediaIcons.h"
#include "utils/Utils.h"

#if XCB_AVAILABLE && QT_CONFIG(xcb)
#include <xcb/xproto.h>
#endif

namespace {

bool
openWithBrowserCommand(const QString &command, const QUrl &url)
{
    const auto trimmedCommand = command.trimmed();
    if (trimmedCommand.isEmpty())
        return false;

    auto args = QProcess::splitCommand(trimmedCommand);
    if (args.isEmpty())
        return false;

    const QString formattedUrl = url.toString(QUrl::FullyEncoded);
    bool hasPlaceholder        = false;
    for (auto &arg : args) {
        if (arg.contains(QStringLiteral("%u"))) {
            hasPlaceholder = true;
            arg            = arg.replace(QStringLiteral("%u"), formattedUrl);
        }
    }

    auto browserCommand = args.takeFirst();
    if (browserCommand.isEmpty())
        return false;

    if (!hasPlaceholder)
        args.push_back(formattedUrl);

    const bool started = QProcess::startDetached(browserCommand, args);
    if (!started) {
        komai::logging::ui()->warn("Failed to start custom browser command '{}' for URL '{}'",
                                   trimmedCommand.toStdString(),
                                   url.toDisplayString().toStdString());
        return false;
    }

    return true;
}

QVariantMap
toQmlProfileSummaryMap(const profile_manager::ProfileSummary &summary)
{
    QVariantMap item;
    item.insert(QStringLiteral("id"), summary.id);
    item.insert(QStringLiteral("isDefault"), summary.isDefault);
    item.insert(QStringLiteral("isCurrent"), summary.isCurrent);
    item.insert(QStringLiteral("userId"), summary.userId);
    item.insert(QStringLiteral("homeserver"), summary.homeserver);
    item.insert(QStringLiteral("themeSlug"), summary.themeSlug);
    item.insert(QStringLiteral("secretsProvider"), summary.secretsProvider);
    item.insert(QStringLiteral("themeAccentColor"), summary.accentColor.name(QColor::HexArgb));
    item.insert(QStringLiteral("themeWindowColor"), summary.windowColor.name(QColor::HexArgb));
    item.insert(QStringLiteral("themeDarkColor"), summary.darkColor.name(QColor::HexArgb));
    item.insert(QStringLiteral("themeTextColor"), summary.textColor.name(QColor::HexArgb));
    item.insert(QStringLiteral("themeBrightTextColor"),
                summary.brightTextColor.name(QColor::HexArgb));
    return item;
}

uint64_t
directorySizeBytes(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists())
        return 0;
    if (info.isFile())
        return static_cast<uint64_t>(std::max<qint64>(0, info.size()));

    uint64_t total = 0;
    QDirIterator it(path,
                    QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += static_cast<uint64_t>(std::max<qint64>(0, it.fileInfo().size()));
    }

    return total;
}

}

Komai::Komai()
{
    connect(UserSettings::instance().get(),
            &UserSettings::uiThemeSlugChanged,
            this,
            &Komai::colorsChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::uiLayoutDensityChanged,
            this,
            &Komai::layoutMetricsChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::uiFontSizePtChanged,
            this,
            &Komai::layoutMetricsChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::uiFontFamilyChanged,
            this,
            &Komai::layoutMetricsChanged);
    // Properties that return tr()-translated strings (tagline, matrixWord)
    // need their NOTIFY signal to fire on language switch — Qt's
    // qmlEngine->retranslate() refreshes qsTr() bindings but doesn't know
    // about C++ Q_PROPERTYs that happen to return localised text.
    // Queued so the emit lands AFTER MainApplication's same-signal slot
    // has installed the new translators; otherwise the binding re-evaluates
    // against the previous language.
    connect(UserSettings::instance().get(),
            &UserSettings::uiLanguageChanged,
            this,
            &Komai::localizedStringsChanged,
            Qt::QueuedConnection);
    connect(UserSettings::instance().get(),
            &UserSettings::navigationRoomListShowLastMessageTimeChanged,
            this,
            &Komai::navigationRoomListShowLastMessageTimeChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::profileChanged,
            this,
            &Komai::localCacheInfoChanged);

    connect(UserSettings::instance().get(),
            &UserSettings::userIdChanged,
            this,
            &Komai::localCacheInfoChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::sessionAuthStateChanged,
            this,
            &Komai::localCacheInfoChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::sessionAuthStateChanged,
            this,
            &Komai::updateUserProfile);
    connect(ChatPage::instance(), &ChatPage::contentLoaded, this, &Komai::updateUserProfile);
    connect(ChatPage::instance(), &ChatPage::showRoomJoinPrompt, this, &Komai::showRoomJoinPrompt);
    connect(
      ChatPage::instance(), &ChatPage::promptUnlockKeyBackup, this, &Komai::promptUnlockKeyBackup);
    connect(this, &Komai::joinRoom, ChatPage::instance(), &ChatPage::joinRoom);
    if (auto *mainWindow = MainWindow::instance()) {
        connect(mainWindow,
                &MainWindow::openCloseToTrayPromptDialog,
                this,
                &Komai::openCloseToTrayPromptDialog);
    }
}

void
Komai::updateUserProfile()
{
    const auto *mainWindow      = MainWindow::instance();
    const bool hasMatrixRuntime = mainWindow && mainWindow->matrixBackendHandleId() != 0;
    const auto localUserId      = utils::localUser().trimmed();

    if (hasMatrixRuntime && !localUserId.isEmpty()) {
        komai::logging::ui()->info("Refreshing Komai.currentUser (user_id='{}', matrix_runtime={})",
                                   localUserId.toStdString(),
                                   hasMatrixRuntime);
        currentUser_.reset(
          new UserProfile(QLatin1String(""), localUserId, ChatPage::instance()->timelineManager()));
    } else {
        komai::logging::ui()->info("Clearing Komai.currentUser (user_id='{}', matrix_runtime={})",
                                   localUserId.toStdString(),
                                   hasMatrixRuntime);
        currentUser_.reset();
    }
    emit profileChanged();
}

QPalette
Komai::colors() const
{
    return Theme::paletteFromTheme(UserSettings::instance()->uiThemeSlug());
}

QPalette
Komai::inactiveColors() const
{
    auto p = Theme::paletteFromTheme(UserSettings::instance()->uiThemeSlug());
    p.setCurrentColorGroup(QPalette::ColorGroup::Inactive);
    return p;
}

Theme
Komai::theme() const
{
    return Theme(UserSettings::instance()->uiThemeSlug());
}

QColor
Komai::readableAccentTextColor(QColor accentColor, QColor backgroundColor) const
{
    return utils::deriveReadableAccentTextColor(accentColor, backgroundColor);
}

QString
Komai::humanReadableFileSize(qulonglong bytes) const
{
    return utils::humanReadableFileSize(bytes);
}

QString
Komai::fileTypeIconSource(const QString &mimeType) const
{
    return utils::fileTypeIconSource(mimeType);
}

bool
Komai::profileDesktopLaunchersSupported() const
{
    return app_paths::desktop::supportsProfileDesktopEntries();
}

int
Komai::tooltipDelay() const
{
    return 0;
}

UserSettings::Density
Komai::uiLayoutDensity() const
{
    return UserSettings::instance()->uiLayoutDensity();
}

bool
Komai::navigationRoomListShowLastMessageTime() const
{
    return UserSettings::instance()->navigationRoomListShowLastMessageTime();
}

namespace {
double
avatarMultiplierForDensity(UserSettings::Density density)
{
    switch (density) {
    case UserSettings::Density::Dense:
        return 1.0;
    case UserSettings::Density::Compact:
        return 1.7;
    case UserSettings::Density::Spacious:
        break;
    }
    return 2.0;
}
}

double
Komai::sidebarAvatarMultiplier() const
{
    return avatarMultiplierForDensity(uiLayoutDensity());
}

// Resolved pixel size of the application font.
// Uses QFontInfo so the value is always positive even when the app font
// was set via setPointSizeF() (which makes QFont::pixelSize() return -1).
int
Komai::fontPixelSize() const
{
    return QFontInfo(QGuiApplication::font()).pixelSize();
}

// Resolved font family name. Returns the concrete family from the application
// font, which already reflects the user's uiFontFamily setting (or the system
// default when the setting is empty / "default").
QString
Komai::fontFamily() const
{
    return QFontInfo(QGuiApplication::font()).family();
}

// Font-scaled icon size shared across navigation surfaces.
int
Komai::iconSize() const
{
    return iconLogicalSize();
}

int
Komai::iconLogicalSize()
{
    const auto settings = UserSettings::instance();
    if (!settings)
        return 4;

    const double avatarMultiplier = avatarMultiplierForDensity(settings->uiLayoutDensity());
    const QFontMetricsF fm(QGuiApplication::font());
    const int rawSize = qMax(1, qCeil(fm.lineSpacing() * avatarMultiplier));
    // Round up to the nearest multiple of 4 so the value multiplies cleanly
    // by common DPR values (1.5 → ×4=integer, 2 → ×4=integer, 3 → same).
    // Also keeps avatar circles symmetric (divisible by 2).
    if (rawSize <= 1)
        return 4;
    return (rawSize + 3) & ~3;
}

// Single authoritative physical avatar thumbnail size.  Computed from font
// metrics + screen DPR, callable without a Komai instance.
// Used by MxcImageProvider and LitehtmlContainer to ensure all avatar
// requests produce the same cache key regardless of per-surface DPR.
int
Komai::avatarThumbnailPhysicalSize()
{
    const int logical = iconLogicalSize();
    // Use QScreen::devicePixelRatio (integer DPR, e.g. 2) for crisp HiDPI
    // rendering.  This is a stable value that doesn't vary per-surface.
    double dpr = 1.0;
    for (const auto *s : QGuiApplication::screens())
        dpr = qMax(dpr, s->devicePixelRatio());
    return qMax(1, qRound(logical * dpr));
}

// Shared baseline used to keep room-list and communities rows aligned with
// adjacent bars (for example the top bar, room actions bar, and status banners).
// Dense renders as a single line with inline preview; compact keeps two
// shrunk lines in a 1.7-lineHeight slot; spacious keeps two full-size lines.
int
Komai::navigationRowHeight() const
{
    QFontMetricsF fm(QGuiApplication::font());
    const int lineHeight = qMax(1, qCeil(fm.lineSpacing()));

    int textSlot;
    int vertPad;
    switch (uiLayoutDensity()) {
    case UserSettings::Density::Dense:
        textSlot = lineHeight;
        vertPad  = paddingSmall();
        break;
    case UserSettings::Density::Compact:
        textSlot = qCeil(1.7 * lineHeight);
        vertPad  = paddingSmall();
        break;
    case UserSettings::Density::Spacious:
    default:
        textSlot = 2 * lineHeight + paddingSmall();
        vertPad  = paddingMedium();
        break;
    }

    return qMax(iconSize(), textSlot) + 2 * vertPad;
}

void
Komai::openLink(QString link) const
{
    QUrl url(link);
    // Open externally if we couldn't handle it internally
    if (!ChatPage::instance()->tryHandleMatrixUri(url)) {
        static const QStringList allowedUrlSchemes = {
          QStringLiteral("http"),
          QStringLiteral("https"),
          QStringLiteral("mailto"),
        };

        if (allowedUrlSchemes.contains(url.scheme()) &&
            !UserSettings::instance()->integrationsBrowserCommand().trimmed().isEmpty() &&
            openWithBrowserCommand(UserSettings::instance()->integrationsBrowserCommand(), url)) {
            return;
        } else if (allowedUrlSchemes.contains(url.scheme()))
            QDesktopServices::openUrl(url);
        else
            komai::logging::ui()->warn(
              "Url '{}' not opened, because the scheme is not in the allow list",
              url.toDisplayString().toStdString());
    }
}
QString
Komai::normalizeEmojiForComparison(const QString &emoji)
{
    return emoji::normalizeForComparison(emoji);
}
QString
Komai::formatHtmlEmojis(const QString &html)
{
    return utils::replaceEmoji(html);
}
QString
Komai::markSearchMatchesInHtml(const QString &html, const QString &query)
{
    if (query.isEmpty() || html.isEmpty())
        return html;

    const auto htmlStd  = html.toStdString();
    const auto queryStd = query.toStdString();
    return QString::fromStdString(std::string(komai::rust::html_mark_search_matches(
      ::rust::Str(htmlStd.data(), htmlStd.size()), ::rust::Str(queryStd.data(), queryStd.size()))));
}

bool
Komai::isEmoticonShortcut(const QString &text)
{
    return emoji::isEmoticonShortcut(text);
}

QString
Komai::emoticonReplacementFor(const QString &token)
{
    return emoji::replaceLeadingEmoticon(token);
}

int
Komai::previousGraphemeBoundary(const QString &text, int position)
{
    if (position <= 0 || text.isEmpty())
        return 0;
    if (position > text.length())
        position = text.length();
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    finder.setPosition(position);
    const int prev = finder.toPreviousBoundary();
    return prev < 0 ? 0 : prev;
}

int
Komai::nextGraphemeBoundary(const QString &text, int position)
{
    if (text.isEmpty() || position >= text.length())
        return text.length();
    if (position < 0)
        position = 0;
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    finder.setPosition(position);
    const int next = finder.toNextBoundary();
    return next < 0 ? text.length() : next;
}

bool
Komai::composerTriggerAtWordBoundary(const QString &text, int triggerPos)
{
    if (triggerPos <= 0)
        return true;
    const int utf16Index = std::min(triggerPos, static_cast<int>(text.size()));
    // Convert the UTF-16 prefix to UTF-8 so Rust sees the same code-point
    // span. left() handles a position landing on a low surrogate by
    // dropping the unpaired half — the Rust side then treats the resulting
    // replacement char as a non-word boundary.
    const QByteArray prefixUtf8 = text.left(utf16Index).toUtf8();
    return komai::rust::composer_trigger_at_word_boundary(
      ::rust::Str(prefixUtf8.constData(), static_cast<size_t>(prefixUtf8.size())),
      static_cast<size_t>(prefixUtf8.size()));
}

QVariantList
Komai::composerExtractMentions(const QString &text)
{
    QVariantList out;
    const QByteArray utf8 = text.toUtf8();
    const ::rust::Str rustText(utf8.constData(), static_cast<size_t>(utf8.size()));
    for (const auto &match : komai::rust::composer_extract_mentions(rustText)) {
        QVariantMap entry;
        entry.insert(
          QStringLiteral("userId"),
          QString::fromUtf8(match.user_id.data(), static_cast<qsizetype>(match.user_id.size())));
        entry.insert(
          QStringLiteral("source"),
          QString::fromUtf8(match.source.data(), static_cast<qsizetype>(match.source.size())));
        out.push_back(entry);
    }
    return out;
}

QVariantMap
Komai::composerApplyFormat(const QString &text,
                           int selectionStart,
                           int selectionEnd,
                           ComposerFormatKind kind)
{
    QVariantMap result;
    result.insert(QStringLiteral("applied"), false);

    const int maxIndex = static_cast<int>(text.size());
    int selStart       = std::clamp(selectionStart, 0, maxIndex);
    int selEnd         = std::clamp(selectionEnd, 0, maxIndex);
    if (selStart > selEnd)
        std::swap(selStart, selEnd);

    const QByteArray utf8 = text.toUtf8();
    const ::rust::Str rustText(utf8.constData(), static_cast<size_t>(utf8.size()));
    const std::uint32_t selStartU = static_cast<std::uint32_t>(selStart);
    const std::uint32_t selEndU   = static_cast<std::uint32_t>(selEnd);

    komai::rust::ComposerTransformResult rustResult;
    switch (kind) {
    case ComposerFormatKind::Bold: {
        const ::rust::Str marker("**", 2);
        rustResult = komai::rust::composer_toggle_inline_wrap(rustText, selStartU, selEndU, marker);
        break;
    }
    case ComposerFormatKind::Italic: {
        const ::rust::Str marker("*", 1);
        rustResult = komai::rust::composer_toggle_inline_wrap(rustText, selStartU, selEndU, marker);
        break;
    }
    case ComposerFormatKind::InlineCode: {
        rustResult = komai::rust::composer_toggle_code(rustText, selStartU, selEndU);
        break;
    }
    case ComposerFormatKind::Quote: {
        const ::rust::Str prefix("> ", 2);
        rustResult =
          komai::rust::composer_toggle_block_prefix(rustText, selStartU, selEndU, prefix);
        break;
    }
    case ComposerFormatKind::Link: {
        rustResult = komai::rust::composer_toggle_link(rustText, selStartU, selEndU);
        break;
    }
    }

    result.insert(QStringLiteral("applied"), rustResult.applied);
    if (!rustResult.applied)
        return result;

    const QString replacement =
      QString::fromUtf8(rustResult.replacement_text.data(),
                        static_cast<qsizetype>(rustResult.replacement_text.size()));
    result.insert(QStringLiteral("replaceStart"), static_cast<int>(rustResult.replace_start_utf16));
    result.insert(QStringLiteral("replaceEnd"), static_cast<int>(rustResult.replace_end_utf16));
    result.insert(QStringLiteral("replacement"), replacement);
    result.insert(QStringLiteral("selectionStart"),
                  static_cast<int>(rustResult.new_sel_start_utf16));
    result.insert(QStringLiteral("selectionEnd"), static_cast<int>(rustResult.new_sel_end_utf16));
    return result;
}

void
Komai::composerReplaceRange(QQuickTextDocument *quickTextDocument,
                            int rangeStart,
                            int rangeEnd,
                            const QString &replacement)
{
    if (!quickTextDocument)
        return;
    QTextDocument *doc = quickTextDocument->textDocument();
    if (!doc)
        return;

    int start = std::max(0, rangeStart);
    int end   = std::max(start, rangeEnd);

    // QTextCursor::insertText on a non-collapsed selection wraps its
    // remove+insert pair in QTextDocument::beginEditBlock/endEditBlock
    // internally, so the entire replacement registers as one undo step and
    // prior typing history is preserved.
    QTextCursor cursor(doc);
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.insertText(replacement);
}

QString
Komai::punyLink(QString link) const
{
    QUrl url(link);
    return url.toDisplayString(QUrl::FullyEncoded);
}

QString
Komai::statusMessage() const
{
    return ChatPage::instance()->status();
}

void
Komai::setStatusMessage(QString msg) const
{
    ChatPage::instance()->setStatus(msg);
}

void
Komai::refreshApplicationProfiles()
{
    QVariantList summaries;
    const auto currentProfile = UserSettings::instance()->profile();

    for (const auto &summary : profile_manager::listProfiles(currentProfile))
        summaries.push_back(toQmlProfileSummaryMap(summary));

    if (summaries == applicationProfiles_)
        return;

    applicationProfiles_ = summaries;
    emit applicationProfilesChanged();
}

QString
Komai::validateApplicationProfileId(QString profileId) const
{
    if (const auto validationError = profile_manager::validateNewProfileId(profileId.trimmed());
        validationError) {
        return *validationError;
    }
    return {};
}

QString
Komai::createAndLaunchApplicationProfile(QString profileId, bool createDesktopLauncher) const
{
    const auto trimmedProfile = profileId.trimmed();
    if (const auto validationError = profile_manager::validateNewProfileId(trimmedProfile);
        validationError) {
        return *validationError;
    }

    QString error;
    if (createDesktopLauncher &&
        !profile_manager::ensureProfileDesktopLauncher(trimmedProfile, &error)) {
        return error;
    }

    if (!profile_manager::launchProfileDetached(trimmedProfile, &error))
        return error;

    return {};
}

QString
Komai::launchApplicationProfile(QString profileId) const
{
    const auto trimmedProfile = profileId.trimmed();
    if (trimmedProfile.isEmpty())
        return tr("Profile name is required.");

    QString error;
    if (!profile_manager::launchProfileDetached(trimmedProfile, &error))
        return error;

    return {};
}

QString
Komai::launchProfileSwitcher() const
{
    QString error;
    if (!profile_manager::launchStartupSelectorDetached(&error))
        return error;

    return {};
}

QVariantMap
Komai::localCacheInfo() const
{
    QVariantMap info;

    const auto *settings   = UserSettings::instance().get();
    const auto profileId   = settings->profile();
    const auto userId      = settings->userId().trimmed();
    const bool hasUserId   = !userId.isEmpty();
    const auto matrixPaths = komai::MatrixSdkPathsProvider::forProfile(profileId);

    const QString stateStorePath     = matrixPaths.stateStoreRoot;
    const QString matrixSdkCachePath = matrixPaths.cacheRoot;
    const QString mediaCachePath     = app_paths::cache::mediaRoot(profileId);

    const QFileInfo stateStoreInfo(stateStorePath);
    const QFileInfo matrixSdkCacheInfo(matrixSdkCachePath);
    const QFileInfo mediaInfo(mediaCachePath);

    info.insert(QStringLiteral("profileId"), profileId);
    info.insert(QStringLiteral("hasUserId"), hasUserId);
    info.insert(QStringLiteral("stateStorePath"), stateStorePath);
    info.insert(QStringLiteral("stateStorePathExists"), stateStoreInfo.exists());
    info.insert(QStringLiteral("matrixSdkCachePath"), matrixSdkCachePath);
    info.insert(QStringLiteral("matrixSdkCachePathExists"), matrixSdkCacheInfo.exists());
    info.insert(QStringLiteral("mediaCachePath"), mediaCachePath);
    info.insert(QStringLiteral("mediaCachePathExists"), mediaInfo.exists());

    const auto stateStoreSizeBytes     = directorySizeBytes(stateStorePath);
    const auto matrixSdkCacheSizeBytes = directorySizeBytes(matrixSdkCachePath);
    const auto mediaSizeBytes          = directorySizeBytes(mediaCachePath);
    info.insert(QStringLiteral("stateStoreSizeBytes"),
                static_cast<qulonglong>(stateStoreSizeBytes));
    info.insert(QStringLiteral("stateStoreSizeHuman"),
                utils::humanReadableFileSize(stateStoreSizeBytes));
    info.insert(QStringLiteral("matrixSdkCacheSizeBytes"),
                static_cast<qulonglong>(matrixSdkCacheSizeBytes));
    info.insert(QStringLiteral("matrixSdkCacheSizeHuman"),
                utils::humanReadableFileSize(matrixSdkCacheSizeBytes));
    info.insert(QStringLiteral("mediaCacheSizeBytes"), static_cast<qulonglong>(mediaSizeBytes));
    info.insert(QStringLiteral("mediaCacheSizeHuman"),
                utils::humanReadableFileSize(mediaSizeBytes));

    info.insert(QStringLiteral("backend"), QStringLiteral("Matrix SDK (SQLite)"));

    if (!hasUserId) {
        info.insert(QStringLiteral("statusKind"), QStringLiteral("unavailable"));
        info.insert(QStringLiteral("statusLabel"), tr("Not signed in"));
        info.insert(QStringLiteral("statusDetails"), tr("Sign in to start syncing this profile."));
        return info;
    }

    if (!stateStoreInfo.exists()) {
        info.insert(QStringLiteral("statusKind"), QStringLiteral("empty"));
        info.insert(QStringLiteral("statusLabel"), tr("Not synced"));
        info.insert(QStringLiteral("statusDetails"), tr("No matrix-sdk state store yet."));
        return info;
    }

    info.insert(QStringLiteral("statusKind"), QStringLiteral("ready"));
    info.insert(QStringLiteral("statusLabel"), tr("Ready"));
    info.insert(QStringLiteral("statusDetails"), QString{});

    return info;
}

bool
Komai::openLocalPath(QString path) const
{
    const auto trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty())
        return false;

    return QDesktopServices::openUrl(QUrl::fromLocalFile(trimmedPath));
}

int
Komai::mediaPurgeAgeDays() const
{
    return app_paths::cache::mediaPurgeAgeDays;
}

QString
Komai::purgeMediaCache()
{
    const QString mediaCachePath = app_paths::cache::mediaRoot(UserSettings::instance()->profile());
    const QFileInfo info(mediaCachePath);

    if (info.exists() && !QDir(mediaCachePath).removeRecursively())
        return tr("Could not remove some files.");

    if (!QDir().mkpath(mediaCachePath))
        return tr("Could not recreate the cache folder.");

    emit localCacheInfoChanged();
    return {};
}

QString
Komai::deleteApplicationProfile(QString profileId, bool allowDeletingLoadedProfile)
{
    const auto trimmedProfile = profileId.trimmed();
    if (trimmedProfile.isEmpty())
        return tr("Profile name is required.");

    QString error;
    if (!profile_manager::deleteProfile(trimmedProfile,
                                        UserSettings::instance()->profile(),
                                        &error,
                                        !allowDeletingLoadedProfile)) {
        return error;
    }

    refreshApplicationProfiles();
    return {};
}

UserProfile *
Komai::currentUser() const
{
    komai::logging::ui()->debug("Profile requested");

    return currentUser_.get();
}

void
Komai::showUserSettingsPage() const
{
    if (auto *window = MainWindow::instance())
        window->showUserSettingsPage();
}

void
Komai::logout() const
{
    ChatPage::instance()->initiateLogout();
}

void
Komai::acceptCloseToTrayAsQuit()
{
    if (auto settings = UserSettings::instance(); settings)
        settings->setDesktopSystemTrayFirstClosePrompted(true);
    QCoreApplication::quit();
}

void
Komai::acceptCloseToTrayAsTray()
{
    auto settings = UserSettings::instance();
    if (!settings)
        return;

    settings->setDesktopSystemTrayEnabled(true);
    settings->setDesktopSystemTrayFirstClosePrompted(true);
    if (auto *mainWindow = MainWindow::instance())
        mainWindow->hide();
}

void
Komai::submitUnlockKeyBackup(QString keyOrPassphrase) const
{
    ChatPage::instance()->submitSecretUnlockInput(keyOrPassphrase);
}

void
Komai::cancelUnlockKeyBackup() const
{
    ChatPage::instance()->cancelSecretUnlockInput();
}

void
Komai::setTransientParent(QWindow *window, QWindow *parentWindow) const
{
    if (window)
        window->setTransientParent(parentWindow);
}

void
Komai::createRoom(bool space,
                  const QString &name,
                  const QString &topic,
                  const QString &aliasLocalpart,
                  bool isEncrypted,
                  int preset)
{
    komai::MatrixCreateRoomRequest request;
    request.isSpace = space;

    switch (preset) {
    case 1:
        request.preset = komai::MatrixCreateRoomPreset::PublicChat;
        break;
    case 2:
        request.preset = komai::MatrixCreateRoomPreset::TrustedPrivateChat;
        break;
    case 0:
    default:
        request.preset = komai::MatrixCreateRoomPreset::PrivateChat;
    }

    request.name               = name;
    request.topic              = topic;
    request.roomAliasLocalpart = aliasLocalpart;
    request.isEncrypted        = isEncrypted;
    request.isPublic           = request.preset == komai::MatrixCreateRoomPreset::PublicChat;

    emit ChatPage::instance()->createRoom(request);
}

void
Komai::setWindowRole([[maybe_unused]] QWindow *win, [[maybe_unused]] QString newRole) const
{
#if XCB_AVAILABLE && QT_CONFIG(xcb)
    const QNativeInterface::QX11Application *x11Interface =
      qGuiApp->nativeInterface<QNativeInterface::QX11Application>();

    if (!x11Interface)
        return;

    auto connection = x11Interface->connection();

    auto role = newRole.toStdString();

    char WM_WINDOW_ROLE[] = "WM_WINDOW_ROLE";
    auto cookie = xcb_intern_atom(connection, false, std::size(WM_WINDOW_ROLE) - 1, WM_WINDOW_ROLE);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, nullptr);
    auto atom                      = reply->atom;
    free(reply);

    xcb_change_property(connection,
                        XCB_PROP_MODE_REPLACE,
                        win->winId(),
                        atom,
                        XCB_ATOM_STRING,
                        8,
                        role.size(),
                        role.data());
#endif
}

#include "moc_KomaiGlobalObject.cpp"

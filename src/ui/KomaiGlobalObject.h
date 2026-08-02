// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFontDatabase>
#include <QObject>
#include <QPalette>
#include <QQmlEngine>
#include <QVariantList>
#include <QWindow>

class QQuickTextDocument;

#include "RoomSummary.h"
#include "Theme.h"
#include "UserProfile.h"
#include "models/AliasEditModel.h"
#include "powerlevels/PowerlevelsEditModels.h"
#include "settings/ui/facade/UserSettingsPage.h"

class Komai : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QPalette colors READ colors NOTIFY colorsChanged)
    Q_PROPERTY(QPalette inactiveColors READ inactiveColors NOTIFY colorsChanged)
    Q_PROPERTY(Theme theme READ theme NOTIFY colorsChanged)
    Q_PROPERTY(int mediaPurgeAgeDays READ mediaPurgeAgeDays CONSTANT)
    Q_PROPERTY(int paddingSmall READ paddingSmall CONSTANT)
    Q_PROPERTY(int paddingMedium READ paddingMedium CONSTANT)
    Q_PROPERTY(int paddingLarge READ paddingLarge CONSTANT)
    // Width threshold (in px) below which Settings rows should switch from a
    // side-by-side label/control layout to a stacked one. Centralized so all
    // settings surfaces (the generic Settings rows, the Account device cards,
    // etc.) react at the same point.
    Q_PROPERTY(int settingRowStackBreakpoint READ settingRowStackBreakpoint CONSTANT)
    // Vertical padding inside the composer's multi-line text area. Shared so
    // that composer action buttons can match the textarea's single-line height.
    Q_PROPERTY(int composerTextAreaPadding READ composerTextAreaPadding CONSTANT)
    Q_PROPERTY(int tooltipDelay READ tooltipDelay CONSTANT)
    // Size of the Komai logo shown in the main timeline empty state
    // and the initial sync spinner.
    Q_PROPERTY(int timelineLogoSize READ timelineLogoSize CONSTANT)
    Q_PROPERTY(UserSettings::Density density READ uiLayoutDensity NOTIFY layoutMetricsChanged)
    Q_PROPERTY(
      double sidebarAvatarMultiplier READ sidebarAvatarMultiplier NOTIFY layoutMetricsChanged)
    // Font-scaled icon size shared across all navigation surfaces
    // (list rows, community entries, action bars, avatars).
    Q_PROPERTY(int iconSize READ iconSize NOTIFY layoutMetricsChanged)
    // Shared row height baseline used by navigation surfaces (room/community rows and bars).
    Q_PROPERTY(int navigationRowHeight READ navigationRowHeight NOTIFY layoutMetricsChanged)
    // Resolved pixel size of the application font (always positive, unlike
    // Qt.application.font.pixelSize)
    Q_PROPERTY(int fontPixelSize READ fontPixelSize NOTIFY layoutMetricsChanged)
    // Resolved font family name (never empty, never "default")
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY layoutMetricsChanged)
    Q_PROPERTY(bool navigationRoomListShowLastMessageTime READ navigationRoomListShowLastMessageTime
                 NOTIFY navigationRoomListShowLastMessageTimeChanged)
    // Maximum file size in bytes for a video to be considered a GIF-like video (1 MB).
    Q_PROPERTY(int gifVideoMaxSizeBytes READ gifVideoMaxSizeBytes CONSTANT)
    // Maximum duration in milliseconds for a video to be considered a GIF-like video (3 seconds).
    Q_PROPERTY(int gifVideoMaxDurationMs READ gifVideoMaxDurationMs CONSTANT)

    // Translated strings need a NOTIFY for QML bindings to refresh when the
    // language is switched at runtime. CONSTANT would mean "never re-evaluate".
    Q_PROPERTY(QString tagline READ tagline NOTIFY localizedStringsChanged)
    Q_PROPERTY(QString taglineTemplate READ taglineTemplate NOTIFY localizedStringsChanged)
    Q_PROPERTY(QString matrixWord READ matrixWord NOTIFY localizedStringsChanged)
    Q_PROPERTY(bool profileDesktopLaunchersSupported READ profileDesktopLaunchersSupported CONSTANT)

    Q_PROPERTY(UserProfile *currentUser READ currentUser NOTIFY profileChanged)
    Q_PROPERTY(
      QVariantList applicationProfiles READ applicationProfiles NOTIFY applicationProfilesChanged)

public:
    Komai();

    QPalette colors() const;
    QPalette inactiveColors() const;
    Theme theme() const;

    int mediaPurgeAgeDays() const;

    int paddingSmall() const { return 4; }
    int paddingMedium() const { return 8; }
    int paddingLarge() const { return 20; }
    int settingRowStackBreakpoint() const { return 900; }
    int composerTextAreaPadding() const { return 6; }

    int tooltipDelay() const;
    int timelineLogoSize() const { return 128; }
    int gifVideoMaxSizeBytes() const { return kGifVideoMaxSizeBytes; }
    int gifVideoMaxDurationMs() const { return kGifVideoMaxDurationMs; }

    static constexpr int kGifVideoMaxSizeBytes  = 1048576; // 1 MB
    static constexpr int kGifVideoMaxDurationMs = 3000;    // 3 seconds

    UserSettings::Density uiLayoutDensity() const;
    double sidebarAvatarMultiplier() const;
    int iconSize() const;
    // Single authoritative logical list/avatar size. Use this from C++ code
    // that needs the same sizing as the QML Komai.iconSize property.
    static int iconLogicalSize();
    // Single authoritative physical avatar thumbnail size for cache/download.
    // Computed from font metrics + QScreen DPR. Used by MxcImageProvider and
    // LitehtmlContainer to ensure all avatar requests produce the same cache key.
    static int avatarThumbnailPhysicalSize();
    int navigationRowHeight() const;
    int fontPixelSize() const;
    QString fontFamily() const;
    bool navigationRoomListShowLastMessageTime() const;

    QString taglineTemplate() const { return tr("A fine %1 chat app you can get to love"); }
    QString matrixWord() const { return tr("Matrix"); }
    QString tagline() const { return taglineTemplate().arg(matrixWord()); }
    bool profileDesktopLaunchersSupported() const;

    UserProfile *currentUser() const;
    QVariantList applicationProfiles() const { return applicationProfiles_; }

    Q_INVOKABLE QFont monospaceFont() const
    {
        return QFontDatabase::systemFont(QFontDatabase::FixedFont);
    }
    Q_INVOKABLE static QString normalizeEmojiForComparison(const QString &emoji);
    Q_INVOKABLE static QString formatHtmlEmojis(const QString &html);
    //! Wrap each occurrence of `query` inside the visible text of `html` with
    //! `<mark>…</mark>`, leaving tag/attribute bytes untouched. When an
    //! `<a href>` target contains the query but the visible link text doesn't,
    //! the entire link text is wrapped so href-only matches stay visible.
    //! Returns `html` unchanged when `query` is empty.
    Q_INVOKABLE static QString markSearchMatchesInHtml(const QString &html, const QString &query);
    //! True when `text` is exactly a known text emoticon shortcut (`:)`, `:D`,
    //! `:'(`, etc.). The composer uses this to dismiss the inline emoji picker
    //! once the user has typed a complete emoticon that auto-conversion will
    //! turn into an emoji on send.
    Q_INVOKABLE static bool isEmoticonShortcut(const QString &text);
    //! If `token` begins with a known text emoticon shortcut (case-
    //! insensitive), returns `token` with that leading shortcut replaced by
    //! its emoji, preserving any trailing characters (e.g. `:)?` -> `🙂?`).
    //! Returns an empty string if no shortcut prefixes `token`, or if the
    //! character right after the shortcut is a letter/digit (so `:Dog` is
    //! left alone). Used by the composer to live-convert a shortcut to its
    //! emoji right after the user types the space that completes it.
    Q_INVOKABLE static QString emoticonReplacementFor(const QString &token);
    //! Walk grapheme cluster boundaries in `text`. Used by the composer so
    //! Backspace/Delete remove a whole cluster (e.g. base + VS16, ZWJ
    //! sequence, or skin-tone modifier) rather than a single UTF-16 code unit.
    Q_INVOKABLE static int previousGraphemeBoundary(const QString &text, int position);
    Q_INVOKABLE static int nextGraphemeBoundary(const QString &text, int position);
    //! Used by the composer to decide whether a freshly-typed `:`, `@`, `#`,
    //! or `~` at `triggerPos` sits at a word boundary and should open the
    //! inline picker. Returns false (mid-word) when the preceding code-point
    //! is a Unicode letter, digit, or underscore — e.g. inside an email
    //! address `user@example.com` or shortcut `test:value`. Anything else
    //! (whitespace, punctuation, emoji, start of input) is a boundary.
    Q_INVOKABLE static bool composerTriggerAtWordBoundary(const QString &text, int triggerPos);

    //! Extract intentional user mentions (MSC3952) from composer draft text.
    //! Returns a list of `{ userId, source }` maps, where `source` is the link
    //! substring as it appears in the text (used by the composer to prune a
    //! mention when its text is edited away). Parsing is delegated to ruma; see
    //! the composer_mentions module in the Rust crate.
    Q_INVOKABLE static QVariantList composerExtractMentions(const QString &text);

    enum class ComposerFormatKind
    {
        Bold       = 0,
        Italic     = 1,
        InlineCode = 2,
        Quote      = 3,
        Link       = 4,
    };
    Q_ENUM(ComposerFormatKind)

    //! Compute a composer formatting toggle (bold / italic / code / quote /
    //! link). Returns a QVariantMap with keys `applied` (bool), `replaceStart`
    //! / `replaceEnd` (UTF-16 indices, the range to replace in the current
    //! text) and `replacement` (the replacement string), plus
    //! `selectionStart` / `selectionEnd` (UTF-16 indices into the NEW text
    //! after the replacement). Pure: does not mutate any shared state. The
    //! caller applies the replacement via `composerReplaceRange` and then
    //! `messageInput.select(...)`.
    Q_INVOKABLE static QVariantMap composerApplyFormat(const QString &text,
                                                       int selectionStart,
                                                       int selectionEnd,
                                                       ComposerFormatKind kind);

    //! Replace `[rangeStart, rangeEnd)` (UTF-16 indices) in the
    //! `QQuickTextDocument`'s underlying `QTextDocument` with `replacement`
    //! atomically — one undo step, preserving the prior undo history. The
    //! composer relies on this for all formatting toggles so Ctrl+Z restores
    //! the pre-toggle text in a single press.
    Q_INVOKABLE static void composerReplaceRange(QQuickTextDocument *quickTextDocument,
                                                 int rangeStart,
                                                 int rangeEnd,
                                                 const QString &replacement);
    Q_INVOKABLE QColor readableAccentTextColor(QColor accentColor, QColor backgroundColor) const;
    Q_INVOKABLE QString humanReadableFileSize(qulonglong bytes) const;
    Q_INVOKABLE QString fileTypeIconSource(const QString &mimeType) const;
    Q_INVOKABLE void openLink(QString link) const;
    Q_INVOKABLE QString punyLink(QString link) const;
    Q_INVOKABLE QString statusMessage() const;
    Q_INVOKABLE void setStatusMessage(QString msg) const;
    Q_INVOKABLE void refreshApplicationProfiles();
    Q_INVOKABLE QString validateApplicationProfileId(QString profileId) const;
    Q_INVOKABLE QString createAndLaunchApplicationProfile(QString profileId,
                                                          bool createDesktopLauncher = false) const;
    Q_INVOKABLE QString launchApplicationProfile(QString profileId) const;
    Q_INVOKABLE QString launchProfileSwitcher() const;
    Q_INVOKABLE QString deleteApplicationProfile(QString profileId,
                                                 bool allowDeletingLoadedProfile = false);
    Q_INVOKABLE QVariantMap localCacheInfo() const;
    Q_INVOKABLE bool openLocalPath(QString path) const;
    Q_INVOKABLE QString purgeMediaCache();
    Q_INVOKABLE void showUserSettingsPage() const;
    Q_INVOKABLE void logout() const;
    //! Chosen from the "close to tray" prompt: persist the choice and quit.
    Q_INVOKABLE void acceptCloseToTrayAsQuit();
    //! Chosen from the "close to tray" prompt: persist the choice, enable
    //! close-to-tray, and hide the main window.
    Q_INVOKABLE void acceptCloseToTrayAsTray();
    Q_INVOKABLE void submitUnlockKeyBackup(QString keyOrPassphrase) const;
    Q_INVOKABLE void cancelUnlockKeyBackup() const;
    Q_INVOKABLE void createRoom(bool space,
                                const QString &name,
                                const QString &topic,
                                const QString &aliasLocalpart,
                                bool isEncrypted,
                                int preset);
    Q_INVOKABLE PowerlevelEditingModels *editPowerlevels(QString room_id_) const
    {
        return new PowerlevelEditingModels(room_id_);
    }
    Q_INVOKABLE AliasEditingModel *editAliases(QString room_id_) const
    {
        return new AliasEditingModel(room_id_.toStdString());
    }
    Q_INVOKABLE void setTransientParent(QWindow *window, QWindow *parentWindow) const;
    Q_INVOKABLE void setWindowRole(QWindow *win, QString newRole) const;

public slots:
    void updateUserProfile();

signals:
    void colorsChanged();
    void profileChanged();
    void applicationProfilesChanged();
    void localCacheInfoChanged();
    void layoutMetricsChanged();
    void localizedStringsChanged();
    void navigationRoomListShowLastMessageTimeChanged();

    void openLogoutDialog();
    void openJoinRoomDialog();
    void openCloseToTrayPromptDialog();
    void joinRoom(QString roomId, QString reason = "");
    void promptUnlockKeyBackup();

    void showRoomJoinPrompt(RoomSummary *summary);

private:
    QScopedPointer<UserProfile> currentUser_;
    QVariantList applicationProfiles_;
};

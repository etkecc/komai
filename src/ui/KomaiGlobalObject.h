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

#include "RoomSummary.h"
#include "Theme.h"
#include "UserProfile.h"
#include "models/AliasEditModel.h"
#include "powerlevels/PowerlevelsEditModels.h"

class Komai : public QObject
{
    Q_OBJECT

    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QPalette colors READ colors NOTIFY colorsChanged)
    Q_PROPERTY(QPalette inactiveColors READ inactiveColors NOTIFY colorsChanged)
    Q_PROPERTY(Theme theme READ theme NOTIFY colorsChanged)
    Q_PROPERTY(int mediaPurgeAgeDays READ mediaPurgeAgeDays CONSTANT)
    Q_PROPERTY(int paddingSmall READ paddingSmall CONSTANT)
    Q_PROPERTY(int paddingMedium READ paddingMedium CONSTANT)
    Q_PROPERTY(int paddingLarge READ paddingLarge CONSTANT)
    Q_PROPERTY(int tooltipDelay READ tooltipDelay CONSTANT)
    // Size of the Komai logo shown in the main timeline empty state
    // and the initial sync spinner.
    Q_PROPERTY(int timelineLogoSize READ timelineLogoSize CONSTANT)
    Q_PROPERTY(bool uiLayoutCompactMode READ uiLayoutCompactMode NOTIFY layoutMetricsChanged)
    Q_PROPERTY(
      double sidebarAvatarMultiplier READ sidebarAvatarMultiplier NOTIFY layoutMetricsChanged)
    // Font-scaled icon size for list entries (room list rows, community entries)
    Q_PROPERTY(int listIconSize READ listIconSize NOTIFY layoutMetricsChanged)
    // Shared row height baseline used by navigation surfaces (room/community rows and bars).
    Q_PROPERTY(int navigationRowHeight READ navigationRowHeight NOTIFY layoutMetricsChanged)
    // Icon size for action bars (top bar, room list actions bar)
    Q_PROPERTY(int barIconSize READ barIconSize NOTIFY layoutMetricsChanged)
    // Resolved pixel size of the application font (always positive, unlike
    // Qt.application.font.pixelSize)
    Q_PROPERTY(int fontPixelSize READ fontPixelSize NOTIFY layoutMetricsChanged)
    // Resolved font family name (never empty, never "default")
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY layoutMetricsChanged)
    Q_PROPERTY(bool sidebarsRoomListShowLastMessageTime READ sidebarsRoomListShowLastMessageTime
                 NOTIFY sidebarsRoomListShowLastMessageTimeChanged)
    // Maximum file size in bytes for a video to be considered a GIF-like video (1 MB).
    Q_PROPERTY(int gifVideoMaxSizeBytes READ gifVideoMaxSizeBytes CONSTANT)
    // Maximum duration in milliseconds for a video to be considered a GIF-like video (3 seconds).
    Q_PROPERTY(int gifVideoMaxDurationMs READ gifVideoMaxDurationMs CONSTANT)

    Q_PROPERTY(QString tagline READ tagline CONSTANT)
    Q_PROPERTY(QString taglineTemplate READ taglineTemplate CONSTANT)
    Q_PROPERTY(QString matrixWord READ matrixWord CONSTANT)

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

    int tooltipDelay() const;
    int timelineLogoSize() const { return 128; }
    int gifVideoMaxSizeBytes() const { return kGifVideoMaxSizeBytes; }
    int gifVideoMaxDurationMs() const { return kGifVideoMaxDurationMs; }

    static constexpr int kGifVideoMaxSizeBytes  = 1048576; // 1 MB
    static constexpr int kGifVideoMaxDurationMs = 3000;    // 3 seconds

    bool uiLayoutCompactMode() const;
    double sidebarAvatarMultiplier() const;
    int listIconSize() const;
    // Single authoritative logical list/avatar size. Use this from C++ code
    // that needs the same sizing as the QML Komai.listIconSize property.
    static int listIconLogicalSize();
    // Single authoritative physical avatar thumbnail size for cache/download.
    // Computed from font metrics + QScreen DPR. Used by MxcImageProvider and
    // LitehtmlContainer to ensure all avatar requests produce the same cache key.
    static int avatarThumbnailPhysicalSize();
    int navigationRowHeight() const;
    int barIconSize() const;
    int fontPixelSize() const;
    QString fontFamily() const;
    bool sidebarsRoomListShowLastMessageTime() const;

    QString taglineTemplate() const { return tr("A fine %1 chat app you can get to love"); }
    QString matrixWord() const { return tr("Matrix"); }
    QString tagline() const { return taglineTemplate().arg(matrixWord()); }

    UserProfile *currentUser() const;
    QVariantList applicationProfiles() const { return applicationProfiles_; }

    Q_INVOKABLE QFont monospaceFont() const
    {
        return QFontDatabase::systemFont(QFontDatabase::FixedFont);
    }
    Q_INVOKABLE QColor readableAccentTextColor(QColor accentColor, QColor backgroundColor) const;
    Q_INVOKABLE void openLink(QString link) const;
    Q_INVOKABLE QString punyLink(QString link) const;
    Q_INVOKABLE QString statusMessage() const;
    Q_INVOKABLE void setStatusMessage(QString msg) const;
    Q_INVOKABLE void refreshApplicationProfiles();
    Q_INVOKABLE QString validateApplicationProfileId(QString profileId) const;
    Q_INVOKABLE QString createAndLaunchApplicationProfile(QString profileId) const;
    Q_INVOKABLE QString launchApplicationProfile(QString profileId) const;
    Q_INVOKABLE QString launchProfileSwitcher() const;
    Q_INVOKABLE QString deleteApplicationProfile(QString profileId,
                                                 bool allowDeletingLoadedProfile = false);
    Q_INVOKABLE QVariantMap localCacheInfo() const;
    Q_INVOKABLE bool openLocalPath(QString path) const;
    Q_INVOKABLE QString purgeMediaCache();
    Q_INVOKABLE void showUserSettingsPage() const;
    Q_INVOKABLE void logout() const;
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
    void sidebarsRoomListShowLastMessageTimeChanged();

    void openLogoutDialog();
    void openJoinRoomDialog();
    void joinRoom(QString roomId, QString reason = "");
    void promptUnlockKeyBackup();

    void showRoomJoinPrompt(RoomSummary *summary);

private:
    bool hasPreviewLayout() const;

    QScopedPointer<UserProfile> currentUser_;
    QVariantList applicationProfiles_;
};

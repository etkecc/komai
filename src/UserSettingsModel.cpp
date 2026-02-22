// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMessageBox>
#include <QMetaEnum>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>

#include "Cache.h"
#include "JdenticonProvider.h"
#include "MainWindow.h"
#include "UserSettingsPage.h"
#include "Utils.h"
#include "config/nheko.h"
#include "encryption/Olm.h"
#include "settings/SettingKeys.h"
#include "ui/Theme.h"
#include "ui/ThemeRegistry.h"
#include "voip/CallDevices.h"

QHash<int, QByteArray>
UserSettingsModel::roleNames() const
{
    static QHash<int, QByteArray> roles{
      {Name, "name"},
      {Description, "description"},
      {Value, "value"},
      {Type, "type"},
      {ValueLowerBound, "valueLowerBound"},
      {ValueUpperBound, "valueUpperBound"},
      {ValueStep, "valueStep"},
      {Values, "values"},
      {Good, "good"},
      {Enabled, "enabled"},
      {ThemeVariantValue, "themeVariantValue"},
      {ThemeVariantValues, "themeVariantValues"},
      {SettingImage, "settingImage"},
      {Tab, "tab"},
    };

    return roles;
}

QObject *
UserSettingsModel::modelForTab(int tab) const
{
    auto it = filteredModels_.find(tab);
    if (it != filteredModels_.end())
        return it.value();

    auto *proxyModel = new QSortFilterProxyModel(const_cast<UserSettingsModel *>(this));
    proxyModel->setSourceModel(const_cast<UserSettingsModel *>(this));
    proxyModel->setFilterRole(Tab);
    proxyModel->setFilterRegularExpression(QStringLiteral("^%1$").arg(tab));
    filteredModels_.insert(tab, proxyModel);

    return proxyModel;
}

// ── Metadata table for settings model ──────────────────────────────────────────

namespace {

struct SettingMeta
{
    const char *name;                   // tr() key (nullptr = skip)
    const char *description;            // tr() key (nullptr = no description)
    int type;                           // Types enum
    int tab;                            // SettingsTab enum
    QVariant (*getValue)();             // getter (nullptr for sections)
    bool (*setValue)(const QVariant &); // setter (nullptr for read-only/sections)
    QVariant lowerBound, upperBound, step;
    QVariant (*getValues)(); // for Options type (nullptr if N/A)
    bool (*isEnabled)();     // nullptr = always enabled
};

#define I UserSettings::instance()
#define SM UserSettingsModel

// Helper: convert std::vector<std::string> to QStringList
static QStringList
vecToList(const std::vector<std::string> &vec)
{
    QStringList l;
    for (const auto &d : vec)
        l.push_back(QString::fromStdString(d));
    return l;
}

// clang-format off
static const SettingMeta settingsTable[] = {
    // ── Look & Feel Tab ─────────────────────────────────────────────────────

    // LookFeelThemeSection
    { QT_TR_NOOP("THEME"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Theme
    { QT_TR_NOOP("Theme"), nullptr, SM::ThemeSelector, SM::TabLookFeel,
      []() -> QVariant {
          auto variant = ThemeRegistry::instance().themeVariant(I->theme());
          if (variant == u"system") return -1;
          return ThemeRegistry::instance().themeSlugs(variant).indexOf(I->theme());
      },
      [](const QVariant &v) -> bool {
          auto variant = ThemeRegistry::instance().themeVariant(I->theme());
          if (variant == u"system") return false;
          auto slugs = ThemeRegistry::instance().themeSlugs(variant);
          int idx = v.toInt();
          if (idx >= 0 && idx < slugs.size()) { I->setTheme(slugs.at(idx)); return true; }
          return false;
      },
      {}, {}, {},
      []() -> QVariant {
          auto variant = ThemeRegistry::instance().themeVariant(I->theme());
          if (variant == u"system") return QStringList{};
          return ThemeRegistry::instance().themeNames(variant);
      },
      nullptr },
    // LookFeelFontsSection
    { QT_TR_NOOP("FONTS"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Font
    { QT_TR_NOOP("Font family"), nullptr, SM::Options, SM::TabLookFeel,
      []() -> QVariant {
          if (I->font().isEmpty()) return 0;
          auto fonts = QFontDatabase::families();
          fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System font"));
          return fonts.indexOf(I->font());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto idx = v.toInt();
          I->setFontFamily(idx == 0 ? QString{} : QFontDatabase::families().at(idx - 1));
          return true;
      },
      {}, {}, {},
      []() -> QVariant {
          auto fonts = QFontDatabase::families();
          fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System font"));
          return fonts;
      },
      nullptr },
    // FontSize
    { QT_TR_NOOP("Font size"), nullptr, SM::Double, SM::TabLookFeel,
      []() -> QVariant { return I->fontSize(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<double>())) return false;
          I->setFontSize(v.toDouble()); return true;
      },
      8.0, 24.0, 0.5, nullptr, nullptr },
    // EmojiFont
    { QT_TR_NOOP("Emoji font family"), nullptr, SM::Options, SM::TabLookFeel,
      []() -> QVariant {
          if (I->emojiFontFamily().isEmpty()) return 0;
          auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
          fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System emoji font"));
          return fonts.indexOf(I->emojiFontFamily());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto idx = v.toInt();
          I->setEmojiFontFamily(
            idx <= 0 ? QString()
                     : QFontDatabase::families(QFontDatabase::WritingSystem::Symbol).at(idx - 1));
          return true;
      },
      {}, {}, {},
      []() -> QVariant {
          auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
          fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System emoji font"));
          return fonts;
      },
      nullptr },
#ifndef Q_OS_MACOS
    // ScaleFactor
    { QT_TR_NOOP("Scale factor [restart required]"),
      QT_TR_NOOP("Change the scale factor of the whole user interface. Requires a restart to take effect."),
      SM::Double, SM::TabLookFeel,
      []() -> QVariant { return I->scaleFactor(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<double>())) return false;
          I->setScaleFactor(v.toDouble()); return true;
      },
      1.0, 3.0, .25, nullptr, nullptr },
#endif
    // LookFeelBehaviorSection
    { QT_TR_NOOP("BEHAVIOR"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableUIAnimations
    { QT_TR_NOOP("Enable UI animations"),
      QT_TR_NOOP("Komai uses animations in several places around the interface. Enable or disable them here."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return !I->reducedMotion(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setReducedMotion(!v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // EnableTextSelection
    { QT_TR_NOOP("Enable text selection on timeline"),
      QT_TR_NOOP("Enable text selection in timeline messages. Disable this for a touch-style input "
                 "experience."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return !I->mobileMode(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setMobileMode(!v.toBool());
          return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // EnableSwipeGestures
    { QT_TR_NOOP("Enable swipe gestures"),
      QT_TR_NOOP("Enable swipe gestures like swiping left/right between Rooms and Timeline, or swiping a message to reply."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->enableSwipeGestures(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setEnableSwipeGestures(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // LookFeelRoomListSection
    { QT_TR_NOOP("ROOM LIST"), nullptr, SM::SectionTitle, SM::TabSidebars,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // CompactRoomList
    { QT_TR_NOOP("Compact mode"),
      QT_TR_NOOP("Use smaller avatars and tighter spacing in the room list and communities sidebar."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->compactRoomList(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setCompactRoomList(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowRoomListTime
    { QT_TR_NOOP("Show last message timestamp"),
      QT_TR_NOOP("Show the timestamp of the last message next to the room name."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->showRoomListTime(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowRoomListTime(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowLastMessagePreview
    { QT_TR_NOOP("Show last message preview"),
      QT_TR_NOOP("Show a preview of the most recent message in each room."),
      SM::Options, SM::TabSidebars,
      []() -> QVariant { return static_cast<int>(I->showLastMessagePreview()); },
      [](const QVariant &v) -> bool {
          auto val = v.toInt();
          if (val < 0 || QMetaEnum::fromType<UserSettings::LastMessagePreview>().keyCount() <= val) return false;
          I->setShowLastMessagePreview(static_cast<UserSettings::LastMessagePreview>(val)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Always"),
            QCoreApplication::translate("UserSettingsModel", "Only in unencrypted rooms"),
            QCoreApplication::translate("UserSettingsModel", "Never"),
          };
      },
      nullptr },
    // ShowCommunityNotificationCounts
    { QT_TR_NOOP("Show notification counts"),
      QT_TR_NOOP("Show total notification counts for communities and tags."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->showCommunityNotificationCounts(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowCommunityNotificationCounts(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // UseCircularAvatars
    { QT_TR_NOOP("Use circular avatars"),
      QT_TR_NOOP("Change the appearance of user avatars in chats.\nOFF - square, ON - circle."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->useCircularAvatars(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUseCircularAvatars(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // UseIdenticon
    { QT_TR_NOOP("Use identicons"),
      QT_TR_NOOP("Display an identicon instead of a letter when no avatar is set."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->useIdenticon(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUseIdenticon(v.toBool()); return true;
      },
      {}, {}, {}, nullptr,
      []() -> bool { return JdenticonProvider::isAvailable(); } },
    // ScrollbarsInRoomlist
    { QT_TR_NOOP("Show scrollbars"),
      QT_TR_NOOP("Show scrollbars in the room list and communities sidebar."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->scrollbarsInRoomlist(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setScrollbarsInRoomlist(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // RoomSorting
    { QT_TR_NOOP("Sorting"),
      QT_TR_NOOP("How to order rooms."),
      SM::Options, SM::TabSidebars,
      []() -> QVariant { return static_cast<int>(I->roomSortOrder()); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          I->setRoomSortOrder(static_cast<UserSettings::RoomSortOrder>(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Unread first, then recent"),
            QCoreApplication::translate("UserSettingsModel", "Unread first, then A-Z"),
            QCoreApplication::translate("UserSettingsModel", "Recent activity"),
            QCoreApplication::translate("UserSettingsModel", "Alphabetical"),
          };
      },
      nullptr },
    // LookFeelCommunitiesSidebarSection
    { QT_TR_NOOP("COMMUNITIES SIDEBAR"), nullptr, SM::SectionTitle, SM::TabSidebars,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ShowCommunitiesSidebar
    { QT_TR_NOOP("Show communities sidebar"),
      QT_TR_NOOP("Show a column containing communities and tags."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->showCommunitiesSidebar(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowCommunitiesSidebar(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // IntegrationsSystemTraySection
    { QT_TR_NOOP("SYSTEM TRAY"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IntegrationsTray
    { QT_TR_NOOP("Minimize to tray"),
      QT_TR_NOOP("Keep the application running in the background after closing the client window."),
      SM::Toggle, SM::TabIntegrations,
      []() -> QVariant { return I->tray(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setTray(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // IntegrationsStartInTray
    { QT_TR_NOOP("Start in tray"),
      QT_TR_NOOP("Start the application in the background without showing the client window."),
      SM::Toggle, SM::TabIntegrations,
      []() -> QVariant { return I->startInTray(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setStartInTray(v.toBool()); return true;
      },
      {}, {}, {}, nullptr,
      []() -> bool { return I->tray(); } },
#ifdef NHEKO_DBUS_SYS
    // IntegrationsDbusSection
    { QT_TR_NOOP("D-BUS"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IntegrationsDbusApiAccess
    { QT_TR_NOOP("D-Bus access"),
      QT_TR_NOOP("Choose how much D-Bus access Komai exposes to local callers."),
      SM::Options, SM::TabIntegrations,
      []() -> QVariant { return I->integrationsDbusApiAccess(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int && !v.canConvert(QMetaType::Int))
              return false;
          auto access = v.toInt();
          if (access < IntegrationsDbusAccessNone || access > IntegrationsDbusAccessReadWrite)
              return false;
          I->setIntegrationsDbusApiAccess(access);
          return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "None"),
            QCoreApplication::translate("UserSettingsModel", "Read-only"),
            QCoreApplication::translate("UserSettingsModel", "Read & write"),
          };
      },
      nullptr },
#endif
    // IntegrationsBrowserSection
    { QT_TR_NOOP("BROWSER"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },

    // ── Timeline Tab ────────────────────────────────────────────────────────

    // TimelineMessagesSection
    { QT_TR_NOOP("MESSAGES"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Bubbles
    { QT_TR_NOOP("Enable message bubbles"),
      QT_TR_NOOP("Messages get a bubble background. This also triggers some layout changes (WIP)."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->bubbles(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setBubbles(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // SmallAvatars
    { QT_TR_NOOP("Use small avatars"),
      QT_TR_NOOP("Avatars are resized to fit above the message."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->smallAvatars(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setSmallAvatars(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowOwnAvatarInBubbleLayout
    { QT_TR_NOOP("Show your avatar next to your own messages (bubble layout)"),
      QT_TR_NOOP("When bubble layout is enabled, show your avatar next to your own messages. This improves left/right symmetry and makes authorship easier to scan."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->showOwnAvatarInBubbleLayout(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowOwnAvatarInBubbleLayout(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowSenderUsername
    { QT_TR_NOOP("Show sender username above messages"),
      QT_TR_NOOP("Control when sender usernames are displayed above messages. In bubble mode, your own username is always hidden. In smaller rooms, avatars and bubble colors are often enough context."),
      SM::Options, SM::TabTimeline,
      []() -> QVariant { return static_cast<int>(I->showSenderUsername()); },
      [](const QVariant &v) -> bool {
          auto val = v.toInt();
          if (val < 0 || QMetaEnum::fromType<UserSettings::ShowSenderUsername>().keyCount() <= val) return false;
          I->setShowSenderUsername(static_cast<UserSettings::ShowSenderUsername>(val)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Always"),
            QCoreApplication::translate("UserSettingsModel", "Only in large rooms (> 16 members)"),
            QCoreApplication::translate("UserSettingsModel", "Never"),
          };
      },
      nullptr },
    // MaxTimelineWidth
    { QT_TR_NOOP("Limit timeline width"),
      QT_TR_NOOP("Set the max width of messages in the timeline (in pixels). This can help readability on wide screen when Komai is maximized"),
      SM::Integer, SM::TabTimeline,
      []() -> QVariant { return I->maxTimelineWidth(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<int>())) return false;
          I->setMaxTimelineWidth(v.toInt()); return true;
      },
      0, 20000, 20, nullptr, nullptr },
    // EnlargeEmojiOnlyMessages
    { QT_TR_NOOP("Enlarge emoji-only messages"),
      QT_TR_NOOP("Make font size larger if messages with only a few emojis are displayed."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->enlargeEmojiOnlyMessages(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setEnlargeEmojiOnlyMessages(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // MessageHoverHighlight
    { QT_TR_NOOP("Highlight message on hover"),
      QT_TR_NOOP("Change the background color of messages when you hover over them."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->messageHoverHighlight(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setMessageHoverHighlight(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowActionButtons
    { QT_TR_NOOP("Show action buttons"),
      QT_TR_NOOP("Show quick actions next to each message (react, reply, forward, and more)."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->showActionButtons(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowActionButtons(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // PinnedReactions
    { QT_TR_NOOP("Pinned reactions"),
      QT_TR_NOOP("Comma-separated list of reactions always shown in the timeline hover bar (max 10). Your recent reactions fill the remaining slots up to 10 total."),
      SM::TextInput, SM::TabTimeline,
      []() -> QVariant { return I->pinnedReactions(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<QString>())) return false;
          I->setPinnedReactions(v.toString()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // TimelineMediaSection
    { QT_TR_NOOP("MEDIA"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // FancyEffects
    { QT_TR_NOOP("Show message effects"),
      QT_TR_NOOP("Some messages can be sent with fancy effects. For example, messages sent with '/confetti' will show confetti on screen."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->fancyEffects(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setFancyEffects(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // AnimateImagesOnHover
    { QT_TR_NOOP("Play animated images only on hover"),
      QT_TR_NOOP("Plays media like GIFs or WEBPs only when explicitly hovering over them."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->animateImagesOnHover(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setAnimateImagesOnHover(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowImage
    { QT_TR_NOOP("Show images automatically"),
      QT_TR_NOOP("If images should be automatically displayed. You can select between always showing images by default, only show them by default in private rooms or always require interaction to show images."),
      SM::Options, SM::TabTimeline,
      []() -> QVariant { return static_cast<int>(I->showImage()); },
      [](const QVariant &v) -> bool {
          auto val = v.toInt();
          if (val < 0 || QMetaEnum::fromType<UserSettings::ShowImage>().keyCount() <= val) return false;
          I->setShowImage(static_cast<UserSettings::ShowImage>(val)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Always"),
            QCoreApplication::translate("UserSettingsModel", "Only in private rooms"),
            QCoreApplication::translate("UserSettingsModel", "Never"),
          };
      },
      nullptr },
    // OpenImagesInExternalApp
    { QT_TR_NOOP("Open images in an external app"),
      QT_TR_NOOP("Open images in an external app when clicking the image.\nNote that when this option is ON, opened files are left unencrypted on disk and must be manually deleted."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->openImagesInExternalApp(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setOpenImagesInExternalApp(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // OpenVideosInExternalApp
    { QT_TR_NOOP("Open videos in an external app"),
      QT_TR_NOOP("Open videos in an external app when clicking the video.\nNote that when this option is ON, opened files are left unencrypted on disk and must be manually deleted."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->openVideosInExternalApp(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setOpenVideosInExternalApp(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },

    // ── Composer Tab ────────────────────────────────────────────────────────

    // ComposerInputSection
    { QT_TR_NOOP("INPUT"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Markdown
    { QT_TR_NOOP("Send messages as <a href=\"https://commonmark.org/help/\">Markdown</a>"),
      QT_TR_NOOP("Allow using markdown in messages.\nWhen disabled, all messages are sent as a plain text."),
      SM::Toggle, SM::TabComposer,
      []() -> QVariant { return I->markdown(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setMarkdown(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // SendMessageKey
    { QT_TR_NOOP("Send messages with a shortcut"),
      QT_TR_NOOP("Select what Enter key combination sends the message. Shift+Enter adds a new line, unless it has been selected, in which case Enter adds a new line instead.\n\nIf an emoji picker or a mention picker is open, it is always handled first."),
      SM::Options, SM::TabComposer,
      []() -> QVariant { return static_cast<int>(I->sendMessageKey()); },
      [](const QVariant &v) -> bool {
          auto val = v.toInt();
          if (val < 0 || QMetaEnum::fromType<UserSettings::SendMessageKey>().keyCount() <= val) return false;
          I->setSendMessageKey(static_cast<UserSettings::SendMessageKey>(val)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Enter"),
            QCoreApplication::translate("UserSettingsModel", "Shift+Enter"),
            QCoreApplication::translate("UserSettingsModel", "Ctrl+Enter"),
          };
      },
      nullptr },
    // AutoReplaceEmoji
    { QT_TR_NOOP("Auto-replace text emoticons with emoji"),
      QT_TR_NOOP("Automatically replace text emoticons like :) :D :P with their emoji equivalents when sending a message. Choose whether to replace everywhere or only at the end."),
      SM::Options, SM::TabComposer,
      []() -> QVariant { return static_cast<int>(I->autoReplaceEmoji()); },
      [](const QVariant &v) -> bool {
          auto val = v.toInt();
          if (val < 0 || QMetaEnum::fromType<UserSettings::AutoReplaceEmoji>().keyCount() <= val) return false;
          I->setAutoReplaceEmoji(static_cast<UserSettings::AutoReplaceEmoji>(val)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Always"),
            QCoreApplication::translate("UserSettingsModel", "Only at the end of messages"),
            QCoreApplication::translate("UserSettingsModel", "Never"),
          };
      },
      nullptr },
    // ComposerFeedbackSection
    { QT_TR_NOOP("FEEDBACK"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // TypingNotifications
    { QT_TR_NOOP("Typing notifications"),
      QT_TR_NOOP("Show who is typing in a room.\nThis will also enable or disable sending typing notifications to others."),
      SM::Toggle, SM::TabComposer,
      []() -> QVariant { return I->typingNotifications(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setTypingNotifications(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ReadReceipts
    { QT_TR_NOOP("Read receipts"),
      QT_TR_NOOP("Show if your message was read.\nStatus is displayed next to timestamps.\nWarning: If your homeserver does not support this, your rooms will never be marked as read!"),
      SM::Toggle, SM::TabComposer,
      []() -> QVariant { return I->readReceipts(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setReadReceipts(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ComposerExtrasSection
    { QT_TR_NOOP("EXTRAS"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableStickers
    { QT_TR_NOOP("Enable stickers"),
      QT_TR_NOOP("Show the sticker button in the message composer, allowing you to send stickers from custom sticker packs."),
      SM::Toggle, SM::TabComposer,
      []() -> QVariant { return I->enableStickers(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setEnableStickers(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },

    // ── Notifications Tab ───────────────────────────────────────────────────

    // NotificationsDesktopSection
    { QT_TR_NOOP("DESKTOP"), nullptr, SM::SectionTitle, SM::TabNotifications,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DesktopNotifications
    { QT_TR_NOOP("Desktop notifications"),
      QT_TR_NOOP("Notify about received messages when the client is not currently focused."),
      SM::Toggle, SM::TabNotifications,
      []() -> QVariant { return I->hasDesktopNotifications(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setDesktopNotifications(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // AlertOnIncomingMessages
    { QT_TR_NOOP("Alert on incoming messages"),
      QT_TR_NOOP("Show an alert when a message is received.\nThis usually causes the application icon in the task bar to animate in some fashion."),
      SM::Toggle, SM::TabNotifications,
      []() -> QVariant { return I->alertOnIncomingMessages(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setAlertOnIncomingMessages(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // DecryptNotifications
    { QT_TR_NOOP("Decrypt notifications"),
      QT_TR_NOOP("Decrypt messages shown in notifications for encrypted chats."),
      SM::Toggle, SM::TabNotifications,
      []() -> QVariant { return I->decryptNotifications(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setDecryptNotifications(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },

    // ── Calls Tab ───────────────────────────────────────────────────────────

    // CallsGeneralSection
    { QT_TR_NOOP("GENERAL"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableLegacyCalls
    { QT_TR_NOOP("Enable legacy calls"),
      QT_TR_NOOP("Show the call button in the message composer. This uses the old VoIP calling feature which may not work reliably. Element Call support is expected in a future release."),
      SM::Toggle, SM::TabCalls,
      []() -> QVariant { return I->enableLegacyCalls(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setEnableLegacyCalls(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // UseFallbackCallRelayServer
    { QT_TR_NOOP("Use fallback call relay server"),
      QT_TR_NOOP("Use turn.matrix.org as a fallback relay/STUN server when your homeserver does not provide one."),
      SM::Toggle, SM::TabCalls,
      []() -> QVariant { return I->useFallbackCallRelayServer(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUseFallbackCallRelayServer(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // CallsDevicesSection
    { QT_TR_NOOP("DEVICES"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Microphone
    { QT_TR_NOOP("Microphone"),
      QT_TR_NOOP("Select the microphone used for voice and video calls."),
      SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(false, I->microphone().toStdString()))
            .indexOf(I->microphone());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto list = vecToList(CallDevices::instance().names(false, I->microphone().toStdString()));
          I->setMicrophone(list.at(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(false, I->microphone().toStdString()));
      },
      nullptr },
    // Camera
    { QT_TR_NOOP("Camera"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(true, I->camera().toStdString()))
            .indexOf(I->camera());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto list = vecToList(CallDevices::instance().names(true, I->camera().toStdString()));
          I->setCamera(list.at(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(true, I->camera().toStdString()));
      },
      nullptr },
    // CameraResolution
    { QT_TR_NOOP("Camera resolution"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().resolutions(I->camera().toStdString()))
            .indexOf(I->cameraResolution());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto list = vecToList(CallDevices::instance().resolutions(I->camera().toStdString()));
          I->setCameraResolution(list.at(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().resolutions(I->camera().toStdString()));
      },
      nullptr },
    // CameraFrameRate
    { QT_TR_NOOP("Camera frame rate"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().frameRates(
            I->camera().toStdString(), I->cameraResolution().toStdString()))
            .indexOf(I->cameraFrameRate());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto list = vecToList(CallDevices::instance().frameRates(
            I->camera().toStdString(), I->cameraResolution().toStdString()));
          I->setCameraFrameRate(list.at(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().frameRates(
            I->camera().toStdString(), I->cameraResolution().toStdString()));
      },
      nullptr },
    // Ringtone
    { QT_TR_NOOP("Ringtone"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          auto v = I->ringtone();
          if (v == QStringView(u"Mute")) return 0;
          if (v == QStringView(u"Default")) return 1;
          if (v == QStringView(u"Other")) return 2;
          return 3;
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          int ringtone = v.toInt();
          if (ringtone == 2) {
              QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
              auto filepath = QFileDialog::getOpenFileName(
                nullptr,
                QCoreApplication::translate("UserSettingsModel", "Select a file"),
                homeFolder,
                QCoreApplication::translate("UserSettingsModel", "All Files (*)"));
              if (!filepath.isEmpty()) { I->setRingtone(filepath); I->setRingtone(filepath); }
          } else if (ringtone == 0) {
              I->setRingtone(QStringLiteral("Mute")); I->setRingtone(QStringLiteral("Mute"));
          } else if (ringtone == 1) {
              I->setRingtone(QStringLiteral("Default")); I->setRingtone(QStringLiteral("Default"));
          }
          return true;
      },
      {}, {}, {},
      []() -> QVariant {
          QStringList l{QStringLiteral("Mute"), QStringLiteral("Default"), QStringLiteral("Other")};
          if (!l.contains(I->ringtone())) l.push_back(I->ringtone());
          return l;
      },
      nullptr },

    // ── Privacy Tab ─────────────────────────────────────────────────────────

    // PrivacyScreenLockSection
    { QT_TR_NOOP("SCREEN LOCK"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // PrivacyScreen
    { QT_TR_NOOP("Privacy screen"),
      QT_TR_NOOP("When the window loses focus, the timeline will\nbe blurred."),
      SM::Toggle, SM::TabPrivacy,
      []() -> QVariant { return I->privacyScreen(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setPrivacyScreen(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // PrivacyScreenTimeoutSeconds
    { QT_TR_NOOP("Privacy screen timeout (seconds)"),
      QT_TR_NOOP("Set how long after focus loss before the screen is blurred.\nSet to 0 to blur immediately after focus loss.\nMaximum is 3600 seconds (1 hour)."),
      SM::Integer, SM::TabPrivacy,
      []() -> QVariant { return I->privacyScreenTimeoutSeconds(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<int>())) return false;
          I->setPrivacyScreenTimeoutSeconds(v.toInt()); return true;
      },
      0, 3600, 10, nullptr,
      []() -> bool { return I->privacyScreen(); } },
    // PrivacyDataSection
    { QT_TR_NOOP("DATA & MAINTENANCE"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ExpireEvents
    { QT_TR_NOOP("Periodically delete expired events"),
      QT_TR_NOOP("Regularly redact expired events as specified in the event expiration configuration. Since this is currently not executed server side, you need to have one client running this regularly."),
      SM::Toggle, SM::TabPrivacy,
      []() -> QVariant { return I->expireEvents(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setExpireEvents(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // HiddenTimelineEvents
    { QT_TR_NOOP("Hidden events"),
      QT_TR_NOOP("Configure whether to show or hide certain events like room joins."),
      SM::ConfigureHiddenEvents, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UpdateSpaceVias
    { QT_TR_NOOP("Periodically update community routing information"),
      QT_TR_NOOP("To allow new users to join a community, the community needs to expose some information about what servers participate in a room to community members. Since the room participants can change over time, this needs to be updated from time to time. This setting enables a background job to do that automatically."),
      SM::Toggle, SM::TabPrivacy,
      []() -> QVariant { return I->updateSpaceVias(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUpdateSpaceVias(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // PrivacyUsersSection
    { QT_TR_NOOP("USERS"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IgnoredUsers
    { QT_TR_NOOP("Ignored users"),
      QT_TR_NOOP("Manage your ignored users."),
      SM::ManageIgnoredUsers, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },

    // ── Encryption Tab ──────────────────────────────────────────────────────

    // EncryptionKeySharingSection
    { QT_TR_NOOP("KEY SHARING"), nullptr, SM::SectionTitle, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // OnlyShareKeysWithVerifiedUsers
    { QT_TR_NOOP("Send encrypted messages to verified users only"),
      QT_TR_NOOP("Requires a user to be verified to send encrypted messages to them. This improves safety but makes E2EE more tedious."),
      SM::Toggle, SM::TabEncryption,
      []() -> QVariant { return I->onlyShareKeysWithVerifiedUsers(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setOnlyShareKeysWithVerifiedUsers(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShareKeysWithTrustedUsers
    { QT_TR_NOOP("Share keys with verified users and devices"),
      QT_TR_NOOP("Automatically replies to key requests from other users if they are verified, even if that device shouldn't have access to those keys otherwise."),
      SM::Toggle, SM::TabEncryption,
      []() -> QVariant { return I->shareKeysWithTrustedUsers(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShareKeysWithTrustedUsers(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // EncryptionBackupSection
    { QT_TR_NOOP("BACKUP"), nullptr, SM::SectionTitle, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UseOnlineKeyBackup
    { QT_TR_NOOP("Online key backup"),
      QT_TR_NOOP("Download message encryption keys from and upload to the encrypted online key backup."),
      SM::Toggle, SM::TabEncryption,
      []() -> QVariant { return I->useOnlineKeyBackup(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUseOnlineKeyBackup(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // SessionKeys
    { QT_TR_NOOP("Session keys"), nullptr, SM::SessionKeyImportExport, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EncryptionCrossSigningSection
    { QT_TR_NOOP("CROSS-SIGNING"), nullptr, SM::SectionTitle, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // OnlineBackupKey
    { QT_TR_NOOP("Online backup key"),
      QT_TR_NOOP("The key to decrypt online key backups. If it is cached, you can enable online key backup to store encryption keys securely encrypted on the server."),
      SM::KeyStatus, SM::TabEncryption,
      []() -> QVariant { return cache::secret(mtx::secret_storage::secrets::megolm_backup_v1).has_value(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // SelfSigningKey
    { QT_TR_NOOP("Self signing key"),
      QT_TR_NOOP("The key to verify your own devices. If it is cached, verifying one of your devices will mark it verified for all your other devices and for users that have verified you."),
      SM::KeyStatus, SM::TabEncryption,
      []() -> QVariant { return cache::secret(mtx::secret_storage::secrets::cross_signing_self_signing).has_value(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // UserSigningKey
    { QT_TR_NOOP("User signing key"),
      QT_TR_NOOP("The key to verify other users. If it is cached, verifying a user will verify all their devices."),
      SM::KeyStatus, SM::TabEncryption,
      []() -> QVariant { return cache::secret(mtx::secret_storage::secrets::cross_signing_user_signing).has_value(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // MasterKey
    { QT_TR_NOOP("Master signing key"),
      QT_TR_NOOP("Your most important key. You don't need to have it cached, since not caching it makes it less likely it can be stolen and it is only needed to rotate your other signing keys."),
      SM::KeyStatus, SM::TabEncryption,
      []() -> QVariant { return cache::secret(mtx::secret_storage::secrets::cross_signing_master).has_value(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // CrossSigningSecrets
    { QT_TR_NOOP("Cross-signing secrets"), nullptr, SM::XSignKeysRequestDownload, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },

    // ── Session Tab ─────────────────────────────────────────────────────────

    // SessionAccountSection
    { QT_TR_NOOP("ACCOUNT"), nullptr, SM::SectionTitle, SM::TabSession,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UserId
    { QT_TR_NOOP("User ID"), nullptr, SM::ReadOnlyText, SM::TabSession,
      []() -> QVariant { return I->userId(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Homeserver
    { QT_TR_NOOP("Homeserver"), nullptr, SM::ReadOnlyText, SM::TabSession,
      []() -> QVariant { return I->homeserver(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Profile
    { QT_TR_NOOP("Profile"), nullptr, SM::ProfileButton, SM::TabSession,
      []() -> QVariant {
          return I->profile().isEmpty()
            ? QCoreApplication::translate("UserSettingsModel", "Default")
            : I->profile();
      },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // SessionDeviceSection
    { QT_TR_NOOP("DEVICE"), nullptr, SM::SectionTitle, SM::TabSession,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DeviceId
    { QT_TR_NOOP("Device ID"), nullptr, SM::ReadOnlyText, SM::TabSession,
      []() -> QVariant { return I->deviceId(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // DeviceFingerprint
    { QT_TR_NOOP("Device fingerprint"), nullptr, SM::ReadOnlyText, SM::TabSession,
      []() -> QVariant { return utils::humanReadableFingerprint(olm::client()->identity_keys().ed25519); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AccessToken
    { QT_TR_NOOP("Access token"),
      QT_TR_NOOP("Your access token gives full access to your account. Do not share it with anyone."),
      SM::AccessTokenField, SM::TabSession,
      []() -> QVariant { return I->accessToken(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // SessionActionsSection
    { QT_TR_NOOP("ACTIONS"), nullptr, SM::SectionTitle, SM::TabSession,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Logout
    { QT_TR_NOOP("Logout"), nullptr, SM::LogoutButton, SM::TabSession,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },

    // ── About Tab ───────────────────────────────────────────────────────────

    // AboutApplicationSection
    { QT_TR_NOOP("APPLICATION"), nullptr, SM::SectionTitle, SM::TabAbout,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // AppName
    { QT_TR_NOOP("Name"), nullptr, SM::Link, SM::TabAbout,
      []() -> QVariant {
          return QStringLiteral("<a href=\"https://github.com/etkecc/komai\">Komai</a> @ ") +
                 QString::fromStdString(nheko::version) +
                 QStringLiteral(" (<a href=\"https://github.com/etkecc/komai/commit/") +
                 QString::fromStdString(nheko::commit_hash) + QStringLiteral("\">") +
                 QString::fromStdString(nheko::commit_hash) + QStringLiteral("</a>)");
      },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Platform
    { QT_TR_NOOP("Platform"), nullptr, SM::ReadOnlyText, SM::TabAbout,
      []() -> QVariant { return QString::fromStdString(nheko::build_os); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // BasedOn
    { QT_TR_NOOP("Based on"), nullptr, SM::Link, SM::TabAbout,
      []() -> QVariant {
          return QStringLiteral(
            "<a href=\"https://nheko.im/nheko-reborn/nheko\">nheko</a> @ ~v0.12.1 "
            "(<a href=\"https://nheko.im/nheko-reborn/nheko/-/commit/"
            "abb2325a995f936081219f402339fc8e0a661ac1\">abb2325a</a>)");
      },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // MaintainedBy
    { QT_TR_NOOP("Maintained by"), nullptr, SM::Link, SM::TabAbout,
      []() -> QVariant { return QStringLiteral("<a href=\"https://etke.cc\">etke.cc</a>"); },
      nullptr, {}, {}, {}, nullptr, nullptr },
};
// clang-format on

// Note: settingsTable must have exactly COUNT entries (one per Indices enum value before COUNT).
// The Indices enum puts ScaleFactor, IntegrationsDbusSection, and the D-Bus rows
// after COUNT when platform flags exclude them.

#undef I
#undef SM

} // anonymous namespace

QVariant
UserSettingsModel::data(const QModelIndex &index, int role) const
{
    if (index.row() >= COUNT)
        return {};

    auto i = UserSettings::instance();
    if (!i)
        return {};

    const auto &m = settingsTable[index.row()];

    switch (role) {
    case Name:
        return m.name ? tr(m.name) : QVariant{};
    case Description:
        return m.description ? tr(m.description) : QVariant{};
    case Type:
        return m.type;
    case Tab:
        return m.tab;
    case Value:
        return m.getValue ? m.getValue() : QVariant{};
    case Enabled:
        return m.isEnabled ? m.isEnabled() : true;
    case ValueLowerBound:
        return m.lowerBound;
    case ValueUpperBound:
        return m.upperBound;
    case ValueStep:
        return m.step;
    case Values:
        return m.getValues ? m.getValues() : QVariant{};

    // Special roles with only a few entries — keep as small switches
    case Good:
        switch (index.row()) {
        case OnlineBackupKey:
            return cache::secret(mtx::secret_storage::secrets::megolm_backup_v1).has_value();
        case SelfSigningKey:
            return cache::secret(mtx::secret_storage::secrets::cross_signing_self_signing)
              .has_value();
        case UserSigningKey:
            return cache::secret(mtx::secret_storage::secrets::cross_signing_user_signing)
              .has_value();
        case MasterKey:
            return true;
        }
        break;
    case ThemeVariantValue:
        if (index.row() == Theme) {
            auto variant = ThemeRegistry::instance().themeVariant(i->theme());
            if (variant == u"light")
                return 0;
            if (variant == u"dark")
                return 1;
            return 2;
        }
        return -1;
    case ThemeVariantValues:
        if (index.row() == Theme)
            return QStringList{
              QStringLiteral("Light"), QStringLiteral("Dark"), QStringLiteral("System")};
        return QStringList{};
    case SettingImage:
        return QString();
    }

    return {};
}

bool
UserSettingsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.row() >= COUNT)
        return false;

    auto i = UserSettings::instance();

    if (role == Value) {
        const auto &m = settingsTable[index.row()];
        return m.setValue ? m.setValue(value) : false;
    } else if (role == ThemeVariantValue) {
        if (index.row() == Theme) {
            int variantIdx = value.toInt();
            QString newVariant;
            if (variantIdx == 0)
                newVariant = QStringLiteral("light");
            else if (variantIdx == 1)
                newVariant = QStringLiteral("dark");
            else
                newVariant = QStringLiteral("system");
            auto currentVariant = ThemeRegistry::instance().themeVariant(i->theme());
            if (newVariant == currentVariant)
                return false;
            i->setTheme(ThemeRegistry::instance().defaultThemeSlug(newVariant));
            return true;
        }
        return false;
    }
    return false;
}

void
UserSettingsModel::importSessionKeys()
{
    const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString fileName   = QFileDialog::getOpenFileName(
      nullptr, tr("Open Sessions File"), homeFolder, QLatin1String(""));

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(nullptr, tr("Error"), file.errorString());
        return;
    }

    auto bin     = file.peek(file.size());
    auto payload = std::string(bin.data(), bin.size());

    bool ok;
    auto password = QInputDialog::getText(nullptr,
                                          tr("File Password"),
                                          tr("Enter the passphrase to decrypt the file:"),
                                          QLineEdit::Password,
                                          QLatin1String(""),
                                          &ok);
    if (!ok)
        return;

    if (password.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Error"), tr("The password cannot be empty"));
        return;
    }

    try {
        auto sessions = mtx::crypto::decrypt_exported_sessions(payload, password.toStdString());
        cache::importSessionKeys(std::move(sessions));
    } catch (const std::exception &e) {
        QMessageBox::warning(nullptr, tr("Error"), e.what());
    }
}
void
UserSettingsModel::exportSessionKeys()
{
    // Open password dialog.
    bool ok;
    auto password = QInputDialog::getText(nullptr,
                                          tr("File Password"),
                                          tr("Enter passphrase to encrypt your session keys:"),
                                          QLineEdit::Password,
                                          QLatin1String(""),
                                          &ok);
    if (!ok)
        return;

    if (password.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Error"), tr("The password cannot be empty"));
        return;
    }

    auto repeatedPassword = QInputDialog::getText(nullptr,
                                                  tr("Repeat File Password"),
                                                  tr("Repeat the passphrase:"),
                                                  QLineEdit::Password,
                                                  QLatin1String(""),
                                                  &ok);
    if (!ok)
        return;

    if (password != repeatedPassword) {
        QMessageBox::warning(nullptr, tr("Error"), tr("Passwords don't match"));
        return;
    }

    // Open file dialog to save the file.
    const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString fileName   = QFileDialog::getSaveFileName(
      nullptr, tr("File to save the exported session keys"), homeFolder);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(nullptr, tr("Error"), file.errorString());
        return;
    }

    // Export sessions & save to file.
    try {
        auto encrypted_blob = mtx::crypto::encrypt_exported_sessions(cache::exportSessionKeys(),
                                                                     password.toStdString());

        QString b64 = QString::fromStdString(mtx::crypto::bin2base64(encrypted_blob));

        QString prefix(QStringLiteral("-----BEGIN MEGOLM SESSION DATA-----"));
        QString suffix(QStringLiteral("-----END MEGOLM SESSION DATA-----"));
        QString newline(QStringLiteral("\n"));
        QTextStream out(&file);
        out << prefix << newline << b64 << newline << suffix << newline;
        file.close();
    } catch (const std::exception &e) {
        QMessageBox::warning(nullptr, tr("Error"), e.what());
    }
}
void
UserSettingsModel::requestCrossSigningSecrets()
{
    olm::request_cross_signing_keys();
}
void
UserSettingsModel::downloadCrossSigningSecrets()
{
    olm::download_cross_signing_keys();
}

UserSettingsModel::UserSettingsModel(QObject *p)
  : QAbstractListModel(p)
{
    auto s = UserSettings::instance();

#define CONNECT_SETTING(idx, sig, ...)                                                             \
    connect(s.get(), &UserSettings::sig, this, [this]() {                                          \
        emit dataChanged(index(idx), index(idx), {__VA_ARGS__});                                   \
    })

    // Look & Feel
    CONNECT_SETTING(Theme, themeChanged, Value, Values, ThemeVariantValue);
    CONNECT_SETTING(Font, fontChanged, Value);
    CONNECT_SETTING(FontSize, fontSizeChanged, Value);
    CONNECT_SETTING(EmojiFont, emojiFontChanged, Value);
    CONNECT_SETTING(EnableUIAnimations, reducedMotionChanged, Value);
    CONNECT_SETTING(CompactRoomList, compactRoomListChanged, Value);
    CONNECT_SETTING(ShowRoomListTime, showRoomListTimeChanged, Value);
    CONNECT_SETTING(ShowLastMessagePreview, showLastMessagePreviewChanged, Value);
    CONNECT_SETTING(ShowCommunityNotificationCounts, showCommunityNotificationCountsChanged, Value);
    CONNECT_SETTING(UseCircularAvatars, useCircularAvatarsChanged, Value);
    CONNECT_SETTING(UseIdenticon, useIdenticonChanged, Value);
    CONNECT_SETTING(ScrollbarsInRoomlist, scrollbarsInRoomlistChanged, Value);
    CONNECT_SETTING(RoomSorting, roomSortOrderChanged, Value);
    CONNECT_SETTING(ShowCommunitiesSidebar, showCommunitiesSidebarChanged, Value);
    CONNECT_SETTING(MobileMode, mobileModeChanged, Value);
    CONNECT_SETTING(EnableSwipeGestures, enableSwipeGesturesChanged, Value);

    // Integrations
    CONNECT_SETTING(IntegrationsTray, trayChanged, Value);
    CONNECT_SETTING(IntegrationsStartInTray, startInTrayChanged, Value);
#ifdef NHEKO_DBUS_SYS
    CONNECT_SETTING(IntegrationsDbusApiAccess, integrationsDbusApiAccessChanged, Value);
#endif

    // Tray has a side-effect on StartInTray's Enabled state
    connect(s.get(), &UserSettings::trayChanged, this, [this]() {
        emit dataChanged(index(IntegrationsTray), index(IntegrationsTray), {Value});
        emit dataChanged(index(IntegrationsStartInTray), index(IntegrationsStartInTray), {Enabled});
    });

    // Timeline
    CONNECT_SETTING(Bubbles, bubblesChanged, Value);
    CONNECT_SETTING(SmallAvatars, smallAvatarsChanged, Value);
    CONNECT_SETTING(ShowOwnAvatarInBubbleLayout, showOwnAvatarInBubbleLayoutChanged, Value);
    CONNECT_SETTING(ShowSenderUsername, showSenderUsernameChanged, Value);
    CONNECT_SETTING(MaxTimelineWidth, maxTimelineWidthChanged, Value);
    CONNECT_SETTING(EnlargeEmojiOnlyMessages, enlargeEmojiOnlyMessagesChanged, Value);
    CONNECT_SETTING(MessageHoverHighlight, messageHoverHighlightChanged, Value);
    CONNECT_SETTING(ShowActionButtons, showActionButtonsChanged, Value);
    CONNECT_SETTING(PinnedReactions, pinnedReactionsChanged, Value);
    CONNECT_SETTING(FancyEffects, fancyEffectsChanged, Value);
    CONNECT_SETTING(AnimateImagesOnHover, animateImagesOnHoverChanged, Value);
    CONNECT_SETTING(ShowImage, showImageChanged, Value);
    CONNECT_SETTING(OpenImagesInExternalApp, openImagesInExternalAppChanged, Value);
    CONNECT_SETTING(OpenVideosInExternalApp, openVideosInExternalAppChanged, Value);

    // Composer
    CONNECT_SETTING(Markdown, markdownChanged, Value);
    CONNECT_SETTING(SendMessageKey, sendMessageKeyChanged, Value);
    CONNECT_SETTING(AutoReplaceEmoji, autoReplaceEmojiChanged, Value);
    CONNECT_SETTING(TypingNotifications, typingNotificationsChanged, Value);
    CONNECT_SETTING(ReadReceipts, readReceiptsChanged, Value);
    CONNECT_SETTING(EnableStickers, enableStickersChanged, Value);

    // Notifications
    CONNECT_SETTING(DesktopNotifications, desktopNotificationsChanged, Value);
    CONNECT_SETTING(AlertOnIncomingMessages, alertOnIncomingMessagesChanged, Value);
    CONNECT_SETTING(DecryptNotifications, decryptNotificationsChanged, Value);

    // Calls
    CONNECT_SETTING(EnableLegacyCalls, enableLegacyCallsChanged, Value);
    CONNECT_SETTING(UseFallbackCallRelayServer, useFallbackCallRelayServerChanged, Value);
    CONNECT_SETTING(Microphone, microphoneChanged, Value, Values);
    CONNECT_SETTING(Camera, cameraChanged, Value, Values);
    CONNECT_SETTING(CameraResolution, cameraResolutionChanged, Value, Values);
    CONNECT_SETTING(CameraFrameRate, cameraFrameRateChanged, Value, Values);
    CONNECT_SETTING(Ringtone, ringtoneChanged, Values, Value);

    // Privacy — PrivacyScreen has a side-effect on PrivacyScreenTimeoutSeconds's Enabled state
    connect(s.get(), &UserSettings::privacyScreenChanged, this, [this]() {
        emit dataChanged(index(PrivacyScreen), index(PrivacyScreen), {Value});
        emit dataChanged(
          index(PrivacyScreenTimeoutSeconds), index(PrivacyScreenTimeoutSeconds), {Enabled});
    });
    CONNECT_SETTING(PrivacyScreenTimeoutSeconds, privacyScreenTimeoutSecondsChanged, Value);
    CONNECT_SETTING(ExpireEvents, expireEventsChanged, Value);
    CONNECT_SETTING(UpdateSpaceVias, updateSpaceViasChanged, Value);

    // Encryption
    CONNECT_SETTING(OnlyShareKeysWithVerifiedUsers, onlyShareKeysWithVerifiedUsersChanged, Value);
    CONNECT_SETTING(ShareKeysWithTrustedUsers, shareKeysWithTrustedUsersChanged, Value);
    CONNECT_SETTING(UseOnlineKeyBackup, useOnlineKeyBackupChanged, Value);

    // Cross-signing secrets (from MainWindow, not UserSettings)
    connect(MainWindow::instance(), &MainWindow::secretsChanged, this, [this]() {
        emit dataChanged(index(OnlineBackupKey), index(MasterKey), {Value, Good});
    });

#undef CONNECT_SETTING
}

// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineViewManager.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMimeData>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QStandardPaths>
#include <QString>
#include <QUrl>

#include "TimelineModel.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "encryption/VerificationManager.h"
#include "events/EventAccessors.h"
#include "imagepacks/CombinedImagePackModel.h"
#include "imagepacks/GridImagePackModel.h"
#include "imagepacks/ImagePackListModel.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "models/CommandCompleter.h"
#include "models/CompletionProxyModel.h"
#include "models/InviteesModel.h"
#include "models/MemberList.h"
#include "models/RoomsModel.h"
#include "models/UsersModel.h"
#include "providers/MxcImageProvider.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/CommunitiesModel.h"
#include "timeline/PresenceEmitter.h"
#include "timeline/RoomlistModel.h"
#include "ui/MainWindow.h"
#include "ui/RoomSettings.h"
#include "ui/UserProfile.h"
#include "utils/Utils.h"
#include "voip/CallManager.h"
#include "voip/WebRTCSession.h"

namespace {
bool
isTruthyEnvValue(const QByteArray &value)
{
    const auto normalized = value.trimmed().toLower();
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

}

void
TimelineViewManager::updateColorPalette()
{
    userColors.clear();
    roomUserColors_.clear();
    roomMemberCache_.clear();
}

QColor
TimelineViewManager::userColor(QString id, QColor background)
{
    std::pair<QString, quint64> idx{id, background.rgba64()};
    if (!userColors.contains(idx))
        userColors.insert(idx, QColor(utils::generateContrastingHexColor(id, background)));
    return userColors.value(idx);
}

// 16 maximally-spaced hues (in degrees) for small-room palette assignment.
// Ordered so that adjacent slots have large hue separation (golden-angle inspired).
const std::vector<double> TimelineViewManager::kPaletteHues = {
  0,     // red
  137.5, // green-cyan
  275,   // violet
  52.5,  // amber/yellow
  190,   // cyan-blue
  327.5, // magenta-pink
  95,    // lime/chartreuse
  232.5, // blue-indigo
  22.5,  // orange-red
  160,   // teal
  297.5, // purple
  75,    // yellow-green
  212.5, // azure
  350,   // rose
  117.5, // green
  255,   // blue-violet
};

QColor
TimelineViewManager::roomUserColor(QString roomId,
                                   QString userId,
                                   QColor background,
                                   QColor accentColor,
                                   int colorCodingPolicy)
{
    // Guard against empty strings (e.g. event data not yet loaded) to avoid
    // backend key-size errors from cache lookups with zero-length keys.
    if (roomId.isEmpty() || userId.isEmpty())
        return QColor();

    auto selfId              = utils::localUser();
    const bool isPreviewRoom = roomId.startsWith(QLatin1String("!timeline-preview:"));

    const auto policy = [colorCodingPolicy]() {
        if (colorCodingPolicy >=
              static_cast<int>(UserSettings::TimelineUserColorCodingPolicy::AdaptiveByRoomSize) &&
            colorCodingPolicy <=
              static_cast<int>(UserSettings::TimelineUserColorCodingPolicy::MeVsOthers)) {
            return static_cast<UserSettings::TimelineUserColorCodingPolicy>(colorCodingPolicy);
        }

        const auto settings = UserSettings::instance();
        return settings ? settings->timelineUserColorCodingPolicy()
                        : UserSettings::TimelineUserColorCodingPolicy::AdaptiveByRoomSize;
    }();

    const auto othersColor = [accentColor]() {
        // Hue is offset 150 degrees from the theme accent so it always contrasts
        // with the sender's own bubble color (e.g. orange accent -> teal, blue -> magenta).
        double accentHue = accentColor.hslHue();
        int neutralHue   = (static_cast<int>(accentHue + 150)) % 360;
        return QColor::fromHsl(neutralHue, 80, 130);
    };

    if (isPreviewRoom) {
        if (policy == UserSettings::TimelineUserColorCodingPolicy::MeVsOthers)
            return userId == selfId ? accentColor : othersColor();

        // Settings preview uses a synthetic room that does not exist in cache; generate stable
        // per-member colors directly from ids so color-coding policy changes remain visible.
        return userColor(QStringLiteral("%1|%2").arg(roomId, userId), background);
    }

    // Former member: return a neutral gray regardless of room size.
    if (!cache::isRoomMember(userId.toStdString(), roomId.toStdString())) {
        auto bgLightness = background.lightnessF();
        if (bgLightness > 0.5)
            return QColor::fromHsl(0, 0, 180); // light theme: medium-light gray
        else
            return QColor::fromHsl(0, 0, 100); // dark theme: medium-dark gray
    }

    if (policy == UserSettings::TimelineUserColorCodingPolicy::MeVsOthers)
        return userId == selfId ? accentColor : othersColor();

    auto memberCount = static_cast<int>(cache::memberCount(roomId.toStdString()));

    // Large room (>16 members): return a uniform accent-complementary color.
    if (memberCount > 16) {
        return othersColor();
    }

    // Small room (<=16 members): assign unique palette colors.
    std::pair<QString, QString> cacheKey{roomId, userId};
    if (roomUserColors_.contains(cacheKey))
        return roomUserColors_.value(cacheKey);

    // Build or retrieve the sorted member list (excluding self).
    // Invalidate if cached size doesn't match current member count.
    bool needsRefresh = !roomMemberCache_.contains(roomId);
    if (!needsRefresh) {
        auto cachedSize = static_cast<int>(roomMemberCache_.value(roomId).size());
        if (cachedSize + 1 != memberCount)
            needsRefresh = true;
    }
    if (needsRefresh) {
        auto it = roomUserColors_.begin();
        while (it != roomUserColors_.end()) {
            if (it.key().first == roomId)
                it = roomUserColors_.erase(it);
            else
                ++it;
        }

        auto members = cache::roomMembers(roomId.toStdString());
        members.erase(std::remove(members.begin(), members.end(), selfId.toStdString()),
                      members.end());
        std::sort(members.begin(), members.end());
        roomMemberCache_.insert(roomId, members);
    }

    const auto &members = roomMemberCache_.value(roomId);

    // Filter palette hues that are too close to the accent color (self bubble hue).
    double accentHue                = accentColor.hslHueF() * 360.0;
    constexpr double kExclusionZone = 30.0; // degrees on each side

    std::vector<double> filteredHues;
    filteredHues.reserve(kPaletteHues.size());
    for (double h : kPaletteHues) {
        double diff = std::abs(h - accentHue);
        if (diff > 180.0)
            diff = 360.0 - diff;
        if (diff >= kExclusionZone)
            filteredHues.push_back(h);
    }
    // Fallback: if too many hues were filtered, use the full palette.
    if (filteredHues.size() < 8)
        filteredHues = kPaletteHues;

    // Find this user's palette slot.
    auto it  = std::find(members.begin(), members.end(), userId.toStdString());
    int slot = 0;
    if (it != members.end())
        slot = static_cast<int>(std::distance(members.begin(), it));

    double hue   = filteredHues[static_cast<size_t>(slot) % filteredHues.size()];
    QColor color = QColor::fromHslF(hue / 360.0, 0.7, 0.5);

    roomUserColors_.insert(cacheKey, color);
    return color;
}

TimelineViewManager::TimelineViewManager(CallManager *, ChatPage *parent)
  : QObject(parent)
  , rooms_(new RoomlistModel(this))
  , frooms_(new FilteredRoomlistModel(this->rooms_))
  , communities_(new CommunitiesModel(this))
  , verificationManager_(new VerificationManager(this))
  , presenceEmitter(new PresenceEmitter(this))
{
    instance_              = this;
    roomSwitchPerfEnabled_ = isTruthyEnvValue(qgetenv("KOMAI_ROOM_SWITCH_PERF")) ||
                             isTruthyEnvValue(qgetenv("KOMAI_PERF_ROOM_SWITCH"));

    if (roomSwitchPerfEnabled_) {
        nhlog::ui()->info("Room-switch performance tracing enabled (set by "
                          "KOMAI_ROOM_SWITCH_PERF/KOMAI_PERF_ROOM_SWITCH).");
        nhlog::ui()->flush();
    }

    connect(this->communities_,
            &CommunitiesModel::currentTagIdChanged,
            frooms_,
            &FilteredRoomlistModel::updateFilterTag);
    connect(this->communities_,
            &CommunitiesModel::hiddenTagsChanged,
            frooms_,
            &FilteredRoomlistModel::updateHiddenTagsAndSpaces);

    updateColorPalette();

    connect(UserSettings::instance().get(),
            &UserSettings::uiThemeSlugChanged,
            this,
            &TimelineViewManager::updateColorPalette);
    connect(UserSettings::instance().get(),
            &UserSettings::timelineUserColorCodingPolicyChanged,
            this,
            &TimelineViewManager::updateColorPalette);
    connect(parent,
            &ChatPage::receivedRoomDeviceVerificationRequest,
            verificationManager_,
            &VerificationManager::receivedRoomDeviceVerificationRequest);
    connect(parent,
            &ChatPage::receivedDeviceVerificationRequest,
            verificationManager_,
            &VerificationManager::receivedDeviceVerificationRequest);
    connect(parent,
            &ChatPage::receivedDeviceVerificationStart,
            verificationManager_,
            &VerificationManager::receivedDeviceVerificationStart);
    connect(parent, &ChatPage::loggedOut, this, [this]() {
        waitingForFirstSync_ = true;
        emit waitingForFirstSyncChanged(true);
    });
    connect(parent, &ChatPage::connectionLost, this, [this] {
        isConnected_ = false;
        emit isConnectedChanged(false);
    });
    connect(parent, &ChatPage::connectionRestored, this, [this] {
        isConnected_ = true;
        emit isConnectedChanged(true);
    });
    connect(rooms_, &RoomlistModel::spaceSelected, communities_, [this](QString roomId) {
        communities_->setCurrentTagId("space:" + roomId);
    });
}

TimelineViewManager *
TimelineViewManager::create(QQmlEngine *qmlEngine, QJSEngine *)
{
    // The instance has to exist before it is used. We cannot replace it.
    Q_ASSERT(instance_);

    // The engine has to have the same thread affinity as the singleton.
    Q_ASSERT(qmlEngine->thread() == instance_->thread());

    // There can only be one engine accessing the singleton.
    static QJSEngine *s_engine = nullptr;
    if (s_engine)
        Q_ASSERT(qmlEngine == s_engine);
    else
        s_engine = qmlEngine;

    QJSEngine::setObjectOwnership(instance_, QJSEngine::CppOwnership);
    return instance_;
}

void
TimelineViewManager::clearAll()
{
    rooms_->clear();
}

void
TimelineViewManager::sync(const mtx::responses::Sync &sync_)
{
    this->rooms_->sync(sync_);
    this->communities_->sync(sync_);
    this->presenceEmitter->sync(sync_.presence);
    this->processIgnoredUsers(sync_.account_data);

    if (waitingForFirstSync_) {
        this->waitingForFirstSync_ = false;
        emit waitingForFirstSyncChanged(false);
    }
}

QString
TimelineViewManager::escapeEmoji(QString str) const
{
    return utils::replaceEmoji(str);
}

void
TimelineViewManager::markRoomSwitchRequested(const QString &roomId, const QString &reason)
{
    if (!roomSwitchPerfEnabled_ || roomId.isEmpty())
        return;

    roomSwitchPerfSwitchId_++;
    roomSwitchPerfActiveRoomId_ = roomId;
    roomSwitchPerfTimer_.restart();

    nhlog::ui()->info("[perf][room-switch] switch_id={} room_id={} phase=request reason={}",
                      roomSwitchPerfSwitchId_,
                      roomId.toStdString(),
                      reason.toStdString());
    nhlog::ui()->flush();
}

void
TimelineViewManager::markRoomSwitchPhaseCpp(const QString &roomId, const QString &phase)
{
    logRoomSwitchPhase(roomId, phase, "cpp");
}

void
TimelineViewManager::markRoomSwitchPhase(const QString &roomId, const QString &phase)
{
    logRoomSwitchPhase(roomId, phase, "qml");
}

bool
TimelineViewManager::perfUiFlagEnabled(const QString &flag) const
{
    if (flag == QLatin1String("disable_composer"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_COMPOSER"));

    if (flag == QLatin1String("disable_room_header"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_ROOM_HEADER"));

    if (flag == QLatin1String("disable_timeline_effects"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_TIMELINE_EFFECTS"));

    if (flag == QLatin1String("disable_timeline_list"))
        return isTruthyEnvValue(qgetenv("KOMAI_PERF_DISABLE_TIMELINE_LIST"));

    return false;
}

void
TimelineViewManager::logRoomSwitchPhase(const QString &roomId,
                                        const QString &phase,
                                        const QString &source)
{
    if (!roomSwitchPerfEnabled_ || roomId.isEmpty() || phase.isEmpty())
        return;

    const qint64 elapsedMs = roomSwitchPerfTimer_.isValid() ? roomSwitchPerfTimer_.elapsed() : -1;
    const bool activeMatch = roomId == roomSwitchPerfActiveRoomId_;

    if (activeMatch) {
        nhlog::ui()->info(
          "[perf][room-switch] switch_id={} room_id={} phase={} source={} elapsed_ms={}",
          roomSwitchPerfSwitchId_,
          roomId.toStdString(),
          phase.toStdString(),
          source.toStdString(),
          elapsedMs);
        nhlog::ui()->flush();
    } else {
        nhlog::ui()->info("[perf][room-switch] switch_id={} room_id={} phase={} source={} "
                          "elapsed_ms={} active_match=false active_room_id={}",
                          roomSwitchPerfSwitchId_,
                          roomId.toStdString(),
                          phase.toStdString(),
                          source.toStdString(),
                          elapsedMs,
                          roomSwitchPerfActiveRoomId_.toStdString());
        nhlog::ui()->flush();
    }
}

QAbstractItemModel *
TimelineViewManager::completerFor(const QString &completerName, const QString &roomId)
{
    if (completerName == QLatin1String("user")) {
        auto userModel = new UsersModel(roomId.toStdString());
        auto proxy     = new CompletionProxyModel(userModel);
        userModel->setParent(proxy);
        return proxy;
    } else if (completerName == QLatin1String("emoji")) {
        auto emojiModel = new CombinedImagePackModel(roomId.toStdString());
        auto proxy      = new CompletionProxyModel(emojiModel);
        emojiModel->setParent(proxy);
        return proxy;
    } else if (completerName == QLatin1String("room")) {
        auto roomModel = new RoomsModel(false);
        auto proxy     = new CompletionProxyModel(roomModel, 4);
        roomModel->setParent(proxy);
        return proxy;
    } else if (completerName == QLatin1String("roomAliases")) {
        auto roomModel = new RoomsModel(true);
        auto proxy     = new CompletionProxyModel(roomModel);
        roomModel->setParent(proxy);
        return proxy;
    } else if (completerName == QLatin1String("emojigrid")) {
        auto stickerModel = new GridImagePackModel(roomId.toStdString(), false);
        return stickerModel;
    } else if (completerName == QLatin1String("stickergrid")) {
        auto stickerModel = new GridImagePackModel(roomId.toStdString(), true);
        return stickerModel;
    } else if (completerName == QLatin1String("command")) {
        auto commandCompleter = new CommandCompleter();
        auto proxy            = new CompletionProxyModel(commandCompleter);
        commandCompleter->setParent(proxy);
        return proxy;
    }
    return nullptr;
}

using IgnoredUsers = mtx::events::EphemeralEvent<mtx::events::account_data::IgnoredUsers>;

static QVector<QString>
convertIgnoredToQt(const IgnoredUsers &ev)
{
    QVector<QString> users;
    for (const mtx::events::account_data::IgnoredUser &user : ev.content.users) {
        users.push_back(QString::fromStdString(user.id));
    }

    return users;
}

QVector<QString>
TimelineViewManager::getIgnoredUsers()
{
    const auto cache = cache::getAccountData(mtx::events::EventType::IgnoredUsers);
    if (!cache) {
        return {};
    }

    return convertIgnoredToQt(std::get<IgnoredUsers>(*cache));
}

void
TimelineViewManager::processIgnoredUsers(const mtx::responses::AccountData &data)
{
    for (const mtx::events::collections::RoomAccountDataEvents::variant &ev : data.events) {
        if (!std::holds_alternative<IgnoredUsers>(ev)) {
            continue;
        }
        const auto &ignoredEv = std::get<IgnoredUsers>(ev);

        emit this->ignoredUsersChanged(convertIgnoredToQt(ignoredEv));
        break;
    }
}
#include "moc_TimelineViewManager.cpp"

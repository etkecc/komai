// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/facade/UserSettingsPage.h"

bool
UserSettings::messageHoverHighlight() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesHoverHighlight);
        value.has_value())
        return *value;
    return messageHoverHighlight_;
}

bool
UserSettings::enlargeEmojiOnlyMessages() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesEmojiOnlyEnlarge);
        value.has_value())
        return *value;
    return enlargeEmojiOnlyMessages_;
}

bool
UserSettings::timelineMediaEffectsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaEffectsEnabled);
        value.has_value())
        return *value;
    return timelineMediaEffectsEnabled_;
}

bool
UserSettings::markdownEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerInputMarkdownEnabled);
        value.has_value())
        return *value;
    return markdownEnabled_;
}

UserSettings::SendMessageKey
UserSettings::sendMessageKey() const
{
    if (const auto value = coreStore_.valueAs<int>(settings::core::SettingId::ComposerInputSendKey);
        value.has_value() && *value >= static_cast<int>(UserSettings::SendMessageKey::Enter) &&
        *value <= static_cast<int>(UserSettings::SendMessageKey::CtrlEnter))
        return static_cast<UserSettings::SendMessageKey>(*value);
    return sendMessageKey_;
}

UserSettings::AutoReplaceEmoji
UserSettings::autoReplaceEmoji() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::ComposerInputAutoReplaceEmoji);
        value.has_value() && *value >= static_cast<int>(UserSettings::AutoReplaceEmoji::Always) &&
        *value <= static_cast<int>(UserSettings::AutoReplaceEmoji::Never))
        return static_cast<UserSettings::AutoReplaceEmoji>(*value);
    return autoReplaceEmoji_;
}

UserSettings::TimelineMessageLayout
UserSettings::timelineMessageLayout() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesLayoutStyle);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::TimelineMessageLayout::Minimal) &&
        *value <= static_cast<int>(UserSettings::TimelineMessageLayout::Bubbles))
        return static_cast<UserSettings::TimelineMessageLayout>(*value);
    return timelineMessageLayout_;
}

bool
UserSettings::timelineSmallAvatarsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesLayoutSmallAvatars);
        value.has_value())
        return *value;
    return timelineSmallAvatarsEnabled_;
}

bool
UserSettings::stickersEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerExtrasStickersEnabled);
        value.has_value())
        return *value;
    return stickersEnabled_;
}

bool
UserSettings::timelineShowOwnAvatarInBubbleLayout() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesLayoutShowOwnAvatar);
        value.has_value())
        return *value;
    return timelineShowOwnAvatarInBubbleLayout_;
}

QString
UserSettings::pinnedReactions() const
{
    if (const auto value = coreStore_.valueAs<std::string>(
          settings::core::SettingId::TimelineMessageActionsPinnedReactions);
        value.has_value())
        return QString::fromStdString(*value);
    return pinnedReactions_;
}

UserSettings::ShowSenderUsername
UserSettings::showSenderUsername() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesSenderUsername);
        value.has_value() && *value >= static_cast<int>(UserSettings::ShowSenderUsername::Always) &&
        *value <= static_cast<int>(UserSettings::ShowSenderUsername::Never))
        return static_cast<UserSettings::ShowSenderUsername>(*value);
    return showSenderUsername_;
}

int
UserSettings::showSenderUsernameLargeRoomThreshold() const
{
    return 16;
}

bool
UserSettings::animateImagesOnHover() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaAnimateOnHover);
        value.has_value())
        return *value;
    return animateImagesOnHover_;
}

bool
UserSettings::sendTypingNotificationsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerTypingSendEnabled);
        value.has_value())
        return *value;
    return sendTypingNotificationsEnabled_;
}

bool
UserSettings::showTypingNotificationsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineTypingShowEnabled);
        value.has_value())
        return *value;
    return showTypingNotificationsEnabled_;
}

UserSettings::TimelineMessageActionsPolicy
UserSettings::timelineMessageActionsPolicy() const
{
    if (const auto value = coreStore_.valueAs<int>(
          settings::core::SettingId::TimelineMessageActionsActivationPolicy);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::TimelineMessageActionsPolicy::OnHover) &&
        *value <= static_cast<int>(UserSettings::TimelineMessageActionsPolicy::Never))
        return static_cast<UserSettings::TimelineMessageActionsPolicy>(*value);
    return timelineMessageActionsPolicy_;
}

bool
UserSettings::readReceiptsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineReadReceiptsEnabled);
        value.has_value())
        return *value;
    return readReceiptsEnabled_;
}

int
UserSettings::maxTimelineWidth() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesMaxWidthPx);
        value.has_value())
        return *value;
    return maxTimelineWidth_;
}

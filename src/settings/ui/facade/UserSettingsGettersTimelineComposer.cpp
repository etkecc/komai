// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/facade/UserSettingsPage.h"

bool
UserSettings::timelineMessagesHoverHighlight() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesHoverHighlight);
        value.has_value())
        return *value;
    return timelineMessagesHoverHighlight_;
}

bool
UserSettings::timelineMessagesEmojiOnlyEnlarge() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesEmojiOnlyEnlarge);
        value.has_value())
        return *value;
    return timelineMessagesEmojiOnlyEnlarge_;
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
UserSettings::composerInputMarkdownEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerInputMarkdownEnabled);
        value.has_value())
        return *value;
    return composerInputMarkdownEnabled_;
}

UserSettings::SendMessageKey
UserSettings::composerInputSendKey() const
{
    if (const auto value = coreStore_.valueAs<int>(settings::core::SettingId::ComposerInputSendKey);
        value.has_value() && *value >= static_cast<int>(UserSettings::SendMessageKey::Enter) &&
        *value <= static_cast<int>(UserSettings::SendMessageKey::CtrlEnter))
        return static_cast<UserSettings::SendMessageKey>(*value);
    return composerInputSendKey_;
}

UserSettings::AutoReplaceEmoji
UserSettings::composerInputAutoReplaceEmoji() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::ComposerInputAutoReplaceEmoji);
        value.has_value() && *value >= static_cast<int>(UserSettings::AutoReplaceEmoji::Always) &&
        *value <= static_cast<int>(UserSettings::AutoReplaceEmoji::Never))
        return static_cast<UserSettings::AutoReplaceEmoji>(*value);
    return composerInputAutoReplaceEmoji_;
}

UserSettings::TimelineMessageLayout
UserSettings::timelineMessagesLayoutStyle() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesLayoutStyle);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::TimelineMessageLayout::Minimal) &&
        *value <= static_cast<int>(UserSettings::TimelineMessageLayout::Bubbles))
        return static_cast<UserSettings::TimelineMessageLayout>(*value);
    return timelineMessagesLayoutStyle_;
}

bool
UserSettings::timelineMessagesLayoutSmallAvatars() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesLayoutSmallAvatars);
        value.has_value())
        return *value;
    return timelineMessagesLayoutSmallAvatars_;
}

bool
UserSettings::composerExtrasStickersEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerExtrasStickersEnabled);
        value.has_value())
        return *value;
    return composerExtrasStickersEnabled_;
}

bool
UserSettings::timelineMessagesLayoutShowOwnAvatar() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesLayoutShowOwnAvatar);
        value.has_value())
        return *value;
    return timelineMessagesLayoutShowOwnAvatar_;
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
UserSettings::timelineMessagesSenderUsername() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesSenderUsername);
        value.has_value() && *value >= static_cast<int>(UserSettings::ShowSenderUsername::Always) &&
        *value <= static_cast<int>(UserSettings::ShowSenderUsername::Never))
        return static_cast<UserSettings::ShowSenderUsername>(*value);
    return timelineMessagesSenderUsername_;
}

int
UserSettings::timelineMessagesSenderUsernameLargeRoomThreshold() const
{
    return 16;
}

bool
UserSettings::timelineMediaAnimateOnHover() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaAnimateOnHover);
        value.has_value())
        return *value;
    return timelineMediaAnimateOnHover_;
}

bool
UserSettings::composerTypingSendEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerTypingSendEnabled);
        value.has_value())
        return *value;
    return composerTypingSendEnabled_;
}

bool
UserSettings::timelineTypingShowEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineTypingShowEnabled);
        value.has_value())
        return *value;
    return timelineTypingShowEnabled_;
}

UserSettings::TimelineMessageActionsPolicy
UserSettings::timelineMessageActionsActivationPolicy() const
{
    if (const auto value = coreStore_.valueAs<int>(
          settings::core::SettingId::TimelineMessageActionsActivationPolicy);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::TimelineMessageActionsPolicy::OnHover) &&
        *value <= static_cast<int>(UserSettings::TimelineMessageActionsPolicy::Never))
        return static_cast<UserSettings::TimelineMessageActionsPolicy>(*value);
    return timelineMessageActionsActivationPolicy_;
}

bool
UserSettings::timelineReadReceiptsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineReadReceiptsEnabled);
        value.has_value())
        return *value;
    return timelineReadReceiptsEnabled_;
}

int
UserSettings::timelineMessagesMaxWidthPx() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesMaxWidthPx);
        value.has_value())
        return *value;
    return timelineMessagesMaxWidthPx_;
}

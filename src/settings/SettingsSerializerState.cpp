// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include "komai-rust-cxxbridge/ffi.h"

#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::serializer {

void
stageState(const UserSettings &settings, ::komai::rust::SettingsProfileHandle &profileHandle)
{
    ::komai::rust::SettingsStateSnapshot snapshot{
      .window_width                  = settings.windowWidth(),
      .window_height                 = settings.windowHeight(),
      .sidebars_room_list_width_px   = settings.sidebarsRoomListWidthPx(),
      .sidebars_communities_width_px = settings.sidebarsCommunitiesWidthPx(),
      .current_filter_id             = settings.currentFilterId().toStdString(),
      .current_room_id               = settings.currentRoomId().toStdString(),
      .global_excludes               = {},
      .badges_hidden_filters         = {},
      .hidden_pins                   = {},
      .hidden_widgets                = {},
      .collapsed_spaces              = {},
      .composer_drafts_by_room       = {},
    };
    for (const auto &value : settings.globalExcludes())
        snapshot.global_excludes.push_back(value.toStdString());
    for (const auto &value : settings.badgesHiddenFilters())
        snapshot.badges_hidden_filters.push_back(value.toStdString());
    for (const auto &value : settings.hiddenPins())
        snapshot.hidden_pins.push_back(value.toStdString());
    for (const auto &value : settings.hiddenWidgets())
        snapshot.hidden_widgets.push_back(value.toStdString());
    for (const auto &value : settings.collapsedSpaces())
        snapshot.collapsed_spaces.push_back(value.toStdString());
    for (auto it = settings.composerDraftsByRoom().constBegin();
         it != settings.composerDraftsByRoom().constEnd();
         ++it) {
        snapshot.composer_drafts_by_room.push_back(
          {.key = it.key().toStdString(), .value = it.value().toStdString()});
    }

    ::komai::rust::settings_profile_replace_state_snapshot(profileHandle, snapshot);
}

} // namespace settings::serializer

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::config;

#[derive(Clone, Debug, Default)]
pub struct StartupSnapshot {
    pub ui_scale_factor: Option<f32>,
}

pub fn snapshot_from_config_text(config_text: &str) -> StartupSnapshot {
    let config = config::parse_config_text(config_text);

    StartupSnapshot {
        ui_scale_factor: config.ui.scale.factor,
    }
}

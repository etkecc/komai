// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pub mod logging;
pub mod matrix_backend;
pub mod settings;
pub mod theme;
pub(crate) mod ffi;

pub(crate) use crate::ffi::{
    into_ffi_matrix_notification_item,
    into_ffi_matrix_room_summary,
    runtime,
};

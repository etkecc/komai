// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "rust/cxx.h"

namespace komai::rust_bridge {

rust::String
matrix_profile_data_root(rust::Str profile_id);

rust::String
matrix_profile_cache_root(rust::Str profile_id);

rust::String
matrix_storage_user_component(rust::Str profile_id, rust::Str user_id);

} // namespace komai::rust_bridge

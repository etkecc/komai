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
matrix_store_passphrase(rust::Str profile_id);

rust::String
matrix_homeserver_url(rust::Str profile_id);

rust::String
matrix_serialized_session(rust::Str profile_id);

void
matrix_save_session_secrets(rust::Str profile_id,
                            rust::Str store_passphrase,
                            rust::Str homeserver_url,
                            rust::Str serialized_session);

void
matrix_clear_session_secrets(rust::Str profile_id);

} // namespace komai::rust_bridge

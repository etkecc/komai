// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "komai-rust-cxxbridge/ffi.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"

namespace komai {

MatrixRoomSummary
fromFfiRoomSummary(const ::komai::rust::MatrixRoomSummary &room);

} // namespace komai

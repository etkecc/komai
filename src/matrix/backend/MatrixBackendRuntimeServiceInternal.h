// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "matrix/backend/MatrixBlockingCall.h"

namespace komai {

template<typename Func>
auto
invokeRuntimeWorkerCall(const char *operation, Func &&func)
{
    return matrix_backend::invokeBlockingCall(
      operation,
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      std::forward<Func>(func));
}

} // namespace komai

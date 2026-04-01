// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "komai-rust-cxxbridge/ffi.h"
#include "matrix/backend/MatrixBlockingCall.h"

namespace komai::matrix_backend {

// Exported blocking Rust FFI entrypoints take this generated struct as their first argument.
// Keep the conversion here so callers pass one explicit policy object instead of rebuilding the
// Rust bridge details at each site.
[[nodiscard]] inline ::komai::rust::MatrixFfiBlockingThreadPolicy
toRustBlockingThreadPolicy(BlockingCallThreadPolicy policy)
{
    return policy == BlockingCallThreadPolicy::RequireWorkerThread
             ? ::komai::rust::MatrixFfiBlockingThreadPolicy::RequireWorkerThread
             : ::komai::rust::MatrixFfiBlockingThreadPolicy::AllowUiThread;
}

[[nodiscard]] inline ::komai::rust::MatrixFfiCallerThread
toRustCallerThread(BlockingCallCallerThread callerThread)
{
    return callerThread == BlockingCallCallerThread::AppUiThread
             ? ::komai::rust::MatrixFfiCallerThread::AppUiThread
             : ::komai::rust::MatrixFfiCallerThread::WorkerThread;
}

[[nodiscard]] inline ::komai::rust::MatrixFfiBlockingContext
toRustBlockingContext(const BlockingCallContext &context)
{
    return ::komai::rust::MatrixFfiBlockingContext{
      .thread_policy = toRustBlockingThreadPolicy(context.threadPolicy()),
      .caller_thread = toRustCallerThread(context.callerThread()),
    };
}

} // namespace komai::matrix_backend

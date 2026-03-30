// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QCoreApplication>
#include <QString>
#include <QThread>
#include <QtGlobal>

#include <type_traits>
#include <utility>

#include "logging/Logging.h"

namespace komai::matrix_backend {

enum class BlockingCallThreadPolicy
{
    AllowUiThread,
    RequireWorkerThread,
};

enum class BlockingCallCallerThread
{
    AppUiThread,
    WorkerThread,
};

class BlockingCallContext
{
public:
    BlockingCallContext(const BlockingCallContext &)            = default;
    BlockingCallContext &operator=(const BlockingCallContext &) = default;

    [[nodiscard]] BlockingCallThreadPolicy threadPolicy() const { return policy_; }
    [[nodiscard]] BlockingCallCallerThread callerThread() const { return callerThread_; }

private:
    // This context is carried from the C++ caller to the Rust FFI seam so the blocking policy is
    // explicit instead of being inferred ad hoc at each bridge call.
    explicit BlockingCallContext(BlockingCallThreadPolicy policy,
                                 BlockingCallCallerThread callerThread)
      : policy_(policy)
      , callerThread_(callerThread)
    {
    }

    BlockingCallThreadPolicy policy_;
    BlockingCallCallerThread callerThread_;

    friend BlockingCallContext blockingCallContext();
    friend BlockingCallContext allowUiThreadBlockingCallContext();
};

inline bool
isAppUiThread()
{
    auto *app = QCoreApplication::instance();
    return app && QThread::currentThread() == app->thread();
}

inline void
logUiThreadBlockingCall(const char *operation, const char *message)
{
    if (auto logger = nhlog::ui(); logger) {
        logger->error("Blocking matrix-sdk call '{}' {}", operation, message);
    }
}

[[noreturn]] inline void
failUiThreadBlockingCall(const char *operation)
{
    const auto detail =
      QStringLiteral("was invoked on the app/UI thread. Move this call to a worker-thread path "
                     "instead of entering Rust synchronously from the UI.");
    const auto detailStd = detail.toStdString();
    logUiThreadBlockingCall(operation, detailStd.c_str());
    qFatal("Blocking matrix-sdk call '%s' was invoked on the app/UI thread", operation);
}

[[nodiscard]] inline BlockingCallContext
blockingCallContext()
{
    // Worker-only blocking callers must opt in explicitly and fail loudly if they are still on the
    // app/UI thread. This turns stale direct callers into obvious breakage instead of later hangs.
    if (isAppUiThread()) {
        logUiThreadBlockingCall("blockingCallContext", "was created on the app/UI thread");
        qFatal("Blocking matrix-sdk call context was created on the app/UI thread");
    }

    return BlockingCallContext{
      BlockingCallThreadPolicy::RequireWorkerThread,
      BlockingCallCallerThread::WorkerThread,
    };
}

[[nodiscard]] inline BlockingCallContext
allowUiThreadBlockingCallContext()
{
    // This is a temporary escape hatch for synchronous UI-thread callers that have not moved to a
    // worker path yet. Keeping it explicit makes those remaining debt sites easy to audit later.
    return BlockingCallContext{
      BlockingCallThreadPolicy::AllowUiThread,
      isAppUiThread() ? BlockingCallCallerThread::AppUiThread
                      : BlockingCallCallerThread::WorkerThread,
    };
}

template<typename Func>
auto
invokeBlockingCall(const char *operation, BlockingCallThreadPolicy policy, Func &&func)
{
    using Result = std::invoke_result_t<Func>;

    if (policy == BlockingCallThreadPolicy::RequireWorkerThread && isAppUiThread())
        failUiThreadBlockingCall(operation);

    if constexpr (std::is_void_v<Result>) {
        std::forward<Func>(func)();
        return;
    } else {
        return std::forward<Func>(func)();
    }
}

} // namespace komai::matrix_backend

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

class BlockingCallContext
{
public:
    BlockingCallContext(const BlockingCallContext &)            = default;
    BlockingCallContext &operator=(const BlockingCallContext &) = default;

private:
    explicit BlockingCallContext(int) {}

    friend BlockingCallContext blockingCallContext();
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
    if (isAppUiThread()) {
        logUiThreadBlockingCall("blockingCallContext", "was created on the app/UI thread");
        qFatal("Blocking matrix-sdk call context was created on the app/UI thread");
    }

    return BlockingCallContext{0};
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

// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMetaObject>
#include <QPointer>

#include <thread>
#include <type_traits>
#include <utility>

namespace komai::qt_worker_task {

template<typename ObjectT, typename WorkFnT, typename UiFnT>
void
runQueued(ObjectT *context, WorkFnT work, UiFnT ui)
{
    QPointer<ObjectT> guard(context);
    std::thread([guard, work = std::move(work), ui = std::move(ui)]() mutable {
        using Result = std::invoke_result_t<WorkFnT &>;

        if constexpr (std::is_void_v<Result>) {
            work();

            if (!guard)
                return;

            QMetaObject::invokeMethod(
              guard,
              [guard, ui = std::move(ui)]() mutable {
                  if (!guard)
                      return;

                  ui(guard.data());
              },
              Qt::QueuedConnection);
        } else {
            auto result = work();

            if (!guard)
                return;

            QMetaObject::invokeMethod(
              guard,
              [guard, result = std::move(result), ui = std::move(ui)]() mutable {
                  if (!guard)
                      return;

                  ui(guard.data(), std::move(result));
              },
              Qt::QueuedConnection);
        }
    }).detach();
}

} // namespace komai::qt_worker_task

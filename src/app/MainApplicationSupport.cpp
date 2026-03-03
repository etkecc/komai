// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/MainApplicationSupport.h"

#include <stdexcept>

#include <QApplication>
#include <QDir>

#if defined(GSTREAMER_AVAILABLE) && (defined(Q_OS_MACOS) || defined(Q_OS_WINDOWS))
#include <QAbstractEventDispatcher>
#include <cstring>
#include <gst/gst.h>

namespace {
GMainLoop *gloop = nullptr;
GThread *gthread = nullptr;

extern "C"
{
    static gpointer glibMainLoopThreadFunc(gpointer)
    {
        gloop = g_main_loop_new(nullptr, false);
        g_main_loop_run(gloop);
        g_main_loop_unref(gloop);
        gloop = nullptr;
        return nullptr;
    }
} // extern "C"
} // namespace
#endif

#if HAVE_BACKTRACE_SYMBOLS_FD
#include <csignal>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>

namespace {
void
stacktraceHandler(int signum)
{
    std::signal(signum, SIG_DFL);

    // boost::stacktrace::safe_dump_to("./komai-backtrace.dump");

    // see
    // https://stackoverflow.com/questions/77005/how-to-automatically-generate-a-stacktrace-when-my-program-crashes/77336#77336
    void *array[50];
    int size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 50);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", signum);
    backtrace_symbols_fd(array, size, STDERR_FILENO);

    int file = ::open("/tmp/komai-crash.dump",
                      O_CREAT | O_WRONLY | O_TRUNC
#if defined(S_IWUSR) && defined(S_IRUSR)
                      ,
                      S_IWUSR | S_IRUSR
#elif defined(S_IWRITE) && defined(S_IREAD)
                      ,
                      S_IWRITE | S_IREAD
#endif
    );
    if (file != -1) {
        constexpr char header[]   = "Error: signal\n";
        [[maybe_unused]] auto ret = write(file, header, std::size(header) - 1);
        backtrace_symbols_fd(array, size, file);
        close(file);
    }

    std::raise(SIGABRT);
}
} // namespace
#endif

namespace app::support {

SelectedProfileArg
selectedProfileFromArgs(int argc, char *argv[])
{
    SelectedProfileArg result;

    for (int i = 1; i < argc; ++i) {
        const QString arg{argv[i]};
        if (arg == QLatin1String("-p") || arg == QLatin1String("--profile")) {
            result.provided = true;
            if (i + 1 < argc)
                result.value = QString{argv[i + 1]};
            return result;
        }

        if (arg.startsWith(QLatin1String("--profile="))) {
            result.provided = true;
            result.value    = arg.sliced(QStringLiteral("--profile=").size());
            return result;
        }

        if (arg.size() > 2 && arg.startsWith(QLatin1String("-p"))) {
            result.provided = true;
            result.value    = arg.sliced(2);
            return result;
        }
    }

    return result;
}

void
createDirectory(const QString &dir)
{
    if (!QDir().mkpath(dir)) {
        throw std::runtime_error(("Unable to create state directory:" + dir).toStdString().c_str());
    }
}

void
registerSignalHandlers()
{
#if HAVE_BACKTRACE_SYMBOLS_FD
    std::signal(SIGSEGV, &stacktraceHandler);
    std::signal(SIGABRT, &stacktraceHandler);
#endif
}

void
initializeGstreamerEventLoopIfNeeded(QApplication &app)
{
#if defined(GSTREAMER_AVAILABLE) && (defined(Q_OS_MACOS) || defined(Q_OS_WINDOWS))
    // If the version of Qt we're running in does not use GLib, we need to
    // start a GMainLoop so that gstreamer can dispatch events.
    const QMetaObject *mo = QAbstractEventDispatcher::instance(app.thread())->metaObject();
    if (gloop == nullptr && strcmp(mo->className(), "QEventDispatcherGlib") != 0 &&
        strcmp(mo->superClass()->className(), "QEventDispatcherGlib") != 0) {
        gthread = g_thread_new(nullptr, glibMainLoopThreadFunc, nullptr);
    }
#else
    (void)app;
#endif
}

} // namespace app::support

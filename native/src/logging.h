#pragma once
#include <android/log.h>
#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <time.h>
#include <unistd.h>

#ifndef BACKFIX_STATUS_SERVER
#define BACKFIX_STATUS_SERVER 1
#endif

namespace logging {
#if BACKFIX_STATUS_SERVER
inline std::atomic_bool debugEnabled{false};
inline std::mutex debugMutex;
inline bool isDebugEnabled() { return debugEnabled.load(std::memory_order_relaxed); }
inline void setDebugEnabled(bool enabled) {
    // A disable acknowledgement also waits for any in-flight debug write.
    std::lock_guard<std::mutex> lock(debugMutex);
    debugEnabled.store(enabled, std::memory_order_relaxed);
}
#else
inline constexpr bool isDebugEnabled() { return false; }
#endif

inline void writeMessage(int priority, const char *format, va_list args) {
    const int savedErrno = errno;
    char message[1536];
    vsnprintf(message, sizeof(message), format, args);
    static const auto session = [] {
        timespec now{};
        clock_gettime(CLOCK_BOOTTIME, &now);
        return static_cast<unsigned long long>(now.tv_sec) * 1000000000 + now.tv_nsec;
    }();
    static std::atomic<unsigned long long> sequence{0};
    // LSPosed 2.2.1 parses this module envelope before collecting the message.
    // Keep each entry below its 3000-byte chunk limit; no ART/JNI is involved.
    __android_log_print(priority, "LSPosedFramework",
            "(com.miui.home)[dev.scrpr.hyosbackfix,HyosBackFixNative,%x-%llx-native-%llx,0,1] %s",
            getpid(), session, sequence.fetch_add(1, std::memory_order_relaxed), message);
    errno = savedErrno;
}

inline void installation(int priority, const char *format, ...) __attribute__((format(printf, 2, 3)));
inline void installation(int priority, const char *format, ...) {
    va_list args;
    va_start(args, format);
    writeMessage(priority, format, args);
    va_end(args);
}

inline void debug(int priority, const char *format, ...) __attribute__((format(printf, 2, 3)));
inline void debug(int priority, const char *format, ...) {
#if BACKFIX_STATUS_SERVER
    std::lock_guard<std::mutex> lock(debugMutex);
    if (!isDebugEnabled()) return;
    va_list args;
    va_start(args, format);
    writeMessage(priority, format, args);
    va_end(args);
#else
    (void)priority;
    (void)format;
#endif
}
}

// Do not evaluate diagnostic-only state reads while logging is disabled.
// The constant false condition also strips strings and calls from no-GUI builds.
#define BACKFIX_DEBUG_LOG(...) do { if (logging::isDebugEnabled()) logging::debug(__VA_ARGS__); } while (0)

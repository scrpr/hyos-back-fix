#pragma once
#include "logging.h"
#include "no_reuse.h"
#include <unistd.h>

#ifndef BACKFIX_STATUS_SERVER
#define BACKFIX_STATUS_SERVER 1
#endif

#if BACKFIX_STATUS_SERVER
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#endif

// Process-owned diagnostics and GUI settings. No JNI or root required.
// Keep this protocol and port calculation in sync with NativeStatusClient.kt.
namespace diagnostics {
#if BACKFIX_STATUS_SERVER
inline std::mutex snapshotMutex;
inline std::string snapshot;

inline long long elapsedMillis() {
    timespec now{};
    clock_gettime(CLOCK_BOOTTIME, &now);
    return static_cast<long long>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
}
#endif

inline void publish(const char *stage, bool guard, bool remote, bool remoteBack,
                    bool merge, size_t pages, bool noReuse) {
#if BACKFIX_STATUS_SERVER
    char report[1024];
    snprintf(report, sizeof(report),
            "version=" MODULE_VERSION "\nversionCode=%d\nuid=%u\npid=%d\nstage=%s\n"
            "guard=%d\nremote=%d\nremoteBack=%d\nmerge=%d\npages=%zu\npageSize=%ld\nnoReuse=%d\n",
            MODULE_VERSION_CODE, getuid(), getpid(), stage, guard, remote, remoteBack,
            merge, pages, sysconf(_SC_PAGESIZE), noReuse);
    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        snapshot = report;
    }
#endif
    logging::installation(ANDROID_LOG_INFO,
            "status pid=%d version=" MODULE_VERSION " stage=%s guard=%d remote=%d remoteBack=%d merge=%d pages=%zu pageSize=%ld noReuse=%d",
            getpid(), stage, guard, remote, remoteBack, merge, pages, sysconf(_SC_PAGESIZE), noReuse);
}

#if BACKFIX_STATUS_SERVER
inline void serveClient(int client) {
    // One bounded request/reply. R/r persistently enable/disable launch-reuse prevention.
    timeval timeout{0, 250000};
    if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) ||
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout))) return;
    constexpr char prefix[] = "HYOS_BACK_FIX_STATUS/2 ";
    // A 32-digit nonce, space, then '?' (query), '0'/'1' (debug), or 'r'/'R' (reuse).
    constexpr size_t length = sizeof(prefix) - 1 + 32 + 3;
    char request[length + 1]{};
    size_t received = 0;
    auto deadline = elapsedMillis() + 500;
    while (received < length && elapsedMillis() < deadline) {
        auto count = recv(client, request + received, length - received, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return;
        received += count;
    }
    if (received != length || strncmp(request, prefix, sizeof(prefix) - 1) ||
        request[length - 1] != '\n') return;
    for (size_t i = sizeof(prefix) - 1; i < sizeof(prefix) - 1 + 32; ++i)
        if (!((request[i] >= '0' && request[i] <= '9') ||
              (request[i] >= 'a' && request[i] <= 'f'))) return;
    const char action = request[length - 2];
    if (request[length - 3] != ' ' || (action != '?' && action != '0' && action != '1'
            && action != 'r' && action != 'R')) return;
    if (action == '0' || action == '1') logging::setDebugEnabled(action == '1');
    if (action == 'r' || action == 'R') window_reuse::setEnabled(action == 'R');
    // Echo only the protocol and nonce; the resulting state is in debugLogging.
    std::string reply(request, sizeof(prefix) - 1 + 32);
    reply += '\n';
    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        reply += snapshot;
    }
    reply += std::string("debugLogging=") + (logging::isDebugEnabled() ? "1\n" : "0\n");
    reply += "noReuseEnabled=" + std::to_string(noReuseEnabled.load(std::memory_order_acquire)) + "\n";
    reply += "elapsedMs=" + std::to_string(elapsedMillis()) + "\nend\n";
    size_t sent = 0;
    while (sent < reply.size()) {
        auto count = send(client, reply.data() + sent, reply.size() - sent, MSG_NOSIGNAL);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return;
        sent += count;
    }
}

inline void start() {
    // Android user profiles get separate endpoints. Never bind INADDR_ANY.
    unsigned user = getuid() / 100000;
    if (user > 22775) return;
    std::thread([user] {
        int listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (listener < 0) return;
        int reuse = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(42760 + user);
        if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) ||
            listen(listener, 4)) {
            BACKFIX_DEBUG_LOG(ANDROID_LOG_WARN,
                    "diagnostics endpoint unavailable errno=%d", errno);
            close(listener);
            return; // Diagnostics availability never changes installation.
        }
        for (;;) {
            int client = accept4(listener, nullptr, nullptr, SOCK_CLOEXEC);
            if (client < 0) {
                if (errno == EINTR || errno == ECONNABORTED) continue;
                break;
            }
            serveClient(client);
            close(client);
        }
        close(listener);
    }).detach();
}
#else
inline void start() {} // No status endpoint or diagnostics thread in the minimal APK.
#endif
}

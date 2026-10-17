#pragma once
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>

// Native diagnostics only: logcat plus a private snapshot for manual inspection.
// A snapshot can outlive its process; PID/start identify which process wrote it.
namespace diagnostics {
inline void publish(const char *stage, bool guard, bool remote, bool remoteBack,
                    bool merge, size_t pages) {
    __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative",
            "status pid=%d version=" MODULE_VERSION " stage=%s guard=%d remote=%d remoteBack=%d merge=%d pages=%zu pageSize=%ld",
            getpid(), stage, guard, remote, remoteBack, merge, pages, sysconf(_SC_PAGESIZE));
    char stat[4096]{};
    FILE *file = fopen("/proc/self/stat", "r");
    if (!file) return;
    auto *read = fgets(stat, sizeof(stat), file);
    fclose(file);
    if (!read) return;
    char *cursor = strrchr(stat, ')') + 2, *save = nullptr, *start = nullptr;
    for (int field = 3; field <= 22; ++field)
        start = strtok_r(field == 3 ? cursor : nullptr, " ", &save);
    char directory[192], path[256], temporary[272];
    snprintf(directory, sizeof(directory), "/data/user/%u/com.miui.home/files", getuid()/100000);
    mkdir(directory, 0700);
    snprintf(path, sizeof(path), "%s/hyos-back-fix.status", directory);
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    file = fopen(temporary, "w");
    if (!file) return; // Diagnostics availability never changes native behavior.
    fprintf(file, "pid=%d\nstart=%s\nversion=" MODULE_VERSION "\nstage=%s\nguard=%d\nremote=%d\nremoteBack=%d\nmerge=%d\npages=%zu\npageSize=%ld\n",
            getpid(), start, stage, guard, remote, remoteBack, merge, pages, sysconf(_SC_PAGESIZE));
    fclose(file);
    rename(temporary, path);
}
}

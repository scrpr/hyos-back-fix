#pragma once
#include <atomic>
#include <unistd.h>
#include "symbols.h"

extern "C" {
#if BACKFIX_STATUS_SERVER
extern std::atomic<uint32_t> noReuseEnabled;
extern uintptr_t originalNoReuse;
#endif
void disableWindowReuse();
}

namespace window_reuse {
inline uintptr_t findLoaded() {
    FILE *maps = fopen("/proc/self/maps", "r");
    if (!maps) return 0;
    char line[2048], permissions[5], path[1024];
    uintptr_t begin, end, offset, result = 0;
    while (fgets(line, sizeof(line), maps)) {
        if (sscanf(line, "%lx-%lx %4s %lx %*s %*s %1023s", &begin, &end,
                   permissions, &offset, path) != 5 || permissions[2] != 'x') continue;
        std::string image(path);
        if (image.ends_with(".apk")) image += "!/lib/arm64-v8a/libapp.so";
        else if (!image.ends_with("/libapp.so")) continue;
        Symbols symbols(0, image.c_str());
        auto target = symbols.find({"AppLauncherHelper.shouldReuseExistingWindow"}, true);
        if (!target.address) continue;
        auto position = symbols.fileOffset(target);
        if (position >= offset && position-offset < end-begin) {
            result = begin+position-offset;
            break;
        }
    }
    fclose(maps);
    return result;
}

#if BACKFIX_STATUS_SERVER
inline std::string configPath() {
    return "/data/user/" + std::to_string(getuid()/100000)
        + "/com.miui.home/files/hyos-back-fix-window-reuse";
}
inline void loadConfig() {
    if (FILE *file = fopen(configPath().c_str(), "r")) {
        noReuseEnabled.store(fgetc(file) != '0');
        fclose(file);
    }
}
inline void setEnabled(bool enabled) {
    FILE *file = fopen(configPath().c_str(), "w");
    if (!file) return;
    bool saved = fputc(enabled ? '1' : '0', file) != EOF;
    if (fclose(file)) saved = false;
    if (saved) noReuseEnabled.store(enabled);
}
#endif
}

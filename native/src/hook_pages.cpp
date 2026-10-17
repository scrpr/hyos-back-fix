#include "hook_pages.h"
#include "reclaim_ranges.h"
#include <android/log.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sys/mman.h>
#include <unistd.h>

namespace hook_pages {
namespace {
using Madvise = int (*)(void *, size_t, int);
std::atomic<Madvise> originalMadvise;
struct PageState {
    std::mutex lock;
    uintptr_t pages[6]{};
    size_t count = 0;
    const size_t size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
};
PageState state;

uintptr_t pageStart(uintptr_t address) {
    return address & ~(uintptr_t(state.size)-1);
}

int guardedMadvise(void *address, size_t length, int advice) {
    auto original = originalMadvise.load(std::memory_order_acquire);
    uintptr_t begin = reinterpret_cast<uintptr_t>(address);
    // Preserve the syscall's validation and all other advice operations.
    if (advice != MADV_DONTNEED || !length || begin%state.size || length > UINTPTR_MAX-begin)
        return original(address, length, advice);

    std::unique_lock<std::mutex> lock(state.lock);
    unsigned skipped = 0;
    int result = reclaimRanges(begin, length, state.size, state.pages, state.count,
            [&](uintptr_t at, size_t size) { return original(reinterpret_cast<void *>(at), size, advice); }, skipped);
    if (result != 0) return result;
    lock.unlock();
    if (skipped)
        __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative",
                "reclaim preserved hook_pages=%u address=%p length=%zu", skipped, address, length);
    return 0;
}

int protectionAt(uintptr_t address) {
    FILE *maps = fopen("/proc/self/maps", "r");
    char line[1024], permissions[5];
    uintptr_t begin, end;
    int protection = 0;
    while (fgets(line, sizeof(line), maps)) {
        if (sscanf(line, "%lx-%lx %4s", &begin, &end, permissions) != 3) continue;
        if (address < begin || address >= end) continue;
        if (permissions[0] == 'r') protection |= PROT_READ;
        if (permissions[1] == 'w') protection |= PROT_WRITE;
        if (permissions[2] == 'x') protection |= PROT_EXEC;
        break;
    }
    fclose(maps);
    return protection;
}
}

bool install(const dl_phdr_info &image) {
    const ElfW(Dyn) *dynamic = nullptr;
    for (size_t i=0; i<image.dlpi_phnum; ++i)
        if (image.dlpi_phdr[i].p_type == PT_DYNAMIC)
            dynamic = reinterpret_cast<const ElfW(Dyn) *>(image.dlpi_addr+image.dlpi_phdr[i].p_vaddr);
    const ElfW(Sym) *symbols = nullptr;
    const char *strings = nullptr;
    const ElfW(Rela) *relocations = nullptr;
    size_t relocationBytes = 0;
    if (!dynamic) return false;
    for (auto *entry=dynamic; entry->d_tag != DT_NULL; ++entry) {
        uintptr_t address = image.dlpi_addr+entry->d_un.d_ptr;
        switch (entry->d_tag) {
            case DT_SYMTAB: symbols = reinterpret_cast<const ElfW(Sym) *>(address); break;
            case DT_STRTAB: strings = reinterpret_cast<const char *>(address); break;
            case DT_JMPREL: relocations = reinterpret_cast<const ElfW(Rela) *>(address); break;
            case DT_PLTRELSZ: relocationBytes = entry->d_un.d_val; break;
        }
    }
    if (!symbols || !strings || !relocations) return false;
    for (size_t i=0; i<relocationBytes/sizeof(ElfW(Rela)); ++i) {
        const auto &entry = relocations[i];
        if (ELF64_R_TYPE(entry.r_info) != R_AARCH64_JUMP_SLOT
                || strcmp(strings+symbols[ELF64_R_SYM(entry.r_info)].st_name, "madvise")) continue;
        auto *slot = reinterpret_cast<Madvise *>(image.dlpi_addr+entry.r_offset);
        uintptr_t page = pageStart(reinterpret_cast<uintptr_t>(slot));
        int protection = protectionAt(reinterpret_cast<uintptr_t>(slot));
        if (!protection || mprotect(reinterpret_cast<void *>(page), state.size, protection|PROT_WRITE) != 0)
            return false;
        originalMadvise.store(__atomic_load_n(slot, __ATOMIC_ACQUIRE), std::memory_order_release);
        __atomic_store_n(slot, guardedMadvise, __ATOMIC_RELEASE);
        if (mprotect(reinterpret_cast<void *>(page), state.size, protection) != 0) return false;
        // The runtime's import slot is in data/RELRO, outside its r-x code reclamation.
        __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative",
                "reclaim guard installed slot=%p page_size=%zu", static_cast<void *>(slot), state.size);
        return true;
    }
    errno = ENOENT;
    return false;
}

int hook(InlineHook installer, void *target, void *replacement, void **original) {
    // Serialize patch installation with in-flight reclamation and publish before writing.
    std::lock_guard<std::mutex> lock(state.lock);
    uintptr_t begin = reinterpret_cast<uintptr_t>(target);
    for (uintptr_t page=pageStart(begin); page<=pageStart(begin+31); page+=state.size) {
        if (std::find(state.pages, state.pages+state.count, page) == state.pages+state.count)
            state.pages[state.count++] = page;
    }
    return installer(target, replacement, original);
}

size_t count() {
    std::lock_guard<std::mutex> lock(state.lock);
    return state.count;
}

}

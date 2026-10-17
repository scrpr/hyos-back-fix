#pragma once
#include <cstddef>
#include <cstdint>
#include <link.h>

namespace hook_pages {
using InlineHook = int (*)(void *, void *, void **);
using InlineUnhook = int (*)(void *);
bool install(const dl_phdr_info &image);
int hook(InlineHook installer, void *target, void *replacement, void **original);
int unhook(InlineUnhook remover, void *target);
bool uninstall();
void adoptState(uintptr_t address);
uintptr_t stateAddress();
size_t count();
}

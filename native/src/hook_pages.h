#pragma once
#include <cstddef>
#include <cstdint>
#include <link.h>

namespace hook_pages {
using InlineHook = int (*)(void *, void *, void **);
bool install(const dl_phdr_info &image);
int hook(InlineHook installer, void *target, void *replacement, void **original);
size_t count();
}

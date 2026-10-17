#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace hook_pages {
// Caller holds the same lock used to publish inline patch pages.
template<class Reclaim>
int reclaimRanges(uintptr_t begin, size_t length, size_t pageSize,
                  const uintptr_t *pages, size_t pageCount, Reclaim reclaim, unsigned &skipped) {
    uintptr_t end = begin + length, cursor = begin;
    while (cursor < end) {
        uintptr_t next = end;
        for (size_t i = 0; i < pageCount; ++i)
            if (pages[i] >= cursor && pages[i] < next) next = pages[i];
        if (next == end) break;
        if (next > cursor && reclaim(cursor, next - cursor) != 0) return -1;
        cursor = end - next < pageSize ? end : next + pageSize;
        ++skipped;
    }
    return cursor < end ? reclaim(cursor, end - cursor) : 0;
}
}

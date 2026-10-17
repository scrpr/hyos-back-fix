#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>

struct MergeBusiness {
    uintptr_t address;
    int32_t interruptOffset;
};

namespace aarch64 {
constexpr unsigned rd(uint32_t op) { return op & 31; }
constexpr unsigned rn(uint32_t op) { return (op >> 5) & 31; }
constexpr int offset(uint32_t op) { return int32_t(op << 11) >> 23; }
constexpr bool addImmediate(uint32_t op, unsigned dest, unsigned source, unsigned value) {
    return (op & 0xffc00000) == 0x91000000 && rd(op) == dest && rn(op) == source
            && ((op >> 10) & 4095) == value;
}
constexpr bool moveImmediate(uint32_t op, unsigned dest, unsigned value) {
    // MOVZ Wn/Xn: tolerate the compiler's choice of register width.
    return (op & 0x7fe00000) == 0x52800000 && rd(op) == dest && ((op >> 5) & 65535) == value;
}
constexpr bool frame(uint32_t op) {
    return (op & 0xffc00000) == 0xa9800000 && rd(op) == 29
            && ((op >> 10) & 31) == 30 && (rn(op) == 15 || rn(op) == 31);
}
constexpr bool loadField(uint32_t op) {
    return (op & 0xffe00c00) == 0xb8400000 && rn(op) == 0 && rd(op) == 1;
}
constexpr bool decompress(uint32_t op) {
    return (op & 0xff200000) == 0x8b000000 && rd(op) == 1 && rn(op) == 1
            && ((op >> 16) & 31) == 28 && ((op >> 22) & 3) == 0 && ((op >> 10) & 63) == 32;
}
constexpr bool branchTrue(uint32_t op) {
    return (op & 0xff000000) == 0x37000000 && rd(op) == 1 && ((op >> 19) & 31) == 4;
}
constexpr bool storeFlag(uint32_t op) {
    return (op & 0xffe00c00) == 0xb8000000 && rn(op) == 0 && rd(op) == 1;
}
}

// Decode the business operation rather than compare a version's full bytes:
// OPEN_NEAR_FULL (3), false, false; within that Dart frame, load/decompress/test
// the interrupt flag, then mark merge handoff in another field of that instance.
// x0/x1 and x22/x28 are merge_business.S's ABI.
inline MergeBusiness findMergeBusiness(uintptr_t start, uintptr_t finish) {
    using namespace aarch64;
    auto *begin = reinterpret_cast<const uint32_t *>(start);
    auto *end = reinterpret_cast<const uint32_t *>(finish);
    for (auto *tail = begin; tail + 2 < end; ++tail) {
        if (!moveImmediate(tail[0], 2, 3) || !addImmediate(tail[1], 3, 22, 48)
                || !addImmediate(tail[2], 5, 22, 48)) continue;
        auto *entry = tail;
        while (entry > begin && !frame(*entry)) --entry;
        for (auto *field = entry; field + 4 < tail; ++field) {
            if (loadField(field[0]) && decompress(field[1]) && branchTrue(field[2])
                    && addImmediate(field[3], 1, 22, 32) && storeFlag(field[4]))
                return {reinterpret_cast<uintptr_t>(field), offset(field[0])};
        }
    }
    return {};
}

// Flutter can map its AOT snapshot independently of ld.so, including from APK.
inline MergeBusiness findLoadedMergeBusiness() {
    FILE *maps = fopen("/proc/self/maps", "r");
    char line[1024], permissions[5];
    uintptr_t begin, end;
    MergeBusiness business{};
    while (fgets(line, sizeof(line), maps)) {
        if (!strstr(line, "libapp.so") && !strstr(line, ".apk")) continue;
        sscanf(line, "%lx-%lx %4s", &begin, &end, permissions);
        if (permissions[0] != 'r' || permissions[2] != 'x') continue;
        business = findMergeBusiness(begin, end);
        if (business.address) break;
    }
    fclose(maps);
    return business;
}

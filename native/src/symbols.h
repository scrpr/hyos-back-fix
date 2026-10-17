#pragma once
#include <elf.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>
#include "xz.h"

// The linker names an uncompressed APK library as APK!/entry.
inline std::vector<uint8_t> readImage(const char *path) {
    std::string name(path);
    size_t separator = name.find("!/");
    FILE *file = fopen(name.substr(0, separator).c_str(), "rb");
    fseeko(file, 0, SEEK_END);
    size_t size = ftello(file);
    std::vector<uint8_t> data(size);
    rewind(file);
    fread(data.data(), 1, size, file);
    fclose(file);
    if (separator == std::string::npos) return data;
    auto u16 = [&](size_t p) { uint16_t n; memcpy(&n, data.data()+p, 2); return n; };
    auto u32 = [&](size_t p) { uint32_t n; memcpy(&n, data.data()+p, 4); return n; };
    size_t end = size-22;
    while (u32(end) != 0x06054b50) --end;
    std::string entry = name.substr(separator+2);
    size_t central = u32(end+16);
    for (unsigned i=0; i<u16(end+10); ++i) {
        size_t length = u16(central+28);
        if (entry == std::string(reinterpret_cast<char *>(data.data()+central+46), length)) {
            size_t local = u32(central+42);
            size_t start = local+30+u16(local+26)+u16(local+28);
            return {data.begin()+start, data.begin()+start+u32(central+24)};
        }
        central += 46+length+u16(central+30)+u16(central+32);
    }
    return {};
}

class Symbols {
    uintptr_t base;
    std::vector<uint8_t> data;
    const Elf64_Shdr *sections() const {
        auto *header = reinterpret_cast<const Elf64_Ehdr *>(data.data());
        return reinterpret_cast<const Elf64_Shdr *>(data.data()+header->e_shoff);
    }
public:
    struct Symbol { uintptr_t address; size_t size; };
    Symbols(uintptr_t address, const char *path) : base(address), data(readImage(path)) {
        auto *header = reinterpret_cast<const Elf64_Ehdr *>(data.data());
        const auto *table = sections();
        const char *names = reinterpret_cast<const char *>(data.data()+table[header->e_shstrndx].sh_offset);
        for (unsigned i=0; i<header->e_shnum; ++i) {
            if (strcmp(names+table[i].sh_name, ".gnu_debugdata")) continue;
            std::vector<uint8_t> output(1024*1024);
            xz_crc32_init();
            xz_crc64_init();
            xz_dec *decoder = xz_dec_init(XZ_DYNALLOC, UINT32_MAX);
            xz_buf buffer{data.data()+table[i].sh_offset, 0, size_t(table[i].sh_size),
                    output.data(), 0, output.size()};
            xz_ret result;
            do {
                buffer.out = output.data();
                buffer.out_size = output.size();
                result = xz_dec_run(decoder, &buffer);
                if (result == XZ_OK) output.resize(output.size()*2);
            } while (result == XZ_OK);
            xz_dec_end(decoder);
            output.resize(buffer.out_pos);
            data = std::move(output);
            break;
        }
    }
    Symbol find(std::initializer_list<const char *> parts) const {
        auto *header = reinterpret_cast<const Elf64_Ehdr *>(data.data());
        const auto *table = sections();
        for (unsigned i=0; i<header->e_shnum; ++i) {
            if (table[i].sh_type != SHT_SYMTAB) continue;
            auto *symbols = reinterpret_cast<const Elf64_Sym *>(data.data()+table[i].sh_offset);
            const char *strings = reinterpret_cast<const char *>(data.data()+table[table[i].sh_link].sh_offset);
            for (size_t j=0; j<table[i].sh_size/sizeof(Elf64_Sym); ++j) {
                bool matches = true;
                for (const char *part : parts) matches &= strstr(strings+symbols[j].st_name, part) != nullptr;
                if (matches && symbols[j].st_shndx != SHN_UNDEF)
                    return {base+symbols[j].st_value, size_t(symbols[j].st_size)};
            }
        }
        return {};
    }
};

// Track the stock code's PC-relative addresses and global pointer loads.
inline uintptr_t referencedGlobal(Symbols::Symbol function, uintptr_t enter = 0) {
    uintptr_t values[32]{}, slots[32]{};
    auto *code = reinterpret_cast<const uint32_t *>(function.address);
    for (size_t i=0; i<function.size/4; ++i) {
        uint32_t op = code[i];
        unsigned rd = op&31, rn = (op>>5)&31;
        uintptr_t pc = function.address+i*4;
        if ((op&0x9f000000) == 0x90000000) {
            int64_t pages = int64_t(int32_t(((op>>5)&0x7ffff)<<13 | ((op>>29)&3)<<11))>>11;
            values[rd] = (pc&~uintptr_t(4095)) + pages*4096;
        } else if ((op&0xffc00000) == 0x91000000) {
            values[rd] = values[rn]+((op>>10)&4095);
        } else if ((op&0xffc00000) == 0xf9400000) {
            slots[rd] = values[rn]+((op>>10)&4095)*8;
        } else if (!enter && (op&0xfffffc1f) == 0xc89ffc00) {
            return values[rn]-40;
        } else if (enter && (op&0xfc000000) == 0x94000000) {
            int64_t relative = int64_t(int32_t(op<<6))>>4;
            if (pc+relative == enter) return slots[(code[i-1]>>5)&31];
        }
    }
    return 0;
}

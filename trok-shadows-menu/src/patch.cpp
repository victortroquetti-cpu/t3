// Trok Shadows Menu (.asi) -- Victor_Trok
// Escrita na memoria do gta_sa.exe.

#include "tsm.h"
#include "game.h"

#include <cstring>

namespace patch {

bool Readable(uintptr_t addr, size_t size) {
    uintptr_t end = addr + size;
    while (addr < end) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi))) {
            return false;
        }
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
            return false;
        }
        addr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return true;
}

void Write(uintptr_t addr, const void* data, size_t size) {
    DWORD old;
    VirtualProtect(reinterpret_cast<void*>(addr), size, PAGE_EXECUTE_READWRITE, &old);
    memcpy(reinterpret_cast<void*>(addr), data, size);
    VirtualProtect(reinterpret_cast<void*>(addr), size, old, &old);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(addr), size);
}

uintptr_t CallTarget(uintptr_t site) {
    if (!Readable(site, 5)) {
        return 0;
    }
    const uint8_t op = game::At<uint8_t>(site);
    if (op != 0xE8 && op != 0xE9) {
        return 0;
    }
    return site + 5 + game::At<int32_t>(site + 1);
}

static void WriteRel(uintptr_t site, uint8_t op, const void* fn) {
    uint8_t code[5];
    code[0] = op;
    const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(fn) - (site + 5));
    memcpy(code + 1, &rel, 4);
    Write(site, code, 5);
}

void SetCall(uintptr_t site, const void* fn) {
    WriteRel(site, 0xE8, fn);
}

void SetJump(uintptr_t site, const void* fn) {
    WriteRel(site, 0xE9, fn);
}

} // namespace patch

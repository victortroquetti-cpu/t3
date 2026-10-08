// Trok Shadows (.asi) -- Victor_Trok
// Escrita na memoria do gta_sa.exe. Toda chamada trocada guarda o destino que estava la antes: se outro mod ja
// tinha desviado a mesma chamada, o Trok Shadows chama o gancho dele no lugar da funcao original.

#include "trok.h"
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

void Fill(uintptr_t addr, uint8_t value, size_t size) {
    uint8_t buf[32];
    memset(buf, value, sizeof(buf));
    while (size) {
        const size_t n = size < sizeof(buf) ? size : sizeof(buf);
        Write(addr, buf, n);
        addr += n;
        size -= n;
    }
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

bool InImage(uintptr_t addr) {
    return addr >= game::EXE_BEGIN && addr < game::EXE_END;
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

bool Call::Check() const {
    if (!Readable(site, 5) || game::At<uint8_t>(site) != 0xE8) {
        return false;
    }
    const uintptr_t target = CallTarget(site);
    // Sem destino esperado (eventos): basta ser uma chamada.
    return !expected || target == expected || !InImage(target);
}

bool Call::Install(const void* hook) {
    if (installed) {
        return true;
    }
    if (!Check()) {
        Log("  pulado: %s (0x%06X nao tem a chamada do 1.0 US)", name, (unsigned)site);
        return false;
    }
    original = CallTarget(site);
    if (expected && original != expected) {
        Log("  %s: encadeado com outro mod (0x%06X chamava 0x%08X)", name, (unsigned)site, (unsigned)original);
    }
    SetCall(site, hook);
    installed = true;
    return true;
}

static bool Matches(Expect expect, const uint8_t* code) {
    switch (expect) {
    case EXPECT_SHORT_JCC:
        return code[0] >= 0x70 && code[0] <= 0x7F;
    case EXPECT_NEAR_JCC:
        return code[0] == 0x0F && code[1] >= 0x80 && code[1] <= 0x8F;
    case EXPECT_CALL:
        return code[0] == 0xE8;
    }
    return false;
}

void Toggle::Set(bool enable) {
    if (!saved) {
        if (!Readable(addr, size)) {
            broken = true;
        } else {
            memcpy(original, reinterpret_cast<void*>(addr), size);
            broken = !Matches(expect, original);
        }
        saved = true;
        if (broken) {
            Log("  pulado: %s (0x%06X diferente do 1.0 US)", name, static_cast<unsigned>(addr));
        }
    }
    if (broken || enable == on) {
        return;
    }
    Write(addr, enable ? value : original, size);
    on = enable;
}

} // namespace patch

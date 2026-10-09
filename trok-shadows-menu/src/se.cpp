// Trok Shadows Menu (.asi) -- Victor_Trok
// O Shadows Extender 2.0 por dentro. Acha o shadows.asi e confere que e o 2.0 do DK22Pac; le e muda as variaveis dele
// e os bytes que ele troca no jogo; liga a correcao do veiculo; recria as sombras quando a resolucao muda; grava o
// shadows.ini.
//
// Os enderecos do Shadows Extender sao relativos ao comeco do modulo ja descompactado: o ASPack desempacota no DllMain
// dele, e o Windows pode carregar o modulo em qualquer endereco.

#include "tsm.h"
#include "game.h"

#include <d3d9.h>
#include <tlhelp32.h>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace game;

extern "C" {
extern uintptr_t g_tsmSeExtras;
void TsmExtrasStub();
}

namespace {

typedef uint32_t u32;
typedef void(__thiscall* ThisFn)(void* self);
typedef void(__thiscall* ReturnShadowFn)(void* manager, void* shadow);
typedef void*(__cdecl* ForAllAtomicsFn)(void* clump, void* callback, void* data);
typedef void*(__cdecl* AtomicCallbackFn)(void* atomic, void* data);
typedef int(__cdecl* RenderStateSetFn)(int state, uintptr_t value);

template <class F>
F Fn(uintptr_t addr) {
    return reinterpret_cast<F>(addr);
}

// ------------------------------------------------------------------------------------------------ Shadows Extender
namespace se {
// Funcoes dele que o mod chama.
constexpr u32 PED_HOOK = 0x12F0; // em CPed::PreRenderAfterTest: UpdateRpHAnim e o pedido de sombra em tempo real
constexpr u32 EXTRAS = 0x3120;   // em CRealTimeShadow::Update: arma, paraquedas e mochila; depois fecha a camera
// Variaveis: os valores do shadows.ini depois de lidos.
constexpr u32 MAX_SHADOWS = 0x1F3E4;
constexpr u32 STENCIL_COLOR[4] = {0x1F3D8, 0x1F404, 0x1F31C, 0x1F304};
constexpr u32 REALTIME_COLOR[4] = {0x1F3E8, 0x1F318, 0x1F380, 0x1F3F0};
constexpr u32 STENCIL_DISTANCE = 0x1F324, STENCIL_DISTANCE_SQ = 0x1F398; // o jogo le por ponteiro
constexpr u32 REALTIME_DISTANCE = 0x1F374;
constexpr u32 BLUR1 = 0x1F408, BLUR2 = 0x1F3A4, BLUR_LEVEL = 0x1F3BC;
constexpr u32 RASTER = 0x1F3B0, BLUR_RASTER = 0x1F394, RASTER2 = 0x1F36C, BLUR_RASTER2 = 0x1F3AC;
constexpr u32 GRADIENT_MAX = 0x1F3F4, GRADIENT_MIN = 0x1F364;
constexpr u32 BOUND = 0x1F3D0, BOUND_AIR = 0x1F3C4, SUN_Z = 0x1F3EC, Z_LIMIT = 0x1F384, Z_LIMIT_AIR = 0x1F3C0;
constexpr u32 NIGHT = 0x1F314, CLOUDS = 0x1F3A8;
constexpr u32 DISABLE_VEHICLE_DEFAULT = 0x1F410;
constexpr u32 SHADER = 0x1F370, SHADER_READY = 0x1F301, COMBINE = 0x1F30C;
constexpr u32 SHADER_REALTIME = 0x1F320, SHADER_STENCIL = 0x1F414; // IDirect3DPixelShader9*
constexpr u32 LAST_VARIABLE = 0x1F418;
// Para reconhecer o modulo: textos dele e o gancho do pedestre, que so tem enderecos do jogo (iguais em qualquer
// endereco do modulo).
const uint8_t PED_HOOK_CODE[] = {0x56, 0xB8, 0x20, 0x2B, 0x53, 0x00, 0x8B, 0xF1, 0xFF, 0xD0, 0x83,
                                 0x7E, 0x18, 0x00, 0x74, 0x0D, 0x56, 0xBA, 0xA0, 0x6B, 0x70, 0x00,
                                 0xB9, 0x50, 0x03, 0xC4, 0x00, 0xFF, 0xD2, 0x5E, 0xC3};
} // namespace se

// Funcoes do jogo usadas so aqui.
constexpr uintptr_t CRealTimeShadowManager_Init = 0x7067C0;
constexpr uintptr_t CRealTimeShadowManager_Exit = 0x706A60;
constexpr uintptr_t CRealTimeShadowManager_ReturnRealTimeShadow = 0x705B30;
constexpr int RS_TEXTURERASTER = 1;
// CRealTimeShadow e CRealTimeShadowManager (gta-reversed).
constexpr u32 RTSHADOW_BLURRED = 0x10, RTSHADOW_BLUR_CAMERA = 0x14, RTSHADOW_BLUR_PASSES = 0x1C;
constexpr u32 RTSHADOW_MORE_BLUR = 0x20;
constexpr u32 MANAGER_BLUR_CAMERA = 0x44, MANAGER_GRADIENT_CAMERA = 0x4C;

// ------------------------------------------------------------------------------------------------ estado
uintptr_t g_se = 0;          // comeco do shadows.asi na memoria
bool g_attached = false;     // valores lidos: o menu pode mexer
bool g_fixInstalled = false; // correcao do veiculo ligada (os ganchos do Shadows Extender estavam la)
int g_waitFrames = 0;
const char* g_problem = "Procurando o Shadows Extender (shadows.asi)...";
char g_iniPath[MAX_PATH] = "";
uintptr_t g_sePed = 0;       // gancho do pedestre do Shadows Extender

Settings g_cur;      // valores em uso
Settings g_saved;    // valores no shadows.ini (o que foi lido ou gravado)
Settings g_good;     // resolucao e desfoque que o jogo conseguiu criar
const Settings g_defaults;
bool g_recreatePending = false;
bool g_restartPending = false;
int g_startMaxShadows = 0; // o pool de sombras stencil foi criado com este tamanho

template <class T>
T& SE(u32 offset) {
    return *reinterpret_cast<T*>(g_se + offset);
}

bool Same(uintptr_t addr, const void* data, size_t size) {
    return patch::Readable(addr, size) && !memcmp(reinterpret_cast<void*>(addr), data, size);
}

bool IsShadowsExtender(uintptr_t base) {
    return Same(base + 0x1714C, "2.0", 4) && Same(base + 0x17164, "Shadows Extender", 17) &&
           Same(base + 0x1727C, "shadows.ini", 12) &&
           Same(base + se::PED_HOOK, se::PED_HOOK_CODE, sizeof(se::PED_HOOK_CODE)) &&
           patch::Readable(base + 0x1F300, se::LAST_VARIABLE - 0x1F300);
}

// Pelo nome (o Shadows Extender procura a si mesmo como "shadows.asi") e, se foi renomeado, em todos os modulos.
uintptr_t FindShadowsExtender() {
    const uintptr_t named = reinterpret_cast<uintptr_t>(GetModuleHandleA("shadows.asi"));
    if (named && IsShadowsExtender(named)) {
        return named;
    }
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return 0;
    }
    uintptr_t found = 0;
    MODULEENTRY32 me;
    me.dwSize = sizeof(me);
    for (BOOL ok = Module32First(snap, &me); ok && !found; ok = Module32Next(snap, &me)) {
        const uintptr_t base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
        if (me.modBaseSize >= se::LAST_VARIABLE && IsShadowsExtender(base)) {
            found = base;
        }
    }
    CloseHandle(snap);
    return found;
}

// ------------------------------------------------------------------------------------------------ gta_sa.exe no disco
// Os bytes originais dos lugares que o Shadows Extender ja trocou ao abrir o jogo (para desligar ao vivo um recurso
// que ele ligou). Vale so se o arquivo tem o mesmo codigo que a memoria: num exe compactado nada e lido.
HANDLE g_exe = INVALID_HANDLE_VALUE;
IMAGE_SECTION_HEADER g_sections[24];
int g_sectionCount = 0;
uintptr_t g_imageBase = 0;
bool g_exeTried = false;

bool ExeRead(uintptr_t va, uint8_t* out, size_t size) {
    if (g_exe == INVALID_HANDLE_VALUE || va < g_imageBase) {
        return false;
    }
    const u32 rva = static_cast<u32>(va - g_imageBase);
    for (int i = 0; i < g_sectionCount; i++) {
        const IMAGE_SECTION_HEADER& s = g_sections[i];
        if (rva >= s.VirtualAddress && rva + size <= s.VirtualAddress + s.SizeOfRawData) {
            DWORD read = 0;
            SetFilePointer(g_exe, static_cast<LONG>(s.PointerToRawData + (rva - s.VirtualAddress)), nullptr,
                           FILE_BEGIN);
            return ReadFile(g_exe, out, static_cast<DWORD>(size), &read, nullptr) && read == size;
        }
    }
    return false;
}

bool ExeOpen() {
    if (g_exeTried) {
        return g_exe != INVALID_HANDLE_VALUE;
    }
    g_exeTried = true;
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        return false;
    }
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS32 nt;
    DWORD read = 0;
    bool ok = ReadFile(f, &dos, sizeof(dos), &read, nullptr) && read == sizeof(dos) && dos.e_magic == IMAGE_DOS_SIGNATURE;
    ok = ok && SetFilePointer(f, dos.e_lfanew, nullptr, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
         ReadFile(f, &nt, sizeof(nt), &read, nullptr) && read == sizeof(nt) && nt.Signature == IMAGE_NT_SIGNATURE;
    if (ok) {
        g_sectionCount = nt.FileHeader.NumberOfSections < 24 ? nt.FileHeader.NumberOfSections : 24;
        g_imageBase = nt.OptionalHeader.ImageBase;
        const DWORD at = dos.e_lfanew + offsetof(IMAGE_NT_HEADERS32, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        ok = SetFilePointer(f, at, nullptr, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
             ReadFile(f, g_sections, g_sectionCount * sizeof(IMAGE_SECTION_HEADER), &read, nullptr);
    }
    if (ok) {
        g_exe = f;
        // CRealTimeShadow::Update ate antes do gancho do Shadows Extender: tem que ser igual a memoria.
        uint8_t disk[0x70];
        ok = ExeRead(0x706600, disk, sizeof(disk)) && Same(0x706600, disk, sizeof(disk));
    }
    if (!ok) {
        g_exe = INVALID_HANDLE_VALUE;
        CloseHandle(f);
        Log("o gta_sa.exe no disco nao tem o mesmo codigo da memoria: o que o Shadows Extender ligou ao abrir o jogo "
            "so desliga quando o jogo abrir de novo");
    }
    return ok;
}

// ------------------------------------------------------------------------------------------------ liga/desliga
// Bytes que o Shadows Extender troca ao abrir o jogo, conforme o shadows.ini. "on" e o que ele escreve; o original
// vem da memoria (se ele nao trocou) ou do gta_sa.exe no disco.
enum Kind : uint8_t { SHORT_JCC, CALL, NEAR_JCC, JCC_OPCODE };

struct Site {
    uintptr_t addr;
    uint8_t size;
    Kind kind;
    uint8_t on[6];
    uint8_t original[6];
    bool known;
};

struct Toggle {
    const char* name;
    int count;
    Site site[2];
};

Toggle g_flagIgnore = {"FlagIgnoreSomeShadows", 1, {{0x711E3D, 2, SHORT_JCC, {0x90, 0x90}, {}, false}, {}}};
Toggle g_disableBuildings = {
    "DisableBuildingShadows", 1, {{0x711E41, 5, CALL, {0x90, 0x90, 0x90, 0x90, 0x90}, {}, false}, {}}};
Toggle g_stencilLow = {"DisplayShadowsAtLowSettings do stencil",
                       2,
                       {{0x711D9D, 2, SHORT_JCC, {0x90, 0x90}, {}, false}, {0x7113C0, 2, SHORT_JCC, {0x90, 0x90}, {}, false}}};
Toggle g_realtimeLow = {"DisplayShadowsAtLowSettings da sombra em tempo real",
                        2,
                        {{0x706BCC, 1, JCC_OPCODE, {0xEB}, {}, false}, {0x5E6766, 1, JCC_OPCODE, {0xEB}, {}, false}}};
Toggle g_vehicleDefault = {"DrawVehicleDefaultShadowWithRealTime",
                           1,
                           {{0x70BDAB, 6, NEAR_JCC, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90}, {}, false}, {}}};
Toggle g_morePlayers = {"MoreThanOnePlayer", 1, {{0x7069F5, 1, JCC_OPCODE, {0xEB}, {}, false}, {}}};
Toggle* const kToggles[] = {&g_flagIgnore, &g_disableBuildings, &g_stencilLow, &g_realtimeLow, &g_vehicleDefault,
                            &g_morePlayers};

bool LooksOriginal(const Site& s, const uint8_t* code) {
    switch (s.kind) {
    case SHORT_JCC:
    case JCC_OPCODE:
        return code[0] >= 0x70 && code[0] <= 0x7F;
    case CALL:
        return code[0] == 0xE8;
    case NEAR_JCC:
        return code[0] == 0x0F && code[1] >= 0x80 && code[1] <= 0x8F;
    }
    return false;
}

void Learn(Toggle& t) {
    for (int i = 0; i < t.count; i++) {
        Site& s = t.site[i];
        if (!patch::Readable(s.addr, 8)) {
            continue;
        }
        const uint8_t* live = reinterpret_cast<const uint8_t*>(s.addr);
        if (memcmp(live, s.on, s.size) && LooksOriginal(s, live)) {
            memcpy(s.original, live, s.size);
            s.known = true;
            continue;
        }
        // O Shadows Extender ja trocou: o original so no disco. A instrucao seguinte tem que bater com a memoria.
        uint8_t disk[8];
        if (ExeOpen() && ExeRead(s.addr, disk, 8) && LooksOriginal(s, disk) &&
            !memcmp(disk + s.size, live + s.size, 8 - s.size)) {
            memcpy(s.original, disk, s.size);
            s.known = true;
        }
    }
}

bool IsOn(const Toggle& t) {
    for (int i = 0; i < t.count; i++) {
        if (!Same(t.site[i].addr, t.site[i].on, t.site[i].size)) {
            return false;
        }
    }
    return true;
}

// Liga ou desliga. false = nao deu para desligar ao vivo (o original nao e conhecido): vale ao reiniciar.
bool Set(Toggle& t, bool enable) {
    if (IsOn(t) == enable) {
        return true;
    }
    for (int i = 0; i < t.count; i++) {
        if (!enable && !t.site[i].known) {
            return false;
        }
    }
    for (int i = 0; i < t.count; i++) {
        const Site& s = t.site[i];
        patch::Write(s.addr, enable ? s.on : s.original, s.size);
    }
    return true;
}

// ------------------------------------------------------------------------------------------------ correcao do veiculo
// Silhueta de um clump na camera da sombra, como CShadowCamera::Update(RpClump*) faz com o dono: sem textura, luz
// nem cor na geometria durante o desenho.
void* __cdecl SilhouetteAtomic(void* atomic, void* data) {
    if (!(Field<uint8_t>(atomic, ATOMIC_FLAGS) & 4)) {
        return atomic;
    }
    void* geometry = Field<void*>(atomic, ATOMIC_GEOMETRY);
    const u32 flags = geometry ? Field<u32>(geometry, GEOMETRY_FLAGS) : 0;
    if (geometry) {
        Field<u32>(geometry, GEOMETRY_FLAGS) = flags & ~SHADOW_GEOMETRY_FLAGS;
    }
    Fn<AtomicCallbackFn>(atomicQuickRender)(atomic, data);
    if (geometry) {
        Field<u32>(geometry, GEOMETRY_FLAGS) = flags;
    }
    return atomic;
}

// Pedestre dentro de um veiculo que ja tem sombra em tempo real: ele entra na sombra do veiculo (TsmVehicleExtras) em
// vez de ter a dele. Com duas sombras, onde elas se cruzavam escurecia dobrado (moto e piloto). Se o veiculo ficou sem
// sombra (as 16 em uso), o pedestre continua com a dele.
bool InShadowedVehicle(void* ped) {
    if (!g_cur.fixOccupants || !HasRwObject(ped)) {
        return false;
    }
    void* vehicle = VehicleOf(ped);
    return vehicle && HasRwObject(vehicle) && Field<void*>(vehicle, PHYSICAL_SHADOW_DATA);
}

// CPed::PreRenderAfterTest (0x5E6664): o Shadows Extender atualiza os ossos e pede sombra em tempo real para todo
// pedestre. (O pedido do proprio jogo, para quem anda de moto, ele apaga em 0x5E68A2.)
void __thiscall PedHook(void* ped) {
    if (InShadowedVehicle(ped)) {
        Fn<ThisFn>(CEntity_UpdateRpHAnim)(ped);
        return;
    }
    Fn<ThisFn>(g_sePed)(ped);
}

void InstallFix() {
    const uintptr_t ped = g_se + se::PED_HOOK, extras = g_se + se::EXTRAS;
    const bool pedOk = At<uint8_t>(0x5E6664) == 0xE8 && patch::CallTarget(0x5E6664) == ped;
    const bool extrasOk = At<uint8_t>(0x706676) == 0xE9 && patch::CallTarget(0x706676) == extras;
    if (!pedOk || !extrasOk) {
        Log("aviso: os ganchos do Shadows Extender em 0x5E6664/0x706676 nao estao la (outro mod?) -- a correcao do "
            "veiculo ficou desligada");
        return;
    }
    g_sePed = ped;
    g_tsmSeExtras = extras;
    patch::SetCall(0x5E6664, reinterpret_cast<const void*>(&PedHook));
    patch::SetJump(0x706676, reinterpret_cast<const void*>(&TsmExtrasStub));
    g_fixInstalled = true;
    Log("aplicado: quem esta no veiculo entra na sombra dele (nada de escurecer dobrado)");
}

// ------------------------------------------------------------------------------------------------ valores
void ReadLive(Settings& s) {
    s.maxShadows = SE<int>(se::MAX_SHADOWS);
    s.stencilDistance = SE<float>(se::STENCIL_DISTANCE);
    s.flagIgnoreSome = IsOn(g_flagIgnore);
    s.disableBuildings = IsOn(g_disableBuildings);
    s.stencilLow = IsOn(g_stencilLow);
    for (int i = 0; i < 4; i++) {
        s.stencilColor[i] = SE<int>(se::STENCIL_COLOR[i]);
        s.realtimeColor[i] = SE<int>(se::REALTIME_COLOR[i]);
    }
    s.realtimeLow = IsOn(g_realtimeLow);
    s.combine = SE<int>(se::COMBINE) != 0;
    s.realtimeDistance = At<float>(RT_MAX_DISTANCE);
    // O que o jogo usa ao criar as sombras: os push de CRealTimeShadowManager::Init e CRealTimeShadow::Create.
    s.blur1 = At<uint8_t>(0x706814) != 0;
    s.blur2 = At<uint8_t>(0x706810) != 0;
    s.blurLevel = At<uint8_t>(0x706812);
    s.raster = At<uint8_t>(0x7064C2);
    s.blurRaster = At<uint8_t>(0x7064F9);
    s.raster2 = At<uint8_t>(0x706825);
    s.blurRaster2 = At<uint8_t>(0x706832);
    s.gradientMax = At<int>(GRADIENT_MAX);
    s.gradientMin = At<int>(GRADIENT_MIN);
    s.bound = SE<float>(se::BOUND);
    s.boundAir = SE<float>(se::BOUND_AIR);
    s.sunZ = SE<float>(se::SUN_Z);
    s.zLimit = SE<float>(se::Z_LIMIT);
    s.zLimitAir = SE<float>(se::Z_LIMIT_AIR);
    s.vehicleDefaultWithRealtime = IsOn(g_vehicleDefault);
    s.disableVehicleDefault = SE<int>(se::DISABLE_VEHICLE_DEFAULT) != 0;
    s.shader = SE<uint8_t>(se::SHADER_READY) != 0;
    s.night = SE<float>(se::NIGHT);
    s.clouds = SE<float>(se::CLOUDS);
    s.morePlayers = IsOn(g_morePlayers);
}

void SetPush(uintptr_t addr, int value) {
    if (At<uint8_t>(addr - 1) == 0x6A) { // push imm8
        const uint8_t b = static_cast<uint8_t>(value);
        patch::Write(addr, &b, 1);
    }
}

void WriteCreateValues(const Settings& s) {
    SetPush(0x706814, s.blur1);
    SetPush(0x706810, s.blur2);
    SetPush(0x706812, s.blurLevel);
    SetPush(0x7064C2, s.raster);
    SetPush(0x7064F9, s.blurRaster);
    SetPush(0x706825, s.raster2);
    SetPush(0x706832, s.blurRaster2);
    At<int>(GRADIENT_MAX) = s.gradientMax;
    At<int>(GRADIENT_MIN) = s.gradientMin;
    SE<int>(se::BLUR1) = s.blur1;
    SE<int>(se::BLUR2) = s.blur2;
    SE<int>(se::BLUR_LEVEL) = s.blurLevel;
    SE<int>(se::RASTER) = s.raster;
    SE<int>(se::BLUR_RASTER) = s.blurRaster;
    SE<int>(se::RASTER2) = s.raster2;
    SE<int>(se::BLUR_RASTER2) = s.blurRaster2;
    SE<int>(se::GRADIENT_MAX) = s.gradientMax;
    SE<int>(se::GRADIENT_MIN) = s.gradientMin;
}

void** ManagerSlots() {
    return reinterpret_cast<void**>(REALTIME_SHADOW_MAN + MANAGER_SHADOWS);
}

bool ManagerReady() {
    return At<uint8_t>(REALTIME_SHADOW_MAN) != 0;
}

// Desfoque e degrade de cada sombra ja criada: valem no proximo quadro, sem recriar nada.
void ApplyBlurToShadows(const Settings& s) {
    if (!ManagerReady()) {
        return;
    }
    for (int i = 0; i < MANAGER_SLOTS; i++) {
        void* shadow = ManagerSlots()[i];
        if (shadow) {
            Field<u32>(shadow, RTSHADOW_BLUR_PASSES) = static_cast<u32>(s.blurLevel);
            Field<uint8_t>(shadow, RTSHADOW_MORE_BLUR) = s.blur2;
        }
    }
}

bool ShadowsComplete() {
    if (!Field<void*>(reinterpret_cast<void*>(REALTIME_SHADOW_MAN), MANAGER_BLUR_CAMERA) ||
        !Field<void*>(reinterpret_cast<void*>(REALTIME_SHADOW_MAN), MANAGER_GRADIENT_CAMERA)) {
        return false;
    }
    for (int i = 0; i < MANAGER_SLOTS; i++) {
        void* shadow = ManagerSlots()[i];
        if (!shadow || !Field<void*>(shadow, RTSHADOW_CAMERA) ||
            (Field<uint8_t>(shadow, RTSHADOW_BLURRED) && !Field<void*>(shadow, RTSHADOW_BLUR_CAMERA))) {
            return false;
        }
    }
    return true;
}

void RebuildManager(const Settings& s) {
    void* manager = reinterpret_cast<void*>(REALTIME_SHADOW_MAN);
    for (int i = 0; i < MANAGER_SLOTS; i++) {
        void* shadow = ManagerSlots()[i];
        if (shadow && Field<void*>(shadow, RTSHADOW_OWNER)) {
            Fn<ReturnShadowFn>(CRealTimeShadowManager_ReturnRealTimeShadow)(manager, shadow);
        }
    }
    Fn<ThisFn>(CRealTimeShadowManager_Exit)(manager);
    WriteCreateValues(s);
    Fn<ThisFn>(CRealTimeShadowManager_Init)(manager);
    // O RenderWare lembra a ultima textura posta: com uma nova no mesmo endereco ele nao a poria de novo.
    Fn<RenderStateSetFn>(RwRenderStateSet)(RS_TEXTURERASTER, 0);
}

// Resolucao, desfoque e degrade novos: o jogo so os usa ao criar as sombras. Entre CGame::Process e o
// CRealTimeShadowManager::Update nenhuma sombra esta guardada para desenhar (RenderStoredShadows zera a lista no fim):
// as 16 sombras sao devolvidas, apagadas e criadas de novo com os valores novos, como o jogo faz ao iniciar.
void Recreate() {
    IDirect3DDevice9* device = At<IDirect3DDevice9*>(RW_D3D_DEVICE);
    if (!device || device->TestCooperativeLevel() != D3D_OK) {
        return; // tela minimizada ou device perdido: tenta no proximo quadro
    }
    g_recreatePending = false;
    if (!ManagerReady()) {
        WriteCreateValues(g_cur); // o jogo ainda vai criar as sombras: ja usa os valores novos
        return;
    }
    RebuildManager(g_cur);
    if (ShadowsComplete()) {
        g_good = g_cur;
        Log("sombras em tempo real recriadas: resolucao %d/%d/%d/%d, desfoque %d/%d/%d, degrade %d/%d", g_cur.raster,
            g_cur.blurRaster, g_cur.raster2, g_cur.blurRaster2, g_cur.blur1, g_cur.blurLevel, g_cur.blur2,
            g_cur.gradientMax, g_cur.gradientMin);
        return;
    }
    // A placa nao criou: volta para o que funcionava.
    Log("aviso: a placa nao criou as sombras com resolucao %d/%d -- voltou para %d/%d", g_cur.raster, g_cur.blurRaster,
        g_good.raster, g_good.blurRaster);
    g_cur.raster = g_good.raster;
    g_cur.blurRaster = g_good.blurRaster;
    g_cur.raster2 = g_good.raster2;
    g_cur.blurRaster2 = g_good.blurRaster2;
    g_cur.blur1 = g_good.blur1;
    RebuildManager(g_cur);
}

void Attach() {
    Learn(g_flagIgnore);
    Learn(g_disableBuildings);
    Learn(g_stencilLow);
    Learn(g_realtimeLow);
    Learn(g_vehicleDefault);
    Learn(g_morePlayers);
    ReadLive(g_cur);

    // shadows.ini ao lado do shadows.asi, como o Shadows Extender monta (sem ele pelo nome, ao lado do gta_sa.exe).
    char dir[MAX_PATH];
    GetModuleFileNameA(GetModuleHandleA("shadows.asi"), dir, MAX_PATH);
    char* slash = strrchr(dir, '\\');
    if (slash) {
        slash[1] = 0;
    } else {
        dir[0] = 0;
    }
    snprintf(g_iniPath, sizeof(g_iniPath), "%sshadows.ini", dir);

    // O Shadows Extender lia DisplayShadowsAtLowSettings do [REALTIME_SHADOWS] duas vezes: a do [STENCIL_SHADOWS]
    // nunca valia. Aqui vale.
    const bool stencilLow = GetPrivateProfileIntA("STENCIL_SHADOWS", "DisplayShadowsAtLowSettings", 0, g_iniPath) != 0;
    if (stencilLow != g_cur.stencilLow) {
        if (Set(g_stencilLow, stencilLow)) {
            g_cur.stencilLow = stencilLow;
            Log("corrigido: DisplayShadowsAtLowSettings do [STENCIL_SHADOWS] = %d (o Shadows Extender usava o do "
                "[REALTIME_SHADOWS])",
                stencilLow);
        } else {
            g_restartPending = true;
        }
    }
    g_cur.fixOccupants = GetPrivateProfileIntA("TROK_MENU", "CorrigirVeiculo", 1, g_iniPath) != 0;
    g_saved = g_cur;
    g_good = g_cur;
    g_startMaxShadows = g_cur.maxShadows;
    g_attached = true;
    g_problem = nullptr;
    InstallFix();

    int known = 0, total = 0;
    for (const Toggle* t : kToggles) {
        for (int i = 0; i < t->count; i++) {
            known += t->site[i].known;
            total++;
        }
    }
    Log("Shadows Extender 2.0 em 0x%08X -- INI: %s", static_cast<unsigned>(g_se), g_iniPath);
    Log("  em uso: stencil %d/%.0fm, tempo real %.0fm, raster %d/%d/%d/%d, desfoque %d/%d/%d, combinado %d, shader %d",
        g_cur.maxShadows, g_cur.stencilDistance, g_cur.realtimeDistance, g_cur.raster, g_cur.blurRaster, g_cur.raster2,
        g_cur.blurRaster2, g_cur.blur1, g_cur.blurLevel, g_cur.blur2, g_cur.combine, g_cur.shader);
    Log("  liga/desliga ao vivo: %d de %d lugares com o byte original conhecido", known, total);
}

} // namespace

// Chamado pelo TsmExtrasStub em CRealTimeShadow::Update, com a camera da sombra ainda aberta: quem esta no veiculo
// entra na sombra dele, em silhueta (motorista e ate 8 passageiros). Depois o trecho do Shadows Extender fecha a camera.
extern "C" void TsmVehicleExtras(void* shadow) {
    if (!g_cur.fixOccupants) {
        return;
    }
    void* owner = Field<void*>(shadow, RTSHADOW_OWNER);
    if (!owner || !HasRwObject(owner) || EntityType(owner) != ENTITY_VEHICLE) {
        return;
    }
    void* camera = Field<void*>(shadow, RTSHADOW_CAMERA);
    void* globals = At<void*>(RW_ENGINE_INSTANCE);
    if (!camera || !globals || *static_cast<void**>(globals) != camera) {
        return; // a camera nao esta aberta (sombra de atomic, ou o RwCameraBeginUpdate recusou)
    }
    for (int i = 0; i < 9; i++) {
        void* ped = Field<void*>(owner, i == 0 ? VEHICLE_DRIVER : VEHICLE_PASSENGERS + (i - 1) * 4);
        if (!ped || !HasRwObject(ped) || VehicleOf(ped) != owner) {
            continue;
        }
        Fn<ForAllAtomicsFn>(RpClumpForAllAtomics)(Field<void*>(ped, ENTITY_RWOBJECT),
                                                  reinterpret_cast<void*>(&SilhouetteAtomic), nullptr);
    }
}

void SeFrame() {
    if (g_attached) {
        if (g_recreatePending) {
            Recreate();
        }
        return;
    }
    if (!g_se) {
        g_se = FindShadowsExtender();
        if (!g_se) {
            if (++g_waitFrames == 600) {
                Log("o Shadows Extender 2.0 (shadows.asi do DK22Pac) nao foi achado -- o menu fica sem efeito");
                g_problem = "O Shadows Extender 2.0 (shadows.asi) n\xC3\xA3o est\xC3\xA1 instalado.";
            }
            return;
        }
        g_waitFrames = 0;
    }
    // Ele termina de iniciar no evento do RenderWare: os ganchos dele ja estao no jogo quando o primeiro quadro roda.
    const bool started = At<uint8_t>(0x706676) == 0xE9 && patch::CallTarget(0x706676) == g_se + se::EXTRAS;
    if (started || ++g_waitFrames >= 600) {
        if (!started) {
            Log("aviso: o Shadows Extender nao ligou o gancho dele em 0x706676 -- os valores sao lidos assim mesmo");
        }
        Attach();
    }
}

// ------------------------------------------------------------------------------------------------ menu
namespace backend {

bool Attached() {
    return g_attached;
}

const char* Problem() {
    return g_problem;
}

Settings& Current() {
    return g_cur;
}

const Settings& Defaults() {
    return g_defaults;
}

void Apply(const Settings& b) {
    if (!g_attached) {
        return;
    }
    Settings& s = g_cur;
    if (s.stencilDistance != b.stencilDistance) {
        SE<float>(se::STENCIL_DISTANCE) = s.stencilDistance;
        SE<float>(se::STENCIL_DISTANCE_SQ) = s.stencilDistance * s.stencilDistance;
    }
    for (int i = 0; i < 4; i++) {
        SE<int>(se::STENCIL_COLOR[i]) = s.stencilColor[i];
        SE<int>(se::REALTIME_COLOR[i]) = s.realtimeColor[i];
    }
    SE<int>(se::COMBINE) = s.combine;
    if (s.realtimeDistance != b.realtimeDistance) {
        SE<float>(se::REALTIME_DISTANCE) = s.realtimeDistance;
        At<float>(RT_MAX_DISTANCE) = s.realtimeDistance;
        At<float>(RT_MAX_DISTANCE_SQ) = s.realtimeDistance * s.realtimeDistance;
    }
    SE<float>(se::BOUND) = s.bound;
    SE<float>(se::BOUND_AIR) = s.boundAir;
    SE<float>(se::SUN_Z) = s.sunZ;
    SE<float>(se::Z_LIMIT) = s.zLimit;
    SE<float>(se::Z_LIMIT_AIR) = s.zLimitAir;
    SE<float>(se::NIGHT) = s.night;
    SE<float>(se::CLOUDS) = s.clouds;
    SE<int>(se::DISABLE_VEHICLE_DEFAULT) = s.disableVehicleDefault;
    if (s.shader != b.shader) {
        // Ligar so funciona se o Shadows Extender compilou os shaders ao abrir o jogo.
        if (s.shader && (!SE<void*>(se::SHADER_REALTIME) || !SE<void*>(se::SHADER_STENCIL))) {
            g_restartPending = true;
        } else {
            SE<uint8_t>(se::SHADER_READY) = s.shader;
        }
        SE<int>(se::SHADER) = s.shader;
    }
    const struct {
        Toggle* toggle;
        bool on, was;
    } toggles[] = {
        {&g_flagIgnore, s.flagIgnoreSome, b.flagIgnoreSome},
        {&g_disableBuildings, s.disableBuildings, b.disableBuildings},
        {&g_stencilLow, s.stencilLow, b.stencilLow},
        {&g_realtimeLow, s.realtimeLow, b.realtimeLow},
        {&g_vehicleDefault, s.vehicleDefaultWithRealtime, b.vehicleDefaultWithRealtime},
        {&g_morePlayers, s.morePlayers, b.morePlayers},
    };
    for (const auto& t : toggles) {
        if (t.on != t.was && !Set(*t.toggle, t.on)) {
            g_restartPending = true;
            Log("%s=%d vale quando o jogo abrir de novo (o byte original nao e conhecido)", t.toggle->name, t.on);
        }
    }
    if (s.blurLevel != b.blurLevel || s.blur2 != b.blur2) {
        SetPush(0x706812, s.blurLevel);
        SetPush(0x706810, s.blur2);
        SE<int>(se::BLUR_LEVEL) = s.blurLevel;
        SE<int>(se::BLUR2) = s.blur2;
        ApplyBlurToShadows(s);
    }
    if (s.raster != b.raster || s.blurRaster != b.blurRaster || s.raster2 != b.raster2 ||
        s.blurRaster2 != b.blurRaster2 || s.blur1 != b.blur1 || s.gradientMax != b.gradientMax ||
        s.gradientMin != b.gradientMin) {
        g_recreatePending = true;
    }
    SE<int>(se::MAX_SHADOWS) = s.maxShadows; // o jogo cria o pool de sombras stencil uma vez so: vale ao reiniciar
}

bool RestartPending() {
    return g_restartPending || g_cur.maxShadows != g_startMaxShadows;
}

bool Save() {
    if (!g_attached) {
        return false;
    }
    if (!IniWrite(g_iniPath, g_cur, g_saved)) {
        Log("aviso: nao deu para gravar %s", g_iniPath);
        return false;
    }
    g_saved = g_cur;
    return true;
}

bool FixInstalled() {
    return g_fixInstalled;
}

int ActiveShadows() {
    if (!ManagerReady()) {
        return 0;
    }
    int n = 0;
    for (int i = 0; i < MANAGER_SLOTS; i++) {
        void* shadow = ManagerSlots()[i];
        n += shadow && Field<void*>(shadow, RTSHADOW_OWNER) && Field<uint8_t>(shadow, RTSHADOW_INTENSITY);
    }
    return n;
}

} // namespace backend

// Para o teste sem o jogo (test/host.cpp): os mesmos caminhos do menu.
extern "C" __declspec(dllexport) Settings* TsmCurrent() {
    return &g_cur;
}

extern "C" __declspec(dllexport) void TsmApply(const Settings* before) {
    backend::Apply(*before);
}

extern "C" __declspec(dllexport) bool TsmSave() {
    return backend::Save();
}

// Trok Shadows Menu (.asi) -- Victor_Trok
// O Shadows Extender 2.0 por dentro. Acha o shadows.asi e confere que e o 2.0 do DK22Pac; le e muda as variaveis dele
// e os bytes que ele troca no jogo; desenha a sombra desfocada em camadas (o cruzamento de duas sombras escurece uma
// vez so); recria as sombras quando a resolucao muda; grava o shadows.ini.
//
// Os enderecos do Shadows Extender sao relativos ao comeco do modulo ja descompactado: o ASPack desempacota no DllMain
// dele, e o Windows pode carregar o modulo em qualquer endereco.

#include "tsm.h"
#include "game.h"

#include <d3d9.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace game;

namespace {

typedef uint32_t u32;
typedef void(__thiscall* ThisFn)(void* self);
typedef void(__thiscall* ReturnShadowFn)(void* manager, void* shadow);
typedef int(__cdecl* RenderStateSetFn)(int state, uintptr_t value);
typedef int(__cdecl* RenderStateGetFn)(int state, uintptr_t* value);
typedef void(__cdecl* VoidFn)();
typedef void(__cdecl* RectFn)(const void* rect, const void* color);
typedef void(__cdecl* CastFn)(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32,
                              u32);

template <class F>
F Fn(uintptr_t addr) {
    return reinterpret_cast<F>(addr);
}

// ------------------------------------------------------------------------------------------------ Shadows Extender
namespace se {
// Funcoes dele.
constexpr u32 PED_HOOK = 0x12F0;     // em CPed::PreRenderAfterTest: UpdateRpHAnim e o pedido de sombra em tempo real
constexpr u32 EXTRAS = 0x3120;       // em CRealTimeShadow::Update: arma, paraquedas e mochila; depois fecha a camera
constexpr u32 STENCIL_RECT = 0x1350; // o retangulo do stencil (0x71167F) com a cor e a forca do INI
constexpr u32 FLUSH = 0x3510;        // RenderStuffInBuffer com o shader dele (0x7082A4, 0x7082BD e o fim do CAST)
constexpr u32 CAST = 0x36E0;         // CastRealTimeShadowSectorList (0x70AD0D) e depois FLUSH
constexpr u32 CAST_FLUSH_JMP = 0x3766; // o "jmp FLUSH" no fim do CAST
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
constexpr uintptr_t CShadows_CastRealTimeShadowSectorList = 0x70A7E0;
constexpr uintptr_t RenderBuffer_RenderStuffInBuffer = 0x707800;
constexpr uintptr_t CSprite2d_DrawRect = 0x727B60;
// Onde o Shadows Extender desviou o desenho da sombra em tempo real (conferido antes de mexer).
constexpr uintptr_t SITE_CAST = 0x70AD0D;    // CShadows::RenderStoredShadows chamando CastRealTimeShadowSectorList
constexpr uintptr_t SITE_FULL_1 = 0x7082A4;  // RenderBuffer::StartStoring com o buffer cheio
constexpr uintptr_t SITE_FULL_2 = 0x7082BD;
constexpr uintptr_t SITE_RECT = 0x71167F;    // CStencilShadows::RenderStencilShadows chamando CSprite2d::DrawRect
// RwRenderState e os valores do RenderWare.
constexpr int RS_TEXTURERASTER = 1, RS_ZTESTENABLE = 6, RS_SHADEMODE = 7, RS_SRCBLEND = 10, RS_DESTBLEND = 11;
constexpr int RS_VERTEXALPHAENABLE = 12, RS_STENCILENABLE = 21, RS_STENCILFAIL = 22, RS_STENCILZFAIL = 23;
constexpr int RS_STENCILPASS = 24, RS_STENCILFUNCTION = 25, RS_STENCILFUNCTIONREF = 26, RS_STENCILFUNCTIONMASK = 27;
constexpr int RS_STENCILFUNCTIONWRITEMASK = 28, RS_ALPHATESTFUNCTION = 29, RS_ALPHATESTFUNCTIONREF = 30;
constexpr int STENCIL_KEEP = 1, STENCIL_ZERO = 2, STENCIL_REPLACE = 3;
constexpr int FUNC_LESSEQUAL = 4, FUNC_GREATER = 5, FUNC_ALWAYS = 8;
constexpr int BLEND_ZERO = 1, BLEND_ONE = 2, BLEND_SRCALPHA = 5, BLEND_INVSRCALPHA = 6;
constexpr int SHADE_FLAT = 1;
// CRealTimeShadow e CRealTimeShadowManager (gta-reversed).
constexpr u32 RTSHADOW_BLURRED = 0x10, RTSHADOW_BLUR_CAMERA = 0x14, RTSHADOW_BLUR_PASSES = 0x1C;
constexpr u32 RTSHADOW_MORE_BLUR = 0x20;
constexpr u32 MANAGER_BLUR_CAMERA = 0x44, MANAGER_GRADIENT_CAMERA = 0x4C;

// ------------------------------------------------------------------------------------------------ estado
uintptr_t g_se = 0;             // comeco do shadows.asi na memoria
bool g_attached = false;        // valores lidos: o menu pode mexer
bool g_layersInstalled = false; // sombra desfocada em camadas ligada (os desvios do Shadows Extender estavam la)
int g_waitFrames = 0;
int g_frameEvents = 0;     // vezes que o evento de quadro (0x53E981) chegou
int g_framesAtPresent = 0; // g_frameEvents no Present anterior
const char* g_problem = "Procurando o Shadows Extender (shadows.asi)...";
char g_iniPath[MAX_PATH] = "";

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
    static bool explained = false;
    if (named && !explained) {
        explained = true;
        Log("o shadows.asi esta carregado em 0x%08X, mas nao e o Shadows Extender 2.0 que o menu conhece (versao %s, "
            "nome %s, INI %s, gancho do pedestre %s)",
            static_cast<unsigned>(named), Same(named + 0x1714C, "2.0", 4) ? "ok" : "diferente",
            Same(named + 0x17164, "Shadows Extender", 17) ? "ok" : "diferente",
            Same(named + 0x1727C, "shadows.ini", 12) ? "ok" : "diferente",
            Same(named + se::PED_HOOK, se::PED_HOOK_CODE, sizeof(se::PED_HOOK_CODE)) ? "ok" : "diferente");
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

// ------------------------------------------------------------------------------------------------ sombra em camadas
// Com a sombra desfocada (CombineRealTimeShadowsWithStencil=0), cada sombra em tempo real escurece o que ja esta na tela
// (destino x (1 - sombra)): onde duas se cruzavam (o piloto e a moto, dois jogadores) escurecia dobrado. Aqui cada
// sombra e desenhada em LAYERS faixas de escurecimento, cada faixa com o shader abaixo. O stencil (bits 4 a 6) guarda
// ate que faixa cada pixel ja foi escurecido, e uma faixa so escurece o pixel que ainda nao chegou nela. Sozinha, a
// sombra sai igual a do Shadows Extender (as faixas se multiplicam no mesmo valor); no cruzamento, o pixel fica com o
// escurecimento da sombra mais forte ali (no maximo uma faixa mais claro), em vez da soma das duas. As marcas saem do
// stencil antes do retangulo do stencil do jogo (SITE_RECT), que escurece todo pixel com stencil diferente de zero.
constexpr int LAYERS = 4;
constexpr uintptr_t LAYER_MASK = 0x70;
constexpr int LAYER_SHIFT = 4;

// ps_2_0. c0 = cor (rgb) e forca (a), o mesmo c0 do Shadows Extender; c1.x = comeco da faixa, c1.y = 1 / largura;
// c2.rgb = (1 - cor) * largura / (1 - (1 - cor) * comeco). Saida: o escurecimento desta faixa (para destino x (1 - cor))
// e, no alfa, o escurecimento da sombra ali (o teste de alfa descarta o que nao chega na faixa).
//   texld r0, t0, s0
//   mul r0.w, r0.x, c0.w          ; s = forca * sombra
//   add r1.x, r0.w, -c1.x
//   mul_sat r1.x, r1.x, c1.y      ; quanto da faixa o pixel preenche
//   mul r1.xyz, r1.x, c2
//   mov r1.w, r0.w
//   mov oC0, r1
const DWORD kLayerShader[] = {
    0xFFFF0200,
    0x0200001F, 0x80000000, 0xB0030000,
    0x0200001F, 0x90000000, 0xA00F0800,
    0x03000042, 0x800F0000, 0xB0E40000, 0xA0E40800,
    0x03000005, 0x80080000, 0x80000000, 0xA0FF0000,
    0x03000002, 0x80010001, 0x80FF0000, 0xA1000001,
    0x03000005, 0x80110001, 0x80000001, 0xA0550001,
    0x03000005, 0x80070001, 0x80000001, 0xA0E40002,
    0x02000001, 0x80080001, 0x80FF0000,
    0x02000001, 0x800F0800, 0x80E40001,
    0x0000FFFF,
};

IDirect3DPixelShader9* g_layerShader = nullptr;
IDirect3DDevice9* g_layerDevice = nullptr;
bool g_layerShaderFailed = false;
uintptr_t g_seCast = 0, g_seFlush = 0, g_seRect = 0;
bool g_inCast = false; // desenhando uma sombra em tempo real (entre SITE_CAST e o fim do CAST)
bool g_marked = false; // ha marcas das camadas no stencil

void SetState(int state, uintptr_t value) {
    Fn<RenderStateSetFn>(RwRenderStateSet)(state, value);
}

// Guarda os estados que vao mudar e devolve no fim. Sem valor guardado (o RenderWare recusou), volta para o padrao.
struct SavedStates {
    struct Item {
        int state;
        uintptr_t value;
    } items[16];
    int count = 0;

    void Save(int state, uintptr_t fallback) {
        uintptr_t value = fallback;
        if (!Fn<RenderStateGetFn>(RwRenderStateGet)(state, &value)) {
            value = fallback;
        }
        items[count++] = {state, value};
    }

    void Restore() const {
        for (int i = count - 1; i >= 0; i--) {
            SetState(items[i].state, items[i].value);
        }
    }
};

IDirect3DPixelShader9* LayerShader() {
    IDirect3DDevice9* device = At<IDirect3DDevice9*>(RW_D3D_DEVICE);
    if (!device) {
        return nullptr;
    }
    if (device != g_layerDevice) {
        // Outro device (o do SA-MP na frente do real, ou um novo): cria o shader nele. O antigo nao e liberado, o
        // device dele pode nao existir mais.
        g_layerDevice = device;
        g_layerShader = nullptr;
        g_layerShaderFailed = false;
    }
    if (!g_layerShader && !g_layerShaderFailed) {
        const HRESULT hr = device->CreatePixelShader(kLayerShader, &g_layerShader);
        if (FAILED(hr) || !g_layerShader) {
            g_layerShader = nullptr;
            g_layerShaderFailed = true;
            Log("aviso: a placa nao aceitou o shader das camadas (0x%08X) -- a sombra desfocada fica como no original",
                static_cast<unsigned>(hr));
        }
    }
    return g_layerShader;
}

// So no modo desfocado com o shader do Shadows Extender: e ele que desenha cada faixa. No modo combinado o stencil ja
// junta as sombras.
bool UseLayers() {
    return g_cur.layered && !SE<int>(se::COMBINE) && SE<uint8_t>(se::SHADER_READY) &&
           SE<IDirect3DPixelShader9*>(se::SHADER_REALTIME);
}

void SeFlush() {
    Fn<VoidFn>(g_seFlush)();
}

// O que esta no buffer de desenho (uma sombra em tempo real ja projetada no chao), em LAYERS passadas: o FLUSH do
// Shadows Extender desenha cada uma com o shader das camadas no lugar do dele.
void LayeredFlush() {
    const u32 vertices = At<u32>(TEMP_VERTICES_STORED), indices = At<u32>(TEMP_INDICES_STORED);
    IDirect3DDevice9* device = At<IDirect3DDevice9*>(RW_D3D_DEVICE);
    IDirect3DPixelShader9* shader = LayerShader();
    if (!vertices || !indices || !device || !shader) {
        SeFlush();
        return;
    }
    // A forca que o Shadows Extender poe em c0.a (a mesma conta dele) vezes o maximo da textura da sombra (o degrade
    // multiplica a sombra ate o maior dos dois valores dele). As faixas cobrem de 0 ate ai, com uma folga.
    float color[3];
    for (int i = 0; i < 3; i++) {
        color[i] = SE<int>(se::REALTIME_COLOR[i]) / 255.0f;
    }
    const float clouds = std::max(SE<float>(se::CLOUDS), 1.0f - At<float>(CLOUD_COVERAGE));
    const float night = std::max(SE<float>(se::NIGHT), 1.0f - At<float>(DN_BALANCE));
    const float strength = SE<int>(se::REALTIME_COLOR[3]) / 255.0f * clouds * night;
    const float texMax = g_cur.blur2 ? std::max(At<int>(GRADIENT_MAX), At<int>(GRADIENT_MIN)) / 255.0f : 1.0f;
    const float core = std::min(1.0f, strength * std::min(1.0f, texMax) * 1.02f);
    if (core < 1.0f / 255.0f) {
        SeFlush(); // fraca demais para aparecer
        return;
    }
    const float width = core / LAYERS;

    SavedStates saved;
    saved.Save(RS_STENCILENABLE, FALSE);
    saved.Save(RS_STENCILFUNCTION, FUNC_ALWAYS);
    saved.Save(RS_STENCILFUNCTIONREF, 0);
    saved.Save(RS_STENCILFUNCTIONMASK, 0xFFFFFFFF);
    saved.Save(RS_STENCILFUNCTIONWRITEMASK, 0xFFFFFFFF);
    saved.Save(RS_STENCILFAIL, STENCIL_KEEP);
    saved.Save(RS_STENCILZFAIL, STENCIL_KEEP);
    saved.Save(RS_STENCILPASS, STENCIL_KEEP);
    saved.Save(RS_ALPHATESTFUNCTION, FUNC_GREATER);
    saved.Save(RS_ALPHATESTFUNCTIONREF, 0);
    // A faixa j escurece o pixel cuja marca ainda e menor que j e marca j nele.
    SetState(RS_STENCILENABLE, TRUE);
    SetState(RS_STENCILFUNCTION, FUNC_GREATER);
    SetState(RS_STENCILFUNCTIONMASK, LAYER_MASK);
    SetState(RS_STENCILFUNCTIONWRITEMASK, LAYER_MASK);
    SetState(RS_STENCILFAIL, STENCIL_KEEP);
    SetState(RS_STENCILZFAIL, STENCIL_KEEP);
    SetState(RS_STENCILPASS, STENCIL_REPLACE);
    SetState(RS_ALPHATESTFUNCTION, FUNC_GREATER);

    // O teste de alfa descarta o pixel que nao chega na faixa: sem ele, a faixa marcaria o stencil em todo o quadrado
    // da sombra. O RenderWare liga com a mistura; aqui fica ligado na placa de qualquer jeito e volta no fim.
    DWORD alphaTest = FALSE, alphaFunc = D3DCMP_GREATER;
    const bool alphaKnown = SUCCEEDED(device->GetRenderState(D3DRS_ALPHATESTENABLE, &alphaTest)) &&
                            SUCCEEDED(device->GetRenderState(D3DRS_ALPHAFUNC, &alphaFunc));
    if (alphaKnown) {
        device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    }

    IDirect3DPixelShader9*& seShader = SE<IDirect3DPixelShader9*>(se::SHADER_REALTIME);
    IDirect3DPixelShader9* const original = seShader;
    seShader = shader;
    for (int j = 1; j <= LAYERS; j++) {
        const float low = (j - 1) * width;
        const float band[4] = {low, 1.0f / width, 0.0f, 0.0f};
        float scale[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        for (int i = 0; i < 3; i++) {
            const float darken = 1.0f - color[i];
            scale[i] = darken * width / (1.0f - darken * low);
        }
        device->SetPixelShaderConstantF(1, band, 1);
        device->SetPixelShaderConstantF(2, scale, 1);
        SetState(RS_STENCILFUNCTIONREF, static_cast<uintptr_t>(j) << LAYER_SHIFT);
        SetState(RS_ALPHATESTFUNCTIONREF, j == 1 ? 0 : static_cast<uintptr_t>(std::ceil(low * 255.0f)));
        At<u32>(TEMP_VERTICES_STORED) = vertices; // o desenho zera o buffer
        At<u32>(TEMP_INDICES_STORED) = indices;
        SeFlush();
    }
    seShader = original;
    if (alphaKnown) {
        device->SetRenderState(D3DRS_ALPHATESTENABLE, alphaTest);
        device->SetRenderState(D3DRS_ALPHAFUNC, alphaFunc);
    }
    saved.Restore();
    g_marked = true;
}

// SITE_CAST: projeta uma sombra em tempo real no chao de um setor (o CAST do Shadows Extender, que termina no FLUSH).
void __cdecl CastHook(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7, u32 a8, u32 a9, u32 a10, u32 a11,
                      u32 a12, u32 a13, u32 a14, u32 a15, u32 a16, u32 a17, u32 a18) {
    g_inCast = true;
    Fn<CastFn>(g_seCast)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18);
    g_inCast = false;
}

// O FLUSH do Shadows Extender: no fim do CAST e com o buffer cheio (SITE_FULL_*). Fora de uma sombra em tempo real
// (outros efeitos tambem enchem o buffer), segue igual ao original.
void __cdecl FlushHook() {
    if (g_inCast && UseLayers()) {
        LayeredFlush();
    } else {
        SeFlush();
    }
}

// SITE_RECT: antes do retangulo do stencil, apaga as marcas das camadas (so os bits delas, em toda a tela). Os
// estados do retangulo voltam como o jogo deixou.
void __cdecl StencilRectHook(const void* rect, const void* color) {
    if (g_marked) {
        g_marked = false;
        // Sem valor guardado, volta o que CStencilShadows::RenderStencilShadows deixa para o retangulo dele.
        SavedStates saved;
        saved.Save(RS_STENCILENABLE, TRUE);
        saved.Save(RS_STENCILFUNCTION, FUNC_LESSEQUAL);
        saved.Save(RS_STENCILFUNCTIONWRITEMASK, 0xFFFFFFFF);
        saved.Save(RS_STENCILFAIL, STENCIL_KEEP);
        saved.Save(RS_STENCILZFAIL, STENCIL_KEEP);
        saved.Save(RS_STENCILPASS, STENCIL_KEEP);
        saved.Save(RS_SRCBLEND, BLEND_SRCALPHA);
        saved.Save(RS_DESTBLEND, BLEND_INVSRCALPHA);
        saved.Save(RS_ZTESTENABLE, FALSE);
        saved.Save(RS_ALPHATESTFUNCTIONREF, 0);
        saved.Save(RS_VERTEXALPHAENABLE, TRUE);
        saved.Save(RS_TEXTURERASTER, 0);
        saved.Save(RS_SHADEMODE, SHADE_FLAT);
        SetState(RS_STENCILENABLE, TRUE);
        SetState(RS_STENCILFUNCTION, FUNC_ALWAYS);
        SetState(RS_STENCILFUNCTIONWRITEMASK, LAYER_MASK);
        SetState(RS_STENCILFAIL, STENCIL_KEEP);
        SetState(RS_STENCILZFAIL, STENCIL_KEEP);
        SetState(RS_STENCILPASS, STENCIL_ZERO);
        SetState(RS_SRCBLEND, BLEND_ZERO); // a cor da tela fica como esta
        SetState(RS_DESTBLEND, BLEND_ONE);
        SetState(RS_ZTESTENABLE, FALSE);
        SetState(RS_ALPHATESTFUNCTIONREF, 0);
        const uint8_t clear[4] = {0, 0, 0, 1}; // alfa 1: com mistura e passando no teste de alfa
        Fn<RectFn>(CSprite2d_DrawRect)(rect, clear);
        saved.Restore();
    }
    Fn<RectFn>(g_seRect)(rect, color);
}

bool CallsInto(uintptr_t site, uint8_t opcode, uintptr_t target) {
    return patch::Readable(site, 5) && At<uint8_t>(site) == opcode && patch::CallTarget(site) == target;
}

void PatchLayerSites() {
    patch::SetCall(SITE_CAST, reinterpret_cast<const void*>(&CastHook));
    patch::SetCall(SITE_FULL_1, reinterpret_cast<const void*>(&FlushHook));
    patch::SetCall(SITE_FULL_2, reinterpret_cast<const void*>(&FlushHook));
    patch::SetCall(SITE_RECT, reinterpret_cast<const void*>(&StencilRectHook));
}

void InstallLayers() {
    const uintptr_t cast = g_se + se::CAST, flush = g_se + se::FLUSH, rect = g_se + se::STENCIL_RECT;
    const bool ok = CallsInto(SITE_CAST, 0xE8, cast) && CallsInto(g_se + se::CAST_FLUSH_JMP, 0xE9, flush) &&
                    CallsInto(SITE_FULL_1, 0xE8, flush) && CallsInto(SITE_FULL_2, 0xE8, flush) &&
                    CallsInto(SITE_RECT, 0xE8, rect);
    if (!ok) {
        Log("aviso: o desenho da sombra em tempo real nao esta como o Shadows Extender deixa (outro mod?) -- a sombra "
            "desfocada fica como no original");
        return;
    }
    g_seCast = cast;
    g_seFlush = flush;
    g_seRect = rect;
    patch::SetJump(g_se + se::CAST_FLUSH_JMP, reinterpret_cast<const void*>(&FlushHook));
    PatchLayerSites();
    g_layersInstalled = true;
    Log("aplicado: com a sombra desfocada, onde duas sombras se cruzam escurece uma vez so");
}

// Se o Shadows Extender iniciar de novo (o evento do RenderWare dele), ele poe os desvios dele de volta nesses lugares.
void KeepLayers() {
    if (g_layersInstalled && CallsInto(SITE_CAST, 0xE8, g_seCast) && CallsInto(SITE_FULL_1, 0xE8, g_seFlush) &&
        CallsInto(SITE_FULL_2, 0xE8, g_seFlush) && CallsInto(SITE_RECT, 0xE8, g_seRect)) {
        PatchLayerSites();
        Log("os desvios do Shadows Extender voltaram (ele iniciou de novo) -- camadas ligadas de novo");
    }
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
    g_cur.layered = GetPrivateProfileIntA("TROK_MENU", "EscurecerUmaVez", 1, g_iniPath) != 0;
    g_saved = g_cur;
    g_good = g_cur;
    g_startMaxShadows = g_cur.maxShadows;
    g_attached = true;
    g_problem = nullptr;
    InstallLayers();

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

// Procura o Shadows Extender, liga o mod nele e recria as sombras quando pedido. Roda no evento de quadro e, se ele
// nao chega (outro mod tomou a chamada de CGame::Process sem repassar), no Present.
void Tick(const char* from) {
    if (g_attached) {
        KeepLayers();
        if (g_recreatePending) {
            Recreate();
        }
        return;
    }
    static bool first = true;
    if (first) {
        first = false;
        Log("procurando o Shadows Extender (pelo %s)", from);
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

} // namespace

void SeFrame() {
    g_frameEvents++;
    Tick("evento de quadro");
}

void SePresent() {
    const bool frameEventAlive = g_frameEvents != g_framesAtPresent;
    g_framesAtPresent = g_frameEvents;
    if (frameEventAlive) {
        return;
    }
    static bool logged = false;
    if (!logged && g_frameEvents == 0) {
        logged = true;
        Log("o evento de quadro (0x53E981) ainda nao chegou -- o Present faz o trabalho dele");
    }
    // No Present a cena ja acabou e nenhuma sombra esta guardada para desenhar. As sombras criadas de novo desenham
    // nas cameras delas: o alvo de desenho do jogo volta como estava.
    IDirect3DDevice9* device = At<IDirect3DDevice9*>(RW_D3D_DEVICE);
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* depth = nullptr;
    const bool recreating = g_attached && g_recreatePending && device;
    if (recreating) {
        device->GetRenderTarget(0, &target);
        device->GetDepthStencilSurface(&depth);
    }
    Tick("Present");
    if (recreating) {
        if (target) {
            device->SetRenderTarget(0, target);
            target->Release();
        }
        if (depth) {
            device->SetDepthStencilSurface(depth);
            depth->Release();
        }
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

bool LayersInstalled() {
    return g_layersInstalled;
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

extern "C" __declspec(dllexport) void TsmPresent() {
    SePresent();
}

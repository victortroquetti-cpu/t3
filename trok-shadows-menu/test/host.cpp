// Teste do Trok Shadows Menu.asi sem o jogo (Wine 32 bits), com o Shadows Extender 2.0 de verdade.
// Monta em 0x400000 a memoria que os dois conferem no gta_sa.exe 1.0 US, carrega o shadows.asi original e o
// complemento, dispara os eventos e chama os ganchos com dados falsos. As funcoes do jogo viram gravadores.
//
// Roda dentro do launcher.exe (test/launcher.c), que ocupa a faixa de enderecos do gta_sa.exe.
// Uso: launcher.exe completo | liga_desliga | renomeado | sem_evento | sem_original
//   completo:     o shadows.ini do Victor_Trok (tudo ligado): sombra em camadas, valores ao vivo, recriar, gravar
//   liga_desliga: tudo desligado no INI: liga e desliga ao vivo com os bytes originais; DisplayShadowsAtLowSettings
//                 do [STENCIL_SHADOWS] corrigido
//   renomeado:    o shadows.asi com outro nome: achado pelo conteudo
//   sem_evento:   o evento de quadro nunca chega (outro mod tomou a chamada): o Present acha o original
//   sem_original: sem o Shadows Extender: o complemento so avisa

#include <windows.h>
#include <d3d9.h>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "../src/settings.h"

typedef uint32_t u32;

// ------------------------------------------------------------------------------------------------ resultado
int g_pass = 0, g_fail = 0;

void Check(bool ok, const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s %s\n", ok ? "ok  " : "FALHA", buf);
    (ok ? g_pass : g_fail)++;
}

bool Near(float a, float b) {
    return fabsf(a - b) < 1e-4f;
}

// ------------------------------------------------------------------------------------------------ memoria do jogo
uint8_t* Mem(uintptr_t a) {
    return reinterpret_cast<uint8_t*>(a);
}
template <class T>
T& At(uintptr_t a) {
    return *reinterpret_cast<T*>(a);
}
void Put(uintptr_t a, std::initializer_list<uint8_t> bytes) {
    for (uint8_t b : bytes) {
        Mem(a++)[0] = b;
    }
}
void PutRel(uintptr_t site, uintptr_t target, uint8_t op) {
    Mem(site)[0] = op;
    At<int32_t>(site + 1) = static_cast<int32_t>(target - (site + 5));
}
void PutCall(uintptr_t site, uintptr_t target) {
    PutRel(site, target, 0xE8);
}
void PutJump(uintptr_t at, const void* fn) {
    PutRel(at, reinterpret_cast<uintptr_t>(fn), 0xE9);
}
uintptr_t Target(uintptr_t site) {
    return site + 5 + At<int32_t>(site + 1);
}
bool InExe(uintptr_t a) {
    return a >= 0x400000 && a < 0xD00000;
}
bool Bytes(uintptr_t a, std::initializer_list<uint8_t> bytes) {
    for (uint8_t b : bytes) {
        if (Mem(a++)[0] != b) {
            return false;
        }
    }
    return true;
}
bool AllNop(uintptr_t a, int n) {
    for (int i = 0; i < n; i++) {
        if (Mem(a + i)[0] != 0x90) {
            return false;
        }
    }
    return true;
}

// ------------------------------------------------------------------------------------------------ gravador
struct Rec {
    const char* name;
    u32 a[19];
    int n;
};
Rec g_rec[256];
int g_recCount = 0;

void Record(const char* name, int n, ...) {
    if (g_recCount >= 256) {
        return;
    }
    Rec& r = g_rec[g_recCount++];
    r.name = name;
    r.n = n;
    va_list args;
    va_start(args, n);
    for (int i = 0; i < n; i++) {
        r.a[i] = va_arg(args, u32);
    }
    va_end(args);
}

void ClearRec() {
    g_recCount = 0;
}

int CountRec(const char* name) {
    int c = 0;
    for (int i = 0; i < g_recCount; i++) {
        if (!strcmp(g_rec[i].name, name)) {
            c++;
        }
    }
    return c;
}

const Rec* FindRec(const char* name, int which = 0) {
    for (int i = 0; i < g_recCount; i++) {
        if (!strcmp(g_rec[i].name, name) && which-- == 0) {
            return &g_rec[i];
        }
    }
    return nullptr;
}

u32 FBits(float f) {
    u32 u;
    memcpy(&u, &f, 4);
    return u;
}
float FReal(u32 u) {
    float f;
    memcpy(&f, &u, 4);
    return f;
}

// ------------------------------------------------------------------------------------------------ funcoes falsas do jogo
// Estados do RenderWare: o falso guarda o ultimo valor de cada um e o devolve no Get, como o cache dele.
enum {
    RS_TEXTURERASTER = 1, RS_ZTESTENABLE = 6, RS_SRCBLEND = 10, RS_DESTBLEND = 11, RS_VERTEXALPHAENABLE = 12,
    RS_STENCILENABLE = 21, RS_STENCILFAIL = 22, RS_STENCILZFAIL = 23, RS_STENCILPASS = 24, RS_STENCILFUNCTION = 25,
    RS_STENCILFUNCTIONREF = 26, RS_STENCILFUNCTIONMASK = 27, RS_STENCILFUNCTIONWRITEMASK = 28,
    RS_ALPHATESTFUNCTION = 29, RS_ALPHATESTFUNCTIONREF = 30,
};
u32 g_rs[64];
void ResetStates() {
    memset(g_rs, 0, sizeof(g_rs));
    g_rs[RS_STENCILFUNCTION] = 8; // sempre
    g_rs[RS_STENCILFUNCTIONMASK] = g_rs[RS_STENCILFUNCTIONWRITEMASK] = 0xFFFFFFFF;
    g_rs[RS_STENCILFAIL] = g_rs[RS_STENCILZFAIL] = g_rs[RS_STENCILPASS] = 1; // manter
    g_rs[RS_ALPHATESTFUNCTION] = 5;                                         // maior
    g_rs[RS_ZTESTENABLE] = 1;
}
bool g_rectCallsNopDuringDraw = false;

void __cdecl FakeInitRw() {
    Record("InitRw original", 0);
}
void __cdecl FakeShutdownRw() {
    Record("ShutdownRw original", 0);
}
void __cdecl FakeInitGame() {
    Record("InitGame original", 0);
}
void __cdecl FakeGameProcess() {
    Record("GameProcess original", 0);
}
// Buffer de desenho do jogo (RenderBuffer): o "Cast" falso guarda um triangulo e o "Render" falso grava quantos
// vertices havia e a posicao do primeiro, e zera o buffer como o do jogo.
const uintptr_t kTempVerticesStored = 0xC4B950, kTempIndicesStored = 0xC4B954, kTempIndices = 0xC4B958,
                kTempVertices = 0xC4D958;
float g_castTriangle[3][3] = {{-1, 10, -10}, {1, 10, -10}, {0, 12, -10}}; // chao 10 m abaixo da camera (origem)
bool g_castAddsTriangle = true;

extern IDirect3DDevice9* g_real;
extern uintptr_t g_se;
// a[0] vertices; a[1..3] o primeiro; a[4] o shader de sombra do Shadows Extender (0x1F320) na hora do desenho;
// a[5..10] stencil: ligado, funcao, ref, mascara, mascara de escrita, passou; a[11..12] teste de alfa: ref, funcao;
// a[13..14] c1.xy; a[15..17] c2.rgb; a[18] c0.a.
void __cdecl RecRender() {
    const float* p = reinterpret_cast<float*>(kTempVertices);
    float c[12] = {};
    if (g_real) {
        g_real->GetPixelShaderConstantF(0, c, 3);
    }
    Record("RenderStuffInBuffer", 19, (u32)At<uint16_t>(kTempVerticesStored), FBits(p[0]), FBits(p[1]), FBits(p[2]),
           g_se ? At<u32>(g_se + 0x1F320) : 0u, g_rs[RS_STENCILENABLE], g_rs[RS_STENCILFUNCTION],
           g_rs[RS_STENCILFUNCTIONREF], g_rs[RS_STENCILFUNCTIONMASK], g_rs[RS_STENCILFUNCTIONWRITEMASK],
           g_rs[RS_STENCILPASS], g_rs[RS_ALPHATESTFUNCTIONREF], g_rs[RS_ALPHATESTFUNCTION], FBits(c[4]), FBits(c[5]),
           FBits(c[8]), FBits(c[9]), FBits(c[10]), FBits(c[3]));
    At<uint16_t>(kTempVerticesStored) = 0;
    At<uint16_t>(kTempIndicesStored) = 0;
}

void StoreTriangle() {
    const int base = At<uint16_t>(kTempVerticesStored);
    const int idx = At<uint16_t>(kTempIndicesStored);
    for (int i = 0; i < 3; i++) {
        float* v = reinterpret_cast<float*>(kTempVertices + (base + i) * 36);
        v[0] = g_castTriangle[i][0];
        v[1] = g_castTriangle[i][1];
        v[2] = g_castTriangle[i][2];
        At<uint16_t>(kTempIndices + (idx + i) * 2) = static_cast<uint16_t>(base + i);
    }
    At<uint16_t>(kTempVerticesStored) = static_cast<uint16_t>(base + 3);
    At<uint16_t>(kTempIndicesStored) = static_cast<uint16_t>(idx + 3);
}
int __cdecl RecRenderStateSet(int state, u32 value) {
    Record("RwRenderStateSet", 2, (u32)state, value);
    if (state >= 0 && state < 64) {
        g_rs[state] = value;
    }
    return 1;
}
int __cdecl RecRenderStateGet(int state, u32* out) {
    Record("RwRenderStateGet", 1, (u32)state);
    *out = state >= 0 && state < 64 ? g_rs[state] : 0;
    return 1;
}
bool g_castFillsBuffer = false; // o buffer enche no meio: RenderBuffer::StartStoring desenha (0x7082A4) e continua
void __cdecl RecCast(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7, u32 a8, u32 a9, u32 a10, u32 a11,
                     u32 a12, u32 a13, u32 a14, u32 a15, u32 a16, u32 a17, u32 a18) {
    Record("CastRealTimeShadowSectorList", 19, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
           a16, a17, a18);
    if (g_castAddsTriangle) {
        StoreTriangle();
        if (g_castFillsBuffer) {
            reinterpret_cast<void(__cdecl*)()>(0x7082A4 + 5 + *reinterpret_cast<int32_t*>(0x7082A5))();
            StoreTriangle();
        }
    }
}
bool __thiscall RecSphereVisible(void* camera, const float* center, u32 radius) {
    Record("IsSphereVisible", 3, (u32)(uintptr_t)camera, (u32)(uintptr_t)center, radius);
    return true;
}
void __cdecl RecStore(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7, u32 a8, u32 a9, u32 a10, u32 a11,
                      u32 a12, u32 a13, u32 a14, u32 a15) {
    Record("StoreShadowToBeRendered", 16, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15);
}
void __thiscall RecInvert(void* camera) {
    Record("InvertRaster", 1, (u32)(uintptr_t)camera);
}
void* __cdecl RecEndUpdate(void* camera) {
    Record("RwCameraEndUpdate", 1, (u32)(uintptr_t)camera);
    return camera;
}
void __cdecl RecSetVS(void* shader) {
    Record("_rwD3D9SetVertexShader", 1, (u32)(uintptr_t)shader);
}
void __cdecl RecSetPS(void* shader) {
    Record("_rwD3D9SetPixelShader", 1, (u32)(uintptr_t)shader);
}
// a[6..12]: stencil (ligado, funcao, passou, mascara de escrita), mistura (origem, destino), teste de profundidade.
void __cdecl RecDrawRect(void* rect, uint8_t* color) {
    g_rectCallsNopDuringDraw = AllNop(0x7FB81D, 5) && AllNop(0x7FB824, 5);
    Record("DrawRect", 13, (u32)(uintptr_t)rect, (u32)color[0], (u32)color[1], (u32)color[2], (u32)color[3],
           (u32)g_rectCallsNopDuringDraw, g_rs[RS_STENCILENABLE], g_rs[RS_STENCILFUNCTION], g_rs[RS_STENCILPASS],
           g_rs[RS_STENCILFUNCTIONWRITEMASK], g_rs[RS_SRCBLEND], g_rs[RS_DESTBLEND], g_rs[RS_ZTESTENABLE]);
}
void __thiscall RecDoShadow(void* manager, void* physical) {
    Record("DoShadowThisFrame", 2, (u32)(uintptr_t)manager, (u32)(uintptr_t)physical);
}
void __cdecl RecStoreVehicle(void* vehicle, u32 type) {
    Record("StoreShadowForVehicle", 2, (u32)(uintptr_t)vehicle, type);
}
void* __thiscall RecSetLight(void* shadow, u32 azimuth, u32 elevation, u32 setCam) {
    Record("SetLightProperties", 4, (u32)(uintptr_t)shadow, azimuth, elevation, setCam);
    return shadow;
}
void* __cdecl RecFrameRotate(void* frame, void* axis, u32 angle, u32 combine) {
    Record("RwFrameRotate", 4, (u32)(uintptr_t)frame, (u32)(uintptr_t)axis, angle, combine);
    return frame;
}
void* __cdecl RecTranslate(void* matrix, float* t, u32 op) {
    Record("RwMatrixTranslate", 4, (u32)(uintptr_t)matrix, (u32)(uintptr_t)t, FBits(t[1]), op);
    return matrix;
}
void __thiscall RecColSphere(void* sphere, u32 radius, u32 center, u32 material, u32 flags, u32 light) {
    Record("CColSphere::Set", 6, (u32)(uintptr_t)sphere, radius, center, material, flags, light);
}
void* __cdecl RecForAllAtomics(void* clump, void* cb, void* data) {
    Record("RpClumpForAllAtomics", 3, (u32)(uintptr_t)clump, (u32)(uintptr_t)cb, (u32)(uintptr_t)data);
    return clump;
}
void* __cdecl RecQuickRender(void* atomic, void* data) {
    // a[2] = flags da geometria durante o desenho (a silhueta tira textura, luz e cor).
    void* geometry = At<void*>((uintptr_t)atomic + 0x18);
    Record("atomicQuickRender", 3, (u32)(uintptr_t)atomic, (u32)At<u32>((uintptr_t)atomic + 0x6C),
           geometry ? At<u32>((uintptr_t)geometry + 8) : 0xFFFFFFFFu);
    (void)data;
    return atomic;
}
void* __thiscall RecRtUpdate(void* shadow) {
    Record("CRealTimeShadow::Update", 1, (u32)(uintptr_t)shadow);
    return reinterpret_cast<void*>(0x1234);
}
void __thiscall RecUpdateRpHAnim(void* entity) {
    Record("UpdateRpHAnim", 1, (u32)(uintptr_t)entity);
}
void __thiscall RecBonePos(void* ped, float* out, u32 bone, u32 update) {
    Record("GetBonePosition", 4, (u32)(uintptr_t)ped, (u32)(uintptr_t)out, bone, update);
    out[0] = out[1] = out[2] = 7.0f;
}
bool __cdecl RecStencilInit() {
    Record("CStencilShadows::Init original", 0);
    return true;
}

// ------------------------------------------------------------------------------------------------ device (proxy)
// O mod so usa 3 metodos do device: CreatePixelShader, SetPixelShader e SetPixelShaderConstantF. O proxy grava
// e repassa para o device de verdade do Wine (que valida o bytecode dos shaders).
IDirect3DDevice9* g_real = nullptr;
uintptr_t g_se = 0;
void* g_proxyVtbl[119];
struct Proxy {
    void** vtbl;
} g_proxy = {g_proxyVtbl};
int g_psCreated = 0;

HRESULT __stdcall ProxyCreatePS(void*, const DWORD* code, IDirect3DPixelShader9** out) {
    HRESULT hr = g_real ? g_real->CreatePixelShader(code, out) : E_FAIL;
    Record("CreatePixelShader", 1, (u32)hr);
    if (SUCCEEDED(hr)) {
        g_psCreated++;
    }
    return hr;
}
HRESULT __stdcall ProxySetPS(void*, IDirect3DPixelShader9* ps) {
    Record("SetPixelShader", 1, (u32)(uintptr_t)ps);
    return g_real->SetPixelShader(ps);
}
HRESULT __stdcall ProxySetPSConstF(void*, UINT reg, const float* data, UINT count) {
    Record("SetPixelShaderConstantF", 6, (u32)reg, FBits(data[0]), FBits(data[1]), FBits(data[2]), FBits(data[3]),
           (u32)count);
    return g_real->SetPixelShaderConstantF(reg, data, count);
}
void __stdcall ProxyTrap() {
    printf("FALHA metodo inesperado do device\n");
    ExitProcess(3);
}

bool CreateRealDevice() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "trokhost";
    RegisterClassA(&wc);
    HWND wnd = CreateWindowA("trokhost", "trokhost", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr,
                             wc.hInstance, nullptr);
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) {
        return false;
    }
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.hDeviceWindow = wnd;
    return SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                       &pp, &g_real));
}

// ------------------------------------------------------------------------------------------------ RwGlobals falso
struct FakeGlobals {
    void* curCamera;
} g_globals = {nullptr};

// ------------------------------------------------------------------------------------------------ stubs em asm
// Entram no meio do codigo "do jogo" com os registradores como o jogo deixaria e voltam por um jmp gravado no
// endereco de retorno do stub do mod.
extern "C" {
u32 g_saveEsp;
u32 g_inEax, g_inEdx, g_inEdi, g_inEsi, g_inEbx;
u32 g_outEax, g_outEcx, g_outEdx, g_outEbx, g_outEsi, g_outEdi, g_outEbp;
float g_outSt0;
}

extern "C" __attribute__((naked)) void HostEnterSun() {
    __asm__(".intel_syntax noprefix\n"
            "push ebp\n push ebx\n push esi\n push edi\n"
            "mov [_g_saveEsp], esp\n"
            "mov eax, [_g_inEax]\n mov edx, [_g_inEdx]\n mov edi, [_g_inEdi]\n mov esi, [_g_inEsi]\n"
            "mov ebx, [_g_inEbx]\n"
            "push 0x707E2B\n ret\n"
            ".att_syntax prefix\n");
}
extern "C" __attribute__((naked)) void HostBackSun() {
    __asm__(".intel_syntax noprefix\n"
            "fstp dword ptr [_g_outSt0]\n"
            "mov [_g_outEax], eax\n mov [_g_outEcx], ecx\n mov [_g_outEdx], edx\n mov [_g_outEbx], ebx\n"
            "mov [_g_outEsi], esi\n mov [_g_outEdi], edi\n"
            "mov esp, [_g_saveEsp]\n"
            "pop edi\n pop esi\n pop ebx\n pop ebp\n ret\n"
            ".att_syntax prefix\n");
}
extern "C" __attribute__((naked)) void HostBackExtras() {
    __asm__(".intel_syntax noprefix\n"
            "mov [_g_outEax], eax\n mov [_g_outEcx], ecx\n mov [_g_outEdx], edx\n mov [_g_outEbx], ebx\n"
            "mov [_g_outEsi], esi\n mov [_g_outEdi], edi\n mov [_g_outEbp], ebp\n"
            "mov esp, [_g_saveEsp]\n"
            "pop edi\n pop esi\n pop ebx\n pop ebp\n ret\n"
            ".att_syntax prefix\n");
}

// ------------------------------------------------------------------------------------------------ montagem
const uintptr_t kDistanceSites[] = {0x711943, 0x711954, 0x711969, 0x711977};
const uintptr_t kDistanceSqSites[] = {0x711827, 0x71183B, 0x7118DC, 0x711C05, 0x711D22};
const uintptr_t kQuadSites[] = {0x707EF7, 0x707F05, 0x707F13, 0x707F21};
const uintptr_t kProjectionSites[] = {0x70A211, 0x70A228};
const uintptr_t kJccSites[] = {0x711E3D, 0x711D9D, 0x7113C0, 0x706BCC, 0x5E6766, 0x7069F5};

void SetupGame() {
    // Eventos: chamadas para funcoes do host.
    PutCall(0x5BD779, reinterpret_cast<uintptr_t>(&FakeInitRw));
    PutCall(0x53BC21, reinterpret_cast<uintptr_t>(&FakeShutdownRw));
    PutCall(0x748CFB, reinterpret_cast<uintptr_t>(&FakeInitGame));
    PutCall(0x53E981, reinterpret_cast<uintptr_t>(&FakeGameProcess));

    // Chamadas do 1.0 US que o mod troca ou confere.
    const struct {
        uintptr_t site, target;
    } calls[] = {
        {0x53BCAB, 0x70F9E0}, {0x71167F, 0x727B60}, {0x6ABCF5, 0x70BDA0}, {0x6BD667, 0x70BDA0}, {0x6C0B21, 0x70BDA0},
        {0x6C58A0, 0x70BDA0}, {0x6CA73A, 0x70BDA0}, {0x707E4F, 0x705900}, {0x70596A, 0x7F1010}, {0x70A1AC, 0x7F2450},
        {0x7082A4, 0x707800}, {0x7082BD, 0x707800}, {0x70AD0D, 0x70A7E0}, {0x705C4A, 0x749B70}, {0x707F2C, 0x707390},
        {0x70A2C8, 0x40FD10}, {0x5B1F3C, 0x706BA0}, {0x5E6664, 0x532B20}, {0x706B29, 0x706600}, {0x707CF1, 0x5E4280},
        {0x705C57, 0x705660}, {0x705C5F, 0x7EE180}, {0x711E41, 0x710310}, {0x711E26, 0x70FAE0}, {0x7FB81D, 0x7F9FB0},
        {0x7FB824, 0x7F9FF0}, {0x70A0C9, 0x40FD10},
    };
    for (const auto& c : calls) {
        PutCall(c.site, c.target);
    }
    Put(0x706676, {0x8B, 0x07, 0x8A, 0x4E, 0x10}); // mov eax, [edi]; mov cl, [esi+10h]
    Put(0x707E2B, {0x8B, 0xCF, 0xD9, 0x40, 0x08}); // mov ecx, edi; fld dword ptr [eax+8]
    Put(0x7064C1, {0x6A, 7});
    Put(0x7064F8, {0x6A, 6});
    Put(0x70680F, {0x6A, 1});
    Put(0x706811, {0x6A, 4});
    Put(0x706813, {0x6A, 1});
    Put(0x706824, {0x6A, 6});
    Put(0x706831, {0x6A, 6});
    for (uintptr_t a : kJccSites) {
        Put(a, {0x75, 0x10});
    }
    Put(0x70BDAB, {0x0F, 0x85, 0x10, 0x00, 0x00, 0x00});
    Put(0x5E68A2, {0xB9, 0x50, 0x03, 0xC4, 0x00, 0x56}); // mov ecx, offset g_realTimeShadowMan; push esi
    PutCall(0x5E68A8, 0x706BA0);
    PutCall(0x707D20, 0x420D40); // CShadows::StoreRealTimeShadow: TheCamera.IsSphereVisible(posicao, 2)

    // Constantes do jogo (em .rdata) apontadas pelas instrucoes.
    At<float>(0x858000) = 50.0f;
    At<float>(0x858004) = 2500.0f;
    At<float>(0x858008) = 1.5f;
    At<float>(0x85800C) = 1.0f;
    for (uintptr_t s : kDistanceSites) At<u32>(s) = 0x858000;
    for (uintptr_t s : kDistanceSqSites) At<u32>(s) = 0x858004;
    for (uintptr_t s : kQuadSites) At<u32>(s) = 0x858008;
    for (uintptr_t s : kProjectionSites) At<u32>(s) = 0x85800C;

    // O que o SA-MP deixa: chamada do Idle apagada e ret no comeco do CRealTimeShadowManager::Update.
    memset(Mem(0x53EA08), 0x90, 10);
    Put(0x706AB0, {0xC3, 0x53, 0x57, 0x8B, 0xF9});

    // Funcoes do jogo chamadas pelo mod: jmp para os gravadores.
    PutJump(0x707800, reinterpret_cast<void*>(&RecRender));
    PutJump(0x7FE420, reinterpret_cast<void*>(&RecRenderStateSet));
    PutJump(0x7FD810, reinterpret_cast<void*>(&RecRenderStateGet));
    PutJump(0x70A7E0, reinterpret_cast<void*>(&RecCast));
    PutJump(0x707390, reinterpret_cast<void*>(&RecStore));
    PutJump(0x705660, reinterpret_cast<void*>(&RecInvert));
    PutJump(0x7EE180, reinterpret_cast<void*>(&RecEndUpdate));
    PutJump(0x7F9FB0, reinterpret_cast<void*>(&RecSetVS));
    PutJump(0x7F9FF0, reinterpret_cast<void*>(&RecSetPS));
    PutJump(0x727B60, reinterpret_cast<void*>(&RecDrawRect));
    PutJump(0x706BA0, reinterpret_cast<void*>(&RecDoShadow));
    PutJump(0x70BDA0, reinterpret_cast<void*>(&RecStoreVehicle));
    PutJump(0x705900, reinterpret_cast<void*>(&RecSetLight));
    PutJump(0x7F1010, reinterpret_cast<void*>(&RecFrameRotate));
    PutJump(0x7F2450, reinterpret_cast<void*>(&RecTranslate));
    PutJump(0x40FD10, reinterpret_cast<void*>(&RecColSphere));
    PutJump(0x749B70, reinterpret_cast<void*>(&RecForAllAtomics));
    PutJump(0x705620, reinterpret_cast<void*>(&RecQuickRender));
    PutJump(0x706600, reinterpret_cast<void*>(&RecRtUpdate));
    PutJump(0x532B20, reinterpret_cast<void*>(&RecUpdateRpHAnim));
    PutJump(0x5E4280, reinterpret_cast<void*>(&RecBonePos));
    PutJump(0x70F9E0, reinterpret_cast<void*>(&RecStencilInit));
    PutJump(0x420D40, reinterpret_cast<void*>(&RecSphereVisible));
    PutJump(0x70667B, reinterpret_cast<void*>(&HostBackExtras));
    PutJump(0x707E30, reinterpret_cast<void*>(&HostBackSun));

    // Dados.
    At<float>(0xC81304) = 0.25f; // nuvens
    At<float>(0x8D12C0) = 0.0f;  // dia
    At<void*>(0xC97B24) = &g_globals;
    for (int i = 0; i < 119; i++) {
        g_proxyVtbl[i] = reinterpret_cast<void*>(&ProxyTrap);
    }
    g_proxyVtbl[106] = reinterpret_cast<void*>(&ProxyCreatePS);
    g_proxyVtbl[107] = reinterpret_cast<void*>(&ProxySetPS);
    g_proxyVtbl[109] = reinterpret_cast<void*>(&ProxySetPSConstF);
    At<void*>(0xC97C28) = &g_proxy;
}

void FireEvent(uintptr_t site) {
    reinterpret_cast<void(__cdecl*)()>(Target(site))();
}

bool LogHas(const char* text) {
    FILE* f = fopen("Trok Shadows Menu.log", "rb");
    if (!f) {
        return false;
    }
    static char buf[65536];
    const size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    return strstr(buf, text) != nullptr;
}

bool FileHas(const char* path, const char* text) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    static char buf[65536];
    const size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    return strstr(buf, text) != nullptr;
}

void WriteIni(const char* text) {
    FILE* f = fopen("shadows.ini", "wb");
    fputs(text, f);
    fclose(f);
}

// Troca a importacao de MessageBoxW do .asi (a caixa travaria o teste).
int g_messageBoxes = 0;
int WINAPI FakeMessageBoxW(HWND, LPCWSTR, LPCWSTR, UINT) {
    g_messageBoxes++;
    return IDOK;
}
void PatchImport(HMODULE mod, const char* dll, const char* fn, void* replacement) {
    uint8_t* base = reinterpret_cast<uint8_t*>(mod);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); imp->Name; imp++) {
        if (lstrcmpiA(reinterpret_cast<char*>(base + imp->Name), dll)) {
            continue;
        }
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imp->OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++) {
            auto* byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            if (!strcmp(reinterpret_cast<char*>(byName->Name), fn)) {
                DWORD old;
                VirtualProtect(&slots->u1.Function, 4, PAGE_READWRITE, &old);
                slots->u1.Function = reinterpret_cast<uintptr_t>(replacement);
                VirtualProtect(&slots->u1.Function, 4, old, &old);
            }
        }
    }
}


// ------------------------------------------------------------------------------------------------ gerente de sombras falso
// CRealTimeShadowManager::Init/Exit e ReturnRealTimeShadow: o Init falso cria 16 sombras com as cameras (ou sem, para
// imitar a placa recusando uma resolucao alta) e grava os valores dos push que o jogo usaria.
bool g_refuseBig = false;
const uintptr_t kManager = 0xC40350;

void __thiscall RecManagerInit(void* manager) {
    Record("Manager::Init", 4, (u32)Mem(0x7064C2)[0], (u32)Mem(0x7064F9)[0], (u32)Mem(0x706812)[0],
           (u32)Mem(0x706810)[0]);
    if (At<uint8_t>(kManager)) {
        return;
    }
    const bool refuse = g_refuseBig && Mem(0x7064C2)[0] > 9;
    for (int i = 0; i < 16; i++) {
        uint8_t* s = new uint8_t[0x4C]();
        At<void*>((uintptr_t)s + 0x08) = refuse ? nullptr : reinterpret_cast<void*>(0x7000 + i); // m_camera
        At<uint8_t>((uintptr_t)s + 0x10) = Mem(0x706814)[0];                                       // m_bBlurred
        At<void*>((uintptr_t)s + 0x14) = reinterpret_cast<void*>(0x7100 + i);                      // m_blurCamera
        At<u32>((uintptr_t)s + 0x1C) = Mem(0x706812)[0];                                           // m_nBlurPasses
        At<uint8_t>((uintptr_t)s + 0x20) = Mem(0x706810)[0];                                       // m_bDrawMoreBlur
        At<void*>(kManager + 4 + 4 * i) = s;
    }
    At<void*>(kManager + 0x44) = reinterpret_cast<void*>(0x7200);
    At<void*>(kManager + 0x4C) = reinterpret_cast<void*>(0x7300);
    At<uint8_t>(kManager) = 1;
}

void __thiscall RecManagerExit(void* manager) {
    Record("Manager::Exit", 0);
    for (int i = 0; i < 16; i++) {
        delete[] At<uint8_t*>(kManager + 4 + 4 * i);
        At<void*>(kManager + 4 + 4 * i) = nullptr;
    }
    At<uint8_t>(kManager) = 0;
}

void __thiscall RecReturnShadow(void* manager, void* shadow) {
    Record("ReturnRealTimeShadow", 2, (u32)(uintptr_t)manager, (u32)(uintptr_t)shadow);
    void* owner = At<void*>((uintptr_t)shadow);
    At<void*>((uintptr_t)owner + 0x134) = nullptr;
    At<void*>((uintptr_t)shadow) = nullptr;
}

// ------------------------------------------------------------------------------------------------ objetos falsos
struct Fake {
    alignas(16) uint8_t bytes[0x800];
};

void* NewEntity(int type, bool withModel, uintptr_t clump = 0x1111) {
    Fake* e = new Fake();
    memset(e, 0, sizeof(*e));
    At<uint8_t>((uintptr_t)e + 0x36) = static_cast<uint8_t>(type);
    At<void*>((uintptr_t)e + 0x18) = withModel ? reinterpret_cast<void*>(clump) : nullptr;
    At<float>((uintptr_t)e + 0x04) = 1.0f;
    At<float>((uintptr_t)e + 0x08) = 2.0f;
    At<float>((uintptr_t)e + 0x0C) = 3.0f;
    return e;
}

// ------------------------------------------------------------------------------------------------ o complemento
typedef Settings*(__cdecl* CurrentFn)();
typedef void(__cdecl* ApplyFn)(const Settings*);
typedef bool(__cdecl* SaveFn)();
CurrentFn TsmCurrent;
ApplyFn TsmApply;
SaveFn TsmSave;
typedef void(__cdecl* PresentFn)();
PresentFn TsmPresent;

template <class T>
T& SE(u32 offset) {
    return *reinterpret_cast<T*>(g_se + offset);
}

// Muda os valores como o menu faz: copia, mexe e manda aplicar.
template <class F>
void Change(F f) {
    Settings* s = TsmCurrent();
    const Settings before = *s;
    f(*s);
    TsmApply(&before);
}

HMODULE ModuleOf(uintptr_t addr) {
    HMODULE m = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(addr), &m);
    return m;
}

// Carrega o original (com o nome dado) e o complemento, e liga o jogo ate o primeiro quadro.
HMODULE g_menu = nullptr;
bool Boot(const char* originalName, bool frameEvent = true) {
    if (!CreateRealDevice()) {
        printf("aviso: sem device D3D9 no Wine\n");
    }
    SetupGame();
    ResetStates();
    PutJump(0x7067C0, reinterpret_cast<void*>(&RecManagerInit));
    PutJump(0x706A60, reinterpret_cast<void*>(&RecManagerExit));
    PutJump(0x705B30, reinterpret_cast<void*>(&RecReturnShadow));
    At<void*>(0xC97C28) = g_real; // device de verdade: o desenho do menu e o shader do original usam
    if (originalName) {
        g_se = reinterpret_cast<uintptr_t>(LoadLibraryA(originalName));
        Check(g_se != 0, "LoadLibrary do %s (Shadows Extender 2.0 original) em 0x%08X", originalName, (unsigned)g_se);
        if (!g_se) {
            return false;
        }
    }
    g_menu = LoadLibraryA("Trok Shadows Menu.asi");
    Check(g_menu != nullptr, "LoadLibrary do Trok Shadows Menu.asi");
    if (!g_menu) {
        return false;
    }
    TsmCurrent = reinterpret_cast<CurrentFn>(GetProcAddress(g_menu, "TsmCurrent"));
    TsmApply = reinterpret_cast<ApplyFn>(GetProcAddress(g_menu, "TsmApply"));
    TsmSave = reinterpret_cast<SaveFn>(GetProcAddress(g_menu, "TsmSave"));
    TsmPresent = reinterpret_cast<PresentFn>(GetProcAddress(g_menu, "TsmPresent"));
    Check(Mem(0x53E981)[0] == 0xE8 && ModuleOf(Target(0x53E981)) == g_menu,
          "evento de quadro ligado (0x53E981) pelo complemento");
    FireEvent(0x5BD779);                 // o original le o shadows.ini e remenda o jogo
    RecManagerInit(reinterpret_cast<void*>(kManager)); // CGame::Init3 cria as 16 sombras
    FireEvent(0x748CFB);
    ClearRec();
    if (frameEvent) {
        FireEvent(0x53E981); // primeiro quadro: o complemento acha o original
    }
    return true;
}

// ------------------------------------------------------------------------------------------------ testes
typedef void(__thiscall* PedFn)(void*);

void TestAttach() {
    printf("\n-- o complemento acha o original\n");
    Check(LogHas("Shadows Extender 2.0 em 0x") && LogHas("procurando o Shadows Extender (pelo evento de quadro)"),
          "log: Shadows Extender 2.0 achado no evento de quadro");
    Check(ModuleOf(Target(0x70AD0D)) == g_menu && ModuleOf(Target(0x7082A4)) == g_menu &&
              ModuleOf(Target(0x7082BD)) == g_menu && ModuleOf(Target(0x71167F)) == g_menu,
          "projecao, buffer cheio e retangulo do stencil passam pelo complemento");
    Check(Mem(g_se + 0x3766)[0] == 0xE9 && ModuleOf(Target(g_se + 0x3766)) == g_menu,
          "o fim do CAST do original desenha pelo complemento");
    Check(Target(0x5E6664) == g_se + 0x12F0 && Target(0x706676) == g_se + 0x3120,
          "os ganchos do pedestre e da camera ficam os do original (cada um com a sua sombra)");
    Check(LogHas("aplicado: com a sombra desfocada, onde duas sombras se cruzam escurece uma vez so"),
          "log: sombra em camadas ligada");
}

void TestValuesRead() {
    printf("\n-- valores em uso lidos do original\n");
    const Settings* s = TsmCurrent();
    Check(s->maxShadows == 500 && Near(s->stencilDistance, 180.0f) && s->flagIgnoreSome && s->disableBuildings &&
              s->stencilLow,
          "stencil: 500 sombras, 180 m, os tres liga/desliga ligados");
    bool colors = true;
    const int rgba[4] = {5, 12, 20, 80};
    for (int i = 0; i < 4; i++) colors = colors && s->stencilColor[i] == rgba[i] && s->realtimeColor[i] == rgba[i];
    Check(colors, "cores 5,12,20 e forca 80 (stencil e tempo real)");
    Check(s->realtimeLow && s->combine && Near(s->realtimeDistance, 100.0f) && s->blur1 && s->blurLevel == 4 &&
              s->blur2 && s->raster == 10 && s->blurRaster == 9 && s->raster2 == 9 && s->blurRaster2 == 9 &&
              s->gradientMax == 128 && s->gradientMin == 128,
          "tempo real: combinado, 100 m, raster 10/9/9/9, desfoque 1/4/1, degrade 128/128");
    Check(Near(s->bound, 10.0f) && Near(s->boundAir, 20.0f) && Near(s->sunZ, 0.6f) && Near(s->zLimit, 8.0f) &&
              Near(s->zLimitAir, 10.0f) && Near(s->night, 0.2f) && Near(s->clouds, 0.4f),
          "projecao 10/20, sol 0.6, alcance 8/10, noite 0.2, nuvens 0.4");
    Check(s->vehicleDefaultWithRealtime && !s->disableVehicleDefault && s->shader && s->morePlayers && s->layered,
          "sombra simples junto, shader, MoreThanOnePlayer e as camadas ligados");
}

// ------------------------------------------------------------------------------------------------ sombra em camadas
typedef void(__cdecl* CastFn)(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32,
                              u32);
typedef void(__cdecl* RectFn)(const void*, const void*);

// O jogo projeta uma sombra em tempo real (CShadows::RenderStoredShadows chamando o CAST do original em 0x70AD0D).
void CastShadow() {
    static uint8_t owner[0x800];
    static uint8_t shadow[0x4C];
    *reinterpret_cast<void**>(shadow) = owner; // o CAST do original guarda o dono da sombra
    u32 a[19] = {};
    a[17] = (u32)(uintptr_t)shadow;
    reinterpret_cast<CastFn>(Target(0x70AD0D))(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11],
                                               a[12], a[13], a[14], a[15], a[16], a[17], a[18]);
}

struct Pass {
    u32 shader, stencilOn, stencilFunc, stencilRef, stencilMask, writeMask, passOp, alphaRef, alphaFunc;
    float low, invWidth, k[3], strength;
};

Pass PassOf(const Rec& r) {
    Pass p;
    p.shader = r.a[4];
    p.stencilOn = r.a[5];
    p.stencilFunc = r.a[6];
    p.stencilRef = r.a[7];
    p.stencilMask = r.a[8];
    p.writeMask = r.a[9];
    p.passOp = r.a[10];
    p.alphaRef = r.a[11];
    p.alphaFunc = r.a[12];
    p.low = FReal(r.a[13]);
    p.invWidth = FReal(r.a[14]);
    for (int i = 0; i < 3; i++) p.k[i] = FReal(r.a[15 + i]);
    p.strength = FReal(r.a[18]);
    return p;
}

// Um pixel de tela pelas passadas gravadas: shader das camadas, teste de alfa (maior que a ref) e stencil (a ref maior
// que a marca), como a placa faria. s = forca x textura da sombra ali.
void DrawPixel(const Pass* passes, int n, float s, float rgb[3], u32* stencil) {
    for (int i = 0; i < n; i++) {
        const Pass& p = passes[i];
        const int alpha = static_cast<int>(s * 255.0f + 0.5f);
        if (alpha <= static_cast<int>(p.alphaRef)) continue;
        if ((p.stencilRef & p.stencilMask) <= (*stencil & p.stencilMask)) continue;
        float t = (s - p.low) * p.invWidth;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        for (int c = 0; c < 3; c++) rgb[c] *= 1.0f - t * p.k[c];
        *stencil = (*stencil & ~p.writeMask) | (p.stencilRef & p.writeMask);
    }
}

void TestLayers() {
    printf("\n-- sombra desfocada em camadas: o cruzamento escurece uma vez so\n");
    Change([](Settings& s) { s.combine = false; });
    const u32 seShader = At<u32>(g_se + 0x1F320);
    ClearRec();
    CastShadow();
    Pass passes[8];
    int n = 0;
    for (int i = 0; i < g_recCount && n < 8; i++) {
        if (!strcmp(g_rec[i].name, "RenderStuffInBuffer")) passes[n++] = PassOf(g_rec[i]);
    }
    Check(CountRec("CastRealTimeShadowSectorList") == 1 && n == 4, "uma sombra projetada vira 4 desenhos (%d)", n);
    bool same = n == 4;
    for (int i = 0; i < n; i++) {
        const Rec* r = FindRec("RenderStuffInBuffer", i);
        same = same && r->a[0] == 3 && passes[i].shader && passes[i].shader != seShader && passes[i].stencilOn == 1 &&
               passes[i].stencilFunc == 5 && passes[i].stencilRef == (u32)(i + 1) << 4 && passes[i].stencilMask == 0x70 &&
               passes[i].writeMask == 0x70 && passes[i].passOp == 3 && passes[i].alphaFunc == 5;
    }
    Check(same, "cada desenho: os 3 vertices, o shader das camadas, stencil faixa 1 a 4 nos bits 4-6");
    Check(At<u32>(g_se + 0x1F320) == seShader && g_rs[RS_STENCILENABLE] == 0 && g_rs[RS_ALPHATESTFUNCTIONREF] == 0 &&
              g_rs[RS_STENCILFUNCTION] == 8,
          "depois: o shader do original e os estados voltam");
    // A forca do original: A/255 x max(nuvens, 1 - cobertura) x max(noite, 1 - noite agora) = 120/255 x 0.75 x 1.
    const float strength = 120.0f / 255.0f * 0.75f;
    const float width = strength * 1.02f / 4; // degrade desligado no TestLive: a textura vai ate 1
    bool bands = n == 4;
    for (int i = 0; i < n; i++) {
        const float low = i * width;
        const float expectRef = i == 0 ? 0 : ceilf(low * 255.0f);
        bands = bands && fabsf(passes[i].strength - strength) < 1e-4f && fabsf(passes[i].low - low) < 1e-4f &&
                fabsf(passes[i].invWidth * width - 1.0f) < 1e-3f && passes[i].alphaRef == (u32)expectRef;
        const float color[3] = {40 / 255.0f, 12 / 255.0f, 20 / 255.0f};
        for (int c = 0; c < 3; c++) {
            const float d = 1 - color[c];
            bands = bands && fabsf(passes[i].k[c] - d * width / (1 - d * low)) < 1e-4f;
        }
    }
    Check(bands, "faixas de %.3f: comeco, largura, ref de alfa e a cor de cada uma", width);

    // O que a placa faria com essas passadas, pixel a pixel.
    const float d[3] = {1 - 40 / 255.0f, 1 - 12 / 255.0f, 1 - 20 / 255.0f};
    float worstSingle = 0, worstDark = 0, worstLight = 0, worstOriginal = 0;
    for (int a = 0; a <= 40; a++) {
        const float sa = strength * a / 40;
        float rgb[3] = {1, 1, 1};
        u32 st = 0;
        DrawPixel(passes, n, sa, rgb, &st);
        for (int c = 0; c < 3; c++) worstSingle = fmaxf(worstSingle, fabsf(rgb[c] - (1 - d[c] * sa)));
        for (int b = 0; b <= 40; b++) {
            const float sb = strength * b / 40;
            float two[3] = {1, 1, 1};
            u32 st2 = 0;
            DrawPixel(passes, n, sa, two, &st2);
            DrawPixel(passes, n, sb, two, &st2);
            const float strongest = fmaxf(sa, sb);
            for (int c = 0; c < 3; c++) {
                const float ideal = 1 - d[c] * strongest;
                worstDark = fmaxf(worstDark, ideal - two[c]);
                worstLight = fmaxf(worstLight, two[c] - ideal);
                worstOriginal = fmaxf(worstOriginal, ideal - (1 - d[c] * sa) * (1 - d[c] * sb));
            }
        }
    }
    Check(worstSingle < 0.006f, "sozinha, a sombra sai igual a do original (diferenca %.4f)", worstSingle);
    Check(worstDark < 0.006f && worstLight <= d[0] * width + 0.006f,
          "duas cruzadas: escurece como a mais forte (mais escuro %.4f, mais claro %.4f; no original: %.4f mais escuro)",
          worstDark, worstLight, worstOriginal);

    // Buffer cheio no meio da sombra: as duas metades em camadas.
    g_castFillsBuffer = true;
    ClearRec();
    CastShadow();
    g_castFillsBuffer = false;
    Check(CountRec("RenderStuffInBuffer") == 8, "buffer cheio no meio da sombra: as duas partes em camadas (%d)",
          CountRec("RenderStuffInBuffer"));
    // Outro efeito enchendo o buffer, fora de uma sombra em tempo real: segue igual ao original.
    StoreTriangle();
    ClearRec();
    reinterpret_cast<void(__cdecl*)()>(Target(0x7082A4))();
    const Rec* other = FindRec("RenderStuffInBuffer");
    Check(CountRec("RenderStuffInBuffer") == 1 && other && other->a[4] == seShader && other->a[5] == 0,
          "outro efeito com o buffer cheio: um desenho, como no original");

    // Retangulo do stencil: antes dele, as marcas das camadas saem (so os bits 4-6), e os estados voltam.
    g_rs[RS_STENCILFUNCTION] = 4; // como CStencilShadows::RenderStencilShadows deixa
    g_rs[RS_STENCILPASS] = 1;
    g_rs[RS_STENCILFUNCTIONWRITEMASK] = 0xFFFFFFFF;
    g_rs[RS_SRCBLEND] = 5;
    g_rs[RS_DESTBLEND] = 6;
    g_rs[RS_ZTESTENABLE] = 0;
    g_rs[RS_STENCILENABLE] = 1;
    const float rect[4] = {0, 0, 640, 480};
    const uint8_t color[4] = {0, 0, 0, 50};
    ClearRec();
    reinterpret_cast<RectFn>(Target(0x71167F))(rect, color);
    const Rec* clear = FindRec("DrawRect", 0);
    const Rec* game = FindRec("DrawRect", 1);
    Check(CountRec("DrawRect") == 2 && clear && clear->a[0] == (u32)(uintptr_t)rect && clear->a[4] == 1 &&
              clear->a[6] == 1 && clear->a[7] == 8 && clear->a[8] == 2 && clear->a[9] == 0x70 && clear->a[10] == 1 &&
              clear->a[11] == 2,
          "antes do retangulo do stencil: um retangulo que zera so os bits 4-6, sem mudar a cor");
    Check(game && game->a[7] == 4 && game->a[8] == 1 && game->a[9] == 0xFFFFFFFF && game->a[10] == 5 &&
              game->a[11] == 6,
          "depois o retangulo do original, com os estados do jogo");
    ClearRec();
    reinterpret_cast<RectFn>(Target(0x71167F))(rect, color);
    Check(CountRec("DrawRect") == 1, "sem camadas no quadro: so o retangulo do original");
    ResetStates();

    // Quando nao entra: modo combinado (o stencil ja junta), camadas desligadas no menu, shader desligado.
    Change([](Settings& s) { s.combine = true; });
    ClearRec();
    CastShadow();
    Check(CountRec("RenderStuffInBuffer") == 1, "modo combinado: um desenho, o do original");
    Change([](Settings& s) {
        s.combine = false;
        s.layered = false;
    });
    ClearRec();
    CastShadow();
    Check(CountRec("RenderStuffInBuffer") == 1 && FindRec("RenderStuffInBuffer")->a[5] == 0,
          "desligado no menu: um desenho, sem stencil");
    Change([](Settings& s) {
        s.layered = true;
        s.shader = false;
    });
    ClearRec();
    CastShadow();
    Check(CountRec("RenderStuffInBuffer") == 1, "sem o shader do original: um desenho, como ele");
    Change([](Settings& s) { s.shader = true; });

    // O original iniciou de novo e pos os desvios dele de volta: no quadro seguinte as camadas voltam.
    PutCall(0x70AD0D, g_se + 0x36E0);
    PutCall(0x7082A4, g_se + 0x3510);
    PutCall(0x7082BD, g_se + 0x3510);
    PutCall(0x71167F, g_se + 0x1350);
    FireEvent(0x53E981);
    Check(ModuleOf(Target(0x70AD0D)) == g_menu && ModuleOf(Target(0x7082A4)) == g_menu &&
              ModuleOf(Target(0x7082BD)) == g_menu && ModuleOf(Target(0x71167F)) == g_menu &&
              LogHas("os desvios do Shadows Extender voltaram"),
          "o original pos os desvios dele de novo: as camadas voltam no quadro seguinte");
}

void TestLive() {
    printf("\n-- valores mudados com o jogo aberto\n");
    Change([](Settings& s) {
        s.combine = false;
        s.realtimeColor[0] = 40;
        s.realtimeColor[3] = 120;
        s.stencilColor[1] = 33;
    });
    Check(SE<int>(0x1F30C) == 0 && SE<int>(0x1F3E8) == 40 && SE<int>(0x1F3F0) == 120 && SE<int>(0x1F404) == 33,
          "modo desfocado e cores no original na hora");
    Change([](Settings& s) { s.stencilDistance = 90.0f; });
    const float* d = *reinterpret_cast<float**>(0x711943);
    const float* d2 = *reinterpret_cast<float**>(0x711827);
    Check(Near(*d, 90.0f) && Near(*d2, 8100.0f), "stencil a 90 m: o jogo le pelos ponteiros que o original pos");
    Change([](Settings& s) { s.realtimeDistance = 60.0f; });
    Check(Near(At<float>(0x8D5240), 60.0f) && Near(At<float>(0xC4B6B0), 3600.0f), "sombra em tempo real ate 60 m");
    Change([](Settings& s) {
        s.bound = 6;
        s.boundAir = 12;
        s.sunZ = 0.5f;
        s.zLimit = 5;
        s.zLimitAir = 7;
        s.night = 0.3f;
        s.clouds = 0.5f;
        s.disableVehicleDefault = true;
    });
    Check(Near(SE<float>(0x1F3D0), 6) && Near(SE<float>(0x1F3C4), 12) && Near(SE<float>(0x1F3EC), 0.5f) &&
              Near(SE<float>(0x1F384), 5) && Near(SE<float>(0x1F3C0), 7) && Near(SE<float>(0x1F314), 0.3f) &&
              Near(SE<float>(0x1F3A8), 0.5f) && SE<int>(0x1F410) == 1,
          "projecao, sol, alcance, noite, nuvens e sombra simples dos veiculos no original");
    Change([](Settings& s) {
        s.blurLevel = 6;
        s.blur2 = false;
    });
    bool shadows = true;
    for (int i = 0; i < 16; i++) {
        uint8_t* sh = At<uint8_t*>(kManager + 4 + 4 * i);
        shadows = shadows && At<u32>((uintptr_t)sh + 0x1C) == 6 && At<uint8_t>((uintptr_t)sh + 0x20) == 0;
    }
    Check(Mem(0x706812)[0] == 6 && Mem(0x706810)[0] == 0 && shadows,
          "desfoque 6 e degrade desligado nas 16 sombras, sem recriar");
    Change([](Settings& s) { s.flagIgnoreSome = false; });
    Check(AllNop(0x711E3D, 2) && LogHas("FlagIgnoreSomeShadows=0 vale quando o jogo abrir de novo"),
          "FlagIgnoreSomeShadows: o original ja tinha trocado o byte e o exe no disco nao confere -- fica para depois");
    Change([](Settings& s) { s.shader = false; });
    const bool off = Mem(g_se + 0x1F301)[0] == 0;
    Change([](Settings& s) { s.shader = true; });
    Check(off && Mem(g_se + 0x1F301)[0] == 1, "shader do original desligado e religado na hora");
}

void TestRecreate() {
    printf("\n-- resolucao nova: sombras recriadas\n");
    void* ped = NewEntity(3, true);
    void* slot0 = At<void*>(kManager + 4);
    At<void*>((uintptr_t)slot0) = ped;
    At<void*>((uintptr_t)ped + 0x134) = slot0;
    ClearRec();
    Change([](Settings& s) {
        s.raster = 8;
        s.blurRaster = s.raster2 = s.blurRaster2 = 7;
    });
    Check(CountRec("Manager::Init") == 0, "nada recriado no meio do quadro");
    FireEvent(0x53E981);
    int ret = -1, exitAt = -1, init = -1;
    for (int i = 0; i < g_recCount; i++) {
        if (!strcmp(g_rec[i].name, "ReturnRealTimeShadow")) ret = i;
        if (!strcmp(g_rec[i].name, "Manager::Exit")) exitAt = i;
        if (!strcmp(g_rec[i].name, "Manager::Init")) init = i;
    }
    Check(ret >= 0 && g_rec[ret].a[0] == kManager && g_rec[ret].a[1] == (u32)(uintptr_t)slot0 && ret < exitAt &&
              exitAt < init && g_rec[init].a[0] == 8 && g_rec[init].a[1] == 7,
          "no quadro seguinte: devolve a sombra do dono, Exit, Init com 256/128");
    Check(At<void*>((uintptr_t)ped + 0x134) == nullptr, "o dono fica sem a sombra velha");
    Check(LogHas("sombras em tempo real recriadas: resolucao 8/7/7/7"), "log: recriadas");

    g_refuseBig = true;
    ClearRec();
    Change([](Settings& s) {
        s.raster = 10;
        s.blurRaster = s.raster2 = s.blurRaster2 = 9;
    });
    FireEvent(0x53E981);
    const Rec* i1 = FindRec("Manager::Init", 0);
    const Rec* i2 = FindRec("Manager::Init", 1);
    Check(i1 && i1->a[0] == 10 && i2 && i2->a[0] == 8 && TsmCurrent()->raster == 8,
          "a placa recusou 1024: recria com 256 de novo");
    Check(LogHas("a placa nao criou as sombras com resolucao 10/9 -- voltou para 8/7"), "log: voltou");
    g_refuseBig = false;
}

void TestSave() {
    printf("\n-- shadows.ini gravado sem perder os comentarios\n");
    Check(TsmSave(), "gravou");
    const char* ini = "shadows.ini";
    Check(FileHas(ini, "CombineRealTimeShadowsWithStencil=0 ; this will draw real-time shadows with stencil"),
          "valor trocado, comentario do Shadows Extender mantido");
    Check(FileHas(ini, "MaxDistance=90.0          ; default 50.0") &&
              FileHas(ini, "MaxDistance=60.0          ; default 15.0"),
          "as duas MaxDistance (secoes diferentes), mesmo alinhamento");
    Check(FileHas(ini, "RasterSize=8              ; default 7") && FileHas(ini, "BlurLevel=6                ; default 4"),
          "RasterSize 8 e BlurLevel 6");
    Check(FileHas(ini, "MaxShadows=500             ; default 64") && FileHas(ini, "ShadowSunZLimit=0.5"),
          "o que nao mudou fica igual; o que mudou com o formato do original");
    Check(FileHas(ini, "FlagIgnoreSomeShadows=0   ; default 1"), "liga/desliga gravado para quando o jogo abrir");
    Check(!FileHas(ini, "TROK_MENU"), "sem secao nova se as camadas nao mudaram");
    Change([](Settings& s) { s.layered = false; });
    TsmSave();
    Check(FileHas(ini, "MoreThanOnePlayer=1 ; Use for SA:MP\r\n\r\n[TROK_MENU]\r\nEscurecerUmaVez=0\r\n"),
          "camadas desligadas: secao [TROK_MENU] no fim");
}

int RunFull() {
    if (!Boot("shadows.asi")) {
        return 1;
    }
    TestAttach();
    TestValuesRead();
    TestLive();
    TestLayers();
    TestRecreate();
    TestSave();
    return 0;
}

// Tudo desligado no INI: os bytes originais ficam na memoria e o complemento liga e desliga ao vivo.
int RunToggles() {
    if (!Boot("shadows.asi")) {
        return 1;
    }
    printf("\n-- DisplayShadowsAtLowSettings do [STENCIL_SHADOWS] corrigido\n");
    Check(AllNop(0x711D9D, 2) && AllNop(0x7113C0, 2) && Bytes(0x706BCC, {0x75}) && Bytes(0x5E6766, {0x75}),
          "stencil no grafico baixo ligado pela chave dele; tempo real continua desligado");
    Check(LogHas("corrigido: DisplayShadowsAtLowSettings do [STENCIL_SHADOWS] = 1"), "log: corrigido");
    printf("\n-- liga e desliga ao vivo\n");
    struct Case {
        const char* name;
        bool Settings::*field;
        uintptr_t addr;
        int size;
        uint8_t on[6], off[6];
    };
    const Case cases[] = {
        {"FlagIgnoreSomeShadows", &Settings::flagIgnoreSome, 0x711E3D, 2, {0x90, 0x90}, {0x75, 0x10}},
        {"DisableBuildingShadows", &Settings::disableBuildings, 0x711E41, 5, {0x90, 0x90, 0x90, 0x90, 0x90},
         {0xE8, Mem(0x711E42)[0], Mem(0x711E43)[0], Mem(0x711E44)[0], Mem(0x711E45)[0]}},
        {"tempo real no grafico baixo", &Settings::realtimeLow, 0x706BCC, 1, {0xEB}, {0x75}},
        {"DrawVehicleDefaultShadowWithRealTime", &Settings::vehicleDefaultWithRealtime, 0x70BDAB, 6,
         {0x90, 0x90, 0x90, 0x90, 0x90, 0x90}, {0x0F, 0x85, 0x10, 0x00, 0x00, 0x00}},
        {"MoreThanOnePlayer", &Settings::morePlayers, 0x7069F5, 1, {0xEB}, {0x75}},
    };
    for (const Case& c : cases) {
        Change([&](Settings& s) { s.*(c.field) = true; });
        const bool on = !memcmp(Mem(c.addr), c.on, c.size);
        Change([&](Settings& s) { s.*(c.field) = false; });
        const bool off = !memcmp(Mem(c.addr), c.off, c.size);
        Check(on && off, "%s: liga e volta ao byte original", c.name);
    }
    Check(Target(0x711E41) == 0x710310 && Bytes(0x5E6766, {0x75}), "a chamada e o segundo lugar voltam inteiros");
    return 0;
}

int RunRenamed() {
    CopyFileA("shadows.asi", "sombras extender.asi", FALSE);
    if (!Boot("sombras extender.asi")) {
        return 1;
    }
    printf("\n-- shadows.asi com outro nome\n");
    Check(LogHas("Shadows Extender 2.0 em 0x"), "achado pelo conteudo do modulo");
    Check(ModuleOf(Target(0x70AD0D)) == g_menu && LogHas("aplicado: com a sombra desfocada"), "camadas ligadas");
    return 0;
}

// Outro mod tomou a chamada de CGame::Process e o evento de quadro nunca chega: o Present acha o original.
int RunNoFrameEvent() {
    if (!Boot("shadows.asi", false)) {
        return 1;
    }
    printf("\n-- sem o evento de quadro\n");
    Check(!LogHas("Shadows Extender 2.0 em 0x"), "sem quadro, ainda nao achou");
    TsmPresent();
    Check(LogHas("o evento de quadro (0x53E981) ainda nao chegou") && LogHas("procurando o Shadows Extender (pelo Present)") &&
              LogHas("Shadows Extender 2.0 em 0x"),
          "o Present acha o original");
    Check(ModuleOf(Target(0x70AD0D)) == g_menu, "e liga as camadas");
    // Resolucao nova: sem evento de quadro, o Present recria.
    ClearRec();
    Change([](Settings& s) {
        s.raster = 8;
        s.blurRaster = s.raster2 = s.blurRaster2 = 7;
    });
    TsmPresent();
    const Rec* init = FindRec("Manager::Init");
    Check(CountRec("Manager::Exit") == 1 && init && init->a[0] == 8, "o Present recria as sombras com a resolucao nova");
    // Com o evento de quadro chegando, o Present nao faz nada.
    FireEvent(0x53E981);
    ClearRec();
    Change([](Settings& s) { s.raster = 9; });
    TsmPresent();
    Check(CountRec("Manager::Init") == 0, "com o evento de quadro chegando, o Present deixa para ele");
    return 0;
}

int RunMissing() {
    if (!Boot(nullptr)) {
        return 1;
    }
    printf("\n-- sem o Shadows Extender\n");
    for (int i = 0; i < 600; i++) {
        FireEvent(0x53E981);
    }
    Check(LogHas("o Shadows Extender 2.0 (shadows.asi do DK22Pac) nao foi achado"), "log: avisa");
    Check(Target(0x5E6664) == 0x532B20 && Bytes(0x706676, {0x8B, 0x07, 0x8A, 0x4E, 0x10}) &&
              Target(0x70AD0D) == 0x70A7E0 && Target(0x7082A4) == 0x707800 && Target(0x71167F) == 0x727B60 &&
              Mem(0x7064C2)[0] == 7,
          "nada do jogo alterado");
    return 0;
}

extern "C" __declspec(dllexport) int HostMain(const char* mode, uintptr_t gameBegin) {
    (void)gameBegin;
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (!strcmp(mode, "liga_desliga")) {
        RunToggles();
    } else if (!strcmp(mode, "renomeado")) {
        RunRenamed();
    } else if (!strcmp(mode, "sem_evento")) {
        RunNoFrameEvent();
    } else if (!strcmp(mode, "sem_original")) {
        RunMissing();
    } else {
        RunFull();
    }
    printf("\n%s: %d ok, %d falhas\n", mode, g_pass, g_fail);
    fflush(stdout);
    // O desenho do menu tem uma thread esperando o SA-MP: sai sem esperar por ela.
    TerminateProcess(GetCurrentProcess(), g_fail ? 1 : 0);
    return g_fail ? 1 : 0;
}

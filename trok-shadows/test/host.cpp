// Teste do Trok Shadows.asi sem o jogo (Wine 32 bits).
// Monta em 0x400000 a memoria que o mod confere no gta_sa.exe 1.0 US (chamadas, bytes, constantes), carrega o
// .asi, dispara os eventos e chama cada gancho com dados falsos. As funcoes do jogo que o mod chama viram
// gravadores: o teste confere argumentos, ordem e convencao de chamada (o que o Shadows Extender fazia).
//
// Roda dentro do launcher.exe (test/launcher.c), que ocupa a faixa de enderecos do gta_sa.exe.
// Uso: launcher.exe completo | errado | conflito | atualizar
//   completo: tudo (precisa de shadows.ini na pasta: o mod importa os valores dele)
//   errado:   memoria zerada (outro exe): o mod tem que ficar desligado sem escrever nada
//   conflito: um "shadows.asi" carregado antes: o mod tem que ficar desligado e avisar
//   atualizar: INI da versao 1.0 (sem as chaves novas): o mod reescreve com os mesmos valores e as chaves novas

#include <windows.h>
#include <d3d9.h>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

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
void __cdecl RecRender() {
    Record("RenderStuffInBuffer", 0);
}
int __cdecl RecRenderStateSet(int state, u32 value) {
    Record("RwRenderStateSet", 2, (u32)state, value);
    return 1;
}
int __cdecl RecRenderStateGet(int state, u32* out) {
    Record("RwRenderStateGet", 1, (u32)state);
    *out = 0x55;
    return 1;
}
void __cdecl RecCast(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7, u32 a8, u32 a9, u32 a10, u32 a11,
                     u32 a12, u32 a13, u32 a14, u32 a15, u32 a16, u32 a17, u32 a18) {
    Record("CastRealTimeShadowSectorList", 19, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
           a16, a17, a18);
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
void __cdecl RecDrawRect(void* rect, uint8_t* color) {
    g_rectCallsNopDuringDraw = AllNop(0x7FB81D, 5) && AllNop(0x7FB824, 5);
    Record("DrawRect", 6, (u32)(uintptr_t)rect, (u32)color[0], (u32)color[1], (u32)color[2], (u32)color[3],
           (u32)g_rectCallsNopDuringDraw);
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
extern "C" __attribute__((naked)) void HostEnterExtras() {
    __asm__(".intel_syntax noprefix\n"
            "push ebp\n push ebx\n push esi\n push edi\n"
            "mov [_g_saveEsp], esp\n"
            "mov eax, [_g_inEax]\n mov edx, [_g_inEdx]\n mov edi, [_g_inEdi]\n mov esi, [_g_inEsi]\n"
            "mov ebx, [_g_inEbx]\n mov ebp, 0x0BADF00D\n"
            "push 0x706676\n ret\n"
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
    FILE* f = fopen("Trok Shadows.log", "rb");
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
    FILE* f = fopen("Trok Shadows.ini", "wb");
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

// ------------------------------------------------------------------------------------------------ objetos falsos
// Pedestre / veiculo / sombra com so os campos que o mod le.
struct Fake {
    alignas(16) uint8_t bytes[0x800];
};

void* NewEntity(int type, bool withModel, int vehicleSubType = 0) {
    Fake* e = new Fake();
    memset(e, 0, sizeof(*e));
    At<uint8_t>((uintptr_t)e + 0x36) = static_cast<uint8_t>(type);
    At<void*>((uintptr_t)e + 0x18) = withModel ? reinterpret_cast<void*>(0x1111) : nullptr;
    At<int>((uintptr_t)e + 0x594) = vehicleSubType;
    At<float>((uintptr_t)e + 0x04) = 1.0f; // posicao (sem matriz)
    At<float>((uintptr_t)e + 0x08) = 2.0f;
    At<float>((uintptr_t)e + 0x0C) = 3.0f;
    return e;
}

void* NewShadow(void* owner, void* camera) {
    Fake* s = new Fake();
    memset(s, 0, sizeof(*s));
    At<void*>((uintptr_t)s + 0x00) = owner;
    At<void*>((uintptr_t)s + 0x08) = camera;
    At<uint8_t>((uintptr_t)s + 0x10) = 0xA5; // m_bBlurred (o stub repete "mov cl, [esi+10h]")
    return s;
}

// ------------------------------------------------------------------------------------------------ testes
typedef void(__cdecl* VoidFn)();

void TestPatches() {
    printf("\n-- remendos depois do evento RenderWare iniciado\n");
    const uintptr_t hooked[] = {0x53BCAB, 0x71167F, 0x6ABCF5, 0x6BD667, 0x6C0B21, 0x6C58A0, 0x6CA73A,
                                0x707E4F, 0x70596A, 0x70A1AC, 0x7082A4, 0x7082BD, 0x70AD0D, 0x705C4A,
                                0x707F2C, 0x70A2C8, 0x5B1F3C, 0x5E6664, 0x706B29, 0x707CF1};
    int inDll = 0;
    for (uintptr_t s : hooked) {
        inDll += Mem(s)[0] == 0xE8 && !InExe(Target(s));
    }
    Check(inDll == 20, "20 chamadas desviadas para o .asi (%d)", inDll);
    Check(Mem(0x706676)[0] == 0xE9 && !InExe(Target(0x706676)), "jmp do extras em 0x706676");
    Check(Mem(0x707E2B)[0] == 0xE9 && !InExe(Target(0x707E2B)), "jmp do sol em 0x707E2B");
    Check(AllNop(0x705C57, 5) && AllNop(0x705C5F, 5), "InvertRaster/EndUpdate de CShadowCamera::Update viram NOP");
    Check(AllNop(0x5E68A2, 11), "pedido condicional do pedestre vira NOP (11 bytes)");
    Check(AllNop(0x711E26, 5), "sombra stencil de veiculo vira NOP");
    Check(AllNop(0x70A0C9, 5), "0x70A0C9 vira NOP");
    // Valores do shadows.ini do usuario.
    Check(Mem(0x7064C2)[0] == 10 && Mem(0x7064F9)[0] == 9 && Mem(0x706825)[0] == 9 && Mem(0x706832)[0] == 9,
          "Raster 10/9/9/9");
    Check(Mem(0x706814)[0] == 1 && Mem(0x706810)[0] == 1 && Mem(0x706812)[0] == 4, "Blur 1/1/4");
    Check(At<int>(0x8D5218) == 128 && At<int>(0x8D521C) == 128, "Gradient 128/128");
    Check(Near(At<float>(0x8D5240), 100.0f) && Near(At<float>(0xC4B6B0), 10000.0f), "MaxDistance tempo real 100");
    bool dist = true;
    for (uintptr_t s : kDistanceSites) dist = dist && Near(*reinterpret_cast<float*>(At<u32>(s)), 180.0f);
    for (uintptr_t s : kDistanceSqSites) dist = dist && Near(*reinterpret_cast<float*>(At<u32>(s)), 32400.0f);
    Check(dist, "MaxDistance stencil 180 (e 32400 nos 5 quadrados)");
    bool quad = true;
    for (uintptr_t s : kQuadSites) quad = quad && Near(*reinterpret_cast<float*>(At<u32>(s)), 2.15f);
    for (uintptr_t s : kProjectionSites) quad = quad && Near(*reinterpret_cast<float*>(At<u32>(s)), 1.15f);
    Check(quad, "constantes 2.15 e 1.15");
    Check(Bytes(0x711E3D, {0x90, 0x90}), "FlagIgnoreSomeShadows=1");
    Check(AllNop(0x711E41, 5), "DisableBuildingShadows=1");
    Check(Bytes(0x711D9D, {0x90, 0x90}) && Bytes(0x7113C0, {0x90, 0x90}), "stencil DisplayShadowsAtLowSettings=1");
    Check(Mem(0x706BCC)[0] == 0xEB && Mem(0x5E6766)[0] == 0xEB, "tempo real DisplayShadowsAtLowSettings=1");
    Check(AllNop(0x70BDAB, 6), "DrawVehicleDefaultShadowWithRealTime=1");
    Check(Mem(0x7069F5)[0] == 0xEB, "MoreThanOnePlayer=1");
    Check(g_psCreated == 2, "os 2 pixel shaders aceitos pelo device do Wine (%d)", g_psCreated);
}

void TestStencil() {
    printf("\n-- sombras stencil\n");
    ClearRec();
    reinterpret_cast<bool(__cdecl*)()>(Target(0x53BCAB))();
    uint8_t* pool = At<uint8_t*>(0xC6A168);
    bool linked = pool && At<void*>(0xC6A16C) == nullptr;
    for (int i = 0; linked && i < 500; i++) {
        uint8_t* obj = pool + i * 0x1C;
        linked = At<void*>((uintptr_t)obj + 0x14) == (i < 499 ? obj + 0x1C : nullptr) &&
                 At<void*>((uintptr_t)obj + 0x18) == (i > 0 ? obj - 0x1C : nullptr);
    }
    Check(linked && CountRec("CStencilShadows::Init original") == 0, "pool de 500 objetos encadeado");

    ClearRec();
    uint8_t color[4] = {0, 0, 0, 50};
    float rect[4] = {0, 0, 640, 480};
    reinterpret_cast<void(__cdecl*)(void*, void*)>(Target(0x71167F))(rect, color);
    const Rec* draw = FindRec("DrawRect");
    // A = 80 * max(1 - 0.25, 0.4) * max(1 - 0, 0.2) = 60
    Check(draw && draw->a[1] == 5 && draw->a[2] == 12 && draw->a[3] == 20 && draw->a[4] == 60,
          "retangulo com cor 5,12,20 e forca 60 (%u,%u,%u,%u)", draw ? draw->a[1] : 0, draw ? draw->a[2] : 0,
          draw ? draw->a[3] : 0, draw ? draw->a[4] : 0);
    Check(draw && draw->a[5] == 1, "as 2 chamadas do Im2D ficam NOP durante o retangulo");
    Check(Mem(0x7FB81D)[0] == 0xE8 && Target(0x7FB81D) == 0x7F9FB0 && Target(0x7FB824) == 0x7F9FF0,
          "e voltam depois");
    Check(g_recCount == 4 && !strcmp(g_rec[0].name, "_rwD3D9SetVertexShader") &&
              !strcmp(g_rec[1].name, "_rwD3D9SetPixelShader") && !strcmp(g_rec[3].name, "_rwD3D9SetPixelShader"),
          "ordem: SetVertexShader(0), SetPixelShader(0), DrawRect, SetPixelShader(0)");
}

void TestRealtime(void* ped, void* heli, void* noModel) {
    printf("\n-- sombras em tempo real\n");
    // Veiculo: pede a sombra em tempo real e guarda a simples.
    ClearRec();
    reinterpret_cast<void(__cdecl*)(void*, u32)>(Target(0x6ABCF5))(heli, 1);
    const Rec* ds = FindRec("DoShadowThisFrame");
    const Rec* sv = FindRec("StoreShadowForVehicle");
    Check(ds && ds->a[0] == 0xC40350 && ds->a[1] == (u32)(uintptr_t)heli && sv && sv->a[1] == 1,
          "veiculo: DoShadowThisFrame(g_realTimeShadowMan, veiculo) + StoreShadowForVehicle(veiculo, 1)");

    // Pedestre: UpdateRpHAnim e depois DoShadowThisFrame.
    ClearRec();
    reinterpret_cast<void(__thiscall*)(void*)>(Target(0x5E6664))(ped);
    Check(g_recCount == 2 && !strcmp(g_rec[0].name, "UpdateRpHAnim") && g_rec[0].a[0] == (u32)(uintptr_t)ped &&
              !strcmp(g_rec[1].name, "DoShadowThisFrame") && g_rec[1].a[1] == (u32)(uintptr_t)ped,
          "pedestre: UpdateRpHAnim + DoShadowThisFrame");
    ClearRec();
    reinterpret_cast<void(__thiscall*)(void*)>(Target(0x5E6664))(noModel);
    Check(g_recCount == 1, "pedestre sem modelo: so UpdateRpHAnim");

    // Cutscene.
    ClearRec();
    reinterpret_cast<void(__thiscall*)(void*, void*)>(Target(0x5B1F3C))(reinterpret_cast<void*>(0xC40350), noModel);
    reinterpret_cast<void(__thiscall*)(void*, void*)>(Target(0x5B1F3C))(reinterpret_cast<void*>(0xC40350), ped);
    Check(CountRec("DoShadowThisFrame") == 1, "cutscene: so pede sombra de quem tem modelo");

    // CRealTimeShadow::Update so com dono com modelo.
    void* shadowPed = NewShadow(ped, reinterpret_cast<void*>(0x2222));
    void* shadowNone = NewShadow(noModel, reinterpret_cast<void*>(0x2222));
    ClearRec();
    void* r1 = reinterpret_cast<void*(__thiscall*)(void*)>(Target(0x706B29))(shadowNone);
    void* r2 = reinterpret_cast<void*(__thiscall*)(void*)>(Target(0x706B29))(shadowPed);
    Check(!r1 && r2 == reinterpret_cast<void*>(0x1234) && CountRec("CRealTimeShadow::Update") == 1,
          "atualizar sombra: pula dono sem modelo");

    // Posicao do osso: sem modelo, a posicao do pedestre.
    float out[3] = {0, 0, 0};
    ClearRec();
    reinterpret_cast<void(__thiscall*)(void*, float*, u32, u32)>(Target(0x707CF1))(noModel, out, 1, 0);
    Check(g_recCount == 0 && out[0] == 1.0f && out[1] == 2.0f && out[2] == 3.0f, "osso sem modelo: posicao do pedestre");
    reinterpret_cast<void(__thiscall*)(void*, float*, u32, u32)>(Target(0x707CF1))(ped, out, 1, 0);
    Check(CountRec("GetBonePosition") == 1 && out[0] == 7.0f, "osso com modelo: GetBonePosition do jogo");

    // Luz: guarda a elevacao e usa no RwFrameRotate.
    ClearRec();
    void* s = NewShadow(ped, nullptr);
    reinterpret_cast<void*(__thiscall*)(void*, u32, u32, u32)>(Target(0x707E4F))(s, FBits(30.0f), FBits(-55.0f), 1);
    reinterpret_cast<void*(__cdecl*)(void*, void*, u32, u32)>(Target(0x70596A))(reinterpret_cast<void*>(0x3333),
                                                                               reinterpret_cast<void*>(0x4444),
                                                                               FBits(-90.0f), 2);
    const Rec* sl = FindRec("SetLightProperties");
    const Rec* fr = FindRec("RwFrameRotate");
    Check(sl && sl->a[0] == (u32)(uintptr_t)s && FReal(sl->a[1]) == 30.0f && FReal(sl->a[2]) == -55.0f &&
              sl->a[3] == 1,
          "SetLightProperties recebe os mesmos argumentos (thiscall)");
    Check(fr && fr->a[0] == 0x3333 && fr->a[1] == 0x4444 && FReal(fr->a[2]) == -55.0f && fr->a[3] == 2,
          "RwFrameRotate usa a elevacao (-55) no lugar de -90");

    // Translacao da projecao: y = 0.5.
    ClearRec();
    float t[3] = {9, 9, 9};
    reinterpret_cast<void*(__cdecl*)(void*, float*, u32)>(Target(0x70A1AC))(reinterpret_cast<void*>(0x5555), t, 1);
    const Rec* tr = FindRec("RwMatrixTranslate");
    Check(tr && FReal(tr->a[2]) == 0.5f && t[0] == 9.0f && t[2] == 9.0f, "RwMatrixTranslate com y = 0.5");

    // StoreShadowToBeRendered: distancia vertical do INI (8 / 10 no ar), sem dono com modelo nao guarda.
    for (int k = 0; k < 3; k++) {
        void* owner = k == 0 ? ped : k == 1 ? heli : noModel;
        void* sh = NewShadow(owner, nullptr);
        ClearRec();
        reinterpret_cast<void(__cdecl*)(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32,
                                        u32)>(Target(0x707F2C))(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, FBits(4.0f), 13, 14,
                                                                (u32)(uintptr_t)sh, 16);
        const Rec* st = FindRec("StoreShadowToBeRendered");
        if (k == 2) {
            Check(!st, "StoreShadowToBeRendered: dono sem modelo nao guarda");
            continue;
        }
        bool same = st != nullptr;
        for (int i = 0; same && i < 16; i++) {
            if (i != 11 && i != 14) same = st->a[i] == (u32)(i + 1);
        }
        Check(same && FReal(st->a[11]) == (k == 0 ? 8.0f : 10.0f) && st->a[14] == (u32)(uintptr_t)sh,
              "StoreShadowToBeRendered: 16 argumentos iguais, z = %s", k == 0 ? "8 (pedestre)" : "10 (helicoptero)");
    }
}

void TestShader(void* ped, void* heli, bool combine) {
    printf("\n-- shader (%s)\n", combine ? "modo combinado" : "cor propria");
    for (int k = 0; k < 2; k++) {
        void* owner = k == 0 ? ped : heli;
        void* sh = NewShadow(owner, nullptr);
        ClearRec();
        u32 args[19];
        for (int i = 0; i < 19; i++) args[i] = 100 + i;
        args[17] = (u32)(uintptr_t)sh;
        reinterpret_cast<void(__cdecl*)(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32,
                                        u32, u32, u32)>(Target(0x70AD0D))(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10],
            args[11], args[12], args[13], args[14], args[15], args[16], args[17], args[18]);
        const Rec* cast = FindRec("CastRealTimeShadowSectorList");
        bool same = cast != nullptr;
        for (int i = 0; same && i < 19; i++) same = cast->a[i] == args[i];
        if (k == 0) {
            Check(same, "CastRealTimeShadowSectorList recebe os 19 argumentos");
            // Ordem: Cast, SetPixelShader, SetPixelShaderConstantF, [8 estados], Render, SetPixelShader(0), [estado]
            const Rec* ps = FindRec("SetPixelShader");
            const Rec* pc = FindRec("SetPixelShaderConstantF");
            Check(ps && ps->a[0] != 0 && pc && pc->a[0] == 0 && pc->a[5] == 1, "pixel shader e c0 antes do desenho");
            if (combine) {
                Check(pc && FReal(pc->a[1]) == 0 && FReal(pc->a[2]) == 0 && FReal(pc->a[3]) == 0 &&
                          FReal(pc->a[4]) == 1,
                      "c0 = (0, 0, 0, 1)");
                const u32 states[8][2] = {{10, 1}, {11, 2}, {21, 1}, {27, 0xFFFFFFFF},
                                          {28, 0xFFFFFFFF}, {25, 7}, {26, 0}, {24, 4}};
                bool ok = CountRec("RwRenderStateSet") == 9;
                for (int i = 0; ok && i < 8; i++) {
                    const Rec* r = FindRec("RwRenderStateSet", i);
                    ok = r && r->a[0] == states[i][0] && r->a[1] == states[i][1];
                }
                const Rec* last = FindRec("RwRenderStateSet", 8);
                Check(ok && last && last->a[0] == 21 && last->a[1] == 0,
                      "estados do stencil (8) antes e STENCILENABLE=0 depois");
            } else {
                // c0 = (5/255, 12/255, 20/255, 80/255 * 0.75 * 1)
                Check(pc && Near(FReal(pc->a[1]), 5 / 255.0f) && Near(FReal(pc->a[2]), 12 / 255.0f) &&
                          Near(FReal(pc->a[3]), 20 / 255.0f) && Near(FReal(pc->a[4]), 80 / 255.0f * 0.75f),
                      "c0 = cor do INI e forca com nuvens (%.4f)", pc ? FReal(pc->a[4]) : 0);
                Check(CountRec("RwRenderStateSet") == 0, "sem estados de stencil");
            }
            int render = -1, unset = -1, set = -1;
            for (int i = 0; i < g_recCount; i++) {
                if (!strcmp(g_rec[i].name, "RenderStuffInBuffer")) render = i;
                if (!strcmp(g_rec[i].name, "SetPixelShader") && g_rec[i].a[0] == 0) unset = i;
                if (!strcmp(g_rec[i].name, "SetPixelShader") && g_rec[i].a[0] != 0) set = i;
            }
            Check(set >= 0 && set < render && render < unset, "desenha com o shader e tira depois");
        }
        // CColSphere::Set depois de projetar: raio 10 (pedestre) ou 20 (helicoptero).
        ClearRec();
        reinterpret_cast<void(__thiscall*)(void*, u32, u32, u32, u32, u32)>(Target(0x70A2C8))(
            reinterpret_cast<void*>(0x6666), FBits(2.0f), 0x7777, 1, 2, 3);
        const Rec* cs = FindRec("CColSphere::Set");
        const float radius = k == 0 ? 10.0f : 20.0f;
        Check(cs && cs->a[0] == 0x6666 && FReal(cs->a[1]) == radius && cs->a[2] == 0x7777 && cs->a[3] == 1 &&
                  cs->a[4] == 2 && cs->a[5] == 3,
              "CColSphere::Set com raio %g (thiscall, 5 argumentos)", radius);
    }
    // Buffer cheio fora da projecao: so desenha, sem shader.
    ClearRec();
    reinterpret_cast<VoidFn>(Target(0x7082A4))();
    reinterpret_cast<VoidFn>(Target(0x7082BD))();
    Check(g_recCount == 2 && CountRec("RenderStuffInBuffer") == 2, "buffer cheio fora da sombra: sem shader");
}

void TestAtomics(void* ped, void* heli) {
    printf("\n-- partes desenhadas na camera da sombra\n");
    ClearRec();
    reinterpret_cast<void*(__cdecl*)(void*, void*, void*)>(Target(0x705C4A))(reinterpret_cast<void*>(0x8888),
                                                                            reinterpret_cast<void*>(0x705620),
                                                                            reinterpret_cast<void*>(0x9999));
    const Rec* fa = FindRec("RpClumpForAllAtomics");
    Check(fa && fa->a[0] == 0x8888 && fa->a[1] != 0x705620 && fa->a[2] == 0x9999,
          "RpClumpForAllAtomics com o callback do mod");
    if (!fa) {
        return;
    }
    auto cb = reinterpret_cast<void*(__cdecl*)(void*, void*)>(fa->a[1]);
    Fake* atomic = new Fake();
    memset(atomic, 0, sizeof(*atomic));
    const uintptr_t a = reinterpret_cast<uintptr_t>(atomic);
    At<void*>(a + 0x6C) = reinterpret_cast<void*>(0xABCD); // pipeline
    // Dono pedestre (via o gancho do Update).
    void* sp = NewShadow(ped, nullptr);
    reinterpret_cast<void*(__thiscall*)(void*)>(Target(0x706B29))(sp);
    ClearRec();
    At<uint8_t>(a + 2) = 0; // rpATOMICRENDER desligado
    cb(atomic, nullptr);
    Check(g_recCount == 0, "atomic invisivel: nao desenha");
    At<uint8_t>(a + 2) = 4;
    At<u32>(a + 0x48) = 0x7331E0; // LOD de veiculo
    cb(atomic, nullptr);
    Check(g_recCount == 0, "LOD de veiculo: nao desenha");
    At<u32>(a + 0x48) = 0x123456;
    cb(atomic, nullptr);
    const Rec* qr = FindRec("atomicQuickRender");
    Check(qr && qr->a[1] == 0xABCD && At<u32>(a + 0x6C) == 0xABCD, "pedestre: desenha com o pipeline dele");
    // Dono veiculo: pipeline nulo durante o desenho.
    void* sv = NewShadow(heli, nullptr);
    reinterpret_cast<void*(__thiscall*)(void*)>(Target(0x706B29))(sv);
    ClearRec();
    cb(atomic, nullptr);
    qr = FindRec("atomicQuickRender");
    Check(qr && qr->a[1] == 0 && At<u32>(a + 0x6C) == 0xABCD, "veiculo: pipeline nulo no desenho e volta depois");
    // Helice no modo combinado: ALPHATESTFUNCTIONREF 0xFE e volta.
    ClearRec();
    At<u32>(a + 0x48) = 0x7340B0;
    cb(atomic, nullptr);
    const Rec* set0 = FindRec("RwRenderStateSet", 0);
    const Rec* set1 = FindRec("RwRenderStateSet", 1);
    Check(CountRec("RwRenderStateGet") == 1 && set0 && set0->a[0] == 30 && set0->a[1] == 0xFE && set1 &&
              set1->a[0] == 30 && set1->a[1] == 0x55,
          "helice: ALPHATESTFUNCTIONREF = 0xFE durante o desenho e volta ao valor de antes");
}

void EnterExtras(uint8_t* shadow) {
    g_inEsi = (u32)(uintptr_t)shadow;
    g_inEdi = (u32)(uintptr_t)(shadow + 8);
    g_inEax = 0x1;
    g_inEdx = 0x22222222;
    g_inEbx = 0x33333333;
    HostEnterExtras();
}

// O clump do dono desenhado na camera da sombra (o gancho de 0x705C4A marca a camera aberta).
void ClumpRendered() {
    reinterpret_cast<void*(__cdecl*)(void*, void*, void*)>(Target(0x705C4A))(reinterpret_cast<void*>(0x8888),
                                                                            reinterpret_cast<void*>(0x705620), nullptr);
}

void TestStubs(void* ped) {
    printf("\n-- trechos em assembly\n");
    // Sol: z = max(z, 0.5 * noite + 0.6), eax e edx intactos, ecx = edi.
    float vec[3] = {0.3f, 0.2f, 0.9f};
    const float zs[3] = {0.9f, 0.1f, 0.9f};
    const float dn[3] = {0.0f, 0.0f, 1.0f};
    const float expect[3] = {0.9f, 0.6f, 1.1f};
    for (int i = 0; i < 3; i++) {
        vec[2] = zs[i];
        At<float>(0x8D12C0) = dn[i];
        g_inEax = (u32)(uintptr_t)vec;
        g_inEdx = 0x12345678;
        g_inEdi = 0xDEADBEEF;
        g_inEsi = 0x0F0F0F0F;
        g_inEbx = 0x77777777;
        HostEnterSun();
        Check(Near(g_outSt0, expect[i]) && g_outEcx == 0xDEADBEEF && g_outEax == (u32)(uintptr_t)vec &&
                  g_outEdx == 0x12345678 && g_outEsi == 0x0F0F0F0F && g_outEbx == 0x77777777,
              "sol: z %.1f, noite %.0f -> %.2f (eax, edx, ebx, esi intactos; ecx = edi)", zs[i], dn[i], g_outSt0);
    }
    At<float>(0x8D12C0) = 0.0f;

    // Extras: sem o clump desenhado (sombra de atomic, ou BeginUpdate recusado) -> nada; com ele ->
    // InvertRaster(&sombra->camera) e RwCameraEndUpdate(camera).
    void* camera = reinterpret_cast<void*>(0x4242);
    uint8_t* shadow = static_cast<uint8_t*>(NewShadow(ped, camera));
    g_globals.curCamera = camera; // RwCameraBeginUpdate deixa a camera da sombra como a atual
    EnterExtras(shadow);          // consome uma marca que tenha sobrado de outro teste
    for (int open = 0; open < 2; open++) {
        if (open) {
            ClumpRendered();
        }
        ClearRec();
        EnterExtras(shadow);
        const bool regs = g_outEax == 0x4242 && (g_outEcx & 0xFF) == 0xA5 && g_outEdx == 0x22222222 &&
                          g_outEbx == 0x33333333 && g_outEsi == (u32)(uintptr_t)shadow &&
                          g_outEdi == (u32)(uintptr_t)(shadow + 8) && g_outEbp == 0x0BADF00D;
        if (!open) {
            Check(regs && g_recCount == 0, "extras sem o clump desenhado: nada (registradores do jogo intactos)");
        } else {
            const Rec* inv = FindRec("InvertRaster");
            const Rec* end = FindRec("RwCameraEndUpdate");
            Check(regs && inv && inv->a[0] == (u32)(uintptr_t)(shadow + 8) && end && end->a[0] == 0x4242,
                  "extras com o clump desenhado: InvertRaster(&camera) e RwCameraEndUpdate(camera)");
        }
    }
    Check(!LogHas("nao e a da sombra"), "camera aberta e a da sombra: nada no log");
    g_globals.curCamera = nullptr;
    ClumpRendered();
    EnterExtras(shadow);
    Check(LogHas("nao e a da sombra"), "camera aberta diferente da sombra: avisa no log");
}

// Moto com piloto e garupa: os dois entram na sombra da moto, em silhueta, e nao pedem sombra propria.
void TestOccupants() {
    printf("\n-- quem esta no veiculo entra na sombra dele\n");
    uint8_t* bike = static_cast<uint8_t*>(NewEntity(2, true, 9));
    uint8_t* rider = static_cast<uint8_t*>(NewEntity(3, true));
    uint8_t* passenger = static_cast<uint8_t*>(NewEntity(3, true));
    for (uint8_t* p : {rider, passenger}) {
        At<uint8_t>((uintptr_t)p + 0x46D) = 1; // bInVehicle
        At<void*>((uintptr_t)p + 0x58C) = bike;
    }
    // Clumps diferentes para saber quem foi desenhado.
    At<void*>((uintptr_t)rider + 0x18) = reinterpret_cast<void*>(0xA001);
    At<void*>((uintptr_t)passenger + 0x18) = reinterpret_cast<void*>(0xA002);
    At<void*>((uintptr_t)bike + 0x460) = rider;
    At<void*>((uintptr_t)bike + 0x464 + 4 * 2) = passenger; // garupa no 3o lugar

    ClearRec();
    reinterpret_cast<void(__thiscall*)(void*)>(Target(0x5E6664))(rider);
    Check(CountRec("UpdateRpHAnim") == 1 && CountRec("DoShadowThisFrame") == 0,
          "piloto na moto: nao pede sombra propria");

    uint8_t* shadow = static_cast<uint8_t*>(NewShadow(bike, reinterpret_cast<void*>(0x4343)));
    EnterExtras(shadow);
    ClumpRendered();
    ClearRec();
    EnterExtras(shadow);
    const Rec* first = FindRec("RpClumpForAllAtomics", 0);
    const Rec* second = FindRec("RpClumpForAllAtomics", 1);
    Check(CountRec("RpClumpForAllAtomics") == 2 && first && first->a[0] == 0xA001 && second && second->a[0] == 0xA002,
          "a sombra da moto desenha o piloto e a garupa");
    Check(FindRec("InvertRaster") && FindRec("RwCameraEndUpdate"), "e depois inverte e fecha a camera");

    // O callback da silhueta: tira textura, luz e cor (0xEC) so durante o desenho.
    if (first) {
        Fake* atomic = new Fake();
        Fake* geometry = new Fake();
        memset(atomic, 0, sizeof(*atomic));
        memset(geometry, 0, sizeof(*geometry));
        const uintptr_t a = reinterpret_cast<uintptr_t>(atomic);
        At<uint8_t>(a + 2) = 4;
        At<void*>(a + 0x18) = geometry;
        At<u32>((uintptr_t)geometry + 8) = 0xFF;
        ClearRec();
        reinterpret_cast<void*(__cdecl*)(void*, void*)>(first->a[1])(atomic, nullptr);
        const Rec* qr = FindRec("atomicQuickRender");
        Check(qr && qr->a[2] == 0x13 && At<u32>((uintptr_t)geometry + 8) == 0xFF,
              "silhueta: flags 0xFF viram 0x13 no desenho e voltam (%02X)", qr ? qr->a[2] : 0);
    }

    // Sem sombra em tempo real de veiculo, o piloto volta a pedir a sua.
    Sleep(1100);
    WriteIni("[REALTIME_SHADOWS]\r\nVehicleRealTimeShadows=0\r\nCombineRealTimeShadowsWithStencil=1\r\n"
             "[GERAL]\r\nrecarregar=1\r\n");
    FireEvent(0x53E981);
    ClearRec();
    reinterpret_cast<void(__thiscall*)(void*)>(Target(0x5E6664))(rider);
    reinterpret_cast<void(__cdecl*)(void*, u32)>(Target(0x6ABCF5))(bike, 0);
    Check(CountRec("DoShadowThisFrame") == 1 && FindRec("DoShadowThisFrame")->a[1] == (u32)(uintptr_t)rider,
          "VehicleRealTimeShadows=0: o piloto pede a sombra dele e a moto nao");
    Sleep(1100);
    WriteIni("[REALTIME_SHADOWS]\r\nCombineRealTimeShadowsWithStencil=1\r\nMaxDistance=100\r\n"
             "[GERAL]\r\nrecarregar=1\r\n");
    FireEvent(0x53E981);
}

// MaxRealTimeShadows: das 20 entidades que pedem sombra, so as 12 mais perto ganham (raio do quadro anterior). Alem
// de MaxDistance (100) ninguem pede: o jogo nao desenharia a sombra.
void TestLimit() {
    printf("\n-- limite de sombras em tempo real\n");
    void* cars[21];
    for (int i = 0; i < 21; i++) {
        cars[i] = NewEntity(2, true, 0);
        // 10 m, 14 m, ... 86 m da camera (na origem), o 21o a 150 m. A altura (60 m) nao conta: o jogo mede no plano.
        At<float>((uintptr_t)cars[i] + 0x04) = i < 20 ? 10.0f + 4.0f * i : 150.0f;
        At<float>((uintptr_t)cars[i] + 0x08) = 0.0f;
        At<float>((uintptr_t)cars[i] + 0x0C) = 60.0f;
    }
    auto requestAll = [&] {
        for (void* c : cars) {
            reinterpret_cast<void(__cdecl*)(void*, u32)>(Target(0x6ABCF5))(c, 0);
        }
    };
    FireEvent(0x53E981); // zera o quadro
    ClearRec();
    requestAll();
    bool skipFar = true;
    for (int i = 0; i < CountRec("DoShadowThisFrame"); i++) {
        skipFar = skipFar && FindRec("DoShadowThisFrame", i)->a[1] != (u32)(uintptr_t)cars[20];
    }
    Check(CountRec("DoShadowThisFrame") == 20 && skipFar, "primeiro quadro: sem raio ainda, os 20 pedem (o de 150 m nao)");
    FireEvent(0x53E981);
    ClearRec();
    requestAll();
    bool nearest = CountRec("DoShadowThisFrame") == 12;
    for (int i = 0; nearest && i < 12; i++) {
        nearest = FindRec("DoShadowThisFrame", i)->a[1] == (u32)(uintptr_t)cars[i];
    }
    Check(nearest, "depois: so as 12 mais perto (%d)", CountRec("DoShadowThisFrame"));
    Check(LogHas("diagnostico: 20 entidades pediram sombra"), "log: mais pedidos que vagas (distancia no plano)");

    // Quem ja tem sombra ganha uma folga no raio: o 13o (58 m) com sombra continua.
    At<void*>((uintptr_t)cars[12] + 0x134) = reinterpret_cast<void*>(0x5151);
    FireEvent(0x53E981);
    ClearRec();
    requestAll();
    Check(CountRec("DoShadowThisFrame") == 13 && FindRec("DoShadowThisFrame", 12)->a[1] == (u32)(uintptr_t)cars[12],
          "o 13o, que ja tem sombra, nao perde a vaga na borda");

    // O jogador e o veiculo dele sempre ganham (dentro de MaxDistance).
    void* player = NewEntity(3, true);
    At<void*>(0xB7CD98) = player;
    At<uint8_t>((uintptr_t)player + 0x46D) = 1;
    At<void*>((uintptr_t)player + 0x58C) = cars[19];
    FireEvent(0x53E981);
    ClearRec();
    requestAll();
    Check(FindRec("DoShadowThisFrame", CountRec("DoShadowThisFrame") - 1)->a[1] == (u32)(uintptr_t)cars[19],
          "o veiculo do jogador (86 m) ganha mesmo fora das 12 mais perto");
    At<void*>((uintptr_t)player + 0x58C) = cars[20];
    ClearRec();
    requestAll();
    Check(CountRec("DoShadowThisFrame") == 13, "mas a 150 m (alem de MaxDistance) nao pede");
    At<void*>(0xB7CD98) = nullptr;
    At<void*>((uintptr_t)cars[12] + 0x134) = nullptr;
    FireEvent(0x53E981);
    FireEvent(0x53E981);
}

// O SA-MP desligando a atualizacao das sombras com o jogo aberto: o mod religa no quadro seguinte. Um gancho de outro
// mod no mesmo lugar fica.
void TestUpdateRestore() {
    printf("\n-- atualizacao das sombras em tempo real desligada de novo\n");
    memset(Mem(0x53EA08), 0x90, 10);
    Mem(0x706AB0)[0] = 0xC3;
    FireEvent(0x53E981);
    Check(Bytes(0x53EA08, {0xB9, 0x50, 0x03, 0xC4, 0x00, 0xE8, 0x9E, 0x80, 0x1C, 0x00}) &&
              Bytes(0x706AB0, {0x51, 0x53, 0x57, 0x8B, 0xF9}),
          "religada no quadro seguinte");
    Check(LogHas("religado de novo"), "log: religado de novo");
    PutCall(0x53EA0D, reinterpret_cast<uintptr_t>(&FakeGameProcess)); // call para fora do exe
    PutJump(0x706AB0, reinterpret_cast<void*>(&FakeGameProcess));       // jmp no comeco da funcao
    uint8_t idle[10], start[5];
    memcpy(idle, Mem(0x53EA08), 10);
    memcpy(start, Mem(0x706AB0), 5);
    FireEvent(0x53E981);
    Check(!memcmp(idle, Mem(0x53EA08), 10) && !memcmp(start, Mem(0x706AB0), 5),
          "gancho de outro mod na chamada e no comeco da funcao: fica");
    Put(0x53EA08, {0xB9, 0x50, 0x03, 0xC4, 0x00, 0xE8, 0x9E, 0x80, 0x1C, 0x00});
    Put(0x706AB0, {0x51, 0x53, 0x57, 0x8B, 0xF9});
}

void TestDoubleUpdate(void* ped) {
    printf("\n-- diagnostico do Update duas vezes no quadro\n");
    void* shadow = NewShadow(ped, nullptr);
    At<void*>(0xC40350 + 4) = shadow; // m_apShadows[0]
    At<u32>(0xB7CB4C) = 500;
    reinterpret_cast<void*(__thiscall*)(void*)>(Target(0x706B29))(shadow);
    At<u32>(0xB7CB4C) = 501;
    reinterpret_cast<void*(__thiscall*)(void*)>(Target(0x706B29))(shadow);
    Check(!LogHas("2 vezes no mesmo quadro"), "um Update por quadro: nada no log");
    reinterpret_cast<void*(__thiscall*)(void*)>(Target(0x706B29))(shadow);
    Check(LogHas("2 vezes no mesmo quadro"), "dois no mesmo quadro: avisa no log");
    At<void*>(0xC40350 + 4) = nullptr;
}

int RunFull() {
    if (!CreateRealDevice()) {
        printf("aviso: sem device D3D9 no Wine, shaders nao testados\n");
    }
    SetupGame();
    HMODULE mod = LoadLibraryA("Trok Shadows.asi");
    Check(mod != nullptr, "LoadLibrary do .asi");
    if (!mod) {
        return 1;
    }
    PatchImport(mod, "USER32.dll", "MessageBoxW", reinterpret_cast<void*>(&FakeMessageBoxW));
    Check(LogHas("1.0 US confirmado"), "log: versao confirmada");
    int events = 0;
    for (uintptr_t s : {0x5BD779u, 0x53BC21u, 0x748CFBu, 0x53E981u}) events += !InExe(Target(s));
    Check(events == 4, "4 eventos ligados");
    Check(Mem(0x706676)[0] == 0x8B, "nada alem dos eventos antes do RenderWare iniciar");

    ClearRec();
    FireEvent(0x5BD779);
    Check(FindRec("InitRw original") && g_rec[0].name == FindRec("InitRw original")->name,
          "evento RenderWare: a original roda primeiro");
    Check(LogHas("INI criado com os valores do shadows.ini antigo"), "INI criado a partir do shadows.ini");
    TestPatches();

    ClearRec();
    FireEvent(0x748CFB);
    Check(FindRec("InitGame original") != nullptr, "evento jogo iniciado: a original roda");
    Check(Bytes(0x53EA08, {0xB9, 0x50, 0x03, 0xC4, 0x00, 0xE8, 0x9E, 0x80, 0x1C, 0x00}) &&
              Bytes(0x706AB0, {0x51, 0x53, 0x57, 0x8B, 0xF9}),
          "atualizacao das sombras em tempo real religada (o que o SA-MP desliga)");

    void* ped = NewEntity(3, true);
    void* heli = NewEntity(2, true, 3);
    void* noModel = NewEntity(3, false);
    TestStencil();
    TestRealtime(ped, heli, noModel);
    TestShader(ped, heli, true);
    TestAtomics(ped, heli);
    TestStubs(ped);
    TestOccupants();
    TestLimit();
    TestDoubleUpdate(ped);
    TestUpdateRestore();

    // INI mudado com o jogo aberto (guarda antes o INI que o mod criou, para conferir o formato).
    printf("\n-- INI recarregado com o jogo aberto\n");
    CopyFileA("Trok Shadows.ini", "Trok Shadows (criado pelo mod).ini", FALSE);
    Sleep(1200);
    WriteIni("[STENCIL_SHADOWS]\r\nMaxShadows=64\r\nMaxDistance=77\r\nFlagIgnoreSomeShadows=0\r\n"
             "DisableBuildingShadows=0\r\nDisplayShadowsAtLowSettings=0\r\n"
             "[STENCIL_SHADOWS_COLOR]\r\nR=5\r\nG=12\r\nB=20\r\nA=80\r\n"
             "[REALTIME_SHADOWS_COLOR]\r\nR=5\r\nG=12\r\nB=20\r\nA=80\r\n"
             "[REALTIME_SHADOWS]\r\nCombineRealTimeShadowsWithStencil=0\r\nMaxDistance=40\r\nRasterSize=10\r\n"
             "BlurRasterSize=9\r\nRasterSize2=9\r\nBlurRasterSize2=9\r\nGradientMin=128\r\nShadowBoundSphere=10\r\n"
             "ShadowBoundSphereInAir=20\r\nShadowZDistanceLimit=8\r\nShadowZDistanceLimitInAir=10\r\n"
             "DisplayShadowsAtLowSettings=0\r\nMoreThanOnePlayer=0\r\nDrawVehicleDefaultShadowWithRealTime=0\r\n"
             "[GERAL]\r\nrecarregar=1\r\n");
    ClearRec();
    FireEvent(0x53E981);
    Check(FindRec("GameProcess original") != nullptr, "evento de quadro: a original roda");
    Check(LogHas("INI recarregado"), "log: INI recarregado");
    Check(Near(*reinterpret_cast<float*>(At<u32>(kDistanceSites[0])), 77.0f) &&
              Near(*reinterpret_cast<float*>(At<u32>(kDistanceSqSites[0])), 5929.0f),
          "MaxDistance stencil 77 na hora");
    Check(Near(At<float>(0x8D5240), 40.0f) && Near(At<float>(0xC4B6B0), 1600.0f), "MaxDistance tempo real 40 na hora");
    Check(Bytes(0x711E3D, {0x75, 0x10}) && Mem(0x711E41)[0] == 0xE8 && Target(0x711E41) == 0x710310,
          "FlagIgnoreSomeShadows e DisableBuildingShadows desligados (bytes do jogo de volta)");
    Check(Bytes(0x711D9D, {0x75, 0x10}) && Bytes(0x7113C0, {0x75, 0x10}) && Mem(0x706BCC)[0] == 0x75 &&
              Mem(0x5E6766)[0] == 0x75 && Mem(0x7069F5)[0] == 0x75 && Bytes(0x70BDAB, {0x0F, 0x85}),
          "graficos baixos, MoreThanOnePlayer e sombra simples junto desligados");
    Check(Mem(0x7064C2)[0] == 10, "Raster continua 10 (so muda ao reiniciar)");
    void* ped2 = NewEntity(3, true);
    void* heli2 = NewEntity(2, true, 3);
    TestShader(ped2, heli2, false);

    ClearRec();
    FireEvent(0x53BC21);
    Check(FindRec("ShutdownRw original") != nullptr, "evento RenderWare desligado: a original roda");
    Check(g_messageBoxes == 0, "nenhuma caixa de mensagem");
    return 0;
}

int RunWrong(uintptr_t gameBegin) {
    // Memoria do "jogo" zerada (outro exe): o mod nao pode escrever nada.
    HMODULE mod = LoadLibraryA("Trok Shadows.asi");
    Check(mod != nullptr, "LoadLibrary do .asi");
    Check(LogHas("nao e o 1.0 US"), "log: versao recusada");
    bool zero = true;
    for (uintptr_t a = (gameBegin + 3) & ~3u; zero && a < 0xD00000; a += 4) zero = At<u32>(a) == 0;
    Check(zero, "nenhum byte do exe alterado");
    return 0;
}

// INI da versao 1.0 (sem as chaves novas): o mod reescreve com os mesmos valores e as chaves novas.
int RunUpgrade() {
    SetupGame();
    WriteIni("[STENCIL_SHADOWS]\r\nMaxShadows=200\r\nMaxDistance=77.0\r\n[REALTIME_SHADOWS]\r\nMaxDistance=60.0\r\n"
             "MoreThanOnePlayer=auto\r\n[GERAL]\r\nrecarregar=1\r\n");
    HMODULE mod = LoadLibraryA("Trok Shadows.asi");
    Check(mod != nullptr, "LoadLibrary do .asi");
    if (!mod) {
        return 1;
    }
    PatchImport(mod, "USER32.dll", "MessageBoxW", reinterpret_cast<void*>(&FakeMessageBoxW));
    FireEvent(0x5BD779);
    const char* ini = "Trok Shadows.ini";
    Check(LogHas("INI atualizado com as opcoes novas"), "log: INI atualizado");
    Check(FileHas(ini, "EnableRealTimeShadows=1") && FileHas(ini, "VehicleRealTimeShadows=1") &&
              FileHas(ini, "WeaponsInShadow=1") && FileHas(ini, "MaxRealTimeShadows=12") &&
              FileHas(ini, "; Quantas sombras em tempo real"),
          "chaves novas no INI, com comentario");
    Check(FileHas(ini, "MaxShadows=200") && FileHas(ini, "MaxDistance=77.0") && FileHas(ini, "MaxDistance=60.0") &&
              FileHas(ini, "MoreThanOnePlayer=auto"),
          "os valores de antes ficaram");
    Check(Near(At<float>(0x8D5240), 60.0f), "MaxDistance tempo real 60 aplicado");
    return 0;
}

int RunConflict() {
    HMODULE old = LoadLibraryA("shadows.asi");
    Check(old != nullptr, "shadows.asi falso carregado antes");
    SetupGame();
    HMODULE mod = LoadLibraryA("Trok Shadows.asi");
    Check(mod != nullptr, "LoadLibrary do .asi");
    if (!mod) {
        return 1;
    }
    PatchImport(mod, "USER32.dll", "MessageBoxW", reinterpret_cast<void*>(&FakeMessageBoxW));
    FireEvent(0x5BD779);
    Check(LogHas("conflito"), "log: conflito com o shadows.asi");
    Check(g_messageBoxes == 1, "uma caixa de mensagem avisando");
    Check(Mem(0x706676)[0] == 0x8B && Mem(0x53BCAB)[0] == 0xE8 && Target(0x53BCAB) == 0x70F9E0,
          "nenhum recurso aplicado");
    return 0;
}

// Chamado pelo launcher.exe, que ja ocupa 0x400000-0xD00000 (gravavel e executavel) como o gta_sa.exe.
extern "C" __declspec(dllexport) int HostMain(const char* mode, uintptr_t gameBegin) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (!strcmp(mode, "errado")) {
        RunWrong(gameBegin);
    } else if (!strcmp(mode, "conflito")) {
        RunConflict();
    } else if (!strcmp(mode, "atualizar")) {
        RunUpgrade();
    } else {
        RunFull();
    }
    printf("\n%s: %d ok, %d falhas\n", mode, g_pass, g_fail);
    return g_fail ? 1 : 0;
}

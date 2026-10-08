// Smoke test do .asi fora do jogo (Wine 32 bits + Xvfb): um "GTA de mentira" que
//  - deixa os enderecos fixos do gta_sa.exe 1.0 US dentro da propria imagem (array gigante no .bss);
//  - carrega o samp.dll falso (enderecos do 0.3.7 R1, gerado por make_fake_samp.py) e o .asi, antes de
//    existir device, como o ASI Loader;
//  - cria o device d3d9 e o expoe em 0xC97C28 como o RenderWare. No cenario "samp-mods" o ponteiro vira
//    um proxy estilo SA-MP (marcador verde) e dois mods de vtable com a regra do Trok Dialogs e do Trok
//    Radar (marcadores vermelho e azul) encadeiam no Present da vtable do device real;
//  - digita /trokui (chama o callback que o .asi registrou), salva o back buffer e confere os pixels:
//    o menu tem que aparecer por cima de tudo e sumir no segundo /trokui.
//
// Uso: fake_gta.exe <asi> <direto|samp-mods|wrapper> [so-janela-do-d3d|so-classe]
//   wrapper           como samp-mods, mas o proxy esconde o device real (GetBackBuffer falha), como um
//                     wrapper de d3d9 faria: o .asi tem que achar o Present com o device descartavel
//   so-janela-do-d3d  deixa 0xC8CF88 (PsGlobal.window) zerado: so o hDeviceWindow do RenderWare
//   so-classe         zera os dois: o .asi tem que achar a janela pela classe

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>

namespace {

// A imagem vai de 0x400000 ate ~0xD5x000 e cobre 0xB7CB4C..0xC9C05C.
unsigned char g_gtaMemory[0x960000];

template <typename T> T& Mem(DWORD address) {
    return *reinterpret_cast<T*>(address);
}

typedef HRESULT(__stdcall* PresentFn)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef void(__cdecl* CmdProc)(const char*);

constexpr int W = 1600;
constexpr int H = 900;
constexpr D3DCOLOR CLEAR = D3DCOLOR_XRGB(20, 90, 40);

int g_failures = 0;
IDirect3DDevice9* g_real = nullptr;
HMODULE g_d3d9 = nullptr;
DWORD g_shot[W * H];

void Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "ok" : "FALHA", what);
    fflush(stdout);
    if (!ok) {
        ++g_failures;
    }
}

void Marker(IDirect3DDevice9* device, LONG x, D3DCOLOR color) {
    D3DRECT r = {x, 10, x + 20, 30};
    device->Clear(1, &r, D3DCLEAR_TARGET, color, 1.0f, 0);
}

// ---------------------------------------------------------------- proxy estilo SA-MP

void* g_proxyVtable[119];
void* g_proxyObject[1] = {g_proxyVtable};

HRESULT __stdcall ProxyPresent(IDirect3DDevice9* real, const RECT* a, const RECT* b, HWND c, const RGNDATA* d) {
    Marker(real, 70, D3DCOLOR_XRGB(40, 230, 90)); // o SA-MP desenha chat e afins aqui
    return real->Present(a, b, c, d);              // e chama o Present do real pela vtable dele
}

HRESULT __stdcall HiddenGetBackBuffer(IDirect3DDevice9*, UINT, UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9**) {
    return D3DERR_INVALIDCALL;
}

// Cada metodo do proxy e um thunk que troca o "this" pelo device real e salta pela vtable ao vivo dele.
IDirect3DDevice9* MakeProxy(IDirect3DDevice9* real, bool hideReal) {
    BYTE* code = static_cast<BYTE*>(VirtualAlloc(nullptr, 119 * 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    for (int i = 0; i < 119; ++i) {
        BYTE* p = code + i * 32;
        g_proxyVtable[i] = p;
        *p++ = 0xB8; // mov eax, real
        *reinterpret_cast<DWORD*>(p) = reinterpret_cast<DWORD>(real);
        p += 4;
        *p++ = 0x89; // mov [esp+4], eax
        *p++ = 0x44;
        *p++ = 0x24;
        *p++ = 0x04;
        if (i == 17 || (i == 18 && hideReal)) {
            void* target = i == 17 ? reinterpret_cast<void*>(ProxyPresent) : reinterpret_cast<void*>(HiddenGetBackBuffer);
            *p++ = 0xE9; // jmp target
            *reinterpret_cast<DWORD*>(p) = reinterpret_cast<DWORD>(target) - reinterpret_cast<DWORD>(p + 4);
        } else {
            *p++ = 0x8B; // mov eax, [eax]
            *p++ = 0x00;
            *p++ = 0xFF; // jmp [eax + i*4]
            *p++ = 0xA0;
            *reinterpret_cast<DWORD*>(p) = i * 4;
        }
    }
    return reinterpret_cast<IDirect3DDevice9*>(g_proxyObject);
}

// ---------------------------------------------------------------- mods de vtable estilo Trok Dialogs/Radar
// Regra dos dois (lida no binario do Trok Dialogs 2.28): encadeiam no slot 17 da vtable do device real
// guardando o valor antigo como "original" (e como "puro" se ele for do d3d9.dll); se o gancho reentra
// (laco), consertam a corrente apontando direto para o puro; e so reencadeiam quando perderam o slot E
// ficaram 120 ms sem ser chamados com o jogo andando (3 quadros no CTimer::m_FrameCounter, 0xB7CB4C;
// nos primeiros 30 quadros basta o silencio).

struct VtableMod {
    const char* name;
    LONG markerX;
    D3DCOLOR color;
    void* hook;
    PresentFn original;
    PresentFn pure;
    volatile LONG inside;
    DWORD lastCall;
    DWORD lastFrame;
    int chains;
    int loops;
    int calls;
};

template <int N> HRESULT __stdcall ModPresent(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);

VtableMod g_mods[2] = {
    {"dialogs", 10, D3DCOLOR_XRGB(230, 40, 40), reinterpret_cast<void*>(ModPresent<0>), nullptr, nullptr, 0, 0, 0, 0, 0, 0},
    {"radar", 40, D3DCOLOR_XRGB(40, 90, 240), reinterpret_cast<void*>(ModPresent<1>), nullptr, nullptr, 0, 0, 0, 0, 0, 0},
};

bool InD3D9(const void* address) {
    HMODULE module = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<LPCSTR>(address), &module);
    return module && module == g_d3d9;
}

template <int N>
HRESULT __stdcall ModPresent(IDirect3DDevice9* device, const RECT* a, const RECT* b, HWND c, const RGNDATA* d) {
    VtableMod& m = g_mods[N];
    if (InterlockedCompareExchange(&m.inside, 1, 0) != 0) {
        if (!m.pure) {
            return D3D_OK;
        }
        if (m.original != m.pure) {
            m.original = m.pure;
            ++m.loops;
        }
        return m.pure(device, a, b, c, d);
    }
    ++m.calls;
    m.lastCall = GetTickCount();
    m.lastFrame = Mem<DWORD>(0xB7CB4C);
    HRESULT scene = device->BeginScene();
    Marker(device, m.markerX, m.color);
    if (SUCCEEDED(scene)) {
        device->EndScene();
    }
    HRESULT hr = m.original(device, a, b, c, d);
    InterlockedExchange(&m.inside, 0);
    return hr;
}

void Chain(VtableMod& m) {
    void** vtable = *reinterpret_cast<void***>(g_real);
    DWORD old;
    VirtualProtect(&vtable[17], sizeof(void*), PAGE_READWRITE, &old);
    m.original = reinterpret_cast<PresentFn>(vtable[17]);
    vtable[17] = m.hook;
    VirtualProtect(&vtable[17], sizeof(void*), old, &old);
    if (InD3D9(reinterpret_cast<void*>(m.original))) {
        m.pure = m.original;
    }
    ++m.chains;
    m.lastCall = GetTickCount();
}

void Watch(VtableMod& m) {
    void** vtable = *reinterpret_cast<void***>(g_real);
    bool skipped = m.calls < 30 || Mem<DWORD>(0xB7CB4C) - m.lastFrame >= 3;
    if (vtable[17] != m.hook && GetTickCount() - m.lastCall >= 120 && skipped) {
        Chain(m);
    }
}

// ---------------------------------------------------------------- captura

bool Capture(const char* path) {
    IDirect3DSurface9* back = nullptr;
    IDirect3DSurface9* sys = nullptr;
    bool ok = false;
    if (SUCCEEDED(g_real->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) &&
        SUCCEEDED(g_real->CreateOffscreenPlainSurface(W, H, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, nullptr)) &&
        SUCCEEDED(g_real->GetRenderTargetData(back, sys))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
            for (int y = 0; y < H; ++y) {
                memcpy(&g_shot[y * W], static_cast<BYTE*>(lr.pBits) + y * lr.Pitch, W * 4);
            }
            sys->UnlockRect();
            ok = true;
        }
    }
    if (sys) {
        sys->Release();
    }
    if (back) {
        back->Release();
    }
    if (!ok) {
        return false;
    }
    FILE* f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    BITMAPFILEHEADER fh = {};
    BITMAPINFOHEADER ih = {};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + W * H * 3;
    ih.biSize = sizeof(ih);
    ih.biWidth = W;
    ih.biHeight = H;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    static BYTE row[W * 3];
    for (int y = H - 1; y >= 0; --y) {
        for (int x = 0; x < W; ++x) {
            DWORD p = g_shot[y * W + x];
            row[x * 3] = p & 0xFF;
            row[x * 3 + 1] = (p >> 8) & 0xFF;
            row[x * 3 + 2] = (p >> 16) & 0xFF;
        }
        fwrite(row, sizeof(row), 1, f);
    }
    fclose(f);
    return true;
}

void Rgb(int x, int y, int* r, int* g, int* b) {
    DWORD p = g_shot[y * W + x];
    *r = (p >> 16) & 0xFF;
    *g = (p >> 8) & 0xFF;
    *b = p & 0xFF;
}

// Fracao dos pixels de um retangulo que nao sao a cor de fundo do "jogo".
double Covered(int x0, int y0, int x1, int y1) {
    int covered = 0;
    int total = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            int r, g, b;
            Rgb(x, y, &r, &g, &b);
            covered += !(r == 20 && g == 90 && b == 40);
            ++total;
        }
    }
    return total ? double(covered) / total : 0.0;
}

bool Dominant(int x, int y, int channel) {
    int c[3];
    Rgb(x, y, &c[0], &c[1], &c[2]);
    return c[channel] > 150 && c[channel] > c[(channel + 1) % 3] + 60 && c[channel] > c[(channel + 2) % 3] + 60;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("uso: fake_gta.exe <asi> <direto|samp-mods|wrapper> [so-janela-do-d3d|so-classe]\n");
        return 2;
    }
    bool hideReal = strcmp(argv[2], "wrapper") == 0;
    bool sampMods = hideReal || strcmp(argv[2], "samp-mods") == 0;
    const char* window = argc > 3 ? argv[3] : "";
    DWORD lo = reinterpret_cast<DWORD>(g_gtaMemory);
    DWORD hi = lo + sizeof(g_gtaMemory);
    if (lo > 0xB7CB4C || hi < 0xC9C060) {
        printf("FALHA: memoria falsa do GTA fora do lugar (0x%08lX..0x%08lX)\n", lo, hi);
        return 2;
    }

    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "Grand theft auto San Andreas";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "GTA: San Andreas", WS_POPUP | WS_VISIBLE, 0, 0, W, H, nullptr,
                                nullptr, wc.hInstance, nullptr);
    SetForegroundWindow(hwnd);
    Mem<int>(0xC17044) = W;
    Mem<int>(0xC17048) = H;
    if (strcmp(window, "so-classe") != 0) {
        Mem<HWND>(0xC9C05C) = hwnd;
    }
    if (!window[0]) {
        Mem<HWND>(0xC8CF88) = hwnd;
    }

    HMODULE samp = LoadLibraryA("samp.dll");
    Check(samp != nullptr, "samp.dll falso carregado");
    HMODULE asi = LoadLibraryA(argv[1]);
    Check(asi != nullptr, "asi carregado");
    if (!samp || !asi) {
        return 1;
    }
    DWORD sampBase = reinterpret_cast<DWORD>(samp);
    auto field = [&](DWORD rva) -> DWORD& { return Mem<DWORD>(sampBase + 0x300000 + rva); };
    Sleep(300);

    g_d3d9 = GetModuleHandleA("d3d9.dll");
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp = {};
    pp.BackBufferWidth = W;
    pp.BackBufferHeight = H;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.hDeviceWindow = hwnd;
    pp.Windowed = TRUE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    HRESULT hr = d3d ? d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                         &pp, &g_real)
                     : E_FAIL;
    Check(SUCCEEDED(hr) && g_real, "device d3d9 criado");
    if (!g_real) {
        return 1;
    }
    IDirect3DDevice9* exposed = sampMods ? MakeProxy(g_real, hideReal) : g_real;
    Mem<IDirect3DDevice9*>(0xC97C28) = exposed;

    DWORD start = GetTickCount();
    DWORD lastWatch = 0;
    int step = 0;
    int chatBefore = 0;
    for (;;) {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        DWORD t = GetTickCount() - start;

        if (sampMods && t >= 1500 && !g_mods[0].chains) {
            Chain(g_mods[0]);
        }
        if (sampMods && t >= 1700 && !g_mods[1].chains) {
            Chain(g_mods[1]);
        }
        if (sampMods && g_mods[1].chains && t - lastWatch >= 100) {
            lastWatch = t;
            Watch(g_mods[0]);
            Watch(g_mods[1]);
        }

        if (step == 0 && t >= 4500) {
            const char* name = reinterpret_cast<const char*>(field(0));
            Check(name && strcmp(name, "trokui") == 0, "/trokui registrado no CInput do samp.dll");
            chatBefore = field(8);
            if (field(4)) {
                reinterpret_cast<CmdProc>(field(4))("");
            }
            ++step;
        }

        ++Mem<DWORD>(0xB7CB4C); // CTimer::m_FrameCounter
        exposed->Clear(0, nullptr, D3DCLEAR_TARGET, CLEAR, 1.0f, 0);
        if (SUCCEEDED(exposed->BeginScene())) {
            exposed->EndScene();
        }
        exposed->Present(nullptr, nullptr, nullptr, nullptr);

        if (step == 1 && t >= 5500) {
            Check(Capture("menu_aberto.bmp"), "back buffer salvo (menu_aberto.bmp)");
            double menu = Covered(W / 2 - 150, H / 2 - 100, W / 2 + 150, H / 2 + 100);
            char text[160];
            snprintf(text, sizeof(text), "menu aberto no centro da tela (%.0f%% coberto)", menu * 100);
            Check(menu > 0.95, text);
            Check(int(field(0xC)) == 2, "cursor do SA-MP no modo 2 com o menu aberto");
            Check(int(field(8)) == chatBefore, "nenhum aviso de 'desenho nao ativo' no chat");
            if (sampMods) {
                Check(Dominant(20, 20, 0) && Dominant(50, 20, 2) && Dominant(80, 20, 1),
                      "marcadores do proxy e dos mods de vtable continuam na tela");
            }
            reinterpret_cast<CmdProc>(field(4))("");
            ++step;
        }
        if (step == 2 && t >= 6500) {
            Check(Capture("menu_fechado.bmp"), "back buffer salvo (menu_fechado.bmp)");
            double menu = Covered(W / 2 - 150, H / 2 - 100, W / 2 + 150, H / 2 + 100);
            Check(menu < 0.01, "menu fechado no segundo /trokui");
            Check(int(field(0xC)) == 0, "cursor do SA-MP devolvido (modo 0)");
            ++step;
        }
        if (step == 3) {
            break;
        }
        if (t > 20000) {
            Check(false, "tempo esgotado");
            break;
        }
        Sleep(5);
    }
    if (sampMods) {
        for (VtableMod& m : g_mods) {
            printf("info: mod %s: %d encadeamentos, %d lacos consertados, %d quadros\n", m.name, m.chains, m.loops,
                   m.calls);
        }
    }
    printf(g_failures ? "FALHOU (%d)\n" : "PASSOU\n", g_failures);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), g_failures ? 1 : 0); // sem descarregar o .asi (o jogo tambem nao)
    return 0;
}

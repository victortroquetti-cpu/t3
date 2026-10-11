// Smoke test do Trok Chat fora do jogo (Wine 32 bits + Xvfb): um "GTA de mentira" que
//  - deixa os enderecos fixos do gta_sa.exe 1.0 US dentro da propria imagem (array gigante no .bss);
//  - carrega o samp.dll falso (enderecos do 0.3.7 R1, gerado por make_fake_samp.py) e o .asi, antes de existir
//    device, como o ASI Loader;
//  - cria o device d3d9 e o expoe em 0xC97C28 atras de um proxy estilo SA-MP;
//  - a cada quadro chama o CChat::Draw e o CChat::Render do samp.dll (como o SA-MP faz) e apresenta;
//  - o WndProc da janela faz o papel do SA-MP (no fim da corrente, como o samp.dll): abre a caixa dele (CInput::Open)
//    com o caractere "t" e com o F6, menos com dialogo; e um "mod" com campo de texto fica na frente dele;
//  - confere: o chat do SA-MP some e o do Trok Chat aparece (com as mensagens que ja estavam no chat); o AddEntry
//    desviado chama o original e devolve a pilha certa; o T e o F6 do jogador abrem a barra do Trok Chat no lugar da
//    caixa do SA-MP; com a barra aberta o jogo nao ve as teclas; Enter manda o texto em cp1252 pelo CInput::Send; Esc
//    fecha; o "t" digitado no campo do mod fica com ele; script abrindo a caixa abre a do SA-MP; script fechando fecha
//    a barra; /trokchat troca; sem o Present do Trok Chat o chat do SA-MP volta sozinho.
//
// Uso: fake_gta.exe <asi> <r1|outra-versao|sem-samp>
//   outra-versao  samp.dll com outro ponto de entrada: o .asi nao pode mexer em nada
//   sem-samp      sem samp.dll (single player): idem

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

// A imagem vai de 0x400000 ate ~0xD5x000 e cobre 0xB73418..0xC9C05C.
unsigned char g_gtaMemory[0x960000];

template <typename T> T& Mem(DWORD address) {
    return *reinterpret_cast<T*>(address);
}

typedef void(__thiscall* DrawFn)(void*);
typedef void(__thiscall* InputFn)(void*);
typedef void(__cdecl* CmdProc)(const char*);

constexpr int W = 1600;
constexpr int H = 900;
constexpr D3DCOLOR CLEAR = D3DCOLOR_XRGB(20, 90, 40);

// Campos do samp.dll falso (RVA, ver make_fake_samp.py).
constexpr DWORD CMD_NAME = 0x300000, CMD_PROC = 0x300004, ADD_COUNT = 0x300008, DRAW_COUNT = 0x30000C;
constexpr DWORD RENDER_COUNT = 0x300010, SEND_COUNT = 0x300014, LAST_TYPE = 0x300018, OPEN_COUNT = 0x30001C;
constexpr DWORD CLOSE_COUNT = 0x300020, SEND_BUF = 0x300100;
constexpr DWORD CHAT = 0x301000, CINPUT = 0x308000, CDIALOG = 0x30A000;
constexpr DWORD ADD_ENTRY = 0x64010, DRAW = 0x64230, RENDER = 0x63D70, INPUT_OPEN = 0x657E0, INPUT_CLOSE = 0x658E0;

int g_failures = 0;
IDirect3DDevice9* g_real = nullptr;
DWORD g_shot[W * H];

// O que chegou ao SA-MP e ao jogo (o WndProc da janela, no fim da corrente).
int g_downs[256], g_ups[256], g_chars = 0;
DWORD g_samp = 0;

// O "mod" com campo de texto, na frente do SA-MP (subclasse instalada antes do .asi): digitando nele, fica com os
// caracteres.
WNDPROC g_modNext = nullptr;
bool g_modTyping = false;
int g_modChars = 0;

DWORD& SampField(DWORD rva) {
    return Mem<DWORD>(g_samp + rva);
}

void Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "ok" : "FALHA", what);
    fflush(stdout);
    if (!ok) {
        ++g_failures;
    }
}

LRESULT CALLBACK GameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam < 256) {
        ++g_downs[wParam];
    }
    if (msg == WM_KEYUP && wParam < 256) {
        ++g_ups[wParam];
    }
    if (msg == WM_CHAR) {
        ++g_chars;
    }
    // O SA-MP: com a caixa fechada e sem dialogo, o caractere "t" e o F6 abrem a caixa dele.
    if (g_samp && !SampField(CINPUT + 0x14E0) && !SampField(CDIALOG + 0x28) &&
        ((msg == WM_CHAR && (wParam == 't' || wParam == 'T')) || (msg == WM_KEYDOWN && wParam == VK_F6))) {
        reinterpret_cast<InputFn>(g_samp + INPUT_OPEN)(reinterpret_cast<void*>(g_samp + CINPUT));
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK ModWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_modTyping && msg == WM_CHAR) {
        ++g_modChars;
        return 0;
    }
    return CallWindowProcA(g_modNext, hwnd, msg, wParam, lParam);
}

void Down(HWND hwnd, WPARAM vk) {
    SendMessageA(hwnd, WM_KEYDOWN, vk, 1);
}

void Up(HWND hwnd, WPARAM vk) {
    SendMessageA(hwnd, WM_KEYUP, vk, 0xC0000001);
}

void Chars(HWND hwnd, const char* text) {
    for (const char* p = text; *p; ++p) {
        SendMessageA(hwnd, WM_CHAR, static_cast<unsigned char>(*p), 1);
    }
}

// O T de verdade: a tecla, o caractere que o TranslateMessage gera e a tecla solta.
void TapT(HWND hwnd) {
    Down(hwnd, 'T');
    Chars(hwnd, "t");
    Up(hwnd, 'T');
}

// ---------------------------------------------------------------- proxy estilo SA-MP

void* g_proxyVtable[119];
void* g_proxyObject[1] = {g_proxyVtable};

HRESULT __stdcall ProxyPresent(IDirect3DDevice9* real, const RECT* a, const RECT* b, HWND c, const RGNDATA* d) {
    return real->Present(a, b, c, d); // o SA-MP chama o Present do real pela vtable dele
}

// Cada metodo do proxy e um thunk que troca o "this" pelo device real e salta pela vtable ao vivo dele.
IDirect3DDevice9* MakeProxy(IDirect3DDevice9* real) {
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
        if (i == 17) {
            *p++ = 0xE9; // jmp ProxyPresent
            *reinterpret_cast<DWORD*>(p) = reinterpret_cast<DWORD>(ProxyPresent) - reinterpret_cast<DWORD>(p + 4);
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

// ---------------------------------------------------------------- AddEntry com conferencia da pilha

struct AddEntryCall {
    void* fn;
    void* chat;
    DWORD type;
    const char* text;
    const char* prefix;
    DWORD color;
    DWORD prefixColor;
};

// Chama o CChat::AddEntry (thiscall, 5 argumentos) e devolve quanto o ESP andou: 0 = quem atendeu (o desvio do .asi)
// limpou a pilha como o original.
int CallAddEntry(AddEntryCall* c) {
    int delta;
    asm volatile("mov edi, esp\n\t"
                 "push dword ptr [esi+24]\n\t"
                 "push dword ptr [esi+20]\n\t"
                 "push dword ptr [esi+16]\n\t"
                 "push dword ptr [esi+12]\n\t"
                 "push dword ptr [esi+8]\n\t"
                 "mov ecx, dword ptr [esi+4]\n\t"
                 "call dword ptr [esi]\n\t"
                 "mov eax, esp\n\t"
                 "sub eax, edi\n\t"
                 "mov esp, edi\n\t"
                 : "=a"(delta)
                 : "S"(c)
                 : "ecx", "edx", "edi", "memory", "cc");
    return delta;
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
    // So o canto de cima, onde o chat fica.
    const int w = 800, h = 300;
    BITMAPFILEHEADER fh = {};
    BITMAPINFOHEADER ih = {};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + w * h * 3;
    ih.biSize = sizeof(ih);
    ih.biWidth = w;
    ih.biHeight = h;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    static BYTE row[w * 3];
    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
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

// Fracao dos pixels de um retangulo que nao sao a cor de fundo do "jogo".
double Covered(int x0, int y0, int x1, int y1) {
    int covered = 0;
    int total = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            DWORD p = g_shot[y * W + x] & 0xFFFFFF;
            covered += p != (CLEAR & 0xFFFFFF);
            ++total;
        }
    }
    return total ? double(covered) / total : 0.0;
}

bool LogHas(const char* text) {
    FILE* f = fopen("Trok Chat.log", "rb");
    if (!f) {
        return false;
    }
    std::string log;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        log.append(buf, n);
    }
    fclose(f);
    return log.find(text) != std::string::npos;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("uso: fake_gta.exe <asi> <r1|outra-versao|sem-samp>\n");
        return 2;
    }
    const char* mode = argv[2];
    const bool r1 = strcmp(mode, "r1") == 0;
    const bool withSamp = strcmp(mode, "sem-samp") != 0;
    DWORD lo = reinterpret_cast<DWORD>(g_gtaMemory);
    DWORD hi = lo + sizeof(g_gtaMemory);
    if (lo > 0xB73418 || hi < 0xC9C060) {
        printf("FALHA: memoria falsa do GTA fora do lugar (0x%08lX..0x%08lX)\n", lo, hi);
        return 2;
    }

    WNDCLASSA wc = {};
    wc.lpfnWndProc = GameWndProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "Grand theft auto San Andreas";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "GTA: San Andreas", WS_POPUP | WS_VISIBLE, 0, 0, W, H, nullptr,
                                nullptr, wc.hInstance, nullptr);
    SetForegroundWindow(hwnd);
    Mem<int>(0xC17044) = W;
    Mem<int>(0xC17048) = H;
    Mem<HWND>(0xC9C05C) = hwnd;
    Mem<HWND>(0xC8CF88) = hwnd;
    g_modNext = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ModWndProc)));

    HMODULE samp = withSamp ? LoadLibraryA("samp.dll") : nullptr;
    if (withSamp) {
        Check(samp != nullptr, "samp.dll falso carregado");
    }
    HMODULE asi = LoadLibraryA(argv[1]);
    Check(asi != nullptr, "asi carregado");
    if ((withSamp && !samp) || !asi) {
        return 1;
    }
    const DWORD base = reinterpret_cast<DWORD>(samp);
    g_samp = base;
    auto field = [&](DWORD rva) -> DWORD& { return Mem<DWORD>(base + rva); };
    void* chat = reinterpret_cast<void*>(base + CHAT);
    void* input = reinterpret_cast<void*>(base + CINPUT);
    DrawFn draw = reinterpret_cast<DrawFn>(base + DRAW);
    DrawFn render = reinterpret_cast<DrawFn>(base + RENDER);
    InputFn scriptOpen = reinterpret_cast<InputFn>(base + INPUT_OPEN);   // sampSetChatInputEnabled(true)
    InputFn scriptClose = reinterpret_cast<InputFn>(base + INPUT_CLOSE); // sampSetChatInputEnabled(false)
    auto inputOpen = [&]() -> DWORD& { return field(CINPUT + 0x14E0); };
    auto dialogOpen = [&]() -> DWORD& { return field(CDIALOG + 0x28); };
    Sleep(300);

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
    IDirect3DDevice9* exposed = MakeProxy(g_real);
    Mem<IDirect3DDevice9*>(0xC97C28) = exposed;

    // Regioes da tela (1600x900: u = 0.708): o chat vai de (16, 10) a (570, 207) aberto.
    const int cx0 = 30, cy0 = 20, cx1 = 560, cy1 = 200;
    DWORD start = GetTickCount();
    int step = 0;
    DWORD draws = 0, renders = 0, adds = 0;
    char text[160];
    for (;;) {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        DWORD t = GetTickCount() - start;

        ++Mem<DWORD>(0xB7CB4C); // CTimer::m_FrameCounter
        if (samp) {
            draw(chat); // o SA-MP desenha o chat dele a cada quadro
            render(chat);
        }
        exposed->Clear(0, nullptr, D3DCLEAR_TARGET, CLEAR, 1.0f, 0);
        if (SUCCEEDED(exposed->BeginScene())) {
            exposed->EndScene();
        }
        exposed->Present(nullptr, nullptr, nullptr, nullptr);

        if (!r1) {
            // Sem o SA-MP 0.3.7 R1: o .asi nao pode encostar em nada.
            if (step == 0 && t >= 4000) {
                if (samp) {
                    draws = field(DRAW_COUNT);
                }
                TapT(hwnd);
                Check(g_downs['T'] == 1 && g_chars == 1 && g_ups['T'] == 1, "o T chega ao SA-MP");
                if (samp) {
                    Check(field(OPEN_COUNT) == 1 && inputOpen() == 1, "e abre a caixa do SA-MP");
                }
                ++step;
            }
            if (step == 1 && t >= 4500) {
                if (samp) {
                    Check(field(DRAW_COUNT) > draws, "o chat do SA-MP continua sendo desenhado");
                    Check(field(CMD_NAME) == 0, "nenhum comando registrado");
                    Check(LogHas("nao e o 0.3.7 R1"), "log: samp.dll de outra versao");
                }
                Check(Capture("tela.bmp"), "back buffer salvo (tela.bmp)");
                Check(Covered(cx0, cy0, cx1, cy1) == 0.0, "nada desenhado no canto do chat");
                break;
            }
            continue;
        }

        if (step == 0 && t >= 4000) {
            const char* name = reinterpret_cast<const char*>(field(CMD_NAME));
            Check(name && strcmp(name, "trokchat") == 0, "/trokchat registrado no CInput do samp.dll");
            draws = field(DRAW_COUNT);
            renders = field(RENDER_COUNT);
            ++step;
        }
        if (step == 1 && t >= 4500) {
            Check(field(DRAW_COUNT) == draws && field(RENDER_COUNT) == renders,
                  "chat do SA-MP escondido (CChat::Draw e CChat::Render pulados)");
            AddEntryCall call = {reinterpret_cast<void*>(base + ADD_ENTRY), chat, 8,
                                 "{FF6347}[ADMIN]{FFFFFF} mensagem nova pelo AddEntry", nullptr, 0xFFFFFFFF, 0};
            adds = field(ADD_COUNT);
            const int delta = CallAddEntry(&call);
            snprintf(text, sizeof(text), "AddEntry desviado devolve a pilha certa (ESP andou %d)", delta);
            Check(delta == 0, text);
            Check(field(ADD_COUNT) == adds + 1 && field(LAST_TYPE) == 8, "e o AddEntry do SA-MP continua sendo chamado");
            ++step;
        }
        if (step == 2 && t >= 4800) {
            Check(Capture("chat_fechado.bmp"), "back buffer salvo (chat_fechado.bmp)");
            const double closed = Covered(cx0, cy0, cx1, cy1);
            snprintf(text, sizeof(text), "mensagens do Trok Chat no canto (%.1f%% dos pixels)", closed * 100);
            Check(closed > 0.01 && closed < 0.5, text);
            Check(Covered(800, 300, 1500, 800) == 0.0, "nada fora do canto do chat");
            Check(LogHas("3 mensagens que ja estavam no chat"), "as 3 mensagens que ja estavam no chat vieram junto");

            TapT(hwnd);
            Check(inputOpen() == 1 && field(OPEN_COUNT) == 0,
                  "T: o SA-MP pede a caixa dele e a barra do Trok Chat abre no lugar (\"chat aberto\" ligado)");
            Check(g_downs['T'] == 1 && g_chars == 1 && g_ups['T'] == 1, "o T fez o caminho de sempre ate o SA-MP");
            Down(hwnd, 'W');
            Check(g_downs['W'] == 0, "tecla apertada com a barra aberta fica com ela (o boneco nao anda)");
            Up(hwnd, 'W');
            Check(g_ups['W'] == 1, "tecla solta sempre passa (nada fica preso no jogo)");
            Chars(hwnd, "Ol\xE1 [teste]");
            Check(g_chars == 1, "os caracteres digitados nao chegam ao jogo");
            Down(hwnd, VK_F8);
            Check(g_downs[VK_F8] == 1, "teclas F continuam com o SA-MP (F8 tira foto)");
            ++step;
        }
        if (step == 3 && t >= 5100) {
            Check(Capture("chat_aberto.bmp"), "back buffer salvo (chat_aberto.bmp)");
            const double open = Covered(cx0, cy0, cx1, cy1);
            snprintf(text, sizeof(text), "barra aberta: painel do chat na tela (%.0f%% coberto)", open * 100);
            Check(open > 0.95, text);
            Down(hwnd, VK_RETURN);
            Up(hwnd, VK_RETURN);
            ++step;
        }
        if (step == 4 && t >= 5400) {
            const char* sent = reinterpret_cast<const char*>(base + SEND_BUF);
            snprintf(text, sizeof(text), "Enter manda o texto em cp1252 pelo CInput::Send (\"%s\", %lu vez)", sent,
                     field(SEND_COUNT));
            Check(field(SEND_COUNT) == 1 && strcmp(sent, "Ol\xE1 [teste]") == 0, text);
            Check(inputOpen() == 0, "e fecha a barra (\"chat aberto\" desligado)");
            Check(g_downs[VK_RETURN] == 0 && g_ups[VK_RETURN] == 1, "o Enter nao chega ao jogo (so a tecla solta)");
            Down(hwnd, VK_F6);
            Check(inputOpen() == 1 && field(OPEN_COUNT) == 0, "F6 tambem abre a barra");
            Up(hwnd, VK_F6);
            Down(hwnd, VK_ESCAPE);
            Up(hwnd, VK_ESCAPE);
            ++step;
        }
        if (step == 5 && t >= 5700) {
            Check(inputOpen() == 0 && field(SEND_COUNT) == 1 && g_downs[VK_ESCAPE] == 0,
                  "Esc fecha sem mandar e o jogo nao ve o Esc (o menu de pausa nao abre)");
            dialogOpen() = 1;
            TapT(hwnd);
            Check(inputOpen() == 0 && field(OPEN_COUNT) == 0, "com dialogo do servidor o T nao abre nada (como no SA-MP)");
            dialogOpen() = 0;
            g_modTyping = true;
            const int sampChars = g_chars;
            TapT(hwnd);
            Check(inputOpen() == 0 && g_modChars == 1 && g_chars == sampChars,
                  "o 't' digitado no campo de texto de outro mod fica com ele (o chat nao abre)");
            g_modTyping = false;

            scriptOpen(input);
            Check(field(OPEN_COUNT) == 1 && inputOpen() == 1, "script abrindo o chat: abre a caixa do SA-MP");
            const int before = g_chars;
            Chars(hwnd, "/r ");
            Check(g_chars == before + 3, "e o que se digita vai para ela");
            scriptClose(input);
            Check(inputOpen() == 0 && field(CLOSE_COUNT) == 1, "script fecha a caixa do SA-MP");
            TapT(hwnd);
            Check(inputOpen() == 1 && field(OPEN_COUNT) == 1, "T de novo: a barra do Trok Chat");
            scriptClose(input);
            const int closedChars = g_chars;
            Chars(hwnd, "x");
            Check(inputOpen() == 0 && g_chars == closedChars + 1, "script fechando o chat fecha a barra");

            TapT(hwnd);
            Mem<BYTE>(0xBA67A4) = 1;
            ++step;
        }
        if (step == 6 && t >= 5900) {
            Check(inputOpen() == 0, "menu de pausa fecha a barra");
            Mem<BYTE>(0xBA67A4) = 0;
            adds = field(ADD_COUNT);
            reinterpret_cast<CmdProc>(field(CMD_PROC))("");
            Check(field(ADD_COUNT) == adds + 1, "/trokchat avisa no chat");
            draws = field(DRAW_COUNT);
            ++step;
        }
        if (step == 7 && t >= 6200) {
            Check(field(DRAW_COUNT) > draws, "/trokchat: o chat do SA-MP volta");
            Check(Capture("trokchat_desligado.bmp"), "back buffer salvo (trokchat_desligado.bmp)");
            Check(Covered(cx0, cy0, cx1, cy1) == 0.0, "e o Trok Chat sai da tela");
            TapT(hwnd);
            Check(inputOpen() == 1 && field(OPEN_COUNT) == 2, "e o T volta a abrir a caixa do SA-MP");
            scriptClose(input);
            reinterpret_cast<CmdProc>(field(CMD_PROC))("");
            ++step;
        }
        if (step == 8 && t >= 6500) {
            draws = field(DRAW_COUNT);
            ++step;
        }
        if (step == 9 && t >= 6900) {
            Check(field(DRAW_COUNT) == draws, "/trokchat de novo: o Trok Chat volta para o lugar");
            // O Present para de passar pelo Trok Chat (outro mod cortou a corrente, por exemplo).
            draws = field(DRAW_COUNT);
            for (int i = 0; i < 40; ++i) {
                draw(chat);
            }
            snprintf(text, sizeof(text), "sem o Present do Trok Chat, o chat do SA-MP volta sozinho (%lu de 40)",
                     field(DRAW_COUNT) - draws);
            Check(field(DRAW_COUNT) - draws == 10, text);
            ++step;
        }
        if (step == 10 && t >= 7200) {
            draws = field(DRAW_COUNT);
            ++step;
        }
        if (step == 11 && t >= 7600) {
            Check(field(DRAW_COUNT) == draws, "com o Present de volta, o chat do SA-MP some de novo");
            Check(LogHas("o desenho do Trok Chat parou") && LogHas("o desenho do Trok Chat voltou"),
                  "log: o desenho parou e voltou");
            break;
        }
    }
    printf("== %d falha(s)\n", g_failures);
    fflush(stdout);
    ExitProcess(g_failures ? 1 : 0);
}

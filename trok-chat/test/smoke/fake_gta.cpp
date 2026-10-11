// Smoke test do Trok Chat fora do jogo (Wine 32 bits + Xvfb): um "GTA de mentira" que
//  - deixa os enderecos fixos do gta_sa.exe 1.0 US dentro da propria imagem (array gigante no .bss);
//  - carrega o samp.dll falso (enderecos do 0.3.7 R1, gerado por make_fake_samp.py) e o .asi, antes de existir
//    device, como o ASI Loader;
//  - cria o device d3d9 e o expoe em 0xC97C28 atras de um proxy estilo SA-MP;
//  - a cada quadro chama o CChat::Draw e o CChat::Render do samp.dll (como o SA-MP faz) e apresenta;
//  - o WndProc da janela faz o papel do SA-MP e do GTA, no fim da corrente: chama o KeyPressHandler e o
//    CharInputHandler do samp.dll (desviados pelo .asi), e o "SA-MP" abre a caixa dele (CInput::Open) com o caractere
//    "t" e com o F6, menos com dialogo ou menu de pausa; o que passa dele e o "jogo". Um "mod" com campo de texto fica
//    na frente.
//
// Uso: fake_gta.exe <asi> <r1|teclas|desligado|outra-versao|sem-samp>
//   r1            teclas padrao (T e F6): o chat do SA-MP some e o do Trok Chat aparece; as teclas abrem a barra no
//                 lugar da caixa do SA-MP; Enter manda; Esc fecha; dialogo, mod com campo de texto, scripts (com texto
//                 pronto), menu de pausa, o menu do /trokchat; o Trok Chat.ini guarda o /pagesize; sem o Present do
//                 Trok Chat o chat do SA-MP volta sozinho
//   teclas        Trok Chat.ini com Y e Enter: Y abre, Enter abre e manda, Enter segurado nao reabre nem chega ao
//                 jogo, T e F6 nao abrem nada; linhas, hora e F7 do INI voltam
//   desligado     Trok Chat.ini com Ligado=0: o chat e a caixa do SA-MP; o /trokchat abre o menu
//   outra-versao  samp.dll com outro ponto de entrada: o .asi nao pode mexer em nada
//   sem-samp      sem samp.dll (single player): idem

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

namespace {

// A imagem vai de 0x400000 ate ~0xD5x000 e cobre 0xB73418..0xC9C05C.
unsigned char g_gtaMemory[0x960000];

template <typename T> T& Mem(DWORD address) {
    return *reinterpret_cast<T*>(address);
}

typedef void(__thiscall* DrawFn)(void*);
typedef void(__thiscall* InputFn)(void*);
typedef BOOL(__cdecl* KeyFn)(unsigned int);
typedef void(__cdecl* CmdProc)(const char*);

constexpr int W = 1600;
constexpr int H = 900;
constexpr D3DCOLOR CLEAR = D3DCOLOR_XRGB(20, 90, 40);
constexpr DWORD GTA_MENU_ACTIVE = 0xBA67A4;

// Campos do samp.dll falso (RVA, ver make_fake_samp.py).
constexpr DWORD CMD_NAME = 0x300000, CMD_PROC = 0x300004, ADD_COUNT = 0x300008, DRAW_COUNT = 0x30000C;
constexpr DWORD RENDER_COUNT = 0x300010, SEND_COUNT = 0x300014, LAST_TYPE = 0x300018, OPEN_COUNT = 0x30001C;
constexpr DWORD CLOSE_COUNT = 0x300020, CURSOR_MODE = 0x300024, KEY_PRESS_SLOT = 0x300040, CHAR_INPUT_SLOT = 0x300044;
constexpr DWORD SEND_BUF = 0x300100;
constexpr DWORD CHAT = 0x301000, CINPUT = 0x308000, CDIALOG = 0x30A000, EDIT_TEXT = 0x30C100;
constexpr DWORD ADD_ENTRY = 0x64010, DRAW = 0x64230, RENDER = 0x63D70, INPUT_OPEN = 0x657E0, INPUT_CLOSE = 0x658E0;
constexpr DWORD KEY_PRESS = 0x5D850, CHAR_INPUT = 0x5DA80;

int g_failures = 0;
IDirect3DDevice9* g_real = nullptr;
IDirect3DDevice9* g_exposed = nullptr;
HWND g_hwnd = nullptr;
DWORD g_samp = 0;
DWORD g_shot[W * H];
char g_text[256];

// O que passou do SA-MP e chegou ao jogo.
int g_downs[256], g_ups[256], g_chars = 0;

// O "mod" com campo de texto, na frente do SA-MP (subclasse instalada antes do .asi): digitando nele, fica com os
// caracteres.
WNDPROC g_modNext = nullptr;
bool g_modTyping = false;
int g_modChars = 0;

DWORD& F(DWORD rva) {
    return Mem<DWORD>(g_samp + rva);
}

DWORD& InputOpen() {
    return F(CINPUT + 0x14E0);
}

DWORD& DialogOpen() {
    return F(CDIALOG + 0x28);
}

void* InputObject() {
    return reinterpret_cast<void*>(g_samp + CINPUT);
}

void Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "ok" : "FALHA", what);
    fflush(stdout);
    if (!ok) {
        ++g_failures;
    }
}

// ---------------------------------------------------------------- o SA-MP e o jogo, no fim da corrente

// O que o samp.dll faz com as teclas (o original dos desvios do .asi): abre a caixa com o F6 e o "t".
bool SampMayOpen() {
    return !InputOpen() && !DialogOpen() && !Mem<BYTE>(GTA_MENU_ACTIVE);
}

void OpenSampInput() {
    reinterpret_cast<InputFn>(g_samp + INPUT_OPEN)(InputObject()); // pelo endereco desviado, como o SA-MP
}

BOOL __cdecl NativeKeyPress(unsigned int vk) {
    if (vk == VK_F6 && SampMayOpen()) {
        OpenSampInput();
        return TRUE;
    }
    return FALSE;
}

BOOL __cdecl NativeCharInput(unsigned int ch) {
    if ((ch == 't' || ch == 'T') && SampMayOpen()) {
        OpenSampInput();
        return TRUE;
    }
    return FALSE;
}

LRESULT CALLBACK GameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_samp) {
        if (msg == WM_KEYDOWN && reinterpret_cast<KeyFn>(g_samp + KEY_PRESS)(static_cast<unsigned>(wParam))) {
            return 0;
        }
        if (msg == WM_CHAR && reinterpret_cast<KeyFn>(g_samp + CHAR_INPUT)(static_cast<unsigned>(wParam))) {
            return 0;
        }
    }
    if (msg == WM_KEYDOWN && wParam < 256) {
        ++g_downs[wParam];
    }
    if (msg == WM_KEYUP && wParam < 256) {
        ++g_ups[wParam];
    }
    if (msg == WM_CHAR) {
        ++g_chars;
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

void Down(WPARAM vk, bool repeat = false) {
    SendMessageA(g_hwnd, WM_KEYDOWN, vk, repeat ? 0x40000001 : 1);
}

void Up(WPARAM vk) {
    SendMessageA(g_hwnd, WM_KEYUP, vk, 0xC0000001);
}

void Char(unsigned char c) {
    SendMessageA(g_hwnd, WM_CHAR, c, 1);
}

void Chars(const char* text) {
    for (const char* p = text; *p; ++p) {
        Char(static_cast<unsigned char>(*p));
    }
}

// Uma tecla de verdade: a tecla, o caractere que o TranslateMessage gera (se gera) e a tecla solta.
void Press(WPARAM vk, unsigned char c) {
    Down(vk);
    if (c) {
        Char(c);
    }
    Up(vk);
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

// ---------------------------------------------------------------- quadros

void Frame() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    ++Mem<DWORD>(0xB7CB4C); // CTimer::m_FrameCounter
    if (g_samp) {
        reinterpret_cast<DrawFn>(g_samp + DRAW)(reinterpret_cast<void*>(g_samp + CHAT)); // o SA-MP desenha o chat
        reinterpret_cast<DrawFn>(g_samp + RENDER)(reinterpret_cast<void*>(g_samp + CHAT));
    }
    g_exposed->Clear(0, nullptr, D3DCLEAR_TARGET, CLEAR, 1.0f, 0);
    if (SUCCEEDED(g_exposed->BeginScene())) {
        g_exposed->EndScene();
    }
    g_exposed->Present(nullptr, nullptr, nullptr, nullptr);
    Sleep(8);
}

void Frames(int n) {
    for (int i = 0; i < n; ++i) {
        Frame();
    }
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

int IniInt(const char* key) {
    char path[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, path);
    lstrcatA(path, "\\Trok Chat.ini");
    return static_cast<int>(GetPrivateProfileIntA("TROK_CHAT", key, -1, path));
}

const char* Sent() {
    return reinterpret_cast<const char*>(g_samp + SEND_BUF);
}

// O chat aberto ocupa (16, 10)-(570, 207) em 1600x900 (u = 0.708).
constexpr int CX0 = 30, CY0 = 20, CX1 = 560, CY1 = 200;

// ---------------------------------------------------------------- cenarios

void RunR1() {
    const char* name = reinterpret_cast<const char*>(F(CMD_NAME));
    Check(name && strcmp(name, "trokchat") == 0, "/trokchat registrado no CInput do samp.dll");
    DWORD draws = F(DRAW_COUNT), renders = F(RENDER_COUNT);
    Frames(10);
    Check(F(DRAW_COUNT) == draws && F(RENDER_COUNT) == renders,
          "chat do SA-MP escondido (CChat::Draw e CChat::Render pulados)");
    AddEntryCall call = {reinterpret_cast<void*>(g_samp + ADD_ENTRY), reinterpret_cast<void*>(g_samp + CHAT), 8,
                         "{FF6347}[ADMIN]{FFFFFF} mensagem nova pelo AddEntry", nullptr, 0xFFFFFFFF, 0};
    const DWORD adds = F(ADD_COUNT);
    const int delta = CallAddEntry(&call);
    snprintf(g_text, sizeof(g_text), "AddEntry desviado devolve a pilha certa (ESP andou %d)", delta);
    Check(delta == 0, g_text);
    Check(F(ADD_COUNT) == adds + 1 && F(LAST_TYPE) == 8, "e o AddEntry do SA-MP continua sendo chamado");
    Frames(5);
    Check(Capture("chat_fechado.bmp"), "back buffer salvo (chat_fechado.bmp)");
    const double closed = Covered(CX0, CY0, CX1, CY1);
    snprintf(g_text, sizeof(g_text), "mensagens do Trok Chat no canto (%.1f%% dos pixels)", closed * 100);
    Check(closed > 0.01 && closed < 0.5, g_text);
    Check(Covered(800, 300, 1500, 800) == 0.0, "nada fora do canto do chat");
    Check(LogHas("3 mensagens que ja estavam no chat"), "as 3 mensagens que ja estavam no chat vieram junto");
    Check(LogHas("caixa de texto do SA-MP reconhecida (tipo 7)"), "caixa de texto do SA-MP reconhecida (DXUT)");

    // T: o SA-MP abriria a caixa dele; abre a barra do Trok Chat. Segurado, a repeticao nao digita na barra.
    Down('T');
    Char('t');
    Check(InputOpen() == 1 && F(OPEN_COUNT) == 0, "T abre a barra do Trok Chat, nao a caixa do SA-MP");
    Down('T', true);
    Char('t');
    Down('T', true);
    Char('t');
    Up('T');
    Check(g_downs['T'] == 0 && g_chars == 0 && g_ups['T'] == 1, "nem a tecla nem o 't' chegam ao jogo (so a solta)");
    Frames(3);
    Down('W');
    Check(g_downs['W'] == 0, "tecla apertada com a barra aberta fica com ela (o boneco nao anda)");
    Up('W');
    Check(g_ups['W'] == 1, "tecla solta sempre passa (nada fica preso no jogo)");
    Chars("Ol\xE1 [teste]");
    Check(g_chars == 0, "os caracteres digitados nao chegam ao jogo");
    Down(VK_F8);
    Check(g_downs[VK_F8] == 1, "teclas F continuam com o SA-MP (F8 tira foto)");
    Up(VK_F8);
    Frames(8);
    Check(Capture("chat_aberto.bmp"), "back buffer salvo (chat_aberto.bmp)");
    const double open = Covered(CX0, CY0, CX1, CY1);
    snprintf(g_text, sizeof(g_text), "barra aberta: painel do chat na tela (%.0f%% coberto)", open * 100);
    Check(open > 0.95, g_text);
    Press(VK_RETURN, '\r');
    Frames(5);
    snprintf(g_text, sizeof(g_text), "Enter manda o texto em cp1252 pelo CInput::Send (\"%s\"); o T segurado nao digitou",
             Sent());
    Check(F(SEND_COUNT) == 1 && strcmp(Sent(), "Ol\xE1 [teste]") == 0, g_text);
    Check(InputOpen() == 0, "e fecha a barra (\"chat aberto\" desligado)");
    Check(g_downs[VK_RETURN] == 0 && g_ups[VK_RETURN] == 1, "o Enter nao chega ao jogo (so a tecla solta)");

    Down(VK_F6);
    Check(InputOpen() == 1 && F(OPEN_COUNT) == 0 && g_downs[VK_F6] == 0, "F6 tambem abre a barra (o jogo nao ve o F6)");
    Up(VK_F6);
    Frames(3);
    Press(VK_ESCAPE, 0);
    Frames(5);
    Check(InputOpen() == 0 && F(SEND_COUNT) == 1 && g_downs[VK_ESCAPE] == 0,
          "Esc fecha sem mandar e o jogo nao ve o Esc (o menu de pausa nao abre)");

    DialogOpen() = 1;
    Press('T', 't');
    Frames(2);
    Check(InputOpen() == 0 && F(OPEN_COUNT) == 0 && g_downs['T'] == 1,
          "com dialogo do servidor o T segue o caminho do SA-MP e nao abre nada");
    DialogOpen() = 0;
    g_modTyping = true;
    const int chars = g_chars;
    Press('T', 't');
    Check(InputOpen() == 0 && g_modChars == 1 && g_chars == chars,
          "o 't' digitado no campo de texto de outro mod fica com ele (o chat nao abre)");
    g_modTyping = false;
    Frames(2);

    // Script abrindo o chat com texto pronto (sampSetChatInputText + sampSetChatInputEnabled).
    wchar_t* native = reinterpret_cast<wchar_t*>(g_samp + EDIT_TEXT);
    wcscpy(native, L"/r ");
    reinterpret_cast<InputFn>(g_samp + INPUT_OPEN)(InputObject());
    Check(InputOpen() == 1 && F(OPEN_COUNT) == 0, "script abrindo o chat: abre a barra do Trok Chat (so o nosso aparece)");
    Frames(5);
    Check(native[0] == 0, "o texto do script saiu da caixa do SA-MP");
    Chars("na escuta");
    Frames(3);
    Press(VK_RETURN, '\r');
    Frames(5);
    snprintf(g_text, sizeof(g_text), "o texto pronto do script ficou na barra (\"%s\")", Sent());
    Check(F(SEND_COUNT) == 2 && strcmp(Sent(), "/r na escuta") == 0, g_text);
    reinterpret_cast<InputFn>(g_samp + INPUT_OPEN)(InputObject());
    Frames(5);
    Chars("ok");
    Frames(3);
    Press(VK_RETURN, '\r');
    Frames(5);
    Check(F(SEND_COUNT) == 3 && strcmp(Sent(), "ok") == 0, "outro script sem texto: a barra abre vazia");

    Press('T', 't');
    Frames(3);
    reinterpret_cast<InputFn>(g_samp + INPUT_CLOSE)(InputObject());
    Check(InputOpen() == 0 && F(CLOSE_COUNT) == 1, "script fechando o chat fecha a barra");
    const int before = g_chars;
    Chars("x");
    Check(g_chars == before + 1, "e o que se digita volta para o jogo");

    Press('T', 't');
    Frames(2);
    Mem<BYTE>(GTA_MENU_ACTIVE) = 1;
    Frames(3);
    Check(InputOpen() == 0, "menu de pausa fecha a barra");
    Press('T', 't');
    Frames(2);
    Check(InputOpen() == 0, "e com ele aberto o T nao abre");
    Mem<BYTE>(GTA_MENU_ACTIVE) = 0;
    Frames(2);

    // /pagesize do SA-MP: fica guardado no Trok Chat.ini (um segundo depois).
    F(CHAT + 0x0) = 12;
    Frames(160);
    Check(IniInt("Linhas") == 12, "o /pagesize do SA-MP fica guardado no Trok Chat.ini");

    // Menu do /trokchat: cursor do SA-MP, o T nao abre o chat, Esc fecha.
    reinterpret_cast<CmdProc>(F(CMD_PROC))("");
    Frames(15);
    Check(F(CURSOR_MODE) == 2, "/trokchat abre o menu com o cursor do SA-MP");
    Check(Capture("menu.bmp"), "back buffer salvo (menu.bmp)");
    Check(Covered(W / 2 - 150, H / 2 - 100, W / 2 + 150, H / 2 + 100) > 0.95, "menu no centro da tela");
    const int downsT = g_downs['T'];
    Press('T', 't');
    Frames(2);
    Check(InputOpen() == 0 && g_downs['T'] == downsT, "com o menu aberto o T fica com ele (nem chat nem jogo)");
    Press(VK_ESCAPE, 0);
    Frames(5);
    Check(F(CURSOR_MODE) == 0, "Esc fecha o menu e devolve o mouse para a camera");

    // O Present para de passar pelo Trok Chat (outro mod cortou a corrente, por exemplo).
    draws = F(DRAW_COUNT);
    for (int i = 0; i < 40; ++i) {
        reinterpret_cast<DrawFn>(g_samp + DRAW)(reinterpret_cast<void*>(g_samp + CHAT));
    }
    snprintf(g_text, sizeof(g_text), "sem o Present do Trok Chat, o chat do SA-MP volta sozinho (%lu de 40)",
             F(DRAW_COUNT) - draws);
    Check(F(DRAW_COUNT) - draws == 10, g_text);
    Frames(3);
    draws = F(DRAW_COUNT);
    Frames(10);
    Check(F(DRAW_COUNT) == draws, "com o Present de volta, o chat do SA-MP some de novo");
    Check(LogHas("o desenho do Trok Chat parou") && LogHas("o desenho do Trok Chat voltou"), "log: parou e voltou");
}

void RunKeys() {
    Check(F(CHAT + 0x0) == 15 && Mem<BYTE>(g_samp + CHAT + 0xC) == 1 && F(CHAT + 0x8) == 1,
          "o Trok Chat.ini volta linhas (15), hora e o F7 (sem contorno)");
    Check(LogHas("opcoes: teclas Y e Enter"), "log: teclas Y e Enter");
    Press('Y', 'y');
    Check(InputOpen() == 1 && g_downs['Y'] == 0 && g_chars == 0, "Y (tecla principal) abre a barra; o 'y' nao entra");
    Frames(3);
    Chars("oi");
    Frames(2);
    Down(VK_RETURN);
    Char('\r');
    Frames(5);
    Check(F(SEND_COUNT) == 1 && strcmp(Sent(), "oi") == 0 && InputOpen() == 0,
          "Enter manda a mensagem, mesmo sendo a tecla alternativa");
    Down(VK_RETURN, true); // ainda segurando o Enter que mandou: as repeticoes
    Char('\r');
    Down(VK_RETURN, true);
    Char('\r');
    Frames(3);
    Check(InputOpen() == 0 && g_downs[VK_RETURN] == 0,
          "Enter segurado depois de mandar: a barra nao reabre e o jogo nao ve (nao entra no carro)");
    Up(VK_RETURN);
    Frames(2);
    Down(VK_RETURN);
    Char('\r');
    Frames(3);
    Check(InputOpen() == 1 && g_downs[VK_RETURN] == 0, "Enter de novo (outra apertada): abre a barra");
    Up(VK_RETURN);
    Frames(3);
    Press(VK_RETURN, '\r');
    Frames(5);
    Check(InputOpen() == 0 && F(SEND_COUNT) == 1, "o Enter que abriu nao entrou na barra: o seguinte so fecha");
    Press('T', 't');
    Frames(2);
    Check(InputOpen() == 0 && F(OPEN_COUNT) == 0 && g_downs['T'] == 1,
          "T (fora das teclas escolhidas) nao abre o chat e segue para o jogo");
    Down(VK_F6);
    Up(VK_F6);
    Frames(2);
    Check(InputOpen() == 0 && F(OPEN_COUNT) == 0, "F6 (fora das teclas escolhidas) nao abre nada");
}

void RunOff() {
    Check(LogHas("DESLIGADO (chat do SA-MP)"), "log: desligado no Trok Chat.ini");
    const DWORD draws = F(DRAW_COUNT);
    Frames(10);
    Check(F(DRAW_COUNT) > draws, "o chat do SA-MP continua sendo desenhado");
    Check(Capture("tela.bmp"), "back buffer salvo (tela.bmp)");
    Check(Covered(CX0, CY0, CX1, CY1) == 0.0, "nada do Trok Chat no canto do chat");
    Press('T', 't');
    Check(F(OPEN_COUNT) == 1 && InputOpen() == 1, "o T abre a caixa do SA-MP");
    reinterpret_cast<InputFn>(g_samp + INPUT_CLOSE)(InputObject());
    reinterpret_cast<CmdProc>(F(CMD_PROC))("");
    Frames(15);
    Check(F(CURSOR_MODE) == 2, "o /trokchat continua abrindo o menu (para ligar de novo)");
    Press(VK_ESCAPE, 0);
    Frames(5);
}

void RunWithoutR1(bool withSamp) {
    Frames(300);
    const DWORD draws = withSamp ? F(DRAW_COUNT) : 0;
    Press('T', 't');
    Check(g_downs['T'] == 1 && g_ups['T'] == 1, "o T chega ao SA-MP e ao jogo como sempre");
    if (withSamp) {
        Check(F(OPEN_COUNT) == 1 && InputOpen() == 1, "e abre a caixa do SA-MP");
        Frames(10);
        Check(F(DRAW_COUNT) > draws, "o chat do SA-MP continua sendo desenhado");
        Check(F(CMD_NAME) == 0, "nenhum comando registrado");
        Check(LogHas("nao e o 0.3.7 R1"), "log: samp.dll de outra versao");
    }
    Check(Capture("tela.bmp"), "back buffer salvo (tela.bmp)");
    Check(Covered(CX0, CY0, CX1, CY1) == 0.0, "nada desenhado no canto do chat");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("uso: fake_gta.exe <asi> <r1|teclas|desligado|outra-versao|sem-samp>\n");
        return 2;
    }
    const std::string mode = argv[2];
    const bool withSamp = mode != "sem-samp";
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
    g_hwnd = CreateWindowExA(0, wc.lpszClassName, "GTA: San Andreas", WS_POPUP | WS_VISIBLE, 0, 0, W, H, nullptr,
                             nullptr, wc.hInstance, nullptr);
    SetForegroundWindow(g_hwnd);
    Mem<int>(0xC17044) = W;
    Mem<int>(0xC17048) = H;
    Mem<HWND>(0xC9C05C) = g_hwnd;
    Mem<HWND>(0xC8CF88) = g_hwnd;
    g_modNext = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ModWndProc)));

    HMODULE samp = withSamp ? LoadLibraryA("samp.dll") : nullptr;
    if (withSamp) {
        Check(samp != nullptr, "samp.dll falso carregado");
        if (!samp) {
            return 1;
        }
        g_samp = reinterpret_cast<DWORD>(samp);
        F(KEY_PRESS_SLOT) = reinterpret_cast<DWORD>(NativeKeyPress);
        F(CHAR_INPUT_SLOT) = reinterpret_cast<DWORD>(NativeCharInput);
    }
    HMODULE asi = LoadLibraryA(argv[1]);
    Check(asi != nullptr, "asi carregado");
    if (!asi) {
        return 1;
    }
    Sleep(300);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp = {};
    pp.BackBufferWidth = W;
    pp.BackBufferHeight = H;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.hDeviceWindow = g_hwnd;
    pp.Windowed = TRUE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    HRESULT hr = d3d ? d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                         &pp, &g_real)
                     : E_FAIL;
    Check(SUCCEEDED(hr) && g_real, "device d3d9 criado");
    if (!g_real) {
        return 1;
    }
    g_exposed = MakeProxy(g_real);
    Mem<IDirect3DDevice9*>(0xC97C28) = g_exposed;

    if (mode == "outra-versao" || mode == "sem-samp") {
        RunWithoutR1(withSamp);
    } else {
        // Espera o .asi ligar (o boot dele da 1,5 s para o SA-MP montar o proxy) e o ImGui subir.
        DWORD start = GetTickCount();
        while (!F(CMD_NAME) && GetTickCount() - start < 15000) {
            Frame();
        }
        Frames(30);
        Check(F(CMD_NAME) != 0, "Trok Chat ligado no SA-MP");
        if (mode == "teclas") {
            RunKeys();
        } else if (mode == "desligado") {
            RunOff();
        } else {
            RunR1();
        }
    }
    printf("== %d falha(s)\n", g_failures);
    fflush(stdout);
    ExitProcess(g_failures ? 1 : 0);
}

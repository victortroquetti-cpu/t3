// Teste do Trok Skin no Wine: imita o moonloader com tres scripts do mimgui, usando o cimguidx9.dll de
// verdade (o mesmo codigo do mimgui 1.7.1), e salva o retangulo de cada controle e uma captura da tela.
//
// Uso: skin_host.exe <saida> [skin] [alternar|cliques]
//   skin      carrega o Trok Skin.asi antes (como o ASI Loader); a config vem do Trok Skin.ini ao lado
//   alternar  carrega tambem um samp.dll falso (0.3.7 R1) e, depois da primeira carga, digita /trokskin
//             duas vezes, salvando a tela depois de cada uma (out_desligado.bmp e out_religado.bmp)
//   cliques   carrega o samp.dll falso e, depois da primeira carga, usa mouse e teclado simulados: no "Painel A",
//             clica no interruptor, arrasta o slider e clica no botao "Salvar"; clica no "Painel A2" e aperta Esc; abre a lista do Painel A
//             e aperta Esc (fecha a lista) e de novo (com o chat do SA-MP aberto e fechado); clica fora e aperta
//             Esc; clica no X do "Painel B". Grava os valores em out_cliques.txt e as telas out_lista.bmp (lista
//             aberta), out_lista_fechada.bmp e out_cliques.bmp (no fim). Cliques e teclas passam pelo
//             ImGui_ImplWin32_WndProcHandler de cada script, como no onWindowMessage do mimgui: o primeiro script
//             com io.WantCaptureMouse ou io.WantCaptureKeyboard ligado segura a mensagem e os seguintes nem veem.
//
// Scripts (os arquivos ficam em moonloader\, como no jogo: a skin le o de cada script):
//   "Painel A"  Trok_Painel_A.lua: tema claro do proprio script (StyleColorsLight) e cantos retos. Tem "trok" no
//               nome, mas nao usa o kit: e padronizado (o nome do arquivo nao conta). Abre tambem a janela "Painel
//               A2", sem botao de fechar
//   "Painel B"  painel_b.lua: tema escuro padrao; empurra fundo vermelho antes do Begin; botao e texto vermelhos
//   "HUD"       hud.lua: fundo transparente, sem titulo; texto com tamanho explicito (AddText/CalcTextSize)
//   "Casa"      casa.lua: mod da casa renomeado (sem "trok" no nome), com janela ##trokCasa e estilo proprio: a
//               skin nao pode mudar nada nele
// Depois de desenhar, descarrega e recarrega a DLL (como o Ctrl+R do moonloader) e desenha de novo.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>

#include "imgui.h" // 1.72: so structs e enums

namespace {

constexpr int W = 1280;
constexpr int H = 800;

struct Api {
    HMODULE dll;
    void*(__cdecl* ImplDX9_Init)(IDirect3DDevice9*);
    void(__cdecl* ImplDX9_Shutdown)(void*);
    void(__cdecl* ImplDX9_NewFrame)(void*);
    void(__cdecl* ImplDX9_RenderDrawData)(void*, ImDrawData*);
    bool(__cdecl* ImplWin32_Init)(HWND, INT64*, INT64*);
    void(__cdecl* ImplWin32_NewFrame)(HWND, INT64, INT64*);
    ImGuiContext*(__cdecl* CreateContext)(ImFontAtlas*);
    void(__cdecl* DestroyContext)(ImGuiContext*);
    void(__cdecl* SetCurrentContext)(ImGuiContext*);
    ImGuiIO*(__cdecl* GetIO)();
    ImGuiStyle*(__cdecl* GetStyle)();
    void(__cdecl* StyleColorsLight)(ImGuiStyle*);
    void(__cdecl* NewFrame)();
    void(__cdecl* Render)();
    ImDrawData*(__cdecl* GetDrawData)();
    ImFont*(__cdecl* AddFontFromFileTTF)(ImFontAtlas*, const char*, float, const ImFontConfig*, const ImWchar*);
    const ImWchar*(__cdecl* GetGlyphRangesCyrillic)(ImFontAtlas*);
    bool(__cdecl* Begin)(const char*, bool*, ImGuiWindowFlags);
    void(__cdecl* End)();
    void(__cdecl* SetNextWindowPos)(const ImVec2, ImGuiCond, const ImVec2);
    void(__cdecl* SetNextWindowSize)(const ImVec2, ImGuiCond);
    void(__cdecl* PushStyleColor)(ImGuiCol, const ImVec4);
    void(__cdecl* PopStyleColor)(int);
    void(__cdecl* Text)(const char*, ...);
    bool(__cdecl* Button)(const char*, const ImVec2);
    void(__cdecl* SameLine)(float, float);
    bool(__cdecl* Checkbox)(const char*, bool*);
    bool(__cdecl* SliderFloat)(const char*, float*, float, float, const char*, float);
    bool(__cdecl* InputText)(const char*, char*, size_t, ImGuiInputTextFlags, ImGuiInputTextCallback, void*);
    bool(__cdecl* ComboStr)(const char*, int*, const char*, int);
    void(__cdecl* Separator)();
    bool(__cdecl* Selectable)(const char*, bool, ImGuiSelectableFlags, const ImVec2);
    bool(__cdecl* CollapsingHeader)(const char*, ImGuiTreeNodeFlags);
    bool(__cdecl* BeginTabBar)(const char*, ImGuiTabBarFlags);
    void(__cdecl* EndTabBar)();
    bool(__cdecl* BeginTabItem)(const char*, bool*, ImGuiTabItemFlags);
    void(__cdecl* EndTabItem)();
    void(__cdecl* GetItemRectMin)(ImVec2*);
    void(__cdecl* GetItemRectMax)(ImVec2*);
    ImDrawList*(__cdecl* GetWindowDrawList)();
    ImFont*(__cdecl* GetFont)();
    void(__cdecl* AddTextFontPtr)(ImDrawList*, const ImFont*, float, const ImVec2, ImU32, const char*, const char*, float,
                                  const ImVec4*);
    void(__cdecl* CalcTextSizeA)(ImVec2*, ImFont*, float, float, float, const char*, const char*, const char**);
    LRESULT(__cdecl* WndProcHandler)(HWND, UINT, WPARAM, LPARAM);
};

template <typename T> void Get(HMODULE dll, const char* name, T& out) {
    out = reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(dll, name)));
    if (!out) {
        printf("FALHA: cimguidx9.dll sem %s\n", name);
        ExitProcess(2);
    }
}

bool LoadApi(Api& a) {
    // Como o LuaJIT (ffi.load): LoadLibraryExA.
    a.dll = LoadLibraryExA("cimguidx9.dll", nullptr, 0);
    if (!a.dll) {
        return false;
    }
    Get(a.dll, "ImGui_ImplDX9_Init", a.ImplDX9_Init);
    Get(a.dll, "ImGui_ImplDX9_Shutdown", a.ImplDX9_Shutdown);
    Get(a.dll, "ImGui_ImplDX9_NewFrame", a.ImplDX9_NewFrame);
    Get(a.dll, "ImGui_ImplDX9_RenderDrawData", a.ImplDX9_RenderDrawData);
    Get(a.dll, "ImGui_ImplWin32_Init", a.ImplWin32_Init);
    Get(a.dll, "ImGui_ImplWin32_NewFrame", a.ImplWin32_NewFrame);
    Get(a.dll, "igCreateContext", a.CreateContext);
    Get(a.dll, "igDestroyContext", a.DestroyContext);
    Get(a.dll, "igSetCurrentContext", a.SetCurrentContext);
    Get(a.dll, "igGetIO", a.GetIO);
    Get(a.dll, "igGetStyle", a.GetStyle);
    Get(a.dll, "igStyleColorsLight", a.StyleColorsLight);
    Get(a.dll, "igNewFrame", a.NewFrame);
    Get(a.dll, "igRender", a.Render);
    Get(a.dll, "igGetDrawData", a.GetDrawData);
    Get(a.dll, "ImFontAtlas_AddFontFromFileTTF", a.AddFontFromFileTTF);
    Get(a.dll, "ImFontAtlas_GetGlyphRangesCyrillic", a.GetGlyphRangesCyrillic);
    Get(a.dll, "igBegin", a.Begin);
    Get(a.dll, "igEnd", a.End);
    Get(a.dll, "igSetNextWindowPos", a.SetNextWindowPos);
    Get(a.dll, "igSetNextWindowSize", a.SetNextWindowSize);
    Get(a.dll, "igPushStyleColor", a.PushStyleColor);
    Get(a.dll, "igPopStyleColor", a.PopStyleColor);
    Get(a.dll, "igText", a.Text);
    Get(a.dll, "igButton", a.Button);
    Get(a.dll, "igSameLine", a.SameLine);
    Get(a.dll, "igCheckbox", a.Checkbox);
    Get(a.dll, "igSliderFloat", a.SliderFloat);
    Get(a.dll, "igInputText", a.InputText);
    Get(a.dll, "igComboStr", a.ComboStr);
    Get(a.dll, "igSeparator", a.Separator);
    Get(a.dll, "igSelectable", a.Selectable);
    Get(a.dll, "igCollapsingHeader", a.CollapsingHeader);
    Get(a.dll, "igBeginTabBar", a.BeginTabBar);
    Get(a.dll, "igEndTabBar", a.EndTabBar);
    Get(a.dll, "igBeginTabItem", a.BeginTabItem);
    Get(a.dll, "igEndTabItem", a.EndTabItem);
    Get(a.dll, "igGetItemRectMin_nonUDT", a.GetItemRectMin);
    Get(a.dll, "igGetItemRectMax_nonUDT", a.GetItemRectMax);
    Get(a.dll, "igGetWindowDrawList", a.GetWindowDrawList);
    Get(a.dll, "igGetFont", a.GetFont);
    Get(a.dll, "ImDrawList_AddTextFontPtr", a.AddTextFontPtr);
    Get(a.dll, "ImFont_CalcTextSizeA_nonUDT", a.CalcTextSizeA);
    Get(a.dll, "ImGui_ImplWin32_WndProcHandler", a.WndProcHandler);
    return true;
}

struct Script {
    const char* name;
    char ini[MAX_PATH] = {}; // como o mimgui: config\mimgui\<arquivo do script>.ini
    void* d3d = nullptr;
    ImGuiContext* ctx = nullptr;
    INT64 ticks = 0, time = 0;
    bool check = true;
    float volume = 65;
    char nome[64] = "Victor_Trok";
    int modo = 1;
    bool open = true; // a janela some quando o X (ou o Esc) fecha
    int saved = 0;    // cliques no botao "Salvar"
    // Onde o botao, o interruptor, o slider e a lista ficaram no ultimo quadro.
    ImVec2 saveMin, saveMax, checkMin, checkMax, sliderMin, sliderMax, comboMin, comboMax;
};

Api g;
FILE* g_rects = nullptr;
HWND g_hwnd = nullptr;
IDirect3DDevice9* g_device = nullptr;
bool g_record = false;

// Mouse simulado (modo cliques). A posicao vale para todos os scripts (o mimgui le o cursor a cada quadro); o botao
// chega por mensagem da janela, como no jogo (Mouse()).
struct Mouse {
    bool on = false;
    float x = -1, y = -1;
    bool down = false;
} g_mouse;

void Rect(const char* script, const char* what) {
    if (!g_record) {
        return;
    }
    ImVec2 a, b;
    g.GetItemRectMin(&a);
    g.GetItemRectMax(&b);
    fprintf(g_rects, "%s|%s %.2f %.2f %.2f %.2f\n", script, what, a.x, a.y, b.x, b.y);
}

// Como o InitializeRenderer do mimgui: contexto, backend e a fonte padrao (Trebuchet Bold 14 com cirilico).
void Init(Script& s, const char* systemFont) {
    s.d3d = g.ImplDX9_Init(g_device);
    s.ctx = g.CreateContext(nullptr);
    g.SetCurrentContext(s.ctx);
    g.ImplWin32_Init(g_hwnd, &s.ticks, &s.time);
    ImGuiIO* io = g.GetIO();
    // O mimgui grava <moonloader>\config\mimgui\<arquivo do script>.ini aqui, antes de qualquer fonte (a skin
    // tira dali o nome e o arquivo do script).
    const char* file = strcmp(s.name, "Painel A") == 0   ? "Trok_Painel_A.lua"
                       : strcmp(s.name, "Painel B") == 0 ? "painel_b.lua"
                       : strcmp(s.name, "HUD") == 0      ? "hud.lua"
                                                         : "casa.lua";
    char dir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, dir);
    snprintf(s.ini, sizeof(s.ini), "%s\\moonloader\\config\\mimgui\\%s.ini", dir, file);
    io->IniFilename = s.ini;
    io->LogFilename = nullptr;
    g.AddFontFromFileTTF(io->Fonts, systemFont, 14.0f, nullptr, g.GetGlyphRangesCyrillic(io->Fonts));
    // OnInitialize de cada script.
    if (strcmp(s.name, "Painel A") == 0) {
        g.StyleColorsLight(nullptr);
        g.GetStyle()->WindowRounding = 0;
        g.GetStyle()->FrameRounding = 0;
    } else if (strcmp(s.name, "HUD") == 0) {
        g.GetStyle()->Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
        g.GetStyle()->WindowBorderSize = 0;
    } else if (strcmp(s.name, "Casa") == 0) {
        g.GetStyle()->WindowRounding = 0;
        g.GetStyle()->FrameRounding = 2;
    }
}

void Widgets(Script& s) {
    g.Text("Configura\xc3\xa7\xc3\xb5\x65s do menu (%s)", s.name);
    Rect(s.name, "texto");
    if (g.Button("Salvar", ImVec2(0, 0))) {
        ++s.saved;
    }
    Rect(s.name, "salvar");
    g.GetItemRectMin(&s.saveMin);
    g.GetItemRectMax(&s.saveMax);
    g.SameLine(0, -1);
    g.Button("Fechar", ImVec2(0, 0));
    Rect(s.name, "fechar");
    g.Checkbox("Ativar som", &s.check);
    Rect(s.name, "checkbox");
    g.GetItemRectMin(&s.checkMin);
    g.GetItemRectMax(&s.checkMax);
    g.SliderFloat("Volume", &s.volume, 0, 100, "%.0f", 1.0f);
    Rect(s.name, "slider");
    g.GetItemRectMin(&s.sliderMin);
    g.GetItemRectMax(&s.sliderMax);
    g.InputText("Nome", s.nome, sizeof(s.nome), 0, nullptr, nullptr);
    Rect(s.name, "campo");
    g.ComboStr("Modo", &s.modo, "Um\0Dois\0Tr\xc3\xaas\0\0", -1);
    Rect(s.name, "combo");
    g.GetItemRectMin(&s.comboMin);
    g.GetItemRectMax(&s.comboMax);
    g.Separator();
    g.Selectable("Item selecionado", true, 0, ImVec2(0, 0));
    Rect(s.name, "selecionavel");
    g.CollapsingHeader("Avan\xc3\xa7\x61\x64o", 0);
    Rect(s.name, "cabecalho");
    if (g.BeginTabBar("abas", 0)) {
        if (g.BeginTabItem("Geral", nullptr, 0)) {
            Rect(s.name, "aba");
            g.EndTabItem();
        }
        if (g.BeginTabItem("Teclas", nullptr, 0)) {
            g.EndTabItem();
        }
        g.EndTabBar();
    }
    g.Text("\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82, \xd0\xbc\xd0\xb8\xd1\x80 (cir\xc3\xadlico)");
    Rect(s.name, "cirilico");
}

void Draw(Script& s, int index) {
    if (strcmp(s.name, "Casa") == 0) {
        g.SetNextWindowPos(ImVec2(940, 200), ImGuiCond_Always, ImVec2(0, 0));
        g.SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
        g.PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.2f, 0.6f, 1.0f));
        g.Begin("##trokCasa", nullptr, ImGuiWindowFlags_NoTitleBar);
        g.PopStyleColor(1);
        g.Text("Janela da casa");
        Rect(s.name, "texto");
        g.Button("Botao padrao", ImVec2(0, 0));
        Rect(s.name, "botao");
        g.Checkbox("Caixa", &s.check);
        Rect(s.name, "caixa");
        g.End();
        return;
    }
    if (strcmp(s.name, "HUD") == 0) {
        g.SetNextWindowPos(ImVec2(940, 40), ImGuiCond_Always, ImVec2(0, 0));
        g.SetNextWindowSize(ImVec2(300, 120), ImGuiCond_Always);
        g.Begin("HUD", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoInputs);
        g.Text("Velocidade 120 km/h");
        Rect(s.name, "texto");
        // Texto desenhado com tamanho explicito (14 = o tamanho com que o script carregou a fonte).
        ImDrawList* dl = g.GetWindowDrawList();
        const char* t = "Combust\xc3\xadvel 75%";
        g.AddTextFontPtr(dl, g.GetFont(), 14.0f, ImVec2(950, 120), 0xFFFFFFFF, t, nullptr, 0, nullptr);
        ImVec2 size;
        g.CalcTextSizeA(&size, g.GetFont(), 14.0f, 10000, 0, t, nullptr, nullptr);
        if (g_record) {
            fprintf(g_rects, "%s|largura_explicita %.2f %.2f 0 0\n", s.name, size.x, size.y);
        }
        g.End();
        return;
    }
    if (strcmp(s.name, "Painel A") == 0) {
        // Segunda janela do script, sem botao de fechar (o Esc nao fecha janela sem X).
        g.SetNextWindowPos(ImVec2(40, 600), ImGuiCond_Always, ImVec2(0, 0));
        g.SetNextWindowSize(ImVec2(400, 150), ImGuiCond_Always);
        g.Begin("Painel A2", nullptr, 0);
        g.Text("Janela sem X");
        g.End();
    }
    if (!s.open) {
        return; // como os scripts: a janela so e desenhada enquanto esta aberta
    }
    g.SetNextWindowPos(ImVec2(40.0f + index * 440.0f, 40), ImGuiCond_Always, ImVec2(0, 0));
    g.SetNextWindowSize(ImVec2(400, 520), ImGuiCond_Always);
    bool red = strcmp(s.name, "Painel B") == 0;
    if (red) {
        g.PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.55f, 0.08f, 0.08f, 1.0f)); // tema do script so nesta janela
    }
    g.Begin(s.name, &s.open, 0);
    if (red) {
        g.PopStyleColor(1);
    }
    Widgets(s);
    if (red) {
        // Cores com significado dentro da janela: ficam.
        g.PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.3f, 0.3f, 1));
        g.Text("Erro: senha incorreta");
        Rect(s.name, "texto_vermelho");
        g.PopStyleColor(1);
        g.PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1));
        g.Button("Apagar tudo", ImVec2(0, 0));
        Rect(s.name, "botao_vermelho");
        g.PopStyleColor(1);
    }
    g.End();
}

void Frame(Script* scripts, int count) {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        DispatchMessageA(&msg);
    }
    g_device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(20, 90, 40), 1.0f, 0);
    g_device->BeginScene();
    for (int i = 0; i < count; ++i) {
        Script& s = scripts[i];
        g.SetCurrentContext(s.ctx);
        g.ImplDX9_NewFrame(s.d3d);
        g.ImplWin32_NewFrame(g_hwnd, s.ticks, &s.time);
        if (g_mouse.on) {
            g.GetIO()->MousePos = ImVec2(g_mouse.x, g_mouse.y);
        }
        g.NewFrame();
        Draw(s, i);
        g.Render();
        g.ImplDX9_RenderDrawData(s.d3d, g.GetDrawData());
    }
    g_device->EndScene();
}

bool Capture(const char* path) {
    IDirect3DSurface9* back = nullptr;
    IDirect3DSurface9* sys = nullptr;
    if (FAILED(g_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) ||
        FAILED(g_device->CreateOffscreenPlainSurface(W, H, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, nullptr)) ||
        FAILED(g_device->GetRenderTargetData(back, sys))) {
        return false;
    }
    D3DLOCKED_RECT lr;
    sys->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    FILE* f = fopen(path, "wb");
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
        const DWORD* src = reinterpret_cast<const DWORD*>(static_cast<BYTE*>(lr.pBits) + y * lr.Pitch);
        for (int x = 0; x < W; ++x) {
            row[x * 3] = src[x] & 0xFF;
            row[x * 3 + 1] = (src[x] >> 8) & 0xFF;
            row[x * 3 + 2] = (src[x] >> 16) & 0xFF;
        }
        fwrite(row, sizeof(row), 1, f);
    }
    fclose(f);
    sys->UnlockRect();
    sys->Release();
    back->Release();
    return true;
}

void Run(Script* scripts, int count, const char* systemFont) {
    for (int i = 0; i < count; ++i) {
        Init(scripts[i], systemFont);
    }
    for (int f = 0; f < 6; ++f) {
        g_record = f == 5;
        Frame(scripts, count);
    }
}

// Como o onWindowMessage do mimgui: a mensagem passa pelo ImGui de cada script, na ordem em que carregaram, e o
// primeiro que fica com ela (io.WantCaptureKeyboard ligado logo depois) segura: os seguintes e o jogo nao recebem.
// Devolve o script que segurou ou "jogo".
const char* Key(Script* scripts, int count, UINT msg, WPARAM vk, LPARAM lParam) {
    for (int i = 0; i < count; ++i) {
        g.SetCurrentContext(scripts[i].ctx);
        g.WndProcHandler(g_hwnd, msg, vk, lParam);
        if (g.GetIO()->WantCaptureKeyboard) {
            return scripts[i].name;
        }
    }
    return "jogo";
}

// Como o onWindowMessage do mimgui com o mouse: o primeiro script com o mouse em cima de uma janela dele (ou com
// popup aberto: io.WantCaptureMouse) segura o clique, e os scripts seguintes nem veem. Devolve quem segurou.
const char* Mouse(Script* scripts, int count, UINT msg, LPARAM pos) {
    for (int i = 0; i < count; ++i) {
        g.SetCurrentContext(scripts[i].ctx);
        g.WndProcHandler(g_hwnd, msg, msg == WM_LBUTTONDOWN ? MK_LBUTTON : 0, pos);
        if (g.GetIO()->WantCaptureMouse) {
            return scripts[i].name;
        }
    }
    return "jogo";
}

// Mouse em (x, y) com o botao esquerdo como pedido, e um quadro.
void MouseAt(Script* scripts, int count, float x, float y, bool down) {
    g_mouse.on = true;
    g_mouse.x = x;
    g_mouse.y = y;
    if (down != g_mouse.down) {
        g_mouse.down = down;
        Mouse(scripts, count, down ? WM_LBUTTONDOWN : WM_LBUTTONUP, MAKELPARAM(static_cast<int>(x), static_cast<int>(y)));
    }
    Frame(scripts, count);
}

void Click(Script* scripts, int count, float x, float y) {
    MouseAt(scripts, count, x, y, false);
    MouseAt(scripts, count, x, y, true);
    MouseAt(scripts, count, x, y, false);
}

constexpr LPARAM KEY_DOWN = 0x00010001, KEY_REPEAT = 0x40010001, KEY_UP = 0xC0010001;

// Modo cliques: o "Painel A" usado com mouse e teclado; o "Painel B" fechado pelo X. Valores em <saida>_cliques.txt.
void Clicks(Script* scripts, int count, HMODULE samp, const char* out) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s_cliques.txt", out);
    FILE* f = fopen(path, "w");
    Script& a = scripts[0];
    auto values = [&](const char* when) {
        fprintf(f, "%s check=%d volume=%.0f aberto=%d\n", when, a.check, a.volume, a.open);
    };
    values("antes");
    // Interruptor (ou caixa de marcar): clique perto do comeco do item, no meio da altura.
    float cy = (a.checkMin.y + a.checkMax.y) * 0.5f;
    Click(scripts, count, a.checkMin.x + 10, cy);
    values("interruptor");
    // Slider: aperta na ponta esquerda (vai ao minimo), arrasta ate a ponta direita (maximo) e solta.
    float sy = (a.sliderMin.y + a.sliderMax.y) * 0.5f;
    MouseAt(scripts, count, a.sliderMin.x + 2, sy, false);
    MouseAt(scripts, count, a.sliderMin.x + 2, sy, true);
    values("slider_apertado");
    MouseAt(scripts, count, a.sliderMax.x - 2, sy, true);
    MouseAt(scripts, count, a.sliderMax.x - 2, sy, false);
    values("slider_solto");
    // Botao: o clique chega ao mod (o botao do kit e o do ImGui com outras medidas).
    Click(scripts, count, (a.saveMin.x + a.saveMax.x) * 0.5f, (a.saveMin.y + a.saveMax.y) * 0.5f);
    fprintf(f, "botao salvo=%d\n", a.saved);
    // Clique na janela sem X do mesmo script (o mais recente agora), com o Painel B ainda em foco no ImGui do script
    // dele (o clique no Painel A nunca chegou la): o Esc nao e de nenhum dos dois e vai para o jogo.
    Click(scripts, count, 240, 675);
    Frame(scripts, count); // no quadro em que o arraste da janela termina, o ImGui ainda segura o teclado
    fprintf(f, "esc_outro_script %s\n", Key(scripts, count, WM_KEYDOWN, VK_ESCAPE, KEY_DOWN));
    Frame(scripts, count);
    Key(scripts, count, WM_KEYUP, VK_ESCAPE, KEY_UP);
    Frame(scripts, count);
    fprintf(f, "esc_outro_script painel_b aberto=%d\n", scripts[1].open);
    // Lista suspensa: o clique abre a lista; o Esc fecha so a lista (a janela fica) e some para o jogo.
    Click(scripts, count, a.comboMin.x + 20, (a.comboMin.y + a.comboMax.y) * 0.5f);
    Frame(scripts, count);
    snprintf(path, sizeof(path), "%s_lista.bmp", out);
    Capture(path);
    fprintf(f, "esc_lista %s\n", Key(scripts, count, WM_KEYDOWN, VK_ESCAPE, KEY_DOWN));
    Frame(scripts, count);
    Key(scripts, count, WM_KEYUP, VK_ESCAPE, KEY_UP);
    Frame(scripts, count);
    snprintf(path, sizeof(path), "%s_lista_fechada.bmp", out);
    Capture(path);
    values("esc_lista");
    // Esc com o chat do SA-MP aberto (CInput do samp.dll falso): a tecla e do SA-MP, a janela fica.
    BYTE* input = *reinterpret_cast<BYTE**>(reinterpret_cast<BYTE*>(samp) + 0x21A0E8);
    *reinterpret_cast<int*>(input + 0x14E0) = 1;
    Frame(scripts, count);
    fprintf(f, "esc_chat %s\n", Key(scripts, count, WM_KEYDOWN, VK_ESCAPE, KEY_DOWN));
    Frame(scripts, count);
    Key(scripts, count, WM_KEYUP, VK_ESCAPE, KEY_UP);
    Frame(scripts, count);
    values("esc_chat");
    // Chat fechado e passada a folga de 150 ms: o Esc fecha o Painel A (em foco desde os cliques) e some para o
    // jogo, a repeticao tambem; a tecla solta passa.
    *reinterpret_cast<int*>(input + 0x14E0) = 0;
    Frame(scripts, count);
    Sleep(250);
    Frame(scripts, count);
    fprintf(f, "esc %s\n", Key(scripts, count, WM_KEYDOWN, VK_ESCAPE, KEY_DOWN));
    Frame(scripts, count);
    values("esc");
    fprintf(f, "esc_repeticao %s\n", Key(scripts, count, WM_KEYDOWN, VK_ESCAPE, KEY_REPEAT));
    Frame(scripts, count);
    fprintf(f, "esc_solto %s\n", Key(scripts, count, WM_KEYUP, VK_ESCAPE, KEY_UP));
    Frame(scripts, count);
    // Clique fora de todas as janelas (nenhuma fica em foco): o Esc vai para o jogo (o menu de pausa abre).
    Click(scripts, count, 700, 700);
    fprintf(f, "esc_sem_foco %s\n", Key(scripts, count, WM_KEYDOWN, VK_ESCAPE, KEY_DOWN));
    Frame(scripts, count);
    Key(scripts, count, WM_KEYUP, VK_ESCAPE, KEY_UP);
    Frame(scripts, count);
    // X do Painel B (janela em 480,40 de 400x520): cabecalho de 44, X de 28 a 18 da borda, vezes a escala da tela.
    float u = H / 1080.0f * 0.85f;
    u = u < 0.55f ? 0.55f : u;
    Script& b = scripts[1];
    Click(scripts, count, 480 + 400 - 26 * u, 40 + 22 * u);
    Frame(scripts, count);
    fprintf(f, "x_painel_b aberto=%d\n", b.open);
    fclose(f);
    snprintf(path, sizeof(path), "%s_cliques.bmp", out);
    Capture(path);
    g_mouse.on = false;
}

void Shutdown(Script* scripts, int count) {
    for (int i = 0; i < count; ++i) {
        g.SetCurrentContext(scripts[i].ctx);
        g.ImplDX9_Shutdown(scripts[i].d3d);
        g.DestroyContext(scripts[i].ctx);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("uso: skin_host.exe <saida> [skin]\n");
        return 2;
    }
    const char* out = argv[1];
    bool toggle = argc > 3 && strcmp(argv[3], "alternar") == 0;
    bool clicks = argc > 3 && strcmp(argv[3], "cliques") == 0;
    HMODULE samp = toggle || clicks ? LoadLibraryA("samp.dll") : nullptr;
    if ((toggle || clicks) && !samp) {
        printf("FALHA: samp.dll falso nao carregou\n");
        return 1;
    }
    if (argc > 2 && strcmp(argv[2], "skin") == 0) {
        if (!LoadLibraryA("Trok Skin.asi")) {
            printf("FALHA: Trok Skin.asi nao carregou\n");
            return 1;
        }
        Sleep(500); // o ASI Loader carrega bem antes do moonloader rodar os scripts
    }

    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "SkinHost";
    RegisterClassA(&wc);
    g_hwnd = CreateWindowExA(0, wc.lpszClassName, "skin", WS_POPUP | WS_VISIBLE, 0, 0, W, H, nullptr, nullptr,
                             wc.hInstance, nullptr);
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp = {};
    pp.BackBufferWidth = W;
    pp.BackBufferHeight = H;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.hDeviceWindow = g_hwnd;
    pp.Windowed = TRUE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp,
                                 &g_device))) {
        printf("FALHA: device\n");
        return 1;
    }

    char systemFont[MAX_PATH];
    GetWindowsDirectoryA(systemFont, MAX_PATH);
    strcat(systemFont, "\\Fonts\\trebucbd.ttf");

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s_rects.txt", out);
    g_rects = fopen(path, "w");

    Script scripts[4];
    scripts[0].name = "Painel A";
    scripts[1].name = "Painel B";
    scripts[2].name = "HUD";
    scripts[3].name = "Casa";
    if (!LoadApi(g)) {
        printf("FALHA: cimguidx9.dll nao carregou\n");
        return 1;
    }
    Run(scripts, 4, systemFont);
    snprintf(path, sizeof(path), "%s.bmp", out);
    Capture(path);
    g_device->Present(nullptr, nullptr, nullptr, nullptr);

    if (toggle || clicks) {
        // Campos do samp.dll falso (make_fake_samp.py): nome e callback do comando registrado. Registrado = a skin
        // ja conferiu a versao do SA-MP (e passa a ver o chat e os dialogos dele).
        typedef void(__cdecl * CmdProc)(const char*);
        BYTE* data = reinterpret_cast<BYTE*>(samp) + 0x300000;
        for (int i = 0; i < 100 && !*reinterpret_cast<CmdProc*>(data + 4); ++i) {
            Sleep(50);
        }
        CmdProc proc = *reinterpret_cast<CmdProc*>(data + 4);
        const char* name = *reinterpret_cast<const char**>(data);
        if (!proc || !name || strcmp(name, "trokskin") != 0) {
            printf("FALHA: /trokskin nao registrado\n");
            return 1;
        }
        if (clicks) {
            Clicks(scripts, 4, samp, out);
        }
    }
    if (toggle) {
        typedef void(__cdecl * CmdProc)(const char*);
        CmdProc proc = *reinterpret_cast<CmdProc*>(reinterpret_cast<BYTE*>(samp) + 0x300004);
        const char* shots[] = {"desligado", "religado"};
        for (const char* shot : shots) {
            proc("");
            for (int f = 0; f < 3; ++f) {
                Frame(scripts, 4);
            }
            snprintf(path, sizeof(path), "%s_%s.bmp", out, shot);
            Capture(path);
        }
    }

    // Ctrl+R do moonloader: os scripts fecham (a DLL descarrega se ninguem segurar) e carregam de novo.
    Shutdown(scripts, 4);
    FreeLibrary(g.dll);
    Script again[4];
    again[0].name = "Painel A";
    again[1].name = "Painel B";
    again[2].name = "HUD";
    again[3].name = "Casa";
    if (!LoadApi(g)) {
        printf("FALHA: cimguidx9.dll nao recarregou\n");
        return 1;
    }
    fprintf(g_rects, "--- recarregado\n");
    Run(again, 4, systemFont);
    snprintf(path, sizeof(path), "%s_recarregado.bmp", out);
    Capture(path);
    fclose(g_rects);
    printf("ok: %s\n", out);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), 0);
    return 0;
}

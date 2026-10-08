// Trok Skin (.asi) -- Victor_Trok
// Padroniza o visual dos menus de qualquer mod Lua feito com mimgui ou com o imgui antigo do moonloader
// (moon_imgui), sem editar os mods:
//  - tema da casa (cores, cantos, borda dos campos e titulo centralizado) em todo quadro de cada script;
//  - a fonte da casa no lugar das fontes de sistema (Trebuchet do mimgui, Arial, Tahoma...), com o tamanho
//    ajustado para o texto ocupar a mesma largura. Glifos que a fonte da casa nao tem vem da fonte original.
// Nada que mexe no layout e alterado: espacamentos, tamanhos e bordas de janela ficam como o script deixou.
// HUDs com fundo transparente ficam exatamente como o autor fez. A versao de teste "Trok Skin Layout.asi"
// (layout=1) tambem leva o tamanho das fontes e os espacamentos do kit da casa, e ai o layout muda.
//
// Como, no mimgui: ele carrega uma DLL nativa so (moonloader\lib\mimgui\cimguidx9.dll, Dear ImGui 1.72 +
// cimgui) e todo script chama as funcoes exportadas dela. A skin desvia algumas dessas funcoes com o MinHook
// assim que a DLL carrega. So liga se a DLL for o ImGui 1.72 (o mesmo dos headers usados aqui); com outra
// versao, nao faz nada e diz no log.
// No imgui antigo (moonloader\lib\imgui.lua + MoonImGui.dll, Dear ImGui 1.52): o MoonImGui.dll e um modulo C
// do Lua aberto por cada script; a skin desvia o luaopen_MoonImGui e roda no lua_State do script o
// moon_patch.lua (embutido), que padroniza pela API Lua do proprio moon_imgui. So liga no ImGui 1.52.
//
// Scripts da casa (Kill List, vitrine, mods novos com o kit) ficam intocados, tema e fonte. A skin reconhece pelo
// que o mod tem dentro, nao pelo nome do arquivo (que pode mudar): usa a pasta resource\trok (fonte e icones da
// casa) ou abre janela com id ##trok. O arquivo vem do config\mimgui\<script>.ini que o mimgui guarda no
// contexto de cada script, e do thisScript() do moonloader no imgui antigo.
//
// /trokskin liga e desliga o tema na hora (a fonte so muda quando os scripts recarregam) e anota no log
// cada janela que viu e o que fez com ela.
// Trok Skin.ini (ao lado do .asi): tema=1, fonte=1, manter=Titulo|Outro titulo (janelas que ficam como o
// autor fez) e manter_scripts=mod.lua|outro.lua (scripts inteiros que ficam como estao).

#include <windows.h>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "MinHook.h"
#include "imgui.h" // Dear ImGui 1.72 (mesmo commit do mimgui): so os structs e enums, nada e compilado aqui

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"

namespace {

// ---------------------------------------------------------------- log e configuracao

char g_logPath[MAX_PATH] = {};
char g_iniPath[MAX_PATH] = {};
// Em UTF-8: o ImGui 1.72 da DLL oficial (MSVC) abre arquivos com _wfopen do caminho em UTF-8, e a pasta do
// GTA pode ter acento.
char g_housePath[MAX_PATH * 3] = {};
HMODULE g_module = nullptr;
volatile bool g_theme = true;
volatile bool g_fontOn = true;
// layout=1 (versao de teste, "Trok Skin Layout.asi"): tambem o tamanho das fontes e os espacamentos da casa. Mexe no
// layout dos menus. Sem a linha no .ini, vale o padrao com que a versao foi compilada.
#ifndef TROK_LAYOUT_PADRAO
#define TROK_LAYOUT_PADRAO 0
#endif
volatile bool g_layout = TROK_LAYOUT_PADRAO != 0;
std::vector<std::string> g_kept;        // titulos (minusculos) das janelas que ficam como o autor fez
std::vector<std::string> g_keptScripts; // arquivos (minusculos) de scripts que ficam como estao

void Log(const char* fmt, ...) {
    if (!g_logPath[0]) {
        return;
    }
    FILE* file = fopen(g_logPath, "a");
    if (!file) {
        return;
    }
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(file, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
    va_list args;
    va_start(args, fmt);
    vfprintf(file, fmt, args);
    va_end(args);
    fputs("\n", file);
    fclose(file);
}

std::string Lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t");
    size_t b = s.find_last_not_of(" \t");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// Abre um caminho que pode vir em UTF-8 (o nosso, o do ImGui) ou no codigo de pagina do Windows (scripts).
FILE* OpenPath(const char* path) {
    wchar_t wide[MAX_PATH * 2];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, MAX_PATH * 2) > 0) {
        if (FILE* file = _wfopen(wide, L"rb")) {
            return file;
        }
    }
    return fopen(path, "rb");
}

void LoadConfig() {
    if (GetFileAttributesA(g_iniPath) == INVALID_FILE_ATTRIBUTES) {
        FILE* f = fopen(g_iniPath, "w");
        if (f) {
            fputs("; Trok Skin: visual da casa nos menus feitos com mimgui e com o imgui antigo do moonloader.\n"
                  "[skin]\n"
                  "; 1 liga, 0 desliga. O tema tambem liga e desliga no jogo com /trokskin.\n"
                  "tema=1\n"
                  "; Fonte da casa no lugar das fontes de sistema (vale quando os scripts carregam).\n"
                  "fonte=1\n"
                  "; Janelas que ficam como o autor fez: o titulo que aparece na janela, separados por |\n"
                  "manter=\n"
                  "; Scripts inteiros que ficam como estao: o nome do arquivo, separados por | (os da casa, que\n"
                  "; usam a pasta resource\\trok ou janelas ##trok, ja ficam sozinhos, com qualquer nome)\n"
                  "manter_scripts=\n"
                  "; layout=1 tambem padroniza o tamanho das fontes e os espacamentos (mexe no layout dos menus).\n"
                  "; Sem a linha, vale o padrao da versao: desligado no Trok Skin.asi, ligado no Trok Skin Layout.asi.\n",
                  f);
            fclose(f);
        }
    }
    g_theme = GetPrivateProfileIntA("skin", "tema", 1, g_iniPath) != 0;
    g_fontOn = GetPrivateProfileIntA("skin", "fonte", 1, g_iniPath) != 0;
    g_layout = GetPrivateProfileIntA("skin", "layout", TROK_LAYOUT_PADRAO, g_iniPath) != 0;
    auto readList = [](const char* key, std::vector<std::string>& out) {
        char raw[2048] = {};
        GetPrivateProfileStringA("skin", key, "", raw, sizeof(raw), g_iniPath);
        std::string list = raw;
        size_t start = 0;
        while (start <= list.size()) {
            size_t bar = list.find('|', start);
            std::string item = Trim(list.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
            if (!item.empty()) {
                out.push_back(Lower(item));
            }
            if (bar == std::string::npos) {
                break;
            }
            start = bar + 1;
        }
    };
    readList("manter", g_kept);
    readList("manter_scripts", g_keptScripts);
    Log("config: tema %s, fonte %s, layout %s, %d janela(s) e %d script(s) mantidos", g_theme ? "ligado" : "desligado",
        g_fontOn ? "ligada" : "desligada",
        g_layout ? "LIGADO (tamanho de fonte e espacamentos da casa: mexe no layout)" : "desligado",
        static_cast<int>(g_kept.size()), static_cast<int>(g_keptScripts.size()));
}

// ---------------------------------------------------------------- funcoes do cimguidx9.dll

typedef const char*(__cdecl* GetVersionFn)();
typedef ImGuiContext*(__cdecl* GetCurrentContextFn)();
typedef ImGuiStyle*(__cdecl* GetStyleFn)();
typedef ImGuiIO*(__cdecl* GetIOFn)();
typedef ImFontConfig*(__cdecl* FontConfigNewFn)();
typedef void(__cdecl* FontConfigDestroyFn)(ImFontConfig*);
typedef void(__cdecl* NewFrameFn)();
typedef bool(__cdecl* BeginFn)(const char*, bool*, ImGuiWindowFlags);
typedef void(__cdecl* DestroyContextFn)(ImGuiContext*);
typedef ImFont*(__cdecl* AddFontFromFileFn)(ImFontAtlas*, const char*, float, const ImFontConfig*, const ImWchar*);
typedef ImFont*(__cdecl* AddFontFromMemoryFn)(ImFontAtlas*, void*, int, float, const ImFontConfig*, const ImWchar*);
typedef ImFont*(__cdecl* AddFontFromBase85Fn)(ImFontAtlas*, const char*, float, const ImFontConfig*, const ImWchar*);
typedef void(__cdecl* AtlasFn)(ImFontAtlas*);
typedef void(__cdecl* AddTextFontFn)(ImDrawList*, const ImFont*, float, const ImVec2, ImU32, const char*, const char*,
                                     float, const ImVec4*);
typedef void(__cdecl* CalcTextSizeFn)(ImVec2*, ImFont*, float, float, float, const char*, const char*, const char**);

GetVersionFn p_igGetVersion = nullptr;
GetCurrentContextFn p_igGetCurrentContext = nullptr;
GetStyleFn p_igGetStyle = nullptr;
GetIOFn p_igGetIO = nullptr;
FontConfigNewFn p_ImFontConfig_ImFontConfig = nullptr;
FontConfigDestroyFn p_ImFontConfig_destroy = nullptr;
// Originais (trampolins do MinHook).
NewFrameFn o_igNewFrame = nullptr;
BeginFn o_igBegin = nullptr;
DestroyContextFn o_igDestroyContext = nullptr;
AddFontFromFileFn o_AddFontFromFileTTF = nullptr;
AddFontFromMemoryFn o_AddFontFromMemoryTTF = nullptr;
AddFontFromMemoryFn o_AddFontFromMemoryCompressedTTF = nullptr;
AddFontFromBase85Fn o_AddFontFromBase85 = nullptr;
AtlasFn o_AtlasClear = nullptr;
AtlasFn o_AtlasClearFonts = nullptr;
AtlasFn o_AtlasDestroy = nullptr;
AddTextFontFn o_AddTextFontPtr = nullptr;
CalcTextSizeFn o_CalcTextSizeA = nullptr;

// ---------------------------------------------------------------- tema da casa

// Campos do estilo que a skin troca: os que nao mexem no layout (cantos, borda dos campos, alinhamento do
// titulo e cores) e, so com layout=1, os espacamentos da casa. As bordas de janela, filha e popup ficam como o
// script deixou (a borda da janela filha muda o recuo dela, por isso nao entra).
struct Look {
    float windowRounding, childRounding, popupRounding, frameRounding, scrollbarRounding, grabRounding, tabRounding;
    float frameBorderSize;
    ImVec2 windowTitleAlign;
    ImVec2 windowPadding, framePadding, itemSpacing, itemInnerSpacing; // layout=1
    float indentSpacing, scrollbarSize, grabMinSize;                    // layout=1
    ImVec4 colors[ImGuiCol_COUNT];
};

void Read(const ImGuiStyle& s, Look& l) {
    l.windowRounding = s.WindowRounding;
    l.childRounding = s.ChildRounding;
    l.popupRounding = s.PopupRounding;
    l.frameRounding = s.FrameRounding;
    l.scrollbarRounding = s.ScrollbarRounding;
    l.grabRounding = s.GrabRounding;
    l.tabRounding = s.TabRounding;
    l.frameBorderSize = s.FrameBorderSize;
    l.windowTitleAlign = s.WindowTitleAlign;
    l.windowPadding = s.WindowPadding;
    l.framePadding = s.FramePadding;
    l.itemSpacing = s.ItemSpacing;
    l.itemInnerSpacing = s.ItemInnerSpacing;
    l.indentSpacing = s.IndentSpacing;
    l.scrollbarSize = s.ScrollbarSize;
    l.grabMinSize = s.GrabMinSize;
    memcpy(l.colors, s.Colors, sizeof(l.colors));
}

void Write(ImGuiStyle& s, const Look& l) {
    s.WindowRounding = l.windowRounding;
    s.ChildRounding = l.childRounding;
    s.PopupRounding = l.popupRounding;
    s.FrameRounding = l.frameRounding;
    s.ScrollbarRounding = l.scrollbarRounding;
    s.GrabRounding = l.grabRounding;
    s.TabRounding = l.tabRounding;
    s.FrameBorderSize = l.frameBorderSize;
    s.WindowTitleAlign = l.windowTitleAlign;
    s.WindowPadding = l.windowPadding;
    s.FramePadding = l.framePadding;
    s.ItemSpacing = l.itemSpacing;
    s.ItemInnerSpacing = l.itemInnerSpacing;
    s.IndentSpacing = l.indentSpacing;
    s.ScrollbarSize = l.scrollbarSize;
    s.GrabMinSize = l.grabMinSize;
    memcpy(s.Colors, l.colors, sizeof(l.colors));
}

bool Same(const ImVec4& a, const ImVec4& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

ImVec4 White(float alpha) {
    return ImVec4(1.0f, 1.0f, 1.0f, alpha);
}

// Paleta dos menus da casa (os mesmos valores do kit Trok UI): fundo 12,12,12 a 252, borda branca 14,
// texto 240, dica branca 118.
void HouseColors(ImVec4* c) {
    const ImVec4 bg(12 / 255.0f, 12 / 255.0f, 12 / 255.0f, 252 / 255.0f);
    c[ImGuiCol_Text] = ImVec4(240 / 255.0f, 240 / 255.0f, 240 / 255.0f, 1.0f);
    c[ImGuiCol_TextDisabled] = White(118 / 255.0f);
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = ImVec4(16 / 255.0f, 16 / 255.0f, 16 / 255.0f, 252 / 255.0f);
    c[ImGuiCol_Border] = White(14 / 255.0f);
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = White(0.035f);
    c[ImGuiCol_FrameBgHovered] = White(0.06f);
    c[ImGuiCol_FrameBgActive] = White(0.08f);
    c[ImGuiCol_TitleBg] = bg;
    c[ImGuiCol_TitleBgActive] = bg;
    c[ImGuiCol_TitleBgCollapsed] = bg;
    c[ImGuiCol_MenuBarBg] = White(0.03f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = White(0.12f);
    c[ImGuiCol_ScrollbarGrabHovered] = White(0.2f);
    c[ImGuiCol_ScrollbarGrabActive] = White(0.3f);
    c[ImGuiCol_CheckMark] = ImVec4(0.94f, 0.94f, 0.94f, 1.0f);
    c[ImGuiCol_SliderGrab] = White(0.9f);
    c[ImGuiCol_SliderGrabActive] = White(1.0f);
    c[ImGuiCol_Button] = White(0.045f);
    c[ImGuiCol_ButtonHovered] = White(0.08f);
    c[ImGuiCol_ButtonActive] = White(0.12f);
    c[ImGuiCol_Header] = White(0.06f);
    c[ImGuiCol_HeaderHovered] = White(0.045f);
    c[ImGuiCol_HeaderActive] = White(0.09f);
    c[ImGuiCol_Separator] = White(0.06f);
    c[ImGuiCol_SeparatorHovered] = White(0.15f);
    c[ImGuiCol_SeparatorActive] = White(0.25f);
    c[ImGuiCol_ResizeGrip] = White(0.04f);
    c[ImGuiCol_ResizeGripHovered] = White(0.12f);
    c[ImGuiCol_ResizeGripActive] = White(0.2f);
    c[ImGuiCol_Tab] = White(0.03f);
    c[ImGuiCol_TabHovered] = White(0.08f);
    c[ImGuiCol_TabActive] = White(0.11f);
    c[ImGuiCol_TabUnfocused] = White(0.02f);
    c[ImGuiCol_TabUnfocusedActive] = White(0.07f);
    c[ImGuiCol_PlotLines] = White(0.7f);
    c[ImGuiCol_PlotLinesHovered] = White(1.0f);
    c[ImGuiCol_PlotHistogram] = White(0.7f);
    c[ImGuiCol_PlotHistogramHovered] = White(1.0f);
    c[ImGuiCol_TextSelectedBg] = White(0.18f);
    c[ImGuiCol_DragDropTarget] = White(0.9f);
    c[ImGuiCol_NavHighlight] = White(0.5f);
    c[ImGuiCol_NavWindowingHighlight] = White(0.7f);
    c[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.5f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.55f);
}

ImVec4 g_house[ImGuiCol_COUNT];

// Fundos: se o script deixou transparente (HUD por cima do jogo), a escolha e dele.
bool IsBackground(int i) {
    return i == ImGuiCol_WindowBg || i == ImGuiCol_ChildBg || i == ImGuiCol_PopupBg || i == ImGuiCol_MenuBarBg ||
           i == ImGuiCol_TitleBg || i == ImGuiCol_TitleBgActive || i == ImGuiCol_TitleBgCollapsed ||
           i == ImGuiCol_FrameBg || i == ImGuiCol_FrameBgHovered || i == ImGuiCol_FrameBgActive;
}

// Cores com significado que o script empurra so para um trecho (texto vermelho, grafico verde): nao trocar
// no Begin, so na base do quadro.
bool IsSemantic(int i) {
    return i == ImGuiCol_Text || i == ImGuiCol_TextDisabled || i == ImGuiCol_PlotLines ||
           i == ImGuiCol_PlotLinesHovered || i == ImGuiCol_PlotHistogram || i == ImGuiCol_PlotHistogramHovered;
}

float Scale(float screenHeight) {
    float u = screenHeight / 1080.0f * 0.85f;
    return u < 0.55f ? 0.55f : u;
}

void HouseScalars(Look& l, float u) {
    l.windowRounding = 10 * u;
    l.childRounding = 6 * u;
    l.popupRounding = 8 * u;
    l.frameRounding = 6 * u;
    l.scrollbarRounding = 6 * u;
    l.grabRounding = 6 * u;
    l.tabRounding = 6 * u;
    l.frameBorderSize = 1.0f; // contorno dos campos e o divisor embaixo do titulo
    l.windowTitleAlign = ImVec2(0.5f, 0.5f);
}

// Espacamentos da casa (layout=1), os do kit: margem da janela 18x16; campo de 24 com texto de 16 (folga de 4 em
// cima e embaixo); linhas de 28 com 2 de intervalo (6 entre um campo e outro); recuo 12. Mexem no layout.
void HouseSpacing(Look& l, float u) {
    l.windowPadding = ImVec2(18 * u, 16 * u);
    l.framePadding = ImVec2(10 * u, 4 * u);
    l.itemSpacing = ImVec2(10 * u, 6 * u);
    l.itemInnerSpacing = ImVec2(8 * u, 6 * u);
    l.indentSpacing = 12 * u;
    l.scrollbarSize = 6 * u;
    l.grabMinSize = 10 * u;
}

void House(const Look& script, Look& out, float u) {
    out = script;
    HouseScalars(out, u);
    if (g_layout) {
        HouseSpacing(out, u);
    }
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        bool transparent = IsBackground(i) && script.colors[i].w < 0.5f;
        out.colors[i] = transparent ? script.colors[i] : g_house[i];
    }
}

// Cada contexto e um script do moonloader. O mimgui grava config\mimgui\<script>.ini no IniFilename logo ao
// criar o contexto, antes de qualquer fonte: e dali que sai o nome do script.
struct Script {
    std::string name;                                    // "meu_mod.lua" ("?" se o script apagou o IniFilename antes)
    bool untouched = false;                              // mod da casa ou em manter_scripts=
    float sizeFactor = 0.0f; // layout=1: tamanho de texto da casa / primeira fonte do atlas (0 = ainda nao carregou)
    std::vector<std::pair<std::string, std::string>> windows; // titulo -> o que a skin fez (para o log)
};
std::unordered_map<ImGuiContext*, Script> g_scripts;

// Mod da casa, pelo que tem dentro (o nome do arquivo pode mudar a vontade): usa a pasta da casa
// (moonloader\resource\trok, a fonte e os icones do kit) ou abre janela com id ##trok. Le o arquivo do script; no
// compilado (.luac) os textos aparecem do mesmo jeito.
const char* HouseReason(const std::string& scriptPath) {
    FILE* file = scriptPath.empty() ? nullptr : OpenPath(scriptPath.c_str());
    if (!file) {
        return nullptr;
    }
    std::string text;
    char buffer[65536];
    size_t n = 0;
    while (text.size() < (8u << 20) && (n = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        text.append(buffer, n);
    }
    fclose(file);
    text = Lower(text);
    // "resource", uma ou mais barras (no fonte Lua a barra vem dobrada: \\resource\\trok\\), "trok", barra.
    for (size_t at = text.find("resource"); at != std::string::npos; at = text.find("resource", at + 8)) {
        size_t p = at + 8;
        size_t slashes = 0;
        for (; p < text.size() && (text[p] == '\\' || text[p] == '/'); ++p) {
            ++slashes;
        }
        if (slashes && text.compare(p, 4, "trok") == 0 && p + 4 < text.size() &&
            (text[p + 4] == '\\' || text[p + 4] == '/')) {
            return "da casa (usa a pasta resource\\trok): fica como esta";
        }
    }
    if (text.find("##trok") != std::string::npos) {
        return "da casa (tem janela ##trok): fica como esta";
    }
    return nullptr;
}

// Por que o script fica como esta (listado em manter_scripts= ou mod da casa), ou nullptr se e padronizado.
const char* UntouchedReason(const std::string& name, const std::string& path) {
    std::string low = Lower(name);
    for (const std::string& k : g_keptScripts) {
        if (k == low || k + ".lua" == low) {
            return "em manter_scripts: fica como esta";
        }
    }
    return HouseReason(path);
}

// So com o contexto do script ativo (le o IO dele).
Script& ScriptOf(ImGuiContext* ctx) {
    auto found = g_scripts.find(ctx);
    if (found != g_scripts.end()) {
        return found->second;
    }
    Script& script = g_scripts[ctx];
    const char* ini = p_igGetIO()->IniFilename;
    script.name = "?";
    std::string file; // <moonloader>\<script>: o mimgui grava <moonloader>\config\mimgui\<script>.ini
    if (ini && *ini) {
        std::string path = ini;
        size_t slash = path.find_last_of("\\/");
        std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
        if (base.size() > 4 && Lower(base.substr(base.size() - 4)) == ".ini") {
            base.resize(base.size() - 4);
        }
        if (!base.empty()) {
            script.name = base;
            size_t config = Lower(path).rfind("\\config\\mimgui\\");
            if (config != std::string::npos) {
                file = path.substr(0, config) + "\\" + base;
            }
        }
    }
    const char* reason = UntouchedReason(script.name, file);
    script.untouched = reason != nullptr;
    Log("script do mimgui: %s (%s)", script.name.c_str(), reason ? reason : "padronizado");
    return script;
}

// Anota no log a primeira vez que a janela aparece e cada vez que a decisao muda.
void NoteWindow(Script& script, const char* name, const char* decision) {
    std::string title = name ? name : "";
    size_t hidden = title.find("##");
    std::string shown = hidden == std::string::npos ? title : title.substr(0, hidden);
    if (shown.empty()) {
        shown = title; // so id (##...): mostra o id
    }
    for (auto& w : script.windows) {
        if (w.first == shown) {
            if (w.second != decision) {
                w.second = decision;
                Log("janela \"%s\" de %s: %s", shown.c_str(), script.name.c_str(), decision);
            }
            return;
        }
    }
    if (script.windows.size() >= 32) {
        return; // titulo que muda a cada quadro (FPS no titulo...): o log nao enche
    }
    script.windows.emplace_back(shown, decision);
    Log("janela \"%s\" de %s: %s", shown.c_str(), script.name.c_str(), decision);
}

// Estado por contexto (cada script do mimgui tem o seu): o visual do proprio script (orig), para o tema
// poder desligar na hora, e o que a skin escreveu no ultimo quadro (applied), para perceber o que o script
// mudou depois.
struct State {
    Look orig;
    Look applied;
    ImFontAtlas* atlas = nullptr;
};
std::unordered_map<ImGuiContext*, State> g_states;

// Copia para orig os campos que o script mudou desde a ultima escrita da skin.
void Absorb(State& st, const Look& cur) {
#define ABSORB(f)                                                                                                 \
    if (cur.f != st.applied.f) {                                                                                  \
        st.orig.f = cur.f;                                                                                        \
    }
    ABSORB(windowRounding)
    ABSORB(childRounding)
    ABSORB(popupRounding)
    ABSORB(frameRounding)
    ABSORB(scrollbarRounding)
    ABSORB(grabRounding)
    ABSORB(tabRounding)
    ABSORB(frameBorderSize)
    ABSORB(indentSpacing)
    ABSORB(scrollbarSize)
    ABSORB(grabMinSize)
#undef ABSORB
#define ABSORB2(f)                                                                                                \
    if (cur.f.x != st.applied.f.x || cur.f.y != st.applied.f.y) {                                                 \
        st.orig.f = cur.f;                                                                                        \
    }
    ABSORB2(windowTitleAlign)
    ABSORB2(windowPadding)
    ABSORB2(framePadding)
    ABSORB2(itemSpacing)
    ABSORB2(itemInnerSpacing)
#undef ABSORB2
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        if (!Same(cur.colors[i], st.applied.colors[i])) {
            st.orig.colors[i] = cur.colors[i];
        }
    }
}

// Inicio de cada quadro de cada script: base do estilo = tema da casa (ou o visual do script, desligado).
void ApplyFrame() {
    ImGuiContext* ctx = p_igGetCurrentContext();
    if (!ctx || ScriptOf(ctx).untouched) {
        return;
    }
    ImGuiStyle* style = p_igGetStyle();
    ImGuiIO* io = p_igGetIO();
    Look cur;
    Read(*style, cur);
    auto found = g_states.find(ctx);
    if (found == g_states.end()) {
        State st;
        st.orig = cur;
        st.applied = cur;
        found = g_states.emplace(ctx, st).first;
    } else {
        Absorb(found->second, cur);
    }
    State& st = found->second;
    st.atlas = io->Fonts;
    Look want;
    if (g_theme) {
        House(st.orig, want, Scale(io->DisplaySize.y));
    } else {
        want = st.orig;
    }
    Write(*style, want);
    st.applied = want;
}

bool Kept(const char* name) {
    if (!name) {
        return false;
    }
    // Janelas da casa (Kill List, vitrine, mods novos com o kit): o id comeca com ##trok e o visual ja e o certo.
    if (_strnicmp(name, "##trok", 6) == 0) {
        return true;
    }
    if (g_kept.empty()) {
        return false;
    }
    std::string title = name;
    size_t hidden = title.find("##");
    if (hidden != std::string::npos) {
        title = title.substr(0, hidden);
    }
    title = Lower(Trim(title));
    for (const std::string& k : g_kept) {
        if (k == title) {
            return true;
        }
    }
    return false;
}

// Volta ao visual do script o que ele nao empurrou para esta janela (com push, fica o que ele empurrou).
// spacingOnly: so os espacamentos (HUD com layout=1).
void KeepScript(Look& want, const Look& cur, const State& st, bool spacingOnly) {
#define KEEP(f)                                                                                                   \
    if (cur.f == st.applied.f) {                                                                                  \
        want.f = st.orig.f;                                                                                       \
    }
#define KEEP2(f)                                                                                                  \
    if (cur.f.x == st.applied.f.x && cur.f.y == st.applied.f.y) {                                                 \
        want.f = st.orig.f;                                                                                       \
    }
    KEEP2(windowPadding)
    KEEP2(framePadding)
    KEEP2(itemSpacing)
    KEEP2(itemInnerSpacing)
    KEEP(indentSpacing)
    KEEP(scrollbarSize)
    KEEP(grabMinSize)
    if (!spacingOnly) {
        KEEP(windowRounding)
        KEEP(childRounding)
        KEEP(popupRounding)
        KEEP(frameRounding)
        KEEP(scrollbarRounding)
        KEEP(grabRounding)
        KEEP(tabRounding)
        KEEP(frameBorderSize)
        KEEP2(windowTitleAlign)
        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            if (Same(cur.colors[i], st.applied.colors[i])) {
                want.colors[i] = st.orig.colors[i];
            }
        }
    }
#undef KEEP
#undef KEEP2
}

// Antes de cada janela: o script pode ter mudado cores ou cantos depois do inicio do quadro (ou empurrado
// para esta janela). Janela padronizada volta ao tema; janela mantida (manter= ou da casa, ##trok...) volta
// ao visual do script. HUD (fundo transparente) fica intocado.
void ApplyWindow(const char* name, ImGuiWindowFlags flags) {
    ImGuiContext* ctx = p_igGetCurrentContext();
    auto found = g_states.find(ctx);
    if (!ctx || found == g_states.end()) {
        return; // script intocado (nem tem estado) ou sem quadro ainda
    }
    Script& script = ScriptOf(ctx);
    State& st = found->second;
    ImGuiStyle* style = p_igGetStyle();
    if ((flags & ImGuiWindowFlags_NoBackground) || style->Colors[ImGuiCol_WindowBg].w < 0.5f) {
        NoteWindow(script, name, "fundo transparente (HUD): fica como esta");
        if (g_layout) {
            // O HUD fica onde o autor pos: os espacamentos voltam aos dele (a fonte vale para o script inteiro).
            Look cur;
            Read(*style, cur);
            Look want = cur;
            KeepScript(want, cur, st, true);
            Write(*style, want);
        }
        return;
    }
    bool kept = Kept(name);
    NoteWindow(script, name, kept ? "mantida (##trok ou manter=)" : g_theme ? "padronizada" : "tema desligado");
    Look cur;
    Read(*style, cur);
    Look want = cur;
    if (!g_theme || kept) {
        KeepScript(want, cur, st, false);
    } else {
        float u = Scale(p_igGetIO()->DisplaySize.y);
        HouseScalars(want, u);
        if (g_layout) {
            HouseSpacing(want, u);
        }
        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            bool transparent = IsBackground(i) && cur.colors[i].w < 0.5f;
            if (!transparent && !IsSemantic(i)) {
                want.colors[i] = g_house[i];
            }
        }
    }
    Write(*style, want);
}

void __cdecl HookNewFrame() {
    ApplyFrame();
    o_igNewFrame();
}

bool __cdecl HookBegin(const char* name, bool* open, ImGuiWindowFlags flags) {
    ApplyWindow(name, flags);
    return o_igBegin(name, open, flags);
}

// ---------------------------------------------------------------- fonte da casa

struct FontFile {
    std::vector<unsigned char> data;
    stbtt_fontinfo info;
    bool ok = false;
};
std::unordered_map<std::string, FontFile> g_fontFiles;
FontFile* g_houseFont = nullptr;

struct Replaced {
    ImFontAtlas* atlas;
    float ratio;
};
std::unordered_map<const ImFont*, Replaced> g_replaced;

FontFile* LoadFontFile(const char* path) {
    std::string key = Lower(path);
    auto found = g_fontFiles.find(key);
    if (found != g_fontFiles.end()) {
        return found->second.ok ? &found->second : nullptr;
    }
    FontFile& f = g_fontFiles[key];
    FILE* file = OpenPath(path);
    if (file) {
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        if (size > 0) {
            f.data.resize(static_cast<size_t>(size));
            f.ok = fread(f.data.data(), 1, f.data.size(), file) == f.data.size() &&
                   stbtt_InitFont(&f.info, f.data.data(), stbtt_GetFontOffsetForIndex(f.data.data(), 0)) != 0;
        }
        fclose(file);
    }
    return f.ok ? &f : nullptr;
}

// Largura media de texto de menu por pixel de altura (o ImGui escala a fonte por ascent - descent).
float WidthPerPixel(const FontFile& f, bool cyrillic) {
    static const char* latin = "Configuracoes do menu Ativar Desativar Salvar Fechar Volume 100 Tecla abc xyz 0123456789";
    static const wchar_t* cyr = L"\x041d\x0430\x0441\x0442\x0440\x043e\x0439\x043a\x0438 \x043c\x0435\x043d\x044e "
                                L"\x0412\x043a\x043b\x044e\x0447\x0438\x0442\x044c \x0421\x043e\x0445\x0440\x0430"
                                L"\x043d\x0438\x0442\x044c";
    int ascent = 0, descent = 0, gap = 0;
    stbtt_GetFontVMetrics(&f.info, &ascent, &descent, &gap);
    double sum = 0;
    int count = 0;
    for (const char* p = latin; *p; ++p) {
        int advance = 0, bearing = 0;
        stbtt_GetCodepointHMetrics(&f.info, static_cast<unsigned char>(*p), &advance, &bearing);
        sum += advance;
        ++count;
    }
    if (cyrillic) {
        for (const wchar_t* p = cyr; *p; ++p) {
            int advance = 0, bearing = 0;
            stbtt_GetCodepointHMetrics(&f.info, *p, &advance, &bearing);
            sum += advance;
            ++count;
        }
    }
    int height = ascent - descent;
    return height > 0 && count > 0 ? static_cast<float>(sum / count / height) : 0.0f;
}

bool HasCyrillic(const ImWchar* ranges) {
    for (const ImWchar* r = ranges; r && r[0]; r += 2) {
        if (r[0] <= 0x44F && r[1] >= 0x410) {
            return true;
        }
    }
    return false;
}

// Fontes de interface do Windows que a skin troca. Fontes proprias do mod (decorativas, de icones,
// da pasta do script) ficam.
bool Replaceable(const char* path) {
    static const char* names[] = {"trebuc.ttf",  "trebucbd.ttf", "arial.ttf",    "arialbd.ttf",  "tahoma.ttf",
                                  "tahomabd.ttf", "verdana.ttf",  "verdanab.ttf", "segoeui.ttf",  "segoeuib.ttf",
                                  "seguisb.ttf",  "calibri.ttf",  "calibrib.ttf"};
    const char* slash = strrchr(path, '\\');
    const char* slash2 = strrchr(path, '/');
    if (slash2 > slash) {
        slash = slash2;
    }
    std::string base = Lower(slash ? slash + 1 : path);
    for (const char* n : names) {
        if (base == n) {
            return true;
        }
    }
    return false;
}

// Fator de tamanho para a fonte da casa ocupar a mesma largura de texto que a fonte do script; 0 = a fonte fica
// a dela (nao deu para ler, ou a largura e diferente demais para trocar sem mexer no layout).
float HouseRatio(const char* path, bool cyrillic) {
    FontFile* source = LoadFontFile(path);
    if (!source || !g_houseFont) {
        return 0.0f;
    }
    float houseWidth = WidthPerPixel(*g_houseFont, cyrillic);
    float ratio = houseWidth > 0 ? WidthPerPixel(*source, cyrillic) / houseWidth : 0.0f;
    if (ratio < 0.8f || ratio > 1.25f) {
        Log("fonte %s ficou (largura muito diferente da fonte da casa: %.2f)", path, ratio);
        return 0.0f;
    }
    return ratio;
}

// Altura da tela do jogo, para a escala da casa quando as fontes carregam (antes do primeiro quadro do script):
// a janela do GTA; senao a maior janela visivel deste processo; senao 1080.
BOOL CALLBACK LargestWindow(HWND window, LPARAM param) {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    RECT r;
    if (pid == GetCurrentProcessId() && IsWindowVisible(window) && GetClientRect(window, &r)) {
        auto* best = reinterpret_cast<std::pair<HWND, LONG>*>(param);
        if (r.bottom * r.right > best->second) {
            *best = {window, r.bottom * r.right};
        }
    }
    return TRUE;
}

float ScreenHeight() {
    HWND window = FindWindowA("Grand theft auto San Andreas", nullptr);
    DWORD pid = 0;
    if (window) {
        GetWindowThreadProcessId(window, &pid);
    }
    if (!window || pid != GetCurrentProcessId()) {
        std::pair<HWND, LONG> best = {nullptr, 0};
        EnumWindows(LargestWindow, reinterpret_cast<LPARAM>(&best));
        window = best.first;
    }
    RECT r;
    return window && GetClientRect(window, &r) && r.bottom > 0 ? static_cast<float>(r.bottom) : 1080.0f;
}

// layout=1: a primeira fonte do atlas do script (a padrao do mimgui ou do imgui antigo, Trebuchet 14) vai para o
// tamanho de texto da casa (16 x escala da tela, como no kit); as outras fontes acompanham na mesma proporcao,
// para o titulo do mod continuar maior que o texto.
float LayoutFactor(Script& script, float size) {
    if (script.sizeFactor <= 0.0f) {
        script.sizeFactor = 16.0f * Scale(ScreenHeight()) / size;
        Log("layout: %s: primeira fonte %.1f px -> %.1f px (tamanho de texto da casa); as outras na mesma proporcao",
            script.name.c_str(), size, size * script.sizeFactor);
    }
    return script.sizeFactor;
}

// Como fica uma fonte que o script carrega: o tamanho novo (0 = fica como esta) e, em *face, se ela vira a fonte
// da casa. path nulo = fonte da memoria (so muda de tamanho, com layout=1). Normal: so as fontes de sistema mudam,
// para a fonte da casa com a mesma largura de texto. layout=1: tudo vai para o tamanho da casa.
float FontPlan(Script& script, const char* path, float size, bool cyrillic, bool merge, bool* face) {
    *face = false;
    if (script.untouched || size <= 0.0f) {
        return 0.0f;
    }
    float factor = g_layout ? LayoutFactor(script, size) : 1.0f;
    bool system = path && !merge && g_fontOn && g_houseFont && Replaceable(path);
    if (system && g_layout) {
        *face = LoadFontFile(path) != nullptr; // a fonte original entra de reserva: tem que abrir
    } else if (system) {
        float ratio = HouseRatio(path, cyrillic);
        *face = ratio != 0.0f;
        return *face ? size * ratio : 0.0f;
    }
    return *face || factor != 1.0f ? size * factor : 0.0f;
}

// O construtor do ImFontConfig mora no ImGui da DLL: sem config do script, a copia vem de um criado por ela.
void CopyConfig(const ImFontConfig* cfg, ImFontConfig* out) {
    if (cfg) {
        memcpy(static_cast<void*>(out), cfg, sizeof(ImFontConfig));
        return;
    }
    ImFontConfig* fresh = p_ImFontConfig_ImFontConfig();
    memcpy(static_cast<void*>(out), fresh, sizeof(ImFontConfig));
    p_ImFontConfig_destroy(fresh);
}

// Fonte do script so com outro tamanho (layout=1): texto com tamanho explicito acompanha (Compensated).
ImFont* Resized(ImFontAtlas* atlas, ImFont* font, float size, float newSize, const ImFontConfig* cfg) {
    if (font && !(cfg && cfg->MergeMode)) { // a mesclada devolve a fonte de destino, que ja tem o seu fator
        g_replaced[font] = Replaced{atlas, newSize / size};
    }
    return font;
}

ImFont* __cdecl HookAddFontFromFileTTF(ImFontAtlas* atlas, const char* filename, float size, const ImFontConfig* cfg,
                                       const ImWchar* ranges) {
    ImGuiContext* ctx = p_igGetCurrentContext();
    if (!ctx || !filename) {
        return o_AddFontFromFileTTF(atlas, filename, size, cfg, ranges);
    }
    Script& script = ScriptOf(ctx);
    bool face = false;
    float houseSize = FontPlan(script, filename, size, HasCyrillic(ranges ? ranges : (cfg ? cfg->GlyphRanges : nullptr)),
                               cfg && cfg->MergeMode, &face);
    if (houseSize == 0.0f) {
        return o_AddFontFromFileTTF(atlas, filename, size, cfg, ranges);
    }
    if (!face) {
        return Resized(atlas, o_AddFontFromFileTTF(atlas, filename, houseSize, cfg, ranges), size, houseSize, cfg);
    }
    const std::string& owner = script.name;
    float ratio = houseSize / size;
    alignas(ImFontConfig) unsigned char storage[sizeof(ImFontConfig)];
    ImFontConfig& base = *reinterpret_cast<ImFontConfig*>(storage);
    CopyConfig(cfg, &base);
    ImFont* font = o_AddFontFromFileTTF(atlas, g_housePath, houseSize, &base, ranges);
    if (!font) {
        Log("fonte da casa nao carregou; %s ficou", filename);
        return o_AddFontFromFileTTF(atlas, filename, size, cfg, ranges);
    }
    // Glifos que a fonte da casa nao tem vem da fonte original (o ImGui nao sobrescreve os que ja existem).
    ImFontConfig fallback = base;
    fallback.MergeMode = true;
    fallback.DstFont = nullptr;
    o_AddFontFromFileTTF(atlas, filename, houseSize, &fallback, ranges);
    g_replaced[font] = Replaced{atlas, ratio};
    Log("fonte %s %.1f px de %s -> fonte da casa %.1f px (%s)", filename, size, owner.c_str(), houseSize,
        g_layout ? "tamanho da casa" : "mesma largura de texto");
    return font;
}

// Fontes da memoria (icones embutidos no script etc.): so mudam de tamanho, e so com layout=1.
float MemoryFontSize(float size, const ImFontConfig* cfg) {
    ImGuiContext* ctx = p_igGetCurrentContext();
    if (!g_layout || !ctx) {
        return size;
    }
    bool face = false;
    float newSize = FontPlan(ScriptOf(ctx), nullptr, size, false, cfg && cfg->MergeMode, &face);
    return newSize == 0.0f ? size : newSize;
}

ImFont* __cdecl HookAddFontFromMemoryTTF(ImFontAtlas* atlas, void* data, int bytes, float size, const ImFontConfig* cfg,
                                         const ImWchar* ranges) {
    float newSize = MemoryFontSize(size, cfg);
    ImFont* font = o_AddFontFromMemoryTTF(atlas, data, bytes, newSize, cfg, ranges);
    return newSize == size ? font : Resized(atlas, font, size, newSize, cfg);
}

ImFont* __cdecl HookAddFontFromMemoryCompressedTTF(ImFontAtlas* atlas, void* data, int bytes, float size,
                                                   const ImFontConfig* cfg, const ImWchar* ranges) {
    float newSize = MemoryFontSize(size, cfg);
    ImFont* font = o_AddFontFromMemoryCompressedTTF(atlas, data, bytes, newSize, cfg, ranges);
    return newSize == size ? font : Resized(atlas, font, size, newSize, cfg);
}

ImFont* __cdecl HookAddFontFromBase85(ImFontAtlas* atlas, const char* data, float size, const ImFontConfig* cfg,
                                      const ImWchar* ranges) {
    float newSize = MemoryFontSize(size, cfg);
    ImFont* font = o_AddFontFromBase85(atlas, data, newSize, cfg, ranges);
    return newSize == size ? font : Resized(atlas, font, size, newSize, cfg);
}

void ForgetAtlas(ImFontAtlas* atlas) {
    for (auto it = g_replaced.begin(); it != g_replaced.end();) {
        it = it->second.atlas == atlas ? g_replaced.erase(it) : std::next(it);
    }
}

// O script limpou o atlas: a proxima fonte dele e a nova referencia de tamanho (layout=1).
void ResetLayoutFactor(ImFontAtlas* atlas) {
    ImGuiContext* ctx = p_igGetCurrentContext();
    auto found = ctx ? g_scripts.find(ctx) : g_scripts.end();
    if (found != g_scripts.end() && p_igGetIO()->Fonts == atlas) {
        found->second.sizeFactor = 0.0f;
    }
}

void __cdecl HookAtlasClear(ImFontAtlas* atlas) {
    ForgetAtlas(atlas);
    ResetLayoutFactor(atlas);
    o_AtlasClear(atlas);
}

void __cdecl HookAtlasClearFonts(ImFontAtlas* atlas) {
    ForgetAtlas(atlas);
    ResetLayoutFactor(atlas);
    o_AtlasClearFonts(atlas);
}

void __cdecl HookAtlasDestroy(ImFontAtlas* atlas) {
    ForgetAtlas(atlas);
    o_AtlasDestroy(atlas);
}

void __cdecl HookDestroyContext(ImGuiContext* ctx) {
    ImGuiContext* target = ctx ? ctx : p_igGetCurrentContext();
    auto found = g_states.find(target);
    if (found != g_states.end()) {
        ForgetAtlas(found->second.atlas);
        g_states.erase(found);
    }
    g_scripts.erase(target);
    o_igDestroyContext(ctx);
}

// Texto com tamanho explicito numa fonte trocada: o script pensou no tamanho da fonte antiga; o mesmo
// fator mantem a largura. Se ele ja passou o tamanho da fonte nova, nada muda.
float Compensated(const ImFont* font, float size) {
    if (!font || g_replaced.empty()) {
        return size;
    }
    auto found = g_replaced.find(font);
    if (found == g_replaced.end() || std::fabs(size - font->FontSize) < 0.01f) {
        return size;
    }
    return size * found->second.ratio;
}

void __cdecl HookAddTextFontPtr(ImDrawList* list, const ImFont* font, float size, const ImVec2 pos, ImU32 col,
                                const char* begin, const char* end, float wrap, const ImVec4* clip) {
    o_AddTextFontPtr(list, font, Compensated(font, size), pos, col, begin, end, wrap, clip);
}

void __cdecl HookCalcTextSizeA(ImVec2* out, ImFont* font, float size, float maxWidth, float wrapWidth,
                               const char* begin, const char* end, const char** remaining) {
    o_CalcTextSizeA(out, font, Compensated(font, size), maxWidth, wrapWidth, begin, end, remaining);
}

// ---------------------------------------------------------------- imgui antigo do moonloader (moon_imgui)

// O moon_imgui (lib\imgui.lua + lib\MoonImGui.dll, Dear ImGui 1.52) e um modulo C do Lua: cada script que faz
// require 'imgui' chama o luaopen_MoonImGui do DLL no seu proprio lua_State. A skin desvia essa funcao e, com a
// tabela do modulo pronta, roda nesse lua_State o moon_patch.lua (embutido aqui), que padroniza pela API Lua do
// proprio moon_imgui. Daqui o patch so recebe as regras (paleta, cantos, janelas mantidas, fonte) e o log.

struct lua_State;
typedef int(__cdecl* lua_CFunction)(lua_State*);
constexpr int LUA_TTABLE = 5;

// lua51.dll do moonloader (LuaJIT): so a API C publica do Lua 5.1, resolvida quando o MoonImGui.dll carrega.
struct LuaApi {
    int(__cdecl* loadbuffer)(lua_State*, const char*, size_t, const char*);
    int(__cdecl* pcall)(lua_State*, int, int, int);
    int(__cdecl* gettop)(lua_State*);
    void(__cdecl* settop)(lua_State*, int);
    void(__cdecl* pushvalue)(lua_State*, int);
    int(__cdecl* type)(lua_State*, int);
    const char*(__cdecl* tolstring)(lua_State*, int, size_t*);
    double(__cdecl* tonumber)(lua_State*, int);
    int(__cdecl* toboolean)(lua_State*, int);
    void(__cdecl* pushnumber)(lua_State*, double);
    void(__cdecl* pushboolean)(lua_State*, int);
    void(__cdecl* pushnil)(lua_State*);
    void(__cdecl* pushstring)(lua_State*, const char*);
    void(__cdecl* pushcclosure)(lua_State*, lua_CFunction, int);
    void(__cdecl* createtable)(lua_State*, int, int);
    void(__cdecl* setfield)(lua_State*, int, const char*);
    void(__cdecl* rawseti)(lua_State*, int, int);
};
LuaApi g_lua = {};

const char kMoonPatch[] =
#include "moon_patch.inc" // src/moon_patch.lua, embrulhado pelo build.sh
    ;

std::unordered_map<std::string, Script> g_moonScripts; // por arquivo do script (sobrevive ao Ctrl+R)

const char* Arg(lua_State* L, int index) {
    const char* s = g_lua.tolstring(L, index, nullptr);
    return s ? s : "";
}

int __cdecl LuaLog(lua_State* L) {
    Log("%s", Arg(L, 1));
    return 0;
}

// skin.script(arquivo, caminho): registra o script; true = padronizar, false = fica como esta.
int __cdecl LuaScript(lua_State* L) {
    std::string name = Arg(L, 1);
    if (name.empty()) {
        name = "?";
    }
    Script& script = g_moonScripts[name];
    script.name = name;
    const char* reason = UntouchedReason(name, Arg(L, 2));
    script.untouched = reason != nullptr;
    Log("script do imgui antigo: %s (%s)", name.c_str(), reason ? reason : "padronizado");
    g_lua.pushboolean(L, !script.untouched);
    return 1;
}

int __cdecl LuaTheme(lua_State* L) {
    g_lua.pushboolean(L, g_theme);
    return 1;
}

int __cdecl LuaKept(lua_State* L) {
    g_lua.pushboolean(L, Kept(Arg(L, 1)));
    return 1;
}

// skin.note(arquivo, titulo, decisao): o que a skin fez com a janela (log e resumo do /trokskin).
int __cdecl LuaNote(lua_State* L) {
    auto found = g_moonScripts.find(Arg(L, 1));
    if (found != g_moonScripts.end()) {
        NoteWindow(found->second, Arg(L, 2), Arg(L, 3));
    }
    return 0;
}

// skin.scalars(altura da tela): cantos e alinhamento do titulo da casa (WindowRounding, ChildWindowRounding,
// FrameRounding, ScrollbarRounding, GrabRounding, WindowTitleAlign.x, .y).
int __cdecl LuaScalars(lua_State* L) {
    Look look;
    HouseScalars(look, Scale(static_cast<float>(g_lua.tonumber(L, 1))));
    const float values[] = {look.windowRounding, look.childRounding,      look.frameRounding,     look.scrollbarRounding,
                            look.grabRounding,   look.windowTitleAlign.x, look.windowTitleAlign.y};
    for (float v : values) {
        g_lua.pushnumber(L, v);
    }
    return 7;
}

// Cores do ImGui 1.52 na ordem do imgui.Col do moon_imgui (1 = Text) -> cor da casa (indice do 1.72). Negativo:
// botao de fechar do 1.52, um circulo sempre visivel (o X aparece com o mouse em cima): branco a 8, 16 e 24%.
const int kMoonColors[43] = {
    ImGuiCol_Text,         ImGuiCol_TextDisabled,       ImGuiCol_WindowBg,           ImGuiCol_ChildBg,
    ImGuiCol_PopupBg,      ImGuiCol_Border,             ImGuiCol_BorderShadow,       ImGuiCol_FrameBg,
    ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive,    ImGuiCol_TitleBg,            ImGuiCol_TitleBgActive,
    ImGuiCol_TitleBgCollapsed, ImGuiCol_MenuBarBg,      ImGuiCol_ScrollbarBg,        ImGuiCol_ScrollbarGrab,
    ImGuiCol_ScrollbarGrabHovered, ImGuiCol_ScrollbarGrabActive, ImGuiCol_PopupBg /* ComboBg */, ImGuiCol_CheckMark,
    ImGuiCol_SliderGrab,   ImGuiCol_SliderGrabActive,   ImGuiCol_Button,             ImGuiCol_ButtonHovered,
    ImGuiCol_ButtonActive, ImGuiCol_Header,             ImGuiCol_HeaderHovered,      ImGuiCol_HeaderActive,
    ImGuiCol_Separator,    ImGuiCol_SeparatorHovered,   ImGuiCol_SeparatorActive,    ImGuiCol_ResizeGrip,
    ImGuiCol_ResizeGripHovered, ImGuiCol_ResizeGripActive, -1 /* CloseButton */,      -2 /* CloseButtonHovered */,
    -3 /* CloseButtonActive */, ImGuiCol_PlotLines,     ImGuiCol_PlotLinesHovered,   ImGuiCol_PlotHistogram,
    ImGuiCol_PlotHistogramHovered, ImGuiCol_TextSelectedBg, ImGuiCol_ModalWindowDimBg /* ModalWindowDarkening */};
// Fundos (WindowBg, ChildWindowBg, PopupBg, FrameBg x3, TitleBg x3, MenuBarBg, ComboBg) e cores com significado
// (Text, TextDisabled, PlotLines x2, PlotHistogram x2), nos indices do imgui.Col.
const int kMoonBackground[] = {3, 4, 5, 8, 9, 10, 11, 12, 13, 14, 19};
const int kMoonSemantic[] = {1, 2, 38, 39, 40, 41};

// skin.palette(): {cores = {{r, g, b, a} x 43}, fundo = {[i] = true}, significado = {[i] = true}}.
int __cdecl LuaPalette(lua_State* L) {
    g_lua.createtable(L, 0, 3);
    g_lua.createtable(L, 43, 0);
    for (int i = 0; i < 43; ++i) {
        ImVec4 c = kMoonColors[i] >= 0 ? g_house[kMoonColors[i]] : White(0.08f * static_cast<float>(-kMoonColors[i]));
        g_lua.createtable(L, 4, 0);
        const float parts[] = {c.x, c.y, c.z, c.w};
        for (int k = 0; k < 4; ++k) {
            g_lua.pushnumber(L, parts[k]);
            g_lua.rawseti(L, -2, k + 1);
        }
        g_lua.rawseti(L, -2, i + 1);
    }
    g_lua.setfield(L, -2, "cores");
    g_lua.createtable(L, 0, 0);
    for (int i : kMoonBackground) {
        g_lua.pushboolean(L, 1);
        g_lua.rawseti(L, -2, i);
    }
    g_lua.setfield(L, -2, "fundo");
    g_lua.createtable(L, 0, 0);
    for (int i : kMoonSemantic) {
        g_lua.pushboolean(L, 1);
        g_lua.rawseti(L, -2, i);
    }
    g_lua.setfield(L, -2, "significado");
    return 1;
}

// skin.fonts(): a skin mexe nas fontes do script (troca pela da casa ou, com layout=1, muda o tamanho)?
int __cdecl LuaFonts(lua_State* L) {
    g_lua.pushboolean(L, (g_fontOn && g_houseFont != nullptr) || g_layout);
    return 1;
}

// skin.font(arquivo do script, caminho ou nil, tamanho, cirilico, mesclada): o tamanho novo e se vira a fonte da
// casa (a mesma regra do mimgui, FontPlan), ou nil se a fonte fica como esta.
int __cdecl LuaFont(lua_State* L) {
    auto found = g_moonScripts.find(Arg(L, 1));
    const int LUA_TSTRING = 4;
    const char* path = g_lua.type(L, 2) == LUA_TSTRING ? Arg(L, 2) : nullptr;
    bool face = false;
    float size = found == g_moonScripts.end()
                     ? 0.0f
                     : FontPlan(found->second, path, static_cast<float>(g_lua.tonumber(L, 3)),
                                g_lua.toboolean(L, 4) != 0, g_lua.toboolean(L, 5) != 0, &face);
    if (size == 0.0f) {
        g_lua.pushnil(L);
        return 1;
    }
    g_lua.pushnumber(L, size);
    g_lua.pushboolean(L, face);
    return 2;
}

// skin.resetFonts(arquivo): o script limpou o atlas; a proxima fonte e a nova referencia de tamanho (layout=1).
int __cdecl LuaResetFonts(lua_State* L) {
    auto found = g_moonScripts.find(Arg(L, 1));
    if (found != g_moonScripts.end()) {
        found->second.sizeFactor = 0.0f;
    }
    return 0;
}

// skin.spacing(altura da tela): espacamentos da casa com layout=1 (IndentSpacing, ScrollbarSize, GrabMinSize,
// WindowPadding.x/y, FramePadding.x/y, ItemSpacing.x/y, ItemInnerSpacing.x/y), ou nil com o layout desligado.
int __cdecl LuaSpacing(lua_State* L) {
    if (!g_layout) {
        g_lua.pushnil(L);
        return 1;
    }
    Look look;
    HouseSpacing(look, Scale(static_cast<float>(g_lua.tonumber(L, 1))));
    const float values[] = {look.indentSpacing,      look.scrollbarSize,    look.grabMinSize,
                            look.windowPadding.x,    look.windowPadding.y,  look.framePadding.x,
                            look.framePadding.y,     look.itemSpacing.x,    look.itemSpacing.y,
                            look.itemInnerSpacing.x, look.itemInnerSpacing.y};
    for (float v : values) {
        g_lua.pushnumber(L, v);
    }
    return 11;
}

int __cdecl LuaHousePath(lua_State* L) {
    g_lua.pushstring(L, g_housePath);
    return 1;
}

void PushSkinTable(lua_State* L) {
    static const struct {
        const char* name;
        lua_CFunction fn;
    } functions[] = {{"log", LuaLog},         {"script", LuaScript},     {"theme", LuaTheme},
                     {"kept", LuaKept},       {"note", LuaNote},         {"scalars", LuaScalars},
                     {"spacing", LuaSpacing}, {"palette", LuaPalette},   {"fonts", LuaFonts},
                     {"font", LuaFont},       {"resetFonts", LuaResetFonts}, {"housePath", LuaHousePath}};
    g_lua.createtable(L, 0, static_cast<int>(sizeof(functions) / sizeof(functions[0])));
    for (const auto& f : functions) {
        g_lua.pushcclosure(L, f.fn, 0);
        g_lua.setfield(L, -2, f.name);
    }
}

typedef int(__cdecl* LuaopenFn)(lua_State*);
LuaopenFn o_luaopenMoonImGui = nullptr;

// require 'MoonImGui' (de dentro do imgui.lua) em cada script: o DLL monta a tabela do modulo; a skin roda o
// moon_patch.lua com ela antes de devolver. Erro no patch: o script fica como esta (e o log diz o motivo).
int __cdecl HookLuaopenMoonImGui(lua_State* L) {
    int results = o_luaopenMoonImGui(L);
    int top = g_lua.gettop(L);
    int module = top - results + 1;
    if (results < 1 || g_lua.type(L, module) != LUA_TTABLE) {
        Log("imgui antigo: o MoonImGui.dll nao devolveu a tabela do modulo -- script fica como esta");
        return results;
    }
    if (g_lua.loadbuffer(L, kMoonPatch, sizeof(kMoonPatch) - 1, "=Trok Skin") != 0) {
        Log("imgui antigo: o ajuste da skin nao carregou: %s", Arg(L, -1));
    } else {
        g_lua.pushvalue(L, module);
        PushSkinTable(L);
        if (g_lua.pcall(L, 2, 0, 0) != 0) {
            Log("imgui antigo: erro ao ajustar um script (fica como esta): %s", Arg(L, -1));
        }
    }
    g_lua.settop(L, top);
    return results;
}

// ---------------------------------------------------------------- instalacao

CRITICAL_SECTION g_installLock;
volatile bool g_installed = false;
volatile bool g_gaveUp = false;

template <typename T> bool Resolve(HMODULE dll, const char* name, T& out) {
    out = reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(dll, name)));
    if (!out) {
        Log("cimguidx9.dll sem %s -- skin desligada", name);
    }
    return out != nullptr;
}

template <typename T> bool Hook(HMODULE dll, const char* name, void* detour, T& original) {
    void* target = reinterpret_cast<void*>(GetProcAddress(dll, name));
    if (!target) {
        Log("cimguidx9.dll sem %s -- skin desligada", name);
        return false;
    }
    MH_STATUS status = MH_CreateHook(target, detour, reinterpret_cast<void**>(&original));
    if (status != MH_OK) {
        Log("nao deu para desviar %s (%d) -- skin desligada", name, status);
        return false;
    }
    return true;
}

void Install(HMODULE dll) {
    EnterCriticalSection(&g_installLock);
    if (g_installed || g_gaveUp) {
        LeaveCriticalSection(&g_installLock);
        return;
    }
    g_gaveUp = true; // so uma tentativa por processo
    bool ok = Resolve(dll, "igGetVersion", p_igGetVersion);
    if (ok) {
        const char* version = p_igGetVersion();
        if (!version || strcmp(version, IMGUI_VERSION) != 0) {
            Log("mimgui com Dear ImGui %s; a skin foi feita para o %s -- desligada para nao arriscar",
                version ? version : "?", IMGUI_VERSION);
            ok = false;
        }
    }
    ok = ok && Resolve(dll, "igGetCurrentContext", p_igGetCurrentContext) && Resolve(dll, "igGetStyle", p_igGetStyle) &&
         Resolve(dll, "igGetIO", p_igGetIO) && Resolve(dll, "ImFontConfig_ImFontConfig", p_ImFontConfig_ImFontConfig) &&
         Resolve(dll, "ImFontConfig_destroy", p_ImFontConfig_destroy);
    std::vector<void*> targets;
    if (ok) {
        // A DLL fica carregada ate o jogo fechar: se ela descarregasse (Ctrl+R) os desvios ficariam soltos.
        HMODULE pinned = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"cimguidx9.dll", &pinned);
        ok = Hook(dll, "igNewFrame", reinterpret_cast<void*>(HookNewFrame), o_igNewFrame) &&
             Hook(dll, "igBegin", reinterpret_cast<void*>(HookBegin), o_igBegin) &&
             Hook(dll, "igDestroyContext", reinterpret_cast<void*>(HookDestroyContext), o_igDestroyContext) &&
             Hook(dll, "ImFontAtlas_AddFontFromFileTTF", reinterpret_cast<void*>(HookAddFontFromFileTTF),
                  o_AddFontFromFileTTF) &&
             Hook(dll, "ImFontAtlas_AddFontFromMemoryTTF", reinterpret_cast<void*>(HookAddFontFromMemoryTTF),
                  o_AddFontFromMemoryTTF) &&
             Hook(dll, "ImFontAtlas_AddFontFromMemoryCompressedTTF",
                  reinterpret_cast<void*>(HookAddFontFromMemoryCompressedTTF), o_AddFontFromMemoryCompressedTTF) &&
             Hook(dll, "ImFontAtlas_AddFontFromMemoryCompressedBase85TTF", reinterpret_cast<void*>(HookAddFontFromBase85),
                  o_AddFontFromBase85) &&
             Hook(dll, "ImFontAtlas_Clear", reinterpret_cast<void*>(HookAtlasClear), o_AtlasClear) &&
             Hook(dll, "ImFontAtlas_ClearFonts", reinterpret_cast<void*>(HookAtlasClearFonts), o_AtlasClearFonts) &&
             Hook(dll, "ImFontAtlas_destroy", reinterpret_cast<void*>(HookAtlasDestroy), o_AtlasDestroy) &&
             Hook(dll, "ImDrawList_AddTextFontPtr", reinterpret_cast<void*>(HookAddTextFontPtr), o_AddTextFontPtr) &&
             Hook(dll, "ImFont_CalcTextSizeA_nonUDT", reinterpret_cast<void*>(HookCalcTextSizeA), o_CalcTextSizeA);
        static const char* hooked[] = {"igNewFrame",
                                       "igBegin",
                                       "igDestroyContext",
                                       "ImFontAtlas_AddFontFromFileTTF",
                                       "ImFontAtlas_AddFontFromMemoryTTF",
                                       "ImFontAtlas_AddFontFromMemoryCompressedTTF",
                                       "ImFontAtlas_AddFontFromMemoryCompressedBase85TTF",
                                       "ImFontAtlas_Clear",
                                       "ImFontAtlas_ClearFonts",
                                       "ImFontAtlas_destroy",
                                       "ImDrawList_AddTextFontPtr",
                                       "ImFont_CalcTextSizeA_nonUDT"};
        for (const char* name : hooked) {
            if (void* t = reinterpret_cast<void*>(GetProcAddress(dll, name))) {
                targets.push_back(t);
            }
        }
    }
    if (ok) {
        for (void* t : targets) {
            if (MH_EnableHook(t) != MH_OK) {
                ok = false;
            }
        }
    }
    if (!ok) {
        for (void* t : targets) {
            MH_RemoveHook(t);
        }
    } else {
        g_installed = true;
        Log("skin ligada no mimgui (Dear ImGui %s)%s", IMGUI_VERSION,
            g_houseFont ? "" : " -- sem a fonte da casa, so o tema");
    }
    LeaveCriticalSection(&g_installLock);
}

volatile bool g_moonInstalled = false;
volatile bool g_moonGaveUp = false;

template <typename T> bool ResolveLua(HMODULE dll, const char* name, T& out) {
    out = reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(dll, name)));
    if (!out) {
        Log("lua51.dll sem %s -- imgui antigo fica como esta", name);
    }
    return out != nullptr;
}

void InstallMoon(HMODULE dll) {
    EnterCriticalSection(&g_installLock);
    if (g_moonInstalled || g_moonGaveUp) {
        LeaveCriticalSection(&g_installLock);
        return;
    }
    g_moonGaveUp = true; // so uma tentativa por processo
    // O MoonImGui.dll importa o lua51.dll do moonloader: ja esta carregado.
    HMODULE lua = GetModuleHandleW(L"lua51.dll");
    bool ok = lua != nullptr;
    if (!ok) {
        Log("imgui antigo: lua51.dll nao esta carregado -- fica como esta");
    }
    ok = ok && ResolveLua(lua, "luaL_loadbuffer", g_lua.loadbuffer) && ResolveLua(lua, "lua_pcall", g_lua.pcall) &&
         ResolveLua(lua, "lua_gettop", g_lua.gettop) && ResolveLua(lua, "lua_settop", g_lua.settop) &&
         ResolveLua(lua, "lua_pushvalue", g_lua.pushvalue) && ResolveLua(lua, "lua_type", g_lua.type) &&
         ResolveLua(lua, "lua_tolstring", g_lua.tolstring) && ResolveLua(lua, "lua_tonumber", g_lua.tonumber) &&
         ResolveLua(lua, "lua_toboolean", g_lua.toboolean) && ResolveLua(lua, "lua_pushnumber", g_lua.pushnumber) &&
         ResolveLua(lua, "lua_pushboolean", g_lua.pushboolean) && ResolveLua(lua, "lua_pushnil", g_lua.pushnil) &&
         ResolveLua(lua, "lua_pushstring", g_lua.pushstring) && ResolveLua(lua, "lua_pushcclosure", g_lua.pushcclosure) &&
         ResolveLua(lua, "lua_createtable", g_lua.createtable) && ResolveLua(lua, "lua_setfield", g_lua.setfield) &&
         ResolveLua(lua, "lua_rawseti", g_lua.rawseti);
    void* target = ok ? reinterpret_cast<void*>(GetProcAddress(dll, "luaopen_MoonImGui")) : nullptr;
    if (ok && !target) {
        Log("MoonImGui.dll sem luaopen_MoonImGui -- imgui antigo fica como esta");
        ok = false;
    }
    if (ok) {
        // O DLL fica carregado ate o jogo fechar: se ele descarregasse (Ctrl+R) o desvio ficaria solto.
        HMODULE pinned = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           static_cast<LPCWSTR>(target), &pinned);
        MH_STATUS status = MH_CreateHook(target, reinterpret_cast<void*>(HookLuaopenMoonImGui),
                                         reinterpret_cast<void**>(&o_luaopenMoonImGui));
        if (status == MH_OK) {
            status = MH_EnableHook(target);
            if (status != MH_OK) {
                MH_RemoveHook(target);
            }
        }
        if (status != MH_OK) {
            Log("nao deu para desviar luaopen_MoonImGui (%d) -- imgui antigo fica como esta", status);
            ok = false;
        }
    }
    if (ok) {
        g_moonInstalled = true;
        Log("skin ligada no imgui antigo do moonloader (MoonImGui.dll)%s",
            g_houseFont ? "" : " -- sem a fonte da casa, so o tema");
    }
    LeaveCriticalSection(&g_installLock);
}

bool ModuleIs(HMODULE module, const wchar_t* file) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(module, path, MAX_PATH);
    if (!n || n >= MAX_PATH) {
        return false;
    }
    const wchar_t* slash = wcsrchr(path, L'\\');
    return _wcsicmp(slash ? slash + 1 : path, file) == 0;
}

bool Pending() {
    return (!g_installed && !g_gaveUp) || (!g_moonInstalled && !g_moonGaveUp);
}

// Se o mimgui ou o imgui antigo ja estao carregados (o .asi entrou tarde), instala agora.
void InstallLoaded() {
    if (!g_installed && !g_gaveUp) {
        if (HMODULE dll = GetModuleHandleW(L"cimguidx9.dll")) {
            Install(dll);
        }
    }
    if (!g_moonInstalled && !g_moonGaveUp) {
        if (HMODULE dll = GetModuleHandleW(L"MoonImGui.dll")) {
            InstallMoon(dll);
        }
    }
}

// O LuaJIT carrega a DLL do mimgui (ffi.load) e o MoonImGui.dll (require) com LoadLibraryExA, que termina aqui:
// os desvios entram antes de qualquer script usar a DLL.
typedef HMODULE(WINAPI* LoadLibraryExWFn)(LPCWSTR, HANDLE, DWORD);
LoadLibraryExWFn o_LoadLibraryExW = nullptr;

HMODULE WINAPI HookLoadLibraryExW(LPCWSTR name, HANDLE file, DWORD flags) {
    HMODULE module = o_LoadLibraryExW(name, file, flags);
    const DWORD dataOnly = LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE;
    if (module && !(flags & dataOnly) && Pending()) {
        if (!g_installed && !g_gaveUp && ModuleIs(module, L"cimguidx9.dll")) {
            Install(module);
        } else if (!g_moonInstalled && !g_moonGaveUp && ModuleIs(module, L"MoonImGui.dll")) {
            InstallMoon(module);
        }
    }
    return module;
}

// ---------------------------------------------------------------- SA-MP 0.3.7 R1: /trokskin

constexpr DWORD SAMP_R1_ENTRY = 0x31DF13;
constexpr DWORD SAMP_INFO = 0x21A0F8;
constexpr DWORD SAMP_CHAT = 0x21A0E4;
constexpr DWORD SAMP_INPUT = 0x21A0E8;
constexpr DWORD SAMP_CHAT_ADD_ENTRY = 0x64010;
constexpr DWORD SAMP_ADD_COMMAND = 0x65AD0;
typedef void(__cdecl* CmdProc)(const char*);
typedef void(__thiscall* AddCommandFn)(void*, const char*, CmdProc);
typedef void(__thiscall* AddEntryFn)(void*, int, const char*, const char*, DWORD, DWORD);
DWORD g_samp = 0;

void Chat(const char* text) {
    void* chat = g_samp ? *reinterpret_cast<void**>(g_samp + SAMP_CHAT) : nullptr;
    if (chat) {
        reinterpret_cast<AddEntryFn>(g_samp + SAMP_CHAT_ADD_ENTRY)(chat, 8, text, nullptr, 0xFFFFFFFF, 0);
    }
}

void __cdecl CmdTrokSkin(const char*) {
    if (!g_installed && !g_moonInstalled) {
        Chat("{FF8A8A}[Trok Skin]{FFFFFF} nenhum menu com mimgui ou com o imgui antigo carregou ainda (ou a versao "
             "nao e a suportada). Veja o {FFD27A}Trok Skin.log{FFFFFF}.");
        return;
    }
    g_theme = !g_theme;
    Log("/trokskin: tema %s", g_theme ? "ligado" : "desligado");
    // Resumo: o que cada script tem na tela e o que a skin faz com cada janela.
    int themed = 0;
    auto summarize = [&themed](const Script& script) {
        if (script.untouched) {
            Log("  %s: fica como esta (da casa ou manter_scripts)", script.name.c_str());
            return;
        }
        if (script.windows.empty()) {
            Log("  %s: nenhuma janela desenhada ate agora", script.name.c_str());
        }
        for (const auto& w : script.windows) {
            Log("  %s: janela \"%s\" -> %s", script.name.c_str(), w.first.c_str(), w.second.c_str());
            themed += w.second == "padronizada" || w.second == "tema desligado";
        }
    };
    for (auto& entry : g_scripts) {
        summarize(entry.second);
    }
    for (auto& entry : g_moonScripts) {
        summarize(entry.second);
    }
    Chat(g_theme ? "{FFFFFF}[Trok Skin] visual da casa {8AFF8A}ligado{FFFFFF}."
                 : "{FFFFFF}[Trok Skin] visual da casa {FF8A8A}desligado{FFFFFF} (a fonte volta ao recarregar os "
                   "scripts).");
    if (themed == 0) {
        Chat("{FFFFFF}[Trok Skin] nenhum menu de outro mod apareceu ainda. Abra o menu do mod e use "
             "{FFD27A}/trokskin{FFFFFF} de novo.");
    }
}

void RegisterCommand() {
    for (int i = 0; i < 600 && !g_samp; ++i) {
        g_samp = reinterpret_cast<DWORD>(GetModuleHandleA("samp.dll"));
        if (!g_samp) {
            Sleep(100);
        }
    }
    if (!g_samp) {
        return;
    }
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(g_samp);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(g_samp + dos->e_lfanew);
    if (nt->OptionalHeader.AddressOfEntryPoint != SAMP_R1_ENTRY) {
        Log("samp.dll nao e o 0.3.7 R1 -- sem /trokskin (o tema segue pelo Trok Skin.ini)");
        g_samp = 0;
        return;
    }
    for (int i = 0; i < 1200; ++i) {
        void* input = *reinterpret_cast<void**>(g_samp + SAMP_INPUT);
        void* info = *reinterpret_cast<void**>(g_samp + SAMP_INFO);
        if (input && info) {
            reinterpret_cast<AddCommandFn>(g_samp + SAMP_ADD_COMMAND)(input, "trokskin", CmdTrokSkin);
            Log("/trokskin registrado");
            return;
        }
        Sleep(100);
    }
}

void Start();

DWORD WINAPI Boot(LPVOID) {
    Start();
    // Se o mimgui ou o imgui antigo ja estavam carregados (o .asi entrou tarde), instala agora; senao o desvio do
    // LoadLibrary pega.
    for (int i = 0; i < 20 && Pending(); ++i) {
        InstallLoaded();
        Sleep(250);
    }
    RegisterCommand();
    // Reserva: confere de vez em quando se as DLLs apareceram por outro caminho.
    while (Pending()) {
        InstallLoaded();
        Sleep(1000);
    }
    return 0;
}

// Roda na thread de partida, fora do DllMain (nada de desvio com o loader travado). O moonloader so carrega
// os scripts bem depois, quando o jogo ja esta de pe.
void Start() {
    HouseColors(g_house);
    LoadConfig();

    wchar_t dir[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, dir, MAX_PATH); // gta_sa.exe
    if (wchar_t* slash = wcsrchr(dir, L'\\')) {
        slash[1] = 0;
    }
    wchar_t house[MAX_PATH + 64];
    _snwprintf(house, MAX_PATH + 64, L"%lsmoonloader\\resource\\trok\\font.ttf", dir);
    WideCharToMultiByte(CP_UTF8, 0, house, -1, g_housePath, sizeof(g_housePath), nullptr, nullptr);
    g_houseFont = LoadFontFile(g_housePath);
    Log("fonte da casa: %s (%s)", g_housePath, g_houseFont ? "ok" : "nao encontrada, so o tema");

    if (MH_Initialize() != MH_OK) {
        Log("MinHook nao iniciou -- skin desligada");
        return;
    }
    HMODULE kernel = GetModuleHandleA("kernelbase.dll");
    void* target = kernel ? reinterpret_cast<void*>(GetProcAddress(kernel, "LoadLibraryExW")) : nullptr;
    if (!target) {
        target = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryExW"));
    }
    if (target && MH_CreateHook(target, reinterpret_cast<void*>(HookLoadLibraryExW),
                                reinterpret_cast<void**>(&o_LoadLibraryExW)) == MH_OK &&
        MH_EnableHook(target) == MH_OK) {
        Log("esperando o mimgui ou o imgui antigo carregar");
    } else {
        Log("sem desvio no LoadLibrary -- vou procurar o mimgui e o imgui antigo de tempos em tempos");
    }
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        GetModuleFileNameA(module, g_logPath, MAX_PATH);
        char* slash = strrchr(g_logPath, '\\');
        if (slash) {
            strcpy(g_iniPath, g_logPath);
            strcpy(g_iniPath + (slash - g_logPath) + 1, "Trok Skin.ini");
            strcpy(slash + 1, "Trok Skin.log");
        } else {
            g_logPath[0] = 0;
        }
        char file[MAX_PATH] = {};
        GetModuleFileNameA(module, file, MAX_PATH);
        const char* base = strrchr(file, '\\') ? strrchr(file, '\\') + 1 : file;
        Log("Trok Skin .asi v1.3.0 (%s)", base);
        // Uma copia so por jogo: com o Trok Skin.asi e o Trok Skin Layout.asi juntos na pasta, a que carregar
        // depois fica desligada (as duas desviariam as mesmas funcoes).
        CreateMutexA(nullptr, FALSE, "TrokSkin.UmaCopia");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            Log("outra copia do Trok Skin ja esta ligada neste jogo -- esta (%s) fica desligada; deixe so um .asi", base);
            return TRUE;
        }
        InitializeCriticalSection(&g_installLock);
        CreateThread(nullptr, 0, Boot, nullptr, 0, nullptr);
    }
    return TRUE;
}

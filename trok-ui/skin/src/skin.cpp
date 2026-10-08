// Trok Skin (.asi) -- Victor_Trok
// Padroniza o visual dos menus de qualquer mod Lua feito com mimgui, sem editar os mods:
//  - tema da casa (cores, cantos, borda dos campos e titulo centralizado) em todo quadro de cada script;
//  - a fonte da casa no lugar das fontes de sistema (Trebuchet do mimgui, Arial, Tahoma...), com o tamanho
//    ajustado para o texto ocupar a mesma largura. Glifos que a fonte da casa nao tem vem da fonte original.
// Nada que mexe no layout e alterado: espacamentos, tamanhos e bordas de janela ficam como o script deixou.
// HUDs com fundo transparente ficam exatamente como o autor fez.
//
// Como: o mimgui carrega uma DLL nativa so (moonloader\lib\mimgui\cimguidx9.dll, Dear ImGui 1.72 + cimgui)
// e todo script chama as funcoes exportadas dela. A skin desvia algumas dessas funcoes com o MinHook assim
// que a DLL carrega. So liga se a DLL for o ImGui 1.72 (o mesmo dos headers usados aqui); com outra versao,
// nao faz nada e diz no log.
//
// /trokskin liga e desliga o tema na hora (a fonte so muda quando os scripts recarregam).
// Trok Skin.ini (ao lado do .asi): tema=1, fonte=1 e manter=Titulo|Outro titulo (janelas que ficam como o
// autor fez).

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
std::vector<std::string> g_kept; // titulos (minusculos) das janelas que ficam como o autor fez

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

void LoadConfig() {
    if (GetFileAttributesA(g_iniPath) == INVALID_FILE_ATTRIBUTES) {
        FILE* f = fopen(g_iniPath, "w");
        if (f) {
            fputs("; Trok Skin: visual da casa nos menus feitos com mimgui.\n"
                  "[skin]\n"
                  "; 1 liga, 0 desliga. O tema tambem liga e desliga no jogo com /trokskin.\n"
                  "tema=1\n"
                  "; Fonte da casa no lugar das fontes de sistema (vale quando os scripts carregam).\n"
                  "fonte=1\n"
                  "; Janelas que ficam como o autor fez: o titulo que aparece na janela, separados por |\n"
                  "manter=\n",
                  f);
            fclose(f);
        }
    }
    g_theme = GetPrivateProfileIntA("skin", "tema", 1, g_iniPath) != 0;
    g_fontOn = GetPrivateProfileIntA("skin", "fonte", 1, g_iniPath) != 0;
    char kept[2048] = {};
    GetPrivateProfileStringA("skin", "manter", "", kept, sizeof(kept), g_iniPath);
    std::string list = kept;
    size_t start = 0;
    while (start <= list.size()) {
        size_t bar = list.find('|', start);
        std::string item = Trim(list.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
        if (!item.empty()) {
            g_kept.push_back(Lower(item));
        }
        if (bar == std::string::npos) {
            break;
        }
        start = bar + 1;
    }
    Log("config: tema %s, fonte %s, %d janela(s) mantida(s)", g_theme ? "ligado" : "desligado",
        g_fontOn ? "ligada" : "desligada", static_cast<int>(g_kept.size()));
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
AtlasFn o_AtlasClear = nullptr;
AtlasFn o_AtlasClearFonts = nullptr;
AtlasFn o_AtlasDestroy = nullptr;
AddTextFontFn o_AddTextFontPtr = nullptr;
CalcTextSizeFn o_CalcTextSizeA = nullptr;

// ---------------------------------------------------------------- tema da casa

// Campos do estilo que a skin troca: so os que nao mexem no layout (cantos, borda dos campos, alinhamento
// do titulo e cores). Espacamentos, tamanhos e as bordas de janela, filha e popup ficam como o script
// deixou (a borda da janela filha muda o recuo dela, por isso nao entra).
struct Look {
    float windowRounding, childRounding, popupRounding, frameRounding, scrollbarRounding, grabRounding, tabRounding;
    float frameBorderSize;
    ImVec2 windowTitleAlign;
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

void House(const Look& script, Look& out, float u) {
    out = script;
    HouseScalars(out, u);
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        bool transparent = IsBackground(i) && script.colors[i].w < 0.5f;
        out.colors[i] = transparent ? script.colors[i] : g_house[i];
    }
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
#undef ABSORB
    if (cur.windowTitleAlign.x != st.applied.windowTitleAlign.x ||
        cur.windowTitleAlign.y != st.applied.windowTitleAlign.y) {
        st.orig.windowTitleAlign = cur.windowTitleAlign;
    }
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        if (!Same(cur.colors[i], st.applied.colors[i])) {
            st.orig.colors[i] = cur.colors[i];
        }
    }
}

// Inicio de cada quadro de cada script: base do estilo = tema da casa (ou o visual do script, desligado).
void ApplyFrame() {
    ImGuiContext* ctx = p_igGetCurrentContext();
    if (!ctx) {
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
        Log("script novo do mimgui (contexto 0x%08X, %d no total)", reinterpret_cast<DWORD>(ctx),
            static_cast<int>(g_states.size()));
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

// Antes de cada janela: o script pode ter mudado cores ou cantos depois do inicio do quadro (ou empurrado
// para esta janela). Janela padronizada volta ao tema; janela mantida (manter= ou da casa, ##trok...) volta
// ao visual do script. HUD (fundo transparente) fica intocado.
void ApplyWindow(const char* name, ImGuiWindowFlags flags) {
    auto found = g_states.find(p_igGetCurrentContext());
    if (found == g_states.end()) {
        return;
    }
    State& st = found->second;
    ImGuiStyle* style = p_igGetStyle();
    if ((flags & ImGuiWindowFlags_NoBackground) || style->Colors[ImGuiCol_WindowBg].w < 0.5f) {
        return;
    }
    Look cur;
    Read(*style, cur);
    Look want = cur;
    if (!g_theme || Kept(name)) {
        // Sem push do script, o campo volta ao valor dele; com push, fica o que ele empurrou.
#define KEEP(f)                                                                                                   \
    if (cur.f == st.applied.f) {                                                                                  \
        want.f = st.orig.f;                                                                                       \
    }
        KEEP(windowRounding)
        KEEP(childRounding)
        KEEP(popupRounding)
        KEEP(frameRounding)
        KEEP(scrollbarRounding)
        KEEP(grabRounding)
        KEEP(tabRounding)
        KEEP(frameBorderSize)
#undef KEEP
        if (cur.windowTitleAlign.x == st.applied.windowTitleAlign.x &&
            cur.windowTitleAlign.y == st.applied.windowTitleAlign.y) {
            want.windowTitleAlign = st.orig.windowTitleAlign;
        }
        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            if (Same(cur.colors[i], st.applied.colors[i])) {
                want.colors[i] = st.orig.colors[i];
            }
        }
    } else {
        HouseScalars(want, Scale(p_igGetIO()->DisplaySize.y));
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

ImFont* __cdecl HookAddFontFromFileTTF(ImFontAtlas* atlas, const char* filename, float size, const ImFontConfig* cfg,
                                       const ImWchar* ranges) {
    if (!g_fontOn || !g_houseFont || !filename || (cfg && cfg->MergeMode) || !Replaceable(filename)) {
        return o_AddFontFromFileTTF(atlas, filename, size, cfg, ranges);
    }
    FontFile* source = LoadFontFile(filename);
    if (!source) {
        return o_AddFontFromFileTTF(atlas, filename, size, cfg, ranges);
    }
    bool cyrillic = HasCyrillic(ranges ? ranges : (cfg ? cfg->GlyphRanges : nullptr));
    float houseWidth = WidthPerPixel(*g_houseFont, cyrillic);
    float ratio = houseWidth > 0 ? WidthPerPixel(*source, cyrillic) / houseWidth : 0.0f;
    if (ratio < 0.8f || ratio > 1.25f) {
        Log("fonte %s ficou (largura muito diferente da fonte da casa: %.2f)", filename, ratio);
        return o_AddFontFromFileTTF(atlas, filename, size, cfg, ranges);
    }
    alignas(ImFontConfig) unsigned char storage[sizeof(ImFontConfig)];
    ImFontConfig& base = *reinterpret_cast<ImFontConfig*>(storage);
    CopyConfig(cfg, &base);
    float houseSize = size * ratio;
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
    Log("fonte %s %.1f px -> fonte da casa %.1f px (mesma largura de texto)", filename, size, houseSize);
    return font;
}

void ForgetAtlas(ImFontAtlas* atlas) {
    for (auto it = g_replaced.begin(); it != g_replaced.end();) {
        it = it->second.atlas == atlas ? g_replaced.erase(it) : std::next(it);
    }
}

void __cdecl HookAtlasClear(ImFontAtlas* atlas) {
    ForgetAtlas(atlas);
    o_AtlasClear(atlas);
}

void __cdecl HookAtlasClearFonts(ImFontAtlas* atlas) {
    ForgetAtlas(atlas);
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
             Hook(dll, "ImFontAtlas_Clear", reinterpret_cast<void*>(HookAtlasClear), o_AtlasClear) &&
             Hook(dll, "ImFontAtlas_ClearFonts", reinterpret_cast<void*>(HookAtlasClearFonts), o_AtlasClearFonts) &&
             Hook(dll, "ImFontAtlas_destroy", reinterpret_cast<void*>(HookAtlasDestroy), o_AtlasDestroy) &&
             Hook(dll, "ImDrawList_AddTextFontPtr", reinterpret_cast<void*>(HookAddTextFontPtr), o_AddTextFontPtr) &&
             Hook(dll, "ImFont_CalcTextSizeA_nonUDT", reinterpret_cast<void*>(HookCalcTextSizeA), o_CalcTextSizeA);
        static const char* hooked[] = {"igNewFrame",           "igBegin",           "igDestroyContext",
                                       "ImFontAtlas_AddFontFromFileTTF", "ImFontAtlas_Clear", "ImFontAtlas_ClearFonts",
                                       "ImFontAtlas_destroy",  "ImDrawList_AddTextFontPtr", "ImFont_CalcTextSizeA_nonUDT"};
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

bool IsCimgui(HMODULE module) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(module, path, MAX_PATH);
    if (!n || n >= MAX_PATH) {
        return false;
    }
    const wchar_t* slash = wcsrchr(path, L'\\');
    return _wcsicmp(slash ? slash + 1 : path, L"cimguidx9.dll") == 0;
}

// O LuaJIT carrega a DLL do mimgui com LoadLibraryExA, que termina aqui: os desvios entram antes de
// qualquer script usar a DLL.
typedef HMODULE(WINAPI* LoadLibraryExWFn)(LPCWSTR, HANDLE, DWORD);
LoadLibraryExWFn o_LoadLibraryExW = nullptr;

HMODULE WINAPI HookLoadLibraryExW(LPCWSTR name, HANDLE file, DWORD flags) {
    HMODULE module = o_LoadLibraryExW(name, file, flags);
    const DWORD dataOnly = LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE;
    if (module && !g_installed && !g_gaveUp && !(flags & dataOnly) && IsCimgui(module)) {
        Install(module);
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
    if (!g_installed) {
        Chat("{FF8A8A}[Trok Skin]{FFFFFF} o mimgui ainda nao carregou ou nao e o suportado. Veja o "
             "{FFD27A}Trok Skin.log{FFFFFF}.");
        return;
    }
    g_theme = !g_theme;
    Log("/trokskin: tema %s", g_theme ? "ligado" : "desligado");
    Chat(g_theme ? "{FFFFFF}[Trok Skin] visual da casa {8AFF8A}ligado{FFFFFF}."
                 : "{FFFFFF}[Trok Skin] visual da casa {FF8A8A}desligado{FFFFFF} (a fonte volta ao recarregar os "
                   "scripts).");
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
    // Se o mimgui ja estava carregado (o .asi entrou tarde), instala agora; senao o desvio do LoadLibrary pega.
    for (int i = 0; i < 20 && !g_installed && !g_gaveUp; ++i) {
        if (HMODULE dll = GetModuleHandleW(L"cimguidx9.dll")) {
            Install(dll);
        }
        Sleep(250);
    }
    RegisterCommand();
    // Reserva: confere de vez em quando se a DLL apareceu por outro caminho.
    while (!g_installed && !g_gaveUp) {
        if (HMODULE dll = GetModuleHandleW(L"cimguidx9.dll")) {
            Install(dll);
        }
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
        Log("esperando o mimgui carregar");
    } else {
        Log("sem desvio no LoadLibrary -- vou procurar o mimgui de tempos em tempos");
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
        Log("Trok Skin .asi v1.0.0");
        InitializeCriticalSection(&g_installLock);
        CreateThread(nullptr, 0, Boot, nullptr, 0, nullptr);
    }
    return TRUE;
}

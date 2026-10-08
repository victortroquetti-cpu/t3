#include "trok_ui.h"

#include "imgui_internal.h"

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace tui {

Fonts fonts;
float u = 1.0f;

namespace col {
ImU32 Rgba(int r, int g, int b, int a) { return IM_COL32(r, g, b, a); }
ImU32 White(int a) { return IM_COL32(255, 255, 255, a); }
const ImU32 text = IM_COL32(240, 240, 240, 255);
const ImU32 column = IM_COL32(200, 200, 200, 255);
const ImU32 hint = IM_COL32(255, 255, 255, 118);
const ImU32 disabled = IM_COL32(255, 255, 255, 42);
const ImU32 version = IM_COL32(140, 140, 140, 255);
const ImU32 separator = IM_COL32(255, 255, 255, 10);
const ImU32 selection = IM_COL32(255, 255, 255, 10);
const ImU32 hover = IM_COL32(255, 255, 255, 6);
const ImU32 number = IM_COL32(255, 255, 255, 78);
const ImU32 numberHover = IM_COL32(255, 255, 255, 110);
const ImU32 numberSel = IM_COL32(255, 255, 255, 150);
const ImU32 strong = IM_COL32(226, 226, 226, 255);
const ImU32 ink = IM_COL32(18, 18, 18, 255);
const ImU32 marker = IM_COL32(255, 255, 255, 70);
} // namespace col

namespace {

constexpr float PI = 3.14159265f;

// Codigos oficiais do Lucide (pacote lucide-static). O lucide.ttf da casa e um recorte com o X em
// 'x' (U+0078), seta, chevron, chevron duplo, olho e olho cortado; os outros entram se o arquivo tiver.
namespace glyph {
constexpr ImWchar X_ASCII = 0x0078, X = 0xE1B2;
constexpr ImWchar ARROW_RIGHT = 0xE049;
constexpr ImWchar CHECK = 0xE06C, CHEVRON_DOWN = 0xE06D, CHEVRON_LEFT = 0xE06E, CHEVRON_RIGHT = 0xE06F,
                  CHEVRON_UP = 0xE070, CHEVRONS_RIGHT = 0xE073;
constexpr ImWchar EYE = 0xE0BA, EYE_OFF = 0xE0BB, INFO = 0xE0F9, SEARCH = 0xE151;
} // namespace glyph

std::unordered_map<ImGuiID, float> g_anim;
ImGuiID g_capturingKey = 0;

struct ToastItem {
    std::string title;
    std::string message;
    double born;
};
std::vector<ToastItem> g_toasts;
constexpr double TOAST_LIFE = 4.0;

struct ShellCtx {
    ImVec2 pos;
    ImVec2 size;
} g_shell;

ImU32 Gray(float v, float a = 255.0f) {
    int g = static_cast<int>(v + 0.5f);
    return IM_COL32(g, g, g, static_cast<int>(a + 0.5f));
}

ImU32 Fade(ImU32 color, float alpha) {
    int a = static_cast<int>(((color >> IM_COL32_A_SHIFT) & 0xFF) * alpha + 0.5f);
    return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a) << IM_COL32_A_SHIFT);
}

void Box(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 fill, ImU32 border, float rounding) {
    if (fill & IM_COL32_A_MASK) {
        dl->AddRectFilled(a, b, fill, rounding);
    }
    if (border & IM_COL32_A_MASK) {
        dl->AddRect(ImVec2(a.x + 0.5f, a.y + 0.5f), ImVec2(b.x - 0.5f, b.y - 0.5f), border, rounding, 0, 1.0f);
    }
}

void HandCursor() {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}

void PushPopupStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8 * u);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6 * u, 6 * u));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(14, 14, 14, 252));
    ImGui::PushStyleColor(ImGuiCol_Border, col::White(18));
}

void PopPopupStyle() {
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
}

// ---------------------------------------------------------------- icones

// Desenha o glifo do lucide como um quadrado de textura centrado em c (quarterTurns gira 90 graus
// no sentido horario; mirror espelha na horizontal). Devolve false se a fonte nao tem o glifo.
bool GlyphQuad(ImDrawList* dl, ImFont* font, ImWchar cp, ImVec2 c, ImU32 color, int quarterTurns,
               bool mirror = false) {
    if (!font) {
        return false;
    }
    const ImFontGlyph* g = font->FindGlyphNoFallback(cp);
    if (!g || !g->Visible) {
        return false;
    }
    float hw = (g->X1 - g->X0) * 0.5f, hh = (g->Y1 - g->Y0) * 0.5f;
    bool swap = quarterTurns % 2 == 1;
    // Encosta o canto em pixel inteiro para o icone nao borrar.
    float ex = swap ? hh : hw, ey = swap ? hw : hh;
    c.x = std::round(c.x - ex) + ex;
    c.y = std::round(c.y - ey) + ey;
    const ImVec2 corners[4] = {ImVec2(-hw, -hh), ImVec2(hw, -hh), ImVec2(hw, hh), ImVec2(-hw, hh)};
    float u0 = mirror ? g->U1 : g->U0, u1 = mirror ? g->U0 : g->U1;
    const ImVec2 uvs[4] = {ImVec2(u0, g->V0), ImVec2(u1, g->V0), ImVec2(u1, g->V1), ImVec2(u0, g->V1)};
    ImVec2 p[4];
    for (int i = 0; i < 4; ++i) {
        float x = corners[i].x, y = corners[i].y;
        for (int k = 0; k < quarterTurns; ++k) {
            float t = x;
            x = -y;
            y = t;
        }
        p[i] = ImVec2(c.x + x, c.y + y);
    }
    dl->AddImageQuad(font->ContainerAtlas->TexID, p[0], p[1], p[2], p[3], uvs[0], uvs[1], uvs[2], uvs[3], color);
    return true;
}

void VectorChevron(ImDrawList* dl, ImVec2 c, int dir, ImU32 color) {
    float a = 2.5f * u, b = 5.0f * u, t = 1.6f * u;
    float x = c.x, y = c.y;
    switch (dir) {
    case 0: // esquerda
        dl->AddLine(ImVec2(x + a, y - b), ImVec2(x - a, y), color, t);
        dl->AddLine(ImVec2(x - a, y), ImVec2(x + a, y + b), color, t);
        break;
    case 1: // direita
        dl->AddLine(ImVec2(x - a, y - b), ImVec2(x + a, y), color, t);
        dl->AddLine(ImVec2(x + a, y), ImVec2(x - a, y + b), color, t);
        break;
    case 2: // baixo
        dl->AddLine(ImVec2(x - b, y - a), ImVec2(x, y + a), color, t);
        dl->AddLine(ImVec2(x, y + a), ImVec2(x + b, y - a), color, t);
        break;
    default: // cima
        dl->AddLine(ImVec2(x - b, y + a), ImVec2(x, y - a), color, t);
        dl->AddLine(ImVec2(x, y - a), ImVec2(x + b, y + a), color, t);
        break;
    }
}

void VectorEye(ImDrawList* dl, ImVec2 c, float s, bool slashed, ImU32 color) {
    float t = 1.4f * u;
    dl->AddBezierQuadratic(ImVec2(c.x - s, c.y), ImVec2(c.x, c.y - s * 1.15f), ImVec2(c.x + s, c.y), color, t);
    dl->AddBezierQuadratic(ImVec2(c.x - s, c.y), ImVec2(c.x, c.y + s * 1.15f), ImVec2(c.x + s, c.y), color, t);
    dl->AddCircle(c, s * 0.32f, color, 12, t);
    if (slashed) {
        dl->AddLine(ImVec2(c.x - s * 0.85f, c.y + s * 0.85f), ImVec2(c.x + s * 0.85f, c.y - s * 0.85f), color, t);
    }
}

void VectorCheck(ImDrawList* dl, ImVec2 c, float s, ImU32 color) {
    dl->AddLine(ImVec2(c.x - s, c.y), ImVec2(c.x - s * 0.3f, c.y + s * 0.7f), color, 1.8f * u);
    dl->AddLine(ImVec2(c.x - s * 0.3f, c.y + s * 0.7f), ImVec2(c.x + s, c.y - s * 0.75f), color, 1.8f * u);
}

} // namespace

bool CapturingKey() {
    return g_capturingKey != 0;
}

// ---------------------------------------------------------------- tokens

float Scale(float screenHeight) {
    return std::max(0.55f, (screenHeight / 1080.0f) * 0.85f);
}

void BuildFonts(float screenHeight, const char* gameDir) {
    u = Scale(screenHeight);
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    static const ImWchar textRanges[] = {
        0x0020, 0x00FF, // Latin-1 (acentos do portugues)
        0x0400, 0x052F, // cirilico, como os mods da casa
        0x2013, 0x2026, // travessao, aspas, reticencias, bullet
        0,
    };
    // So os icones que o kit usa: o atlas fica pequeno mesmo com o lucide completo.
    static const ImWchar iconRanges[] = {
        glyph::X_ASCII,     glyph::X_ASCII,    glyph::ARROW_RIGHT,    glyph::ARROW_RIGHT, glyph::CHECK,
        glyph::CHEVRON_UP,  glyph::CHEVRONS_RIGHT, glyph::CHEVRONS_RIGHT, glyph::EYE,     glyph::EYE_OFF,
        glyph::INFO,        glyph::INFO,       glyph::SEARCH,         glyph::SEARCH,      glyph::X,
        glyph::X,           0,
    };

    char house[MAX_PATH], icons[MAX_PATH];
    snprintf(house, sizeof(house), "%smoonloader\\resource\\trok\\font.ttf", gameDir);
    snprintf(icons, sizeof(icons), "%smoonloader\\resource\\trok\\lucide.ttf", gameDir);
#ifdef TROK_UI_TEST_FONT
    snprintf(house, sizeof(house), "%s", TROK_UI_TEST_FONT); // so no teste de renderizacao fora do Windows
#endif
#ifdef TROK_UI_TEST_ICONS
    snprintf(icons, sizeof(icons), "%s", TROK_UI_TEST_ICONS);
#endif
    const char* arial = "C:\\Windows\\Fonts\\arial.ttf";
    const char* body = GetFileAttributesA(house) != INVALID_FILE_ATTRIBUTES ? house : arial;

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    if (GetFileAttributesA(body) != INVALID_FILE_ATTRIBUTES) {
        fonts.body = io.Fonts->AddFontFromFileTTF(body, 16 * u, &cfg, textRanges);
        fonts.title = io.Fonts->AddFontFromFileTTF(body, 20 * u, &cfg, textRanges);
        fonts.desc = io.Fonts->AddFontFromFileTTF(body, 13.5f * u, &cfg, textRanges);
    } else {
        fonts.body = fonts.title = fonts.desc = io.Fonts->AddFontDefault();
    }
    fonts.icon = fonts.iconSmall = nullptr;
    if (GetFileAttributesA(icons) != INVALID_FILE_ATTRIBUTES) {
        ImFontConfig iconCfg;
        iconCfg.OversampleH = 2;
        iconCfg.OversampleV = 2;
        fonts.icon = io.Fonts->AddFontFromFileTTF(icons, 18 * u, &iconCfg, iconRanges);
        fonts.iconSmall = io.Fonts->AddFontFromFileTTF(icons, 13 * u, &iconCfg, iconRanges);
    }
    io.Fonts->Build();
}

ImVec2 TextSize(ImFont* font, const char* text) {
    return font->CalcTextSizeA(font->FontSize, 10000.0f, 0.0f, text);
}

void Text(ImDrawList* dl, ImFont* font, float x, float y, ImU32 color, const char* text) {
    dl->AddText(font, font->FontSize, ImVec2(std::floor(x), std::floor(y)), color, text);
}

float Anim(ImGuiID id, float target, float speed) {
    auto it = g_anim.find(id);
    if (it == g_anim.end()) {
        g_anim[id] = target;
        return target;
    }
    float dt = ImGui::GetIO().DeltaTime;
    float k = 1.0f - std::exp(-speed * dt);
    it->second += (target - it->second) * k;
    if (std::fabs(target - it->second) < 0.001f) {
        it->second = target;
    }
    return it->second;
}

// ---------------------------------------------------------------- icones

void DrawIcon(ImDrawList* dl, Icon icon, ImVec2 c, ImU32 color, bool small) {
    ImFont* font = small ? fonts.iconSmall : fonts.icon;
    switch (icon) {
    case Icon::Close:
        if (GlyphQuad(dl, font, glyph::X_ASCII, c, color, 0) || GlyphQuad(dl, font, glyph::X, c, color, 0)) {
            return;
        }
        dl->AddLine(ImVec2(c.x - 4.5f * u, c.y - 4.5f * u), ImVec2(c.x + 4.5f * u, c.y + 4.5f * u), color, 1.6f * u);
        dl->AddLine(ImVec2(c.x + 4.5f * u, c.y - 4.5f * u), ImVec2(c.x - 4.5f * u, c.y + 4.5f * u), color, 1.6f * u);
        return;
    case Icon::ChevronRight:
        if (!GlyphQuad(dl, font, glyph::CHEVRON_RIGHT, c, color, 0)) {
            VectorChevron(dl, c, 1, color);
        }
        return;
    case Icon::ChevronDown:
        if (!GlyphQuad(dl, font, glyph::CHEVRON_DOWN, c, color, 0) &&
            !GlyphQuad(dl, font, glyph::CHEVRON_RIGHT, c, color, 1)) {
            VectorChevron(dl, c, 2, color);
        }
        return;
    case Icon::ChevronLeft:
        // Espelhado (nao girado) para ficar na mesma altura do chevron da direita.
        if (!GlyphQuad(dl, font, glyph::CHEVRON_LEFT, c, color, 0) &&
            !GlyphQuad(dl, font, glyph::CHEVRON_RIGHT, c, color, 0, true)) {
            VectorChevron(dl, c, 0, color);
        }
        return;
    case Icon::ChevronUp:
        if (!GlyphQuad(dl, font, glyph::CHEVRON_UP, c, color, 0) &&
            !GlyphQuad(dl, font, glyph::CHEVRON_RIGHT, c, color, 3)) {
            VectorChevron(dl, c, 3, color);
        }
        return;
    case Icon::ChevronsRight:
        if (!GlyphQuad(dl, font, glyph::CHEVRONS_RIGHT, c, color, 0)) {
            VectorChevron(dl, ImVec2(c.x - 3 * u, c.y), 1, color);
            VectorChevron(dl, ImVec2(c.x + 3 * u, c.y), 1, color);
        }
        return;
    case Icon::ArrowRight:
        if (!GlyphQuad(dl, font, glyph::ARROW_RIGHT, c, color, 0)) {
            dl->AddLine(ImVec2(c.x - 6 * u, c.y), ImVec2(c.x + 6 * u, c.y), color, 1.6f * u);
            VectorChevron(dl, ImVec2(c.x + 3.5f * u, c.y), 1, color);
        }
        return;
    case Icon::Eye:
    case Icon::EyeOff:
        if (!GlyphQuad(dl, font, icon == Icon::Eye ? glyph::EYE : glyph::EYE_OFF, c, color, 0)) {
            VectorEye(dl, c, (small ? 5.0f : 6.5f) * u, icon == Icon::EyeOff, color);
        }
        return;
    case Icon::Check:
        if (!GlyphQuad(dl, font, glyph::CHECK, c, color, 0)) {
            VectorCheck(dl, c, (small ? 4.5f : 6.0f) * u, color);
        }
        return;
    case Icon::Search:
        if (!GlyphQuad(dl, font, glyph::SEARCH, c, color, 0)) {
            float s = (small ? 7.0f : 9.0f) * u, t = 1.5f * u;
            dl->AddCircle(ImVec2(c.x - s * 0.15f, c.y - s * 0.15f), s * 0.55f, color, 16, t);
            dl->AddLine(ImVec2(c.x + s * 0.25f, c.y + s * 0.25f), ImVec2(c.x + s * 0.75f, c.y + s * 0.75f), color, t);
        }
        return;
    case Icon::Info:
        if (!GlyphQuad(dl, font, glyph::INFO, c, color, 0)) {
            float r = (small ? 6.0f : 8.0f) * u;
            dl->AddCircle(c, r, color, 20, 1.4f * u);
            dl->AddCircleFilled(ImVec2(c.x, c.y - r * 0.42f), 1.2f * u, color);
            dl->AddLine(ImVec2(c.x, c.y - r * 0.1f), ImVec2(c.x, c.y + r * 0.5f), color, 1.4f * u);
        }
        return;
    }
}

float IconWidth(Icon icon, bool small) {
    ImFont* font = small ? fonts.iconSmall : fonts.icon;
    ImWchar cp = 0;
    switch (icon) {
    case Icon::ArrowRight: cp = glyph::ARROW_RIGHT; break;
    case Icon::ChevronRight: cp = glyph::CHEVRON_RIGHT; break;
    case Icon::ChevronsRight: cp = glyph::CHEVRONS_RIGHT; break;
    default: break;
    }
    if (font && cp) {
        if (const ImFontGlyph* g = font->FindGlyphNoFallback(cp)) {
            return g->X1 - g->X0;
        }
    }
    return (icon == Icon::ArrowRight ? 12.0f : 10.0f) * u;
}

ImVec2 RichText(ImDrawList* dl, ImFont* font, float x, float y, ImU32 color, const char* text, bool draw) {
    float lineH = font->FontSize + 4 * u;
    float spaceW = TextSize(font, " ").x;
    float cx = x, cy = y, maxW = 0;
    ImU32 current = color;
    std::string word;
    // Icones alinhados pelo meio das maiusculas (o centro da caixa da linha fica alto demais).
    const ImFontGlyph* capital = font->FindGlyph('H');
    float iconMid = capital ? (capital->Y0 + capital->Y1) * 0.5f : font->FontSize * 0.5f;

    auto flushWord = [&]() {
        if (word.empty()) {
            return;
        }
        // Setas do servidor viram icones do lucide (como no Trok Dialogs).
        Icon icon = Icon::Close;
        bool isIcon = true;
        if (word == ">") {
            icon = Icon::ChevronRight;
        } else if (word == ">>" || word == "\xC2\xBB") {
            icon = Icon::ChevronsRight;
        } else if (word == "->" || word == "=>") {
            icon = Icon::ArrowRight;
        } else {
            isIcon = false;
        }
        if (isIcon) {
            float w = IconWidth(icon);
            if (draw) {
                DrawIcon(dl, icon, ImVec2(cx + w * 0.5f, cy + iconMid), current);
            }
            cx += w;
        } else {
            if (draw) {
                Text(dl, font, cx, cy, current, word.c_str());
            }
            cx += TextSize(font, word.c_str()).x;
        }
        word.clear();
    };

    for (const char* p = text; *p;) {
        if (*p == '{' && std::strlen(p) >= 8 && p[7] == '}') {
            char hex[7];
            std::memcpy(hex, p + 1, 6);
            hex[6] = 0;
            char* end = nullptr;
            unsigned long rgb = std::strtoul(hex, &end, 16);
            if (end == hex + 6) {
                flushWord();
                current = IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, (color >> IM_COL32_A_SHIFT) & 0xFF);
                p += 8;
                continue;
            }
        }
        if (*p == '\n') {
            flushWord();
            maxW = std::max(maxW, cx - x);
            cx = x;
            cy += lineH;
            ++p;
            continue;
        }
        if (*p == ' ') {
            flushWord();
            cx += spaceW;
            ++p;
            continue;
        }
        word.push_back(*p++);
    }
    flushWord();
    maxW = std::max(maxW, cx - x);
    return ImVec2(maxW, cy + lineH - y);
}

void Spinner(ImDrawList* dl, ImVec2 c, float radius, float time) {
    dl->AddCircle(c, radius, col::White(22), 24, 2.0f * u);
    float start = time * 5.0f;
    dl->PathArcTo(c, radius, start, start + PI * 1.4f, 20);
    dl->PathStroke(col::strong, 0, 2.0f * u);
}

// ---------------------------------------------------------------- janela

float HeaderHeight() { return 44 * u; }
float FooterHeight() { return 36 * u; }

void PushStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10 * u);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9 * u);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18 * u, 16 * u));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4 * u);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 2 * u);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(240, 240, 240, 255));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, IM_COL32(120, 120, 120, 255));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(12, 12, 12, 252));
    ImGui::PushStyleColor(ImGuiCol_Border, col::White(14));
    ImGui::PushStyleColor(ImGuiCol_BorderShadow, 0);
    ImGui::PushStyleColor(ImGuiCol_NavHighlight, 0);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, 0);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, col::White(40));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, col::White(70));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, col::White(100));
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 120));
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, col::White(46));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, 0);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, 0);
}

void PopStyle() {
    ImGui::PopStyleColor(15);
    ImGui::PopStyleVar(7);
}

bool BeginShell(const char* id, const char* title, const char* version, float width, float height, Shell& shell,
                ImGuiWindowFlags extraFlags) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 screen = io.DisplaySize;
    width = std::ceil(width);
    height = std::ceil(height);

    if (shell.moved) {
        shell.at.x = std::max(0.0f, std::min(screen.x - width, shell.at.x));
        shell.at.y = std::max(0.0f, std::min(screen.y - height, shell.at.y));
        ImGui::SetNextWindowPos(ImVec2(std::floor(shell.at.x), std::floor(shell.at.y)), ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowPos(ImVec2(screen.x * 0.5f, screen.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    }
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings | extraFlags;
    ImGui::PushFont(fonts.body);
    ImGui::Begin(id, nullptr, flags);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    g_shell = {pos, size};
    float headerH = HeaderHeight();
    float padX = 18 * u;

    dl->AddLine(ImVec2(pos.x + padX, pos.y + headerH - 0.5f), ImVec2(pos.x + size.x - padX, pos.y + headerH - 0.5f),
                col::separator, 1.0f);

    ImVec2 titleSize = TextSize(fonts.title, title);
    ImVec2 versionSize = version ? TextSize(fonts.desc, version) : ImVec2(0, 0);
    float gap = version ? 8 * u : 0.0f;
    float titleX = pos.x + (size.x - titleSize.x - gap - versionSize.x) * 0.5f;
    Text(dl, fonts.title, titleX, pos.y + (headerH - titleSize.y) * 0.5f, col::text, title);
    if (version) {
        Text(dl, fonts.desc, titleX + titleSize.x + gap, pos.y + (headerH - versionSize.y) * 0.5f + 2 * u,
             col::version, version);
    }

    bool keepOpen = true;
    float closeSide = 28 * u;
    float closeX = pos.x + size.x - padX - closeSide + 6 * u;
    float closeY = pos.y + (headerH - closeSide) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(closeX, closeY));
    if (ImGui::InvisibleButton("##fecharX", ImVec2(closeSide, closeSide))) {
        keepOpen = false;
    }
    bool closeHovered = ImGui::IsItemHovered();
    if (closeHovered) {
        HandCursor();
    }
    DrawIcon(dl, Icon::Close, ImVec2(closeX + closeSide * 0.5f, closeY + closeSide * 0.5f),
             closeHovered ? col::text : col::hint);

    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton("##alca", ImVec2(size.x, headerH));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        if (!shell.moved) {
            shell.moved = true;
            shell.at = pos;
        }
        shell.at.x += io.MouseDelta.x;
        shell.at.y += io.MouseDelta.y;
        shell.dragging = true;
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    } else if (shell.dragging && !ImGui::IsMouseDown(0)) {
        shell.dragging = false;
    }
    return keepOpen;
}

int EndShell(const std::vector<Hint>& hints, bool alignRight) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = g_shell.pos;
    ImVec2 size = g_shell.size;
    float footerH = FooterHeight();
    float footerY = pos.y + size.y - footerH;
    float padX = 18 * u;

    dl->AddRectFilled(ImVec2(pos.x + 1, footerY), ImVec2(pos.x + size.x - 1, pos.y + size.y - 1),
                      IM_COL32(0, 0, 0, 46), 10 * u, ImDrawFlags_RoundCornersBottom);
    dl->AddLine(ImVec2(pos.x + 1, footerY + 0.5f), ImVec2(pos.x + size.x - 1, footerY + 0.5f), col::separator, 1.0f);

    float keyGap = 6 * u, hintGap = 22 * u;
    float total = 0.0f;
    for (size_t i = 0; i < hints.size(); ++i) {
        total += TextSize(fonts.desc, hints[i].key).x + keyGap + TextSize(fonts.desc, hints[i].action).x;
        if (i + 1 < hints.size()) {
            total += hintGap;
        }
    }
    float x = alignRight ? pos.x + size.x - padX - total : pos.x + padX;
    float y = footerY + (footerH - fonts.desc->FontSize) * 0.5f;

    int clicked = -1;
    for (size_t i = 0; i < hints.size(); ++i) {
        const Hint& h = hints[i];
        float keyW = TextSize(fonts.desc, h.key).x;
        float actW = TextSize(fonts.desc, h.action).x;
        bool hovered = false;
        if (h.clickable) {
            char id[32];
            snprintf(id, sizeof(id), "##cfgDica%d", static_cast<int>(i));
            ImGui::SetCursorScreenPos(ImVec2(x - 4 * u, footerY));
            if (ImGui::InvisibleButton(id, ImVec2(keyW + keyGap + actW + 8 * u, footerH))) {
                clicked = static_cast<int>(i);
            }
            hovered = ImGui::IsItemHovered();
            if (hovered) {
                HandCursor();
            }
        }
        Text(dl, fonts.desc, x, y, hovered ? col::text : col::column, h.key);
        Text(dl, fonts.desc, x + keyW + keyGap, y, hovered ? col::column : col::hint, h.action);
        x += keyW + keyGap + actW + hintGap;
    }

    ImGui::End();
    ImGui::PopFont();
    return clicked;
}

// ---------------------------------------------------------------- abas

float TabsHeight() { return 34 * u; }

int Tabs(const char* id, const char* const* labels, int count, int* current, float x, float y, float width) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float h = TabsHeight();
    float tabW = width / count;
    int clicked = -1;
    ImGui::PushID(id);
    for (int i = 0; i < count; ++i) {
        float tx = x + tabW * i;
        ImGui::SetCursorScreenPos(ImVec2(tx, y));
        char bid[16];
        snprintf(bid, sizeof(bid), "##aba%d", i);
        if (ImGui::InvisibleButton(bid, ImVec2(tabW, h))) {
            clicked = i;
            *current = i;
        }
        bool hovered = ImGui::IsItemHovered();
        if (hovered) {
            HandCursor();
        }
        ImVec2 ts = TextSize(fonts.desc, labels[i]);
        ImU32 color = (*current == i) ? col::text : (hovered ? col::column : col::hint);
        Text(dl, fonts.desc, tx + (tabW - ts.x) * 0.5f, y + (h - ts.y) * 0.5f - 1 * u, color, labels[i]);
    }

    // Indicador deslizante sob a aba atual (largura do texto + 7u de cada lado).
    ImVec2 ts = TextSize(fonts.desc, labels[*current]);
    float targetX = x + tabW * (*current) + (tabW - ts.x) * 0.5f - 7 * u;
    float targetW = ts.x + 14 * u;
    float ax = Anim(ImGui::GetID("##abaX"), targetX, 18.0f);
    float aw = Anim(ImGui::GetID("##abaW"), targetW, 18.0f);
    dl->AddLine(ImVec2(x, y + h - 0.5f), ImVec2(x + width, y + h - 0.5f), col::separator, 1.0f);
    dl->AddRectFilled(ImVec2(ax, y + h - 2 * u), ImVec2(ax + aw, y + h), col::text, 1 * u);
    ImGui::PopID();
    return clicked;
}

// ---------------------------------------------------------------- linhas

void Rows::Begin(const char* id, float x, float width, int* selected, const Keys& keys, bool showNumbers) {
    dl_ = ImGui::GetWindowDrawList();
    x_ = x;
    w_ = width;
    selected_ = selected;
    keys_ = keys;
    numbers_ = showNumbers;
    index_ = 0;
    rowH_ = 28 * u;
    gap_ = 2 * u;
    recuo_ = 12 * u;
    numberW_ = TextSize(fonts.desc, "99").x;
    labelX_ = showNumbers ? x_ + recuo_ + numberW_ + 9 * u : x_ + recuo_;
    y_ = ImGui::GetCursorScreenPos().y;
    ImGui::PushID(id);
}

int Rows::End() {
    ImGui::PopID();
    keyboardMoved = false;
    return index_;
}

ImGuiID Rows::Id(const char* suffix) const {
    return ImGui::GetID(suffix);
}

void Rows::Start() {
    ++index_;
    y_ = ImGui::GetCursorScreenPos().y;
    restoreVisible_ = false;
    restoreHovered_ = false;
    ImGui::PushID(index_);
}

bool Rows::Row(float height, bool controlHovered, bool selectable) {
    ImGui::SetCursorScreenPos(ImVec2(x_, y_));
    bool clicked = ImGui::InvisibleButton("##linha", ImVec2(w_, height));
    bool hovered = ImGui::IsItemHovered() || controlHovered;
    if (clicked && selectable) {
        *selected_ = index_;
    }
    if (selectable && Selected()) {
        dl_->AddRectFilled(ImVec2(x_, y_), ImVec2(x_ + w_, y_ + height), col::selection, 6 * u);
    } else if (hovered && selectable) {
        dl_->AddRectFilled(ImVec2(x_, y_), ImVec2(x_ + w_, y_ + height), col::hover, 6 * u);
    }
    return hovered;
}

void Rows::Number(bool hovered, float centerY) {
    if (!numbers_) {
        return;
    }
    char number[8];
    snprintf(number, sizeof(number), "%d", index_);
    ImVec2 ns = TextSize(fonts.desc, number);
    ImU32 nc = Selected() ? col::numberSel : (hovered ? col::numberHover : col::number);
    Text(dl_, fonts.desc, x_ + recuo_ + numberW_ - ns.x, centerY - ns.y * 0.5f, nc, number);
}

void Rows::Label(const char* label, bool hovered) {
    float centerY = y_ + rowH_ * 0.5f;
    Number(hovered, centerY);
    RichText(dl_, fonts.body, labelX_, centerY - fonts.body->FontSize * 0.5f, col::text, label);
    if (restoreVisible_) {
        Text(dl_, fonts.desc, restoreX_, y_ + (rowH_ - fonts.desc->FontSize) * 0.5f,
             restoreHovered_ ? col::text : col::hint, "Restaurar");
    }
}

// "Restaurar" discreto a esquerda do controle, so quando o valor saiu do padrao (Trok Radar).
// A dica mostra qual e o valor padrao.
bool Rows::Restore(bool differs, float controlLeft, const char* defaultText) {
    if (!differs) {
        return false;
    }
    ImVec2 ts = TextSize(fonts.desc, "Restaurar");
    restoreX_ = controlLeft - 12 * u - ts.x;
    restoreVisible_ = true;
    ImGui::SetCursorScreenPos(ImVec2(restoreX_ - 4 * u, y_));
    bool clicked = ImGui::InvisibleButton("##restaurar", ImVec2(ts.x + 8 * u, rowH_));
    restoreHovered_ = ImGui::IsItemHovered();
    if (restoreHovered_) {
        HandCursor();
        char tip[96];
        snprintf(tip, sizeof(tip), "Volta ao padr\xC3\xA3o: %s", defaultText);
        Tooltip(tip);
    }
    return clicked;
}

// Ponto discreto que marca "este e o valor padrao" (setas, giro e segmentado).
void Rows::DefaultDot(float x, float y) {
    dl_->AddCircleFilled(ImVec2(x, y), 1.6f * u, col::marker, 12);
}

void Rows::Finish(float height) {
    ImGui::SetCursorScreenPos(ImVec2(x_, y_));
    ImGui::Dummy(ImVec2(w_, height + gap_));
    if (keyboardMoved && Selected()) {
        ImGui::ScrollToRect(ImGui::GetCurrentWindow(), ImRect(x_, y_ - gap_ * 4, x_ + w_, y_ + height + gap_ * 4),
                            ImGuiScrollFlags_KeepVisibleEdgeY);
    }
    ImGui::PopID();
    y_ = ImGui::GetCursorScreenPos().y;
}

bool Rows::Action(const char* label, const char* button) {
    Start();
    float bw = std::max(136 * u, TextSize(fonts.desc, button).x + 28 * u), bh = 24 * u;
    float bx = x_ + w_ - recuo_ * 0.5f - bw, by = y_ + (rowH_ - bh) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(bx, by));
    bool clicked = ImGui::InvisibleButton("##acao", ImVec2(bw, bh));
    bool bHovered = ImGui::IsItemHovered();
    bool bActive = ImGui::IsItemActive();
    if (bHovered) {
        HandCursor();
    }
    bool hovered = Row(rowH_, bHovered);
    Label(label, hovered);
    if (Selected() && (keys_.enter || keys_.space)) {
        clicked = true;
    }
    if (clicked) {
        *selected_ = index_;
    }
    bool lit = bHovered || bActive;
    Box(dl_, ImVec2(bx, by), ImVec2(bx + bw, by + bh), col::White(lit ? 16 : 8), col::White(lit ? 40 : 22), 6 * u);
    ImVec2 ts = TextSize(fonts.desc, button);
    Text(dl_, fonts.desc, bx + (bw - ts.x) * 0.5f, by + (bh - ts.y) * 0.5f, lit ? col::text : col::column, button);
    Finish(rowH_);
    return clicked;
}

namespace {

// Controle de setas do Kill List: < valor >. Botoes primeiro (ganham o hover), desenho depois.
struct Arrows {
    float cx, aw, vw;
    bool cl, cv, cr, hl, hv, hr;
};

Arrows ArrowButtons(float right, float y, float rowH) {
    Arrows a{};
    float cw = 150 * u;
    a.aw = 32 * u;
    a.vw = cw - 2 * a.aw;
    a.cx = right - cw;
    ImGui::SetCursorScreenPos(ImVec2(a.cx, y));
    a.cl = ImGui::InvisibleButton("##cfgE", ImVec2(a.aw, rowH));
    a.hl = ImGui::IsItemHovered();
    ImGui::SetCursorScreenPos(ImVec2(a.cx + a.aw, y));
    a.cv = ImGui::InvisibleButton("##cfgV", ImVec2(a.vw, rowH));
    a.hv = ImGui::IsItemHovered();
    ImGui::SetCursorScreenPos(ImVec2(a.cx + a.aw + a.vw, y));
    a.cr = ImGui::InvisibleButton("##cfgD", ImVec2(a.aw, rowH));
    a.hr = ImGui::IsItemHovered();
    return a;
}

void DrawArrows(ImDrawList* dl, const Arrows& a, float cy, const char* text, bool leftOff, bool rightOff) {
    DrawIcon(dl, Icon::ChevronLeft, ImVec2(a.cx + a.aw * 0.5f, cy),
             leftOff ? col::disabled : (a.hl ? col::text : col::column));
    DrawIcon(dl, Icon::ChevronRight, ImVec2(a.cx + a.aw + a.vw + a.aw * 0.5f, cy),
             rightOff ? col::disabled : (a.hr ? col::text : col::column));
    ImVec2 ts = TextSize(fonts.desc, text);
    Text(dl, fonts.desc, a.cx + a.aw + (a.vw - ts.x) * 0.5f, cy - ts.y * 0.5f, a.hv ? col::text : col::column, text);
}

} // namespace

bool Rows::Stepper(const char* label, int* value, int min, int max, int step, const char* fmt, int def) {
    Start();
    float cy = y_ + rowH_ * 0.5f;
    bool changed = false;
    Arrows a = ArrowButtons(x_ + w_ - recuo_ * 0.5f, y_, rowH_);
    bool leftOff = *value <= min, rightOff = *value >= max;
    a.hl = a.hl && !leftOff;
    a.hr = a.hr && !rightOff;
    char defText[32];
    snprintf(defText, sizeof(defText), fmt, def);
    if (Restore(*value != def, a.cx, defText)) {
        *value = def;
        changed = true;
    }
    if (a.hl || a.hr) {
        HandCursor();
    }
    if ((a.cl || (Selected() && keys_.left)) && !leftOff) {
        *value = std::max(min, *value - step);
        changed = true;
    }
    if ((a.cr || (Selected() && keys_.right)) && !rightOff) {
        *value = std::min(max, *value + step);
        changed = true;
    }
    if (a.cl || a.cr) {
        *selected_ = index_;
    }

    bool hovered = Row(rowH_, a.hl || a.hv || a.hr);
    Label(label, hovered);
    char text[32];
    snprintf(text, sizeof(text), fmt, *value);
    DrawArrows(dl_, a, cy, text, *value <= min, *value >= max);
    if (*value == def) {
        DefaultDot(a.cx + a.aw + a.vw * 0.5f, cy + 9 * u);
    }
    Finish(rowH_);
    return changed;
}

bool Rows::Cycle(const char* label, int* index, const char* const* options, int count, int def) {
    Start();
    float cy = y_ + rowH_ * 0.5f;
    bool changed = false;
    Arrows a = ArrowButtons(x_ + w_ - recuo_ * 0.5f, y_, rowH_);
    if (Restore(*index != def, a.cx, options[def])) {
        *index = def;
        changed = true;
    }
    if (a.hl || a.hv || a.hr) {
        HandCursor();
    }
    if (a.cl || (Selected() && keys_.left)) {
        *index = (*index + count - 1) % count;
        changed = true;
    }
    if (a.cv || a.cr || (Selected() && (keys_.right || keys_.enter))) {
        *index = (*index + 1) % count;
        changed = true;
    }
    if (a.cl || a.cv || a.cr) {
        *selected_ = index_;
    }

    bool hovered = Row(rowH_, a.hl || a.hv || a.hr);
    Label(label, hovered);
    DrawArrows(dl_, a, cy, options[*index], false, false);
    if (*index == def) {
        DefaultDot(a.cx + a.aw + a.vw * 0.5f, cy + 9 * u);
    }
    Finish(rowH_);
    return changed;
}

bool Rows::Toggle(const char* label, bool* value) {
    Start();
    float tw = 40 * u, th = 22 * u;
    float tx = x_ + w_ - recuo_ * 0.5f - tw, ty = y_ + (rowH_ - th) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(tx, ty));
    bool clicked = ImGui::InvisibleButton("##b", ImVec2(tw, th));
    bool thov = ImGui::IsItemHovered();
    if (thov) {
        HandCursor();
    }
    ImGui::SetCursorScreenPos(ImVec2(x_, y_));
    bool rowClicked = ImGui::InvisibleButton("##bLinha", ImVec2(w_, rowH_));
    bool hovered = ImGui::IsItemHovered() || thov;
    if (rowClicked || (Selected() && (keys_.enter || keys_.space || keys_.left || keys_.right))) {
        clicked = true;
    }
    if (clicked) {
        *value = !*value;
        *selected_ = index_;
    }
    if (Selected()) {
        dl_->AddRectFilled(ImVec2(x_, y_), ImVec2(x_ + w_, y_ + rowH_), col::selection, 6 * u);
    } else if (hovered) {
        dl_->AddRectFilled(ImVec2(x_, y_), ImVec2(x_ + w_, y_ + rowH_), col::hover, 6 * u);
    }
    Label(label, hovered);

    // Interruptor do Trok Radar: trilho escuro -> claro, bolinha clara -> escura.
    float t = Anim(Id("##toggle"), *value ? 1.0f : 0.0f);
    float track = thov ? 40 + 200 * t : 32 + 194 * t;
    float knobBase = thov ? 226.0f : 196.0f;
    float knob = knobBase + (18 - knobBase) * t;
    float r = th * 0.5f;
    dl_->AddRectFilled(ImVec2(tx, ty), ImVec2(tx + tw, ty + th), Gray(track), r);
    float border = (1 - t) * (thov ? 34 : 22);
    dl_->AddRect(ImVec2(tx - 0.5f, ty - 0.5f), ImVec2(tx + tw + 0.5f, ty + th + 0.5f), col::White((int)border),
                 r + 0.5f, 0, 1.0f);
    dl_->AddCircleFilled(ImVec2(tx + r + (tw - th) * t, ty + r), r - 3 * u, Gray(knob), 24);
    Finish(rowH_);
    return clicked;
}

bool Rows::Segmented(const char* label, int* index, const char* const* options, int count, int def) {
    Start();
    float segW = 0;
    for (int i = 0; i < count; ++i) {
        segW = std::max(segW, TextSize(fonts.desc, options[i]).x + 26 * u);
    }
    float sh = 24 * u, sw = segW * count;
    float sx = x_ + w_ - recuo_ * 0.5f - sw, sy = y_ + (rowH_ - sh) * 0.5f;
    bool changed = false;
    int hoveredSeg = -1;
    for (int i = 0; i < count; ++i) {
        ImGui::SetCursorScreenPos(ImVec2(sx + segW * i, sy));
        char bid[24];
        snprintf(bid, sizeof(bid), "##e%d", i);
        if (ImGui::InvisibleButton(bid, ImVec2(segW, sh)) && *index != i) {
            *index = i;
            changed = true;
            *selected_ = index_;
        }
        if (ImGui::IsItemHovered()) {
            hoveredSeg = i;
            HandCursor();
        }
    }
    if (Restore(*index != def, sx, options[def])) {
        *index = def;
        changed = true;
    }
    if (Selected() && keys_.left && *index > 0) {
        --*index;
        changed = true;
    }
    if (Selected() && keys_.right && *index < count - 1) {
        ++*index;
        changed = true;
    }
    bool hovered = Row(rowH_, hoveredSeg >= 0);
    Label(label, hovered);

    Box(dl_, ImVec2(sx, sy), ImVec2(sx + sw, sy + sh), col::White(6), col::White(18), 6 * u);
    float px = Anim(Id("##seg"), sx + segW * (*index), 20.0f);
    Box(dl_, ImVec2(px + 2 * u, sy + 2 * u), ImVec2(px + segW - 2 * u, sy + sh - 2 * u), col::White(26),
        col::White(34), 5 * u);
    for (int i = 0; i < count; ++i) {
        ImVec2 ts = TextSize(fonts.desc, options[i]);
        ImU32 c = (*index == i) ? col::text : (hoveredSeg == i ? col::column : col::hint);
        float segX = sx + segW * i;
        Text(dl_, fonts.desc, segX + (segW - ts.x) * 0.5f, sy + (sh - ts.y) * 0.5f - 1 * u, c, options[i]);
        if (i == def) {
            DefaultDot(segX + segW * 0.5f, sy + sh - 4 * u);
        }
    }
    Finish(rowH_);
    return changed;
}

bool Rows::Slider(const char* label, int* value, int min, int max, int step, const char* fmt, int def) {
    Start();
    float valueW = 56 * u, trackW = 180 * u, knobR = 7 * u;
    float right = x_ + w_ - recuo_ * 0.5f;
    float tx = right - valueW - 10 * u - trackW, cy = y_ + rowH_ * 0.5f;
    bool changed = false;

    ImGui::SetCursorScreenPos(ImVec2(tx - knobR, y_));
    ImGui::InvisibleButton("##l", ImVec2(trackW + knobR * 2, rowH_));
    bool shov = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();
    if (shov || active) {
        HandCursor();
    }
    if (active) {
        float f = (ImGui::GetIO().MousePos.x - tx) / trackW;
        f = std::max(0.0f, std::min(1.0f, f));
        int v = min + static_cast<int>(std::round(f * (max - min) / step)) * step;
        v = std::max(min, std::min(max, v));
        if (v != *value) {
            *value = v;
            changed = true;
        }
        *selected_ = index_;
    }
    char defText[32];
    snprintf(defText, sizeof(defText), fmt, def);
    if (Restore(*value != def, tx - knobR, defText)) {
        *value = def;
        changed = true;
    }
    if (Selected() && keys_.left && *value > min) {
        *value = std::max(min, *value - step);
        changed = true;
    }
    if (Selected() && keys_.right && *value < max) {
        *value = std::min(max, *value + step);
        changed = true;
    }

    bool hovered = Row(rowH_, shov);
    Label(label, hovered);
    float f = static_cast<float>(*value - min) / static_cast<float>(max - min);
    float fx = Anim(Id("##sl"), tx + trackW * f, 24.0f);
    float th = 4 * u;
    dl_->AddRectFilled(ImVec2(tx, cy - th * 0.5f), ImVec2(tx + trackW, cy + th * 0.5f), col::White(26), th * 0.5f);
    dl_->AddRectFilled(ImVec2(tx, cy - th * 0.5f), ImVec2(fx, cy + th * 0.5f), col::strong, th * 0.5f);
    // Linha vertical no valor padrao (Trok Radar), por baixo da bolinha.
    float dx = std::floor(tx + trackW * static_cast<float>(def - min) / static_cast<float>(max - min)) + 0.5f;
    dl_->AddLine(ImVec2(dx, cy - 7 * u), ImVec2(dx, cy + 7 * u), (shov || active) ? col::White(130) : col::White(90),
                 1.5f * u);
    float kr = (shov || active) ? knobR + 1 * u : knobR;
    dl_->AddCircleFilled(ImVec2(fx, cy), kr, col::text, 24);
    dl_->AddCircle(ImVec2(fx, cy), kr, IM_COL32(0, 0, 0, 90), 24, 1.0f);
    char text[32];
    snprintf(text, sizeof(text), fmt, *value);
    ImVec2 ts = TextSize(fonts.desc, text);
    Text(dl_, fonts.desc, right - ts.x, cy - ts.y * 0.5f, active ? col::text : col::column, text);
    Finish(rowH_);
    return changed;
}

bool Rows::Keybind(const char* label, ImGuiKey* key, ImGuiKey def) {
    Start();
    ImGuiID& capturing = g_capturingKey;
    ImGuiID myId = Id("##k");
    bool isCapturing = capturing == myId;

    const char* name = *key == ImGuiKey_None ? "Nenhuma" : ImGui::GetKeyName(*key);
    const char* shown = isCapturing ? "Pressione uma tecla" : name;
    float cw = std::max(64 * u, TextSize(fonts.desc, shown).x + 22 * u), ch = 24 * u;
    float cx = x_ + w_ - recuo_ * 0.5f - cw, cy = y_ + (rowH_ - ch) * 0.5f;
    bool changed = false;

    ImGui::SetCursorScreenPos(ImVec2(cx, cy));
    bool clicked = ImGui::InvisibleButton("##k", ImVec2(cw, ch));
    bool khov = ImGui::IsItemHovered();
    if (khov) {
        HandCursor();
    }
    if (Restore(*key != def && !isCapturing, cx, def == ImGuiKey_None ? "Nenhuma" : ImGui::GetKeyName(def))) {
        *key = def;
        changed = true;
    }
    if (clicked || (Selected() && keys_.enter && !isCapturing)) {
        capturing = myId;
        isCapturing = true;
        *selected_ = index_;
    } else if (isCapturing) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsMouseClicked(1)) {
            capturing = 0;
        } else if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
            *key = ImGuiKey_None;
            capturing = 0;
            changed = true;
        } else {
            for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
                ImGuiKey kk = static_cast<ImGuiKey>(k);
                bool isMouse = kk >= ImGuiKey_MouseLeft && kk <= ImGuiKey_MouseWheelY;
                bool isMod = kk >= ImGuiKey_ReservedForModCtrl;
                if (!isMouse && !isMod && kk != ImGuiKey_Escape && ImGui::IsKeyPressed(kk, false)) {
                    *key = kk;
                    capturing = 0;
                    changed = true;
                    break;
                }
            }
        }
    }

    bool hovered = Row(rowH_, khov);
    Label(label, hovered);
    float pulse = isCapturing ? 0.55f + 0.45f * std::sin(static_cast<float>(ImGui::GetTime()) * 6.0f) : 1.0f;
    Box(dl_, ImVec2(cx, cy), ImVec2(cx + cw, cy + ch), col::White(khov || isCapturing ? 14 : 8),
        col::White(isCapturing ? static_cast<int>(90 * pulse) : (khov ? 40 : 22)), 6 * u);
    ImVec2 ts = TextSize(fonts.desc, shown);
    ImU32 tc = isCapturing ? Fade(col::text, pulse) : (*key == ImGuiKey_None ? col::hint : col::text);
    Text(dl_, fonts.desc, cx + (cw - ts.x) * 0.5f, cy + (ch - ts.y) * 0.5f, tc, shown);
    Finish(rowH_);
    return changed;
}

namespace {

void HsvToU32(float h, float s, float v, ImU32* out) {
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
    *out = IM_COL32(static_cast<int>(r * 255 + 0.5f), static_cast<int>(g * 255 + 0.5f),
                    static_cast<int>(b * 255 + 0.5f), 255);
}

void U32ToHsv(ImU32 c, float* h, float* s, float* v) {
    float r = ((c >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
    float g = ((c >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
    float b = ((c >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
    ImGui::ColorConvertRGBtoHSV(r, g, b, *h, *s, *v);
}

void HexText(ImU32 c, char* out, size_t size) {
    snprintf(out, size, "#%02X%02X%02X", (c >> IM_COL32_R_SHIFT) & 0xFF, (c >> IM_COL32_G_SHIFT) & 0xFF,
             (c >> IM_COL32_B_SHIFT) & 0xFF);
}

// Item de menu flutuante (lista suspensa, menu do campo). Devolve true no clique.
bool PopupItem(const char* label, bool checked, bool isDefault, float width) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = 26 * u;
    bool clicked = ImGui::InvisibleButton(label, ImVec2(width, h));
    bool hovered = ImGui::IsItemHovered();
    if (hovered) {
        HandCursor();
        dl->AddRectFilled(p, ImVec2(p.x + width, p.y + h), col::White(12), 5 * u);
    }
    ImVec2 ts = TextSize(fonts.desc, label);
    Text(dl, fonts.desc, p.x + 10 * u, p.y + (h - ts.y) * 0.5f, hovered || checked ? col::text : col::column, label);
    float right = p.x + width - 10 * u;
    if (checked) {
        DrawIcon(dl, Icon::Check, ImVec2(right - 5 * u, p.y + h * 0.5f), col::text, true);
        right -= 18 * u;
    }
    if (isDefault) {
        const char* tag = "padr\xC3\xA3o";
        ImVec2 tg = TextSize(fonts.desc, tag);
        Text(dl, fonts.desc, right - tg.x, p.y + (h - tg.y) * 0.5f, col::hint, tag);
    }
    return clicked;
}

} // namespace

bool Rows::Color(const char* label, ImU32* color, ImU32 def) {
    Start();
    float sw = 34 * u, sh = 20 * u;
    float sx = x_ + w_ - recuo_ * 0.5f - sw, sy = y_ + (rowH_ - sh) * 0.5f;
    char hex[16], defHex[16];
    HexText(*color, hex, sizeof(hex));
    HexText(def, defHex, sizeof(defHex));
    ImVec2 hs = TextSize(fonts.desc, hex);
    float hexX = sx - 10 * u - hs.x;
    bool changed = false;

    ImGui::SetCursorScreenPos(ImVec2(hexX - 4 * u, y_));
    bool clicked = ImGui::InvisibleButton("##corA", ImVec2(sx + sw - hexX + 4 * u, rowH_));
    bool chov = ImGui::IsItemHovered();
    if (chov) {
        HandCursor();
    }
    if (Restore((*color | IM_COL32_A_MASK) != (def | IM_COL32_A_MASK), hexX, defHex)) {
        *color = def;
        changed = true;
    }
    if (clicked || (Selected() && keys_.enter)) {
        *selected_ = index_;
        ImGui::OpenPopup("##corPop");
    }

    bool hovered = Row(rowH_, chov);
    Label(label, hovered);
    Text(dl_, fonts.desc, hexX, y_ + (rowH_ - hs.y) * 0.5f, chov ? col::text : col::column, hex);
    dl_->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + sw, sy + sh), *color | IM_COL32_A_MASK, 5 * u);
    dl_->AddRect(ImVec2(sx - 0.5f, sy - 0.5f), ImVec2(sx + sw + 0.5f, sy + sh + 0.5f), col::White(chov ? 70 : 40),
                 5 * u, 0, 1.0f);

    // Seletor: quadrado de saturacao/brilho, barra de matiz, RGB, o padrao e cores rapidas.
    ImGui::SetNextWindowPos(ImVec2(sx + sw, sy + sh + 6 * u), ImGuiCond_Appearing, ImVec2(1.0f, 0.0f));
    PushPopupStyle();
    if (ImGui::BeginPopup("##corPop")) {
        ImDrawList* pdl = ImGui::GetWindowDrawList();
        static float h = 0, s = 0, v = 0;
        static ImGuiID editing = 0;
        ImGuiID pid = ImGui::GetID("##corPop");
        if (ImGui::IsWindowAppearing() || editing != pid) {
            U32ToHsv(*color, &h, &s, &v);
            editing = pid;
        }
        float side = 168 * u, barW = 14 * u, pad = 8 * u;
        ImGui::Dummy(ImVec2(0, pad * 0.5f));
        ImVec2 p = ImGui::GetCursorScreenPos();
        p.x += pad * 0.5f;

        ImGui::SetCursorScreenPos(p);
        ImGui::InvisibleButton("##corSV", ImVec2(side, side));
        if (ImGui::IsItemActive()) {
            ImVec2 m = ImGui::GetIO().MousePos;
            s = std::max(0.0f, std::min(1.0f, (m.x - p.x) / side));
            v = 1.0f - std::max(0.0f, std::min(1.0f, (m.y - p.y) / side));
            HsvToU32(h, s, v, color);
            changed = true;
        }
        ImU32 hueColor;
        HsvToU32(h, 1, 1, &hueColor);
        pdl->AddRectFilledMultiColor(p, ImVec2(p.x + side, p.y + side), col::White(255), hueColor, hueColor,
                                     col::White(255));
        pdl->AddRectFilledMultiColor(p, ImVec2(p.x + side, p.y + side), 0, 0, IM_COL32(0, 0, 0, 255),
                                     IM_COL32(0, 0, 0, 255));
        pdl->AddRect(p, ImVec2(p.x + side, p.y + side), col::White(30), 0, 0, 1.0f);
        ImVec2 dot(p.x + s * side, p.y + (1 - v) * side);
        pdl->AddCircle(dot, 6 * u, IM_COL32(0, 0, 0, 160), 16, 3 * u);
        pdl->AddCircle(dot, 6 * u, col::White(255), 16, 1.5f * u);

        ImVec2 hp(p.x + side + pad, p.y);
        ImGui::SetCursorScreenPos(hp);
        ImGui::InvisibleButton("##corH", ImVec2(barW, side));
        if (ImGui::IsItemActive()) {
            h = std::max(0.0f, std::min(0.9999f, (ImGui::GetIO().MousePos.y - hp.y) / side));
            HsvToU32(h, s, v, color);
            changed = true;
        }
        for (int i = 0; i < 6; ++i) {
            ImU32 c0, c1;
            HsvToU32(i / 6.0f, 1, 1, &c0);
            HsvToU32((i + 1) / 6.0f, 1, 1, &c1);
            float y0 = hp.y + side * i / 6.0f, y1 = hp.y + side * (i + 1) / 6.0f;
            pdl->AddRectFilledMultiColor(ImVec2(hp.x, y0), ImVec2(hp.x + barW, y1), c0, c0, c1, c1);
        }
        float hy = hp.y + h * side;
        pdl->AddRect(ImVec2(hp.x - 2 * u, hy - 3 * u), ImVec2(hp.x + barW + 2 * u, hy + 3 * u), col::White(255),
                     2 * u, 0, 1.5f * u);

        float bottom = p.y + side + pad;
        char rgb[48];
        snprintf(rgb, sizeof(rgb), "R %d   G %d   B %d", (*color >> IM_COL32_R_SHIFT) & 0xFF,
                 (*color >> IM_COL32_G_SHIFT) & 0xFF, (*color >> IM_COL32_B_SHIFT) & 0xFF);
        Text(pdl, fonts.desc, p.x, bottom, col::column, rgb);
        HexText(*color, hex, sizeof(hex));
        ImVec2 hx = TextSize(fonts.desc, hex);
        Text(pdl, fonts.desc, hp.x + barW - hx.x, bottom, col::text, hex);

        // Primeira amostra = a cor padrao (com o ponto da casa); depois, cores rapidas.
        static const ImU32 presets[] = {
            IM_COL32(240, 240, 240, 255), IM_COL32(150, 150, 150, 255), IM_COL32(255, 107, 107, 255),
            IM_COL32(255, 196, 87, 255),  IM_COL32(120, 224, 143, 255), IM_COL32(87, 191, 255, 255),
            IM_COL32(178, 137, 255, 255),
        };
        constexpr int slots = 8;
        float py = bottom + fonts.desc->FontSize + pad;
        float pw = (side + pad + barW - (slots - 1) * 4 * u) / slots;
        for (int i = 0; i < slots; ++i) {
            ImU32 swatch = i == 0 ? (def | IM_COL32_A_MASK) : presets[i - 1];
            float px = p.x + i * (pw + 4 * u);
            ImGui::SetCursorScreenPos(ImVec2(px, py));
            char bid[16];
            snprintf(bid, sizeof(bid), "##pre%d", i);
            if (ImGui::InvisibleButton(bid, ImVec2(pw, 16 * u))) {
                *color = swatch;
                U32ToHsv(*color, &h, &s, &v);
                changed = true;
            }
            bool ph = ImGui::IsItemHovered();
            if (ph) {
                HandCursor();
                if (i == 0) {
                    char tip[48];
                    snprintf(tip, sizeof(tip), "Cor padr\xC3\xA3o (%s)", defHex);
                    Tooltip(tip);
                }
            }
            pdl->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + 16 * u), swatch, 4 * u);
            if (i == 0) {
                int lum = (((swatch >> IM_COL32_R_SHIFT) & 0xFF) * 3 + ((swatch >> IM_COL32_G_SHIFT) & 0xFF) * 6 +
                           ((swatch >> IM_COL32_B_SHIFT) & 0xFF)) / 10;
                pdl->AddCircleFilled(ImVec2(px + pw * 0.5f, py + 8 * u), 2 * u,
                                     lum > 140 ? IM_COL32(0, 0, 0, 150) : col::White(200), 12);
            }
            if (ph || swatch == (*color | IM_COL32_A_MASK)) {
                pdl->AddRect(ImVec2(px - 1.5f, py - 1.5f), ImVec2(px + pw + 1.5f, py + 16 * u + 1.5f),
                             col::White(200), 5 * u, 0, 1.0f);
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(p.x, py + 16 * u));
        ImGui::Dummy(ImVec2(side + pad + barW + pad * 0.5f, pad));
        // Enter que abriu o seletor nao pode fecha-lo no mesmo quadro.
        if (!ImGui::IsWindowAppearing() &&
            (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    PopPopupStyle();
    Finish(rowH_);
    return changed;
}

bool Rows::Dropdown(const char* label, int* index, const char* const* options, int count, int def) {
    Start();
    float dw = 170 * u, dh = 24 * u;
    float dx = x_ + w_ - recuo_ * 0.5f - dw, dy = y_ + (rowH_ - dh) * 0.5f;
    bool changed = false;

    ImGui::SetCursorScreenPos(ImVec2(dx, dy));
    bool clicked = ImGui::InvisibleButton("##menu", ImVec2(dw, dh));
    bool dhov = ImGui::IsItemHovered();
    if (dhov) {
        HandCursor();
    }
    if (Restore(*index != def, dx, options[def])) {
        *index = def;
        changed = true;
    }
    bool open = ImGui::IsPopupOpen("##lista");
    if (clicked || (Selected() && keys_.enter)) {
        *selected_ = index_;
        ImGui::OpenPopup("##lista");
    }
    if (Selected() && keys_.left && *index > 0) {
        --*index;
        changed = true;
    }
    if (Selected() && keys_.right && *index < count - 1) {
        ++*index;
        changed = true;
    }

    bool hovered = Row(rowH_, dhov);
    Label(label, hovered);
    bool lit = dhov || open;
    Box(dl_, ImVec2(dx, dy), ImVec2(dx + dw, dy + dh), col::White(lit ? 14 : 8),
        col::White(open ? 60 : (dhov ? 40 : 22)), 6 * u);
    ImVec2 ts = TextSize(fonts.desc, options[*index]);
    Text(dl_, fonts.desc, dx + 10 * u, dy + (dh - ts.y) * 0.5f, lit ? col::text : col::column, options[*index]);
    DrawIcon(dl_, open ? Icon::ChevronUp : Icon::ChevronDown, ImVec2(dx + dw - 14 * u, dy + dh * 0.5f),
             lit ? col::text : col::column, true);

    ImGui::SetNextWindowPos(ImVec2(dx, dy + dh + 4 * u), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(dw, 0), ImGuiCond_Always);
    PushPopupStyle();
    if (ImGui::BeginPopup("##lista")) {
        for (int i = 0; i < count; ++i) {
            ImGui::PushID(i);
            if (PopupItem(options[i], *index == i, i == def, dw - 12 * u)) {
                if (*index != i) {
                    *index = i;
                    changed = true;
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    PopPopupStyle();
    Finish(rowH_);
    return changed;
}

namespace {

// Menu do botao direito nos campos de texto (Trok Dialogs: Copiar, Colar, Recortar, Limpar).
bool FieldMenu(const char* popupId, char* buffer, size_t size) {
    bool changed = false;
    PushPopupStyle();
    if (ImGui::BeginPopup(popupId)) {
        float w = 150 * u;
        bool hasText = buffer[0] != 0;
        if (PopupItem("Copiar", false, false, w) && hasText) {
            ImGui::SetClipboardText(buffer);
        }
        if (PopupItem("Colar", false, false, w)) {
            const char* clip = ImGui::GetClipboardText();
            if (clip) {
                strncat(buffer, clip, size - strlen(buffer) - 1);
                changed = true;
            }
        }
        if (PopupItem("Recortar", false, false, w) && hasText) {
            ImGui::SetClipboardText(buffer);
            buffer[0] = 0;
            changed = true;
        }
        if (PopupItem("Limpar", false, false, w)) {
            buffer[0] = 0;
            changed = true;
        }
        ImGui::EndPopup();
    }
    PopPopupStyle();
    return changed;
}

// Campo de texto da casa. kind: 0 normal, 1 senha (olho), 2 busca (lupa + limpar).
// Canais do draw list: 0 fundo da linha, 1 moldura do campo, 2 texto do ImGui.
bool Field(ImDrawList* dl, const char* id, char* buffer, size_t size, const char* placeholder, int kind, float x,
           float y, float w, float h, bool focusNow, bool multiline, bool* outActive) {
    ImGuiStorage* st = ImGui::GetStateStorage();
    ImGuiID revealId = ImGui::GetID("##olhoEstado");
    bool reveal = st->GetBool(revealId, false);
    float iconW = kind ? 28 * u : 0.0f;
    float leftPad = kind == 2 ? 24 * u : 0.0f;

    // Botoes de icone antes do campo para ganharem o hover.
    bool iconHovered = false;
    if (kind == 1) {
        ImGui::SetCursorScreenPos(ImVec2(x + w - iconW, y));
        if (ImGui::InvisibleButton("##olho", ImVec2(iconW, h))) {
            st->SetBool(revealId, !reveal);
            reveal = !reveal;
        }
        iconHovered = ImGui::IsItemHovered();
        if (iconHovered) {
            Tooltip(reveal ? "ocultar" : "ver");
        }
    } else if (kind == 2 && buffer[0]) {
        ImGui::SetCursorScreenPos(ImVec2(x + w - iconW, y));
        if (ImGui::InvisibleButton("##limpar", ImVec2(iconW, h))) {
            buffer[0] = 0;
            ImGui::ClearActiveID();
        }
        iconHovered = ImGui::IsItemHovered();
    }
    if (iconHovered) {
        HandCursor();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10 * u, (h - fonts.desc->FontSize) * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6 * u);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushFont(fonts.desc);
    ImGui::SetCursorScreenPos(ImVec2(x + leftPad, y));
    if (focusNow) {
        ImGui::SetKeyboardFocusHere();
    }
    bool changed;
    ImGuiInputTextFlags flags = (kind == 1 && !reveal) ? ImGuiInputTextFlags_Password : 0;
    if (multiline) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10 * u, 7 * u));
        changed = ImGui::InputTextMultiline(id, buffer, size, ImVec2(w - leftPad, h), flags);
        ImGui::PopStyleVar();
    } else {
        ImGui::SetNextItemWidth(w - leftPad - iconW);
        changed = ImGui::InputText(id, buffer, size, flags);
    }
    bool active = ImGui::IsItemActive();
    bool fhov = ImGui::IsItemHovered();
    if (fhov) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
    }
    char popupId[32];
    snprintf(popupId, sizeof(popupId), "##menuCampo%s", id);
    if (fhov && ImGui::IsMouseClicked(1)) {
        ImGui::OpenPopup(popupId);
    }
    ImGui::PopFont();
    ImGui::PopStyleVar(3);
    if (FieldMenu(popupId, buffer, size)) {
        changed = true;
    }

    *outActive = active;
    dl->ChannelsSetCurrent(1);
    Box(dl, ImVec2(x, y), ImVec2(x + w, y + h), col::White(active ? 10 : 6),
        col::White(active ? 70 : (fhov || iconHovered ? 34 : 22)), 6 * u);
    if (!buffer[0] && !active && placeholder) {
        float py = multiline ? y + 7 * u : y + (h - fonts.desc->FontSize) * 0.5f;
        Text(dl, fonts.desc, x + leftPad + 10 * u, py, col::hint, placeholder);
    }
    if (kind == 1) {
        // Olho do lucide, como no Trok Dialogs: aberto = senha visivel, cortado = oculta.
        DrawIcon(dl, reveal ? Icon::Eye : Icon::EyeOff, ImVec2(x + w - iconW * 0.5f, y + h * 0.5f),
                 iconHovered ? col::text : col::hint, h < 28 * u);
    } else if (kind == 2) {
        DrawIcon(dl, Icon::Search, ImVec2(x + 13 * u, y + h * 0.5f), active ? col::text : col::hint, true);
        if (buffer[0]) {
            DrawIcon(dl, Icon::Close, ImVec2(x + w - iconW * 0.5f, y + h * 0.5f), iconHovered ? col::text : col::hint,
                     true);
        }
    }
    dl->ChannelsSetCurrent(2);
    return changed;
}

} // namespace

bool TextField(const char* id, char* buffer, size_t size, const char* placeholder, bool password, float x, float y,
               float w, float h, bool focus) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->ChannelsSplit(3);
    dl->ChannelsSetCurrent(2);
    bool active = false;
    ImGui::PushID(id);
    bool changed = Field(dl, "##campo", buffer, size, placeholder, password ? 1 : 0, x, y, w, h, focus, false, &active);
    ImGui::PopID();
    dl->ChannelsMerge();
    return changed;
}

bool Rows::Input(const char* label, char* buffer, size_t size, const char* placeholder, bool password) {
    Start();
    float fw = 240 * u, fh = 24 * u;
    float fx = x_ + w_ - recuo_ * 0.5f - fw, fy = y_ + (rowH_ - fh) * 0.5f;
    dl_->ChannelsSplit(3);
    dl_->ChannelsSetCurrent(2);
    bool active = false;
    bool changed = Field(dl_, password ? "##senha" : "##entrada", buffer, size, placeholder, password ? 1 : 0, fx, fy,
                         fw, fh, Selected() && keys_.enter, false, &active);
    if (active) {
        *selected_ = index_;
    }
    dl_->ChannelsSetCurrent(0);
    bool hovered = Row(rowH_, false);
    Label(label, hovered);
    dl_->ChannelsMerge();
    Finish(rowH_);
    return changed;
}

bool Rows::Search(const char* label, char* buffer, size_t size, const char* placeholder) {
    Start();
    float fw = 240 * u, fh = 24 * u;
    float fx = x_ + w_ - recuo_ * 0.5f - fw, fy = y_ + (rowH_ - fh) * 0.5f;
    dl_->ChannelsSplit(3);
    dl_->ChannelsSetCurrent(2);
    bool active = false;
    bool changed = Field(dl_, "##busca", buffer, size, placeholder, 2, fx, fy, fw, fh, Selected() && keys_.enter, false,
                         &active);
    if (active) {
        *selected_ = index_;
    }
    dl_->ChannelsSetCurrent(0);
    bool hovered = Row(rowH_, false);
    Label(label, hovered);
    dl_->ChannelsMerge();
    Finish(rowH_);
    return changed;
}

bool Rows::Multiline(const char* label, char* buffer, size_t size, const char* placeholder, int lines) {
    Start();
    float fw = 240 * u, fh = fonts.desc->FontSize * lines + 14 * u;
    float rowH = fh + 8 * u;
    float fx = x_ + w_ - recuo_ * 0.5f - fw, fy = y_ + 4 * u;
    dl_->ChannelsSplit(3);
    dl_->ChannelsSetCurrent(2);
    bool active = false;
    bool changed = Field(dl_, "##texto", buffer, size, placeholder, 0, fx, fy, fw, fh, Selected() && keys_.enter, true,
                         &active);
    if (active) {
        *selected_ = index_;
    }
    dl_->ChannelsSetCurrent(0);
    bool hovered = Row(rowH, false);
    Label(label, hovered); // rowH_ (28u) alinha o rotulo com a primeira linha do campo
    dl_->ChannelsMerge();
    Finish(rowH);
    return changed;
}

void Rows::Progress(const char* label, float fraction, bool indeterminate) {
    Start();
    float bw = 200 * u, bh = 6 * u, valueW = 44 * u;
    float right = x_ + w_ - recuo_ * 0.5f;
    float bx = right - valueW - 10 * u - bw, cy = y_ + rowH_ * 0.5f;
    bool hovered = Row(rowH_, false);
    Label(label, hovered);
    dl_->AddRectFilled(ImVec2(bx, cy - bh * 0.5f), ImVec2(bx + bw, cy + bh * 0.5f), col::White(16), bh * 0.5f);
    const char* text;
    char percent[16];
    if (indeterminate) {
        float t = static_cast<float>(std::fmod(ImGui::GetTime() * 0.8, 1.0));
        float seg = bw * 0.3f;
        float s0 = bx - seg + (bw + seg) * t;
        float a = std::max(bx, s0), b = std::min(bx + bw, s0 + seg);
        if (b > a) {
            dl_->AddRectFilled(ImVec2(a, cy - bh * 0.5f), ImVec2(b, cy + bh * 0.5f), col::strong, bh * 0.5f);
        }
        text = "...";
    } else {
        fraction = std::max(0.0f, std::min(1.0f, fraction));
        if (fraction > 0) {
            dl_->AddRectFilled(ImVec2(bx, cy - bh * 0.5f), ImVec2(bx + bw * fraction, cy + bh * 0.5f), col::strong,
                               bh * 0.5f);
        }
        snprintf(percent, sizeof(percent), "%d%%", static_cast<int>(fraction * 100 + 0.5f));
        text = percent;
    }
    ImVec2 ts = TextSize(fonts.desc, text);
    Text(dl_, fonts.desc, right - ts.x, cy - ts.y * 0.5f, col::column, text);
    Finish(rowH_);
}

void Rows::Loading(const char* label, const char* status) {
    Start();
    float right = x_ + w_ - recuo_ * 0.5f, cy = y_ + rowH_ * 0.5f;
    bool hovered = Row(rowH_, false);
    Label(label, hovered);
    ImVec2 ts = TextSize(fonts.desc, status);
    Text(dl_, fonts.desc, right - ts.x, cy - ts.y * 0.5f, col::column, status);
    Spinner(dl_, ImVec2(right - ts.x - 18 * u, cy), 7 * u, static_cast<float>(ImGui::GetTime()));
    Finish(rowH_);
}

void Rows::Badges(const char* label, const char* const* badges, const int* kinds, int count) {
    Start();
    float right = x_ + w_ - recuo_ * 0.5f, cy = y_ + rowH_ * 0.5f, bh = 20 * u;
    bool hovered = Row(rowH_, false);
    Label(label, hovered);
    float x = right;
    for (int i = count - 1; i >= 0; --i) {
        ImVec2 ts = TextSize(fonts.desc, badges[i]);
        float bw = ts.x + 16 * u;
        x -= bw;
        ImVec2 a(x, cy - bh * 0.5f), b(x + bw, cy + bh * 0.5f);
        ImU32 tc = col::column;
        if (kinds[i] == 1) {
            dl_->AddRectFilled(a, b, col::strong, bh * 0.5f);
            tc = col::ink;
        } else if (kinds[i] == 2) {
            Box(dl_, a, b, 0, col::White(46), bh * 0.5f);
        } else {
            dl_->AddRectFilled(a, b, col::White(16), bh * 0.5f);
        }
        Text(dl_, fonts.desc, x + 8 * u, cy - ts.y * 0.5f, tc, badges[i]);
        x -= 6 * u;
    }
    Finish(rowH_);
}

void Rows::Info(const char* label, const char* tooltip) {
    Start();
    float right = x_ + w_ - recuo_ * 0.5f, cy = y_ + rowH_ * 0.5f, r = 9 * u;
    ImGui::SetCursorScreenPos(ImVec2(right - 2 * r - 4 * u, y_));
    ImGui::InvisibleButton("##info", ImVec2(2 * r + 8 * u, rowH_));
    bool ihov = ImGui::IsItemHovered();
    bool hovered = Row(rowH_, ihov);
    Label(label, hovered);
    DrawIcon(dl_, Icon::Info, ImVec2(right - r, cy), ihov ? col::text : col::hint);
    if (ihov || (Selected() && ImGui::IsKeyDown(ImGuiKey_Enter))) {
        Tooltip(tooltip);
    }
    Finish(rowH_);
}

void Rows::Section(const char* title) {
    y_ = ImGui::GetCursorScreenPos().y;
    float h = 26 * u;
    ImVec2 ts = TextSize(fonts.desc, title);
    float ty = y_ + h - ts.y - 4 * u;
    Text(dl_, fonts.desc, x_ + recuo_, ty, col::hint, title);
    float lx = x_ + recuo_ + ts.x + 10 * u;
    dl_->AddLine(ImVec2(lx, ty + ts.y * 0.5f + 1), ImVec2(x_ + w_ - recuo_ * 0.5f, ty + ts.y * 0.5f + 1),
                 col::separator, 1.0f);
    ImGui::SetCursorScreenPos(ImVec2(x_, y_));
    ImGui::Dummy(ImVec2(w_, h));
    y_ = ImGui::GetCursorScreenPos().y;
}

bool Rows::Item(const char* text) {
    Start();
    bool hovered = Row(rowH_, false);
    bool activated = (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) || (Selected() && keys_.enter) ||
                     keys_.digit == index_;
    if (keys_.digit == index_) {
        *selected_ = index_;
    }
    Label(text, hovered);
    Finish(rowH_);
    return activated;
}

void Rows::TableHeader(const char* const* columns, const float* widths, int count) {
    y_ = ImGui::GetCursorScreenPos().y;
    float h = 24 * u, x = labelX_;
    for (int i = 0; i < count; ++i) {
        ImVec2 ts = TextSize(fonts.desc, columns[i]);
        Text(dl_, fonts.desc, x, y_ + (h - ts.y) * 0.5f, col::hint, columns[i]);
        x += widths[i] * u;
    }
    dl_->AddLine(ImVec2(x_ + recuo_, y_ + h - 0.5f), ImVec2(x_ + w_ - recuo_ * 0.5f, y_ + h - 0.5f), col::separator,
                 1.0f);
    ImGui::SetCursorScreenPos(ImVec2(x_, y_));
    ImGui::Dummy(ImVec2(w_, h + gap_));
    y_ = ImGui::GetCursorScreenPos().y;
}

bool Rows::TableItem(const char* const* cells, const float* widths, int count) {
    Start();
    bool hovered = Row(rowH_, false);
    bool activated = (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) || (Selected() && keys_.enter) ||
                     keys_.digit == index_;
    if (keys_.digit == index_) {
        *selected_ = index_;
    }
    float cy = y_ + rowH_ * 0.5f;
    Number(hovered, cy);
    float x = labelX_;
    for (int i = 0; i < count; ++i) {
        ImFont* f = i == 0 ? fonts.body : fonts.desc;
        RichText(dl_, f, x, cy - f->FontSize * 0.5f, i == 0 ? col::text : col::column, cells[i]);
        x += widths[i] * u;
    }
    Finish(rowH_);
    return activated;
}

bool Rows::CheckItem(const char* text, bool* checked) {
    Start();
    bool hovered = Row(rowH_, false);
    bool toggled = ImGui::IsItemClicked(0) || (Selected() && (keys_.enter || keys_.space)) || keys_.digit == index_;
    if (toggled) {
        *checked = !*checked;
        *selected_ = index_;
    }
    float s = 16 * u, bx = labelX_, cy = y_ + rowH_ * 0.5f;
    float t = Anim(Id("##chk"), *checked ? 1.0f : 0.0f, 20.0f);
    ImVec2 a(bx, cy - s * 0.5f), b(bx + s, cy + s * 0.5f);
    Box(dl_, a, b, col::White(6), col::White(hovered ? 80 : 60), 4 * u);
    if (t > 0.01f) {
        dl_->AddRectFilled(a, b, Fade(col::strong, t), 4 * u);
        DrawIcon(dl_, Icon::Check, ImVec2(bx + s * 0.5f, cy), Fade(col::ink, t), true);
    }
    Number(hovered, cy);
    RichText(dl_, fonts.body, bx + s + 10 * u, cy - fonts.body->FontSize * 0.5f, col::text, text);
    Finish(rowH_);
    return toggled;
}

bool Rows::RadioItem(const char* text, int* group, int value) {
    Start();
    bool hovered = Row(rowH_, false);
    bool picked = ImGui::IsItemClicked(0) || (Selected() && (keys_.enter || keys_.space)) || keys_.digit == index_;
    if (picked) {
        *group = value;
        *selected_ = index_;
    }
    float s = 16 * u, bx = labelX_, cy = y_ + rowH_ * 0.5f;
    ImVec2 c(bx + s * 0.5f, cy);
    float t = Anim(Id("##radio"), *group == value ? 1.0f : 0.0f, 20.0f);
    dl_->AddCircleFilled(c, s * 0.5f, col::White(6), 24);
    dl_->AddCircle(c, s * 0.5f - 0.5f, col::White(hovered ? 80 : 60), 24, 1.0f);
    if (t > 0.01f) {
        dl_->AddCircleFilled(c, s * 0.5f, Fade(col::strong, t), 24);
        dl_->AddCircleFilled(c, s * 0.18f * t, col::ink, 16);
    }
    Number(hovered, cy);
    RichText(dl_, fonts.body, bx + s + 10 * u, cy - fonts.body->FontSize * 0.5f, col::text, text);
    Finish(rowH_);
    return picked;
}

// ---------------------------------------------------------------- avisos

void Toast(const char* title, const char* message) {
    g_toasts.push_back({title, message, ImGui::GetTime()});
    if (g_toasts.size() > 4) {
        g_toasts.erase(g_toasts.begin());
    }
}

bool ToastsVisible() {
    return !g_toasts.empty();
}

void DrawToasts() {
    if (g_toasts.empty()) {
        return;
    }
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImGuiIO& io = ImGui::GetIO();
    double now = ImGui::GetTime();
    float w = 320 * u, margin = 24 * u, gap = 8 * u;
    float y = io.DisplaySize.y - margin;
    for (int i = static_cast<int>(g_toasts.size()) - 1; i >= 0; --i) {
        ToastItem& t = g_toasts[i];
        double age = now - t.born;
        if (age > TOAST_LIFE) {
            g_toasts.erase(g_toasts.begin() + i);
            continue;
        }
        float in = static_cast<float>(std::min(1.0, age / 0.18));
        float out = static_cast<float>(std::min(1.0, (TOAST_LIFE - age) / 0.3));
        float a = std::min(in, out);
        float slide = (1 - in) * 16 * u;

        float textW = w - 32 * u;
        ImVec2 ms = fonts.desc->CalcTextSizeA(fonts.desc->FontSize, FLT_MAX, textW, t.message.c_str());
        float h = 14 * u + fonts.body->FontSize + 4 * u + ms.y + 16 * u;
        float x = io.DisplaySize.x - margin - w + slide;
        ImVec2 a0(x, y - h), b0(x + w, y);

        ImVec2 m = io.MousePos;
        bool hov = m.x >= a0.x && m.x <= b0.x && m.y >= a0.y && m.y <= b0.y;
        if (hov && ImGui::IsMouseClicked(0)) {
            t.born = now - TOAST_LIFE + 0.3;
        }

        dl->AddRectFilled(a0, b0, Fade(IM_COL32(16, 16, 16, 245), a), 8 * u);
        dl->AddRect(a0, b0, Fade(col::White(hov ? 30 : 16), a), 8 * u, 0, 1.0f);
        dl->AddText(fonts.body, fonts.body->FontSize, ImVec2(x + 16 * u, y - h + 14 * u), Fade(col::text, a),
                    t.title.c_str());
        dl->AddText(fonts.desc, fonts.desc->FontSize, ImVec2(x + 16 * u, y - h + 18 * u + fonts.body->FontSize),
                    Fade(col::hint, a), t.message.c_str(), nullptr, textW);
        float life = static_cast<float>(1.0 - age / TOAST_LIFE);
        dl->AddRectFilled(ImVec2(x + 8 * u, y - 3 * u), ImVec2(x + 8 * u + (w - 16 * u) * life, y - 1 * u),
                          Fade(col::White(60), a), 1 * u);
        y -= h + gap;
    }
}

void Tooltip(const char* text) {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    if (g.HoveredIdTimer < 0.35f && !ImGui::IsKeyDown(ImGuiKey_Enter)) {
        return;
    }
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 m = ImGui::GetIO().MousePos;
    float maxW = 260 * u, pad = 8 * u;
    ImVec2 ts = fonts.desc->CalcTextSizeA(fonts.desc->FontSize, FLT_MAX, maxW, text);
    ImVec2 a(m.x + 14 * u, m.y + 18 * u), b(a.x + ts.x + pad * 2, a.y + ts.y + pad * 1.5f);
    ImVec2 screen = ImGui::GetIO().DisplaySize;
    if (b.x > screen.x - 4) {
        float d = b.x - screen.x + 4;
        a.x -= d;
        b.x -= d;
    }
    dl->AddRectFilled(a, b, IM_COL32(22, 22, 22, 250), 6 * u);
    dl->AddRect(a, b, col::White(18), 6 * u, 0, 1.0f);
    dl->AddText(fonts.desc, fonts.desc->FontSize, ImVec2(a.x + pad, a.y + pad * 0.75f), col::column, text, nullptr,
                maxW);
}

} // namespace tui

// Trok Tags -- desenho das pilhas em cima dos personagens: de baixo para cima, barras de vida e colete, a
// nametag (faixa na cor do jogador, nome, ID e os indicadores a direita), os textos fixos do servidor e o
// balao de fala. Medidas em u (escala da casa) vezes o Tamanho do menu; cores da paleta do kit.

#include "overlay.h"

#include "config.h"
#include "trok_ui.h"

#include <windows.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace tags {
namespace {

using namespace tui;

ImFont* g_icons = nullptr; // lucide com os icones do Trok Tags
constexpr ImWchar GLYPH_MOON = 0xE11E;

constexpr ImU32 HEALTH = IM_COL32(255, 93, 93, 255);
constexpr ImU32 HEALTH_LOW = IM_COL32(255, 61, 61, 255);
constexpr ImU32 ARMOUR = IM_COL32(233, 233, 233, 255);
constexpr ImU32 AMBER = IM_COL32(255, 196, 87, 255);
constexpr ImU32 CHIP_BLUE = IM_COL32(90, 162, 255, 255);

// Medidas do quadro (u vezes o Tamanho escolhido).
struct Metrics {
    float s = 1, k = 1;
    float nameSize = 12, smallSize = 10, chipSize = 9;
    float pillH = 20, pillR = 6, padL = 6, padR = 8, gap = 4, accW = 3, accH = 11;
    float barW = 64, healthH = 3, armourH = 2, barGap = 2;
    float stackGap = 4;
    float bubblePadX = 9, bubblePadY = 5, bubbleR = 8, tailW = 10, tailH = 5, bubbleMaxW = 236;
    float lineH = 16, chipH = 14, chipPad = 4, chipR = 4;
};

Metrics Measure() {
    Metrics m;
    m.k = cfg.size / 100.0f;
    m.s = u * m.k;
    m.nameSize = fonts.body->FontSize * m.k;
    m.smallSize = fonts.desc->FontSize * m.k;
    m.chipSize = fonts.desc->FontSize * 0.86f * m.k;
    m.pillH = 28 * m.s;
    m.pillR = 8 * m.s;
    m.padL = 8 * m.s;
    m.padR = 11 * m.s;
    m.gap = 6 * m.s;
    m.accW = 4 * m.s;
    m.accH = 15 * m.s;
    m.barW = 88 * m.s;
    m.healthH = std::max(2.0f, 4 * m.s);
    m.armourH = std::max(1.5f, 3 * m.s);
    m.barGap = std::max(1.0f, 3 * m.s);
    m.stackGap = 5 * m.s;
    m.bubblePadX = 12 * m.s;
    m.bubblePadY = 7 * m.s;
    m.bubbleR = 10 * m.s;
    m.tailW = 13 * m.s;
    m.tailH = 6 * m.s;
    m.bubbleMaxW = 326 * m.s * cfg.bubbleWidth / 100.0f;
    m.lineH = m.nameSize * 1.34f;
    m.chipH = 19 * m.s;
    m.chipPad = 6 * m.s;
    m.chipR = 5 * m.s;
    return m;
}

ImU32 Mul(ImU32 c, float a) {
    int alpha = static_cast<int>(((c >> IM_COL32_A_SHIFT) & 0xFF) * a + 0.5f);
    alpha = alpha < 0 ? 0 : alpha > 255 ? 255 : alpha;
    return (c & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}

int Red(ImU32 c) { return (c >> IM_COL32_R_SHIFT) & 0xFF; }
int Green(ImU32 c) { return (c >> IM_COL32_G_SHIFT) & 0xFF; }
int Blue(ImU32 c) { return (c >> IM_COL32_B_SHIFT) & 0xFF; }

// Cor do servidor sobre o fundo escuro: as muito escuras sobem em direcao ao branco (sem perder o tom).
ImU32 Readable(ImU32 c) {
    int r = Red(c), g = Green(c), b = Blue(c);
    float lum = 0.299f * r + 0.587f * g + 0.114f * b;
    if (lum < 110.0f) {
        float k = (110.0f - lum) / 110.0f * 0.75f;
        r += static_cast<int>((255 - r) * k);
        g += static_cast<int>((255 - g) * k);
        b += static_cast<int>((255 - b) * k);
    }
    return IM_COL32(r, g, b, 255);
}

// Tem cor de verdade (nao e branco nem cinza)?
bool Colorful(ImU32 c) {
    int hi = std::max(Red(c), std::max(Green(c), Blue(c)));
    int lo = std::min(Red(c), std::min(Green(c), Blue(c)));
    return hi - lo > 40;
}

ImU32 TagBg(float a) { return IM_COL32(12, 12, 12, static_cast<int>(173 * a)); }
ImU32 TagBorder(float a) { return col::White(static_cast<int>(19 * a)); }

void Box(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 fill, ImU32 border, float rounding) {
    dl->AddRectFilled(ImVec2(a.x, a.y + 1), ImVec2(b.x, b.y + 1), IM_COL32(0, 0, 0, (fill >> IM_COL32_A_SHIFT) / 2), rounding);
    dl->AddRectFilled(a, b, fill, rounding);
    if (border >> IM_COL32_A_SHIFT) {
        dl->AddRect(a, b, border, rounding, 0, 1.0f);
    }
}

float Width(ImFont* font, float size, const char* begin, const char* end = nullptr) {
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, begin, end).x;
}

void Text(ImDrawList* dl, ImFont* font, float size, float x, float y, ImU32 color, const char* begin,
          const char* end = nullptr, bool shadow = false) {
    if (shadow) {
        dl->AddText(font, size, ImVec2(std::floor(x) + 1, std::floor(y) + 1), Mul(IM_COL32(0, 0, 0, 200), (color >> 24) / 255.0f),
                    begin, end);
    }
    dl->AddText(font, size, ImVec2(std::floor(x), std::floor(y)), color, begin, end);
}

// Icone do lucide centrado pelo grid (metade do avanco, metade da altura), como o kit faz.
bool Glyph(ImDrawList* dl, ImWchar cp, ImVec2 c, float size, ImU32 color) {
    if (!g_icons) {
        return false;
    }
    const ImFontGlyph* glyph = g_icons->FindGlyphNoFallback(cp);
    if (!glyph) {
        return false;
    }
    float scale = size / g_icons->FontSize;
    g_icons->RenderChar(dl, size, ImVec2(c.x - glyph->AdvanceX * scale * 0.5f, c.y - size * 0.5f), color, cp);
    return true;
}

// ---------------------------------------------------------------- texto: cores, etiquetas e quebra de linha

struct Run {
    bool chip = false;
    std::string text;
    ImU32 color = 0;
};

enum ParseFlags { CHIPS = 1, WORD_CHIP = 2, OOC = 4 };

bool HexColor(const char* p, ImU32* out) {
    unsigned v = 0;
    for (int i = 0; i < 6; ++i) {
        char c = p[i];
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) {
            return false;
        }
        v = v << 4 | static_cast<unsigned>(d);
    }
    if (p[6] != '}') {
        return false;
    }
    *out = IM_COL32((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF, 255);
    return true;
}

// Maiusculas em UTF-8 (ASCII e os acentos do Latin-1), para as etiquetas.
std::string Upper(const std::string& s) {
    std::string r = s;
    for (size_t i = 0; i < r.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(r[i]);
        if (c >= 'a' && c <= 'z') {
            r[i] = static_cast<char>(c - 32);
        } else if (c == 0xC3 && i + 1 < r.size()) {
            unsigned char d = static_cast<unsigned char>(r[i + 1]);
            if (d >= 0xA0 && d <= 0xBE && d != 0xB7) {
                r[i + 1] = static_cast<char>(d - 0x20);
            }
            ++i;
        }
    }
    return r;
}

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(' ');
    if (a == std::string::npos) {
        return std::string();
    }
    size_t b = s.find_last_not_of(' ');
    return s.substr(a, b - a + 1);
}

// Quebra o texto do servidor em pedacos de texto (com a cor de cada um) e etiquetas:
// {RRGGBB} troca a cor; [conteudo] vira etiqueta; "PALAVRA:" no comeco vira etiqueta (textos fixos, como o
// "RG:"); (( ... )) vira a etiqueta OOC.
std::vector<Run> Parse(const char* text, ImU32 base, int flags) {
    std::vector<Run> runs;
    ImU32 color = base;
    std::string cur;
    auto flush = [&]() {
        if (!cur.empty()) {
            runs.push_back({false, cur, color});
            cur.clear();
        }
    };
    const char* p = text;
    bool ooc = false;
    if (flags & OOC) {
        const char* q = p;
        ImU32 c;
        while (*q == ' ' || (*q == '{' && HexColor(q + 1, &c))) {
            if (*q == '{') {
                color = c;
                q += 8;
            } else {
                ++q;
            }
        }
        if (q[0] == '(' && q[1] == '(') {
            ooc = true;
            runs.push_back({true, "OOC", color});
            p = q + 2;
            while (*p == ' ') {
                ++p;
            }
        } else {
            color = base;
        }
    }
    bool atStart = true;
    while (*p) {
        ImU32 c;
        if (*p == '{' && HexColor(p + 1, &c)) {
            flush();
            color = c;
            p += 8;
            continue;
        }
        if ((flags & CHIPS) && *p == '[') {
            const char* e = strchr(p + 1, ']');
            size_t n = e ? static_cast<size_t>(e - p - 1) : 0;
            if (e && n >= 1 && n <= 24 && !memchr(p + 1, '[', n)) {
                std::string inner = Trim(std::string(p + 1, e));
                if (!inner.empty()) {
                    flush();
                    runs.push_back({true, Upper(inner), color});
                    p = e + 1;
                    while (*p == ' ') {
                        ++p;
                    }
                    atStart = false;
                    continue;
                }
            }
        }
        if ((flags & WORD_CHIP) && atStart) {
            int n = 0;
            while (n < 8 && ((p[n] >= 'A' && p[n] <= 'Z') || (p[n] >= 'a' && p[n] <= 'z') || (p[n] >= '0' && p[n] <= '9'))) {
                ++n;
            }
            if (n >= 1 && p[n] == ':' && (p[n + 1] == ' ' || p[n + 1] == 0 || p[n + 1] == '{')) {
                flush();
                runs.push_back({true, Upper(std::string(p, n)), color});
                p += n + 1;
                while (*p == ' ') {
                    ++p;
                }
                atStart = false;
                continue;
            }
        }
        if (*p == '\n') {
            flush();
            runs.push_back({false, "\n", color});
            ++p;
            atStart = true;
            continue;
        }
        if (*p != ' ') {
            atStart = false;
        }
        cur += *p++;
    }
    flush();
    if (ooc) {
        // Tira o "))" do fim.
        for (auto it = runs.rbegin(); it != runs.rend(); ++it) {
            if (it->chip) {
                continue;
            }
            std::string t = it->text;
            while (!t.empty() && t.back() == ' ') {
                t.pop_back();
            }
            if (t.size() >= 2 && t.compare(t.size() - 2, 2, "))") == 0) {
                t.resize(t.size() - 2);
                while (!t.empty() && t.back() == ' ') {
                    t.pop_back();
                }
            }
            it->text = t;
            break;
        }
    }
    return runs;
}

struct Item {
    float x = 0, y = 0, w = 0;
    bool chip = false;
    std::string text;
    ImU32 color = 0;
};

struct Laid {
    std::vector<Item> items;
    float w = 0, h = 0, lineH = 0;
};

float ChipWidth(const Metrics& m, const std::string& text) {
    return Width(fonts.desc, m.chipSize, text.c_str()) + 2 * m.chipPad;
}

// Posiciona os pedacos em linhas. maxW <= 0: sem quebra (so nas \n).
Laid Layout(const std::vector<Run>& runs, const Metrics& m, ImFont* font, float size, float maxW) {
    Laid out;
    out.lineH = std::max(size * 1.34f, m.chipH + 2 * m.s);
    float x = 0, y = 0;
    float space = Width(font, size, " ");
    auto newline = [&]() {
        x = 0;
        y += out.lineH;
    };
    for (const Run& r : runs) {
        if (!r.chip && r.text == "\n") {
            newline();
            continue;
        }
        if (r.chip) {
            float w = ChipWidth(m, r.text);
            if (maxW > 0 && x > 0 && x + w > maxW) {
                newline();
            }
            out.items.push_back({x, y, w, true, r.text, r.color});
            out.w = std::max(out.w, x + w);
            x += w + m.chipPad;
            continue;
        }
        const char* s = r.text.c_str();
        while (*s) {
            const char* e = s;
            while (*e && *e != ' ') {
                ++e;
            }
            const char* after = e;
            while (*after == ' ') {
                ++after;
            }
            float w = Width(font, size, s, e);
            if (maxW > 0 && x > 0 && x + w > maxW) {
                newline();
            }
            if (e > s) {
                out.items.push_back({x, y, w, false, std::string(s, e), r.color});
                out.w = std::max(out.w, x + w);
                x += w;
            }
            if (x > 0) {
                x += space * static_cast<float>(after - e);
            }
            s = after;
        }
    }
    out.h = y + out.lineH;
    return out;
}

void DrawChip(ImDrawList* dl, const Metrics& m, float x, float y, float w, const std::string& text, ImU32 color, float alpha) {
    bool tinted = Colorful(color);
    ImU32 bg = tinted ? Mul(Readable(color), 0.22f) : col::White(33);
    ImU32 fg = tinted ? Readable(color) : col::text;
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + m.chipH), Mul(bg, alpha), m.chipR);
    Text(dl, fonts.desc, m.chipSize, x + m.chipPad, y + (m.chipH - m.chipSize) * 0.5f, Mul(fg, alpha), text.c_str());
}

void DrawLaid(ImDrawList* dl, const Laid& laid, const Metrics& m, ImFont* font, float size, float ox, float oy, float alpha,
              bool shadow) {
    for (const Item& it : laid.items) {
        if (it.chip) {
            DrawChip(dl, m, ox + it.x, oy + it.y + (laid.lineH - m.chipH) * 0.5f, it.w, it.text, it.color, alpha);
        } else {
            Text(dl, font, size, ox + it.x, oy + it.y + (laid.lineH - size) * 0.5f, Mul(it.color, alpha), it.text.c_str(),
                 nullptr, shadow);
        }
    }
}

// ---------------------------------------------------------------- pecas da pilha

void Track(ImDrawList* dl, float x, float y, float w, float h, float alpha) {
    dl->AddRectFilled(ImVec2(x - 1, y - 1), ImVec2(x + w + 1, y + h + 1), IM_COL32(0, 0, 0, static_cast<int>(97 * alpha)), h * 0.5f + 1);
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), col::White(static_cast<int>(41 * alpha)), h * 0.5f);
}

// Barras embaixo da nametag: vida (sempre) e colete (so com colete, em cima da vida). Devolve o topo.
float DrawBars(ImDrawList* dl, const Nametag& tag, const Metrics& m, float cx, float bottom, float alpha, float time) {
    float x0 = cx - m.barW * 0.5f;
    float hy = bottom - m.healthH;
    Track(dl, x0, hy, m.barW, m.healthH, alpha);
    float f = std::min(1.0f, std::max(0.0f, tag.health / 100.0f));
    bool low = cfg.lowHealth && tag.health > 0.0f && tag.health < 25.0f;
    ImU32 fill = HEALTH;
    if (low) {
        float pulse = 0.6f + 0.4f * (0.5f + 0.5f * std::sin(time * 5.0f));
        fill = Mul(HEALTH_LOW, pulse);
        dl->AddRectFilled(ImVec2(x0 - 2, hy - 2), ImVec2(x0 + m.barW * f + 2, hy + m.healthH + 2),
                          Mul(IM_COL32(255, 61, 61, 90), pulse * alpha), m.healthH);
    }
    if (f > 0) {
        dl->AddRectFilled(ImVec2(x0, hy), ImVec2(x0 + std::max(m.healthH, m.barW * f), hy + m.healthH), Mul(fill, alpha),
                          m.healthH * 0.5f);
    }
    float top = hy;
    if (tag.armour > 0.0f) {
        float ay = top - m.barGap - m.armourH;
        Track(dl, x0, ay, m.barW, m.armourH, alpha);
        float a = std::min(1.0f, tag.armour / 100.0f);
        dl->AddRectFilled(ImVec2(x0, ay), ImVec2(x0 + std::max(m.armourH, m.barW * a), ay + m.armourH), Mul(ARMOUR, alpha),
                          m.armourH * 0.5f);
        top = ay;
    }
    return top;
}

float BadgeBox(ImDrawList* dl, const Metrics& m, float x, float y, float h, float contentW, float alpha) {
    float w = contentW + 2 * 7 * m.s;
    if (cfg.style == 0) {
        Box(dl, ImVec2(x, y), ImVec2(x + w, y + h), TagBg(alpha), TagBorder(alpha), m.pillR);
    } else {
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(12, 12, 12, static_cast<int>(120 * alpha)), m.pillR);
    }
    return w;
}

// Indicadores a direita da nametag (digitando, AFK, ping alto). Devolve o x depois deles.
float DrawBadges(ImDrawList* dl, const Nametag& tag, const Metrics& m, float x, float y, float h, float alpha, float time) {
    float gap = 4 * m.s;
    if (cfg.typing && tag.typing) {
        float r = 2.8f * m.s, step = 9.0f * m.s;
        float w = BadgeBox(dl, m, x, y, h, 2 * step + 2 * r, alpha);
        float cx = x + 7 * m.s + r, cy = y + h * 0.5f;
        for (int i = 0; i < 3; ++i) {
            float a = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(time * 6.0f - i * 0.9f));
            dl->AddCircleFilled(ImVec2(cx + i * step, cy), r, Mul(col::column, a * alpha), 12);
        }
        x += w + gap;
    }
    if (cfg.afk && tag.afk) {
        char t[16] = "";
        if (cfg.afkTime && tag.afkSeconds >= 0) {
            int s = static_cast<int>(tag.afkSeconds);
            if (s >= 3600) {
                snprintf(t, sizeof(t), "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
            } else {
                snprintf(t, sizeof(t), "%d:%02d", s / 60, s % 60);
            }
        }
        float icon = m.smallSize;
        float tw = t[0] ? Width(fonts.desc, m.smallSize, t) + 5 * m.s : 0.0f;
        float w = BadgeBox(dl, m, x, y, h, icon + tw, alpha);
        ImVec2 c(x + 7 * m.s + icon * 0.5f, y + h * 0.5f);
        if (!Glyph(dl, GLYPH_MOON, c, icon, Mul(col::column, alpha))) {
            float r = icon * 0.38f;
            dl->AddCircleFilled(c, r, Mul(col::column, alpha), 16);
            dl->AddCircleFilled(ImVec2(c.x + r * 0.55f, c.y - r * 0.35f), r * 0.85f, IM_COL32(20, 20, 20, static_cast<int>(255 * alpha)), 16);
        }
        if (t[0]) {
            Text(dl, fonts.desc, m.smallSize, x + 7 * m.s + icon + 5 * m.s, y + (h - m.smallSize) * 0.5f, Mul(col::column, alpha), t);
        }
        x += w + gap;
    }
    if (cfg.ping && tag.ping >= cfg.pingLimit && tag.ping > 0) {
        char t[16];
        snprintf(t, sizeof(t), "%d ms", tag.ping);
        float barW = 3.5f * m.s, step = 5.5f * m.s, bars = 2 * step + barW;
        float tw = Width(fonts.desc, m.smallSize, t);
        float w = BadgeBox(dl, m, x, y, h, bars + 5 * m.s + tw, alpha);
        float bx = x + 7 * m.s, base = y + h * 0.5f + 7 * m.s;
        for (int i = 0; i < 3; ++i) {
            float bh = (5.5f + 4.2f * i) * m.s;
            ImU32 c = i == 0 ? AMBER : col::White(56);
            dl->AddRectFilled(ImVec2(bx + i * step, base - bh), ImVec2(bx + i * step + barW, base), Mul(c, alpha), 1);
        }
        Text(dl, fonts.desc, m.smallSize, bx + bars + 5 * m.s, y + (h - m.smallSize) * 0.5f, Mul(AMBER, alpha), t);
        x += w + gap;
    }
    return x;
}

// Nametag: faixa na cor do jogador, [etiqueta] do nome, nome e ID; indicadores a direita. Devolve o topo.
float DrawNametag(ImDrawList* dl, const Nametag& tag, const Metrics& m, float cx, float bottom, float alpha, bool compact,
                  float time) {
    ImFont* font = compact ? fonts.desc : fonts.body;
    float size = compact ? m.smallSize : m.nameSize;
    float h = compact ? 22 * m.s : m.pillH;
    ImU32 player = Readable(tag.color);
    bool barMode = cfg.colorMode == 0;
    std::vector<Run> runs = Parse(tag.name.c_str(), barMode ? col::text : player, cfg.nameTags ? CHIPS : 0);
    Laid laid = Layout(runs, m, font, size, 0);
    char id[16];
    snprintf(id, sizeof(id), "%d", tag.id);
    bool showId = cfg.showId && !compact;
    float idW = showId ? Width(fonts.desc, m.smallSize, id) : 0.0f;
    float acc = barMode ? m.accW + m.gap * 0.9f : 0.0f;
    float w = m.padL + acc + laid.w + (showId ? m.gap + idW : 0.0f) + m.padR;
    if (!barMode) {
        w += m.padR - m.padL; // sem a faixa, as duas margens ficam iguais
    }
    float x0 = std::floor(cx - w * 0.5f), y0 = std::floor(bottom - h);
    bool clean = cfg.style == 1;
    if (!clean) {
        Box(dl, ImVec2(x0, y0), ImVec2(x0 + w, y0 + h), TagBg(alpha), TagBorder(alpha), m.pillR);
    }
    float x = x0 + (barMode ? m.padL : m.padR);
    if (barMode) {
        float ah = compact ? m.accH * 0.8f : m.accH;
        if (clean) {
            dl->AddRectFilled(ImVec2(x - 1, y0 + (h - ah) * 0.5f - 1), ImVec2(x + m.accW + 1, y0 + (h + ah) * 0.5f + 1),
                              IM_COL32(0, 0, 0, static_cast<int>(150 * alpha)), m.accW);
        }
        dl->AddRectFilled(ImVec2(x, y0 + (h - ah) * 0.5f), ImVec2(x + m.accW, y0 + (h + ah) * 0.5f), Mul(player, alpha),
                          m.accW * 0.5f);
        x += m.accW + m.gap * 0.9f;
    }
    DrawLaid(dl, laid, m, font, size, x, y0 + (h - laid.lineH) * 0.5f, alpha, clean);
    x += laid.w;
    if (showId) {
        x += m.gap;
        Text(dl, fonts.desc, m.smallSize, x, y0 + (h - m.smallSize) * 0.5f + 1 * m.s, Mul(clean ? col::column : col::hint, alpha), id,
             nullptr, clean);
    }
    if (!compact) {
        DrawBadges(dl, tag, m, x0 + w + 4 * m.s, y0, h, alpha, time);
    }
    return y0;
}

// Texto fixo do servidor (o RG do Horizonte, o "[ADMIN] Em servico"...). Devolve o topo.
float DrawFixed(ImDrawList* dl, const FixedText& fixed, const Metrics& m, float cx, float bottom, float alpha) {
    ImU32 base = cfg.fixedText == 1 ? col::text : Readable(fixed.color);
    std::vector<Run> runs = Parse(fixed.text.c_str(), base, cfg.fixedTags ? (CHIPS | WORD_CHIP) : 0);
    for (Run& r : runs) {
        if (r.chip) {
            if (cfg.fixedChip == 1) {
                r.color = CHIP_BLUE;
            } else if (cfg.fixedChip == 2) {
                r.color = col::text;
            }
        } else if (cfg.fixedText == 1) {
            r.color = col::text;
        } else {
            r.color = Readable(r.color);
        }
    }
    Laid laid = Layout(runs, m, fonts.body, m.nameSize, 0);
    if (laid.items.empty()) {
        return bottom + m.stackGap; // texto vazio: nao ocupa lugar
    }
    float padX = 10 * m.s;
    float h = std::max(m.pillH, laid.h + 8 * m.s);
    float w = laid.w + 2 * padX;
    float x0 = std::floor(cx - w * 0.5f), y0 = std::floor(bottom - h);
    bool clean = cfg.style == 1;
    if (!clean) {
        Box(dl, ImVec2(x0, y0), ImVec2(x0 + w, y0 + h), TagBg(alpha), TagBorder(alpha), m.pillR);
    }
    DrawLaid(dl, laid, m, fonts.body, m.nameSize, x0 + padX, y0 + (h - laid.h) * 0.5f, alpha, clean);
    return y0;
}

// Balao de fala com bico embaixo. Devolve o topo.
float DrawBubble(ImDrawList* dl, const Bubble& bubble, const Metrics& m, float cx, float bottom, float alpha, float time) {
    ImU32 bg = IM_COL32(14, 14, 14, static_cast<int>(255 * cfg.bubbleOpacity / 100.0f * alpha));
    ImU32 border = TagBorder(alpha);
    float boxBottom = bottom - m.tailH;
    float w, h;
    Laid laid;
    if (bubble.typing) {
        w = 2 * 10.0f * m.s + 2 * m.bubblePadX;
        h = m.lineH * 0.9f + 2 * m.bubblePadY;
    } else {
        std::vector<Run> runs = Parse(bubble.text.c_str(), Readable(bubble.color), (cfg.bubbleTags ? CHIPS : 0) | (cfg.ooc ? OOC : 0));
        for (Run& r : runs) {
            if (!r.chip) {
                r.color = Readable(r.color);
            }
        }
        laid = Layout(runs, m, fonts.body, m.nameSize, m.bubbleMaxW - 2 * m.bubblePadX);
        if (laid.items.empty()) {
            return bottom;
        }
        w = laid.w + 2 * m.bubblePadX;
        h = laid.h + 2 * m.bubblePadY;
    }
    float x0 = std::floor(cx - w * 0.5f), y0 = std::floor(boxBottom - h);
    Box(dl, ImVec2(x0, y0), ImVec2(x0 + w, boxBottom), bg, border, m.bubbleR);
    dl->AddTriangleFilled(ImVec2(cx - m.tailW * 0.5f, boxBottom - 0.5f), ImVec2(cx + m.tailW * 0.5f, boxBottom - 0.5f),
                          ImVec2(cx, bottom), bg);
    if (bubble.typing) {
        float r = 3.2f * m.s, step = 10.0f * m.s;
        float sx = cx - step, cy = y0 + h * 0.5f;
        for (int i = 0; i < 3; ++i) {
            float a = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(time * 6.0f - i * 0.9f));
            dl->AddCircleFilled(ImVec2(sx + i * step, cy), r, Mul(col::column, a * alpha), 12);
        }
    } else {
        DrawLaid(dl, laid, m, fonts.body, m.nameSize, x0 + m.bubblePadX, y0 + m.bubblePadY, alpha, false);
    }
    return y0;
}

void DrawStack(ImDrawList* dl, const Stack& st, const Metrics& m, float time) {
    float t = st.limit > 0 ? st.distance / st.limit : 0.0f;
    float alpha = 1.0f;
    if (cfg.fade && t > 0.6f) {
        alpha = std::max(0.45f, 1.0f - (t - 0.6f) / 0.4f * 0.55f);
    }
    float cx = std::floor(st.at.x), y = st.at.y;
    if (st.hasTag) {
        bool compact = t > 0.75f;
        float tagAlpha = alpha * (cfg.afk && st.tag.afk ? 0.55f : 1.0f);
        if (cfg.bars && !compact) {
            y = DrawBars(dl, st.tag, m, cx, y, tagAlpha, time) - m.stackGap;
        }
        y = DrawNametag(dl, st.tag, m, cx, y, tagAlpha, compact, time) - m.stackGap;
    }
    for (auto it = st.fixed.rbegin(); it != st.fixed.rend(); ++it) {
        y = DrawFixed(dl, *it, m, cx, y, alpha) - m.stackGap;
    }
    if (st.hasBubble) {
        DrawBubble(dl, st.bubble, m, cx, y, alpha * st.bubble.alpha, time);
    }
}

} // namespace

void AddFonts(const char* gameDir) {
    char icons[MAX_PATH];
    snprintf(icons, sizeof(icons), "%smoonloader\\resource\\trok\\lucide.ttf", gameDir);
#ifdef TROK_UI_TEST_ICONS
    snprintf(icons, sizeof(icons), "%s", TROK_UI_TEST_ICONS);
#endif
    g_icons = nullptr;
    if (GetFileAttributesA(icons) == INVALID_FILE_ATTRIBUTES) {
        return;
    }
    static const ImWchar ranges[] = {GLYPH_MOON, GLYPH_MOON, 0};
    ImFontConfig cfgFont;
    cfgFont.OversampleH = 2;
    cfgFont.OversampleV = 2;
    cfgFont.RasterizerMultiply = 1.3f;
    g_icons = ImGui::GetIO().Fonts->AddFontFromFileTTF(icons, 16 * u, &cfgFont, ranges);
}

void DrawStacks(ImDrawList* dl, std::vector<Stack>& stacks, float time) {
    if (stacks.empty() || !fonts.body || !fonts.desc) {
        return;
    }
    // Longe primeiro: a de quem esta mais perto fica por cima.
    std::sort(stacks.begin(), stacks.end(), [](const Stack& a, const Stack& b) { return a.distance > b.distance; });
    Metrics m = Measure();
    for (const Stack& st : stacks) {
        DrawStack(dl, st, m, time);
    }
}

bool IsTypingText(const char* text) {
    std::string s;
    for (const char* p = text; *p;) {
        ImU32 c;
        if (*p == '{' && HexColor(p + 1, &c)) {
            p += 8;
            continue;
        }
        unsigned char ch = static_cast<unsigned char>(*p);
        // As reticencias de um caractere (… em UTF-8) contam como tres pontos.
        if (ch == 0xE2 && static_cast<unsigned char>(p[1]) == 0x80 && static_cast<unsigned char>(p[2]) == 0xA6) {
            s += "...";
            p += 3;
            continue;
        }
        if (strchr(" *()[]-~_", *p)) {
            ++p;
            continue;
        }
        s += static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch);
        ++p;
    }
    size_t dots = 0;
    while (!s.empty() && s.back() == '.') {
        s.pop_back();
        ++dots;
    }
    if (s.empty()) {
        return dots >= 2;
    }
    return s == "digitando" || s == "escrevendo" || s == "typing" || s == "estadigitando" || s == "est\xC3\xA1" "digitando" ||
           s == "est\xC3\xA1" "escrevendo";
}

} // namespace tags

// Trok Veiculos -- a placa: de cima para baixo, a linha do nome (pneus, modelo, ID, cadeado e a vida em numero), a
// barra da blindagem (a vida acima de 1000), a da vida do motor (cor pela fumaca do GTA) e, se pedida, a linha de
// detalhes do /dl. Medidas em u (escala da casa) vezes o Tamanho do menu; cores da paleta do kit.

#include "overlay.h"

#include "config.h"
#include "trok_ui.h"

#include <windows.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace veic {
namespace {

using namespace tui;

ImFont* g_icons = nullptr; // lucide com os icones do Trok Veiculos
constexpr ImWchar GLYPH_FLAME = 0xE0D2, GLYPH_LOCK = 0xE10B, GLYPH_SHIELD = 0xE158;

// A vida do motor muda de cor como o GTA: fumaca branca abaixo de 650, preta abaixo de 390, fogo abaixo de 250.
constexpr float SMOKE_AT = 650.0f, BLACK_SMOKE_AT = 390.0f, FIRE_AT = 250.0f, BASE_HEALTH = 1000.0f;
constexpr ImU32 HEALTH_OK = IM_COL32(232, 232, 232, 255);
constexpr ImU32 SMOKE = IM_COL32(255, 196, 87, 255);
constexpr ImU32 BLACK_SMOKE = IM_COL32(255, 138, 61, 255);
constexpr ImU32 FIRE = IM_COL32(255, 61, 61, 255);
constexpr ImU32 ARMOUR = IM_COL32(143, 184, 255, 255);
constexpr ImU32 TIRE_FLAT = IM_COL32(255, 93, 93, 255);
constexpr ImU32 DAMAGE = IM_COL32(255, 154, 154, 255);
constexpr ImU32 FLAME = IM_COL32(255, 106, 61, 255);
constexpr ImU32 HOT_TEXT = IM_COL32(255, 138, 138, 255);

struct Metrics {
    float s = 1, k = 1;
    float nameSize = 12, smallSize = 10, detailSize = 9;
    float w = 156, compactW = 108, padX = 8, padTop = 5, padBottom = 6, rowH = 15, rowGap = 4;
    float healthH = 5, armourH = 3, barGap = 2, radius = 7, gap = 4;
    float chassisW = 8, chassisH = 13, wheelW = 3, wheelH = 4;
};

Metrics Measure() {
    Metrics m;
    m.k = cfg.size / 100.0f;
    m.s = u * m.k;
    m.nameSize = fonts.body->FontSize * m.k;
    m.smallSize = fonts.desc->FontSize * m.k;
    m.detailSize = fonts.desc->FontSize * 0.92f * m.k;
    m.w = std::floor(216 * m.s);
    m.compactW = std::floor(150 * m.s);
    m.padX = 11 * m.s;
    m.padTop = 7 * m.s;
    m.padBottom = 8 * m.s;
    m.rowH = std::max(21 * m.s, m.nameSize * 1.22f);
    m.rowGap = 5 * m.s;
    m.healthH = std::max(3.0f, std::round(7 * m.s));
    m.armourH = std::max(2.0f, std::round(4 * m.s));
    m.barGap = std::max(1.0f, std::round(3 * m.s));
    m.radius = 10 * m.s;
    m.gap = 6 * m.s;
    m.chassisW = 11 * m.s;
    m.chassisH = 18 * m.s;
    m.wheelW = std::max(2.0f, 4 * m.s);
    m.wheelH = std::max(3.0f, 5.5f * m.s);
    return m;
}

ImU32 Mul(ImU32 c, float a) {
    int alpha = static_cast<int>(((c >> IM_COL32_A_SHIFT) & 0xFF) * a + 0.5f);
    alpha = alpha < 0 ? 0 : alpha > 255 ? 255 : alpha;
    return (c & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}

float Clamp01(float v) {
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

float Width(ImFont* font, float size, const char* text) {
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
}

void Text(ImDrawList* dl, ImFont* font, float size, float x, float y, ImU32 color, const char* text) {
    dl->AddText(font, size, ImVec2(std::floor(x), std::floor(y)), color, text);
}

// Icone do lucide centrado pelo grid (metade do avanco, metade da altura), como o kit faz. Devolve a largura.
float Glyph(ImDrawList* dl, ImWchar cp, float x, float cy, float size, ImU32 color) {
    if (!g_icons) {
        return 0.0f;
    }
    const ImFontGlyph* glyph = g_icons->FindGlyphNoFallback(cp);
    if (!glyph) {
        return 0.0f;
    }
    float scale = size / g_icons->FontSize;
    if (dl) {
        g_icons->RenderChar(dl, size, ImVec2(std::floor(x), std::floor(cy - size * 0.5f)), color, cp);
    }
    return glyph->AdvanceX * scale;
}

bool OnFire(float health) {
    return health < FIRE_AT;
}

ImU32 HealthColor(float health) {
    if (!cfg.stateColors) {
        return HEALTH_OK;
    }
    if (health < FIRE_AT) {
        return FIRE;
    }
    if (health < BLACK_SMOKE_AT) {
        return BLACK_SMOKE;
    }
    if (health < SMOKE_AT) {
        return SMOKE;
    }
    return HEALTH_OK;
}

// A parte que saiu fica parada um quarto de segundo e escorre em meio segundo.
float GhostValue(const Card& c) {
    if (!cfg.damageBar || c.ghost <= c.health) {
        return c.health;
    }
    float drain = Clamp01((c.ghostAge - 0.25f) / 0.5f);
    return c.ghost + (c.health - c.ghost) * drain;
}

void Bar(ImDrawList* dl, float x, float y, float w, float h, float fill, float ghost, ImU32 color, float alpha, bool tick,
         bool glow) {
    float r = h * 0.5f;
    dl->AddRectFilled(ImVec2(x - 1, y - 1), ImVec2(x + w + 1, y + h + 1), Mul(IM_COL32(0, 0, 0, 97), alpha), r + 1);
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), Mul(IM_COL32(255, 255, 255, 36), alpha), r);
    fill = Clamp01(fill);
    ghost = Clamp01(ghost);
    float fx = x + std::round(w * fill);
    if (ghost > fill) {
        dl->AddRectFilled(ImVec2(fx - r, y), ImVec2(x + std::round(w * ghost), y + h), Mul(IM_COL32(255, 255, 255, 199), alpha), r);
    }
    if (fill > 0.0f) {
        if (glow) {
            dl->AddRectFilled(ImVec2(x - 2, y - 2), ImVec2(fx + 2, y + h + 2), Mul(color, alpha * 0.28f), r + 2);
        }
        dl->AddRectFilled(ImVec2(x, y), ImVec2(std::max(fx, x + 1), y + h), Mul(color, alpha), r);
    }
    if (tick) {
        float tx = std::floor(x + w * FIRE_AT / BASE_HEALTH) + 0.5f;
        dl->AddLine(ImVec2(tx, y), ImVec2(tx, y + h), Mul(IM_COL32(0, 0, 0, 160), alpha), 1.0f);
    }
}

// O chassi visto de cima, com a frente para cima: so aparece com algum pneu furado, que fica vermelho.
float TiresWidth(const Metrics& m, int kind) {
    return kind == KIND_BIKE ? m.wheelW + 2 * m.s : m.chassisW + m.wheelW * 1.4f;
}

void Tires(ImDrawList* dl, const Metrics& m, float x, float cy, int kind, unsigned tires, float alpha) {
    ImU32 ok = Mul(IM_COL32(255, 255, 255, 82), alpha);
    ImU32 flat = Mul(TIRE_FLAT, alpha);
    float top = std::floor(cy - m.chassisH * 0.5f), bottom = top + std::round(m.chassisH);
    float r = std::max(1.0f, m.wheelW * 0.3f);
    if (kind == KIND_BIKE) {
        float cx = std::floor(x + TiresWidth(m, kind) * 0.5f);
        float half = std::max(1.0f, std::round(m.s));
        dl->AddRectFilled(ImVec2(cx - half, top + m.wheelH * 0.5f), ImVec2(cx + half, bottom - m.wheelH * 0.5f), ok);
        float ww = std::round(m.wheelW);
        dl->AddRectFilled(ImVec2(cx - ww * 0.5f - 0.5f, top - 1), ImVec2(cx + ww * 0.5f + 0.5f, top - 1 + m.wheelH),
                          (tires & TIRE_FRONT) ? flat : ok, ww * 0.5f);
        dl->AddRectFilled(ImVec2(cx - ww * 0.5f - 0.5f, bottom + 1 - m.wheelH), ImVec2(cx + ww * 0.5f + 0.5f, bottom + 1),
                          (tires & TIRE_REAR) ? flat : ok, ww * 0.5f);
        return;
    }
    float x0 = std::floor(x + m.wheelW * 0.7f), x1 = x0 + std::round(m.chassisW);
    dl->AddRect(ImVec2(x0, top), ImVec2(x1, bottom), ok, 2.5f * m.s, 0, std::max(1.0f, 1.2f * m.s));
    float ww = std::round(m.wheelW), wh = std::round(m.wheelH), inset = std::round(1.5f * m.s);
    struct Wheel {
        unsigned bit;
        bool left, front;
    } wheels[] = {{TIRE_FL, true, true}, {TIRE_FR, false, true}, {TIRE_RL, true, false}, {TIRE_RR, false, false}};
    for (const Wheel& w : wheels) {
        float wx = w.left ? x0 - ww + std::round(0.6f * m.s) : x1 - std::round(0.6f * m.s);
        float wy = w.front ? top + inset : bottom - inset - wh;
        dl->AddRectFilled(ImVec2(wx, wy), ImVec2(wx + ww, wy + wh), (tires & w.bit) ? flat : ok, r);
    }
}

bool HasTires(const Card& c) {
    return cfg.tires && c.tires && (c.kind == KIND_CAR || c.kind == KIND_BIKE);
}

bool HasRow(const Card& c) {
    return (cfg.name && !c.name.empty()) || cfg.id || cfg.healthNumber || HasTires(c) || (cfg.lock && c.locked);
}

bool HasArmour(const Card& c) {
    return cfg.armour && c.armourMax > 0.0f;
}

// "modelo 560 · 9,8 m · 4 lugares", so o que estiver ligado.
std::string Details(const Card& c) {
    std::string out;
    char buf[48];
    auto add = [&](const char* part) {
        if (!out.empty()) {
            out += " \xC2\xB7 ";
        }
        out += part;
    };
    if (cfg.detailModel && c.model) {
        snprintf(buf, sizeof(buf), "modelo %d", c.model);
        add(buf);
    }
    if (cfg.detailDistance) {
        int tenths = static_cast<int>(c.distance * 10.0f + 0.5f);
        snprintf(buf, sizeof(buf), "%d,%d m", tenths / 10, tenths % 10);
        add(buf);
    }
    if (cfg.detailSeats && c.seats > 0) {
        snprintf(buf, sizeof(buf), "%d %s", c.seats, c.seats == 1 ? "lugar" : "lugares");
        add(buf);
    }
    return out;
}

// A linha de cima: pneus, nome, ID e cadeado da esquerda para a direita, a vida em numero encostada na direita.
// Sem dl so mede: devolve a largura que a linha pede.
float NameRow(ImDrawList* dl, const Card& c, const Metrics& m, float x, float right, float cy, float alpha) {
    float start = x;
    if (HasTires(c)) {
        if (dl) {
            Tires(dl, m, x, cy, c.kind, c.tires, alpha);
        }
        x += TiresWidth(m, c.kind) + 6 * m.s;
    }
    if (cfg.name && !c.name.empty()) {
        if (dl) {
            Text(dl, fonts.body, m.nameSize, x, cy - m.nameSize * 0.5f, Mul(col::text, alpha), c.name.c_str());
        }
        x += Width(fonts.body, m.nameSize, c.name.c_str()) + 5 * m.s;
    }
    if (cfg.id) {
        char id[16];
        snprintf(id, sizeof(id), "%d", c.id);
        if (dl) {
            Text(dl, fonts.desc, m.smallSize, x, cy - m.smallSize * 0.5f + m.s, Mul(col::hint, alpha), id);
        }
        x += Width(fonts.desc, m.smallSize, id) + 5 * m.s;
    }
    if (cfg.lock && c.locked) {
        x += Glyph(dl, GLYPH_LOCK, x, cy, m.smallSize, Mul(col::hint, alpha)) + 5 * m.s;
    }
    float value = 0.0f;
    if (cfg.healthNumber) {
        bool fire = OnFire(c.health) && cfg.stateColors;
        char text[16];
        snprintf(text, sizeof(text), "%d", static_cast<int>(std::max(0.0f, c.health) + 0.5f));
        float tw = Width(fonts.desc, m.smallSize, text);
        if (dl) {
            Text(dl, fonts.desc, m.smallSize, right - tw, cy - m.smallSize * 0.5f + m.s, Mul(fire ? HOT_TEXT : col::column, alpha),
                 text);
        }
        value = tw;
        ImWchar icon = fire ? GLYPH_FLAME : HasArmour(c) && c.health > BASE_HEALTH ? GLYPH_SHIELD : 0;
        float gw = icon ? Glyph(nullptr, icon, 0, 0, m.smallSize, 0) : 0.0f;
        if (gw > 0.0f) {
            if (dl) {
                Glyph(dl, icon, right - tw - gw - 3 * m.s, cy, m.smallSize, Mul(icon == GLYPH_FLAME ? FLAME : col::hint, alpha));
            }
            value += gw + 3 * m.s;
        }
        value += 5 * m.s; // folga entre a esquerda e a vida quando a placa cresce
    }
    return x - start + value;
}

// Largura da placa: a de sempre, ou mais larga quando a linha de cima nao cabe (nome comprido, ID de 4 digitos,
// cadeado, pneus e vida com blindagem juntos).
float CardWidth(const Card& c, const Metrics& m, bool compact) {
    if (compact) {
        return m.compactW;
    }
    if (!HasRow(c)) {
        return m.w;
    }
    return std::max(m.w, std::ceil(NameRow(nullptr, c, m, 0.0f, 0.0f, 0.0f, 1.0f) + 2 * m.padX));
}

float CardHeight(const Card& c, const Metrics& m, bool compact, const std::string& details) {
    if (compact) {
        return std::round(5 * m.s) * 2 + m.healthH + (HasArmour(c) ? m.armourH + m.barGap : 0.0f);
    }
    float h = m.padTop + m.padBottom + m.healthH;
    if (HasRow(c)) {
        h += m.rowH + m.rowGap;
    }
    if (HasArmour(c)) {
        h += m.armourH + m.barGap;
    }
    if (!details.empty()) {
        h += m.detailSize * 1.45f;
    }
    return std::round(h);
}

// Desenha a placa com o canto de cima a esquerda em (x0, y0).
void DrawCard(ImDrawList* dl, const Card& c, const Metrics& m, float x0, float y0, float w, float h, bool compact,
              const std::string& details, float alpha, float time) {
    dl->AddRectFilled(ImVec2(x0, y0 + 1), ImVec2(x0 + w, y0 + h + 1), Mul(IM_COL32(0, 0, 0, 86), alpha), m.radius);
    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + w, y0 + h), Mul(IM_COL32(12, 12, 12, 173), alpha), m.radius);
    dl->AddRect(ImVec2(x0, y0), ImVec2(x0 + w, y0 + h), Mul(col::White(19), alpha), m.radius, 0, 1.0f);

    float padX = compact ? std::round(8 * m.s) : m.padX;
    float y = y0 + (compact ? std::round(5 * m.s) : m.padTop);
    float x = x0 + padX, right = x0 + w - padX;
    bool fire = OnFire(c.health);

    if (!compact && HasRow(c)) {
        NameRow(dl, c, m, x, right, y + m.rowH * 0.5f, alpha);
        y += m.rowH + m.rowGap;
    }

    float barX = x0 + padX, barW = w - 2 * padX;
    float ghost = GhostValue(c);
    if (HasArmour(c)) {
        float fill = (c.health - BASE_HEALTH) / c.armourMax;
        float ghostFill = (ghost - BASE_HEALTH) / c.armourMax;
        Bar(dl, barX, y, barW, m.armourH, fill, ghostFill, ARMOUR, alpha, false, false);
        y += m.armourH + m.barGap;
    }
    // Fogo: o vermelho pulsa (o GTA explode o carro em alguns segundos).
    float pulse = fire && cfg.stateColors ? 0.72f + 0.28f * (0.5f + 0.5f * std::sin(time * 7.0f)) : 1.0f;
    Bar(dl, barX, y, barW, m.healthH, std::min(c.health, BASE_HEALTH) / BASE_HEALTH, std::min(ghost, BASE_HEALTH) / BASE_HEALTH,
        HealthColor(c.health), alpha * pulse, cfg.fireLine, fire && cfg.stateColors);
    y += m.healthH;

    if (!compact && !details.empty()) {
        Text(dl, fonts.desc, m.detailSize, barX, y + m.detailSize * 0.3f, Mul(col::hint, alpha), details.c_str());
    }

    // O seu dano: sobe um pouco e some em menos de um segundo.
    if (cfg.damageNumber && c.damage > 0 && c.damageAge < 1.0f) {
        char text[24];
        snprintf(text, sizeof(text), "\xE2\x80\x93%d", c.damage); // travessao (a fonte da casa nao tem o sinal de menos)
        float size = m.nameSize * 1.05f;
        float rise = 12 * m.s * Clamp01(c.damageAge / 0.9f);
        float a = c.damageAge < 0.6f ? 1.0f : 1.0f - (c.damageAge - 0.6f) / 0.4f;
        float tw = Width(fonts.body, size, text);
        float tx = x0 + w - tw - 2 * m.s, ty = y0 - size - 3 * m.s - rise;
        Text(dl, fonts.body, size, tx + 1, ty + 1, Mul(IM_COL32(0, 0, 0, 200), alpha * a), text);
        Text(dl, fonts.body, size, tx, ty, Mul(DAMAGE, alpha * a), text);
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
    static const ImWchar ranges[] = {GLYPH_FLAME, GLYPH_FLAME, GLYPH_LOCK, GLYPH_LOCK, GLYPH_SHIELD, GLYPH_SHIELD, 0};
    ImFontConfig cfgFont;
    cfgFont.OversampleH = 2;
    cfgFont.OversampleV = 2;
    cfgFont.RasterizerMultiply = 1.5f;
    g_icons = ImGui::GetIO().Fonts->AddFontFromFileTTF(icons, 14.5f * u, &cfgFont, ranges);
}

void DrawCards(ImDrawList* dl, std::vector<Card>& cards, float time) {
    if (cards.empty() || !fonts.body || !fonts.desc) {
        return;
    }
    // Longe primeiro: a de quem esta mais perto fica por cima.
    std::sort(cards.begin(), cards.end(), [](const Card& a, const Card& b) { return a.distance > b.distance; });
    Metrics m = Measure();
    for (const Card& c : cards) {
        float t = c.limit > 0.0f ? c.distance / c.limit : 0.0f;
        float alpha = c.alpha * (c.empty ? 0.62f : 1.0f);
        if (cfg.fade && t > 0.75f) {
            alpha *= std::max(0.15f, 1.0f - (t - 0.75f) / 0.25f * 0.85f);
        }
        if (alpha <= 0.01f) {
            continue;
        }
        bool compact = t > 0.85f;
        std::string details = compact ? std::string() : Details(c);
        float w = CardWidth(c, m, compact);
        float h = CardHeight(c, m, compact, details);
        float x0 = std::floor(c.at.x - w * 0.5f);
        float y0 = cfg.position == 1 ? std::floor(c.at.y - m.gap - h) : std::floor(c.at.y + m.gap);
        DrawCard(dl, c, m, x0, y0, w, h, compact, details, alpha, time);
    }
}

float CardPreviewHeight(const Card& card) {
    if (!fonts.body || !fonts.desc) {
        return 0.0f;
    }
    return CardHeight(card, Measure(), false, Details(card));
}

float DrawCardPreview(ImDrawList* dl, float cx, float y, const Card& card, float time) {
    if (!fonts.body || !fonts.desc) {
        return 0.0f;
    }
    Metrics m = Measure();
    std::string details = Details(card);
    float w = CardWidth(card, m, false), h = CardHeight(card, m, false, details);
    DrawCard(dl, card, m, std::floor(cx - w * 0.5f), std::floor(y), w, h, false, details, cfg.enabled ? 1.0f : 0.35f, time);
    return h;
}

} // namespace veic

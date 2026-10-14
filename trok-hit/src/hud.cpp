// Trok Hitmarker (.asi) -- Victor_Trok
// O desenho da troca de tiros. Medidas em pixels de uma tela de 1080 de altura (k escala para a resolucao) e
// multiplicadas pelo "Tamanho" de cada coisa no menu.

#include "hud.h"

#include "trok_ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace hud {
namespace {

ImFont* g_text = nullptr;   // fonte da casa (nicks, rotulos)
ImFont* g_number = nullptr; // numeros: Arial Black ou a fonte da casa
ImFont* g_house = nullptr;  // numeros na fonte da casa
constexpr float PI = 3.14159265f;

float Clamp01(float v) {
    return v < 0 ? 0 : v > 1 ? 1 : v;
}

float EaseOut(float t) {
    t = Clamp01(t);
    return 1 - (1 - t) * (1 - t) * (1 - t);
}

ImU32 Alpha(ImU32 c, float a) {
    int al = static_cast<int>(((c >> IM_COL32_A_SHIFT) & 0xFF) * Clamp01(a) + 0.5f);
    return (c & ~IM_COL32_A_MASK) | (static_cast<ImU32>(al) << IM_COL32_A_SHIFT);
}

ImU32 Opaque(ImU32 c) {
    return c | IM_COL32_A_MASK;
}

ImVec2 Size(ImFont* f, float size, const char* text) {
    return f->CalcTextSizeA(size, 1e9f, 0, text);
}

// Texto com contorno escuro (le em cima de qualquer coisa do jogo).
void Outlined(ImDrawList* dl, ImFont* f, float size, ImVec2 p, ImU32 color, const char* text, float outline) {
    float a = ((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
    ImU32 dark = IM_COL32(0, 0, 0, static_cast<int>(190 * a));
    p = ImVec2(std::floor(p.x), std::floor(p.y));
    if (outline > 0) {
        static const float D[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {0.7f, 0.7f}, {-0.7f, 0.7f}, {0.7f, -0.7f},
                                      {-0.7f, -0.7f}};
        for (const auto& d : D) {
            dl->AddText(f, size, ImVec2(p.x + d[0] * outline, p.y + d[1] * outline), dark, text);
        }
    }
    dl->AddText(f, size, p, color, text);
}

ImFont* NumberFont(const opt::Options& o) {
    return o.numbersFont == 1 && g_house ? g_house : g_number;
}

float KindSize(Kind k) {
    return k == KILL ? 1.25f : k == HEADSHOT ? 1.12f : 1.0f;
}

// ---------------------------------------------------------------- hitmarker

void Classic(ImDrawList* dl, ImVec2 c, float r0, float r1, float th, ImU32 color, bool outline) {
    for (int i = 0; i < 4; ++i) {
        float dx = (i & 1) ? 0.7071f : -0.7071f, dy = (i & 2) ? 0.7071f : -0.7071f;
        ImVec2 a(c.x + dx * r0, c.y + dy * r0), b(c.x + dx * r1, c.y + dy * r1);
        if (outline) {
            dl->AddLine(a, b, Alpha(IM_COL32(0, 0, 0, 255), 0.55f * (((color >> 24) & 0xFF) / 255.0f)), th + 2.4f);
        }
        dl->AddLine(a, b, color, th);
    }
}

// ---------------------------------------------------------------- caveira (aviso de abate)

void Skull(ImDrawList* dl, ImVec2 c, float r, ImU32 color, ImU32 hole) {
    dl->AddCircleFilled(ImVec2(c.x, c.y - r * 0.12f), r, color, 28);
    dl->AddRectFilled(ImVec2(c.x - r * 0.62f, c.y + r * 0.3f), ImVec2(c.x + r * 0.62f, c.y + r * 1.05f), color,
                      r * 0.22f);
    dl->AddCircleFilled(ImVec2(c.x - r * 0.4f, c.y + r * 0.02f), r * 0.27f, hole, 16);
    dl->AddCircleFilled(ImVec2(c.x + r * 0.4f, c.y + r * 0.02f), r * 0.27f, hole, 16);
    dl->AddTriangleFilled(ImVec2(c.x, c.y + r * 0.32f), ImVec2(c.x - r * 0.12f, c.y + r * 0.55f),
                          ImVec2(c.x + r * 0.12f, c.y + r * 0.55f), hole);
    for (int i = -1; i <= 1; ++i) {
        dl->AddLine(ImVec2(c.x + i * r * 0.22f, c.y + r * 0.72f), ImVec2(c.x + i * r * 0.22f, c.y + r * 1.05f), hole,
                    std::max(1.0f, r * 0.09f));
    }
}

// ---------------------------------------------------------------- informer

const char* KindLabel(const Hit& h, char* buf, size_t n) {
    snprintf(buf, n, h.estimated ? "~%d" : "%d", h.value);
    return buf;
}

// Classico: o nick parado e os numeros subindo e sumindo por cima dele, como o damage informer antigo; o combo
// (x3 - 138) ao lado do numero mais novo.
void InformerClassic(ImDrawList* dl, const Line& l, ImVec2 at, bool given, const opt::Options& o, float k) {
    float s = k * o.informerSize / 100.0f;
    float fade = l.age < 2.0f ? 1.0f : 1.0f - (l.age - 2.0f) / 0.5f;
    if (fade <= 0) {
        return;
    }
    int nickMode = given ? o.nickGiven : o.nickTaken;
    ImU32 nickColor = nickMode == 0 ? Opaque(l.nickColor) : (given ? o.cGiven : o.cTaken);
    char nick[64];
    snprintf(nick, sizeof(nick), "%s [%d]", l.nick.c_str(), l.id);
    float nickSize = 17 * s;
    ImVec2 np(at.x, at.y + 0.02f * 1080 * k);
    Outlined(dl, g_text, nickSize, np, Alpha(nickColor, fade), nick, 1.3f * s);
    if (o.weapon && !l.weapon.empty()) {
        ImVec2 ns = Size(g_text, nickSize, nick);
        Outlined(dl, g_text, 14 * s, ImVec2(np.x + ns.x + 8 * s, np.y + 2 * s), Alpha(IM_COL32(205, 205, 205, 255), fade),
                 l.weapon.c_str(), 1.1f * s);
    }
    float numSize = 26 * s;
    ImFont* nf = NumberFont(o);
    for (size_t i = 0; i < l.recent.size(); ++i) {
        const Hit& h = l.recent[i];
        if (h.age > 2.0f) {
            continue;
        }
        float a = 1.0f - h.age / 2.0f;
        float y = at.y - h.age * 0.07f * 1080 * k; // a velocidade do antigo: 7% da tela por segundo
        ImU32 c = given ? KindColor(h.kind, o) : o.cTaken;
        char text[16];
        if (l.afk) {
            snprintf(text, sizeof(text), "AFK");
            c = IM_COL32(204, 204, 204, 255);
        } else {
            KindLabel(h, text, sizeof(text));
        }
        Outlined(dl, nf, numSize, ImVec2(at.x, y), Alpha(c, a), text, 1.6f * s);
        if (given && i + 1 == l.recent.size() && l.hits > 1 && !l.afk) {
            char combo[32];
            snprintf(combo, sizeof(combo), "(x%d \xC2\xB7 %d)", l.hits, l.total);
            ImVec2 ts = Size(nf, numSize, text);
            Outlined(dl, nf, numSize * 0.72f, ImVec2(at.x + ts.x + 10 * s, y + numSize * 0.2f), Alpha(o.cCombo, a),
                     combo, 1.4f * s);
        }
    }
}

// Lista: um cartao por pessoa (o mesmo alvo atualiza a linha dele), o dado de um lado e o levado do outro.
void InformerList(ImDrawList* dl, const std::vector<Line>& lines, ImVec2 at, bool given, const opt::Options& o,
                  float k) {
    float s = k * o.informerSize / 100.0f;
    float y = at.y;
    float h = 32 * s, w = 300 * s;
    for (const Line& l : lines) {
        float fade = l.age < 3.0f ? 1.0f : 1.0f - (l.age - 3.0f) / 0.4f;
        if (fade <= 0) {
            continue;
        }
        float x = given ? at.x : at.x - w;
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), Alpha(IM_COL32(14, 16, 20, 205), fade), 6 * s);
        dl->AddRectFilled(ImVec2(given ? x : x + w - 3 * s, y + 7 * s), ImVec2(given ? x + 3 * s : x + w, y + h - 7 * s),
                          Alpha(given ? o.cGiven : o.cTaken, fade), 1.5f * s);
        int nickMode = given ? o.nickGiven : o.nickTaken;
        ImU32 nc = nickMode == 0 ? Opaque(l.nickColor) : (given ? o.cGiven : o.cTaken);
        float ty = y + (h - 15 * s) * 0.5f;
        float tx = x + 13 * s;
        dl->AddText(g_text, 15 * s, ImVec2(tx, ty), Alpha(nc, fade), l.nick.c_str());
        tx += Size(g_text, 15 * s, l.nick.c_str()).x + 6 * s;
        char id[8];
        snprintf(id, sizeof(id), "%d", l.id);
        dl->AddText(g_text, 12.5f * s, ImVec2(tx, ty + 2 * s), Alpha(IM_COL32(255, 255, 255, 110), fade), id);
        tx += Size(g_text, 12.5f * s, id).x + 10 * s;
        if (o.weapon) {
            dl->AddText(g_text, 12.5f * s, ImVec2(tx, ty + 2 * s), Alpha(IM_COL32(255, 255, 255, 150), fade),
                        l.weapon.c_str());
        }
        char total[32];
        if (l.afk) {
            snprintf(total, sizeof(total), "AFK");
        } else {
            snprintf(total, sizeof(total), "%d", l.total);
        }
        ImFont* nf = NumberFont(o);
        float ns = 19 * s;
        ImVec2 ts = Size(nf, ns, total);
        float rx = x + w - 13 * s;
        char hits[16] = "";
        if (l.hits > 1) {
            snprintf(hits, sizeof(hits), "x%d", l.hits);
            ImVec2 hs = Size(g_text, 12.5f * s, hits);
            dl->AddText(g_text, 12.5f * s, ImVec2(rx - hs.x, ty + 2 * s), Alpha(IM_COL32(255, 255, 255, 120), fade), hits);
            rx -= hs.x + 8 * s;
        }
        ImU32 tc = l.afk ? IM_COL32(204, 204, 204, 255) : given ? IM_COL32(255, 255, 255, 255) : o.cTaken;
        dl->AddText(nf, ns, ImVec2(rx - ts.x, y + (h - ns) * 0.5f - 1 * s), Alpha(tc, fade), total);
        y += h + 5 * s;
    }
}

} // namespace

void AddFonts(float screenHeight, const char* houseFont, const char* numberFont) {
    ImGuiIO& io = ImGui::GetIO();
    float k = screenHeight / 1080.0f;
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x2013, 0x2026, 0};
    g_text = io.Fonts->AddFontFromFileTTF(houseFont, std::round(24 * k), &cfg, ranges);
    g_house = io.Fonts->AddFontFromFileTTF(houseFont, std::round(48 * k), &cfg, ranges);
    g_number = io.Fonts->AddFontFromFileTTF(numberFont, std::round(48 * k), &cfg, ranges);
    if (!g_number) {
        g_number = g_house;
    }
}

ImU32 KindColor(Kind k, const opt::Options& o) {
    switch (k) {
    case ARMOUR: return o.cArmour;
    case HEADSHOT: return o.cHead;
    case KILL: return o.cKill;
    default: return o.cHealth;
    }
}

void DrawMarker(ImDrawList* dl, ImVec2 c, int style, float scale, float age, Kind kind, const opt::Options& o) {
    bool kill = kind == KILL;
    float life = kill ? 0.65f : style == 1 ? 0.2f : 0.32f;
    if (age >= life || style == 3) {
        return;
    }
    float hold = kill ? 0.4f : style == 1 ? 0.08f : 0.16f;
    float a = age < hold ? 1.0f : 1.0f - (age - hold) / (life - hold);
    ImU32 color = Alpha(KindColor(kind, o), a);
    float pop = 1.0f + (kill ? 0.6f : 0.45f) * (1.0f - EaseOut(age / 0.08f));
    float big = kill ? 1.3f : kind == HEADSHOT ? 1.12f : 1.0f;
    float s = scale;
    switch (style) {
    case 0: // classico: o X do CoD
        Classic(dl, c, 8 * s * pop, 19 * s * pop * big, (kill ? 3.4f : 2.6f) * s, color, true);
        break;
    case 1: // discreto: risquinhos finos
        Classic(dl, c, 5 * s * pop, 11 * s * pop * big, 1.6f * s, color, false);
        break;
    default: { // anel que abre
        float r = (9 + 7 * EaseOut(age / life)) * s * big;
        dl->AddCircle(c, r + 1.2f * s, Alpha(IM_COL32(0, 0, 0, 255), 0.5f * a), 40, 4.2f * s);
        dl->AddCircle(c, r, color, 40, 2.2f * s);
        if (kill) {
            dl->AddCircle(c, r * 0.55f, color, 32, 2.0f * s);
        }
        if (kind == HEADSHOT) {
            dl->AddCircleFilled(c, 2.4f * s, color, 12);
        }
        break;
    }
    }
}

void DrawNumber(ImDrawList* dl, const Number& n, float scale, const opt::Options& o) {
    float life = n.kind == KILL ? 1.4f : 1.0f;
    if (n.age >= life) {
        return;
    }
    float t = n.age / life;
    float rise = (o.numbers == 1 ? 120 : 64) * scale * (1 - (1 - t) * (1 - t));
    float pop = 1.0f + 0.5f * (1.0f - EaseOut(n.age / 0.1f));
    if (n.bump < 0.14f) {
        pop += 0.28f * (1.0f - n.bump / 0.14f);
    }
    float a = n.age < life - 0.35f ? 1.0f : (life - n.age) / 0.35f;
    float size = 30 * scale * KindSize(n.kind) * pop;
    char text[16];
    snprintf(text, sizeof(text), n.estimated ? "~%d" : "%d", n.value);
    ImFont* f = NumberFont(o);
    ImVec2 ts = Size(f, size, text);
    float x = n.from.x + n.drift * 34 * scale * EaseOut(n.age / 0.6f) - ts.x * 0.5f;
    float y = n.from.y - rise - ts.y * 0.5f;
    Outlined(dl, f, size, ImVec2(x, y), Alpha(KindColor(n.kind, o), a), text, 2.0f * scale);
}

void DrawBodyMap(ImDrawList* dl, ImVec2 tl, float h, const int* zones, const opt::Options& o, float alpha) {
    float u = h / 54.0f;
    float cx = tl.x + 14 * u;
    auto zc = [&](int part) {
        int n = zones[part];
        if (n <= 0) {
            return Alpha(IM_COL32(255, 255, 255, 46), alpha);
        }
        if (part == HEAD) {
            return Alpha(o.cHead, alpha);
        }
        return Alpha(IM_COL32(255, 255, 255, std::min(255, 150 + n * 35)), alpha);
    };
    dl->AddCircleFilled(ImVec2(cx, tl.y + 5 * u), 4.6f * u, zc(HEAD), 20);
    dl->AddRectFilled(ImVec2(cx - 6 * u, tl.y + 11.5f * u), ImVec2(cx + 6 * u, tl.y + 27 * u), zc(TORSO), 2.5f * u);
    dl->AddRectFilled(ImVec2(cx - 6 * u, tl.y + 28 * u), ImVec2(cx + 6 * u, tl.y + 32 * u), zc(GROIN), 1.5f * u);
    dl->AddRectFilled(ImVec2(cx - 11 * u, tl.y + 12 * u), ImVec2(cx - 7.5f * u, tl.y + 29 * u), zc(RIGHT_ARM), 1.6f * u);
    dl->AddRectFilled(ImVec2(cx + 7.5f * u, tl.y + 12 * u), ImVec2(cx + 11 * u, tl.y + 29 * u), zc(LEFT_ARM), 1.6f * u);
    dl->AddRectFilled(ImVec2(cx - 6 * u, tl.y + 33 * u), ImVec2(cx - 1 * u, tl.y + 53 * u), zc(RIGHT_LEG), 1.6f * u);
    dl->AddRectFilled(ImVec2(cx + 1 * u, tl.y + 33 * u), ImVec2(cx + 6 * u, tl.y + 53 * u), zc(LEFT_LEG), 1.6f * u);
}

void DrawCombo(ImDrawList* dl, ImVec2 center, const Combo& c, float scale, const opt::Options& o) {
    float s = scale;
    float fade = c.kill ? (c.age < 2.2f ? 1.0f : 1.0f - (c.age - 2.2f) / 0.4f)
                        : (c.age < c.window ? 1.0f : 1.0f - (c.age - c.window) / 0.4f);
    if (fade <= 0) {
        return;
    }
    float mapW = o.bodyMap ? 30 * s : 0;
    float w = 268 * s + mapW, h = 78 * s;
    ImVec2 p(std::floor(center.x - w * 0.5f), std::floor(center.y));
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), Alpha(IM_COL32(14, 16, 20, 214), fade), 8 * s);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), Alpha(IM_COL32(255, 255, 255, 18), fade), 8 * s, 0, 1.0f);
    float pad = 12 * s;
    float x = p.x + pad, right = p.x + w - pad - mapW;

    // nick e id
    dl->AddText(g_text, 16 * s, ImVec2(x, p.y + 10 * s), Alpha(Opaque(c.nickColor), fade), c.nick.c_str());
    char id[8];
    snprintf(id, sizeof(id), "%d", c.id);
    float nx = x + Size(g_text, 16 * s, c.nick.c_str()).x + 6 * s;
    dl->AddText(g_text, 13 * s, ImVec2(nx, p.y + 12 * s), Alpha(IM_COL32(255, 255, 255, 110), fade), id);

    // o dano somado (ou o abate)
    ImFont* nf = NumberFont(o);
    char total[16], of[24];
    snprintf(total, sizeof(total), "%d", c.damage);
    snprintf(of, sizeof(of), "de %d", c.pool);
    if (c.kill) {
        const char* label = c.killHead ? "ABATE NA CABE\xC3\x87" "A" : "ABATE";
        ImU32 kc = c.killHead ? o.cHead : o.cKill;
        ImVec2 ls = Size(g_text, 14 * s, label);
        dl->AddText(g_text, 14 * s, ImVec2(right - ls.x, p.y + 12 * s), Alpha(kc, fade), label);
    } else {
        ImVec2 ts = Size(nf, 24 * s, total), os = Size(g_text, 12 * s, of);
        dl->AddText(g_text, 12 * s, ImVec2(right - os.x, p.y + 14 * s), Alpha(IM_COL32(255, 255, 255, 120), fade), of);
        dl->AddText(nf, 24 * s, ImVec2(right - os.x - 6 * s - ts.x, p.y + 6 * s), Alpha(IM_COL32(255, 255, 255, 255), fade),
                    total);
    }

    // barra: colete (cinza claro) + vida (vermelho), proporcional ao que o alvo tinha quando o combo comecou
    float by = p.y + 36 * s, bh = 8 * s, bw = right - x;
    float pool = static_cast<float>(std::max(c.pool, c.health + c.armour + c.ghost));
    dl->AddRectFilled(ImVec2(x, by), ImVec2(x + bw, by + bh), Alpha(IM_COL32(255, 255, 255, 22), fade), bh * 0.5f);
    float hx = x + bw * c.health / pool;
    float ax = hx + bw * c.armour / pool;
    if (c.health > 0) {
        dl->AddRectFilled(ImVec2(x, by), ImVec2(hx, by + bh), Alpha(IM_COL32(229, 72, 77, 255), fade), bh * 0.5f,
                          c.armour > 0 ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersAll);
    }
    if (c.armour > 0) {
        dl->AddRectFilled(ImVec2(hx, by), ImVec2(ax, by + bh), Alpha(IM_COL32(214, 222, 234, 255), fade), bh * 0.5f,
                          c.health > 0 ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersAll);
    }
    if (c.ghost > 0 && c.ghostAge < 0.6f) {
        float gx = ax + bw * c.ghost / pool;
        float ga = 1.0f - c.ghostAge / 0.6f;
        dl->AddRectFilled(ImVec2(ax, by + bh * (0.5f - 0.5f * ga)), ImVec2(gx, by + bh * (0.5f + 0.5f * ga)),
                          Alpha(IM_COL32(255, 255, 255, 230), fade * ga), 1.5f * s);
    }

    // acertos, cabeca e armas
    char info[96];
    if (c.heads > 0) {
        snprintf(info, sizeof(info), "%d acerto%s \xC2\xB7 %d na cabe\xC3\xA7" "a \xC2\xB7 %s", c.hits, c.hits == 1 ? "" : "s",
                 c.heads, c.weapons.c_str());
    } else {
        snprintf(info, sizeof(info), "%d acerto%s \xC2\xB7 %s", c.hits, c.hits == 1 ? "" : "s", c.weapons.c_str());
    }
    dl->AddText(g_text, 12.5f * s, ImVec2(x, p.y + 52 * s), Alpha(IM_COL32(255, 255, 255, 150), fade), info);

    // tempo que falta para o combo fechar
    if (!c.kill) {
        float left = Clamp01(1.0f - c.age / c.window);
        dl->AddRectFilled(ImVec2(p.x + 8 * s, p.y + h - 3 * s), ImVec2(p.x + 8 * s + (w - 16 * s) * left, p.y + h - 1.5f * s),
                          Alpha(IM_COL32(255, 255, 255, 70), fade), 1.0f);
    }
    if (o.bodyMap) {
        DrawBodyMap(dl, ImVec2(p.x + w - pad - mapW + 4 * s, p.y + 12 * s), 54 * s, c.zones, o, fade);
    }
}

void Draw(ImDrawList* dl, const State& st, const opt::Options& o, ImVec2 screen) {
    if (!o.enabled) {
        return;
    }
    float k = screen.y / 1080.0f;

    // borda vermelha ao levar dano
    if (o.edgeFlash && st.flash > 0) {
        ImU32 red = IM_COL32(200, 20, 30, static_cast<int>(100 * Clamp01(st.flash)));
        ImU32 none = IM_COL32(200, 20, 30, 0);
        float ew = screen.x * 0.11f, eh = screen.y * 0.13f;
        dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(ew, screen.y), red, none, none, red);
        dl->AddRectFilledMultiColor(ImVec2(screen.x - ew, 0), screen, none, red, red, none);
        dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(screen.x, eh), red, red, none, none);
        dl->AddRectFilledMultiColor(ImVec2(0, screen.y - eh), screen, none, none, red, red);
    }

    // de onde veio o tiro
    if (o.direction) {
        for (const Incoming& in : st.incoming) {
            if (in.age >= 1.6f) {
                continue;
            }
            float a = in.age < 0.6f ? 1.0f : 1.0f - (in.age - 0.6f);
            a *= 0.55f + 0.45f * Clamp01(in.damage / 46.0f);
            float r = 165 * k, span = 0.36f;
            ImVec2 c = st.crosshair;
            float a0 = in.angle - span - PI / 2, a1 = in.angle + span - PI / 2;
            dl->PathArcTo(c, r, a0, a1, 32);
            dl->PathStroke(Alpha(IM_COL32(255, 40, 40, 255), 0.22f * a), 0, 16 * k);
            dl->PathArcTo(c, r, a0, a1, 32);
            dl->PathStroke(Alpha(IM_COL32(255, 60, 56, 255), 0.9f * a), 0, 6 * k);
            ImVec2 dir(std::sin(in.angle), -std::cos(in.angle));
            ImVec2 tip(c.x + dir.x * (r + 16 * k), c.y + dir.y * (r + 16 * k));
            ImVec2 side(-dir.y, dir.x);
            dl->AddTriangleFilled(tip, ImVec2(c.x + dir.x * (r + 5 * k) + side.x * 9 * k, c.y + dir.y * (r + 5 * k) + side.y * 9 * k),
                                  ImVec2(c.x + dir.x * (r + 5 * k) - side.x * 9 * k, c.y + dir.y * (r + 5 * k) - side.y * 9 * k),
                                  Alpha(IM_COL32(255, 60, 56, 255), 0.9f * a));
            if (o.directionName && !in.nick.empty()) {
                char label[64];
                snprintf(label, sizeof(label), "%s  %s", in.nick.c_str(), in.weapon.c_str());
                ImVec2 ts = Size(g_text, 15 * k, label);
                ImVec2 lp(c.x + dir.x * (r + 42 * k) - ts.x * 0.5f, c.y + dir.y * (r + 40 * k) - ts.y * 0.5f);
                Outlined(dl, g_text, 15 * k, lp, Alpha(Opaque(in.nickColor), a), label, 1.3f * k);
            }
        }
    }

    // numeros
    float numScale = k * o.numbersSize / 100.0f;
    if (o.numbers != 2) {
        for (const Number& n : st.numbers) {
            DrawNumber(dl, n, numScale, o);
        }
    }

    // informer
    ImVec2 gAt(o.given.x * screen.x, o.given.y * screen.y), tAt(o.taken.x * screen.x, o.taken.y * screen.y);
    if (o.informer == 0) {
        if (!st.given.empty()) {
            InformerClassic(dl, st.given.front(), gAt, true, o, k);
        }
        if (!st.taken.empty()) {
            InformerClassic(dl, st.taken.front(), tAt, false, o, k);
        }
    } else if (o.informer == 1) {
        InformerList(dl, st.given, gAt, true, o, k);
        InformerList(dl, st.taken, ImVec2(tAt.x + 300 * k * o.informerSize / 100.0f, tAt.y), false, o, k);
    }

    // combo no alvo
    if (o.combo && st.hasCombo) {
        DrawCombo(dl, ImVec2(o.comboAt.x * screen.x, o.comboAt.y * screen.y), st.combo, k, o);
    }

    // hitmarker
    float ms = k * o.markerSize / 100.0f;
    for (const Marker& m : st.markers) {
        if (o.markerWhere != 1) {
            DrawMarker(dl, st.crosshair, o.marker, ms, m.age, m.kind, o);
        }
        if (o.markerWhere != 0) {
            DrawMarker(dl, m.body, o.marker, ms * 0.8f, m.age, m.kind, o);
        }
    }

    // aviso de abate
    if (o.killBanner && st.hasBanner && st.banner.age < 2.6f) {
        const Banner& b = st.banner;
        float a = b.age < 2.2f ? 1.0f : 1.0f - (b.age - 2.2f) / 0.4f;
        float pop = 1.0f + 0.25f * (1.0f - EaseOut(b.age / 0.12f));
        ImVec2 c(o.banner.x * screen.x, o.banner.y * screen.y);
        ImU32 accent = b.head ? o.cHead : o.cKill;
        Skull(dl, ImVec2(c.x, c.y - 18 * k * pop), 15 * k * pop, Alpha(accent, a), Alpha(IM_COL32(14, 16, 20, 255), a));
        const char* title = b.head ? "ABATE NA CABE\xC3\x87" "A" : "ABATE";
        ImVec2 ts = Size(g_text, 20 * k, title);
        Outlined(dl, g_text, 20 * k, ImVec2(c.x - ts.x * 0.5f, c.y + 6 * k), Alpha(IM_COL32(255, 255, 255, 255), a), title,
                 1.3f * k);
        char who[64];
        snprintf(who, sizeof(who), "%s \xC2\xB7 %s", b.nick.c_str(), b.weapon.c_str());
        ImVec2 ws = Size(g_text, 15 * k, who);
        Outlined(dl, g_text, 15 * k, ImVec2(c.x - ws.x * 0.5f, c.y + 31 * k), Alpha(Opaque(b.nickColor), a), who, 1.2f * k);
        if (o.killStreak) {
            for (int i = 0; i < 5; ++i) {
                ImVec2 d(c.x + (i - 2) * 16 * k, c.y + 60 * k);
                float r = 4.5f * k;
                ImU32 col = i < b.streak ? Alpha(accent, a) : Alpha(IM_COL32(255, 255, 255, 60), a);
                dl->AddQuadFilled(ImVec2(d.x, d.y - r), ImVec2(d.x + r, d.y), ImVec2(d.x, d.y + r), ImVec2(d.x - r, d.y), col);
            }
        }
    }

    // resumo da morte
    if (o.deathRecap && st.hasRecap) {
        const Recap& r = st.recap;
        float a = EaseOut(r.age / 0.25f);
        float w = 440 * k, h = 150 * k;
        ImVec2 p(std::floor(o.recap.x * screen.x - w * 0.5f), std::floor(o.recap.y * screen.y - h * 0.5f));
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), Alpha(IM_COL32(14, 16, 20, 225), a), 10 * k);
        dl->AddRect(p, ImVec2(p.x + w, p.y + h), Alpha(IM_COL32(255, 255, 255, 18), a), 10 * k, 0, 1.0f);
        float x = p.x + 16 * k, y = p.y + 13 * k;
        dl->AddText(g_text, 12 * k, ImVec2(x, y), Alpha(IM_COL32(255, 255, 255, 120), a), "VOC\xC3\x8A MORREU PARA");
        y += 17 * k;
        dl->AddText(g_text, 19 * k, ImVec2(x, y), Alpha(Opaque(r.killerColor), a), r.killer.c_str());
        float tx = x + Size(g_text, 19 * k, r.killer.c_str()).x + 10 * k;
        dl->AddText(g_text, 13.5f * k, ImVec2(tx, y + 4 * k), Alpha(IM_COL32(255, 255, 255, 160), a), r.weapon.c_str());
        tx += Size(g_text, 13.5f * k, r.weapon.c_str()).x + 10 * k;
        if (r.head) {
            const char* hs = "na cabe\xC3\xA7" "a";
            ImVec2 hsz = Size(g_text, 12 * k, hs);
            dl->AddRectFilled(ImVec2(tx, y + 2 * k), ImVec2(tx + hsz.x + 12 * k, y + 21 * k), Alpha(o.cHead, a * 0.18f), 4 * k);
            dl->AddText(g_text, 12 * k, ImVec2(tx + 6 * k, y + 5 * k), Alpha(o.cHead, a), hs);
        }
        y += 34 * k;
        ImFont* nf = NumberFont(o);
        auto column = [&](float cx, const char* label, int value, const char* sub, ImU32 c) {
            dl->AddText(g_text, 12 * k, ImVec2(cx, y), Alpha(IM_COL32(255, 255, 255, 120), a), label);
            char v[16];
            snprintf(v, sizeof(v), "%d", value);
            dl->AddText(nf, 26 * k, ImVec2(cx, y + 14 * k), Alpha(c, a), v);
            float vx = cx + Size(nf, 26 * k, v).x + 8 * k;
            dl->AddText(g_text, 12.5f * k, ImVec2(vx, y + 24 * k), Alpha(IM_COL32(255, 255, 255, 140), a), sub);
        };
        char gsub[32], tsub[48];
        snprintf(gsub, sizeof(gsub), "%d acertos", r.givenHits);
        if (r.takenHeads > 0) {
            snprintf(tsub, sizeof(tsub), "%d acertos \xC2\xB7 %d na cabe\xC3\xA7" "a", r.takenHits, r.takenHeads);
        } else {
            snprintf(tsub, sizeof(tsub), "%d acertos", r.takenHits);
        }
        column(x, "VOC\xC3\x8A DEU", r.given, gsub, o.cGiven);
        column(x + w * 0.47f, "VOC\xC3\x8A LEVOU", r.taken, tsub, o.cTaken);
        y += 52 * k;
        float cx = x;
        for (const std::string& hstr : r.hits) {
            ImVec2 hsz = Size(g_text, 12 * k, hstr.c_str());
            dl->AddRectFilled(ImVec2(cx, y), ImVec2(cx + hsz.x + 12 * k, y + 19 * k), Alpha(IM_COL32(255, 255, 255, 16), a), 4 * k);
            dl->AddText(g_text, 12 * k, ImVec2(cx + 6 * k, y + 3 * k), Alpha(IM_COL32(255, 255, 255, 170), a), hstr.c_str());
            cx += hsz.x + 18 * k;
        }
    }
}

} // namespace hud

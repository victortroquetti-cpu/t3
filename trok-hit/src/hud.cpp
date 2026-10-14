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
        if (given && o.combo && i + 1 == l.recent.size() && l.hits > 1 && !l.afk) {
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
    float h = 32 * s, w = 250 * s;
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

void DrawMarker(ImDrawList* dl, ImVec2 c, int style, float scale, float age, Kind kind, const opt::Options& o,
                float stay) {
    bool kill = kind == KILL;
    float life = kill ? 0.65f : style == 1 ? 0.2f : 0.32f;
    float hold = kill ? 0.4f : style == 1 ? 0.08f : 0.16f;
    if (stay > 0) {
        life = stay;
        hold = std::max(0.1f, stay - 0.35f);
    }
    if (age >= life) {
        return;
    }
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
        float r = (9 + 7 * EaseOut(age / std::min(life, 0.32f))) * s * big;
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
                const char* label = in.nick.c_str();
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
        InformerList(dl, st.taken, ImVec2(tAt.x + 250 * k * o.informerSize / 100.0f, tAt.y), false, o, k);
    }

    // local do acerto: a marca fica na parte do corpo que o tiro pegou (o samp.cpp atualiza a posicao do osso)
    if (o.hitLocal) {
        float bs = k * o.markerSize / 100.0f * o.hitLocalSize / 100.0f;
        for (const Marker& m : st.markers) {
            if (m.bodyVisible) {
                DrawMarker(dl, m.body, o.marker, bs, m.age, m.kind, o, static_cast<float>(o.hitLocalTime));
            }
        }
    }

    // sinal de acerto na mira (onde a mira esta neste quadro)
    if (o.onAim) {
        float ms = k * o.markerSize / 100.0f;
        for (const Marker& m : st.markers) {
            DrawMarker(dl, st.crosshair, o.marker, ms, m.age, m.kind, o);
        }
    }
}

} // namespace hud

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
struct Img {
    ImTextureID texture = nullptr;
    ImVec2 size;
} g_images[IMG_COUNT];

// Desenha a imagem centrada em c (tamanho da imagem a 1080 vezes scale); false se a imagem nao existe.
bool Picture(ImDrawList* dl, int slot, ImVec2 c, float scale, float alpha) {
    const Img& im = g_images[slot];
    if (!im.texture) {
        return false;
    }
    ImVec2 h(im.size.x * scale * 0.5f, im.size.y * scale * 0.5f);
    dl->AddImage(im.texture, ImVec2(c.x - h.x, c.y - h.y), ImVec2(c.x + h.x, c.y + h.y), ImVec2(0, 0), ImVec2(1, 1),
                 IM_COL32(255, 255, 255, static_cast<int>(255 * (alpha < 0 ? 0 : alpha > 1 ? 1 : alpha))));
    return true;
}
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
    if (!given) {
        np.x -= Size(g_text, nickSize, nick).x; // o levado termina no ponto, simetrico ao dado
    }
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
        float nx = given ? at.x : at.x - Size(nf, numSize, text).x;
        Outlined(dl, nf, numSize, ImVec2(nx, y), Alpha(c, a), text, 1.6f * s);
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

void SetImage(int slot, ImTextureID texture, ImVec2 size) {
    if (slot >= 0 && slot < IMG_COUNT) {
        g_images[slot].texture = texture;
        g_images[slot].size = size;
    }
}

// Tempos do Ponto: o abate fica mais (o "morte" da Mira Centralizada durava 150 quadros).
static void PontoTiming(Kind kind, float* life, float* hold) {
    *life = kind == KILL ? 1.2f : kind == HEADSHOT ? 0.55f : 0.35f;
    *hold = kind == KILL ? 0.85f : kind == HEADSHOT ? 0.3f : 0.15f;
}

static ImU32 Mix(ImU32 a, ImU32 b, float t) {
    t = Clamp01(t);
    int r = static_cast<int>(((a >> IM_COL32_R_SHIFT) & 0xFF) * (1 - t) + ((b >> IM_COL32_R_SHIFT) & 0xFF) * t);
    int g = static_cast<int>(((a >> IM_COL32_G_SHIFT) & 0xFF) * (1 - t) + ((b >> IM_COL32_G_SHIFT) & 0xFF) * t);
    int bl = static_cast<int>(((a >> IM_COL32_B_SHIFT) & 0xFF) * (1 - t) + ((b >> IM_COL32_B_SHIFT) & 0xFF) * t);
    return IM_COL32(r, g, bl, 255);
}

// O ponto do Trok (ou a imagem da mira). tint/tintAmount: a cor do acerto que o ponto toma por um instante; grow:
// o pulo. E o mesmo ponto da mira, nunca um segundo por cima.
void DrawCrosshair(ImDrawList* dl, ImVec2 c, float scale, const opt::Options& o, ImU32 tint, float tintAmount,
                   float grow) {
    if (o.crosshair == 2 && Picture(dl, IMG_CROSSHAIR, c, scale * o.crosshairSize / 100.0f * (1 + grow), 1.0f)) {
        return;
    }
    float r = 3.4f * scale * o.crosshairSize / 100.0f * (1 + grow);
    ImU32 color = tintAmount > 0 ? Mix(o.cCrosshair, tint, tintAmount) : Opaque(o.cCrosshair);
    dl->AddCircleFilled(c, r + 1.6f * scale, IM_COL32(0, 0, 0, 200), 20);
    dl->AddCircleFilled(c, r, color, 20);
}

// Ponto: so os detalhes em volta da mira (o ponto em si e o da mira, que muda de cor). Acerto: um anel abrindo.
// Cabeca: quatro tracos em cruz e o anel. Abate: o X na cor do abate.
static void PontoDetails(ImDrawList* dl, ImVec2 c, float s, float age, float life, float a, Kind kind,
                         const opt::Options& o) {
    ImU32 color = Alpha(KindColor(kind, o), a);
    ImU32 dark = Alpha(IM_COL32(0, 0, 0, 255), 0.75f * a);
    float rd = 3.6f * s;
    float pop = 1.0f - EaseOut(age / 0.1f); // 1 -> 0 no primeiro decimo de segundo
    if (kind == KILL) {
        float r0 = rd + 3.5f * s, r1 = rd + (11 + 5 * pop) * s;
        for (int i = 0; i < 4; ++i) {
            float dx = (i & 1) ? 0.7071f : -0.7071f, dy = (i & 2) ? 0.7071f : -0.7071f;
            ImVec2 p0(c.x + dx * r0, c.y + dy * r0), p1(c.x + dx * r1, c.y + dy * r1);
            dl->AddLine(p0, p1, dark, 6.2f * s);
            dl->AddLine(p0, p1, color, 3.6f * s);
        }
        return;
    }
    if (kind == HEADSHOT) {
        float r0 = rd + 2.8f * s, r1 = rd + (8.5f + 4 * pop) * s;
        for (int i = 0; i < 4; ++i) {
            float dx = i == 0 ? 1.0f : i == 1 ? -1.0f : 0.0f, dy = i == 2 ? 1.0f : i == 3 ? -1.0f : 0.0f;
            ImVec2 p0(c.x + dx * r0, c.y + dy * r0), p1(c.x + dx * r1, c.y + dy * r1);
            dl->AddLine(p0, p1, dark, 5.6f * s);
            dl->AddLine(p0, p1, color, 3.0f * s);
        }
    }
    float ring = EaseOut(age / life);
    dl->AddCircle(c, rd + 2 * s + 10 * s * ring, Alpha(KindColor(kind, o), a * (1.0f - ring) * 0.8f), 32, 1.6f * s);
}

void DrawMarker(ImDrawList* dl, ImVec2 c, int style, float scale, float age, Kind kind, const opt::Options& o,
                float stay);

void DrawAimTint(ImDrawList* dl, ImVec2 c, float scale, float age, Kind kind, const opt::Options& o) {
    float life, hold;
    PontoTiming(kind, &life, &hold);
    if (!o.onAim || o.marker != 0 || o.crosshair != 1 || kind == KILL || age >= life) {
        DrawCrosshair(dl, c, scale, o);
        return;
    }
    float a = age < hold ? 1.0f : 1.0f - (age - hold) / (life - hold);
    float pop = 0.6f * (1.0f - EaseOut(age / 0.1f));
    DrawCrosshair(dl, c, scale, o, KindColor(kind, o), a, pop);
}

void DrawAimPreview(ImDrawList* dl, ImVec2 c, float scale, float age, Kind kind, const opt::Options& o) {
    if (o.crosshair >= 1) {
        DrawAimTint(dl, c, scale, age, kind, o);
    } else {
        dl->AddCircle(c, 11 * scale, IM_COL32(255, 255, 255, 150), 32, 1.4f * scale); // a do jogo, so para lembrar
    }
    DrawMarker(dl, c, o.marker, scale * o.markerSize / 100.0f, age, kind, o, 0);
}

void DrawMarker(ImDrawList* dl, ImVec2 c, int style, float scale, float age, Kind kind, const opt::Options& o,
                float stay) {
    bool kill = kind == KILL;
    float life, hold;
    if (style == 4) { // imagens: mesmos tempos e o mesmo pulo do Ponto; sem a imagem, o Ponto
        int slot = kind == KILL ? IMG_KILL : kind == HEADSHOT ? IMG_HEAD : kind == ARMOUR ? IMG_ARMOUR : IMG_HIT;
        if (slot == IMG_ARMOUR && !g_images[IMG_ARMOUR].texture) {
            slot = IMG_HIT;
        }
        if (!g_images[slot].texture) {
            style = 0;
        } else {
            life = kill ? 1.2f : kind == HEADSHOT ? 0.55f : 0.35f;
            hold = kill ? 0.85f : kind == HEADSHOT ? 0.3f : 0.15f;
            if (age >= life) {
                return;
            }
            float a = age < hold ? 1.0f : 1.0f - (age - hold) / (life - hold);
            float pop = 1.0f + 0.35f * (1.0f - EaseOut(age / 0.1f));
            Picture(dl, slot, c, scale * pop, a);
            return;
        }
    }
    if (style == 0) {
        PontoTiming(kind, &life, &hold);
    } else {
        life = kill ? 0.65f : style == 2 ? 0.2f : 0.32f;
        hold = kill ? 0.4f : style == 2 ? 0.08f : 0.16f;
    }
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
    case 0:
        PontoDetails(dl, c, s, age, life, a, kind, o);
        break;
    case 1: // classico: o X do CoD
        Classic(dl, c, 8 * s * pop, 19 * s * pop * big, (kill ? 3.4f : 2.6f) * s, color, true);
        break;
    case 2: // discreto: risquinhos finos
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

    // borda da tela ao levar dano: vida, colete ou cabeca, cada um na sua cor
    if (o.edgeFlash && st.flash > 0) {
        ImU32 base = st.flashKind == ARMOUR ? o.cEdgeArmour : st.flashKind == HEADSHOT ? o.cEdgeHead : o.cEdgeHealth;
        ImU32 red = Alpha(Opaque(base), (st.flashKind == ARMOUR ? 0.55f : 0.4f) * Clamp01(st.flash));
        ImU32 none = Alpha(base, 0);
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
            ImVec2 c(screen.x * 0.5f, screen.y * 0.5f); // centro da tela, onde quer que a mira esteja
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
        }
    }

    // numeros
    float numScale = k * o.numbersSize / 100.0f;
    if (o.numbers == 0) {
        for (const Number& n : st.numbers) {
            DrawNumber(dl, n, numScale, o);
        }
    } else if (o.numbers == 1) {
        for (Number n : st.numbers) { // na tela: sobem do lado direito do centro
            n.from = ImVec2(screen.x * 0.5f + 46 * numScale, screen.y * 0.5f - 12 * numScale);
            n.drift = 0;
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
        InformerList(dl, st.taken, tAt, false, o, k);
    }

    // local do acerto: a marca fica na parte do corpo que o tiro pegou (o samp.cpp atualiza a posicao do osso)
    if (o.hitLocal) {
        float bs = k * o.markerSize / 100.0f * o.hitLocalSize / 100.0f;
        for (const Marker& m : st.markers) {
            if (m.bodyVisible) {
                DrawMarker(dl, m.body, 1, bs, m.age, m.kind, o, static_cast<float>(o.hitLocalTime)); // sempre o X
            }
        }
    }

    // o ponto do Trok (no lugar da mira do jogo) e o sinal de acerto, onde a mira esta neste quadro
    if (o.crosshair >= 1 && st.showCrosshair) {
        const Marker* latest = nullptr;
        for (const Marker& m : st.markers) {
            if (!latest || m.age < latest->age) {
                latest = &m;
            }
        }
        DrawAimTint(dl, st.crosshair, k, latest ? latest->age : 99.0f, latest ? latest->kind : HEALTH, o);
    }
    if (o.onAim) {
        float ms = k * o.markerSize / 100.0f;
        for (const Marker& m : st.markers) {
            DrawMarker(dl, st.crosshair, o.marker, ms, m.age, m.kind, o);
        }
    }
}

} // namespace hud

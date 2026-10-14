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
struct Loaded {
    ImTextureID texture = nullptr;
    ImVec2 uv = ImVec2(1, 1);
    float aspect = 1.0f;
};
Loaded g_images[IMG_COUNT];
constexpr float AIM_BOX = 40.0f;  // px numa tela de 1080: mira e efeitos (todos na mesma caixa, um no lugar do outro)
constexpr float BODY_BOX = 48.0f; // o X do corpo

// A imagem inteira numa caixa quadrada de box px, centrada em c.
void Picture(ImDrawList* dl, int slot, ImVec2 c, float box, float alpha) {
    const Loaded& p = g_images[slot];
    if (!p.texture || alpha <= 0) {
        return;
    }
    float hw = box * 0.5f, hh = box * 0.5f;
    if (p.aspect > 1.0f) {
        hh /= p.aspect;
    } else if (p.aspect > 0.0f && p.aspect < 1.0f) {
        hw *= p.aspect;
    }
    dl->AddImage(p.texture, ImVec2(c.x - hw, c.y - hh), ImVec2(c.x + hw, c.y + hh), ImVec2(0, 0), p.uv,
                 IM_COL32(255, 255, 255, static_cast<int>(255 * (alpha > 1 ? 1 : alpha))));
}
constexpr float PI = 3.14159265f;

// A mesma imagem girada angle rad em volta do centro (as animacoes Soco e Giro).
void PictureTurned(ImDrawList* dl, int slot, ImVec2 c, float box, float alpha, float angle) {
    const Loaded& p = g_images[slot];
    if (!p.texture || alpha <= 0) {
        return;
    }
    if (angle == 0.0f) {
        Picture(dl, slot, c, box, alpha);
        return;
    }
    float hw = box * 0.5f, hh = box * 0.5f;
    if (p.aspect > 1.0f) {
        hh /= p.aspect;
    } else if (p.aspect > 0.0f && p.aspect < 1.0f) {
        hw *= p.aspect;
    }
    float cs = std::cos(angle), sn = std::sin(angle);
    auto at = [&](float x, float y) { return ImVec2(c.x + x * cs - y * sn, c.y + x * sn + y * cs); };
    dl->AddImageQuad(p.texture, at(-hw, -hh), at(hw, -hh), at(hw, hh), at(-hw, hh), ImVec2(0, 0), ImVec2(p.uv.x, 0),
                     p.uv, ImVec2(0, p.uv.y), IM_COL32(255, 255, 255, static_cast<int>(255 * (alpha > 1 ? 1 : alpha))));
}

float Clamp01(float v) {
    return v < 0 ? 0 : v > 1 ? 1 : v;
}

float EaseOut(float t) {
    t = Clamp01(t);
    return 1 - (1 - t) * (1 - t) * (1 - t);
}

// Passa um pouco do fim e volta (o pulo "elastico" do numero no estilo Arco).
float EaseOutBack(float t) {
    t = Clamp01(t);
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    return 1 + c3 * (t - 1) * (t - 1) * (t - 1) + c1 * (t - 1) * (t - 1);
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

// ---------------------------------------------------------------- informer

const char* KindLabel(const Hit& h, char* buf, size_t n) {
    snprintf(buf, n, h.estimated ? "~%d" : "%d", h.value);
    return buf;
}

// Classico: o nick parado e os numeros subindo e sumindo por cima dele, como o damage informer antigo; o combo
// (x3 - 138) ao lado do numero mais novo.
// Somando: o nick parado e, em cima dele, um numero so que cresce a cada acerto (com a cabeca ao lado, dourada, se
// estiver separada); some quando a soma fecha.
void InformerSum(ImDrawList* dl, const Line& l, ImVec2 at, bool given, const opt::Options& o, float k) {
    float s = k * o.informerSize / 100.0f;
    float window = static_cast<float>(o.comboWindow);
    float fade = l.age < window - 0.4f ? 1.0f : 1.0f - (l.age - (window - 0.4f)) / 0.4f;
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
        np.x -= Size(g_text, nickSize, nick).x;
    }
    Outlined(dl, g_text, nickSize, np, Alpha(nickColor, fade), nick, 1.3f * s);

    ImFont* nf = NumberFont(o);
    float pop = l.bump < 0.12f ? 1.0f + 0.35f * (1.0f - EaseOut(l.bump / 0.12f)) : 1.0f;
    float numSize = 30 * s * pop;
    int head = o.splitHead ? l.head : 0;
    int body = l.total - head;
    char bodyText[16] = "", headText[16] = "";
    if (l.afk) {
        snprintf(bodyText, sizeof(bodyText), "AFK");
    } else {
        if (body > 0 || head == 0) {
            snprintf(bodyText, sizeof(bodyText), l.estimated ? "~%d" : "%d", body);
        }
        if (head > 0) {
            snprintf(headText, sizeof(headText), "%d", head);
        }
    }
    Kind last = l.last == HEADSHOT && o.splitHead ? HEALTH : l.last;
    ImU32 bodyColor = l.afk ? IM_COL32(204, 204, 204, 255) : given ? KindColor(last, o) : o.cTaken;
    if (o.colorByHealth && l.life >= 0 && !l.afk) {
        bodyColor = HealthColor(l.life);
    }
    if (l.last == KILL) {
        bodyColor = o.cKill;
    }
    float baseY = at.y + 0.02f * 1080 * k - 30 * s - 4 * s; // a base do numero fica parada; o pulo cresce para cima
    float y = baseY - (numSize - 30 * s);
    ImVec2 bs = Size(nf, numSize, bodyText), hs = Size(nf, numSize, headText);
    float gap = bodyText[0] && headText[0] ? 12 * s : 0;
    float total = bs.x + gap + hs.x;
    float x = given ? at.x : at.x - total;
    if (bodyText[0]) {
        Outlined(dl, nf, numSize, ImVec2(x, y), Alpha(bodyColor, fade), bodyText, 1.8f * s);
    }
    if (headText[0]) {
        Outlined(dl, nf, numSize, ImVec2(x + bs.x + gap, y), Alpha(o.cHead, fade), headText, 1.8f * s);
    }
}

void InformerClassic(ImDrawList* dl, const Line& l, ImVec2 at, bool given, const opt::Options& o, float k) {
    if (o.informerCount == 1) {
        InformerSum(dl, l, at, given, o, k);
        return;
    }
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
        if (o.colorByHealth && h.life >= 0 && h.kind != KILL) {
            c = HealthColor(h.life);
        }
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
        bool summing = o.informerCount == 1;
        int head = summing && o.splitHead ? l.head : 0;
        char total[32];
        if (l.afk) {
            snprintf(total, sizeof(total), "AFK");
        } else {
            snprintf(total, sizeof(total), l.estimated && summing ? "~%d" : "%d", l.total - head);
        }
        ImFont* nf = NumberFont(o);
        float ns = 19 * s * (summing && l.bump < 0.12f ? 1.0f + 0.3f * (1.0f - EaseOut(l.bump / 0.12f)) : 1.0f);
        ImVec2 ts = Size(nf, ns, total);
        float rx = x + w - 13 * s;
        if (head > 0 && !l.afk) { // a cabeca, dourada, antes do total
            char headText[16];
            snprintf(headText, sizeof(headText), "%d", head);
            ImVec2 hs = Size(nf, ns, headText);
            dl->AddText(nf, ns, ImVec2(rx - ts.x - 10 * s - hs.x, y + (h - ns) * 0.5f - 1 * s), Alpha(o.cHead, fade),
                        headText);
        }
        char hits[16] = "";
        if (l.hits > 1 && !summing) {
            snprintf(hits, sizeof(hits), "x%d", l.hits);
            ImVec2 hs = Size(g_text, 12.5f * s, hits);
            dl->AddText(g_text, 12.5f * s, ImVec2(rx - hs.x, ty + 2 * s), Alpha(IM_COL32(255, 255, 255, 120), fade), hits);
            rx -= hs.x + 8 * s;
        }
        ImU32 tc = l.afk ? IM_COL32(204, 204, 204, 255) : given ? IM_COL32(255, 255, 255, 255) : o.cTaken;
        if (o.colorByHealth && l.life >= 0 && !l.afk) {
            tc = l.last == KILL ? o.cKill : HealthColor(l.life);
        }
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
    // Sem o arquivo, a fonte embutida do ImGui (feia, mas nada quebra).
    auto add = [&](const char* path, float size) {
        FILE* f = path ? fopen(path, "rb") : nullptr;
        if (f) {
            fclose(f);
            if (ImFont* font = io.Fonts->AddFontFromFileTTF(path, size, &cfg, ranges)) {
                return font;
            }
        }
        ImFontConfig def;
        def.SizePixels = size;
        return io.Fonts->AddFontDefault(&def);
    };
    g_text = add(houseFont, std::round(24 * k));
    g_house = add(houseFont, std::round(48 * k));
    g_number = numberFont && numberFont[0] ? add(numberFont, std::round(48 * k)) : g_house;
}

int Effective(Kind kind, Kind under, const opt::Options& o) {
    if (kind == KILL && !o.fxKill) {
        kind = under;
    }
    if (kind == HEADSHOT && !o.fxHead) {
        kind = HEALTH;
    }
    if (kind == ARMOUR && !o.fxArmour) {
        kind = HEALTH;
    }
    if (kind == HEALTH && !o.fxHit) {
        return -1;
    }
    return kind;
}

ImU32 HealthColor(float life) {
    struct Stop {
        float at;
        float r, g, b;
    };
    static const Stop stops[] = {{0.0f, 255, 48, 48}, {0.33f, 255, 140, 26}, {0.66f, 255, 221, 51}, {1.0f, 255, 255, 255}};
    life = life < 0 ? 0 : life > 1 ? 1 : life;
    for (int i = 1; i < 4; ++i) {
        if (life <= stops[i].at) {
            const Stop &a = stops[i - 1], &b = stops[i];
            float t = (life - a.at) / (b.at - a.at);
            return IM_COL32(static_cast<int>(a.r + (b.r - a.r) * t), static_cast<int>(a.g + (b.g - a.g) * t),
                            static_cast<int>(a.b + (b.b - a.b) * t), 255);
        }
    }
    return IM_COL32(255, 255, 255, 255);
}

ImU32 KindColor(Kind k, const opt::Options& o) {
    switch (k) {
    case ARMOUR: return o.cArmour;
    case HEADSHOT: return o.cHead;
    case KILL: return o.cKill;
    default: return o.cHealth;
    }
}

void SetImage(int slot, ImTextureID texture, float u1, float v1, float aspect) {
    if (slot >= 0 && slot < IMG_COUNT) {
        g_images[slot].texture = texture;
        g_images[slot].uv = ImVec2(u1, v1);
        g_images[slot].aspect = aspect;
    }
}

// Quanto cada efeito fica (o abate mais: o "morte" da Mira Centralizada durava 150 quadros) e quando comeca a sumir.
static void Timing(Kind kind, float* life, float* hold) {
    *life = kind == KILL ? 1.2f : kind == HEADSHOT ? 0.55f : 0.35f;
    *hold = kind == KILL ? 0.85f : kind == HEADSHOT ? 0.3f : 0.15f;
}

void DrawCrosshair(ImDrawList* dl, ImVec2 c, float scale, const opt::Options& o) {
    Picture(dl, IMG_CROSSHAIR, c, AIM_BOX * scale * o.crosshairSize / 100.0f, 1.0f);
}

// O efeito do acerto, na animacao escolhida (a imagem e sempre a da pasta; muda so o movimento):
//   Pulo      35% maior, voltando em 0,1 s, fica e some
//   Expandir  nasce menor e abre para fora enquanto some (como o anel em volta da mira do Fortnite)
//   Soco      entra 60% maior e torto (12 graus) e assenta em 0,07 s, como um tranco
//   Giro      gira 45 graus e cresce um pouco enquanto some
//   Nenhuma   aparece e some
bool DrawEffect(ImDrawList* dl, ImVec2 c, float scale, float age, Kind kind, const opt::Options& o) {
    float life, hold;
    Timing(kind, &life, &hold);
    if (age >= life) {
        return false;
    }
    float fade = age < hold ? 1.0f : 1.0f - (age - hold) / (life - hold);
    float size = 1.0f, angle = 0.0f, alpha = fade;
    switch (o.fxAnim) {
    case 1: {
        float open = life * 0.35f;
        size = 0.7f + 0.6f * EaseOut(age / life);
        alpha = age < open ? 1.0f : 1.0f - (age - open) / (life - open);
        break;
    }
    case 2:
        size = 1.0f + 0.6f * (1.0f - EaseOut(age / 0.07f));
        angle = 0.21f * (1.0f - EaseOut(age / 0.16f));
        break;
    case 3:
        size = 1.0f + 0.15f * EaseOut(age / life);
        angle = 0.785f * EaseOut(age / life);
        break;
    case 4:
        break;
    default:
        size = 1.0f + 0.35f * (1.0f - EaseOut(age / 0.1f));
        break;
    }
    int slot = kind == KILL ? IMG_KILL : kind == HEADSHOT ? IMG_HEAD : kind == ARMOUR ? IMG_ARMOUR : IMG_HIT;
    PictureTurned(dl, slot, c, AIM_BOX * scale * o.crosshairSize / 100.0f * size, alpha, angle);
    return true;
}

void DrawAimPreview(ImDrawList* dl, ImVec2 c, float scale, float age, Kind kind, const opt::Options& o) {
    int fx = Effective(kind, HEALTH, o);
    float life, hold;
    Timing(fx < 0 ? HEALTH : static_cast<Kind>(fx), &life, &hold);
    bool effect = o.onAim && fx >= 0 && age < life;
    if (o.crosshair == 1 && !effect) {
        DrawCrosshair(dl, c, scale, o);
    } else if (o.crosshair == 0) { // a do jogo, so para lembrar
        dl->AddCircle(c, 11 * scale * o.crosshairSize / 100.0f, IM_COL32(255, 255, 255, 150), 32, 1.4f * scale);
    }
    if (effect) {
        DrawEffect(dl, c, scale, age, static_cast<Kind>(fx), o);
    }
}

// Local do acerto: o X fica inteiro ate quase o fim de stay segundos.
void DrawBodyMarker(ImDrawList* dl, ImVec2 c, float scale, float age, float stay) {
    if (age >= stay) {
        return;
    }
    float hold = std::max(0.1f, stay - 0.35f);
    float a = age < hold ? 1.0f : 1.0f - (age - hold) / (stay - hold);
    float pop = 1.0f + 0.35f * (1.0f - EaseOut(age / 0.1f));
    Picture(dl, IMG_BODY, c, BODY_BOX * scale * pop, a);
}

// O numero, na animacao escolhida. Fica parado n.stay s (somando: enquanto a soma esta aberta) e depois sai:
//   Subir   sobe e some (o damage informer antigo)
//   Arco    estoura grande, assenta com um repique e sai num arco para o lado, como o do Fortnite
//   Saltar  pula e volta para o lugar
//   Parado  nao anda: pulsa a cada acerto e some
void DrawNumber(ImDrawList* dl, const Number& n, float scale, const opt::Options& o) {
    float life = n.kind == KILL ? 1.4f : 1.0f;
    float m = n.age - n.stay; // tempo de saida
    if (m >= life) {
        return;
    }
    float t = Clamp01(m / life);
    float pop, dx = 0, dy = 0;
    if (o.numbersAnim == 1) {
        pop = n.age < 0.22f ? 1.8f - 0.8f * EaseOutBack(n.age / 0.22f) : 1.0f;
        float side = n.drift < 0 ? -1.0f : 1.0f;
        dx = side * 70 * scale * t;
        dy = (-160 * t + 130 * t * t) * scale;
        pop *= 1.0f - 0.25f * t;
    } else {
        pop = 1.0f + 0.5f * (1.0f - EaseOut(n.age / 0.1f));
        if (o.numbersAnim == 2) {
            dy = -70 * scale * 4 * t * (1 - t);
            dx = n.drift * 20 * scale * t;
        } else if (o.numbersAnim == 0) {
            dy = -(o.numbers == 1 ? 120 : 64) * scale * (1 - (1 - t) * (1 - t));
            dx = n.drift * 34 * scale * EaseOut(n.age / 0.6f);
        }
    }
    if (n.bump < 0.14f) {
        pop += 0.32f * (1.0f - n.bump / 0.14f);
    }
    float a = m < life - 0.35f ? 1.0f : (life - m) / 0.35f;
    float size = 30 * scale * KindSize(n.kind) * pop;
    char text[16];
    snprintf(text, sizeof(text), n.estimated ? "~%d" : "%d", n.value);
    ImFont* f = NumberFont(o);
    ImVec2 ts = Size(f, size, text);
    float x = n.from.x + dx - ts.x * 0.5f;
    float y = n.from.y + dy - n.lift * scale - ts.y * 0.5f;
    ImU32 color = o.colorByHealth && n.life >= 0 && n.kind != KILL ? HealthColor(n.life) : KindColor(n.kind, o);
    Outlined(dl, f, size, ImVec2(x, y), Alpha(color, a), text, 2.0f * scale);
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
    if (o.numbers == 0 || o.numbers == 3) {
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

    // local do acerto: o X fica na parte do corpo que o tiro pegou (o samp.cpp atualiza a posicao do osso)
    if (o.hitLocal) {
        for (const Marker& m : st.markers) {
            if (m.bodyVisible && Effective(m.kind, m.under, o) >= 0) {
                DrawBodyMarker(dl, m.body, k * o.hitLocalSize / 100.0f, m.age, static_cast<float>(o.hitLocalTime));
            }
        }
    }

    // a mira e o sinal de acerto, onde a mira esta neste quadro: o efeito do acerto mais novo fica no lugar da mira
    // do Trok enquanto aparece (a imagem do efeito ja traz o ponto); com a mira do jogo, vai por cima dela
    const Marker* active = nullptr;
    int activeFx = -1;
    if (o.onAim) {
        for (const Marker& m : st.markers) {
            int fx = Effective(m.kind, m.under, o);
            float life, hold;
            Timing(fx < 0 ? HEALTH : static_cast<Kind>(fx), &life, &hold);
            if (fx >= 0 && m.age < life && (!active || m.age < active->age)) {
                active = &m;
                activeFx = fx;
            }
        }
    }
    if (o.crosshair == 1 && st.showCrosshair && !active) {
        DrawCrosshair(dl, st.crosshair, k * st.spread, o);
    }
    if (active) {
        DrawEffect(dl, st.crosshair, k, active->age, static_cast<Kind>(activeFx), o);
    }
}

} // namespace hud

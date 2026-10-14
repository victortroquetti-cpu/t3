// Imagens de conceito do Trok Hitmarker, sem o jogo: desenha o hud e o menu com o ImGui de verdade, rasteriza o
// ImDrawData em software e grava PPMs. Uso: ./concept <pasta de saida> <fundo.rgb 1920x1080>
#include "hud.h"
#include "imgui.h"
#include "menu.h"
#include "options.h"
#include "trok_ui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef TROK_UI_TEST_FONT
#define TROK_UI_TEST_FONT "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
#endif
#ifndef NUMBER_FONT
#define NUMBER_FONT TROK_UI_TEST_FONT
#endif

static int W = 1920, H = 1080;
static std::vector<float> fb, background;
static unsigned char* tex;
static int tw, th;
struct Texture {
    std::vector<unsigned char> rgba;
    int w = 0, h = 0;
};
static Texture images[hud::IMG_COUNT];
static const unsigned char* curTex;
static int curW, curH;
static int failures = 0;
static const ImVec2 CROSS(1018, 432); // a mira do GTA: 53% x 40% da tela

static void Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "ok" : "FALHOU", what);
    if (!ok) ++failures;
}

static void Clear(float dim) {
    fb.assign(W * H * 3, 0);
    for (size_t i = 0; i < fb.size(); i += 3) {
        if (!background.empty()) {
            for (int c = 0; c < 3; ++c) fb[i + c] = background[i + c] * dim;
        } else {
            fb[i] = 40 / 255.0f, fb[i + 1] = 44 / 255.0f, fb[i + 2] = 52 / 255.0f;
        }
    }
}

static void Raster(const ImDrawVert& a, const ImDrawVert& b, const ImDrawVert& c, ImVec4 clip) {
    float minx = std::max(clip.x, std::floor(std::min({a.pos.x, b.pos.x, c.pos.x})));
    float maxx = std::min(clip.z, std::ceil(std::max({a.pos.x, b.pos.x, c.pos.x})));
    float miny = std::max(clip.y, std::floor(std::min({a.pos.y, b.pos.y, c.pos.y})));
    float maxy = std::min(clip.w, std::ceil(std::max({a.pos.y, b.pos.y, c.pos.y})));
    float area = (b.pos.x - a.pos.x) * (c.pos.y - a.pos.y) - (b.pos.y - a.pos.y) * (c.pos.x - a.pos.x);
    if (std::fabs(area) < 1e-6f) return;
    for (int y = (int)miny; y < (int)maxy; ++y) {
        for (int x = (int)minx; x < (int)maxx; ++x) {
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            float px = x + 0.5f, py = y + 0.5f;
            float w0 = ((b.pos.x - px) * (c.pos.y - py) - (b.pos.y - py) * (c.pos.x - px)) / area;
            float w1 = ((c.pos.x - px) * (a.pos.y - py) - (c.pos.y - py) * (a.pos.x - px)) / area;
            float w2 = 1 - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            float u = w0 * a.uv.x + w1 * b.uv.x + w2 * c.uv.x, v = w0 * a.uv.y + w1 * b.uv.y + w2 * c.uv.y;
            int tx = std::min(curW - 1, std::max(0, (int)(u * curW))), ty = std::min(curH - 1, std::max(0, (int)(v * curH)));
            const unsigned char* t = &curTex[(ty * curW + tx) * 4];
            float col[4];
            for (int i = 0; i < 4; ++i) {
                float ca = ((a.col >> (8 * i)) & 0xFF) / 255.f, cb = ((b.col >> (8 * i)) & 0xFF) / 255.f,
                      cc = ((c.col >> (8 * i)) & 0xFF) / 255.f;
                col[i] = (w0 * ca + w1 * cb + w2 * cc) * t[i] / 255.f;
            }
            float* p = &fb[(y * W + x) * 3];
            for (int i = 0; i < 3; ++i) p[i] = p[i] * (1 - col[3]) + col[i] * col[3];
        }
    }
}

static void Render(ImDrawData* dd) {
    for (int n = 0; n < dd->CmdListsCount; ++n) {
        const ImDrawList* cl = dd->CmdLists[n];
        for (const ImDrawCmd& cmd : cl->CmdBuffer) {
            if (cmd.UserCallback) continue;
            intptr_t id = (intptr_t)cmd.GetTexID();
            if (id >= 10 && id < 10 + hud::IMG_COUNT && !images[id - 10].rgba.empty()) {
                curTex = images[id - 10].rgba.data(), curW = images[id - 10].w, curH = images[id - 10].h;
            } else {
                curTex = tex, curW = tw, curH = th;
            }
            for (unsigned i = 0; i < cmd.ElemCount; i += 3) {
                const ImDrawIdx* idx = &cl->IdxBuffer[cmd.IdxOffset + i];
                Raster(cl->VtxBuffer[cmd.VtxOffset + idx[0]], cl->VtxBuffer[cmd.VtxOffset + idx[1]],
                       cl->VtxBuffer[cmd.VtxOffset + idx[2]], cmd.ClipRect);
            }
        }
    }
}

// Grava so o retangulo [x0,x1) x [y0,y1).
static void Save(const std::string& path, int x0 = 0, int y0 = 0, int x1 = 1920, int y1 = 1080) {
    FILE* f = fopen(path.c_str(), "wb");
    fprintf(f, "P6\n%d %d\n255\n", x1 - x0, y1 - y0);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            for (int c = 0; c < 3; ++c) fputc((int)std::min(255.f, std::max(0.f, fb[(y * W + x) * 3 + c] * 255.f + 0.5f)), f);
    fclose(f);
}

static int Lit(int x0, int y0, int x1, int y1, float floorLum) {
    int n = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const float* p = &fb[(y * W + x) * 3];
            if (p[0] + p[1] + p[2] > floorLum * 3) ++n;
        }
    return n;
}

// ---------------------------------------------------------------- a cena

// O alvo (Lucas_Mendes, id 3) esta no meio da rua com a cabeca na mira; quem atira em voce esta a esquerda.
static const ImVec2 HEAD(1018, 423), CHEST(1018, 490), LEFTARM(1066, 500), LEG(1000, 640);
static const ImU32 LUCAS = IM_COL32(255, 152, 51, 255), ENZO = IM_COL32(160, 110, 255, 255);

static void GameCrosshair(ImDrawList* dl, ImVec2 at) {
    dl->AddCircle(at, 15 * opt::Get().crosshairSize / 100.0f, IM_COL32(255, 255, 255, 150), 40, 1.6f);
    dl->AddCircleFilled(at, 1.8f, IM_COL32(255, 255, 255, 190), 12);
}

static void Label(ImDrawList* dl, float x, float y, const char* text, bool center = false, ImU32 c = 0) {
    ImVec2 ts = tui::TextSize(tui::fonts.body, text);
    tui::Text(dl, tui::fonts.body, center ? x - ts.x * 0.5f : x, y, c ? c : IM_COL32(255, 255, 255, 200), text);
}

static void Small(ImDrawList* dl, float x, float y, const char* text, bool center = false) {
    ImVec2 ts = tui::TextSize(tui::fonts.desc, text);
    tui::Text(dl, tui::fonts.desc, center ? x - ts.x * 0.5f : x, y, IM_COL32(255, 255, 255, 130), text);
}

static hud::Line LucasLine() {
    hud::Line l;
    l.nick = "Lucas_Mendes";
    l.id = 3;
    l.nickColor = LUCAS;
    l.hits = 5;
    l.total = 126;
    l.age = 0.2f;
    l.recent = {{0.95f, 10, hud::ARMOUR}, {0.6f, 10, hud::ARMOUR}, {0.2f, 46, hud::HEADSHOT}};
    return l;
}

static hud::Line EnzoLine() {
    hud::Line l;
    l.nick = "Enzo_Rampani";
    l.id = 7;
    l.nickColor = ENZO;
    l.hits = 2;
    l.total = 20;
    l.age = 0.3f;
    l.recent = {{0.7f, 10, hud::HEALTH}, {0.3f, 10, hud::HEALTH}};
    return l;
}

static hud::State Fight() {
    hud::State s;
    s.crosshair = CROSS;
    s.showCrosshair = true;
    // os tiros de antes ainda marcados no corpo (perna e peito) e o da cabeca agora
    hud::Marker m;
    m.body = LEG, m.age = 1.3f, m.kind = hud::ARMOUR;
    s.markers.push_back(m);
    m.body = CHEST, m.age = 0.6f, m.kind = hud::ARMOUR;
    s.markers.push_back(m);
    m.body = HEAD, m.age = 0.05f, m.kind = hud::HEADSHOT;
    s.markers.push_back(m);
    hud::Number n;
    // no jogo, o numero sai do lado direito do ponto acertado (a mira fica livre)
    n.from = ImVec2(CHEST.x + 52, CHEST.y - 10);
    n.age = 0.82f, n.value = 10, n.kind = hud::ARMOUR, n.drift = 0.9f;
    s.numbers.push_back(n);
    n.age = 0.5f, n.drift = 0.2f;
    s.numbers.push_back(n);
    n.from = ImVec2(HEAD.x + 46, HEAD.y - 8);
    n.age = 0.16f, n.value = 46, n.kind = hud::HEADSHOT, n.drift = 0.5f;
    s.numbers.push_back(n);
    s.given = {LucasLine()};
    s.taken = {EnzoLine()};
    hud::Incoming in;
    in.angle = -0.95f;
    in.age = 0.3f;
    in.damage = 10;
    s.incoming.push_back(in);
    s.flash = 0.35f;
    return s;
}

static void Frame(const hud::State& s, bool crosshair, float dim, void (*extra)(ImDrawList*) = nullptr) {
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (crosshair && opt::Get().crosshair == 0) GameCrosshair(dl, s.crosshair); // com o ponto do Trok, o GTA nao desenha a dele
    hud::Draw(dl, s, opt::Get(), io.DisplaySize);
    if (extra) extra(dl);
    ImGui::Render();
    Clear(dim);
    Render(ImGui::GetDrawData());
}

// ---------------------------------------------------------------- quadros de detalhe

static void BoardMarkers(ImDrawList* dl) {
    opt::Options o = opt::Get();
    // 1. as imagens como vem (ampliadas), com o nome do arquivo
    const char* files[] = {"mira.png", "acerto.png", "colete.png", "cabeca.png", "abate.png", "corpo.png"};
    Label(dl, 60, 40, "As imagens que v\xC3\xAAm junto (pasta Trok Hitmarker\\mira), ampliadas: edite como quiser");
    for (int i = 0; i < 6; ++i) {
        float x = 120 + i * 170, y = 150;
        dl->AddRectFilled(ImVec2(x - 64, y - 64), ImVec2(x + 64, y + 64), IM_COL32(255, 255, 255, 10), 8);
        dl->AddImage((ImTextureID)(intptr_t)(10 + i), ImVec2(x - 64, y - 64), ImVec2(x + 64, y + 64));
        Small(dl, x, y + 74, files[i], true);
    }
    // 2. no tamanho do jogo: a mira do Trok e a do jogo, com cada efeito no lugar
    Label(dl, 60, 290, "No tamanho do jogo (100%)");
    const char* kinds[] = {"Mira", "Acerto", "Colete", "Cabe\xC3\xA7" "a", "Abate"};
    hud::Kind k[] = {hud::HEALTH, hud::HEALTH, hud::ARMOUR, hud::HEADSHOT, hud::KILL};
    const char* rows[] = {"Do Trok", "A do jogo"};
    for (int r = 0; r < 2; ++r) {
        Small(dl, 60, 352 + r * 80, rows[r]);
        o.crosshair = r == 0 ? 1 : 0;
        for (int c = 0; c < 5; ++c) {
            ImVec2 at(260 + c * 150, 360 + r * 80);
            if (r == 0) Small(dl, at.x, 316, kinds[c], true);
            hud::DrawAimPreview(dl, at, 1.0f, c == 0 ? 99.0f : 0.15f, k[c], o);
        }
    }
    // 3. o pulo e o sumir, quadro a quadro
    Label(dl, 60, 540, "Quadro a quadro (em tempo, n\xC3\xA3o em quadros: igual em qualquer FPS)");
    float ages[] = {0.0f, 0.05f, 0.12f, 0.3f, 0.6f, 1.0f};
    const char* ms[] = {"0 ms", "50 ms", "120 ms", "300 ms", "600 ms", "1 s"};
    hud::Kind rowKind[] = {hud::HEALTH, hud::HEADSHOT, hud::KILL};
    const char* rowName[] = {"acerto", "cabe\xC3\xA7" "a", "abate"};
    o.crosshair = 1;
    for (int rr = 0; rr < 3; ++rr) {
        Small(dl, 60, 600 + rr * 70 - 8, rowName[rr]);
        for (int i = 0; i < 6; ++i) {
            ImVec2 at(260 + i * 120, 600 + rr * 70);
            hud::DrawAimPreview(dl, at, 1.5f, ages[i], rowKind[rr], o);
            if (rr == 2) Small(dl, at.x, at.y + 36, ms[i], true);
        }
    }
}

static void BoardNumbers(ImDrawList* dl) {
    opt::Options& o = opt::Get();
    // personagem: cada tiro um numero (Somar desligado) x somando no mesmo numero
    Label(dl, 140, 70, "Sobem do personagem");
    Small(dl, 140, 100, "um n\xC3\xBAmero por tiro (M4)");
    Small(dl, 620, 100, "somando no mesmo n\xC3\xBAmero");
    hud::Number n;
    float k = 1.0f;
    float ages[] = {0.85f, 0.62f, 0.4f, 0.18f};
    float drifts[] = {-0.8f, 0.5f, -0.3f, 0.7f};
    for (int i = 0; i < 4; ++i) {
        n.from = ImVec2(300, 340);
        n.age = ages[i], n.value = 10, n.kind = i < 2 ? hud::ARMOUR : hud::HEALTH, n.drift = drifts[i] * 1.6f;
        hud::DrawNumber(dl, n, k, o);
    }
    n = hud::Number();
    n.from = ImVec2(780, 330);
    n.age = 0.45f, n.value = 40, n.kind = hud::HEALTH, n.bump = 0.05f;
    hud::DrawNumber(dl, n, k, o);
    Small(dl, 780, 380, "4 tiros = 40", true);

    Label(dl, 1120, 70, "Sobem na tela, do lado do centro");
    dl->AddLine(ImVec2(1192, 300), ImVec2(1208, 300), IM_COL32(255, 255, 255, 90), 1.5f);
    dl->AddLine(ImVec2(1200, 292), ImVec2(1200, 308), IM_COL32(255, 255, 255, 90), 1.5f);
    Small(dl, 1200, 316, "centro", true);
    float a2[] = {0.7f, 0.42f, 0.12f};
    int v2[] = {10, 10, 46};
    hud::Kind k2[] = {hud::HEALTH, hud::HEALTH, hud::HEADSHOT};
    for (int i = 0; i < 3; ++i) {
        n = hud::Number();
        n.from = ImVec2(1250, 292);
        n.age = a2[i], n.value = v2[i], n.kind = k2[i], n.drift = 0;
        o.numbers = 1;
        hud::DrawNumber(dl, n, 0.85f, o);
        o.numbers = 0;
    }

    // as cores
    float y = 520;
    Label(dl, 140, y, "Cores (todas troc\xC3\xA1veis no menu)");
    struct {
        const char* name;
        int value;
        hud::Kind kind;
        bool est;
    } cases[] = {{"vida", 10, hud::HEALTH, false}, {"colete", 10, hud::ARMOUR, false}, {"cabe\xC3\xA7" "a", 46, hud::HEADSHOT, false},
                 {"abate", 74, hud::KILL, false}, {"sem vida real", 46, hud::HEALTH, true}};
    for (int i = 0; i < 5; ++i) {
        n = hud::Number();
        n.from = ImVec2(220 + i * 230, y + 140);
        n.age = 0.3f, n.value = cases[i].value, n.kind = cases[i].kind, n.estimated = cases[i].est;
        hud::DrawNumber(dl, n, 1.0f, o);
        Small(dl, 220 + i * 230, y + 150, cases[i].name, true);
    }
    Small(dl, 140, y + 200,
          "\"~46\": o servidor n\xC3\xA3o mostrou a vida do alvo caindo (vida escondida), ent\xC3\xA3o vale o n\xC3\xBAmero do GTA, com o ~ na frente.");
}

// ---------------------------------------------------------------- animacoes (quadros de GIF)

static float g_animTime = 0;

// 19. Cada linha uma animacao do sinal, cada coluna um efeito (a imagem e a mesma; muda o movimento).
static void BoardFxAnims(ImDrawList* dl) {
    opt::Options o = opt::Get();
    const char* rows[] = {"Pulo", "Expandir", "Soco", "Giro", "Nenhuma"};
    const char* cols[] = {"Acerto", "Colete", "Cabe\xC3\xA7" "a", "Abate"};
    hud::Kind kinds[] = {hud::HEALTH, hud::ARMOUR, hud::HEADSHOT, hud::KILL};
    const float x0 = 220, y0 = 110, cw = 170, ch = 120;
    for (int c = 0; c < 4; ++c) {
        Label(dl, x0 + c * cw + cw * 0.5f, y0 - 40, cols[c], true);
    }
    for (int r = 0; r < 5; ++r) {
        Label(dl, 60, y0 + r * ch + ch * 0.5f - 10, rows[r]);
        o.fxAnim = r;
        for (int c = 0; c < 4; ++c) {
            ImVec2 a(x0 + c * cw + 8, y0 + r * ch + 6), b(x0 + (c + 1) * cw - 8, y0 + (r + 1) * ch - 6);
            dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, 10), 8);
            ImVec2 center((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
            float age = std::fmod(g_animTime + 1.6f - c * 0.12f, 1.6f);
            hud::DrawAimPreview(dl, center, 1.6f, age, kinds[c], o);
        }
    }
}

// 20. Os numeros: as quatro animacoes (tres tiros: vida, colete, cabeca) e o Somando, com e sem a cabeca separada.
static void BoardNumberAnims(ImDrawList* dl) {
    opt::Options o = opt::Get();
    o.numbers = 0;
    const char* rows[] = {"Subir", "Arco", "Saltar", "Parado", "Somando", "Somando, cabe\xC3\xA7" "a separada"};
    const float x0 = 360, y0 = 100, rw = 700, rh = 132;
    float t = g_animTime;
    for (int r = 0; r < 6; ++r) {
        float top = y0 + r * rh;
        dl->AddRectFilled(ImVec2(x0, top + 6), ImVec2(x0 + rw, top + rh - 6), IM_COL32(255, 255, 255, 8), 8);
        Label(dl, 60, top + rh * 0.5f - 10, rows[r]);
        ImVec2 base(x0 + rw * 0.5f, top + rh - 34);
        if (r < 4) {
            o.numbersAnim = r;
            const hud::Kind kinds[] = {hud::HEALTH, hud::ARMOUR, hud::HEADSHOT};
            const float drifts[] = {0.0f, 0.7f, -0.7f};
            for (int i = 0; i < 3; ++i) {
                float age = t - i * 0.45f;
                if (age < 0) continue;
                hud::Number n;
                n.from = ImVec2(base.x - 150 + i * 150, base.y);
                n.age = age;
                n.value = 46;
                n.kind = kinds[i];
                n.drift = drifts[i];
                n.bump = 1.0f;
                hud::DrawNumber(dl, n, 1.0f, o);
            }
        } else {
            o.numbersAnim = 0;
            bool split = r == 5;
            const float hits[] = {0.0f, 0.35f, 0.7f, 1.05f};
            const int values[] = {46, 13, 46, 41}; // Deagle, M4, Deagle na cabeca, Sniper
            int body = 0, head = 0;
            float last = 0, lastHead = 0;
            for (int i = 0; i < 4; ++i) {
                if (t < hits[i]) break;
                if (i == 2 && split) {
                    head += values[i];
                    lastHead = hits[i];
                } else {
                    body += values[i];
                }
                last = hits[i];
            }
            if (t < hits[0]) continue;
            hud::Number n;
            n.from = base;
            n.age = t;
            n.stay = 1.7f;
            n.value = body;
            n.kind = hud::HEALTH;
            n.bump = t - last;
            hud::DrawNumber(dl, n, 1.0f, o);
            if (head > 0) {
                n.value = head;
                n.kind = hud::HEADSHOT;
                n.lift = 40;
                n.age = t - 0.7f;
                n.stay = 1.0f;
                n.bump = t - lastHead;
                hud::DrawNumber(dl, n, 1.0f, o);
            }
        }
    }
}

// 21. O informer: combo e somando (com e sem a cabeca separada), o mesmo combate.
static void BoardInformerSum(ImDrawList* dl) {
    opt::Options o = opt::Get();
    hud::State s;
    hud::Line l;
    l.nick = "Lucas_Mendes";
    l.id = 3;
    l.nickColor = LUCAS;
    l.hits = 4;
    l.total = 138;
    l.head = 46;
    l.age = 0.3f;
    l.bump = 0.5f;
    l.recent = {{1.1f, 46, hud::HEALTH}, {0.7f, 46, hud::HEALTH}, {0.3f, 46, hud::HEADSHOT}};
    s.given = {l};
    const char* titles[] = {"Combo (x3 \xC2\xB7 138)", "Somando", "Somando, cabe\xC3\xA7" "a separada"};
    for (int i = 0; i < 3; ++i) {
        float x = 120 + i * 560;
        dl->AddRectFilled(ImVec2(x - 40, 120), ImVec2(x + 440, 460), IM_COL32(255, 255, 255, 8), 10);
        Label(dl, x - 20, 136, titles[i]);
        o.informer = 0;
        o.informerCount = i == 0 ? 0 : 1;
        o.splitHead = i == 2;
        o.given = opt::Place{x / 1920.0f, 360 / 1080.0f};
        o.numbers = 2, o.direction = false, o.edgeFlash = false, o.onAim = false, o.hitLocal = false;
        hud::Draw(dl, s, o, ImVec2(1920, 1080));
    }
}

// 22. Cor pela vida: o mesmo tiro com a vida que sobrou ao alvo caindo.
static void BoardHealthColors(ImDrawList* dl) {
    opt::Options o = opt::Get();
    o.colorByHealth = true;
    o.numbers = 0;
    o.numbersAnim = 3; // parado
    const float lives[] = {1.0f, 0.8f, 0.6f, 0.45f, 0.3f, 0.15f, 0.04f};
    Label(dl, 100, 120, "Cor pela vida que sobrou ao alvo (no dano levado, a sua)");
    for (int i = 0; i < 7; ++i) {
        float x = 160 + i * 170;
        hud::Number n;
        n.from = ImVec2(x, 230);
        n.age = 0.5f;
        n.stay = 5.0f;
        n.value = 46;
        n.life = lives[i];
        n.bump = 1.0f;
        hud::DrawNumber(dl, n, 1.3f, o);
        char text[32];
        snprintf(text, sizeof(text), "vida %d", static_cast<int>(lives[i] * 100 + 0.5f));
        Small(dl, x, 285, text, true);
    }
    hud::Number k;
    k.from = ImVec2(160 + 7 * 170, 230);
    k.age = 0.5f;
    k.stay = 5.0f;
    k.value = 46;
    k.life = 0.0f;
    k.kind = hud::KILL;
    k.bump = 1.0f;
    hud::DrawNumber(dl, k, 1.3f, o);
    Small(dl, 160 + 7 * 170, 285, "abate", true);
}

static void Animate(void (*board)(ImDrawList*), float cycle, const std::string& prefix, int x0, int y0, int x1, int y1) {
    const int fps = 20;
    int frames = static_cast<int>(cycle * fps);
    hud::State empty;
    std::vector<float> photo;
    photo.swap(background); // fundo liso: o GIF fica nitido e leve
    for (int f = 0; f < frames; ++f) {
        g_animTime = f / static_cast<float>(fps);
        Frame(empty, false, 0.3f, board);
        char name[64];
        snprintf(name, sizeof(name), "%s-%03d.ppm", prefix.c_str(), f);
        Save(name, x0, y0, x1, y1);
    }
    photo.swap(background);
}

static void Menu(int tab, const std::string& out, int x0, int x1) {
    ImGuiIO& io = ImGui::GetIO();
    menu::SetTab(tab);
    for (int f = 0; f < 30; ++f) {
        io.DeltaTime = 1.0f / 60.0f;
        io.AddMousePosEvent(5, 5);
        ImGui::NewFrame();
        menu::Frame();
        ImGui::Render();
    }
    Clear(0.55f);
    Render(ImGui::GetDrawData());
    float u = tui::u, h = std::min(H * 0.8f, 640 * u);
    int y0 = (int)(H * 0.5f - h * 0.5f) - 14, y1 = (int)(H * 0.5f + h * 0.5f) + 14;
    Save(out, x0, y0, x1, y1);
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : ".";
    if (argc > 2) {
        FILE* f = fopen(argv[2], "rb");
        if (f) {
            std::vector<unsigned char> raw(W * H * 3);
            if (fread(raw.data(), 1, raw.size(), f) == raw.size()) {
                background.resize(raw.size());
                for (size_t i = 0; i < raw.size(); ++i) background[i] = raw[i] / 255.0f;
            }
            fclose(f);
        }
    }
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)W, (float)H);
    io.IniFilename = nullptr;
    tui::BuildFonts((float)H, "");
    hud::AddFonts((float)H, TROK_UI_TEST_FONT, NUMBER_FONT);
    io.Fonts->Build();
    io.Fonts->GetTexDataAsRGBA32(&tex, &tw, &th);
    io.Fonts->SetTexID((ImTextureID)1);
    const char* names[] = {"mira", "acerto", "colete", "cabeca", "abate", "corpo"};
    for (int i = 0; i < hud::IMG_COUNT; ++i) {
        std::string path = dir + "/" + names[i] + ".rgba";
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) continue;
        int wh[2];
        if (fread(wh, sizeof(int), 2, f) == 2) {
            images[i].w = wh[0], images[i].h = wh[1];
            images[i].rgba.resize((size_t)wh[0] * wh[1] * 4);
            if (fread(images[i].rgba.data(), 1, images[i].rgba.size(), f) == images[i].rgba.size()) {
                hud::SetImage(i, (ImTextureID)(intptr_t)(10 + i));
            }
        }
        fclose(f);
    }
    Check(!images[0].rgba.empty() && !images[5].rgba.empty(), "imagens padrao carregadas");
    opt::Options& o = opt::Get();

    // 1. Estilo Trok: hitmarker, numeros no personagem, local do acerto, direcao e borda vermelha (sem o informer)
    o.informer = 2;
    Frame(Fight(), true, 1.0f);
    Save(dir + "/01-cena-trok.ppm");
    Check(Lit(990, 404, 1046, 460, 0.5f) > 40, "hitmarker na mira, sem numero por cima");
    Check(Lit(1000, 470, 1036, 510, 0.5f) > 30, "local do acerto marcado no peito");

    // 2. Estilo Damage Informer: o layout antigo (nick parado, numero subindo, combo ao lado), hitmarker no corpo
    o = opt::Options();
    o.numbers = 2, o.onAim = false, o.direction = false, o.edgeFlash = false;
    o.cHealth = IM_COL32(255, 100, 69, 255), o.cArmour = IM_COL32(255, 100, 69, 255);
    o.cHead = IM_COL32(140, 199, 244, 255), o.cCombo = IM_COL32(255, 0, 0, 255);
    Frame(Fight(), true, 1.0f);
    Save(dir + "/02-cena-damage-informer.ppm");
    Check(Lit(1150, 380, 1480, 560, 0.45f) > 300, "informer classico do dano dado, a direita do centro");
    Check(Lit(440, 380, 770, 560, 0.45f) > 300, "informer classico do dano levado, a esquerda do centro");

    // 3. Informer em lista, com a cor fixa no nick (verde ao dar, vermelho ao levar)
    o = opt::Options();
    o.informer = 1, o.nickGiven = 1, o.nickTaken = 1, o.hitLocal = false, o.numbers = 2;
    hud::State s = Fight();
    hud::Line other;
    other.nick = "Victor_Trok", other.id = 12, other.nickColor = IM_COL32(79, 140, 255, 255);
    other.hits = 1, other.total = 10, other.age = 1.4f;
    s.given.push_back(other);
    Frame(s, true, 1.0f);
    Save(dir + "/03-informer-lista.ppm", 400, 300, 1520, 640);

    // 4. Hitmarkers
    o = opt::Options();
    hud::State empty;
    empty.crosshair = CROSS;
    Frame(empty, false, 0.35f, BoardMarkers);
    Save(dir + "/04-hitmarker.ppm", 0, 0, 1180, 880);

    // 5. Numeros
    Frame(empty, false, 0.35f, BoardNumbers);
    Save(dir + "/05-numeros.ppm", 80, 40, 1600, 780);

    // 6. Local do acerto: logo depois dos tiros e 1,5 s depois (a perna ja sumiu, o peito esta sumindo)
    o.numbers = 2, o.informer = 2, o.direction = false, o.edgeFlash = false;
    s = Fight();
    Frame(s, true, 0.85f);
    Save(dir + "/06a.ppm", 868, 360, 1168, 720);
    for (hud::Marker& mk : s.markers) mk.age += 1.5f;
    Frame(s, true, 0.85f);
    Save(dir + "/06b.ppm", 868, 360, 1168, 720);
    o = opt::Options();

    // 7. Recebido: dois tiros (frente-esquerda e por tras a direita); a seta fica em volta do centro da tela
    s = hud::State();
    s.crosshair = CROSS;
    hud::Incoming in;
    in.angle = -0.95f, in.age = 0.2f, in.damage = 46;
    s.incoming.push_back(in);
    in.angle = 2.5f, in.age = 0.7f, in.damage = 10;
    s.incoming.push_back(in);
    s.flash = 0.8f;
    s.taken = {EnzoLine()};
    Frame(s, true, 1.0f);
    Save(dir + "/07-recebido.ppm");
    Check(Lit(780, 400, 880, 500, 0.4f) > 60, "seta de quem atirou pela frente-esquerda, em volta do centro");

    // 15. A borda na cor do que foi atingido: vida, colete e cabeca
    hud::Kind kinds[] = {hud::HEALTH, hud::ARMOUR, hud::HEADSHOT};
    const char* borders[] = {"/15a.ppm", "/15b.ppm", "/15c.ppm"};
    for (int i = 0; i < 3; ++i) {
        s = hud::State();
        s.crosshair = CROSS;
        s.flash = 0.9f;
        s.flashKind = kinds[i];
        Frame(s, true, 1.0f);
        Save(dir + borders[i]);
    }

    // 8. A mira seguida de verdade: a do GTA (53% x 40%) e a de um mod que centraliza a mira (50% x 50%)
    o = opt::Options();
    o.numbers = 2, o.direction = false, o.edgeFlash = false, o.crosshair = 0;
    s = Fight();
    s.taken.clear();
    s.given.clear();
    o.informer = 2;
    Frame(s, true, 0.75f);
    Save(dir + "/08a.ppm", 700, 260, 1340, 620);
    s.crosshair = ImVec2(960, 540);
    Frame(s, true, 0.75f);
    Save(dir + "/08b.ppm", 640, 368, 1280, 728);
    Check(Lit(930, 512, 990, 568, 0.5f) > 40, "hitmarker na mira centralizada");
    o = opt::Options();

    // 9. Modo Mover (o do menu): as duas caixas do informer, com um exemplo dentro, arrastadas com o mouse
    menu::StartMove();
    for (int f = 0; f < 3; ++f) {
        io.DeltaTime = 1.0f / 60.0f;
        io.AddMousePosEvent(1300, 470);
        ImGui::NewFrame();
        menu::Frame();
        ImGui::Render();
    }
    Clear(0.8f);
    Render(ImGui::GetDrawData());
    Save(dir + "/09-mover.ppm", 380, 290, 1540, 1080);
    Check(Lit(1150, 380, 1480, 560, 0.45f) > 300, "mover: exemplo do informer dado na caixa dele");
    for (bool down : {true, false}) { // Esc: sai sem mudar nada
        io.AddKeyEvent(ImGuiKey_Escape, down);
        ImGui::NewFrame();
        menu::Frame();
        ImGui::Render();
    }

    // 10-11. Menu
    menu::Toggle();
    Menu(0, dir + "/10-menu-mira.ppm", 690, 1230);
    Menu(1, dir + "/11-menu-dano.ppm", 690, 1230);
    Menu(2, dir + "/12-menu-informer.ppm", 690, 1230);
    Menu(4, dir + "/13-menu-sons.ppm", 690, 1230);
    Menu(3, dir + "/14-menu-recebido.ppm", 690, 1230);
    Menu(5, dir + "/16-menu-geral.ppm", 690, 1230);
    o.crosshair = 0, o.crosshairStatic = true; // a mira do jogo, parada
    Menu(0, dir + "/17-menu-mira-do-jogo.ppm", 690, 1230);
    o = opt::Options();
    o.crosshairSize = 150, o.fxHead = false, o.fxKill = false; // mira maior, sem cabeca e sem abate proprios
    Menu(0, dir + "/18-menu-mira-tamanho.ppm", 690, 1230);
    o = opt::Options();

    // 19-21. As animacoes (quadros que o concept.sh junta em GIF) e o informer somando
    o = opt::Options();
    Animate(BoardFxAnims, 1.6f, dir + "/gif19", 30, 50, 940, 720);
    Animate(BoardNumberAnims, 2.6f, dir + "/gif20", 30, 80, 1090, 900);
    hud::State none;
    Frame(none, false, 0.3f, BoardInformerSum);
    Save(dir + "/21-informer-somando.ppm", 60, 100, 1740, 480);
    Check(Lit(680, 250, 1100, 420, 0.5f) > 200, "informer somando: um numero so em cima do nick");

    Frame(none, false, 0.3f, BoardHealthColors);
    Save(dir + "/22-cor-pela-vida.ppm", 40, 90, 1500, 320);

    printf("concept: %d falhas\n", failures);
    return failures ? 1 : 0;
}

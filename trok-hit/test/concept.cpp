// Imagens de conceito do Trok Hitmarker, sem o jogo: desenha o hud e o menu com o ImGui de verdade, rasteriza o
// ImDrawData em software e grava PPMs. Uso: ./concept <pasta de saida> <fundo.rgb 1920x1080>
#include "hud.h"
#include "imgui.h"
#include "menu.h"
#include "options.h"
#include "trok_ui.h"

#include <algorithm>
#include <cmath>
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
            int tx = std::min(tw - 1, std::max(0, (int)(u * tw))), ty = std::min(th - 1, std::max(0, (int)(v * th)));
            const unsigned char* t = &tex[(ty * tw + tx) * 4];
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
    dl->AddCircle(at, 15, IM_COL32(255, 255, 255, 150), 40, 1.6f);
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
    in.nick = "Enzo_Rampani";
    in.nickColor = ENZO;
    s.incoming.push_back(in);
    s.flash = 0.35f;
    return s;
}

static void Frame(const hud::State& s, bool crosshair, float dim, void (*extra)(ImDrawList*) = nullptr) {
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (crosshair) GameCrosshair(dl, s.crosshair);
    hud::Draw(dl, s, opt::Get(), io.DisplaySize);
    if (extra) extra(dl);
    ImGui::Render();
    Clear(dim);
    Render(ImGui::GetDrawData());
}

// ---------------------------------------------------------------- quadros de detalhe

static void BoardMarkers(ImDrawList* dl) {
    opt::Options o = opt::Get();
    const char* styles[] = {"Cl\xC3\xA1ssico (CoD)", "Discreto", "Anel"};
    const char* kinds[] = {"Vida", "Colete", "Cabe\xC3\xA7" "a", "Abate"};
    hud::Kind k[] = {hud::HEALTH, hud::ARMOUR, hud::HEADSHOT, hud::KILL};
    float x0 = 330, y0 = 150, cw = 150, rh = 120;
    for (int c = 0; c < 4; ++c) Label(dl, x0 + c * cw, y0 - 60, kinds[c], true);
    for (int r = 0; r < 3; ++r) {
        Label(dl, 60, y0 + r * rh - 10, styles[r]);
        for (int c = 0; c < 4; ++c) hud::DrawMarker(dl, ImVec2(x0 + c * cw, y0 + r * rh), r, 1.3f, 0.1f, k[c], o);
    }
    // a animacao do classico, quadro a quadro
    float y = y0 + 3 * rh + 40;
    Label(dl, 60, y - 10, "Um acerto, quadro a quadro");
    float ages[] = {0.0f, 0.03f, 0.08f, 0.16f, 0.22f, 0.28f};
    const char* ms[] = {"0 ms", "30 ms", "80 ms", "160 ms", "220 ms", "280 ms"};
    for (int i = 0; i < 6; ++i) {
        hud::DrawMarker(dl, ImVec2(x0 + i * 110, y), 0, 1.3f, ages[i], hud::HEALTH, o);
        Small(dl, x0 + i * 110, y + 40, ms[i], true);
    }
    Small(dl, 60, y + 70, "Abate: fica vermelho, maior e dura 0,65 s. Cada tiro novo reinicia a anima\xC3\xA7\xC3\xA3o.");
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

    Label(dl, 1120, 70, "Sobem na tela, ao lado da mira");
    hud::DrawMarker(dl, ImVec2(1200, 300), 0, 1.0f, 0.06f, hud::HEALTH, o);
    dl->AddCircle(ImVec2(1200, 300), 15, IM_COL32(255, 255, 255, 150), 40, 1.6f);
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

static void ExtraMove(ImDrawList* dl) {
    // modo Mover: cada coisa com uma moldura tracejada que o mouse arrasta
    struct Box {
        ImVec2 a, b;
        const char* name;
    } boxes[] = {{{1140, 330}, {1470, 482}, "Informer: dado"},
                 {{565, 330}, {895, 482}, "Informer: levado"}};
    for (const Box& b : boxes) {
        for (float x = b.a.x; x < b.b.x; x += 14) {
            dl->AddLine(ImVec2(x, b.a.y), ImVec2(std::min(x + 7, b.b.x), b.a.y), IM_COL32(255, 255, 255, 160), 1.5f);
            dl->AddLine(ImVec2(x, b.b.y), ImVec2(std::min(x + 7, b.b.x), b.b.y), IM_COL32(255, 255, 255, 160), 1.5f);
        }
        for (float y = b.a.y; y < b.b.y; y += 14) {
            dl->AddLine(ImVec2(b.a.x, y), ImVec2(b.a.x, std::min(y + 7, b.b.y)), IM_COL32(255, 255, 255, 160), 1.5f);
            dl->AddLine(ImVec2(b.b.x, y), ImVec2(b.b.x, std::min(y + 7, b.b.y)), IM_COL32(255, 255, 255, 160), 1.5f);
        }
        Small(dl, b.a.x + 4, b.a.y - 20, b.name);
    }
    const char* help = "Arraste cada caixa  |  Esc ou bot\xC3\xA3o direito cancela";
    ImVec2 ts = tui::TextSize(tui::fonts.desc, help);
    ImVec2 c(960, 1000);
    dl->AddRectFilled(ImVec2(c.x - ts.x * 0.5f - 16, c.y - 15), ImVec2(c.x + ts.x * 0.5f + 16, c.y + 15),
                      IM_COL32(14, 16, 20, 225), 15);
    tui::Text(dl, tui::fonts.desc, c.x - ts.x * 0.5f, c.y - ts.y * 0.5f, IM_COL32(255, 255, 255, 220), help);
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
    Check(Lit(1150, 300, 1500, 480, 0.45f) > 300, "informer classico do dano dado (60% x 40%)");
    Check(Lit(570, 300, 900, 480, 0.45f) > 300, "informer classico do dano levado (30% x 40%)");

    // 3. Informer em lista, com a cor fixa no nick (verde ao dar, vermelho ao levar)
    o = opt::Options();
    o.informer = 1, o.nickGiven = 1, o.nickTaken = 1, o.hitLocal = false, o.numbers = 2;
    o.given = {0.58f, 0.37f}, o.taken = {0.32f, 0.37f};
    hud::State s = Fight();
    hud::Line other;
    other.nick = "Victor_Trok", other.id = 12, other.nickColor = IM_COL32(79, 140, 255, 255);
    other.hits = 1, other.total = 10, other.age = 1.4f;
    s.given.push_back(other);
    Frame(s, true, 1.0f);
    Save(dir + "/03-informer-lista.ppm", 440, 300, 1760, 640);

    // 4. Hitmarkers
    o = opt::Options();
    hud::State empty;
    empty.crosshair = CROSS;
    Frame(empty, false, 0.35f, BoardMarkers);
    Save(dir + "/04-hitmarker.ppm", 0, 40, 1120, 760);

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

    // 7. Recebido: dois tiros (frente-esquerda e por tras a direita), aviso de abate e resumo da morte
    s = hud::State();
    s.crosshair = CROSS;
    hud::Incoming in;
    in.angle = -0.95f, in.age = 0.2f, in.damage = 46, in.nick = "Enzo_Rampani", in.nickColor = ENZO;
    s.incoming.push_back(in);
    in.angle = 2.5f, in.age = 0.7f, in.damage = 10, in.nick = "Lucas_Mendes", in.nickColor = LUCAS;
    s.incoming.push_back(in);
    s.flash = 0.8f;
    s.taken = {EnzoLine()};
    Frame(s, true, 1.0f);
    Save(dir + "/07-recebido.ppm");
    Check(Lit(820, 280, 940, 380, 0.5f) > 100, "arco de quem atirou pela frente-esquerda");

    // 8. A mira seguida de verdade: a do GTA (53% x 40%) e a de um mod que centraliza a mira (50% x 50%)
    o = opt::Options();
    o.numbers = 2, o.direction = false, o.edgeFlash = false;
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

    // 9. Modo Mover: cada caixa com um exemplo dentro
    s = Fight();
    Frame(s, true, 0.8f, ExtraMove);
    Save(dir + "/09-mover.ppm", 480, 290, 1560, 1040);

    // 10-11. Menu
    menu::Toggle();
    Menu(0, dir + "/10-menu-mira.ppm", 690, 1230);
    Menu(1, dir + "/11-menu-dano.ppm", 690, 1230);
    Menu(2, dir + "/12-menu-informer.ppm", 690, 1230);
    Menu(4, dir + "/13-menu-sons.ppm", 690, 1230);

    printf("concept: %d falhas\n", failures);
    return failures ? 1 : 0;
}

// Teste de renderizacao sem o jogo: desenha as placas do Trok Veiculos (estados, pneus, dano) e as abas do menu
// com o ImGui de verdade, rasteriza o ImDrawData em software e grava PPMs.
// Uso: ./render_test <pasta de saida> [fundo.rgb 1920x1080]
#include "config.h"
#include "imgui.h"
#include "menu.h"
#include "overlay.h"
#include "trok_ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace veic {
Config cfg; // no jogo, config.cpp (que le o Trok Veiculos.ini)
}

static int W = 1920, H = 1080;
static std::vector<float> fb, background;
static unsigned char* tex;
static int tw, th;
static int failures = 0;

static void Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "ok" : "FALHOU", what);
    if (!ok) {
        ++failures;
    }
}

static void Clear() {
    if (!background.empty()) {
        fb = background;
        return;
    }
    // Fundo liso (o mesmo das imagens do README).
    fb.assign(W * H * 3, 0);
    for (size_t i = 0; i < fb.size(); i += 3) {
        fb[i] = 40 / 255.0f;
        fb[i + 1] = 44 / 255.0f;
        fb[i + 2] = 52 / 255.0f;
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

static void Save(const std::string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (float v : fb) fputc((int)std::min(255.f, std::max(0.f, v * 255.f + 0.5f)), f);
    fclose(f);
}

// A mesma camera dos conceitos (veic-cena.html): ponto de fuga (960, 600), focal 900, olho a 1,7 m. Devolve o meio da
// borda de baixo do veiculo na tela (traseira em z - comprimento/2).
static ImVec2 Bottom(float x, float z, float length) {
    float z0 = z - length * 0.5f;
    return ImVec2(960.0f + 900.0f * x / z0, 600.0f + 900.0f * 1.7f / z0);
}

static veic::Card Make(const char* name, int id, int model, float health, int kind = veic::KIND_CAR) {
    veic::Card c;
    c.name = name;
    c.id = id;
    c.model = model;
    c.health = health;
    c.kind = kind;
    return c;
}

// A cena do conceito: Sultan com pneu furado, Huntley blindado, Infernus pegando fogo com o seu dano, Premier
// vazio e trancado fumacando e a NRG-500.
static std::vector<veic::Card> Scene() {
    std::vector<veic::Card> s;
    veic::Card sultan = Make("Sultan", 12, 560, 1000);
    sultan.at = Bottom(1.8f, 10.0f, 4.4f);
    sultan.distance = 10;
    sultan.tires = veic::TIRE_RR;
    s.push_back(sultan);
    veic::Card huntley = Make("Huntley", 207, 579, 5000);
    huntley.at = Bottom(-1.9f, 12.0f, 4.4f);
    huntley.distance = 12;
    huntley.armourMax = 4000;
    s.push_back(huntley);
    veic::Card infernus = Make("Infernus", 41, 411, 236);
    infernus.at = Bottom(5.2f, 14.0f, 4.4f);
    infernus.distance = 14;
    infernus.ghost = 356;
    infernus.ghostAge = 0.1f;
    infernus.damage = 120;
    infernus.damageAge = 0.3f;
    s.push_back(infernus);
    veic::Card premier = Make("Premier", 88, 426, 520);
    premier.at = Bottom(-5.2f, 17.0f, 4.4f);
    premier.distance = 17;
    premier.empty = true;
    premier.locked = true;
    s.push_back(premier);
    veic::Card nrg = Make("NRG-500", 305, 522, 780, veic::KIND_BIKE);
    nrg.at = Bottom(-4.3f, 7.0f, 1.8f);
    nrg.distance = 7;
    s.push_back(nrg);
    for (veic::Card& c : s) {
        c.limit = 20;
    }
    return s;
}

// Prancha dos estados, 4 por linha.
static std::vector<veic::Card> States() {
    std::vector<veic::Card> s;
    auto add = [&](veic::Card c) {
        int i = static_cast<int>(s.size());
        c.at = ImVec2(240.0f + (i % 4) * 480.0f, 90.0f + (i / 4) * 150.0f);
        c.distance = c.distance > 0 ? c.distance : 8;
        c.limit = 20;
        s.push_back(c);
    };
    add(Make("Sultan", 12, 560, 1000));
    veic::Card armoured = Make("Huntley", 207, 579, 5000);
    armoured.armourMax = 4000;
    add(armoured);
    veic::Card spent = Make("Huntley", 207, 579, 2600);
    spent.armourMax = 4000;
    add(spent);
    add(Make("Premier", 88, 426, 520));
    add(Make("Premier", 88, 426, 330));
    add(Make("Infernus", 41, 411, 236));
    veic::Card hit = Make("Sultan", 12, 560, 622);
    hit.ghost = 742;
    hit.ghostAge = 0.1f;
    hit.damage = 120;
    hit.damageAge = 0.3f;
    add(hit);
    veic::Card parked = Make("Premier", 88, 426, 980);
    parked.empty = true;
    parked.locked = true;
    add(parked);
    veic::Card bike = Make("NRG-500", 305, 522, 780, veic::KIND_BIKE);
    bike.tires = veic::TIRE_REAR;
    add(bike);
    veic::Card far = Make("Sultan", 12, 560, 910);
    far.distance = 18;
    add(far);
    veic::Card flat = Make("Sultan", 12, 560, 1000);
    flat.tires = veic::TIRE_RR;
    add(flat);
    veic::Card front = Make("Premier", 88, 426, 640);
    front.tires = veic::TIRE_FL | veic::TIRE_FR;
    add(front);
    veic::Card all = Make("Huntley", 207, 579, 5000);
    all.armourMax = 4000;
    all.tires = veic::TIRE_FL | veic::TIRE_FR | veic::TIRE_RL | veic::TIRE_RR;
    add(all);
    // O pior caso da linha de cima: o nome mais comprido, ID de 4 digitos, cadeado, pneus e vida de 5 digitos.
    veic::Card longest = Make("Bloodring Banger", 1999, 504, 10000);
    longest.armourMax = 9000;
    longest.locked = true;
    longest.tires = veic::TIRE_FL | veic::TIRE_RR;
    add(longest);
    return s;
}

static int DrawnPixels() {
    int n = 0;
    for (size_t i = 0; i < fb.size(); i += 3) {
        if (fb[i] > 0.5f || fb[i + 1] > 0.5f || fb[i + 2] > 0.5f) ++n;
    }
    return n;
}

// Pixels vermelhos (pneu furado, fogo) num retangulo.
static int RedPixels(int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int y = std::max(0, y0); y < std::min(H, y1); ++y) {
        for (int x = std::max(0, x0); x < std::min(W, x1); ++x) {
            const float* p = &fb[(y * W + x) * 3];
            n += p[0] > 0.7f && p[1] < 0.5f && p[2] < 0.5f;
        }
    }
    return n;
}

static void Overlay(std::vector<veic::Card> cards, const std::string& out, float time = 0.35f) {
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    veic::DrawCards(ImGui::GetBackgroundDrawList(), cards, time);
    ImGui::Render();
    Clear();
    Render(ImGui::GetDrawData());
    Save(out);
}

static void Menu(int tab, const samp::ServerInfo& server, const std::string& out) {
    ImGuiIO& io = ImGui::GetIO();
    // Clica na aba (as 4 dividem a largura do menu, que fica no meio da tela) e espera as animacoes.
    float u = tui::u, width = 640 * u, height = std::min(H * 0.72f, 560 * u);
    float x = W * 0.5f - std::ceil(width) * 0.5f + 18 * u, y = H * 0.5f - std::ceil(height) * 0.5f + tui::HeaderHeight();
    float tabW = (width - 36 * u) / 4;
    for (int f = 0; f < 40; ++f) {
        io.DeltaTime = 1.0f / 60.0f;
        io.AddMousePosEvent(x + tabW * (tab + 0.5f), y + tui::TabsHeight() * 0.5f);
        if (f == 2) io.AddMouseButtonEvent(0, true);
        if (f == 3) io.AddMouseButtonEvent(0, false);
        if (f == 6) io.AddMousePosEvent(10, 10);
        ImGui::NewFrame();
        menu::Frame(server);
        ImGui::Render();
    }
    Clear();
    Render(ImGui::GetDrawData());
    Save(out);
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
    veic::AddFonts("");
    io.Fonts->Build();
    io.Fonts->GetTexDataAsRGBA32(&tex, &tw, &th);
    io.Fonts->SetTexID((ImTextureID)1);

    Overlay(Scene(), dir + "/01_cena.ppm");
    Check(DrawnPixels() > 0, "cena desenhada");

    background.clear();
    std::vector<veic::Card> states = States();
    Overlay(states, dir + "/02_estados.ppm");
    Check(DrawnPixels() > 3000, "estados desenhados");
    // Pneu furado: o traseiro direito do Sultan (estado 11) fica vermelho; sem furo (estado 1), nada vermelho.
    Check(RedPixels(240 - 110, 90 + 2, 240 - 60, 90 + 40) == 0, "sem pneu furado, nada vermelho na placa");
    Check(RedPixels(1680 - 110, 90 + 300 + 2, 1680 - 60, 90 + 300 + 40) > 3, "pneu furado em vermelho");

    veic::cfg.detailModel = veic::cfg.detailDistance = veic::cfg.detailSeats = true;
    veic::cfg.position = 1;
    Overlay(States(), dir + "/03_detalhes_em_cima.ppm");
    veic::cfg = veic::Config();
    veic::cfg.stateColors = false;
    veic::cfg.healthNumber = false;
    Overlay(States(), dir + "/04_sem_cores_sem_numero.ppm");
    veic::cfg = veic::Config();

    samp::ServerInfo server;
    server.connected = true;
    snprintf(server.address, sizeof(server.address), "%s", "127.0.0.1:7777");
    menu::Toggle();
    const char* names[] = {"05_menu_exibir", "06_menu_barras", "07_menu_informacoes", "08_menu_servidor"};
    for (int t = 0; t < 4; ++t) {
        Menu(t, server, dir + "/" + names[t] + ".ppm");
    }
    Check(menu::CapturesInput(), "menu aberto");

    ImGui::DestroyContext();
    printf("%s\n", failures ? "FALHOU" : "PASSOU");
    return failures ? 1 : 0;
}

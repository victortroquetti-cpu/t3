// Teste de renderizacao sem o jogo: desenha as pilhas do Trok Tags (nametags, baloes, textos fixos e
// indicadores) e as abas do menu com o ImGui de verdade, rasteriza o ImDrawData em software e grava PPMs.
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

namespace tags {
Config cfg; // no jogo, config.cpp (que le o Trok Tags.ini)
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

// Projecao da cena de conceito (mesma camera do README): ponto de fuga (960, 600), focal 900, olho a 1,7 m.
static ImVec2 Project(float sx, float z, float height) {
    return ImVec2(sx, 600.0f - 900.0f * (height - 1.7f) / z);
}

static tags::Stack Player(float sx, float z, const char* name, int id, ImU32 color, float health, float armour) {
    tags::Stack st;
    st.at = Project(sx, z, 1.75f + 0.25f);
    st.distance = std::sqrt(z * z + 0.0f);
    st.limit = 70.0f;
    st.hasTag = true;
    st.tag.name = name;
    st.tag.id = id;
    st.tag.color = color;
    st.tag.health = health;
    st.tag.armour = armour;
    return st;
}

static std::vector<tags::Stack> Scene() {
    std::vector<tags::Stack> s;
    tags::Stack a = Player(470, 3.8f, "Victor_Trok", 12, IM_COL32(79, 140, 255, 255), 88, 100);
    a.hasBubble = true;
    a.bubble.text = "[R\xC3\xA1" "dio] Unidade 12 em patrulha na Grove Street, sem novidades.";
    a.bubble.color = IM_COL32(255, 255, 255, 255);
    s.push_back(a);
    tags::Stack f = Player(700, 8.0f, "Lucas_Mendes", 3, IM_COL32(232, 232, 232, 255), 100, 0);
    f.fixed.push_back({"{FF6B6B}[ADMIN] {FFFFFF}Em servi\xC3\xA7o", IM_COL32(255, 255, 255, 255)});
    s.push_back(f);
    tags::Stack b = Player(930, 6.2f, "Enzo_Rampani", 7, IM_COL32(87, 214, 141, 255), 22, 0);
    b.tag.typing = true;
    s.push_back(b);
    tags::Stack e = Player(1075, 17.0f, "Pedro_Henrique", 19, IM_COL32(215, 215, 215, 255), 100, 0);
    e.distance = 56.0f; // perto do limite: so o nome
    s.push_back(e);
    tags::Stack c = Player(1235, 9.0f, "Catharina", 31, IM_COL32(255, 180, 79, 255), 64, 0);
    c.tag.afk = true;
    c.tag.afkSeconds = 151;
    s.push_back(c);
    tags::Stack d = Player(1470, 4.8f, "Camilla_Souza", 44, IM_COL32(199, 146, 255, 255), 100, 35);
    d.tag.ping = 312;
    d.hasBubble = true;
    d.bubble.text = "* Camilla pega o celular e liga para a central.";
    d.bubble.color = IM_COL32(194, 162, 218, 255);
    s.push_back(d);
    return s;
}

// Prancha com os estados lado a lado (fundo liso), cada coluna um caso.
static std::vector<tags::Stack> States() {
    std::vector<tags::Stack> s;
    struct Case {
        const char* name;
        float health, armour;
        bool afk, typing;
        int ping;
        float distance;
    } cases[] = {
        {"Enzo_Rampani", 100, 0, false, false, 0, 10},  {"Victor_Trok", 92, 60, false, false, 0, 10},
        {"Enzo_Rampani", 18, 0, false, false, 0, 10},   {"Enzo_Rampani", 100, 0, false, true, 0, 10},
        {"Catharina", 64, 0, true, false, 0, 10},       {"Camilla_Souza", 100, 0, false, false, 312, 10},
        {"Pedro_Henrique", 100, 0, false, false, 0, 60}, {"[PM]Victor_Trok", 88, 100, false, false, 0, 10},
    };
    int i = 0;
    for (const Case& k : cases) {
        tags::Stack st;
        st.at = ImVec2(150.0f + (i % 4) * 470.0f, 180.0f + (i / 4) * 160.0f);
        st.distance = k.distance;
        st.limit = 70;
        st.hasTag = true;
        st.tag.name = k.name;
        st.tag.id = 7 + i;
        st.tag.color = IM_COL32(87, 214, 141, 255);
        st.tag.health = k.health;
        st.tag.armour = k.armour;
        st.tag.afk = k.afk;
        st.tag.afkSeconds = 151;
        st.tag.typing = k.typing;
        st.tag.ping = k.ping;
        s.push_back(st);
        ++i;
    }
    // Baloes e textos fixos.
    const char* bubbles[] = {
        "Algu\xC3\xA9m viu um Sultan preto passando por aqui?",
        "(( algu\xC3\xA9m sabe o comando do banco? ))",
        "{FFC457}[An\xC3\xBA" "ncio]{FFFFFF} Vendo Sultan preto, 50 mil, chama no celular.",
        "Pessoal, o evento come\xC3\xA7" "a \xC3\xA0s nove em frente \xC3\xA0 prefeitura, tragam os carros j\xC3\xA1 tunados.",
    };
    for (int b = 0; b < 4; ++b) {
        tags::Stack st;
        st.at = ImVec2(250.0f + b * 470.0f, 640.0f);
        st.distance = 10;
        st.hasBubble = true;
        st.bubble.text = bubbles[b];
        st.bubble.color = b == 1 ? IM_COL32(185, 185, 185, 255) : IM_COL32(255, 255, 255, 255);
        s.push_back(st);
    }
    const char* fixed[] = {"RG: 48213", "{FF6B6B}[ADMIN] {FFFFFF}Em servi\xC3\xA7o", "{9AD0FF}Taxista{FFFFFF}\nLivre",
                           "digitando..."};
    for (int f = 0; f < 3; ++f) {
        tags::Stack st;
        st.at = ImVec2(250.0f + f * 470.0f, 900.0f);
        st.distance = 10;
        st.fixed.push_back({fixed[f], IM_COL32(255, 255, 255, 255)});
        s.push_back(st);
    }
    tags::Stack typing;
    typing.at = ImVec2(250.0f + 3 * 470.0f, 900.0f);
    typing.distance = 10;
    typing.hasBubble = true;
    typing.bubble.typing = true;
    s.push_back(typing);
    return s;
}

static int DrawnPixels() {
    int n = 0;
    for (size_t i = 0; i < fb.size(); i += 3) {
        if (fb[i] > 0.5f || fb[i + 1] > 0.5f || fb[i + 2] > 0.5f) ++n;
    }
    return n;
}

static void Overlay(std::vector<tags::Stack> stacks, const std::string& out) {
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    tags::DrawStacks(ImGui::GetBackgroundDrawList(), stacks, 0.35f);
    ImGui::Render();
    Clear();
    Render(ImGui::GetDrawData());
    Save(out);
}

static void Menu(int tab, const samp::ServerInfo& server, const std::string& out) {
    ImGuiIO& io = ImGui::GetIO();
    // Clica na aba (as 5 dividem a largura do menu, que fica no meio da tela) e espera as animacoes.
    float u = tui::u, width = 640 * u, height = std::min(H * 0.72f, 560 * u);
    float x = W * 0.5f - std::ceil(width) * 0.5f + 18 * u, y = H * 0.5f - std::ceil(height) * 0.5f + tui::HeaderHeight();
    float tabW = (width - 36 * u) / 5;
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
    tags::AddFonts("");
    io.Fonts->Build();
    io.Fonts->GetTexDataAsRGBA32(&tex, &tw, &th);
    io.Fonts->SetTexID((ImTextureID)1);

    Overlay(Scene(), dir + "/01_cena.ppm");
    Check(DrawnPixels() > 0, "cena desenhada");

    std::vector<tags::Stack> states = States();
    background.clear();
    Overlay(states, dir + "/02_estados.ppm");
    int pill = DrawnPixels();
    Check(pill > 2000, "estados desenhados");

    tags::cfg.style = 1;
    Overlay(States(), dir + "/03_estilo_limpo.ppm");
    tags::cfg.style = 0;
    tags::cfg.colorMode = 1;
    tags::cfg.fixedChip = 1;
    Overlay(States(), dir + "/04_nome_colorido_etiqueta_azul.ppm");
    tags::cfg = tags::Config();

    // Etiquetas e texto: o que vira etiqueta e o que e digitando.
    Check(tags::IsTypingText("digitando...") && tags::IsTypingText("{AAAAAA}* digitando *") &&
              tags::IsTypingText("...") && tags::IsTypingText("Typing...") && tags::IsTypingText("est\xC3\xA1 digitando"),
          "baloes de digitando reconhecidos");
    Check(!tags::IsTypingText("Oi") && !tags::IsTypingText("digitando o codigo") && !tags::IsTypingText("."),
          "fala normal nao vira digitando");

    samp::ServerInfo server;
    server.connected = true;
    snprintf(server.address, sizeof(server.address), "%s", "127.0.0.1:7777");
    server.nameTagDistance = 70;
    server.nameTagLos = true;
    server.bubbles = 1;
    menu::Toggle();
    const char* names[] = {"05_menu_nametag", "06_menu_baloes", "07_menu_fixos", "08_menu_indicadores", "09_menu_servidor"};
    for (int t = 0; t < 5; ++t) {
        Menu(t, server, dir + "/" + names[t] + ".ppm");
    }
    Check(menu::CapturesInput(), "menu aberto");

    ImGui::DestroyContext();
    printf("%s\n", failures ? "FALHOU" : "PASSOU");
    return failures ? 1 : 0;
}

// Trok KeyBinder -- teste de renderizacao sem o jogo: as abas do menu, os editores (com os conflitos em vermelho), o
// menu circular e a bolinha da mira, com o ImGui de verdade rasterizado em software. Cliques e teclas simulados
// conferem o que o jogador faria: salvar com conflito nao salva, o circulo devolve a bind clicada, segurar e soltar.
// Uso: ./render_test <pasta de saida> [cena.rgb 1920x1080 com um jogador no meio]
#include "binds.h"
#include "icons.h"
#include "imgui.h"
#include "menu.h"
#include "radial.h"
#include "trok_ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using namespace kb;

static int W = 1920, H = 1080;
static std::vector<float> fb, scene;
static unsigned char* tex;
static int tw, th;
static int failures = 0;

static void Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "ok" : "FALHOU", what);
    if (!ok) {
        ++failures;
    }
}

static void Clear(bool useScene) {
    if (useScene && !scene.empty()) {
        fb = scene;
        return;
    }
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

// Pixels "vermelho do aviso" (FF6B6B) na imagem.
static int RedPixels() {
    int n = 0;
    for (size_t i = 0; i < fb.size(); i += 3) {
        if (fb[i] > 0.84f && fb[i + 1] > 0.29f && fb[i + 1] < 0.55f && fb[i + 2] > 0.29f && fb[i + 2] < 0.55f) ++n;
    }
    return n;
}

// ---------------------------------------------------------------- dados

static Bind KeyBind(const char* name, const char* text, ImGuiKey key, int mods, int mode, bool inMenu, int cls = CLASS_KEY) {
    Bind b;
    b.cls = cls;
    b.name = name;
    b.text = text;
    b.key = {key, mods};
    b.mode = mode;
    b.inMenu = inMenu;
    return b;
}

static Bind Auto(const char* name, const char* text, int cond, int delay, int value = 50, int seat = SEAT_ANY) {
    Bind b;
    b.cls = CLASS_AUTO;
    b.name = name;
    b.text = text;
    b.cond = cond;
    b.delay = delay;
    b.value = value;
    b.seat = seat;
    return b;
}

static Data MakeData() {
    Data d;
    d.settings.radial = {ImGuiKey_X, 0};
    Preset pol;
    pol.name = "hzrp policia";
    pol.binds.push_back(KeyBind("Sauda\xC3\xA7\xC3\xA3o", "oi galera, tudo bem?", ImGuiKey_L, HK_CTRL, MODE_PRESS, true));
    pol.binds.push_back(KeyBind("Lanterna", "/lanterna", ImGuiKey_F, 0, MODE_DOUBLE, false));
    pol.binds.push_back(KeyBind("Reparar", "/reparar\n/me conserta o motor", ImGuiKey_R, 0, MODE_HOLD, true));
    pol.binds.push_back(KeyBind("Motor", "/motor", ImGuiKey_None, 0, MODE_PRESS, true));
    pol.binds.push_back(KeyBind("Kit m\xC3\xA9" "dico", "/usarkit", ImGuiKey_K, 0, MODE_HOLD, false));
    pol.binds.push_back(Auto("Trancar", "/tranca", C_ENTER_VEHICLE, 0));
    pol.binds.push_back(Auto("Cinto", "/cinto", C_ENTER_VEHICLE, 1000, 50, SEAT_DRIVER));
    pol.binds.push_back(Auto("Farol", "/farol", C_ENTER_VEHICLE, 1000));
    pol.binds.push_back(Auto("Kit na vida baixa", "/usarkit", C_HEALTH, 0, 30));
    Bind atender = Auto("Atender", "/atender (id)", C_CHAT, 1500);
    atender.trigger = "est\xC3\xA1 precisando de atendimento";
    pol.binds.push_back(atender);
    pol.binds.push_back(Auto("Saca a Deagle", "/me saca a Desert Eagle", C_WEAPON, 500, 24));
    Bind algemar = KeyBind("Algemar", "/algemar (id)", ImGuiKey_K, 0, MODE_PRESS, true, CLASS_AROUND);
    algemar.radius = 3;
    pol.binds.push_back(algemar);
    Bind revistar = KeyBind("Revistar", "/revistar (id)", ImGuiKey_None, 0, MODE_PRESS, true, CLASS_AROUND);
    pol.binds.push_back(revistar);
    Bind multar = KeyBind("Multar todos", "/multar (id) 500", ImGuiKey_M, HK_SHIFT, MODE_PRESS, false, CLASS_AROUND);
    multar.radius = 10;
    multar.target = TARGET_ALL;
    multar.ignore = {"Lucas_Mendes"};
    pol.binds.push_back(multar);
    pol.binds.push_back(KeyBind("Mandar parar", "/m (nick), encoste o ve\xC3\xAD" "culo!", ImGuiKey_None, 0, MODE_PRESS, true,
                            CLASS_VIEW));
    pol.binds.push_back(KeyBind("Algemar alvo", "/algemar (id)", ImGuiKey_None, 0, MODE_PRESS, true, CLASS_VIEW));
    pol.binds.push_back(KeyBind("Documentos", "/rg (id)", ImGuiKey_None, 0, MODE_PRESS, true, CLASS_VIEW));
    pol.binds.push_back(KeyBind("Mensagem", "/pm (id) Ol\xC3\xA1 (nick), tudo certo?", ImGuiKey_None, 0, MODE_PRESS, true,
                            CLASS_VIEW));
    const char* const iconOf[][2] = {{"Sauda\xC3\xA7\xC3\xA3o", "hand"}, {"Lanterna", "flashlight"}, {"Reparar", "wrench"},
                                    {"Motor", "power"}, {"Kit m\xC3\xA9" "dico", "cross"}, {"Algemar", "link"},
                                    {"Revistar", "search"}, {"Mandar parar", "megaphone"}, {"Algemar alvo", "link"},
                                    {"Documentos", "id-card"}, {"Mensagem", "message-circle"}};
    for (Bind& b : pol.binds) {
        for (const auto& io : iconOf) {
            if (b.name == io[0]) b.icon = io[1];
        }
    }
    d.presets.push_back(pol);
    Preset lad;
    lad.name = "hzrp ladrao";
    lad.binds.push_back(KeyBind("Assalto", "/assaltar", ImGuiKey_G, 0, MODE_HOLD, true));
    d.presets.push_back(lad);
    Preset bpl;
    bpl.name = "Brasil Play Life";
    d.presets.push_back(bpl);
    d.servers.push_back({"10.0.0.1:7777", "Horizonte Roleplay | Servidor 1", "hzrp policia"});
    d.servers.push_back({"10.0.0.2:7777", "Horizonte Roleplay | Servidor 2", "hzrp ladrao"});
    d.servers.push_back({"10.0.0.3:7777", "Horizonte Roleplay | Servidor 3", "hzrp policia"});
    d.servers.push_back({"10.0.0.4:7777", "Horizonte Roleplay | Servidor 4", "hzrp ladrao"});
    d.servers.push_back({"20.0.0.1:7777", "Brasil Play Life", "Brasil Play Life"});
    d.lastPreset = "hzrp policia";
    return d;
}

static Data g_data;

static menu::Context MakeContext() {
    menu::Context c;
    c.data = &g_data;
    c.preset = 0;
    c.connected = true;
    c.address = "10.0.0.1:7777";
    c.hostname = "Horizonte Roleplay | Servidor 1";
    c.players = {{12, "Fulano_Silva", 0}, {3, "Lucas_Mendes", 2.4f}, {7, "Camilla_Souza", 4.1f},
                 {19, "Pedro_Henrique", 8.7f}, {31, "Catharina", 0}};
    return c;
}

// ---------------------------------------------------------------- quadros

static void Key(ImGuiIO& io, ImGuiKey k, bool down) {
    io.AddKeyEvent(k, down);
}

static float MenuW() {
    return 760 * tui::u;
}

static float MenuH() {
    return std::min(H * 0.8f, 620 * tui::u);
}

// Roda n quadros do menu (com os avisos), chamando input antes de cada um.
static void MenuFrames(int n, const std::function<void(int, ImGuiIO&)>& input) {
    ImGuiIO& io = ImGui::GetIO();
    for (int f = 0; f < n; ++f) {
        io.DeltaTime = 1.0f / 60.0f;
        input(f, io);
        ImGui::NewFrame();
        menu::Context c = MakeContext();
        menu::Frame(c);
        tui::DrawToasts();
        ImGui::Render();
    }
}

static void Shot(const std::string& path, bool useScene = false) {
    Clear(useScene);
    Render(ImGui::GetDrawData());
    Save(path);
}

// Clica na aba (as 7 dividem a largura do menu, que fica no meio da tela) e espera as animacoes.
static void GoTab(int tab) {
    float u = tui::u, width = MenuW(), height = MenuH();
    float x = W * 0.5f - std::ceil(width) * 0.5f + 18 * u, y = H * 0.5f - std::ceil(height) * 0.5f + tui::HeaderHeight();
    float tabW = (width - 36 * u) / 7;
    MenuFrames(30, [&](int f, ImGuiIO& io) {
        io.AddMousePosEvent(x + tabW * (tab + 0.5f), y + tui::TabsHeight() * 0.5f);
        if (f == 2) io.AddMouseButtonEvent(0, true);
        if (f == 3) io.AddMouseButtonEvent(0, false);
        if (f == 6) io.AddMousePosEvent(10, 10);
    });
}

// Clica na linha (1 = a primeira) da lista da aba: cada linha tem 30u + 2u de folga, abaixo das abas.
static void OpenRow(int row) {
    float u = tui::u;
    float top = H * 0.5f - std::ceil(MenuH()) * 0.5f + tui::HeaderHeight() + tui::TabsHeight() + 10 * u;
    float y = top + (row - 1) * 32 * u + 15 * u;
    MenuFrames(6, [&](int f, ImGuiIO& io) {
        io.AddMousePosEvent(W * 0.5f - 120 * u, y);
        if (f == 2) io.AddMouseButtonEvent(0, true);
        if (f == 3) io.AddMouseButtonEvent(0, false);
        if (f == 4) io.AddMousePosEvent(10, 10);
    });
    MenuFrames(25, [](int, ImGuiIO&) {});
}

static void Press(ImGuiKey k, int times = 1) {
    for (int i = 0; i < times; ++i) {
        MenuFrames(3, [&](int f, ImGuiIO& io) {
            if (f == 0) Key(io, k, true);
            if (f == 1) Key(io, k, false);
        });
    }
}

// Sai do editor (Esc ate fechar; o menu fica aberto).
static void Leave(int escapes) {
    Press(ImGuiKey_Escape, escapes);
    if (!menu::IsOpen()) {
        menu::Toggle();
        MenuFrames(2, [](int, ImGuiIO&) {});
    }
}

static int RadialFrames(int n, ImVec2 mouse, bool keyDown, const std::function<void(int, ImGuiIO&)>& input,
                        float* clock) {
    ImGuiIO& io = ImGui::GetIO();
    int chosen = -1;
    for (int f = 0; f < n; ++f) {
        io.DeltaTime = 1.0f / 60.0f;
        io.AddMousePosEvent(mouse.x, mouse.y);
        input(f, io);
        ImGui::NewFrame();
        *clock += 1.0f / 60.0f;
        int c = radial::Frame(ImGui::GetForegroundDrawList(), ImVec2(W * 0.5f, H * 0.5f), *clock, keyDown, false, 100);
        if (c >= 0) chosen = c;
        ImGui::Render();
    }
    return chosen;
}

static std::vector<radial::Item> Items(const Preset& p, bool target) {
    std::vector<radial::Item> items;
    for (int pass = 0; pass < 2; ++pass) {
        for (size_t i = 0; i < p.binds.size(); ++i) {
            const Bind& b = p.binds[i];
            bool view = b.cls == CLASS_VIEW;
            if (!b.enabled || !b.inMenu || b.cls == CLASS_AUTO || view != (pass == 0) || (view && !target)) continue;
            items.push_back({b.name, b.cls == CLASS_AROUND ? "Ao redor" : "", static_cast<int>(i), icons::Find(b.icon)});
        }
    }
    return items;
}

// Centro da fatia i de n (0 = em cima, sentido horario).
static ImVec2 Slice(int i, int n) {
    float a = -3.14159265f * 0.5f + i * 2 * 3.14159265f / n;
    float r = 139 * tui::u;
    return ImVec2(W * 0.5f + std::cos(a) * r, H * 0.5f + std::sin(a) * r);
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : ".";
    if (argc > 2) {
        FILE* f = fopen(argv[2], "rb");
        if (f) {
            std::vector<unsigned char> raw(W * H * 3);
            if (fread(raw.data(), 1, raw.size(), f) == raw.size()) {
                scene.resize(raw.size());
                for (size_t i = 0; i < raw.size(); ++i) scene[i] = raw[i] / 255.0f;
            }
            fclose(f);
        }
    }
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)W, (float)H);
    io.IniFilename = nullptr;
    tui::BuildFonts((float)H, "");
    io.Fonts->Build();
    io.Fonts->GetTexDataAsRGBA32(&tex, &tw, &th);
    io.Fonts->SetTexID((ImTextureID)1);

    g_data = MakeData();
    menu::Toggle();
    const char* names[] = {"01_padrao", "02_automatico", "03_ao_redor", "04_na_visao", "05_macro", "06_presets", "07_geral"};
    for (int t = 0; t < 7; ++t) {
        GoTab(t);
        Shot(dir + "/" + names[t] + ".ppm");
        if (t == 0) Check(RedPixels() > 60, "Padr\xC3\xA3o: K repetido com Ao redor fica vermelho");
        if (t == 1) Check(RedPixels() > 60, "Autom\xC3\xA1tico: Farol no mesmo tempo do Cinto fica vermelho");
        if (t == 4) Check(RedPixels() < 50, "Macro: sem conflito na tela");
    }
    Check(menu::IsOpen(), "menu aberto");

    // Automatico: abre o "Farol" (linha 1 preset, 2 aviso, 3 como funciona, 4 Trancar, 5 Cinto, 6 Farol).
    GoTab(1);
    OpenRow(6);
    Shot(dir + "/08_editor_tempo_repetido.ppm");
    Check(RedPixels() > 60, "editor: o tempo repetido em vermelho");
    size_t before = g_data.presets[0].binds.size();
    std::string farolBefore = g_data.presets[0].binds[7].text;
    MenuFrames(30, [](int f, ImGuiIO& io) {
        // O backend do Win32 manda a tecla e o modificador (io.KeyCtrl vem do ImGuiMod_Ctrl).
        if (f == 2) { Key(io, ImGuiKey_LeftCtrl, true); Key(io, ImGuiMod_Ctrl, true); Key(io, ImGuiKey_S, true); }
        if (f == 3) { Key(io, ImGuiKey_S, false); Key(io, ImGuiKey_LeftCtrl, false); Key(io, ImGuiMod_Ctrl, false); }
    });
    Shot(dir + "/09_salvar_bloqueado.ppm");
    Check(tui::ToastsVisible() && g_data.presets[0].binds.size() == before && g_data.presets[0].binds[7].text == farolBefore,
          "Ctrl+S com tempo repetido: nao salva, avisa");
    Leave(1);

    // Atender (chat): linha 8 = Atender; no editor, a linha 8 e "Testar com uma mensagem".
    GoTab(1);
    OpenRow(8);
    Press(ImGuiKey_DownArrow, 7);
    Press(ImGuiKey_Enter);
    MenuFrames(10, [](int f, ImGuiIO& io) {
        if (f == 2) io.AddInputCharactersUTF8("{FF9900}[SOS]{FFFFFF} Fulano_Silva (12) est\xC3\xA1 prec{33AA33}isando de atendimento");
    });
    MenuFrames(20, [](int, ImGuiIO&) {});
    Shot(dir + "/10_editor_chat_teste.ppm");
    Leave(2);

    // Padrao: "Kit medico" (linha 1 preset, 2 aviso, 3 como funciona, 4..8 binds: Kit e a 8).
    GoTab(0);
    OpenRow(8);
    Shot(dir + "/11_editor_tecla_repetida.ppm");
    Check(RedPixels() > 60, "editor: tecla repetida (outra aba) em vermelho");
    Leave(1);

    // Ao redor: "Multar todos" (linha 1 preset, 2 aviso, 3 como funciona, 4 Algemar, 5 Revistar, 6 Multar), pelo
    // numero da linha, como nas listas da casa.
    GoTab(2);
    Press(ImGuiKey_6);
    MenuFrames(20, [](int, ImGuiIO&) {});
    Shot(dir + "/12_editor_ao_redor.ppm");
    Leave(1);

    // O aviso de conflito e acessivel: "Ver" (linha 2, Enter) abre a proxima bind em conflito, na aba dela.
    GoTab(0);
    Press(ImGuiKey_2);
    Press(ImGuiKey_Enter);
    MenuFrames(10, [](int, ImGuiIO&) {});
    std::string firstClash = menu::EditingName();
    Leave(1);
    Press(ImGuiKey_Enter); // a selecao continua no "Ver"
    MenuFrames(10, [](int, ImGuiIO&) {});
    std::string secondClash = menu::EditingName();
    Shot(dir + "/17_ver_conflito.ppm");
    Check(firstClash == "Kit m\xC3\xA9" "dico" && secondClash == "Cinto",
          "\"Ver\" abre a proxima bind em conflito (Kit medico, depois Cinto na aba Automatico)");
    Leave(1);

    // O icone: na Saudacao (linha 4 do Padrao), a linha 7 do editor abre a grade; escolher o carro e salvar.
    GoTab(0);
    OpenRow(4);
    Press(ImGuiKey_7);
    Press(ImGuiKey_Enter);
    MenuFrames(10, [](int, ImGuiIO&) {});
    ImVec2 carCell = menu::IconCell(1 + icons::Find("car"));
    MenuFrames(6, [&](int f, ImGuiIO& io) { io.AddMousePosEvent(carCell.x, carCell.y); });
    Shot(dir + "/18_seletor_icones.ppm");
    MenuFrames(6, [&](int f, ImGuiIO& io) {
        io.AddMousePosEvent(carCell.x, carCell.y);
        if (f == 1) io.AddMouseButtonEvent(0, true);
        if (f == 2) io.AddMouseButtonEvent(0, false);
    });
    MenuFrames(10, [](int f, ImGuiIO& io) {
        if (f == 2) { Key(io, ImGuiKey_LeftCtrl, true); Key(io, ImGuiMod_Ctrl, true); Key(io, ImGuiKey_S, true); }
        if (f == 3) { Key(io, ImGuiKey_S, false); Key(io, ImGuiKey_LeftCtrl, false); Key(io, ImGuiMod_Ctrl, false); }
    });
    Check(carCell.x > 0 && g_data.presets[0].binds[0].icon == "car" && menu::EditingName().empty(),
          "grade de icones: o carro escolhido e salvo na bind");
    g_data.presets[0].binds[0].icon = "hand";

    // Geral com o atalho do circulo igual ao de uma bind: recusado.
    GoTab(6);
    Hotkey radialBefore = g_data.settings.radial;
    Press(ImGuiKey_DownArrow);
    Press(ImGuiKey_Enter);
    Press(ImGuiKey_F);
    Check(g_data.settings.radial == radialBefore && tui::ToastsVisible(),
          "atalho do circulo igual a tecla da Lanterna: recusado com aviso");
    menu::Toggle();
    Check(!menu::IsOpen(), "menu fechado");

    // Circulo sem alvo, mouse na 3a fatia.
    float clock = 10.0f;
    std::vector<radial::Item> items = Items(g_data.presets[0], false);
    radial::Open(items, "Binds", "", clock);
    ImVec2 third = Slice(2, static_cast<int>(items.size()));
    RadialFrames(20, third, true, [](int, ImGuiIO&) {}, &clock);
    Shot(dir + "/13_circulo.ppm", true);
    int chosen = RadialFrames(4, third, true, [](int f, ImGuiIO& io) {
        if (f == 1) io.AddMouseButtonEvent(0, true);
        if (f == 2) io.AddMouseButtonEvent(0, false);
    }, &clock);
    Check(chosen == items[2].bind && !radial::IsOpen(), "clique na fatia roda a bind dela e fecha");

    // Com alvo: as binds da visao primeiro e o nome no meio; segurar o atalho, apontar e soltar.
    items = Items(g_data.presets[0], true);
    radial::Open(items, "Fulano_Silva", "12", clock);
    ImVec2 first = Slice(0, std::min(radial::PAGE, static_cast<int>(items.size())));
    RadialFrames(30, first, true, [](int, ImGuiIO&) {}, &clock);
    Shot(dir + "/14_circulo_alvo.ppm", true);
    chosen = RadialFrames(2, first, false, [](int, ImGuiIO&) {}, &clock);
    Check(chosen == items[0].bind && g_data.presets[0].binds[chosen].cls == CLASS_VIEW,
          "segurar, apontar e soltar roda a bind da visao");
    Check(items.size() > static_cast<size_t>(radial::PAGE), "mais de 8 binds: paginas");

    // O numero da fatia (2) roda a segunda.
    items = Items(g_data.presets[0], false);
    radial::Open(items, "Binds", "", clock);
    RadialFrames(10, ImVec2(10, 10), true, [](int, ImGuiIO&) {}, &clock);
    chosen = RadialFrames(4, ImVec2(10, 10), true, [](int f, ImGuiIO& io) {
        if (f == 1) Key(io, ImGuiKey_2, true);
        if (f == 2) Key(io, ImGuiKey_2, false);
    }, &clock);
    Check(chosen == items[1].bind, "circulo: a tecla 2 roda a segunda fatia");

    // A lista presa no jogador da mira, saindo da bolinha do meio dele (905, 534 na cena).
    std::vector<radial::Item> view;
    for (size_t i = 0; i < g_data.presets[0].binds.size(); ++i) {
        const Bind& b = g_data.presets[0].binds[i];
        if (b.cls == CLASS_VIEW && b.inMenu) view.push_back({b.name, "", static_cast<int>(i), icons::Find(b.icon)});
    }
    ImVec2 anchor(905, 534);
    auto list = [&](int n, ImVec2 mouse, bool keyDown, bool visible, const std::function<void(int, ImGuiIO&)>& input) {
        int got = -1;
        for (int f = 0; f < n; ++f) {
            io.DeltaTime = 1.0f / 60.0f;
            io.AddMousePosEvent(mouse.x, mouse.y);
            input(f, io);
            ImGui::NewFrame();
            clock += 1.0f / 60.0f;
            int c = radial::ListFrame(ImGui::GetForegroundDrawList(), anchor, visible, clock, keyDown, false, 100);
            if (c >= 0) got = c;
            ImGui::Render();
        }
        return got;
    };
    radial::OpenList(view, "Fulano_Silva", "12", clock);
    list(20, ImVec2(10, 10), true, true, [](int, ImGuiIO&) {});
    ImVec2 second = radial::ListItemCenter(1);
    list(10, second, true, true, [](int, ImGuiIO&) {});
    Shot(dir + "/16_lista_no_jogador.ppm", true);
    Check(second.x > anchor.x && std::fabs(second.y - anchor.y) < 80, "lista do lado direito da bolinha, na altura dela");
    anchor = ImVec2(1005, 520); // o jogador andou: a lista vai junto
    list(2, ImVec2(10, 10), true, true, [](int, ImGuiIO&) {});
    ImVec2 moved = radial::ListItemCenter(1);
    Check(std::fabs((moved.x - second.x) - 100) < 2 && std::fabs((moved.y - second.y) + 14) < 2,
          "lista presa no jogador: anda junto com ele");
    chosen = list(4, moved, true, true, [](int f, ImGuiIO& io) {
        if (f == 1) io.AddMouseButtonEvent(0, true);
        if (f == 2) io.AddMouseButtonEvent(0, false);
    });
    Check(chosen == view[1].bind && !radial::ListOpen(), "lista: clique no item roda a bind dele e fecha");
    radial::OpenList(view, "Fulano_Silva", "12", clock);
    list(10, ImVec2(10, 10), false, true, [](int, ImGuiIO&) {});
    chosen = list(4, ImVec2(10, 10), false, true, [](int f, ImGuiIO& io) {
        if (f == 1) Key(io, ImGuiKey_3, true);
        if (f == 2) Key(io, ImGuiKey_3, false);
    });
    Check(chosen == view[2].bind, "lista: a tecla 3 roda o terceiro item");
    radial::OpenList(view, "Fulano_Silva", "12", clock);
    list(10, ImVec2(10, 10), false, true, [](int, ImGuiIO&) {});
    list(10, ImVec2(10, 10), false, false, [](int, ImGuiIO&) {});
    bool waits = radial::ListOpen();
    list(20, ImVec2(10, 10), false, false, [](int, ImGuiIO&) {});
    Check(waits && !radial::ListOpen(), "jogador some da visao: a lista espera 0,3 s e fecha");
    anchor = ImVec2(W - 60.0f, 520);
    radial::OpenList(view, "Fulano_Silva", "12", clock);
    list(20, ImVec2(10, 10), false, true, [](int, ImGuiIO&) {});
    Check(radial::ListItemCenter(0).x < anchor.x, "perto da borda direita a lista abre do lado esquerdo");
    radial::Close();

    // Toque rapido deixa aberto; Esc fecha.
    radial::Open(items, "Binds", "", clock);
    RadialFrames(3, ImVec2(W * 0.5f, H * 0.5f), true, [](int, ImGuiIO&) {}, &clock);
    RadialFrames(3, ImVec2(W * 0.5f, H * 0.5f), false, [](int, ImGuiIO&) {}, &clock);
    bool stays = radial::IsOpen();
    RadialFrames(4, ImVec2(W * 0.5f, H * 0.5f), false, [](int f, ImGuiIO& io) {
        if (f == 1) Key(io, ImGuiKey_Escape, true);
        if (f == 2) Key(io, ImGuiKey_Escape, false);
    }, &clock);
    Check(stays && !radial::IsOpen(), "toque rapido deixa aberto; Esc fecha");

    // A bolinha no jogador da mira (o do meio da cena).
    for (int f = 0; f < 3; ++f) {
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();
        radial::DrawDot(ImGui::GetBackgroundDrawList(), ImVec2(905, 534), 0.2f);
        ImGui::Render();
    }
    Shot(dir + "/15_bolinha.ppm", true);

    ImGui::DestroyContext();
    printf("%s\n", failures ? "FALHOU" : "PASSOU");
    return failures ? 1 : 0;
}

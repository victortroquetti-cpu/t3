// Teste de renderizacao do chat sem o jogo (Linux): roda chat.cpp no ImGui com um SA-MP falso (mensagens de um
// servidor de RP, com {RRGGBB} e [tags]), rasteriza o ImDrawData em software e grava PPMs. O rasterizador e o mesmo
// do teste da vitrine do Trok UI (trok-ui/asi/test/render_test.cpp). chat.cpp entra inteiro aqui para o teste conferir
// tambem a quebra das mensagens em pedacos (Tokenize) e o estado da barra.
// Uso: ./render_test <pasta de saida>
#include "chat.cpp"
#include "imgui_internal.h"
#include "menu.h"
#include "options.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// ------------------------------------------------------------------------------------------------ SA-MP falso
namespace {
host::ChatSettings g_cfg;
std::vector<std::string> g_sent;
int g_inputOpened = 0, g_inputClosed = 0;
bool g_inputFlag = false;
bool g_dialog = false;
int g_wheel = 0;
} // namespace

namespace host {
ChatSettings Settings() {
    return g_cfg;
}
void Apply(const ChatSettings& s) {
    g_cfg = s;
}
void Send(const char* utf8) {
    g_sent.push_back(utf8);
}
void InputOpened(bool open) {
    (open ? g_inputOpened : g_inputClosed)++;
    g_inputFlag = open;
}
bool DialogOpen() {
    return g_dialog;
}
int Wheel() {
    const int w = g_wheel;
    g_wheel = 0;
    return w;
}
} // namespace host

// ------------------------------------------------------------------------------------------------ rasterizador
static int W = 1600, H = 900;
static std::vector<float> fb;
static unsigned char* tex;
static int tw, th;

// Fundo de "jogo": ceu claro em cima (o pior caso para texto branco), chao escuro embaixo.
static void Clear() {
    fb.assign(W * H * 3, 0);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float* p = &fb[(y * W + x) * 3];
            if (y < H * 0.55f) {
                const float t = y / (H * 0.55f);
                p[0] = 0.48f + 0.30f * t + 0.05f * x / W;
                p[1] = 0.64f + 0.20f * t;
                p[2] = 0.82f + 0.08f * t;
            } else {
                const float t = (y - H * 0.55f) / (H * 0.45f);
                p[0] = 0.34f - 0.12f * t;
                p[1] = 0.33f - 0.10f * t;
                p[2] = 0.30f - 0.08f * t;
            }
        }
    }
}

static void Raster(const ImDrawVert& a, const ImDrawVert& b, const ImDrawVert& c, ImVec4 clip) {
    float minx = std::max(clip.x, std::floor(std::min({a.pos.x, b.pos.x, c.pos.x})));
    float maxx = std::min(clip.z, std::ceil(std::max({a.pos.x, b.pos.x, c.pos.x})));
    float miny = std::max(clip.y, std::floor(std::min({a.pos.y, b.pos.y, c.pos.y})));
    float maxy = std::min(clip.w, std::ceil(std::max({a.pos.y, b.pos.y, c.pos.y})));
    float area = (b.pos.x - a.pos.x) * (c.pos.y - a.pos.y) - (b.pos.y - a.pos.y) * (c.pos.x - a.pos.x);
    if (std::fabs(area) < 1e-6f) {
        return;
    }
    for (int y = (int)miny; y < (int)maxy; ++y) {
        for (int x = (int)minx; x < (int)maxx; ++x) {
            if (x < 0 || y < 0 || x >= W || y >= H) {
                continue;
            }
            float px = x + 0.5f, py = y + 0.5f;
            float w0 = ((b.pos.x - px) * (c.pos.y - py) - (b.pos.y - py) * (c.pos.x - px)) / area;
            float w1 = ((c.pos.x - px) * (a.pos.y - py) - (c.pos.y - py) * (a.pos.x - px)) / area;
            float w2 = 1 - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) {
                continue;
            }
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
            for (int i = 0; i < 3; ++i) {
                p[i] = p[i] * (1 - col[3]) + col[i] * col[3];
            }
        }
    }
}

static void Render(ImDrawData* dd) {
    for (int n = 0; n < dd->CmdListsCount; ++n) {
        const ImDrawList* cl = dd->CmdLists[n];
        for (const ImDrawCmd& cmd : cl->CmdBuffer) {
            if (cmd.UserCallback) {
                continue;
            }
            for (unsigned i = 0; i < cmd.ElemCount; i += 3) {
                const ImDrawIdx* idx = &cl->IdxBuffer[cmd.IdxOffset + i];
                Raster(cl->VtxBuffer[cmd.VtxOffset + idx[0]], cl->VtxBuffer[cmd.VtxOffset + idx[1]],
                       cl->VtxBuffer[cmd.VtxOffset + idx[2]], cmd.ClipRect);
            }
        }
    }
}

// So o canto de cima da tela (onde o chat fica), para as imagens ficarem leves.
static int g_saveW = 1000, g_saveH = 470;

static void Save(const std::string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    fprintf(f, "P6\n%d %d\n255\n", g_saveW, g_saveH);
    for (int y = 0; y < g_saveH; ++y) {
        for (int x = 0; x < g_saveW; ++x) {
            for (int i = 0; i < 3; ++i) {
                fputc((int)std::min(255.f, std::max(0.f, fb[(y * W + x) * 3 + i] * 255.f + 0.5f)), f);
            }
        }
    }
    fclose(f);
}

// ------------------------------------------------------------------------------------------------ roteiro
static std::string g_dir;
static int g_fail = 0;

static void Check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FALHA", what);
    g_fail += !ok;
}

static void Frame(const char* shot = nullptr) {
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    chat::Frame();
    if (menu::WantsFrame()) {
        menu::Frame();
    }
    ImGui::Render();
    if (shot) {
        Clear();
        Render(ImGui::GetDrawData());
        Save(g_dir + "/" + shot + ".ppm");
    }
}

static void Idle(int n) {
    for (int i = 0; i < n; ++i) {
        Frame();
    }
}

static void Key(ImGuiKey k) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(k, true);
    Frame();
    io.AddKeyEvent(k, false);
    Frame();
}

static void Type(const char* utf8) {
    ImGui::GetIO().AddInputCharactersUTF8(utf8);
    Frame();
}

static long g_clock = 1700000000;

static void Say(int type, const char* text, uint32_t textColor = 0xFFFFFFFF, const char* prefix = "",
                uint32_t prefixColor = 0xFFFFFFFF) {
    chat::Message m;
    m.type = type;
    m.time = g_clock;
    g_clock += 7;
    m.text = text;
    m.prefix = prefix;
    m.textColor = textColor;
    m.prefixColor = prefixColor;
    chat::Add(m);
}

static void Player(const char* nick, uint32_t color, const char* text) {
    Say(chat::TYPE_CHAT, text, 0xFFFFFFFF, nick, color);
}

// Pedacos de uma mensagem, como texto: [tag] para as tags e "_" para o espaco antes do pedaco.
static std::string Pieces(const chat::Message& m) {
    std::string out;
    for (const auto& t : chat::Tokenize(m)) {
        if (!out.empty()) {
            out += t.space ? " _" : " ";
        }
        out += t.kind == chat::TAG ? "[" + t.text + "]" : t.text;
    }
    return out;
}

static chat::Message Msg(const char* text, uint32_t color = 0xFFFFFFFF) {
    chat::Message m;
    m.type = chat::TYPE_INFO;
    m.text = text;
    m.textColor = color;
    return m;
}

static void Parsing() {
    using chat::Tokenize;
    Check(Pieces(Msg("{FF6347}[ADMIN]{FFFFFF} Victor_Trok: oi")) == "[ADMIN] _Victor_Trok: _oi",
          "[ADMIN] vira tag e o resto continua texto");
    Check(Tokenize(Msg("{FF6347}[ADMIN]{FFFFFF} oi"))[0].color == IM_COL32(0xFF, 0x63, 0x47, 255),
          "a tag fica com a cor do {RRGGBB} de antes dela");
    const auto vip = Tokenize(Msg("[{33AAFF}VIP{FFFFFF}] oi"));
    Check(vip.size() == 2 && vip[0].color == IM_COL32(0x33, 0xAA, 0xFF, 255) &&
              vip[1].color == IM_COL32(255, 255, 255, 255),
          "cor dentro da tag: a tag pega a do texto dela e o {FFFFFF} continua valendo depois");
    Check(Pieces(Msg("[esta frase entre colchetes e grande demais] fim")) ==
              "[esta _frase _entre _colchetes _e _grande _demais] _fim",
          "colchete com mais de 24 caracteres continua texto");
    Check(Pieces(Msg("abre [ e nao fecha")) == "abre _[ _e _nao _fecha", "colchete sem fechar continua texto");
    Check(Pieces(Msg("[]  [ ] fim")) == "[] _[ _] _fim", "colchetes vazios continuam texto");
    Check(Pieces(Msg("x[a]y")) == "x [a] y", "tag colada no texto");
    Check(Pieces(Msg("a\tb\nc")) == "a _b _c", "tab e quebra de linha viram espaco");
    const auto split = Tokenize(Msg("{FF0000}verm{00FF00}elho"));
    Check(split.size() == 2 && !split[1].space && split[1].color == IM_COL32(0, 0xFF, 0, 255),
          "cor no meio da palavra: dois pedacos colados");
    Check(Pieces(Msg("{GGGGGG}texto")) == "{GGGGGG}texto", "{codigo} invalido aparece como texto");
    chat::Message p = Msg("bora pro evento?");
    p.type = chat::TYPE_CHAT;
    p.prefix = "Victor_Trok";
    p.prefixColor = 0xFF33CCFF;
    Check(Pieces(p) == "Victor_Trok: _bora _pro _evento?" && Tokenize(p)[0].color == IM_COL32(0x33, 0xCC, 0xFF, 255),
          "mensagem de jogador: nick com ':' na cor dele");
    Check(Pieces(Msg("{FF6347}[ADMIN]{FFFFFF} oi")).find("[ADMIN]") == 0 &&
              chat::Tokenize(Msg("{FF6347}[ADMIN]{FFFFFF} oi"), false)[0].kind == chat::WORD &&
              chat::Tokenize(Msg("{FF6347}[ADMIN]{FFFFFF} oi"), false)[0].text == "[ADMIN]",
          "com as etiquetas desligadas, [ADMIN] continua texto");
}

static void Fill() {
    Say(chat::TYPE_INFO, "SA-MP {B9C9BF}0.3.7-R1 {FFFFFF}Started", 0xFFA9C4E4);
    Say(chat::TYPE_INFO, "Connected to {FFFFFF}Brasil Trok Roleplay | [PC/Android] | Discord: trok.gg", 0xFFA9C4E4);
    Say(chat::TYPE_DEBUG,
        "{FFFFFF}Bem-vindo de volta, {33AAFF}Victor_Trok{FFFFFF}! Você tem {7CFC00}3{FFFFFF} mensagens não lidas.");
    Say(chat::TYPE_DEBUG, "{FF6347}[ADMIN]{FFFFFF} Kalcor: o servidor reinicia em {FFD27A}5 minutos{FFFFFF}, guardem os "
                          "veículos e saiam das casas alugadas.");
    Player("Victor_Trok", 0xFF33CCFF, "e aí, bora pro evento da praia?");
    Player("Ana_Souza", 0xFFFF99CC, "bora! levo o som [kkkk]");
    Say(chat::TYPE_DEBUG,
        "{FFFFFF}[{33AAFF}VIP{FFFFFF}] {33AAFF}Marcos_Lima{FFFFFF} comprou uma {FFD27A}Infernus{FFFFFF}.");
    Say(chat::TYPE_DEBUG, "{3C78FF}[Polícia]{FFFFFF} Procurado: {FF4040}Diego_Ramos{FFFFFF} (nível 4) — visto por "
                          "último na Grove Street.");
    Say(chat::TYPE_DEBUG, "{FFD200}[Evento]{FFFFFF} Corrida em {FFD200}Los Santos{FFFFFF} às 21h. Use {FFD200}/corrida "
                          "{FFFFFF}para entrar.");
    Say(chat::TYPE_DEBUG, "{FFFFFF}[Info] Use {9A9A9A}/ajuda{FFFFFF} para ver os comandos e {9A9A9A}/regras{FFFFFF} "
                          "antes de jogar.");
    Say(chat::TYPE_DEBUG, "Discord: https://discord.gg/brasil-trok-roleplay-servidor-oficial-samp037-comunidade",
        0xFFC8C8C8);
    Player("Diego_Ramos", 0xFFFF8040, "[ooc] alguém sabe onde fica a autoescola?");
    Say(chat::TYPE_DEBUG, "{9A9A9A}[Trok Chat]{FFFFFF} ligado. {9A9A9A}/trokchat abre as configurações.");
}

int main(int argc, char** argv) {
    g_dir = argc > 1 ? argv[1] : ".";
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)W, (float)H);
    io.IniFilename = nullptr;
    tui::BuildFonts(1080.0f, "C:\\GTA\\"); // escala de 1080p
    io.Fonts->GetTexDataAsRGBA32(&tex, &tw, &th);
    io.Fonts->SetTexID((ImTextureID)1);
    io.AddMousePosEvent(1500, 860);

    Parsing();
    Fill();
    Idle(2);
    Frame("01_fechado");

    // T: a barra abre (o SA-MP fica sabendo pelo "chat aberto").
    chat::Open();
    Idle(3);
    Check(chat::IsOpen() && g_inputOpened == 1 && g_inputFlag, "T abre a barra e liga o 'chat aberto' do SA-MP");
    Frame("02a_aberto_vazio");
    const char* hello = "Olá pessoal, alguém vai no [evento] hoje? Às 21h!";
    Type(hello);
    Idle(20);
    Frame("02_aberto_digitando");
    Key(ImGuiKey_Enter);
    Idle(2);
    Check(g_sent.size() == 1 && g_sent[0] == hello, "Enter manda o texto (UTF-8) para a caixa do SA-MP");
    Check(!chat::IsOpen() && !g_inputFlag && g_inputClosed == 1, "e fecha a barra");

    // Quem digita logo depois do T (antes de a barra pegar o teclado) nao perde letras.
    chat::Open();
    Type("r");
    Type("apido");
    Idle(3);
    Check(std::string(chat::S.input) == "rapido", "letras digitadas logo depois do T nao se perdem");

    // Esc fecha sem mandar.
    Key(ImGuiKey_Escape);
    chat::Open();
    Idle(2);
    Type("/me acena");
    Key(ImGuiKey_Escape);
    Idle(2);
    Check(!chat::IsOpen() && g_sent.size() == 1, "Esc fecha sem mandar");

    // Enter com a barra vazia (ou so espacos) so fecha.
    chat::Open();
    Idle(2);
    Type("   ");
    Key(ImGuiKey_Enter);
    Idle(2);
    Check(!chat::IsOpen() && g_sent.size() == 1, "Enter com a barra vazia so fecha");

    // Historico: seta para cima traz a ultima mensagem mandada.
    chat::Open();
    Idle(2);
    Type("/ajuda");
    Key(ImGuiKey_Enter);
    Idle(2);
    chat::Open();
    Idle(2);
    Type("rascunho");
    Key(ImGuiKey_UpArrow);
    Idle(2);
    Check(std::string(chat::S.input) == "/ajuda", "seta para cima: a ultima mensagem mandada");
    Key(ImGuiKey_UpArrow);
    Idle(2);
    Check(std::string(chat::S.input) == hello, "de novo: a anterior");
    Key(ImGuiKey_DownArrow);
    Key(ImGuiKey_DownArrow);
    Idle(2);
    Check(std::string(chat::S.input) == "rascunho", "para baixo ate o fim: o rascunho volta");
    Frame("03_historico");

    // Texto pronto de um script (sampSetChatInputText): entra na barra aberta, com o cursor no fim.
    Key(ImGuiKey_Escape);
    chat::Open();
    chat::SetInput("/r ");
    Idle(3);
    Type("na escuta");
    Idle(2);
    Check(std::string(chat::S.input) == "/r na escuta", "texto de script na barra, e o que se digita vem depois");
    Key(ImGuiKey_Escape);
    chat::Open();
    Idle(3);
    chat::SetInput("/me ");
    Idle(2);
    Type("acena");
    Idle(2);
    Check(std::string(chat::S.input) == "/me acena", "tambem com a barra ja ativa");
    Key(ImGuiKey_Escape);
    chat::Open();
    Idle(2);
    Type("rascunho");
    Idle(2);

    // A barra perde o teclado (um clique fora, por exemplo): volta sozinha, sem selecionar o texto.
    ImGui::ClearActiveID();
    Idle(3);
    Type("!");
    Check(std::string(chat::S.input) == "rascunho!", "foco de volta com o cursor no fim (o texto nao some)");
    Key(ImGuiKey_Escape);

    // Limite de 128 caracteres, como a caixa do SA-MP.
    chat::Open();
    Idle(2);
    std::string longText;
    for (int i = 0; i < 14; i++) {
        longText += "ação 1234 ";
    }
    Type(longText.c_str());
    Idle(2);
    Check(chat::Codepoints(chat::S.input) == 128, "no maximo 128 caracteres (acentos contam um)");
    Frame("04_limite");
    Key(ImGuiKey_Escape);

    // Muitas mensagens, rolagem com o Page Up e com a roda; mensagens novas com a lista rolada.
    for (int i = 1; i <= 40; i++) {
        char text[96];
        snprintf(text, sizeof(text), "{9A9A9A}[Log]{FFFFFF} Mensagem antiga número %d", i);
        Say(chat::TYPE_DEBUG, text);
    }
    Fill();
    chat::Open();
    Idle(2);
    Check(GImGui->InputTextState.ScrollX == 0.0f, "barra nova sem a rolagem do texto longo de antes (a dica aparece)");
    Key(ImGuiKey_PageUp);
    Idle(2);
    const int afterPage = chat::S.scroll;
    Check(afterPage == g_cfg.pageSize - 1, "Page Up sobe uma pagina (menos uma linha)");
    io.AddMouseWheelEvent(0, 1);
    Idle(2);
    Check(chat::S.scroll == afterPage + 3, "roda do Windows: 3 linhas por volta (enquanto a do jogo nao responde)");
    g_wheel = 1;
    Idle(2);
    Check(chat::S.scroll == afterPage + 6, "roda do jogo: 3 linhas por volta");
    io.AddMouseWheelEvent(0, 1);
    Idle(2);
    Check(chat::S.scroll == afterPage + 6, "depois que a roda do jogo responde, a do Windows fica de fora");
    const int before = chat::S.scroll;
    Player("Ana_Souza", 0xFFFF99CC, "chegou mais gente aqui na praia");
    Say(chat::TYPE_DEBUG, "{FFD200}[Evento]{FFFFFF} Faltam {FFD200}2 minutos{FFFFFF} para a corrida!");
    Idle(2);
    Check(chat::S.scroll == before + 2, "mensagens novas nao mexem no que esta sendo lido");
    Check(chat::g_added - chat::S.seen == 2, "e contam como novas no aviso");
    Frame("05_rolado_mensagens_novas");
    for (int i = 0; i < 40; i++) {
        Key(ImGuiKey_PageDown);
    }
    Check(chat::S.scroll == 0, "Page Down volta para as mais recentes");
    for (int i = 0; i < 80; i++) {
        Key(ImGuiKey_PageUp);
    }
    int total = 0;
    for (const auto& e : chat::g_entries) {
        total += e.lines;
    }
    Check(chat::S.scroll == total - g_cfg.pageSize, "Page Up ate o topo para na primeira mensagem");
    Frame("06_topo");
    Key(ImGuiKey_Escape);

    // Dialogo do servidor abre: a barra fecha.
    chat::Open();
    Idle(2);
    g_dialog = true;
    Idle(2);
    Check(!chat::IsOpen() && !g_inputFlag, "dialogo do servidor na tela fecha a barra");
    g_dialog = false;

    // 300 mensagens no maximo.
    for (int i = 0; i < 400; i++) {
        Say(chat::TYPE_DEBUG, "spam");
    }
    Idle(1);
    Check(chat::g_entries.size() == chat::MAX_MESSAGES, "guarda as ultimas 300 mensagens");
    Fill();

    // Hora (/timestamp), sem sombra (F7), desligado (F7 de novo) e /pagesize 15.
    g_cfg.timestamps = true;
    Idle(2);
    Frame("07_horario");
    g_cfg.timestamps = false;
    g_cfg.mode = 1;
    Idle(2);
    Frame("08_sem_sombra");
    g_cfg.mode = 0;
    Idle(2);
    chat::Open();
    Idle(2);
    Frame("09_desligado_aberto");
    Key(ImGuiKey_Escape);
    g_cfg.mode = 2;
    g_cfg.pageSize = 15;
    chat::Open();
    Idle(2);
    g_saveH = 560;
    Frame("10_pagesize_15");
    Key(ImGuiKey_Escape);
    Idle(2);
    g_cfg.pageSize = 10;
    g_saveH = 470;

    // Escondido com o chat fechado: aberto, as mensagens aparecem.
    g_cfg.mode = 0;
    chat::Open();
    Idle(2);
    Frame("09b_escondido_aberto");
    Key(ImGuiKey_Escape);
    g_cfg.mode = 2;
    Idle(2);

    // ---------------------------------------------------------------- menu do /trokchat
    g_saveW = W;
    g_saveH = H;
    menu::Toggle();
    Idle(30);
    Check(menu::IsOpen(), "/trokchat abre o menu");
    Frame("11_menu");
    // Linha 2 (tecla alternativa): Enter e a proxima tecla apertada fica no lugar.
    Key(ImGuiKey_DownArrow);
    Key(ImGuiKey_Enter);
    Idle(10);
    Check(tui::CapturingKey(), "Enter na linha da tecla: esperando a tecla");
    Frame("12_menu_esperando_tecla");
    Key(ImGuiKey_Enter);
    Idle(20);
    Check(!tui::CapturingKey() && opt::Get().keys[1] == ImGuiKey_Enter && opt::Get().keys[0] == ImGuiKey_T,
          "F6 trocado por Enter (o T continua)");
    Frame("13_menu_f6_por_enter");
    // Linhas na tela (linha 3): seta para a direita.
    Key(ImGuiKey_DownArrow);
    Key(ImGuiKey_RightArrow);
    Idle(5);
    Check(g_cfg.pageSize == 11, "linhas na tela: 10 -> 11 (o /pagesize do SA-MP muda junto)");
    // Hora (linha 4) e o chat fechado (linha 5: escondido).
    Key(ImGuiKey_DownArrow);
    Key(ImGuiKey_Enter);
    Idle(5);
    Check(g_cfg.timestamps, "hora ligada (o /timestamp do SA-MP)");
    Key(ImGuiKey_DownArrow);
    Key(ImGuiKey_RightArrow);
    Key(ImGuiKey_RightArrow);
    Idle(5);
    Check(g_cfg.mode == 0, "com o chat fechado: escondido (o F7 do SA-MP)");
    Key(ImGuiKey_LeftArrow);
    Key(ImGuiKey_LeftArrow);
    Idle(5);
    Check(g_cfg.mode == 2, "e de volta para com contorno");
    // Etiquetas (linha 6) desligadas: as mensagens sao quebradas de novo.
    Key(ImGuiKey_DownArrow);
    Key(ImGuiKey_Enter);
    Idle(30);
    Check(!opt::Get().tags, "etiquetas desligadas");
    Frame("14_menu_opcoes");
    Key(ImGuiKey_Escape);
    Idle(20);
    Check(!menu::IsOpen(), "Esc fecha o menu");
    g_saveW = 1000;
    g_saveH = 470;
    Frame("15_sem_etiquetas_com_hora");
    // Padroes (linha 8): confirma e volta T e F6.
    menu::Toggle();
    Idle(10);
    for (int i = 0; i < 7; i++) {
        Key(ImGuiKey_DownArrow);
    }
    Key(ImGuiKey_Enter);
    Idle(10);
    g_saveW = W;
    g_saveH = H;
    Frame("16_menu_restaurar");
    Key(ImGuiKey_Enter);
    Idle(10);
    Check(opt::Get().keys[0] == ImGuiKey_T && opt::Get().keys[1] == ImGuiKey_F6 && opt::Get().tags &&
              g_cfg.pageSize == 10 && !g_cfg.timestamps && g_cfg.mode == 2,
          "padroes: T e F6, etiquetas, 10 linhas, sem hora, com contorno");
    Key(ImGuiKey_Escape);
    Idle(60);

    ImGui::DestroyContext();
    printf("render: %d falhas\n", g_fail);
    return g_fail ? 1 : 0;
}

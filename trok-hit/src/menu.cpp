// Trok Hitmarker (.asi) -- Victor_Trok
// O menu do /trokhit no kit da casa: Mira, Dano, Informer, Recebido, Sons e Geral.

#include "menu.h"

#include "hud.h"
#include "options.h"
#include "trok_ui.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace menu {
namespace {

using namespace tui;

constexpr const char* VERSION_TAG = "v1.0";

const char* const TABS[] = {"Mira", "Dano", "Informer", "Recebido", "Sons", "Geral"};
constexpr int TAB_COUNT = 6;

struct State {
    bool open = false;
    int tab = 0;
    int selected[TAB_COUNT] = {1, 1, 1, 1, 1, 1};
    int rowCount[TAB_COUNT] = {1, 1, 1, 1, 1, 1};
    Shell shell;
    int age = 0;
    int preset = 0;
    int request = REQ_NONE;
    // Mover: as duas caixas do informer andam com o mouse; Enter grava, Esc ou o botao direito desiste.
    bool moving = false;
    int drag = -1; // 0 dado, 1 levado
    opt::Place place[2];
} S;

Keys ReadKeys(bool allowed) {
    Keys k;
    if (!allowed) {
        return k;
    }
    k.up = ImGui::IsKeyPressed(ImGuiKey_UpArrow, true);
    k.down = ImGui::IsKeyPressed(ImGuiKey_DownArrow, true);
    k.left = ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true);
    k.right = ImGui::IsKeyPressed(ImGuiKey_RightArrow, true);
    k.enter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
    k.space = ImGui::IsKeyPressed(ImGuiKey_Space, false);
    return k;
}

void MoveSelection(int* selected, int count, const Keys& k, Rows& rows) {
    if (count <= 0) {
        return;
    }
    if (k.up) {
        *selected = (*selected + count - 2) % count + 1;
        rows.keyboardMoved = true;
    } else if (k.down) {
        *selected = *selected % count + 1;
        rows.keyboardMoved = true;
    }
}

// Previa no alto da aba: a mira e os quatro sinais de acerto parados no meio da animacao, com o nome de cada um.
void MarkerPreview(float x, float w) {
    opt::Options& o = opt::Get();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 at = ImGui::GetCursorScreenPos();
    float h = 92 * u;
    dl->AddRectFilled(ImVec2(x, at.y + 2 * u), ImVec2(x + w, at.y + h), col::White(6), 8 * u);
    const char* names[] = {"Mira", "Acerto", "Colete", "Cabe\xC3\xA7" "a", "Abate"};
    hud::Kind kinds[] = {hud::HEALTH, hud::HEALTH, hud::ARMOUR, hud::HEADSHOT, hud::KILL};
    float scale = ImGui::GetIO().DisplaySize.y / 1080.0f * 1.3f;
    for (int i = 0; i < 5; ++i) {
        float cx = x + w * (i + 0.5f) / 5;
        ImVec2 c(cx, at.y + 40 * u);
        hud::DrawAimPreview(dl, c, scale, i == 0 ? 99.0f : 0.12f, kinds[i], o); // 99 s: so a mira, parada
        ImVec2 ts = TextSize(fonts.desc, names[i]);
        Text(dl, fonts.desc, cx - ts.x * 0.5f, at.y + 68 * u, col::hint, names[i]);
    }
    ImGui::Dummy(ImVec2(w, h + 4 * u));
}

// Configuracao pronta: so liga e desliga as partes (cores, sons, imagens, tamanhos e posicoes ficam).
void ApplyPreset(int preset) {
    opt::Options& o = opt::Get();
    o.preset = preset;
    o.enabled = true;
    o.informer = 0;
    o.combo = true;
    if (preset == 0) { // Trok
        o.onAim = o.fxHit = o.fxArmour = o.fxHead = o.fxKill = true;
        o.numbers = 0;
        o.numbersSum = true;
        o.hitLocal = true;
        o.direction = o.edgeFlash = true;
    } else { // Damage Informer
        o.numbers = 2;
        o.hitLocal = false;
    }
}

void BeginMove();

void TabMira(Rows& r, float x, float w) {
    opt::Options& o = opt::Get();
    const opt::Options& d = opt::Defaults();
    MarkerPreview(x, w); // em cima: muda na hora com as opcoes de baixo
    static const char* const mira[] = {"A do jogo", "Do Trok"};
    r.Segmented("Mira", &o.crosshair, mira, 2, d.crosshair);
    r.Slider("Tamanho (mira e efeitos)", &o.crosshairSize, 50, 200, 10, "%d%%", d.crosshairSize);
    if (o.crosshair == 0) {
        r.Toggle("Mira est\xC3\xA1tica (n\xC3\xA3o abre ao atirar)", &o.crosshairStatic);
    } else {
        r.Toggle("Sempre vis\xC3\xADvel (tamb\xC3\xA9m sem mirar)", &o.crosshairAlways);
    }
    r.Section("Sinal de acerto na mira");
    r.Toggle("Sinal de acerto", &o.onAim);
    r.Toggle("Acerto", &o.fxHit);
    r.Toggle("Colete com imagem pr\xC3\xB3pria", &o.fxArmour);
    r.Toggle("Cabe\xC3\xA7" "a com imagem pr\xC3\xB3pria", &o.fxHead);
    r.Toggle("Abate com imagem pr\xC3\xB3pria", &o.fxKill);
    r.Info("Desligar um deles",
           "Colete, cabe\xC3\xA7" "a e abate desligados mostram a imagem do acerto normal naquele tiro. Desligar o "
           "Acerto tira o sinal dos tiros normais (os ligados continuam).");
    r.Section("Imagens");
    if (r.Action("Pasta das imagens", "Abrir pasta")) {
        S.request = REQ_OPEN_IMAGES;
    }
    r.Info("Trocar o visual",
           "Cada coisa \xC3\xA9 um PNG na pasta Trok Hitmarker\\mira: mira, acerto, colete, cabeca, abate e corpo (o X "
           "do local do acerto). Edite ou troque o arquivo; apagou, volta a imagem padr\xC3\xA3o. Qualquer resolu\xC3\xA7\xC3\xA3o "
           "serve: a imagem inteira vira o tamanho da mira. Vale na hora.");
}

void TabDano(Rows& r) {
    opt::Options& o = opt::Get();
    const opt::Options& d = opt::Defaults();
    static const char* const where[] = {"No personagem", "Na tela", "Desligado"};
    r.Segmented("N\xC3\xBAmeros de dano", &o.numbers, where, 3, d.numbers);
    r.Toggle("Somar tiros seguidos no mesmo alvo", &o.numbersSum);
    static const char* const source[] = {"Real", "Do GTA"};
    r.Segmented("Dano mostrado", &o.source, source, 2, d.source);
    r.Info("Como o dano real \xC3\xA9 medido",
           "Real: a vida e o colete que o alvo perdeu de verdade, do jeito que o servidor aplicou (arma configurada, "
           "headshot que mata de uma vez, colete). Do GTA: o valor que o jogo calcula para a arma (46.2 na Deagle), "
           "o mesmo que o damage informer antigo mostrava.");
    r.Slider("Tamanho", &o.numbersSize, 50, 200, 10, "%d%%", d.numbersSize);
    static const char* const font[] = {"Arial Black", "Da casa"};
    r.Segmented("Fonte dos n\xC3\xBAmeros", &o.numbersFont, font, 2, d.numbersFont);
    r.Section("Cores dos n\xC3\xBAmeros");
    r.Color("Na vida", &o.cHealth, d.cHealth);
    r.Color("No colete", &o.cArmour, d.cArmour);
    r.Color("Na cabe\xC3\xA7" "a", &o.cHead, d.cHead);
    r.Color("Abate", &o.cKill, d.cKill);
    r.Section("Local do acerto (no corpo)");
    r.Toggle("Marcar onde o tiro pegou (corpo.png)", &o.hitLocal);
    r.Slider("Fica no corpo", &o.hitLocalTime, 1, 5, 1, "%d s", d.hitLocalTime);
    r.Slider("Tamanho no corpo", &o.hitLocalSize, 50, 150, 10, "%d%%", d.hitLocalSize);
}

void TabInformer(Rows& r) {
    opt::Options& o = opt::Get();
    const opt::Options& d = opt::Defaults();
    static const char* const layout[] = {"Cl\xC3\xA1ssico", "Lista", "Desligado"};
    r.Segmented("Informer", &o.informer, layout, 3, d.informer);
    static const char* const nick[] = {"Do jogador", "Fixa"};
    r.Segmented("Cor do nick ao dar dano", &o.nickGiven, nick, 2, d.nickGiven);
    r.Color("Cor fixa ao dar dano", &o.cGiven, d.cGiven);
    r.Segmented("Cor do nick ao levar dano", &o.nickTaken, nick, 2, d.nickTaken);
    r.Color("Cor fixa ao levar dano", &o.cTaken, d.cTaken);
    r.Toggle("AFK no lugar do dano", &o.afk);
    r.Toggle("Combo (acertos e dano somado)", &o.combo);
    r.Slider("Combo fecha depois de", &o.comboWindow, 1, 10, 1, "%d s", d.comboWindow);
    r.Slider("Tamanho", &o.informerSize, 50, 200, 10, "%d%%", d.informerSize);
    if (r.Action("Posi\xC3\xA7\xC3\xA3o na tela", "Mover")) {
        BeginMove();
    }
}

void TabRecebido(Rows& r) {
    opt::Options& o = opt::Get();
    const opt::Options& d = opt::Defaults();
    r.Toggle("Seta de onde veio o tiro", &o.direction);
    r.Toggle("Borda de dano recebido", &o.edgeFlash);
    r.Section("Cor da borda");
    r.Color("Dano na vida", &o.cEdgeHealth, d.cEdgeHealth);
    r.Color("Dano no colete", &o.cEdgeArmour, d.cEdgeArmour);
    r.Color("Dano na cabe\xC3\xA7" "a", &o.cEdgeHead, d.cEdgeHead);
}

void TabSons(Rows& r) {
    opt::Options& o = opt::Get();
    const opt::Options& d = opt::Defaults();
    r.Section("Ao dar dano");
    r.Toggle("Acerto", &o.sHit);
    r.Toggle("Som diferente na cabe\xC3\xA7" "a", &o.sHeadDiff);
    r.Section("Ao levar dano");
    r.Toggle("Dano levado", &o.sTaken);
    r.Section("Abates");
    r.Toggle("Abate", &o.sKill);
    r.Toggle("Abate na cabe\xC3\xA7" "a", &o.sKillHead);
    r.Toggle("Morte", &o.sDeath);
    r.Slider("Volume", &o.volume, 0, 100, 5, "%d%%", d.volume);
    if (r.Action("Ouvir os sons", "Tocar")) {
        S.request = REQ_PLAY_SOUNDS;
    }
    if (r.Action("Pasta dos sons", "Abrir pasta")) {
        S.request = REQ_OPEN_SOUNDS;
    }
    r.Info("Trocar um som",
           "Ponha um .mp3 ou .wav com o mesmo nome na pasta Trok Hitmarker\\sons: acerto, cabeca, dano_levado, abate, "
           "abate_cabeca e morte. Vale na hora, sem reiniciar o jogo.");
}

void TabGeral(Rows& r) {
    opt::Options& o = opt::Get();
    r.Toggle("Trok Hitmarker ligado", &o.enabled);
    static const char* const presets[] = {"Trok", "Damage Informer"};
    r.Cycle("Configura\xC3\xA7\xC3\xA3o pronta", &S.preset, presets, 2, 0);
    if (r.Action("Aplicar a configura\xC3\xA7\xC3\xA3o", "Aplicar")) {
        ApplyPreset(S.preset);
        Toast(presets[S.preset], "Configura\xC3\xA7\xC3\xA3o aplicada. Cores, sons e imagens ficaram como estavam.");
    }
    r.Info("O que cada uma liga",
           "Trok: sinal na mira, local do acerto, n\xC3\xBAmeros saindo do alvo, informer cl\xC3\xA1ssico, seta e borda "
           "de dano recebido. Damage Informer: s\xC3\xB3 o informer cl\xC3\xA1ssico (nick e n\xC3\xBAmero subindo, com o "
           "combo), sem n\xC3\xBAmeros no alvo nem local do acerto. Cores, sons e as imagens da mira n\xC3\xA3o mudam.");
    if (r.Action("Posi\xC3\xA7\xC3\xB5" "es na tela", "Mover")) {
        BeginMove();
    }
    r.Value("Escolhas salvas", "por servidor");
    if (r.Action("Padr\xC3\xB5" "es do Trok Hitmarker", "Restaurar")) {
        o = opt::Defaults();
        S.preset = 0;
        Toast("Padr\xC3\xB5" "es restaurados", "Todas as op\xC3\xA7\xC3\xB5" "es voltaram ao padr\xC3\xA3o do Trok Hitmarker.");
    }
}

// Um exemplo do informer em cada caixa (o layout escolhido), para ver onde ele fica.
void SampleState(hud::State* st) {
    hud::Line g;
    g.nick = "Victor_Trok";
    g.id = 12;
    g.nickColor = IM_COL32(255, 186, 74, 255);
    g.hits = 3;
    g.total = 138;
    hud::Hit h;
    h.value = 46;
    for (float age : {1.1f, 0.6f, 0.15f}) {
        h.age = age;
        g.recent.push_back(h);
    }
    hud::Line t;
    t.nick = "Trok_Bot";
    t.id = 3;
    t.nickColor = IM_COL32(120, 200, 255, 255);
    t.hits = 1;
    t.total = 33;
    h.age = 0.4f;
    h.value = 33;
    t.recent.push_back(h);
    st->given = {g};
    st->taken = {t};
}

// As caixas, em pixels: o classico vai do numero subindo ate o nick; a lista, tres linhas.
void MoveBox(int which, const opt::Options& o, ImVec2 screen, ImVec2* a, ImVec2* b) {
    float k = screen.y / 1080.0f, s = k * o.informerSize / 100.0f;
    const opt::Place& p = S.place[which];
    ImVec2 at(p.x * screen.x, p.y * screen.y);
    float w = o.informer == 1 ? 250 * s : 330 * s;
    float top = o.informer == 1 ? at.y : at.y - 155 * k - 8 * s;
    float bottom = o.informer == 1 ? at.y + 3 * 37 * s : at.y + 22 * k + 24 * s;
    float pad = 8 * s;
    if (which == 0) {
        *a = ImVec2(at.x - pad, top - pad);
        *b = ImVec2(at.x + w + pad, bottom + pad);
    } else {
        *a = ImVec2(at.x - w - pad, top - pad);
        *b = ImVec2(at.x + pad, bottom + pad);
    }
}

void Dashed(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 c, float t) {
    float step = 14 * u, dash = 7 * u;
    for (float x = a.x; x < b.x; x += step) {
        dl->AddLine(ImVec2(x, a.y), ImVec2(std::min(x + dash, b.x), a.y), c, t);
        dl->AddLine(ImVec2(x, b.y), ImVec2(std::min(x + dash, b.x), b.y), c, t);
    }
    for (float y = a.y; y < b.y; y += step) {
        dl->AddLine(ImVec2(a.x, y), ImVec2(a.x, std::min(y + dash, b.y)), c, t);
        dl->AddLine(ImVec2(b.x, y), ImVec2(b.x, std::min(y + dash, b.y)), c, t);
    }
}

void MoveFrame() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 screen = io.DisplaySize;
    opt::Options view = opt::Get();
    view.given = S.place[0];
    view.taken = S.place[1];
    if (view.informer == 2) {
        view.informer = 0; // desligado: mostra o classico so para posicionar
    }
    view.enabled = true;
    view.edgeFlash = view.direction = view.onAim = view.hitLocal = false;
    view.numbers = 2;
    view.crosshair = 0;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(screen);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##trokHitMover", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(0, 0), screen, IM_COL32(0, 0, 0, 70));
    hud::State st;
    SampleState(&st);
    hud::Draw(dl, st, view, screen);

    static const char* const names[] = {"Informer: dado", "Informer: levado"};
    for (int i = 0; i < 2; ++i) {
        ImVec2 a, b;
        MoveBox(i, view, screen, &a, &b);
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(i);
        ImGui::InvisibleButton("##caixa", ImVec2(std::max(1.0f, b.x - a.x), std::max(1.0f, b.y - a.y)));
        bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0.0f)) {
            S.drag = i;
            opt::Place& p = S.place[i];
            p.x = std::max(0.02f, std::min(0.98f, p.x + io.MouseDelta.x / screen.x));
            p.y = std::max(0.05f, std::min(0.95f, p.y + io.MouseDelta.y / screen.y));
        }
        ImGui::PopID();
        if (hovered || S.drag == i) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }
        Dashed(dl, a, b, col::White(S.drag == i ? 230 : hovered ? 200 : 150), 1.5f * u);
        Text(dl, fonts.desc, a.x + 4 * u, a.y - fonts.desc->FontSize - 6 * u, col::White(220), names[i]);
    }
    if (!ImGui::IsMouseDown(0)) {
        S.drag = -1;
    }

    const char* help = "Arraste cada caixa  |  Enter salva  |  Esc ou bot\xC3\xA3o direito cancela";
    ImVec2 hs = TextSize(fonts.body, help);
    float px = (screen.x - hs.x) * 0.5f - 18 * u, py = screen.y - 28 * u - hs.y - 24 * u;
    dl->AddRectFilled(ImVec2(px, py), ImVec2(px + hs.x + 36 * u, py + hs.y + 24 * u), IM_COL32(12, 12, 12, 252), 10 * u);
    dl->AddRect(ImVec2(px, py), ImVec2(px + hs.x + 36 * u, py + hs.y + 24 * u), col::White(14), 10 * u, 0, 1.0f);
    Text(dl, fonts.body, px + 18 * u, py + 12 * u, col::text, help);
    ImGui::End();
    ImGui::PopStyleVar(2);

    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
        opt::Get().given = S.place[0];
        opt::Get().taken = S.place[1];
        S.moving = false;
        S.age = 0;
        Toast("Posi\xC3\xA7\xC3\xB5" "es salvas", "O informer ficou onde voc\xC3\xAA deixou.");
    } else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsMouseClicked(1)) {
        S.moving = false; // desistiu: fica onde estava
        S.age = 0;
    }
}

void BeginMove() {
    S.moving = true;
    S.drag = -1;
    S.place[0] = opt::Get().given;
    S.place[1] = opt::Get().taken;
}

} // namespace

void StartMove() {
    BeginMove();
}

int TakeRequest() {
    int r = S.request;
    S.request = REQ_NONE;
    return r;
}

const char* Version() {
    return VERSION_TAG;
}

bool WantsFrame() {
    return S.open || S.moving || ToastsVisible();
}

void Toggle() {
    if (S.moving) {
        return;
    }
    S.open = !S.open;
    S.age = 0;
    S.preset = opt::Get().preset;
}

void SetTab(int tab) {
    S.tab = tab;
}

bool CapturesInput() {
    return S.open || S.moving;
}

bool Frame() {
    opt::Options before;
    memcpy(&before, &opt::Get(), sizeof(before)); // byte a byte: o memcmp do fim nao ve lixo no preenchimento
    if (S.moving) {
        MoveFrame();
        DrawToasts();
        return memcmp(&before, &opt::Get(), sizeof(before)) != 0;
    }
    if (!S.open) {
        DrawToasts();
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    bool fresh = S.age++ == 0;
    bool keysOk = !fresh && !io.WantTextInput && !CapturingKey();
    Keys keys = ReadKeys(keysOk);
    bool escape = keysOk && ImGui::IsKeyPressed(ImGuiKey_Escape, false);

    float width = 700 * u;
    float height = std::min(io.DisplaySize.y * 0.8f, 690 * u);
    PushStyle();
    bool keep = BeginShell("##trokHit", "Trok Hitmarker", VERSION_TAG, width, height, S.shell);
    ImVec2 pos = ImGui::GetWindowPos();
    float padX = 18 * u;
    Tabs("##abas", TABS, TAB_COUNT, &S.tab, pos.x + padX, pos.y + HeaderHeight(), width - 2 * padX);

    float listTop = pos.y + HeaderHeight() + TabsHeight() + 10 * u;
    float listH = height - (listTop - pos.y) - FooterHeight() - 8 * u;
    ImGui::SetCursorScreenPos(ImVec2(pos.x + padX - 4 * u, listTop));
    ImGui::BeginChild("##lista", ImVec2(width - 2 * padX + 8 * u, listH), false, ImGuiWindowFlags_NoBackground);
    ImVec2 cp = ImGui::GetCursorScreenPos();
    float rowW = ImGui::GetContentRegionAvail().x - 10 * u;

    Rows rows;
    int* sel = &S.selected[S.tab];
    MoveSelection(sel, S.rowCount[S.tab], keys, rows);
    rows.Begin(TABS[S.tab], cp.x + 4 * u, rowW, sel, keys, true);
    switch (S.tab) {
    case 0: TabMira(rows, cp.x + 4 * u, rowW); break;
    case 1: TabDano(rows); break;
    case 2: TabInformer(rows); break;
    case 3: TabRecebido(rows); break;
    case 4: TabSons(rows); break;
    default: TabGeral(rows); break;
    }
    S.rowCount[S.tab] = rows.End();
    ImGui::Dummy(ImVec2(0, 6 * u));
    ImGui::EndChild();

    static const std::vector<Hint> hints = {{"Setas", "Ajustar", false}, {"Esc", "Fechar", true}};
    int clicked = EndShell(hints);
    PopStyle();
    if ((!keep || clicked == 1 || escape) && !S.moving) {
        S.open = false;
    }
    DrawToasts();
    return memcmp(&before, &opt::Get(), sizeof(before)) != 0;
}

} // namespace menu

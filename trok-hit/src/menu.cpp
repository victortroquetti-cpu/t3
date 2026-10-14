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

// Previa embaixo da aba: os quatro hitmarkers parados no meio da animacao, com o nome de cada um.
// Previa embaixo da aba: a mira e os quatro sinais de acerto parados no meio da animacao, com o nome de cada um.
void MarkerPreview(float x, float w) {
    opt::Options& o = opt::Get();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 at = ImGui::GetCursorScreenPos();
    float h = 92 * u;
    dl->AddRectFilled(ImVec2(x, at.y + 6 * u), ImVec2(x + w, at.y + h), col::White(6), 8 * u);
    const char* names[] = {"Mira", "Vida", "Colete", "Cabe\xC3\xA7" "a", "Abate"};
    hud::Kind kinds[] = {hud::HEALTH, hud::HEALTH, hud::ARMOUR, hud::HEADSHOT, hud::KILL};
    float scale = ImGui::GetIO().DisplaySize.y / 1080.0f * 1.3f;
    for (int i = 0; i < 5; ++i) {
        float cx = x + w * (i + 0.5f) / 5;
        ImVec2 c(cx, at.y + 40 * u);
        if (i == 0) {
            if (o.crosshair >= 1) { // ponto do Trok ou a imagem (sem a imagem, o ponto)
                hud::DrawCrosshair(dl, c, scale, o);
            } else { // a do jogo: um circulo, so para lembrar
                dl->AddCircle(c, 11 * scale, col::White(150), 32, 1.4f * scale);
            }
        } else {
            hud::DrawMarker(dl, c, o.marker, scale * o.markerSize / 100.0f, 0.12f, kinds[i], o);
        }
        ImVec2 ts = TextSize(fonts.desc, names[i]);
        Text(dl, fonts.desc, cx - ts.x * 0.5f, at.y + 68 * u, col::hint, names[i]);
    }
    ImGui::Dummy(ImVec2(w, h + 4 * u));
}

void TabMira(Rows& r, float x, float w) {
    opt::Options& o = opt::Get();
    const opt::Options& d = opt::Defaults();
    static const char* const mira[] = {"A do jogo", "Ponto do Trok", "Imagem"};
    r.Segmented("Mira", &o.crosshair, mira, 3, d.crosshair);
    if (o.crosshair >= 1) {
        r.Toggle("Sempre vis\xC3\xADvel (tamb\xC3\xA9m sem mirar)", &o.crosshairAlways);
        r.Slider("Tamanho da mira", &o.crosshairSize, 50, 200, 10, "%d%%", d.crosshairSize);
        if (o.crosshair == 1) {
            r.Color("Cor da mira", &o.cCrosshair, d.cCrosshair);
        }
    }
    r.Section("Sinal de acerto");
    r.Toggle("Sinal de acerto na mira", &o.onAim);
    static const char* const style[] = {"Ponto", "Cl\xC3\xA1ssico (X)", "Discreto", "Anel", "Imagens"};
    r.Cycle("Efeito", &o.marker, style, 5, d.marker);
    r.Slider("Tamanho do efeito", &o.markerSize, 50, 200, 10, "%d%%", d.markerSize);
    if (o.crosshair == 2 || o.marker == 4) {
        r.Action("Pasta das imagens", "Abrir pasta");
        r.Info("Trocar as imagens",
               "PNG com fundo transparente na pasta Trok Hitmarker\\mira: mira, acerto, colete, cabeca e abate. "
               "Sem colete.png, vale o acerto.png. O tamanho \xC3\xA9 o da imagem numa tela de 1080 (32 px = 32 px), "
               "vezes o Tamanho do menu. Trocou o arquivo, vale na hora.");
    }
    r.Section("Cores do acerto");
    r.Color("Na vida", &o.cHealth, d.cHealth);
    r.Color("No colete", &o.cArmour, d.cArmour);
    r.Color("Na cabe\xC3\xA7" "a", &o.cHead, d.cHead);
    r.Color("Abate", &o.cKill, d.cKill);
    MarkerPreview(x, w);
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
    r.Section("Local do acerto (no corpo)");
    r.Toggle("Marcar onde o tiro pegou", &o.hitLocal);
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
    r.Action("Ouvir os sons", "Tocar");
    r.Action("Pasta dos sons", "Abrir pasta");
    r.Info("Trocar um som",
           "Ponha um .mp3 ou .wav com o mesmo nome na pasta Trok Hitmarker\\sons: acerto, cabeca, dano_levado, abate, "
           "abate_cabeca e morte. Vale na hora, sem reiniciar o jogo.");
}

void TabGeral(Rows& r) {
    opt::Options& o = opt::Get();
    r.Toggle("Trok Hitmarker ligado", &o.enabled);
    static const char* const presets[] = {"Trok", "Damage Informer", "CoD", "Valorant", "Apex"};
    r.Cycle("Estilo pronto", &S.preset, presets, 5, 0);
    r.Action("Aplicar o estilo", "Aplicar");
    r.Action("Posi\xC3\xA7\xC3\xB5" "es na tela", "Mover");
    r.Value("Escolhas salvas", "por servidor");
    r.Action("Padr\xC3\xB5" "es do Trok Hitmarker", "Restaurar");
}

} // namespace

void Toggle() {
    S.open = !S.open;
    S.age = 0;
}

void SetTab(int tab) {
    S.tab = tab;
}

bool CapturesInput() {
    return S.open;
}

bool Frame() {
    if (!S.open) {
        return false;
    }
    opt::Options before = opt::Get();
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
    if (!keep || clicked == 1 || escape) {
        S.open = false;
    }
    return memcmp(&before, &opt::Get(), sizeof(before)) != 0;
}

} // namespace menu

// Trok Veiculos -- menu no kit da casa: Exibir, Barras, Informacoes e Servidor (so leitura). A distancia vai de
// 5 m ate os 20 m do /dl, nunca alem.

#include "menu.h"

#include "config.h"
#include "overlay.h"
#include "trok_ui.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace menu {
namespace {

using namespace tui;
using veic::cfg;

constexpr const char* VERSION = "0.1.0";
constexpr const char* VERSION_TAG = "v0.1";

const char* const TABS[] = {"Exibir", "Barras", "Informa\xC3\xA7\xC3\xB5" "es", "Servidor"};
constexpr int TAB_COUNT = 4;

struct State {
    bool open = false;
    int tab = 0;
    int selected[TAB_COUNT] = {1, 1, 1, 1};
    int rowCount[TAB_COUNT] = {1, 1, 1, 1};
    Shell shell;
    int age = 0; // quadros desde que abriu: no primeiro, o Enter do /trokveiculos nao aciona nada
} S;

const veic::Config DEFAULTS;

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

bool KeyboardFree() {
    return !ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && !CapturingKey();
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

// Caixa da previa, com a placa de exemplo no meio.
void Preview(float x, float w, const veic::Card& card) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 at = ImGui::GetCursorScreenPos();
    float top = at.y + 4 * u;
    float caption = fonts.desc->FontSize + 10 * u;
    float cardH = veic::CardPreviewHeight(card);
    float h = caption + cardH + 12 * u;
    dl->AddRectFilled(ImVec2(x, top), ImVec2(x + w, top + h), col::White(7), 8 * u);
    dl->AddText(fonts.desc, fonts.desc->FontSize, ImVec2(std::floor(x + 12 * u), std::floor(top + 6 * u)), col::hint, "Pr\xC3\xA9via");
    veic::DrawCardPreview(dl, x + w * 0.5f, top + caption, card, static_cast<float>(ImGui::GetTime()));
    ImGui::Dummy(ImVec2(w, h + 4 * u));
}

void TabShow(Rows& r) {
    r.Toggle("Trok Ve\xC3\xAD" "culos ligado", &cfg.enabled);
    static const char* const show[] = {"Sempre", "Ao dar dano"};
    r.Segmented("Mostrar", &cfg.show, show, 2, DEFAULTS.show);
    r.Slider("Some depois do dano", &cfg.damageHold, 3, 15, 1, "%d s", DEFAULTS.damageHold);
    r.Toggle("Esconder o meu", &cfg.hideMine);
    r.Toggle("Ve\xC3\xAD" "culos vazios", &cfg.empty);
    static const char* const position[] = {"Embaixo", "Em cima"};
    r.Segmented("Posi\xC3\xA7\xC3\xA3o", &cfg.position, position, 2, DEFAULTS.position);
    r.Slider("Tamanho", &cfg.size, 80, 130, 5, "%d%%", DEFAULTS.size);
    // 0 = os 20 m do /dl (o padrao). A barra vai so ate eles.
    int limit = static_cast<int>(samp::DL_RANGE);
    int value = cfg.distance > 0 ? std::min(cfg.distance, limit) : limit;
    if (r.Slider("Dist\xC3\xA2ncia (/dl: 20 m)", &value, 5, limit, 1, "%d m", limit)) {
        cfg.distance = value >= limit ? 0 : value;
    }
    static const char* const walls[] = {"Esconder", "Como o /dl"};
    r.Segmented("Atr\xC3\xA1s de parede", &cfg.walls, walls, 2, DEFAULTS.walls);
    r.Toggle("Desbotar no fim do alcance", &cfg.fade);
}

void TabBars(Rows& r, float x, float w) {
    r.Toggle("Blindagem (vida acima de 1000)", &cfg.armour);
    static const char* const cap[] = {"Maior visto", "Fixo"};
    r.Segmented("Teto da blindagem", &cfg.armourCap, cap, 2, DEFAULTS.armourCap);
    r.Slider("Teto fixo", &cfg.armourFixed, 500, 10000, 500, "%d", DEFAULTS.armourFixed);
    r.Toggle("Cores da fuma\xC3\xA7" "a e do fogo", &cfg.stateColors);
    r.Toggle("Linha do fogo (250)", &cfg.fireLine);
    r.Toggle("Dano que voc\xC3\xAA deu", &cfg.damageBar);
    r.Toggle("N\xC3\xBAmero do dano", &cfg.damageNumber);
    veic::Card c;
    c.name = "Huntley";
    c.id = 207;
    c.model = 579;
    c.health = 4210.0f;
    c.armourMax = cfg.armourCap == 1 ? static_cast<float>(cfg.armourFixed) : 4000.0f;
    c.kind = veic::KIND_CAR;
    Preview(x, w, c);
}

void TabInfo(Rows& r, float x, float w) {
    r.Toggle("Nome do modelo", &cfg.name);
    r.Toggle("ID do ve\xC3\xAD" "culo", &cfg.id);
    r.Toggle("Vida em n\xC3\xBAmero", &cfg.healthNumber);
    r.Toggle("Pneus furados", &cfg.tires);
    r.Toggle("Cadeado (trancado para voc\xC3\xAA)", &cfg.lock);
    r.Section("Linha de detalhes");
    r.Toggle("N\xC3\xBAmero do modelo", &cfg.detailModel);
    r.Toggle("Dist\xC3\xA2ncia", &cfg.detailDistance);
    r.Toggle("Lugares", &cfg.detailSeats);
    veic::Card c;
    c.name = "Sultan";
    c.id = 12;
    c.model = 560;
    c.seats = 3;
    c.distance = 9.8f;
    c.health = 742.0f;
    c.kind = veic::KIND_CAR;
    c.tires = veic::TIRE_RR;
    c.locked = true;
    Preview(x, w, c);
}

void TabServer(Rows& r, const samp::ServerInfo& server) {
    if (!server.connected) {
        r.Value("Servidor", "desconectado");
        return;
    }
    r.Section("Lido agora");
    r.Value("Endere\xC3\xA7o", server.address);
    r.Value("Alcance do /dl", "20 m");
    char dist[32];
    snprintf(dist, sizeof(dist), "%d m", cfg.distance > 0 ? cfg.distance : static_cast<int>(samp::DL_RANGE));
    r.Value("Sua dist\xC3\xA2ncia", dist);
    r.Value("Atr\xC3\xA1s de parede", cfg.walls == 0 ? "a placa some" : "aparece, como no /dl");
    r.Section("Como o Trok Ve\xC3\xAD" "culos segue");
    r.Value("Nunca passa do alcance do /dl", "sempre");
    r.Value("S\xC3\xB3 o que o /dl e o carro mostram", "sempre");
    r.Value("Escolhas salvas", "por servidor");
}

} // namespace

const char* Version() {
    return VERSION;
}

void Toggle() {
    S.open = !S.open;
    S.age = 0;
}

bool CapturesInput() {
    return S.open;
}

bool Frame(const samp::ServerInfo& server) {
    if (!S.open) {
        return false;
    }
    veic::Config before;
    memcpy(&before, &cfg, sizeof(cfg)); // copia byte a byte (com o enchimento), para o memcmp do fim
    ImGuiIO& io = ImGui::GetIO();
    bool fresh = S.age++ == 0;
    bool keysOk = !fresh && KeyboardFree();
    Keys keys = ReadKeys(keysOk);
    bool escape = keysOk && ImGui::IsKeyPressed(ImGuiKey_Escape, false);

    float width = 640 * u;
    float height = std::min(io.DisplaySize.y * 0.72f, 560 * u);
    PushStyle();
    bool keep = BeginShell("##trokVeiculos", "Trok Ve\xC3\xAD" "culos", VERSION_TAG, width, height, S.shell);

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
    case 0: TabShow(rows); break;
    case 1: TabBars(rows, cp.x + 4 * u, rowW); break;
    case 2: TabInfo(rows, cp.x + 4 * u, rowW); break;
    default: TabServer(rows, server); break;
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
    return memcmp(&before, &cfg, sizeof(cfg)) != 0;
}

} // namespace menu

// Trok Tags -- menu no kit da casa: Nametag, Baloes, Fixos, Indicadores e Servidor (o que o servidor permite,
// so leitura). A distancia vai de 5 m ate o limite do servidor, nunca alem.

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
using tags::cfg;

constexpr const char* VERSION = "0.2.0";
constexpr const char* VERSION_TAG = "v0.2";

const char* const TABS[] = {"Nametag", "Bal\xC3\xB5" "es", "Fixos", "Indicadores", "Servidor"};
constexpr int TAB_COUNT = 5;

struct State {
    bool open = false;
    int tab = 0;
    int selected[TAB_COUNT] = {1, 1, 1, 1, 1};
    int rowCount[TAB_COUNT] = {1, 1, 1, 1, 1};
    Shell shell;
    int age = 0; // quadros desde que abriu: no primeiro, o Enter do /troktags nao aciona nada
} S;

const tags::Config DEFAULTS;

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

int ServerLimit(const samp::ServerInfo& server) {
    float d = server.connected ? server.nameTagDistance : 70.0f;
    if (!(d >= 5.0f && d < 10000.0f)) {
        d = 70.0f;
    }
    return static_cast<int>(std::floor(d));
}

void TabNametag(Rows& r, const samp::ServerInfo& server) {
    r.Toggle("Trok Tags ligado", &cfg.enabled);
    static const char* const style[] = {"P\xC3\xADlula", "Limpo"};
    r.Segmented("Estilo", &cfg.style, style, 2, DEFAULTS.style);
    static const char* const color[] = {"Na faixa", "No nome"};
    r.Segmented("Cor do jogador", &cfg.colorMode, color, 2, DEFAULTS.colorMode);
    r.Toggle("Mostrar o ID", &cfg.showId);
    r.Toggle("Vida e colete", &cfg.bars);
    r.Toggle("[conte\xC3\xBA" "do] vira etiqueta", &cfg.nameTags);
    r.Slider("Tamanho", &cfg.size, 80, 130, 5, "%d%%", DEFAULTS.size);
    // 0 = o limite do servidor (o padrao). A barra vai so ate ele.
    int limit = ServerLimit(server);
    int value = cfg.distance > 0 ? std::min(cfg.distance, limit) : limit;
    char label[64];
    snprintf(label, sizeof(label), "Dist\xC3\xA2ncia (servidor: %d m)", limit);
    if (r.Slider(label, &value, std::min(5, limit), limit, 1, "%d m", limit)) {
        cfg.distance = value >= limit ? 0 : value;
    }
    r.Toggle("Desbotar com a dist\xC3\xA2ncia", &cfg.fade);
}

void TabBubbles(Rows& r, const samp::ServerInfo& server) {
    r.Toggle("Bal\xC3\xB5" "es no estilo da casa", &cfg.bubbles);
    if (server.bubbles < 0) {
        r.Info("Bal\xC3\xB5" "es do SA-MP mantidos",
               "Neste samp.dll a estrutura dos bal\xC3\xB5" "es n\xC3\xA3o conferiu, ent\xC3\xA3o eles continuam os do SA-MP. "
               "Me mande o arquivo Trok Tags.log (pasta do .asi).");
    }
    r.Slider("Largura m\xC3\xA1xima", &cfg.bubbleWidth, 60, 160, 10, "%d%%", DEFAULTS.bubbleWidth);
    r.Toggle("[conte\xC3\xBA" "do] vira etiqueta", &cfg.bubbleTags);
    r.Toggle("(( OOC )) vira etiqueta", &cfg.ooc);
    r.Slider("Fundo do bal\xC3\xA3o", &cfg.bubbleOpacity, 40, 100, 5, "%d%%", DEFAULTS.bubbleOpacity);
}

void TabFixed(Rows& r, float x, float w) {
    r.Toggle("Textos fixos no estilo da casa", &cfg.fixed);
    r.Toggle("[conte\xC3\xBA" "do] vira etiqueta", &cfg.fixedTags);
    static const char* const word[] = {"Etiqueta", "Texto", "Esconder"};
    r.Segmented("Palavra do come\xC3\xA7o (RG:)", &cfg.fixedWord, word, 3, DEFAULTS.fixedWord);
    static const char* const chip[] = {"Do texto", "Azul", "Branca"};
    r.Segmented("Cor da etiqueta", &cfg.fixedChip, chip, 3, DEFAULTS.fixedChip);
    static const char* const text[] = {"Do servidor", "Branco"};
    r.Segmented("Cor do texto", &cfg.fixedText, text, 2, DEFAULTS.fixedText);
    // A linha que fica no lugar da nametag quando o servidor a esconde (o RG do Horizonte).
    r.Section("Sem a nametag do servidor");
    static const char* const initials[] = {"N\xC3\xA3o", "2 letras", "Iniciais"};
    r.Segmented("Sigla do nome", &cfg.fixedInitials, initials, 3, DEFAULTS.fixedInitials);
    r.Toggle("ID do jogador", &cfg.fixedId);
    static const char* const extra[] = {"Azul", "Branco", "Do texto"};
    r.Segmented("Cor da sigla e do ID", &cfg.fixedIdColor, extra, 3, DEFAULTS.fixedIdColor);
    r.Info("Vida, colete e AFK",
           "Vida, colete e AFK s\xC3\xB3 aparecem com a nametag do servidor. A sigla e o ID entram na linha do texto "
           "fixo mais perto da cabe\xC3\xA7" "a, s\xC3\xB3 em quem est\xC3\xA1 sem a nametag do servidor.");
    ImVec2 at = ImGui::GetCursorScreenPos();
    float h = tags::DrawFixedPreview(ImGui::GetWindowDrawList(), x, at.y + 4 * u, w);
    ImGui::Dummy(ImVec2(w, h + 4 * u));
}

void TabIndicators(Rows& r) {
    r.Toggle("AFK", &cfg.afk);
    r.Toggle("Tempo de AFK", &cfg.afkTime);
    r.Toggle("Digitando", &cfg.typing);
    r.Toggle("Ping alto", &cfg.ping);
    r.Slider("Ping alto a partir de", &cfg.pingLimit, 100, 600, 25, "%d ms", DEFAULTS.pingLimit);
    r.Toggle("Vida baixa pulsa", &cfg.lowHealth);
}

void TabServer(Rows& r, const samp::ServerInfo& server) {
    if (!server.connected) {
        r.Value("Servidor", "desconectado");
        return;
    }
    r.Section("Lido do servidor agora");
    r.Value("Endere\xC3\xA7o", server.address);
    r.Value("Nametags", server.nameTags ? "vis\xC3\xADveis" : "ocultas");
    char dist[32];
    snprintf(dist, sizeof(dist), "%d m", ServerLimit(server));
    r.Value("Dist\xC3\xA2ncia da nametag", dist);
    r.Value("Atr\xC3\xA1s de parede", server.nameTagLos ? "escondidas" : "aparecem");
    r.Value("Bal\xC3\xB5" "es", server.bubbles > 0 ? "do Trok Tags" : server.bubbles < 0 ? "do SA-MP" : "esperando o primeiro");
    r.Section("Como o Trok Tags segue");
    r.Value("Nunca passa do limite", "sempre");
    r.Value("Vida, colete e AFK", "s\xC3\xB3 com a nametag");
    r.Value("Sigla e ID sem a nametag", cfg.fixedInitials || cfg.fixedId ? "ligados (aba Fixos)" : "desligados");
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
    tags::Config before;
    memcpy(&before, &cfg, sizeof(cfg)); // copia byte a byte (com o enchimento), para o memcmp do fim
    ImGuiIO& io = ImGui::GetIO();
    bool fresh = S.age++ == 0;
    bool keysOk = !fresh && KeyboardFree();
    Keys keys = ReadKeys(keysOk);
    bool escape = keysOk && ImGui::IsKeyPressed(ImGuiKey_Escape, false);

    float width = 640 * u;
    float height = std::min(io.DisplaySize.y * 0.72f, 560 * u);
    PushStyle();
    bool keep = BeginShell("##trokTags", "Trok Tags", VERSION_TAG, width, height, S.shell);

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
    case 0: TabNametag(rows, server); break;
    case 1: TabBubbles(rows, server); break;
    case 2: TabFixed(rows, cp.x + 4 * u, rowW); break;
    case 3: TabIndicators(rows); break;
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

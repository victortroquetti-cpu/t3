// Trok UI Showcase -- vitrine de todos os controles de menu da casa (.asi).
// Serve de referencia visual para os proximos mods: abra com /trokui (F10 sem SA-MP).

#include "showcase.h"
#include "trok_ui.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace showcase {
namespace {

using namespace tui;

constexpr const char* VERSION = "1.0.0";
constexpr const char* VERSION_TAG = "v1.0.0";

enum class Dialog { None, Message, Input, Password, List, Table };

struct Values {
    int lines = 7;
    int align = 1;
    bool numbers = true;
    bool blur = true;
    int shape = 1;
    int iconSpot = 0;
    int size = 100;
    int border = 2;
    int zoom = 250;
    ImGuiKey mapKey = ImGuiKey_M;
    ImU32 mapColor = IM_COL32(230, 230, 230, 255);
    int font = 0;

    char name[64] = "";
    char password[64] = "";
    char search[64] = "";
    char note[256] = "";

    bool checks[3] = {true, false, true};
    int radio = 1;
};

struct State {
    bool open = false;
    int tab = 0;
    int selected[5] = {1, 1, 1, 1, 1};
    int rowCount[5] = {1, 1, 1, 1, 1};
    Shell shell;
    Values v;

    bool moving = false;
    bool movingDrag = false;
    ImVec2 preview = ImVec2(-1, -1);
    ImVec2 previewStart;

    Dialog dialog = Dialog::None;
    Shell dialogShell;
    int dialogSelected = 1;
    int dialogRows = 1;
    char dialogText[64] = "";

    bool confirm = false;

    // Quadros desde que cada tela abriu. No primeiro quadro o teclado e ignorado: o Enter que
    // mandou o /trokui (ou que abriu a tela) nao pode acionar nada dentro dela.
    int menuAge = 0;
    int dialogAge = 0;
    int confirmAge = 0;
} S;

const Values DEFAULTS;

const char* const TABS[] = {"Linhas", "Texto", "Listas", "Avisos", "Di\xC3\xA1logos"};
constexpr int TAB_COUNT = 5;

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
    for (int i = 0; i < 9; ++i) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i), false) ||
            ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_Keypad1 + i), false)) {
            k.digit = i + 1;
        }
    }
    return k;
}

bool KeyboardFree() {
    ImGuiIO& io = ImGui::GetIO();
    return !io.WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && !CapturingKey();
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

// Escurece o jogo atras de dialogos e confirmacoes (os dialogos da casa usam desfoque; aqui, um veu).
// O veu nunca pega o foco (isso tiraria o cursor do campo de texto do dialogo); ele so sobe para a
// frente no quadro em que aparece, e a janela de cima pega o foco no quadro dela.
void Backdrop(const char* id) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin(id, nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    if (ImGui::IsWindowAppearing()) {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    }
    // Sem o recorte da janela (meio WindowPadding nas laterais), o veu cobre a tela inteira.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRectFullScreen();
    dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 110));
    dl->PopClipRect();
    ImGui::End();
    ImGui::PopStyleVar();
}

// ---------------------------------------------------------------- abas do menu

void TabLines(Rows& r) {
    Values& v = S.v;
    if (r.Action("Posi\xC3\xA7\xC3\xA3o da pr\xC3\xA9via", "Mover pr\xC3\xA9via")) {
        S.moving = true;
        S.movingDrag = false;
        S.previewStart = S.preview;
    }
    r.Stepper("Linhas vis\xC3\xADveis", &v.lines, 1, 20, 1, "%d", DEFAULTS.lines);
    static const char* const align[] = {"Esquerda", "Direita"};
    r.Cycle("Alinhamento", &v.align, align, 2, DEFAULTS.align);
    r.Toggle("N\xC3\xBAmeros nas linhas", &v.numbers);
    r.Toggle("Desfocar o fundo", &v.blur);
    static const char* const shape[] = {"Quadrado", "Redondo"};
    r.Segmented("Formato", &v.shape, shape, 2, DEFAULTS.shape);
    static const char* const spot[] = {"Dentro", "Na borda"};
    r.Segmented("\xC3\x8D" "cones", &v.iconSpot, spot, 2, DEFAULTS.iconSpot);
    r.Slider("Tamanho", &v.size, 80, 130, 5, "%d%%", DEFAULTS.size);
    r.Slider("Espessura da borda", &v.border, 0, 8, 1, "%d px", DEFAULTS.border);
    r.Slider("Zoom (alcance)", &v.zoom, 100, 600, 10, "%d m", DEFAULTS.zoom);
    r.Keybind("Mapa r\xC3\xA1pido", &v.mapKey, DEFAULTS.mapKey);
    r.Color("Cor do mapa", &v.mapColor, DEFAULTS.mapColor);
    static const char* const fontsList[] = {"Normal", "Compacta", "Grande", "Monoespa\xC3\xA7" "ada"};
    r.Dropdown("Fonte do texto", &v.font, fontsList, 4, DEFAULTS.font);
}

void TabText(Rows& r) {
    Values& v = S.v;
    r.Input("Nome", v.name, sizeof(v.name), "Seu nome no servidor");
    r.Input("Senha", v.password, sizeof(v.password), "Nunca \xC3\xA9 lembrada", true);
    r.Search("Buscar", v.search, sizeof(v.search), "Digite para filtrar");
    r.Multiline("Observa\xC3\xA7\xC3\xA3o", v.note, sizeof(v.note), "Texto longo, v\xC3\xA1rias linhas");
    r.Info("Menu do campo",
           "Bot\xC3\xA3o direito em qualquer campo abre Copiar, Colar, Recortar e Limpar. "
           "Enter come\xC3\xA7" "a a digitar na linha selecionada; Esc sai do campo.");
}

void TabLists(Rows& r) {
    Values& v = S.v;
    r.Section("Lista numerada  \xC2\xB7  teclas 1 a 9");
    static const char* const items[] = {"Spawnar no hospital", "Spawnar em casa", "Spawnar na fac\xC3\xA7\xC3\xA3o"};
    for (const char* item : items) {
        if (r.Item(item)) {
            Toast("Item escolhido", item);
        }
    }

    r.Section("Tabela com cabe\xC3\xA7" "alho");
    static const char* const cols[] = {"Jogador", "N\xC3\xADvel", "Ping"};
    static const float widths[] = {220, 120, 80};
    r.TableHeader(cols, widths, 3);
    static const char* const rowsData[][3] = {
        {"Victor_Trok", "32", "41 ms"}, {"Enzo_Rampani", "18", "63 ms"}, {"Catharina", "25", "38 ms"}};
    for (const auto& row : rowsData) {
        if (r.TableItem(row, widths, 3)) {
            Toast("Linha escolhida", row[0]);
        }
    }

    r.Section("V\xC3\xA1rias escolhas");
    r.CheckItem("Mostrar territ\xC3\xB3rios", &v.checks[0]);
    r.CheckItem("Rota de miss\xC3\xA3o", &v.checks[1]);
    r.CheckItem("Indicador de norte", &v.checks[2]);

    r.Section("Uma escolha");
    r.RadioItem("Norte fixo", &v.radio, 0);
    r.RadioItem("Gira com a c\xC3\xA2mera", &v.radio, 1);
    r.RadioItem("Personagem centralizado", &v.radio, 2);
}

void TabFeedback(Rows& r) {
    float t = static_cast<float>(std::fmod(ImGui::GetTime() * 0.18, 1.0));
    r.Progress("Download", t);
    r.Progress("Sincronizando", 0, true);
    r.Loading("Conectando", "Aguarde");
    static const char* const badges[] = {"Novo", "Beta", "v3.3"};
    static const int kinds[] = {1, 2, 0};
    r.Badges("Etiquetas", badges, kinds, 3);
    r.Info("Dica", "Textos de apoio aparecem ap\xC3\xB3s um instante com o mouse parado em cima.");
    if (r.Action("Notifica\xC3\xA7\xC3\xA3o", "Mostrar")) {
        Toast("Trok UI", "Notifica\xC3\xA7\xC3\xA3o no canto da tela. Clique para dispensar.");
    }
    if (r.Action("Confirma\xC3\xA7\xC3\xA3o", "Abrir")) {
        S.confirm = true;
        S.confirmAge = 0;
    }
}

void TabDialogs(Rows& r) {
    if (r.Action("Mensagem", "Abrir")) {
        S.dialog = Dialog::Message;
    }
    if (r.Action("Entrada de texto", "Abrir")) {
        S.dialog = Dialog::Input;
    }
    if (r.Action("Senha", "Abrir")) {
        S.dialog = Dialog::Password;
    }
    if (r.Action("Lista", "Abrir")) {
        S.dialog = Dialog::List;
    }
    if (r.Action("Tabela com cabe\xC3\xA7" "alho", "Abrir")) {
        S.dialog = Dialog::Table;
    }
    if (S.dialog != Dialog::None) {
        S.dialogSelected = 1;
        S.dialogText[0] = 0;
        S.dialogShell = Shell();
        S.dialogAge = 0;
    }
}

void DrawMenu() {
    ImGuiIO& io = ImGui::GetIO();
    bool fresh = S.menuAge++ == 0;
    bool keysOk = !fresh && KeyboardFree() && !S.confirm;
    Keys keys = ReadKeys(keysOk);
    if (keysOk && ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
        S.tab = (S.tab + (io.KeyShift ? TAB_COUNT - 1 : 1)) % TAB_COUNT;
    }
    bool escape = keysOk && ImGui::IsKeyPressed(ImGuiKey_Escape, false);

    float width = 640 * u;
    float height = std::min(io.DisplaySize.y * 0.72f, 540 * u);
    PushStyle();
    bool keep = BeginShell("##trokUiShowcase", "Trok UI", VERSION_TAG, width, height, S.shell);

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
    rows.Begin(TABS[S.tab], cp.x + 4 * u, rowW, sel, keys, S.v.numbers);
    switch (S.tab) {
    case 0: TabLines(rows); break;
    case 1: TabText(rows); break;
    case 2: TabLists(rows); break;
    case 3: TabFeedback(rows); break;
    default: TabDialogs(rows); break;
    }
    S.rowCount[S.tab] = rows.End();
    ImGui::Dummy(ImVec2(0, 6 * u));
    ImGui::EndChild();

    static const std::vector<Hint> hints = {
        {"Setas", "Ajustar", false}, {"Tab", "Trocar de aba", false}, {"Esc", "Fechar", true}};
    int clicked = EndShell(hints);
    PopStyle();

    if (!keep || clicked == 2 || escape) {
        S.open = false;
    }
}

// ---------------------------------------------------------------- confirmacao

void DrawConfirm() {
    Backdrop("##veuConfirmar");
    float width = 400 * u, height = HeaderHeight() + 92 * u + FooterHeight();
    PushStyle();
    if (S.confirmAge == 0) {
        ImGui::SetNextWindowFocus();
    }
    Shell shell;
    bool keep = BeginShell("##trokConfirmar", "Restaurar padr\xC3\xB5" "es?", nullptr, width, height, shell);
    ImVec2 pos = ImGui::GetWindowPos();
    float bodyY = pos.y + HeaderHeight() + 20 * u;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    RichText(dl, fonts.body, pos.x + 22 * u, bodyY, col::text,
             "Todas as linhas voltam ao valor padr\xC3\xA3o.\n{9A9A9A}Isso n\xC3\xA3o pode ser desfeito.");
    static const std::vector<Hint> hints = {{"Enter", "Confirmar", true}, {"Esc", "Cancelar", true}};
    int clicked = EndShell(hints, true);
    PopStyle();

    bool keysOk = S.confirmAge++ > 0;
    bool yes = clicked == 0 || (keysOk && ImGui::IsKeyPressed(ImGuiKey_Enter, false));
    bool no = !keep || clicked == 1 || (keysOk && ImGui::IsKeyPressed(ImGuiKey_Escape, false));
    if (yes) {
        Values fresh;
        std::memcpy(fresh.name, S.v.name, sizeof(fresh.name));
        S.v = fresh;
        S.confirm = false;
        Toast("Padr\xC3\xB5" "es restaurados", "Todas as linhas voltaram ao valor original.");
    } else if (no) {
        S.confirm = false;
    }
}

// ---------------------------------------------------------------- dialogos de exemplo (Trok Dialogs)

void DrawDialog() {
    ImGuiIO& io = ImGui::GetIO();
    if (S.v.blur) {
        Backdrop("##veuDialogo");
    }
    const char* title = "";
    const char* body = nullptr;
    const char* ok = "Enviar";
    const char* cancel = "Cancelar";
    bool hasField = false, password = false;
    switch (S.dialog) {
    case Dialog::Message:
        title = "Bem-vindo";
        body = "{FFFFFF}Bem-vindo ao {9AD0FF}Trok Roleplay{FFFFFF}!\n"
               "-> As regras est\xC3\xA3o em {FFD27A}/regras{FFFFFF}.\n"
               ">> Setas do servidor viram \xC3\xAD" "cones do lucide.\n"
               "{9A9A9A}Cores no formato {RRGGBB} funcionam.";
        ok = "Ok";
        cancel = nullptr;
        break;
    case Dialog::Input:
        title = "Nome do ve\xC3\xAD" "culo";
        body = "Digite um nome para o seu ve\xC3\xAD" "culo:";
        hasField = true;
        break;
    case Dialog::Password:
        title = "Login";
        body = "Esta conta est\xC3\xA1 registrada.\nDigite a sua senha para entrar:";
        ok = "Entrar";
        cancel = "Sair";
        hasField = true;
        password = true;
        break;
    case Dialog::List:
        title = "Spawn";
        ok = "Selecionar";
        break;
    default:
        title = "Jogadores online";
        ok = "Selecionar";
        break;
    }

    float bodyW = 0, bodyH = 0;
    if (body) {
        ImVec2 bs = RichText(nullptr, fonts.body, 0, 0, col::text, body, false);
        bodyW = bs.x;
        bodyH = bs.y;
    }
    float padX = 22 * u;
    float width = std::max(380 * u, bodyW + padX * 2);
    float content = 0;
    if (body) {
        content += bodyH;
    }
    if (hasField) {
        content += 14 * u + 30 * u;
    }
    bool isList = S.dialog == Dialog::List || S.dialog == Dialog::Table;
    if (S.dialog == Dialog::List) {
        width = std::max(width, 420 * u);
        content += 5 * 30 * u;
    } else if (S.dialog == Dialog::Table) {
        width = std::max(width, 480 * u);
        content += 26 * u + 4 * 30 * u;
    }
    float height = HeaderHeight() + 18 * u + content + 18 * u + FooterHeight();
    height = std::min(height, io.DisplaySize.y * 0.8f);

    bool fresh = S.dialogAge++ == 0;
    bool keysOk = !fresh && !io.WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    Keys keys = ReadKeys(keysOk && isList);
    bool submit = !fresh && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
    bool escape = !fresh && ImGui::IsKeyPressed(ImGuiKey_Escape, false) &&
                  !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);

    PushStyle();
    if (fresh) {
        ImGui::SetNextWindowFocus();
    }
    bool keep = BeginShell("##trokDialog", title, nullptr, width, height, S.dialogShell);
    ImVec2 pos = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float y = pos.y + HeaderHeight() + 18 * u;
    if (body) {
        RichText(dl, fonts.body, pos.x + padX, y, col::text, body);
        y += bodyH;
    }
    int activated = 0;
    if (hasField) {
        y += 14 * u;
        TextField("##entrada", S.dialogText, sizeof(S.dialogText), password ? "Senha" : "Digite aqui", password,
                  pos.x + padX, y, width - 2 * padX, 30 * u, ImGui::IsWindowAppearing());
    } else if (isList) {
        ImGui::SetCursorScreenPos(ImVec2(pos.x + padX - 12 * u, y));
        Rows list;
        MoveSelection(&S.dialogSelected, S.dialogRows, keys, list);
        list.Begin("##lista", pos.x + padX - 12 * u, width - 2 * padX + 24 * u, &S.dialogSelected, keys, true);
        if (S.dialog == Dialog::List) {
            static const char* const items[] = {"> Hospital", "> Casa", "> Fac\xC3\xA7\xC3\xA3o", "-> Emprego",
                                                "\xC2\xBB \xC3\x9Altima posi\xC3\xA7\xC3\xA3o"};
            for (int i = 0; i < 5; ++i) {
                if (list.Item(items[i])) {
                    activated = i + 1;
                }
            }
        } else {
            static const char* const cols[] = {"Jogador", "N\xC3\xADvel", "Ping"};
            static const float widths[] = {200, 110, 80};
            list.TableHeader(cols, widths, 3);
            static const char* const data[][3] = {{"Victor_Trok", "32", "41 ms"},
                                                  {"Enzo_Rampani", "18", "63 ms"},
                                                  {"Catharina", "25", "38 ms"},
                                                  {"Camilla", "12", "55 ms"}};
            for (int i = 0; i < 4; ++i) {
                if (list.TableItem(data[i], widths, 3)) {
                    activated = i + 1;
                }
            }
        }
        S.dialogRows = list.End();
    }

    std::vector<Hint> hints = {{"Enter", ok, true}};
    if (cancel) {
        hints.push_back({"Esc", cancel, true});
    }
    int clicked = EndShell(hints, true);
    PopStyle();

    if (activated || clicked == 0 || (submit && !isList)) {
        char msg[160];
        if (isList) {
            int item = activated ? activated : S.dialogSelected;
            snprintf(msg, sizeof(msg), "Bot\xC3\xA3o %s, item %d.", ok, item);
        } else if (hasField) {
            snprintf(msg, sizeof(msg), "Bot\xC3\xA3o %s, %d caracteres digitados.", ok,
                     static_cast<int>(std::strlen(S.dialogText)));
        } else {
            snprintf(msg, sizeof(msg), "Bot\xC3\xA3o %s.", ok);
        }
        Toast("Resposta do di\xC3\xA1logo", msg);
        S.dialog = Dialog::None;
    } else if (!keep || clicked == 1 || escape) {
        Toast("Resposta do di\xC3\xA1logo", cancel ? "Cancelado (bot\xC3\xA3o direito)." : "Fechado.");
        S.dialog = Dialog::None;
    }
}

// ---------------------------------------------------------------- modo de mover (Trok Radar / Kill List)

void DrawMove() {
    ImGuiIO& io = ImGui::GetIO();
    float cw = 260 * u, ch = 64 * u;
    if (S.preview.x < 0) {
        S.preview = ImVec2(io.DisplaySize.x - cw - 40 * u, 120 * u);
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##trokMover", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::SetCursorScreenPos(S.preview);
    ImGui::InvisibleButton("##previa", ImVec2(cw, ch));
    bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0.0f)) {
        S.movingDrag = true;
        S.preview.x = std::max(0.0f, std::min(io.DisplaySize.x - cw, S.preview.x + io.MouseDelta.x));
        S.preview.y = std::max(0.0f, std::min(io.DisplaySize.y - ch, S.preview.y + io.MouseDelta.y));
    }
    if (hovered || S.movingDrag) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }

    ImVec2 a = S.preview, b(a.x + cw, a.y + ch);
    dl->AddRectFilled(a, b, IM_COL32(12, 12, 12, 230), 10 * u);
    float dash = S.movingDrag ? 120.0f : (hovered ? 90.0f : 60.0f);
    dl->AddRect(a, b, col::White(static_cast<int>(dash)), 10 * u, 0, 1.5f * u);
    Text(dl, fonts.body, a.x + 16 * u, a.y + 12 * u, col::text, "Pr\xC3\xA9via");
    Text(dl, fonts.desc, a.x + 16 * u, a.y + 16 * u + fonts.body->FontSize, col::hint,
         S.movingDrag ? "Solte para fixar" : "Arraste para mover");
    ImGui::End();
    ImGui::PopStyleVar(2);

    // Pilula de ajuda no rodape da tela.
    const char* help = S.movingDrag ? "Solte para fixar"
                                    : "Arraste a pr\xC3\xA9via  |  Esc ou bot\xC3\xA3o direito cancela";
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    ImVec2 hs = TextSize(fonts.body, help);
    float px = (io.DisplaySize.x - hs.x) * 0.5f - 18 * u, py = io.DisplaySize.y - 28 * u - hs.y - 24 * u;
    fg->AddRectFilled(ImVec2(px, py), ImVec2(px + hs.x + 36 * u, py + hs.y + 24 * u), IM_COL32(12, 12, 12, 252),
                      10 * u);
    fg->AddRect(ImVec2(px, py), ImVec2(px + hs.x + 36 * u, py + hs.y + 24 * u), col::White(14), 10 * u, 0, 1.0f);
    Text(fg, fonts.body, px + 18 * u, py + 12 * u, col::text, help);

    if (S.movingDrag && !ImGui::IsMouseDown(0)) {
        S.moving = false;
        S.movingDrag = false;
        Toast("Posi\xC3\xA7\xC3\xA3o salva", "A pr\xC3\xA9via ficou onde voc\xC3\xAA soltou.");
    } else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsMouseClicked(1)) {
        S.preview = S.previewStart;
        S.moving = false;
        S.movingDrag = false;
    }
}

} // namespace

const char* Version() {
    return VERSION;
}

void BuildFonts(float screenHeight, HMODULE) {
    char dir[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, dir, MAX_PATH);
    char* slash = strrchr(dir, '\\');
    if (slash) {
        slash[1] = 0;
    } else {
        dir[0] = 0;
    }
    tui::BuildFonts(screenHeight, dir);
}

void Toggle() {
    if (S.moving) {
        S.preview = S.previewStart;
        S.moving = false;
        return;
    }
    if (S.dialog != Dialog::None) {
        S.dialog = Dialog::None;
        return;
    }
    S.open = !S.open;
    S.confirm = false;
    S.menuAge = 0;
}

bool CapturesInput() {
    return S.open || S.moving || S.dialog != Dialog::None;
}

bool WantsFrame() {
    return CapturesInput() || ToastsVisible();
}

void Frame() {
    if (S.moving) {
        DrawMove();
    } else if (S.dialog != Dialog::None) {
        DrawDialog();
    } else if (S.open) {
        DrawMenu();
        if (S.confirm) {
            DrawConfirm();
        }
    }
    DrawToasts();
}

} // namespace showcase

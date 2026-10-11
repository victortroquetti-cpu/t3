// Trok Chat (.asi) -- Victor_Trok
// O menu do /trokchat, com o kit da casa (o mesmo casco, linhas e rodape dos outros menus). As teclas trocam como nos
// menus da casa: Enter (ou clique) na linha, e a proxima tecla apertada fica no lugar; Backspace deixa sem tecla, Esc
// cancela. Linhas, hora e o contorno sao os do proprio SA-MP (/pagesize, /timestamp e o F7), mudados na hora.

#include "menu.h"
#include "chat.h"
#include "options.h"
#include "trok_ui.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace menu {
namespace {

using namespace tui;

constexpr const char* VERSION_TAG = "v1.1";

struct State {
    bool open = false;
    int selected = 1;
    int rowCount = 1;
    Shell shell;
    int menuAge = 0; // quadros desde que abriu: no primeiro o teclado e ignorado (o Enter do /trokchat)
    bool confirm = false;
    int confirmAge = 0;
} S;

// F7 do SA-MP (0 escondido, 1 sem contorno, 2 com contorno) na ordem da linha.
const char* const CLOSED_MODES[] = {"Com contorno", "Sem contorno", "Escondido"};
constexpr int MODE_OF[] = {2, 1, 0};

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
    return !ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
           !CapturingKey();
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
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRectFullScreen();
    dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 110));
    dl->PopClipRect();
    ImGui::End();
    ImGui::PopStyleVar();
}

void DrawRows(Rows& r) {
    opt::Options& o = opt::Get();
    const opt::Options& d = opt::Defaults();
    host::ChatSettings chat = host::Settings();
    const host::ChatSettings before = chat;

    r.Section("Abrir o chat");
    r.Keybind("Tecla principal", &o.keys[0], d.keys[0]);
    r.Keybind("Tecla alternativa", &o.keys[1], d.keys[1]);
    if (o.keys[0] == ImGuiKey_None && o.keys[1] == ImGuiKey_None) {
        r.Info("Nenhuma tecla abre o chat", "Escolha pelo menos uma tecla. Sem elas, o chat s\xC3\xB3 abre por scripts.");
    }

    r.Section("Mensagens");
    r.Stepper("Linhas na tela", &chat.pageSize, 5, 20, 1, "%d", 10);
    r.Toggle("Hora antes de cada mensagem", &chat.timestamps);
    int closed = chat.mode == 1 ? 1 : chat.mode == 0 ? 2 : 0;
    if (r.Cycle("Chat fechado", &closed, CLOSED_MODES, 3, 0)) {
        chat.mode = MODE_OF[closed];
    }
    r.Toggle("[Colchetes] viram etiquetas", &o.tags);

    r.Section("Geral");
    r.Toggle("Usar o Trok Chat", &o.enabled);
    if (!o.enabled) {
        r.Info("O chat do SA-MP est\xC3\xA1 de volta",
               "Com isto desligado, o chat e a caixa de digita\xC3\xA7\xC3\xA3o s\xC3\xA3o os do SA-MP. O /trokchat "
               "continua abrindo este menu.");
    }
    if (r.Action("Padr\xC3\xB5" "es do Trok Chat", "Restaurar")) {
        S.confirm = true;
        S.confirmAge = 0;
    }

    if (chat.pageSize != before.pageSize || chat.timestamps != before.timestamps || chat.mode != before.mode) {
        host::Apply(chat);
    }
}

void DrawMenu() {
    ImGuiIO& io = ImGui::GetIO();
    const bool fresh = S.menuAge++ == 0;
    const bool keysOk = !fresh && KeyboardFree() && !S.confirm;
    Keys keys = ReadKeys(keysOk);
    const bool escape = keysOk && ImGui::IsKeyPressed(ImGuiKey_Escape, false);

    // Cabe tudo, mais as linhas de aviso (sem tecla, Trok Chat desligado) quando aparecem.
    const opt::Options& o = opt::Get();
    const int notes = (o.keys[0] == ImGuiKey_None && o.keys[1] == ImGuiKey_None) + !o.enabled;
    const float width = 560 * u;
    const float height = std::min(io.DisplaySize.y * 0.85f, (420 + 34 * notes) * u);
    PushStyle();
    const bool keep = BeginShell("##trokChatMenu", "Trok Chat", VERSION_TAG, width, height, S.shell);

    const ImVec2 pos = ImGui::GetWindowPos();
    const float padX = 18 * u;
    const float listTop = pos.y + HeaderHeight() + 8 * u;
    const float listH = height - (listTop - pos.y) - FooterHeight() - 8 * u;
    ImGui::SetCursorScreenPos(ImVec2(pos.x + padX - 4 * u, listTop));
    ImGui::BeginChild("##lista", ImVec2(width - 2 * padX + 8 * u, listH), false, ImGuiWindowFlags_NoBackground);
    const ImVec2 cp = ImGui::GetCursorScreenPos();
    const float rowW = ImGui::GetContentRegionAvail().x - 10 * u;

    Rows rows;
    MoveSelection(&S.selected, S.rowCount, keys, rows);
    rows.Begin("##opcoes", cp.x + 4 * u, rowW, &S.selected, keys, true);
    DrawRows(rows);
    S.rowCount = rows.End();
    ImGui::Dummy(ImVec2(0, 6 * u));
    ImGui::EndChild();

    static const std::vector<Hint> hints = {
        {"Setas", "Ajustar", false}, {"Enter", "Trocar a tecla", false}, {"Esc", "Fechar", true}};
    const int clicked = EndShell(hints);
    PopStyle();

    if (!keep || clicked == 2 || escape) {
        S.open = false;
    }
}

void DrawConfirm() {
    Backdrop("##veuConfirmar");
    const float width = 420 * u, height = HeaderHeight() + 92 * u + FooterHeight();
    PushStyle();
    if (S.confirmAge == 0) {
        ImGui::SetNextWindowFocus();
    }
    Shell shell;
    const bool keep = BeginShell("##trokChatConfirmar", "Restaurar padr\xC3\xB5" "es?", nullptr, width, height, shell);
    const ImVec2 pos = ImGui::GetWindowPos();
    RichText(ImGui::GetWindowDrawList(), fonts.body, pos.x + 22 * u, pos.y + HeaderHeight() + 20 * u, col::text,
             "T e F6 voltam a abrir o chat, com 10 linhas e contorno.\n"
             "{9A9A9A}O Trok Chat continua ligado.");
    static const std::vector<Hint> hints = {{"Enter", "Confirmar", true}, {"Esc", "Cancelar", true}};
    const int clicked = EndShell(hints, true);
    PopStyle();

    const bool keysOk = S.confirmAge++ > 0;
    const bool yes = clicked == 0 || (keysOk && ImGui::IsKeyPressed(ImGuiKey_Enter, false));
    const bool no = !keep || clicked == 1 || (keysOk && ImGui::IsKeyPressed(ImGuiKey_Escape, false));
    if (yes) {
        opt::Options& o = opt::Get();
        const bool enabled = o.enabled;
        o = opt::Defaults();
        o.enabled = enabled;
        host::ChatSettings chat = host::Settings();
        chat.pageSize = 10;
        chat.mode = 2;
        chat.timestamps = false;
        host::Apply(chat);
        S.confirm = false;
        Toast("Padr\xC3\xB5" "es restaurados", "T e F6 abrem o chat de novo.");
    } else if (no) {
        S.confirm = false;
    }
}

} // namespace

void Toggle() {
    if (S.confirm) {
        S.confirm = false;
        return;
    }
    S.open = !S.open;
    S.menuAge = 0;
    S.selected = 1; // abre sempre na primeira tecla
}

bool IsOpen() {
    return S.open;
}

bool WantsFrame() {
    return S.open || ToastsVisible();
}

void Frame() {
    if (S.open) {
        DrawMenu();
        if (S.confirm) {
            DrawConfirm();
        }
    }
    DrawToasts();
}

} // namespace menu

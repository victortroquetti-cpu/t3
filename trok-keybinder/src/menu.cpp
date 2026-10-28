// Trok KeyBinder -- menu no kit da casa. Cada aba de classe e uma lista das binds (nome, resumo e o interruptor);
// abrir uma linha (ou "Nova bind") troca a lista pelo editor dela, que so grava no "Salvar". As automaticas com a
// mesma condicao e o mesmo tempo ficam em vermelho e nao salvam ate o tempo mudar.

#include "menu.h"

#include "icons.h"
#include "trok_ui.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace menu {
namespace {

using namespace tui;
using namespace kb;

constexpr const char* VERSION = "0.1.0";
constexpr const char* VERSION_TAG = "v0.1";

#define RED "{FF6B6B}"
#define AMBER "{FFC457}"

const char* const TABS[] = {"Padr\xC3\xA3o", "Autom\xC3\xA1tico", "Ao redor", "Na vis\xC3\xA3o", "Macro", "Presets", "Geral"};
constexpr int TAB_COUNT = 7;
constexpr int TAB_MACRO = 4, TAB_PRESETS = 5, TAB_GERAL = 6;

const int DELAYS[] = {0,    500,  1000, 1500, 2000, 2500, 3000,  3500,  4000,  4500,  5000,  6000,  7000,
                      8000, 9000, 10000, 12000, 15000, 20000, 25000, 30000, 45000, 60000};
constexpr int DELAY_COUNT = sizeof(DELAYS) / sizeof(DELAYS[0]);

struct Draft {
    bool open = false;
    int index = -1; // na lista do preset (-1 = nova)
    Bind b;
    Bind original;
    char name[64] = {};
    char text[1024] = {};
    char trigger[192] = {};
    char test[256] = {};
    ImGuiKey key = ImGuiKey_None;
    int mods = 0;
    bool confirmDelete = false;
    bool confirmLeave = false;
};

struct State {
    bool open = false;
    int tab = 0;
    int selected[TAB_COUNT + 1] = {1, 1, 1, 1, 1, 1, 1, 1};
    int rowCount[TAB_COUNT + 1] = {1, 1, 1, 1, 1, 1, 1, 1};
    Shell shell;
    int age = 0;
    Draft d;
    char rename[64] = {};
    std::string renameFor;
    bool confirmPresetDelete = false;
    int conflictCursor = -1; // a ultima bind aberta pelo "Ver" do aviso de conflito
    ImVec2 iconButton, iconButtonTop; // cantos do botao do icone (o seletor abre embaixo dele, ou em cima)
    std::vector<ImVec2> iconCells;
} S;

const Settings DEFAULTS;

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
    for (int d = 1; d <= 9 && !k.digit; ++d) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_0 + d), false) ||
            ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_Keypad0 + d), false)) {
            k.digit = d;
        }
    }
    return k;
}

bool KeyboardFree() {
    return !ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && !CapturingKey();
}

void MoveSelection(int* selected, int count, const Keys& k, Rows& rows) {
    if (count <= 0) {
        return;
    }
    if (k.digit && k.digit <= count) {
        *selected = k.digit; // o numero da linha leva ate ela
        rows.keyboardMoved = true;
    } else if (k.up) {
        *selected = (*selected + count - 2) % count + 1;
        rows.keyboardMoved = true;
    } else if (k.down) {
        *selected = *selected % count + 1;
        rows.keyboardMoved = true;
    }
}

void Copy(char* dst, size_t size, const std::string& src) {
    snprintf(dst, size, "%s", src.c_str());
}

std::string Seconds(int ms) {
    char buf[32];
    if (ms == 0) {
        return "Na hora";
    }
    if (ms % 1000 == 0) {
        snprintf(buf, sizeof(buf), "%d s", ms / 1000);
    } else {
        snprintf(buf, sizeof(buf), "%d,%d s", ms / 1000, (ms % 1000) / 100);
    }
    return buf;
}

std::string KeyText(const Bind& b) {
    if (b.key.Empty()) {
        return b.inMenu ? "S\xC3\xB3 no menu" : "Sem tecla";
    }
    std::string s = HotkeyName(static_cast<ImGuiKey>(b.key.key), b.key.mods);
    if (b.mode == MODE_HOLD) s += " \xC2\xB7 segurar";
    if (b.mode == MODE_DOUBLE) s += " \xC2\xB7 2x";
    if (b.inMenu) s += " \xC2\xB7 menu";
    return s;
}

std::string Summary(const Bind& b) {
    char buf[64];
    switch (b.cls) {
    case CLASS_AUTO:
        return Describe(b);
    case CLASS_AROUND:
        snprintf(buf, sizeof(buf), "%d m \xC2\xB7 %s \xC2\xB7 ", b.radius, b.target == TARGET_ALL ? "todos" : "o mais perto");
        return buf + KeyText(b);
    default:
        return KeyText(b);
    }
}

std::string Title(const Bind& b) {
    if (!b.name.empty()) {
        return b.name;
    }
    std::vector<std::string> lines = Lines(b.text);
    return lines.empty() ? std::string("(sem texto)") : lines[0];
}

// Nome automatico: a primeira linha, ate 28 bytes sem partir um acento.
std::string NameFromText(const std::string& text) {
    std::vector<std::string> lines = Lines(text);
    std::string s = lines.empty() ? std::string("Bind") : lines[0];
    if (s.size() > 28) {
        size_t cut = 28;
        while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) {
            --cut;
        }
        s = s.substr(0, cut) + "...";
    }
    return s;
}

// ---------------------------------------------------------------- editor

void OpenEditor(const Preset& p, int index, int cls) {
    Draft& d = S.d;
    d = Draft();
    d.open = true;
    d.index = index;
    if (index >= 0) {
        d.b = p.binds[index];
    } else {
        d.b.cls = cls;
        d.b.inMenu = cls == CLASS_VIEW;
        if (cls == CLASS_AROUND) {
            d.b.text = "/comando (id)";
        }
    }
    d.original = d.b;
    Copy(d.name, sizeof(d.name), d.b.name);
    Copy(d.text, sizeof(d.text), d.b.text);
    Copy(d.trigger, sizeof(d.trigger), d.b.trigger);
    d.key = static_cast<ImGuiKey>(d.b.key.key);
    d.mods = d.b.key.mods;
    S.selected[TAB_COUNT] = 1;
}

void SyncDraft() {
    Draft& d = S.d;
    d.b.name = d.name;
    d.b.text = d.text;
    d.b.trigger = d.trigger;
    d.b.key.key = d.key;
    d.b.key.mods = d.mods;
}

bool Same(const Bind& a, const Bind& b) {
    return a.cls == b.cls && a.enabled == b.enabled && a.name == b.name && a.text == b.text && a.key == b.key &&
           a.mode == b.mode && a.inMenu == b.inMenu && a.cond == b.cond && a.value == b.value && a.seat == b.seat &&
           a.delay == b.delay && a.trigger == b.trigger && a.cooldown == b.cooldown && a.radius == b.radius &&
           a.target == b.target && a.ignore == b.ignore && a.icon == b.icon;
}

// Tudo que impede uma bind de ser salva ou ligada: o mesmo tempo na mesma condicao (automaticas), o mesmo gesto
// de tecla em outra bind (de qualquer aba) e o atalho do menu circular.
struct Clash {
    std::vector<int> time, key;
    bool radial = false;
    bool Any() const { return !time.empty() || !key.empty() || radial; }
};

Clash ClashesOf(const Preset& p, const Settings& s, const Bind& b, int self) {
    Clash c;
    c.time = Conflicts(p.binds, b, self);
    c.key = KeyConflicts(p.binds, b, self);
    c.radial = RadialConflict(s, b);
    return c;
}

std::string Where(const Bind& b) {
    return "\"" + Title(b) + "\" (" + ClassName(b.cls) + ")";
}

std::string ClashText(const Preset& p, const Clash& c) {
    if (!c.time.empty()) {
        return Where(p.binds[c.time[0]]) + " dispara na mesma condi\xC3\xA7\xC3\xA3o com o mesmo tempo. Mude o tempo "
               "de espera de uma das duas.";
    }
    if (!c.key.empty()) {
        return Where(p.binds[c.key[0]]) + " j\xC3\xA1 usa esta tecla (do mesmo jeito). Troque a tecla ou o modo.";
    }
    return "Esta tecla abre o menu de binds (aba Geral). Escolha outra.";
}

// Tenta gravar o rascunho no preset. false = ficou no editor (o aviso diz por que).
bool SaveDraft(Preset& p, const Settings& set) {
    Draft& d = S.d;
    SyncDraft();
    if (Lines(d.b.text).empty()) {
        Toast("N\xC3\xA3o salvou", "Escreva o texto da bind: um comando ou uma frase por linha.");
        return false;
    }
    if (d.b.cls == CLASS_AUTO && d.b.cond == C_CHAT && Normalize(d.b.trigger).empty()) {
        Toast("N\xC3\xA3o salvou", "Escreva o trecho do chat que dispara a bind.");
        return false;
    }
    Clash clash = ClashesOf(p, set, d.b, d.index);
    if (clash.Any()) {
        Toast(clash.time.empty() ? "N\xC3\xA3o salvou: tecla repetida" : "N\xC3\xA3o salvou: tempo repetido",
              ClashText(p, clash).c_str());
        return false;
    }
    if (d.b.name.empty()) {
        d.b.name = NameFromText(d.b.text);
    }
    if (d.index >= 0 && d.index < static_cast<int>(p.binds.size())) {
        p.binds[d.index] = d.b;
    } else {
        p.binds.push_back(d.b);
    }
    Toast("Bind salva", d.b.name.c_str());
    d.open = false;
    return true;
}

// A grade de icones (Nenhum + os do Lucide), embaixo do botao, alinhada a direita dele.
void IconPopup(Draft& d) {
    int cur = icons::Find(d.b.icon);
    int n = icons::Count() + 1, cols = 8, rowsN = (n + cols - 1) / cols;
    float cell = 34 * u, pad = 8 * u, titleH = 24 * u;
    float w = cols * cell + 2 * pad, h = titleH + rowsN * cell + 2 * pad;
    // Embaixo do botao, alinhada a direita dele; sem espaco embaixo, em cima.
    float y = S.iconButton.y + 4 * u;
    if (y + h > ImGui::GetIO().DisplaySize.y - 8 * u) {
        y = S.iconButtonTop.y - 4 * u - h;
    }
    ImGui::SetNextWindowPos(ImVec2(S.iconButton.x - w, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8 * u);
    if (ImGui::BeginPopup("##icones", ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 o = ImGui::GetCursorScreenPos();
        const char* title = cur >= 0 ? icons::Label(cur) : "Nenhum";
        S.iconCells.assign(n, ImVec2());
        for (int k = 0; k < n; ++k) {
            int idx = k - 1; // a primeira casa e "Nenhum"
            ImVec2 p(o.x + (k % cols) * cell, o.y + titleH + (k / cols) * cell);
            ImGui::SetCursorScreenPos(p);
            ImGui::PushID(k);
            bool click = ImGui::InvisibleButton("##icone", ImVec2(cell, cell));
            bool hov = ImGui::IsItemHovered();
            ImGui::PopID();
            bool sel = idx == cur;
            ImVec2 a(p.x + 2 * u, p.y + 2 * u), b(p.x + cell - 2 * u, p.y + cell - 2 * u);
            if (sel || hov) {
                dl->AddRectFilled(a, b, col::White(sel ? 22 : 12), 6 * u);
            }
            if (sel) {
                dl->AddRect(a, b, col::White(70), 6 * u, 0, 1.0f);
            }
            ImVec2 c(p.x + cell * 0.5f, p.y + cell * 0.5f);
            S.iconCells[k] = c;
            ImU32 ink = hov || sel ? col::text : col::column;
            if (idx < 0) {
                float r = 7 * u; // sem icone: um circulo cortado
                dl->AddCircle(c, r, col::hint, 24, 1.5f * u);
                dl->AddLine(ImVec2(c.x - r * 0.7f, c.y + r * 0.7f), ImVec2(c.x + r * 0.7f, c.y - r * 0.7f), col::hint, 1.5f * u);
            } else {
                icons::Draw(dl, idx, c, 18 * u, ink);
            }
            if (hov) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                title = idx < 0 ? "Nenhum" : icons::Label(idx);
            }
            if (click) {
                d.b.icon = idx < 0 ? std::string() : std::string(icons::Name(idx));
                ImGui::CloseCurrentPopup();
            }
        }
        Text(dl, fonts.desc, o.x + 4 * u, o.y + (titleH - fonts.desc->FontSize) * 0.5f, col::hint, title);
        ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + titleH + rowsN * cell));
        ImGui::Dummy(ImVec2(1, 1));
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
}

void IconRow(Rows& r, Draft& d) {
    int cur = icons::Find(d.b.icon);
    std::string label = cur >= 0 ? icons::Label(cur) : "Nenhum";
    const char* gap = "      "; // espaco para o icone dentro do botao
    std::string button = cur >= 0 ? gap + label : label;
    ImVec2 bmin, bmax;
    if (r.Action("\xC3\x8D" "cone (no c\xC3\xADrculo e na lista)", button.c_str(), &bmin, &bmax)) {
        ImGui::OpenPopup("##icones");
    }
    S.iconButton = bmax;
    S.iconButtonTop = bmin;
    if (cur >= 0) {
        float tw = TextSize(fonts.desc, button.c_str()).x;
        float start = bmin.x + (bmax.x - bmin.x - tw) * 0.5f;
        icons::Draw(ImGui::GetWindowDrawList(), cur, ImVec2(start + TextSize(fonts.desc, gap).x * 0.4f,
                                                           (bmin.y + bmax.y) * 0.5f), 15 * u, col::text);
    }
    IconPopup(d);
}

void KeyRows(Rows& r, Draft& d, const Preset& p, const Settings& set) {
    SyncDraft();
    Clash clash = ClashesOf(p, set, d.b, d.index);
    bool bad = !clash.key.empty() || clash.radial;
    r.Hotkey(bad ? RED "Tecla (repetida)" : "Tecla (ou Ctrl/Shift/Alt + tecla)", &d.key, &d.mods, ImGuiKey_None);
    static const char* const modes[] = {"Pressionar", "Segurar", "2x seguidas"};
    r.Segmented(bad ? RED "Ao apertar" : "Ao apertar", &d.b.mode, modes, 3, MODE_PRESS);
    SyncDraft();
    clash = ClashesOf(p, set, d.b, d.index);
    for (int i : clash.key) {
        std::string label = RED "Mesma ativa\xC3\xA7\xC3\xA3o: " + Title(p.binds[i]);
        std::string where = std::string("aba ") + ClassName(p.binds[i].cls);
        r.Note(label.c_str(), where.c_str());
    }
    if (clash.radial) {
        r.Note(RED "\xC3\x89 o atalho do menu de binds", "aba Geral");
    }
    r.Toggle("Adicionar ao menu de binds", &d.b.inMenu);
    IconRow(r, d);
}

void WeaponRow(Rows& r, Draft& d) {
    static std::vector<int> ids;
    static std::vector<const char*> names;
    if (ids.empty()) {
        ids.push_back(WEAPON_ANY);
        for (int w = 1; w <= 46; ++w) {
            if (WeaponName(w) && !(w >= 11 && w <= 13)) {
                ids.push_back(w);
            }
        }
        for (int id : ids) {
            names.push_back(WeaponName(id));
        }
    }
    int idx = 0;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] == d.b.value) {
            idx = static_cast<int>(i);
        }
    }
    if (r.Dropdown("Arma", &idx, names.data(), static_cast<int>(names.size()), 0)) {
        d.b.value = ids[idx];
    }
}

void AutoRows(Rows& r, Draft& d, const Preset& p, const Context& c) {
    static const char* conds[C_COUNT];
    for (int i = 0; i < C_COUNT; ++i) {
        conds[i] = CondName(i);
    }
    int before = d.b.cond;
    r.Dropdown("Condi\xC3\xA7\xC3\xA3o", &d.b.cond, conds, C_COUNT, C_ENTER_VEHICLE);
    if (d.b.cond != before) {
        if (d.b.cond == C_VEHICLE_HEALTH && d.b.value <= 100) d.b.value = 400;
        if ((d.b.cond == C_HEALTH || d.b.cond == C_ARMOUR) && (d.b.value <= 0 || d.b.value > 100)) d.b.value = 50;
        if (d.b.cond == C_WEAPON && !WeaponName(d.b.value)) d.b.value = WEAPON_ANY;
    }
    switch (d.b.cond) {
    case C_ENTER_VEHICLE: {
        static const char* const seats[] = {"Qualquer", "Motorista", "Passageiro"};
        r.Segmented("Lugar", &d.b.seat, seats, 3, SEAT_ANY);
        break;
    }
    case C_HEALTH:
    case C_ARMOUR:
        d.b.value = std::min(100, std::max(1, d.b.value));
        r.Slider("Abaixo de", &d.b.value, 1, 100, 1, "%d", 50);
        break;
    case C_VEHICLE_HEALTH:
        d.b.value = std::min(1000, std::max(10, d.b.value));
        r.Slider("Abaixo de (o carro pega fogo em 250)", &d.b.value, 10, 1000, 10, "%d", 400);
        break;
    case C_WEAPON:
        WeaponRow(r, d);
        break;
    case C_CHAT: {
        r.Input("Trecho do chat", d.trigger, sizeof(d.trigger), "est\xC3\xA1 precisando de atendimento", false, 330);
        int secs = d.b.cooldown / 1000;
        if (r.Slider("Esperar entre disparos", &secs, 0, 60, 1, "%d s", 5)) {
            d.b.cooldown = secs * 1000;
        }
        r.Info("Como o jogador \xC3\xA9 achado",
               "Cores e acentos n\xC3\xA3o importam. Para o (id) e o (nick), o mod procura na pr\xC3\xB3pria mensagem: quem "
               "falou, o nick de algu\xC3\xA9m conectado (Fulano_Silva ou Fulano Silva) ou um ID como (12), [12], ID 12 "
               "ou #12. Se o texto usa (id)/(nick) e ningu\xC3\xA9m \xC3\xA9 achado, a bind n\xC3\xA3o vai.");
        r.Input("Testar com uma mensagem", d.test, sizeof(d.test), "cole aqui uma mensagem do chat", false, 330);
        if (d.test[0]) {
            std::string result;
            if (!ChatMatches(d.trigger, d.test)) {
                result = "N\xC3\xA3o dispara";
            } else {
                Player who;
                bool found = FindPlayer(d.test, "", c.players, &who);
                std::vector<std::string> lines = Lines(d.text);
                if (UsesTarget(d.text) && !found) {
                    result = AMBER "Dispara, mas sem jogador (n\xC3\xA3o vai)";
                } else if (!lines.empty()) {
                    result = "Dispara: " + (found ? Fill(lines[0], who.id, who.nick) : lines[0]);
                } else {
                    result = "Dispara";
                }
            }
            r.Note("Resultado", result.c_str());
        }
        break;
    }
    default:
        break;
    }
    // O tempo: com outra bind na mesma condicao e no mesmo tempo, o rotulo e a outra ficam vermelhos.
    SyncDraft();
    std::vector<int> clash = Conflicts(p.binds, d.b, d.index);
    int idx = 0;
    std::vector<std::string> labels;
    std::vector<const char*> ptrs;
    for (int i = 0; i < DELAY_COUNT; ++i) {
        labels.push_back(i == 0 ? std::string("Na hora") : Seconds(DELAYS[i]) + " depois");
        if (DELAYS[i] <= d.b.delay) idx = i;
    }
    for (const std::string& l : labels) ptrs.push_back(l.c_str());
    if (r.Dropdown(clash.empty() ? "Esperar" : RED "Esperar (tempo repetido)", &idx, ptrs.data(), DELAY_COUNT, 0)) {
        d.b.delay = DELAYS[idx];
    }
    for (int i : clash) {
        std::string label = RED "Mesmo tempo: " + Title(p.binds[i]);
        r.Note(label.c_str(), Seconds(p.binds[i].delay).c_str());
    }
}

void AroundRows(Rows& r, Draft& d, const Context& c) {
    r.Slider("Raio", &d.b.radius, 1, 50, 1, "%d m", 5);
    static const char* const targets[] = {"O mais perto", "Todos no raio"};
    r.Segmented("Quem", &d.b.target, targets, 2, TARGET_NEAREST);
    r.Section("Ignorar (o mod pula estes jogadores)");
    std::vector<Player> near;
    for (const Player& p : c.players) {
        if (p.distance > 0 && p.distance <= 60.0f) {
            near.push_back(p);
        }
    }
    std::sort(near.begin(), near.end(), [](const Player& a, const Player& b) { return a.distance < b.distance; });
    if (near.size() > 20) {
        near.resize(20);
    }
    char line[160];
    for (const Player& p : near) {
        bool on = Ignored(d.b, p.nick);
        snprintf(line, sizeof(line), "%s (%d) \xC2\xB7 %.0f m", p.nick.c_str(), p.id, p.distance);
        if (r.CheckItem(line, &on)) {
            if (on) {
                d.b.ignore.push_back(p.nick);
            } else {
                d.b.ignore.erase(std::remove_if(d.b.ignore.begin(), d.b.ignore.end(),
                                                [&](const std::string& n) { return SameNick(n, p.nick); }),
                                 d.b.ignore.end());
            }
        }
    }
    // Ignorados que nao estao por perto agora.
    for (size_t i = 0; i < d.b.ignore.size(); ++i) {
        bool shown = false;
        for (const Player& p : near) {
            shown = shown || SameNick(p.nick, d.b.ignore[i]);
        }
        if (shown) {
            continue;
        }
        bool on = true;
        snprintf(line, sizeof(line), "%s \xC2\xB7 longe", d.b.ignore[i].c_str());
        if (r.CheckItem(line, &on) && !on) {
            d.b.ignore.erase(d.b.ignore.begin() + i);
            break;
        }
    }
    if (near.empty() && d.b.ignore.empty()) {
        r.Note("Ningu\xC3\xA9m por perto agora", "chegue perto para escolher");
    }
}

// O editor de uma bind. true = o preset mudou.
bool Editor(Rows& r, Preset& p, const Context& c) {
    Draft& d = S.d;
    bool changed = false;
    r.Toggle("Ligada", &d.b.enabled);
    r.Input("Nome (aparece no menu de binds)", d.name, sizeof(d.name), "ex.: Trancar o carro", false, 330);
    const char* hint = d.b.cls == CLASS_AROUND || d.b.cls == CLASS_VIEW
                           ? "/comando (id) ou (nick) -- uma mensagem por linha"
                           : "/comando ou frase -- uma mensagem por linha";
    r.Multiline("Texto", d.text, sizeof(d.text), hint, 4, 380);
    std::vector<std::string> lines = Lines(d.text);
    for (size_t i = 0; i < lines.size(); ++i) {
        int n = Codepoints(lines[i]);
        if (n > 128) {
            char label[96], value[32];
            snprintf(label, sizeof(label), AMBER "Linha %d passa de 128 letras", static_cast<int>(i + 1));
            snprintf(value, sizeof(value), "%d (o SA-MP corta)", n);
            r.Note(label, value);
        }
    }
    if ((d.b.cls == CLASS_AROUND || d.b.cls == CLASS_VIEW) && !lines.empty() && !UsesTarget(d.text)) {
        r.Note(AMBER "Sem (id) nem (nick) no texto", "vai igual para todos");
    }
    switch (d.b.cls) {
    case CLASS_KEY:
        KeyRows(r, d, p, c.data->settings);
        break;
    case CLASS_AUTO:
        AutoRows(r, d, p, c);
        break;
    case CLASS_AROUND:
        KeyRows(r, d, p, c.data->settings);
        AroundRows(r, d, c);
        break;
    case CLASS_VIEW:
        KeyRows(r, d, p, c.data->settings);
        r.Info("Como usar",
               "Olhe para o jogador: a bolinha aparece no meio dele. Abra o menu de binds (atalho na aba Geral) e clique "
               "na bind -- ou aperte a tecla dela, se tiver. O (id) e o (nick) viram os dele.");
        break;
    default:
        break;
    }
    SyncDraft();
    if (d.b.cls != CLASS_AUTO) {
        if (d.key == ImGuiKey_None && !d.b.inMenu) {
            r.Note(AMBER "Sem tecla e fora do menu", "n\xC3\xA3o tem como usar");
        }
    }
    r.Section("");
    if (r.Action("Salvar esta bind", "Salvar")) {
        changed = SaveDraft(p, c.data->settings);
    }
    if (d.index >= 0 && r.Action(d.confirmDelete ? RED "Apagar mesmo?" : "Apagar esta bind",
                                 d.confirmDelete ? "Confirmar" : "Apagar")) {
        if (d.confirmDelete) {
            p.binds.erase(p.binds.begin() + d.index);
            d.open = false;
            changed = true;
            Toast("Bind apagada", Title(d.original).c_str());
        } else {
            d.confirmDelete = true;
        }
    }
    return changed;
}

// ---------------------------------------------------------------- abas

bool ClassList(Rows& r, Preset& p, const Settings& set, int cls) {
    bool changed = false;
    if (r.Action(("Preset: " + p.name).c_str(), "Trocar")) {
        S.tab = TAB_PRESETS;
    }
    // Conflitos em qualquer aba do preset: o aviso aparece em todas.
    // "Ver" abre a proxima bind em conflito (na aba dela), uma de cada vez.
    bool tabs[CLASS_COUNT] = {};
    std::vector<int> clashing;
    for (size_t i = 0; i < p.binds.size(); ++i) {
        if (ClashesOf(p, set, p.binds[i], static_cast<int>(i)).Any()) {
            tabs[p.binds[i].cls] = true;
            clashing.push_back(static_cast<int>(i));
        }
    }
    if (!clashing.empty()) {
        std::string where;
        int tabCount = 0;
        for (int t = 0; t < CLASS_COUNT; ++t) {
            if (tabs[t]) {
                where += (where.empty() ? "" : ", ") + std::string(ClassName(t));
                ++tabCount;
            }
        }
        if (tabCount > 2) {
            where = std::to_string(tabCount) + " abas";
        }
        char label[160];
        snprintf(label, sizeof(label), RED "%d binds em conflito (%s)", static_cast<int>(clashing.size()), where.c_str());
        if (r.Action(label, "Ver")) {
            int next = clashing[0];
            for (int i : clashing) {
                if (i > S.conflictCursor) {
                    next = i;
                    break;
                }
            }
            S.conflictCursor = next;
            S.tab = p.binds[next].cls;
            OpenEditor(p, next, p.binds[next].cls);
        }
    }
    static const char* const about[] = {
        "Uma tecla (ou Ctrl/Shift/Alt + tecla) manda um comando ou uma frase. Pressionar, segurar ou 2x seguidas. "
        "Marque \"Adicionar ao menu de binds\" para ela aparecer no c\xC3\xADrculo.",
        "Dispara sozinha quando a condi\xC3\xA7\xC3\xA3o acontece (entrar no carro, vida baixa, mensagem no chat...). "
        "Binds na mesma condi\xC3\xA7\xC3\xA3o precisam de tempos diferentes: ex. /tranca na hora e /cinto 1 s depois.",
        "Usa o jogador perto de voc\xC3\xAA (at\xC3\xA9 o raio): o (id) e o (nick) viram os dele. O mais perto ou "
        "todos no raio, pulando os ignorados.",
        "Usa o jogador na sua mira (o centro da tela). A bolinha mostra quem \xC3\xA9; o menu de binds mostra estas "
        "binds com o nome dele no meio.",
    };
    r.Info("Como funciona", about[cls]);
    int shown = 0;
    for (size_t i = 0; i < p.binds.size(); ++i) {
        Bind& b = p.binds[i];
        if (b.cls != cls) {
            continue;
        }
        ++shown;
        bool clash = ClashesOf(p, set, b, static_cast<int>(i)).Any();
        bool was = b.enabled;
        bool open = r.Entry(Title(b).c_str(), Summary(b).c_str(), &b.enabled, clash ? IM_COL32(255, 107, 107, 255) : 0);
        if (b.enabled != was) {
            Clash now = ClashesOf(p, set, b, static_cast<int>(i));
            if (b.enabled && now.Any()) {
                b.enabled = false;
                Toast("N\xC3\xA3o ligou: conflito", ClashText(p, now).c_str());
            } else {
                changed = true;
            }
        }
        if (open) {
            OpenEditor(p, static_cast<int>(i), cls);
        }
    }
    if (!shown) {
        r.Note("Nenhuma bind aqui ainda", "");
    }
    if (r.Action("Nova bind", "Adicionar")) {
        OpenEditor(p, -1, cls);
    }
    return changed;
}

void TabMacro(Rows& r) {
    r.Note("Macro", "em breve");
    r.Info("O que vai fazer",
           "Gravar o que voc\xC3\xAA faz no chat e nos menus: abrir o chat, escrever, escolher um item de um menu pelo "
           "ID do item (n\xC3\xA3o pela posi\xC3\xA7\xC3\xA3o: o mod procura o item no menu certo). Parar de gravar "
           "salva o macro.");
    r.Info("O que nunca vai gravar",
           "Nada de jogabilidade: andar, dirigir ou ir at\xC3\xA9 um lugar. Macro de jogabilidade \xC3\xA9 proibido nos "
           "servidores.");
}

bool TabPresets(Rows& r, Context& c) {
    Data& data = *c.data;
    Preset& cur = data.presets[c.preset];
    bool changed = false;
    r.Section(c.connected ? "Este servidor" : "Fora de um servidor");
    if (c.connected) {
        r.Note("Servidor", c.hostname.empty() ? c.address.c_str() : AutoName(c.hostname).c_str());
        r.Note("Endere\xC3\xA7o", c.address.c_str());
    }
    std::vector<const char*> names;
    for (const Preset& p : data.presets) {
        names.push_back(p.name.c_str());
    }
    int pick = c.preset;
    if (r.Dropdown(c.connected ? "Preset deste servidor" : "Preset para editar", &pick, names.data(),
                   static_cast<int>(names.size()), c.preset) &&
        pick != c.preset) {
        std::string name = data.presets[pick].name;
        if (c.connected) {
            UsePreset(data, c.address, pick);
        }
        data.lastPreset = name;
        Toast("Preset trocado", name.c_str());
        return true; // os indices podem ter mudado: o resto no proximo quadro
    }
    if (S.renameFor != cur.name) {
        S.renameFor = cur.name;
        Copy(S.rename, sizeof(S.rename), cur.name);
        S.confirmPresetDelete = false;
    }
    r.Input("Nome do preset", S.rename, sizeof(S.rename), "ex.: hzrp policia", false, 300);
    if (cur.name != S.rename) {
        if (r.Action(("Renomear para \"" + std::string(S.rename) + "\"").c_str(), "Renomear")) {
            if (RenamePreset(data, c.preset, S.rename)) {
                S.renameFor = data.presets[c.preset].name;
                Toast("Preset renomeado", S.renameFor.c_str());
                return true;
            }
            Toast("N\xC3\xA3o renomeou", "O nome est\xC3\xA1 vazio ou j\xC3\xA1 \xC3\xA9 de outro preset.");
        }
    }
    std::vector<std::string> servers = ServersOf(data, cur.name);
    std::string list;
    for (size_t i = 0; i < servers.size(); ++i) {
        list += (i ? ", " : "") + servers[i];
    }
    r.Note("Servidores com este preset", servers.empty() ? "nenhum" : list.c_str());
    char count[32];
    snprintf(count, sizeof(count), "%d", static_cast<int>(cur.binds.size()));
    r.Note("Binds", count);
    r.Section("Mais presets");
    if (r.Action("Novo preset vazio", "Criar")) {
        int n = NewPreset(data, "Novo preset", false, -1);
        std::string name = data.presets[n].name;
        if (c.connected) UsePreset(data, c.address, n);
        data.lastPreset = name;
        Toast("Preset criado", name.c_str());
        return true;
    }
    if (r.Action("Copiar este preset", "Duplicar")) {
        int n = NewPreset(data, cur.name + " (c\xC3\xB3pia)", true, c.preset);
        std::string name = data.presets[n].name;
        if (c.connected) UsePreset(data, c.address, n);
        data.lastPreset = name;
        Toast("Preset copiado", name.c_str());
        return true;
    }
    if (r.Action(S.confirmPresetDelete ? RED "Apagar mesmo? As binds dele somem" : "Apagar este preset",
                 S.confirmPresetDelete ? "Confirmar" : "Apagar")) {
        if (S.confirmPresetDelete) {
            std::string name = cur.name;
            DeletePreset(data, c.preset);
            S.confirmPresetDelete = false;
            Toast("Preset apagado", name.c_str());
            return true;
        }
        S.confirmPresetDelete = true;
    }
    if (data.presets.size() > 1) {
        r.Section("Todos os presets");
        for (const Preset& p : data.presets) {
            std::vector<std::string> sv = ServersOf(data, p.name);
            char value[200];
            snprintf(value, sizeof(value), "%d %s \xC2\xB7 %s", static_cast<int>(p.binds.size()),
                     p.binds.size() == 1 ? "bind" : "binds",
                     sv.empty() ? "sem servidor" : sv.size() == 1 ? sv[0].c_str() : (std::to_string(sv.size()) + " servidores").c_str());
            r.Note(p.name.c_str(), value);
        }
    }
    r.Info("Compartilhar",
           "Cada servidor ganha um preset com o nome dele na primeira entrada. Para usar o mesmo preset em v\xC3\xA1rios "
           "(Horizonte 1, 2, 3 e 4) ou trocar de papel (ladr\xC3\xA3o, pol\xC3\xAD" "cia), escolha o preset aqui dentro "
           "de cada servidor.");
    return changed;
}

bool TabGeral(Rows& r, Data& data) {
    Settings& s = data.settings;
    bool changed = false;
    changed |= r.Toggle("Trok KeyBinder ligado", &s.enabled);
    ImGuiKey key = static_cast<ImGuiKey>(s.radial.key);
    Hotkey before = s.radial;
    if (r.Hotkey("Atalho do menu de binds (c\xC3\xADrculo)", &key, &s.radial.mods, ImGuiKey_None)) {
        s.radial.key = key;
        changed = true;
        for (const Preset& p : data.presets) {
            std::vector<int> taken = RadialConflicts(p.binds, s.radial);
            if (!taken.empty()) {
                std::string msg = Where(p.binds[taken[0]]) + " no preset \"" + p.name + "\" j\xC3\xA1 usa esta tecla.";
                Toast("Atalho n\xC3\xA3o trocado: tecla repetida", msg.c_str());
                s.radial = before;
                changed = false;
                break;
            }
        }
    }
    changed |= r.Slider("Tamanho do c\xC3\xADrculo", &s.radialSize, 70, 140, 5, "%d%%", DEFAULTS.radialSize);
    changed |= r.Slider("Intervalo entre mensagens", &s.flood, 500, 5000, 100, "%d ms", DEFAULTS.flood);
    changed |= r.Slider("Segurar por", &s.hold, 500, 5000, 100, "%d ms", DEFAULTS.hold);
    changed |= r.Slider("2x seguidas: at\xC3\xA9", &s.doubleTap, 150, 1000, 25, "%d ms", DEFAULTS.doubleTap);
    changed |= r.Slider("Na vis\xC3\xA3o: dist\xC3\xA2ncia", &s.viewDistance, 3, 100, 1, "%d m", DEFAULTS.viewDistance);
    changed |= r.Toggle("Bolinha no jogador da mira", &s.viewDot);
    static const char* const opens[] = {"Lista no jogador", "C\xC3\xADrculo"};
    int open = s.viewList ? 0 : 1;
    if (r.Segmented("Com algu\xC3\xA9m na mira, o atalho abre", &open, opens, 2, 0)) {
        s.viewList = open == 0;
        changed = true;
    }
    r.Section("Nunca dispara com");
    r.Note("Chat, di\xC3\xA1logo, placar (Tab) e pausa", "bloqueado");
    r.Note("Menus com cursor (este e os de outros mods)", "bloqueado");
    r.Note("Arquivo", "Trok KeyBinder.txt (UTF-8)");
    return changed;
}

} // namespace

const char* Version() {
    return VERSION;
}

void Toggle() {
    S.open = !S.open;
    S.age = 0;
}

bool IsOpen() {
    return S.open;
}

std::string EditingName() {
    return S.open && S.d.open ? S.d.b.name : std::string();
}

ImVec2 IconCell(int k) {
    return k >= 0 && k < static_cast<int>(S.iconCells.size()) ? S.iconCells[k] : ImVec2(-1, -1);
}

bool Frame(Context& c) {
    if (!S.open || !c.data || c.preset < 0 || c.preset >= static_cast<int>(c.data->presets.size())) {
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    bool fresh = S.age++ == 0;
    bool keysOk = !fresh && KeyboardFree();
    Keys keys = ReadKeys(keysOk);
    bool escape = !fresh && !CapturingKey() && !io.WantTextInput &&
                  !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    bool save = !fresh && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false);
    Preset& preset = c.data->presets[c.preset];
    bool changed = false;
    bool editing = S.d.open && S.tab < TAB_MACRO;
    if (S.d.open && S.tab >= TAB_MACRO) {
        S.d.open = false; // trocou de aba no meio da edicao
    }

    float width = 760 * u;
    float height = std::min(io.DisplaySize.y * 0.8f, 620 * u);
    PushStyle();
    bool keep = BeginShell("##trokKeyBinder", "Trok KeyBinder", VERSION_TAG, width, height, S.shell);

    ImVec2 pos = ImGui::GetWindowPos();
    float padX = 18 * u;
    int tabBefore = S.tab;
    Tabs("##abas", TABS, TAB_COUNT, &S.tab, pos.x + padX, pos.y + HeaderHeight(), width - 2 * padX);
    if (S.tab != tabBefore) {
        S.d.open = false;
        editing = false;
    }

    float listTop = pos.y + HeaderHeight() + TabsHeight() + 10 * u;
    float listH = height - (listTop - pos.y) - FooterHeight() - 8 * u;
    ImGui::SetCursorScreenPos(ImVec2(pos.x + padX - 4 * u, listTop));
    ImGui::BeginChild(editing ? "##editor" : "##lista", ImVec2(width - 2 * padX + 8 * u, listH), false,
                      ImGuiWindowFlags_NoBackground);
    ImVec2 cp = ImGui::GetCursorScreenPos();
    float rowW = ImGui::GetContentRegionAvail().x - 10 * u;

    Rows rows;
    int slot = editing ? TAB_COUNT : S.tab;
    int* sel = &S.selected[slot];
    MoveSelection(sel, S.rowCount[slot], keys, rows);
    rows.Begin(editing ? "editor" : TABS[S.tab], cp.x + 4 * u, rowW, sel, keys, true);
    bool structural = false;
    if (editing) {
        changed |= Editor(rows, preset, c);
    } else {
        switch (S.tab) {
        case 0:
        case 1:
        case 2:
        case 3: changed |= ClassList(rows, preset, c.data->settings, S.tab); break;
        case TAB_MACRO: TabMacro(rows); break;
        case TAB_PRESETS: structural = TabPresets(rows, c); break;
        default: changed |= TabGeral(rows, *c.data); break;
        }
    }
    S.rowCount[slot] = rows.End();
    ImGui::Dummy(ImVec2(0, 6 * u));
    ImGui::EndChild();

    static const std::vector<Hint> listHints = {{"Setas", "Navegar", false}, {"Enter", "Abrir", false}, {"Esc", "Fechar", true}};
    static const std::vector<Hint> editHints = {{"Ctrl+S", "Salvar", true}, {"Esc", "Voltar", true}};
    int clicked = EndShell(editing ? editHints : listHints);
    PopStyle();

    if (editing && !structural) {
        if (save || clicked == 0) {
            changed |= SaveDraft(preset, c.data->settings);
        } else if (escape || clicked == 1) {
            SyncDraft();
            if (!Same(S.d.b, S.d.original) && !S.d.confirmLeave) {
                S.d.confirmLeave = true;
                Toast("Altera\xC3\xA7\xC3\xB5" "es n\xC3\xA3o salvas", "Esc de novo para sair sem salvar (Ctrl+S salva).");
            } else {
                S.d.open = false;
            }
        }
    } else if (!keep || (!editing && (clicked == 2 || escape))) {
        S.open = false;
    }
    if (!keep) {
        S.open = false;
    }
    return changed || structural;
}

} // namespace menu

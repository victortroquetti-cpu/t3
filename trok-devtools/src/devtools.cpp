// Trok DevTools (.asi) -- Victor_Trok
// A grade e virtual: so os cards da tela sao desenhados e so eles pedem miniatura. O card aberto e um popup modal da
// casa, com o visualizador a esquerda (arraste para girar, roda para zoom, duplo clique para centralizar) e as
// informacoes e texturas a direita.

#include "devtools.h"

#include "audio_data.h"
#include "audiospy.h"
#include "catalog.h"
#include "core.h"
#include "ifp.h"
#include "pose.h"
#include "player.h"
#include "render.h"
#include "sfx.h"
#include "trok_ui.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

void DevToolsLog(const char* fmt, ...);

namespace devtools {
namespace {

using namespace tui;

constexpr const char* VERSION_TAG = "v0.4";
// As texturas ficam no card do modelo (clique numa textura): nao tem aba propria.
const char* const TABS[] = {"Modelos", "Anima\xC3\xA7\xC3\xB5" "es", "\xC3\x81udios", "Arquivos", "Geral"};
constexpr int TAB_COUNT = 5, TAB_MODELS = 0, TAB_GENERAL = 4;

// Fundo padrao do visualizador (o seletor de cor da casa troca, no card e no Geral).
constexpr ImU32 BACKGROUND_DEFAULT = IM_COL32(20, 20, 20, 255);

struct State {
    bool open = false;
    int tab = TAB_MODELS;
    int age = 0;
    Shell shell;
    // Busca e filtros
    char query[96] = "";
    int kind = -1, archive = -1;
    bool onlyReplaced = false;
    std::vector<int> filtered;
    bool filterDirty = true;
    int builtFrom = -1; // tamanho do catalogo quando filtrou
    // Card
    bool cardRequest = false, cardOpen = false;
    int cardId = -1;
    render::View view;
    bool spin = true;
    std::string texName;              // textura em destaque no visualizador (vazio = o modelo)
    ImU32 background = BACKGROUND_DEFAULT; // fundo do visualizador
    // Geral
    int selected = 1, rowCount = 1;
    bool inspect = false;
    int perFrame = 3;
    // Audios
    char audioQuery[96] = "";
    sfx::Filter audioFilter, audioApplied;
    sfx::Result audioResult;
    bool audioHasResult = false;
    std::vector<bool> audioOpenPack, audioOpenBank;
    int audioSelBank = -1, audioSelSound = -1;
    bool audioReveal = false;
    bool audioShow = false, audioIgnoreFeet = true, audioRepeat = false;
    int audioDelay = 4;
    int audioRowSel = 1;
    // Animacoes
    char animQuery[96] = "";
    ifp::Filter animFilter, animApplied;
    ifp::Result animResult;
    bool animHasResult = false;
    std::vector<bool> animOpen;
    int animSel = -1;
    bool animReveal = false;
    ifp::Clip clip;
    bool clipOk = false;
    float animTime = 0;
    bool animPlaying = true, animScrub = false;
    bool animLoop = true, animInPlace = true, animSpin = false;
    int animSpeed = 2, animSkin = -1; // velocidade: indice em ANIM_SPEEDS; personagem: -1 = o do jogador
    render::View animView;
    mesh::Mesh posed; // a malha do personagem na pose do instante
    char ini[MAX_PATH] = "";
    char gameDir[MAX_PATH] = "";
} S;

// ---------------------------------------------------------------- utilidades

std::string Thousands(int n) {
    char raw[32];
    snprintf(raw, sizeof(raw), "%d", n);
    std::string s = raw, out;
    int count = 0;
    for (int i = static_cast<int>(s.size()) - 1; i >= 0; --i) {
        out.insert(out.begin(), s[i]);
        if (++count % 3 == 0 && i > 0 && s[i - 1] != '-') {
            out.insert(out.begin(), '.');
        }
    }
    return out;
}

std::string Ellipsize(ImFont* font, const std::string& text, float maxW) {
    if (TextSize(font, text.c_str()).x <= maxW) {
        return text;
    }
    std::string s = text;
    while (!s.empty() && TextSize(font, (s + "\xE2\x80\xA6").c_str()).x > maxW) {
        s.pop_back();
    }
    return s + "\xE2\x80\xA6";
}

std::string Decimal(float v, int digits) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", digits, v);
    for (char& c : buf) {
        if (c == '.') {
            c = ',';
        }
    }
    return buf;
}

void Box(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 fill, ImU32 border, float rounding) {
    if (fill) {
        dl->AddRectFilled(a, b, fill, rounding);
    }
    if (border) {
        dl->AddRect(ImVec2(a.x + 0.5f, a.y + 0.5f), ImVec2(b.x - 0.5f, b.y - 0.5f), border, rounding, 0, 1.0f);
    }
}

void Hand() {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}

// Pilula da casa: escolhida = clara com texto escuro. Devolve true no clique; *width = largura usada.
bool Pill(const char* id, const char* label, bool selected, float x, float y, float h, float* width) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 ts = TextSize(fonts.desc, label);
    float w = ts.x + 24 * u;
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    bool clicked = ImGui::InvisibleButton(id, ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    if (hovered) {
        Hand();
    }
    ImU32 fill = selected ? col::strong : col::White(hovered ? 12 : 6);
    ImU32 border = selected ? 0 : col::White(hovered ? 30 : 16);
    Box(dl, ImVec2(x, y), ImVec2(x + w, y + h), fill, border, h * 0.5f);
    Text(dl, fonts.desc, x + 12 * u, y + (h - ts.y) * 0.5f - 0.5f * u, selected ? col::ink : (hovered ? col::text : col::column),
         label);
    if (width) {
        *width = w;
    }
    return clicked;
}

bool Button(const char* id, const char* label, float x, float y, float w, float h, bool primary, bool enabled = true) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    bool clicked = ImGui::InvisibleButton(id, ImVec2(w, h)) && enabled;
    bool hovered = enabled && ImGui::IsItemHovered();
    if (hovered) {
        Hand();
    }
    ImU32 fill, border, color;
    if (!enabled) {
        fill = col::White(4);
        border = col::White(10);
        color = col::disabled;
    } else if (primary) {
        fill = hovered ? col::Rgba(245, 245, 245) : col::strong;
        border = 0;
        color = col::ink;
    } else {
        fill = col::White(hovered ? 12 : 6);
        border = col::White(hovered ? 34 : 18);
        color = hovered ? col::text : col::column;
    }
    Box(dl, ImVec2(x, y), ImVec2(x + w, y + h), fill, border, 7 * u);
    ImVec2 ts = TextSize(fonts.desc, label);
    Text(dl, fonts.desc, x + (w - ts.x) * 0.5f, y + (h - ts.y) * 0.5f - 0.5f * u, color, label);
    return clicked;
}

void Checker(ImDrawList* dl, ImVec2 a, ImVec2 b, float cell) {
    dl->PushClipRect(a, b, true);
    dl->AddRectFilled(a, b, col::Rgba(46, 46, 46));
    int cols = static_cast<int>((b.x - a.x) / cell) + 1, rows = static_cast<int>((b.y - a.y) / cell) + 1;
    for (int r = 0; r < rows; ++r) {
        for (int c = r % 2; c < cols; c += 2) {
            ImVec2 p(a.x + c * cell, a.y + r * cell);
            dl->AddRectFilled(p, ImVec2(p.x + cell, p.y + cell), col::Rgba(30, 30, 30));
        }
    }
    dl->PopClipRect();
}

struct IniFlag {
    const char* key;
    bool* value;
};

std::vector<IniFlag> IniFlags() {
    return {{"inspecionar", &S.inspect},          {"visual_texturas", &S.view.textures},
            {"visual_arame", &S.view.wire},       {"visual_colisao", &S.view.collision},
            {"visual_grade", &S.view.grid},       {"visual_girar", &S.spin},
            {"audio_mostrar", &S.audioShow},      {"audio_ignorar_passos", &S.audioIgnoreFeet},
            {"audio_repetir", &S.audioRepeat},    {"anim_repetir", &S.animLoop},
            {"anim_no_lugar", &S.animInPlace},    {"anim_girar", &S.animSpin}};
}

void SaveIni() {
    if (!S.ini[0]) {
        return;
    }
    char buf[16];
    for (const IniFlag& f : IniFlags()) {
        WritePrivateProfileStringA("devtools", f.key, *f.value ? "1" : "0", S.ini);
    }
    snprintf(buf, sizeof(buf), "%d", S.perFrame);
    WritePrivateProfileStringA("devtools", "miniaturas_por_quadro", buf, S.ini);
    snprintf(buf, sizeof(buf), "%d", S.audioDelay);
    WritePrivateProfileStringA("devtools", "audio_tempo", buf, S.ini);
    snprintf(buf, sizeof(buf), "%d", S.animSkin);
    WritePrivateProfileStringA("devtools", "anim_personagem", buf, S.ini);
    snprintf(buf, sizeof(buf), "%d", S.animSpeed);
    WritePrivateProfileStringA("devtools", "anim_velocidade", buf, S.ini);
    snprintf(buf, sizeof(buf), "%02X%02X%02X", static_cast<unsigned>(S.background & 0xFF),
             static_cast<unsigned>((S.background >> 8) & 0xFF), static_cast<unsigned>((S.background >> 16) & 0xFF));
    WritePrivateProfileStringA("devtools", "visual_fundo", buf, S.ini);
}

// ImU32 do ImGui (ABGR) para a cor do Direct3D (ARGB).
DWORD D3dColor(ImU32 c) {
    return 0xFF000000 | ((c & 0xFF) << 16) | (c & 0xFF00) | ((c >> 16) & 0xFF);
}

void SetBackground(ImU32 c) {
    S.background = c | IM_COL32_A_MASK;
    S.view.background = D3dColor(S.background);
}

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

void OpenCard(int id) {
    S.cardId = id;
    S.cardRequest = true;
    render::View fresh; // o angulo e o zoom voltam ao inicio; as opcoes ficam como estavam
    S.view.yaw = fresh.yaw;
    S.view.pitch = fresh.pitch;
    S.view.zoom = fresh.zoom;
    S.texName.clear();
    core::Open(id);
}

// Trocar o modelo do card pela lista da textura: a textura continua em destaque.
void SelectTextureUser(int id) {
    S.cardId = id;
    core::Open(id);
}

int TexIndex(const mesh::Mesh* m) {
    if (!m || S.texName.empty()) {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(m->textures.size()); ++i) {
        if (m->textures[i].name == S.texName) {
            return i;
        }
    }
    return -1;
}

// ---------------------------------------------------------------- aba Modelos

void Refilter() {
    S.filtered = catalog::Filter(S.query, S.kind, S.archive, S.onlyReplaced);
    S.filterDirty = false;
    S.builtFrom = static_cast<int>(catalog::All().size());
}

void ArchivePopup(float x, float y) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6 * u, 6 * u));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8 * u);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(18, 18, 18, 250));
    if (ImGui::BeginPopup("##arquivos")) {
        const std::vector<std::string>& names = catalog::Archives();
        float w = 200 * u;
        for (const std::string& n : names) {
            w = std::max(w, TextSize(fonts.desc, n.c_str()).x + 40 * u);
        }
        float h = 28 * u;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (int i = -1; i < static_cast<int>(names.size()); ++i) {
            const char* label = i < 0 ? "Todos os .img" : names[i].c_str();
            ImVec2 p = ImGui::GetCursorScreenPos();
            char id[16];
            snprintf(id, sizeof(id), "##arq%d", i);
            if (ImGui::InvisibleButton(id, ImVec2(w, h))) {
                S.archive = i;
                S.filterDirty = true;
                ImGui::CloseCurrentPopup();
            }
            bool hovered = ImGui::IsItemHovered();
            if (hovered) {
                Hand();
                dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col::White(10), 6 * u);
            }
            ImVec2 ts = TextSize(fonts.desc, label);
            Text(dl, fonts.desc, p.x + 12 * u, p.y + (h - ts.y) * 0.5f, S.archive == i ? col::text : col::column, label);
            if (S.archive == i) {
                DrawIcon(dl, Icon::Check, ImVec2(p.x + w - 14 * u, p.y + h * 0.5f), col::text, true);
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void Card(const catalog::Entry& e, float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    char id[24];
    snprintf(id, sizeof(id), "##card%d", e.id);
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    if (ImGui::InvisibleButton(id, ImVec2(w, h))) {
        OpenCard(e.id);
    }
    bool hovered = ImGui::IsItemHovered();
    if (hovered) {
        Hand();
    }
    Box(dl, ImVec2(x, y), ImVec2(x + w, y + h), col::White(hovered ? 10 : 5), col::White(hovered ? 34 : 10), 9 * u);
    float pad = 6 * u;
    ImVec2 t0(x + pad, y + pad), t1(x + w - pad, y + pad + (w - 2 * pad));
    ImTextureID tex = nullptr;
    ImVec2 uv0, uv1;
    if (render::Thumb(e.id, &tex, &uv0, &uv1)) {
        dl->AddImageRounded(tex, t0, t1, uv0, uv1, IM_COL32_WHITE, 6 * u);
    } else {
        dl->AddRectFilled(t0, t1, col::Rgba(20, 20, 20), 6 * u);
        ImVec2 c((t0.x + t1.x) * 0.5f, (t0.y + t1.y) * 0.5f);
        if (core::ThumbFailed(e.id)) {
            const char* label = e.noFile ? "sem arquivo" : "sem malha";
            ImVec2 ts = TextSize(fonts.desc, label);
            Text(dl, fonts.desc, c.x - ts.x * 0.5f, c.y - ts.y * 0.5f, col::disabled, label);
        } else {
            core::WantThumb(e.id);
            Spinner(dl, c, 9 * u, static_cast<float>(ImGui::GetTime()));
        }
    }
    if (e.replaced) {
        const char* badge = "MOD";
        ImVec2 ts = TextSize(fonts.desc, badge);
        ImVec2 b0(t1.x - ts.x - 14 * u, t0.y + 6 * u);
        dl->AddRectFilled(b0, ImVec2(t1.x - 6 * u, b0.y + ts.y + 6 * u), col::strong, 5 * u);
        Text(dl, fonts.desc, b0.x + 4 * u, b0.y + 3 * u - 0.5f * u, col::ink, badge);
    }
    char idText[16];
    snprintf(idText, sizeof(idText), "%d", e.id);
    float textY = t1.y + 7 * u;
    Text(dl, fonts.body, x + pad + 2 * u, textY, col::text, idText);
    std::string name = Ellipsize(fonts.desc, e.dff.empty() ? std::string("\xE2\x80\x94") : e.dff, w - 2 * pad - 4 * u);
    Text(dl, fonts.desc, x + pad + 2 * u, textY + fonts.body->FontSize + 2 * u, col::hint, name.c_str());
    if (hovered) {
        char tip[256];
        snprintf(tip, sizeof(tip), "%d  %s.dff  %s.txd\n%s  %s", e.id, e.dff.c_str(), e.txd.c_str(),
                 catalog::TypeName(e), e.imgName.c_str());
        Tooltip(tip);
    }
}

void TabModels(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (!catalog::Ready()) {
        ImVec2 c(x + w * 0.5f, y + h * 0.45f);
        Spinner(dl, c, 12 * u, static_cast<float>(ImGui::GetTime()));
        char text[96];
        snprintf(text, sizeof(text), "Montando o cat\xC3\xA1logo: %s", catalog::Status());
        ImVec2 ts = TextSize(fonts.desc, text);
        Text(dl, fonts.desc, c.x - ts.x * 0.5f, c.y + 22 * u, col::hint, text);
        core::SetBrowsing(false);
        return;
    }
    if (S.builtFrom != static_cast<int>(catalog::All().size())) {
        S.filterDirty = true;
    }

    // Barra: busca, tipos, arquivo, so substituidos e o inspetor.
    float barH = 32 * u;
    float searchW = std::min(360 * u, w * 0.32f);
    if (SearchField("##busca", S.query, sizeof(S.query), "Buscar por id, DFF, TXD ou .img", x, y, searchW, barH)) {
        S.filterDirty = true;
    }
    float px = x + searchW + 14 * u, pw = 0;
    float pillH = 28 * u, pillY = y + (barH - pillH) * 0.5f;
    for (int k = -1; k < catalog::KIND_COUNT; ++k) {
        char id[16];
        snprintf(id, sizeof(id), "##tipo%d", k);
        if (Pill(id, catalog::KindName(k), S.kind == k, px, pillY, pillH, &pw)) {
            S.kind = k;
            S.filterDirty = true;
        }
        px += pw + 6 * u;
    }
    px += 8 * u;
    std::string archiveLabel = S.archive < 0 ? std::string("Todos os .img")
                                             : catalog::Archives()[std::min<size_t>(S.archive, catalog::Archives().size() - 1)];
    if (Pill("##arquivo", archiveLabel.c_str(), S.archive >= 0, px, pillY, pillH, &pw)) {
        ImGui::OpenPopup("##arquivos");
    }
    float popupX = px, popupY = pillY + pillH + 4 * u;
    px += pw + 6 * u;
    if (Pill("##mods", "S\xC3\xB3 substitu\xC3\xAD" "dos", S.onlyReplaced, px, pillY, pillH, &pw)) {
        S.onlyReplaced = !S.onlyReplaced;
        S.filterDirty = true;
    }
    ArchivePopup(popupX, popupY);
    // O inspetor fica no canto direito da barra.
    const char* look = "Info ao apontar";
    float lookW = TextSize(fonts.desc, look).x + 24 * u;
    if (Pill("##apontar", look, S.inspect, x + w - lookW, pillY, pillH, nullptr)) {
        S.inspect = !S.inspect;
        core::SetInspect(S.inspect);
        SaveIni();
        Toast("Trok DevTools", S.inspect ? "Info ao apontar ligada: feche o menu e mire em algo."
                                        : "Info ao apontar desligada.");
    }
    if (ImGui::IsItemHovered()) {
        Tooltip("Com o menu fechado, mostra o modelo do que est\xC3\xA1 no centro da tela.\n"
                "Bot\xC3\xA3o do meio do mouse: abre o card dele.");
    }

    if (S.filterDirty) {
        Refilter();
    }

    // Linha de estado
    float statusY = y + barH + 10 * u;
    char status[160];
    int queued = core::ThumbsQueued();
    snprintf(status, sizeof(status), "%s modelos  \xC2\xB7  %s no filtro", Thousands(static_cast<int>(catalog::All().size())).c_str(),
             Thousands(static_cast<int>(S.filtered.size())).c_str());
    Text(dl, fonts.desc, x + 2 * u, statusY, col::hint, status);
    if (queued > 0) {
        char loading[64];
        snprintf(loading, sizeof(loading), "carregando miniaturas (%d)", queued);
        ImVec2 ts = TextSize(fonts.desc, loading);
        Spinner(dl, ImVec2(x + w - ts.x - 14 * u, statusY + ts.y * 0.5f), 5 * u, static_cast<float>(ImGui::GetTime()));
        Text(dl, fonts.desc, x + w - ts.x, statusY, col::hint, loading);
    }

    // Grade
    float gridY = statusY + fonts.desc->FontSize + 10 * u;
    float gridH = y + h - gridY;
    ImGui::SetCursorScreenPos(ImVec2(x - 4 * u, gridY));
    ImGui::BeginChild("##grade", ImVec2(w + 8 * u, gridH), false, ImGuiWindowFlags_NoBackground);
    core::SetBrowsing(!S.cardOpen);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    origin.x += 4 * u;
    float avail = w - 6 * u;
    float gap = 10 * u;
    float target = 176 * u;
    int cols = std::max(1, static_cast<int>((avail + gap) / (target + gap)));
    float cw = std::floor((avail - gap * (cols - 1)) / cols);
    float ch = cw + 44 * u;
    int n = static_cast<int>(S.filtered.size());
    int rows = (n + cols - 1) / cols;
    float rowH = ch + gap;
    float scrollY = ImGui::GetScrollY();
    int first = std::max(0, static_cast<int>(scrollY / rowH) - 1);
    int last = std::min(rows - 1, static_cast<int>((scrollY + gridH) / rowH) + 1);
    const std::vector<catalog::Entry>& all = catalog::All();
    for (int r = first; r <= last; ++r) {
        for (int c = 0; c < cols; ++c) {
            int i = r * cols + c;
            if (i >= n) {
                break;
            }
            Card(all[S.filtered[i]], origin.x + c * (cw + gap), origin.y + r * rowH, cw, ch);
        }
    }
    if (n == 0) {
        const char* none = "Nenhum modelo com esse filtro.";
        ImVec2 ts = TextSize(fonts.desc, none);
        Text(ImGui::GetWindowDrawList(), fonts.desc, origin.x + (avail - ts.x) * 0.5f, origin.y + 40 * u, col::hint, none);
    }
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(avail, std::max(1.0f, rows * rowH)));
    ImGui::EndChild();
}

// ---------------------------------------------------------------- card aberto

struct InfoRow {
    std::string label, value;
};

std::string Flags(WORD f) {
    std::string out;
    struct {
        WORD bit;
        const char* name;
    } names[] = {{0x02, "desenha por \xC3\xBAltimo"}, {0x04, "aditivo"}, {0x08, "sem z-buffer"},
                 {0x10, "sem sombra"}, {0x40, "faces de um lado"}, {0x80, "LOD"}};
    for (const auto& n : names) {
        if (f & n.bit) {
            out += out.empty() ? "" : ", ";
            out += n.name;
        }
    }
    char hex[16];
    snprintf(hex, sizeof(hex), "0x%04X", f);
    return out.empty() ? std::string(hex) : std::string(hex) + " (" + out + ")";
}

std::vector<InfoRow> CardInfo(const catalog::Entry& e, const mesh::Mesh* m) {
    std::vector<InfoRow> rows;
    char buf[256];
    snprintf(buf, sizeof(buf), "%d", e.id);
    rows.push_back({"ID", buf});
    rows.push_back({"Tipo", catalog::TypeName(e)});
    rows.push_back({"DFF", e.dff.empty() ? "\xE2\x80\x94" : e.dff + ".dff"});
    std::string txd = e.txd.empty() ? "\xE2\x80\x94" : e.txd + ".txd";
    if (!e.txdParent.empty()) {
        txd += "  (pai: " + e.txdParent + ".txd)";
    }
    rows.push_back({"TXD", txd});
    if (e.noFile) {
        rows.push_back({"Arquivo", "sem arquivo no streaming"});
    } else {
        snprintf(buf, sizeof(buf), "%s  \xC2\xB7  %s KB", e.imgName.empty() ? "?" : e.imgName.c_str(),
                 Decimal(e.size * 2048 / 1024.0f, 1).c_str());
        rows.push_back({"Arquivo", buf});
        if (e.replaced) {
            rows.push_back({"Substitu\xC3\xAD" "do", e.imgEntry.empty()
                                                     ? std::string("o jogo l\xC3\xAA de fora do diret\xC3\xB3rio do .img")
                                                     : "o jogo l\xC3\xAA \"" + e.imgEntry + "\""});
        }
    }
    rows.push_back({"Definido em", e.ide.empty() ? "\xE2\x80\x94" : e.ide});
    if (e.drawDistance > 0) {
        rows.push_back({"Dist\xC3\xA2ncia", Decimal(e.drawDistance, 0) + " m"});
    }
    rows.push_back({"Flags", Flags(e.flags)});
    if (m) {
        rows.push_back({"Tamanho", Decimal(m->max.x - m->min.x, 2) + " \xC3\x97 " + Decimal(m->max.y - m->min.y, 2) +
                                       " \xC3\x97 " + Decimal(m->max.z - m->min.z, 2) + " m"});
        snprintf(buf, sizeof(buf), "%s v\xC3\xA9rtices  \xC2\xB7  %s tri\xC3\xA2ngulos", Thousands(m->vertices).c_str(),
                 Thousands(m->triangles).c_str());
        rows.push_back({"Malha", buf});
        snprintf(buf, sizeof(buf), "%d materiais  \xC2\xB7  %d at\xC3\xB4micos", m->materials, m->atomics);
        rows.push_back({"", buf});
        if (!m->hasCol) {
            rows.push_back({"Colis\xC3\xA3o", "sem colis\xC3\xA3o"});
        } else if (!m->colLoaded) {
            rows.push_back({"Colis\xC3\xA3o", "n\xC3\xA3o carregada (carrega perto dela no mapa)"});
        } else {
            snprintf(buf, sizeof(buf), "%d caixas  \xC2\xB7  %d esferas  \xC2\xB7  %s tri\xC3\xA2ngulos", m->colBoxes,
                     m->colSpheres, Thousands(m->colTriangles).c_str());
            rows.push_back({"Colis\xC3\xA3o", buf});
        }
    }
    return rows;
}

// Fundo: um quadradinho com a cor; o clique abre o seletor de cor da casa (o mesmo do Geral), para cima. Devolve onde
// terminou.
float BackgroundSwatch(ImDrawList* dl, float ox, float oy) {
    float sw = 30 * u, sh = 20 * u, sy = oy + 3 * u;
    ImGui::SetCursorScreenPos(ImVec2(ox, oy));
    if (ImGui::InvisibleButton("##fundo", ImVec2(sw + 52 * u, 26 * u))) {
        ImGui::OpenPopup("##fundoPop");
    }
    bool bgHovered = ImGui::IsItemHovered();
    if (bgHovered) {
        Hand();
        Tooltip("Cor do fundo do visualizador");
    }
    dl->AddRectFilled(ImVec2(ox, sy), ImVec2(ox + sw, sy + sh), S.background | IM_COL32_A_MASK, 5 * u);
    dl->AddRect(ImVec2(ox - 0.5f, sy - 0.5f), ImVec2(ox + sw + 0.5f, sy + sh + 0.5f), col::White(bgHovered ? 110 : 60), 5 * u,
                0, 1.0f);
    Text(dl, fonts.desc, ox + sw + 8 * u, oy + (26 * u - fonts.desc->FontSize) * 0.5f - 0.5f * u,
         bgHovered ? col::text : col::column, "Fundo");
    ImGui::SetNextWindowPos(ImVec2(ox, oy - 8 * u), ImGuiCond_Appearing, ImVec2(0.0f, 1.0f));
    ImU32 bg = S.background;
    if (ColorPopup("##fundoPop", &bg, BACKGROUND_DEFAULT)) {
        SetBackground(bg);
        SaveIni();
    }
    return ox + sw + 8 * u + TextSize(fonts.desc, "Fundo").x;
}

void Viewer(const catalog::Entry& e, const mesh::Mesh* m, ImVec2 a, float side) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 b(a.x + side, a.y + side);
    ImGuiIO& io = ImGui::GetIO();
    std::string why;
    int ti = TexIndex(m);
    if (m && !S.texName.empty() && ti < 0) {
        S.texName.clear(); // o modelo novo nao tem essa textura
    }
    if (ti >= 0) {
        // Uma textura em destaque, sobre o xadrez (para ver o alfa).
        const mesh::Texture& t = m->textures[ti];
        Checker(dl, a, b, 12 * u);
        render::TexInfo info;
        bool described = render::Describe(t.d3d, &info);
        float tw = described ? static_cast<float>(info.width) : 1.0f, th = described ? static_cast<float>(info.height) : 1.0f;
        // Uma faixa em cima para o "Voltar ao modelo" e outra embaixo para a legenda: a textura nunca fica atras deles
        float top = a.y + 46 * u, bottom = b.y - 36 * u;
        float scale = std::min((side - 40 * u) / tw, (bottom - top) / th);
        ImVec2 size(tw * scale, th * scale);
        ImVec2 p((a.x + b.x - size.x) * 0.5f, top + (bottom - top - size.y) * 0.5f);
        if (t.d3d) {
            dl->AddImage(static_cast<ImTextureID>(t.d3d), p, ImVec2(p.x + size.x, p.y + size.y));
        }
        char label[160];
        snprintf(label, sizeof(label), "%s  \xC2\xB7  %d\xC3\x97%d  \xC2\xB7  %s  \xC2\xB7  %d mipmaps%s", t.name.c_str(),
                 info.width, info.height, info.format, info.levels, t.alpha ? "  \xC2\xB7  alfa" : "");
        ImVec2 ts = TextSize(fonts.desc, label);
        Text(dl, fonts.desc, (a.x + b.x - ts.x) * 0.5f, b.y - 26 * u, col::column, label);
        if (Pill("##voltar", "Voltar ao modelo", false, a.x + 10 * u, a.y + 10 * u, 26 * u, nullptr)) {
            S.texName.clear();
        }
        return;
    }
    if (!m) {
        dl->AddRectFilled(a, b, col::Rgba(20, 20, 20), 8 * u);
        ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
        if (core::OpenFailed(&why)) {
            std::string text = "Sem visualiza\xC3\xA7\xC3\xA3o: " + why;
            ImVec2 ts = TextSize(fonts.desc, text.c_str());
            Text(dl, fonts.desc, c.x - ts.x * 0.5f, c.y - ts.y * 0.5f, col::hint, text.c_str());
        } else {
            Spinner(dl, c, 12 * u, static_cast<float>(ImGui::GetTime()));
        }
        return;
    }
    // Interacao: arrastar gira, roda aproxima, duplo clique centraliza. A faixa de baixo fica de fora: com ela dentro, o
    // clique nas opcoes (Grade, fundo...) virava arrasto do visualizador e nao chegava nelas.
    ImGui::SetCursorScreenPos(a);
    ImGui::InvisibleButton("##visual", ImVec2(side, side - 46 * u));
    bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f)) {
        S.view.yaw -= io.MouseDelta.x * 0.4f;
        S.view.pitch = std::max(-85.0f, std::min(85.0f, S.view.pitch + io.MouseDelta.y * 0.4f));
        S.spin = false;
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
    if (hovered && io.MouseWheel != 0.0f) {
        S.view.zoom = std::max(0.3f, std::min(12.0f, S.view.zoom * (io.MouseWheel > 0 ? 1.12f : 1 / 1.12f)));
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        S.view.yaw = render::View().yaw;
        S.view.pitch = render::View().pitch;
        S.view.zoom = 1.0f;
    }
    if (S.spin) {
        S.view.yaw = std::fmod(S.view.yaw - io.DeltaTime * 18.0f, 360.0f);
    }
    int target = side * 1.5f > 1024 ? 2048 : 1024;
    ImTextureID tex = render::DrawViewer(*m, S.view, target);
    if (tex) {
        dl->AddImageRounded(tex, a, b, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, 8 * u);
    } else {
        dl->AddRectFilled(a, b, col::Rgba(20, 20, 20), 8 * u);
    }
    dl->AddRect(a, b, col::White(14), 8 * u);

    // Opcoes do visualizador, no canto de baixo.
    struct Opt {
        const char* id;
        const char* label;
        bool* value;
    } opts[] = {{"##vTex", "Texturas", &S.view.textures},
                {"##vWire", "Arame", &S.view.wire},
                {"##vCol", "Colis\xC3\xA3o", &S.view.collision},
                {"##vGrid", "Grade", &S.view.grid},
                {"##vSpin", "Girar", &S.spin}};
    float ox = a.x + 10 * u, oy = b.y - 36 * u, pw = 0;
    // Uma faixa escura atras das opcoes (canal 0, desenhada no fim, quando a largura e conhecida): elas continuam
    // legiveis em qualquer fundo (claro, branco, verde).
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    for (Opt& o : opts) {
        if (Pill(o.id, o.label, *o.value, ox, oy, 26 * u, &pw)) {
            *o.value = !*o.value;
            SaveIni();
        }
        ox += pw + 6 * u;
    }
    ox = BackgroundSwatch(dl, ox + 4 * u, oy) + 12 * u;
    dl->ChannelsSetCurrent(0);
    dl->AddRectFilled(ImVec2(a.x + 4 * u, oy - 6 * u), ImVec2(ox, oy + 32 * u), IM_COL32(0, 0, 0, 120), 19 * u);
    dl->ChannelsMerge();
}

void TextureTiles(const mesh::Mesh& m, float x, float* y, float w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float tile = 74 * u, gap = 10 * u;
    int cols = std::max(1, static_cast<int>((w + gap) / (tile + gap)));
    tile = std::floor((w - gap * (cols - 1)) / cols);
    float tileH = tile + 34 * u;
    for (int section = 0; section < 2; ++section) {
        std::vector<int> list;
        for (int i = 0; i < static_cast<int>(m.textures.size()); ++i) {
            if (m.textures[i].used == (section == 0)) {
                list.push_back(i);
            }
        }
        if (list.empty()) {
            continue;
        }
        char title[96];
        snprintf(title, sizeof(title), section == 0 ? "Texturas usadas (%d)" : "Tamb\xC3\xA9m na TXD (%d)",
                 static_cast<int>(list.size()));
        Text(dl, fonts.desc, x, *y, col::column, title);
        *y += fonts.desc->FontSize + 8 * u;
        for (size_t k = 0; k < list.size(); ++k) {
            const mesh::Texture& t = m.textures[list[k]];
            float tx = x + (k % cols) * (tile + gap), ty = *y + (k / cols) * (tileH + gap);
            char id[24];
            snprintf(id, sizeof(id), "##tex%d", list[k]);
            ImGui::SetCursorScreenPos(ImVec2(tx, ty));
            if (ImGui::InvisibleButton(id, ImVec2(tile, tileH))) {
                S.texName = t.name;
            }
            bool hovered = ImGui::IsItemHovered();
            if (hovered) {
                Hand();
            }
            Checker(dl, ImVec2(tx, ty), ImVec2(tx + tile, ty + tile), 6 * u);
            if (t.d3d) {
                dl->AddImage(static_cast<ImTextureID>(t.d3d), ImVec2(tx, ty), ImVec2(tx + tile, ty + tile));
            }
            bool focused = S.texName == t.name;
            dl->AddRect(ImVec2(tx, ty), ImVec2(tx + tile, ty + tile), focused ? col::strong : col::White(hovered ? 60 : 14), 4 * u,
                        0, focused ? 2.0f * u : 1.0f);
            std::string name = Ellipsize(fonts.desc, t.name, tile);
            Text(dl, fonts.desc, tx, ty + tile + 4 * u, section == 0 ? col::text : col::column, name.c_str());
            render::TexInfo info;
            char dims[48];
            if (render::Describe(t.d3d, &info)) {
                snprintf(dims, sizeof(dims), "%d\xC3\x97%d %s", info.width, info.height, info.format);
            } else {
                snprintf(dims, sizeof(dims), "%d\xC3\x97%d", t.width, t.height);
            }
            Text(dl, fonts.desc, tx, ty + tile + 4 * u + fonts.desc->FontSize + 1 * u, col::hint, dims);
            if (hovered) {
                Tooltip((t.name + (t.mask.empty() ? "" : "\nm\xC3\xA1scara: " + t.mask)).c_str());
            }
        }
        int rows = (static_cast<int>(list.size()) + cols - 1) / cols;
        *y += rows * (tileH + gap) + 8 * u;
    }
}

// Um modelo da lista "quem usa esta textura": miniatura, id e nome; o do card fica marcado.
bool UserRow(int id, float x, float y, float w, float h, bool current, const char* note) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    char bid[24];
    snprintf(bid, sizeof(bid), "##uso%d", id);
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    bool clicked = ImGui::InvisibleButton(bid, ImVec2(w, h)) && !current;
    bool hovered = ImGui::IsItemHovered() && !current;
    if (hovered) {
        Hand();
    }
    Box(dl, ImVec2(x, y), ImVec2(x + w, y + h), current ? col::White(16) : col::White(hovered ? 9 : 3),
        current ? col::White(70) : col::White(hovered ? 26 : 8), 7 * u);
    float pad = 4 * u, side = h - 2 * pad;
    ImVec2 t0(x + pad, y + pad), t1(t0.x + side, t0.y + side);
    ImTextureID tex = nullptr;
    ImVec2 uv0, uv1;
    if (render::Thumb(id, &tex, &uv0, &uv1)) {
        dl->AddImageRounded(tex, t0, t1, uv0, uv1, IM_COL32_WHITE, 5 * u);
    } else {
        dl->AddRectFilled(t0, t1, col::Rgba(20, 20, 20), 5 * u);
        if (!core::ThumbFailed(id)) {
            core::WantThumb(id);
            core::SetBrowsing(true);
            Spinner(dl, ImVec2((t0.x + t1.x) * 0.5f, (t0.y + t1.y) * 0.5f), 6 * u, static_cast<float>(ImGui::GetTime()));
        }
    }
    const catalog::Entry* e = catalog::Find(id);
    char line1[96], line2[128];
    snprintf(line1, sizeof(line1), "%d  \xC2\xB7  %s", id, e && !e->dff.empty() ? e->dff.c_str() : "?");
    if (note) {
        snprintf(line2, sizeof(line2), "%s", note);
    } else {
        snprintf(line2, sizeof(line2), "%s  \xC2\xB7  %s", e ? catalog::TypeName(*e) : "", e ? e->imgName.c_str() : "");
    }
    float tx = t1.x + 10 * u, ty = y + (h - fonts.body->FontSize - fonts.desc->FontSize - 2 * u) * 0.5f;
    float tagW = 0;
    if (current) {
        const char* tag = "aberto";
        ImVec2 ts = TextSize(fonts.desc, tag);
        tagW = ts.x + 16 * u;
        ImVec2 p0(x + w - tagW - 8 * u, y + (h - ts.y - 6 * u) * 0.5f);
        dl->AddRectFilled(p0, ImVec2(p0.x + tagW, p0.y + ts.y + 6 * u), col::strong, (ts.y + 6 * u) * 0.5f);
        Text(dl, fonts.desc, p0.x + 8 * u, p0.y + 3 * u - 0.5f * u, col::ink, tag);
        tagW += 12 * u;
    }
    float textW = x + w - tx - tagW - 6 * u;
    Text(dl, fonts.body, tx, ty, col::text, Ellipsize(fonts.body, line1, textW).c_str());
    Text(dl, fonts.desc, tx, ty + fonts.body->FontSize + 2 * u, note ? col::Rgba(255, 196, 120) : col::hint,
         Ellipsize(fonts.desc, line2, textW).c_str());
    return clicked;
}

// O lado direito com uma textura em destaque: os dados dela, quem mais usa e as outras texturas do modelo.
void TexturePanel(const catalog::Entry& e, const mesh::Mesh& m, int ti, float x, float* y, float w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const mesh::Texture& t = m.textures[ti];
    core::FindTextureUsers(S.cardId, t.name);
    const core::TextureUsers& tu = core::TextureUsersNow();

    Text(dl, fonts.title, x, *y, col::text, t.name.c_str());
    *y += fonts.title->FontSize + 12 * u;
    std::vector<InfoRow> rows;
    std::string txd = (tu.txdName.empty() ? e.txd : tu.txdName) + ".txd";
    if (tu.txd >= 0 && tu.txd != e.txdIndex) {
        txd += "  (m\xC3\xA3" "e da " + e.txd + ".txd)";
    }
    rows.push_back({"TXD", txd});
    render::TexInfo info;
    bool described = render::Describe(t.d3d, &info);
    char buf[96];
    snprintf(buf, sizeof(buf), "%d \xC3\x97 %d", described ? info.width : t.width, described ? info.height : t.height);
    rows.push_back({"Tamanho", buf});
    if (described) {
        rows.push_back({"Formato", info.format});
        snprintf(buf, sizeof(buf), "%d", info.levels);
        rows.push_back({"Mipmaps", buf});
    }
    rows.push_back({"Alfa", t.alpha ? "sim" : "n\xC3\xA3o"});
    if (!t.mask.empty()) {
        rows.push_back({"M\xC3\xA1scara", t.mask});
    }
    float labelW = 0;
    for (const InfoRow& r : rows) {
        labelW = std::max(labelW, TextSize(fonts.desc, r.label.c_str()).x);
    }
    labelW += 16 * u;
    float lineH = fonts.desc->FontSize + 9 * u;
    for (const InfoRow& r : rows) {
        Text(dl, fonts.desc, x, *y, col::hint, r.label.c_str());
        Text(dl, fonts.desc, x + labelW, *y, col::text, Ellipsize(fonts.desc, r.value, w - labelW).c_str());
        *y += lineH;
    }
    *y += 8 * u;
    float bh = 32 * u, gap = 8 * u, half = std::floor((w - gap) * 0.5f);
    if (Button("##voltarModelo", "Voltar ao modelo", x, *y, half, bh, false)) {
        S.texName.clear();
    }
    if (Button("##copiarTex", "Copiar nome", x + half + gap, *y, w - half - gap, bh, false)) {
        ImGui::SetClipboardText(t.name.c_str());
        Toast("Trok DevTools", "Nome da textura copiado.");
    }
    *y += bh + 20 * u;

    // Quem usa: o do card primeiro, depois os outros em ordem de id
    bool searching = tu.texture == t.name && !tu.done;
    int others = 0;
    bool currentUses = false;
    for (int id : tu.users) {
        currentUses = currentUses || id == S.cardId;
        others += id != S.cardId;
    }
    char title[96];
    snprintf(title, sizeof(title), "Modelos com esta textura (%d)", static_cast<int>(tu.users.size()));
    Text(dl, fonts.desc, x, *y, col::column, title);
    *y += fonts.desc->FontSize + 6 * u;
    char status[160];
    if (searching) {
        snprintf(status, sizeof(status), "procurando nos modelos da TXD: %d de %d", tu.checked, tu.total);
        Spinner(dl, ImVec2(x + 6 * u, *y + fonts.desc->FontSize * 0.5f), 5 * u, static_cast<float>(ImGui::GetTime()));
        Text(dl, fonts.desc, x + 18 * u, *y, col::hint, status);
    } else {
        if (tu.users.empty() || (others == 0 && currentUses)) {
            snprintf(status, sizeof(status), "S\xC3\xB3 este modelo usa (%d modelos da TXD vistos)", tu.checked);
        } else {
            snprintf(status, sizeof(status), "%d modelos da TXD vistos%s", tu.checked,
                     tu.capped ? ", s\xC3\xB3 os primeiros (TXD muito grande)" : "");
        }
        Text(dl, fonts.desc, x, *y, col::hint, status);
    }
    *y += fonts.desc->FontSize + 10 * u;
    float rowH = 52 * u;
    const char* note = !currentUses && tu.done ? "est\xC3\xA1 na TXD, mas este modelo n\xC3\xA3o usa" : nullptr;
    UserRow(S.cardId, x, *y, w, rowH, true, note);
    *y += rowH + 6 * u;
    for (int id : tu.users) {
        if (id == S.cardId) {
            continue;
        }
        if (UserRow(id, x, *y, w, rowH, false, nullptr)) {
            SelectTextureUser(id);
        }
        *y += rowH + 6 * u;
    }
    *y += 14 * u;
    TextureTiles(m, x, y, w);
}

void CardPopup(ImVec2 shellPos, ImVec2 shellSize, bool escape) {
    if (S.cardRequest) {
        S.cardRequest = false;
        S.cardOpen = true;
        ImGui::OpenPopup("##cartao");
    }
    if (!S.cardOpen) {
        return;
    }
    const catalog::Entry* e = catalog::Find(S.cardId);
    float pw = std::floor(shellSize.x * 0.95f), ph = std::floor(shellSize.y * 0.93f);
    ImGui::SetNextWindowPos(ImVec2(shellPos.x + (shellSize.x - pw) * 0.5f, shellPos.y + (shellSize.y - ph) * 0.5f));
    ImGui::SetNextWindowSize(ImVec2(pw, ph));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10 * u);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(12, 12, 12, 254));
    bool visible = ImGui::BeginPopupModal("##cartao", nullptr, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (!visible) {
        S.cardOpen = false;
        core::Close();
        return;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();
    float padX = 18 * u, headerH = HeaderHeight();
    bool close = escape;
    if (escape && !S.texName.empty()) {
        S.texName.clear(); // o Esc volta da textura para o modelo; o proximo fecha o card
        close = false;
    }

    // Cabecalho: o id e o nome no meio, o X a direita.
    char title[160];
    snprintf(title, sizeof(title), "%d  \xC2\xB7  %s", S.cardId, e && !e->dff.empty() ? e->dff.c_str() : "?");
    ImVec2 ts = TextSize(fonts.title, title);
    const char* type = e ? catalog::TypeName(*e) : "";
    ImVec2 tys = TextSize(fonts.desc, type);
    float tx = pos.x + (pw - ts.x - 10 * u - tys.x) * 0.5f;
    Text(dl, fonts.title, tx, pos.y + (headerH - ts.y) * 0.5f, col::text, title);
    Text(dl, fonts.desc, tx + ts.x + 10 * u, pos.y + (headerH - tys.y) * 0.5f + 1 * u, col::version, type);
    dl->AddLine(ImVec2(pos.x + padX, pos.y + headerH - 0.5f), ImVec2(pos.x + pw - padX, pos.y + headerH - 0.5f),
                col::separator);
    float closeSide = 28 * u;
    ImVec2 cp(pos.x + pw - padX - closeSide + 6 * u, pos.y + (headerH - closeSide) * 0.5f);
    ImGui::SetCursorScreenPos(cp);
    if (ImGui::InvisibleButton("##fecharCard", ImVec2(closeSide, closeSide))) {
        close = true;
    }
    bool closeHovered = ImGui::IsItemHovered();
    if (closeHovered) {
        Hand();
    }
    DrawIcon(dl, Icon::Close, ImVec2(cp.x + closeSide * 0.5f, cp.y + closeSide * 0.5f), closeHovered ? col::text : col::hint);

    const mesh::Mesh* m = core::OpenMesh();
    float footerH = FooterHeight();
    float bodyY = pos.y + headerH + 14 * u;
    float bodyH = ph - headerH - 14 * u - footerH - 14 * u;
    float side = std::floor(std::min(bodyH, (pw - 2 * padX) * 0.58f));
    if (e) {
        Viewer(*e, m, ImVec2(pos.x + padX, bodyY), side);
    }

    // Lado direito: informacoes, acoes e texturas (com rolagem).
    float rx = pos.x + padX + side + 20 * u;
    float rw = pos.x + pw - padX - rx;
    ImGui::SetCursorScreenPos(ImVec2(rx, bodyY));
    ImGui::BeginChild("##infoCard", ImVec2(rw + 6 * u, bodyH), false, ImGuiWindowFlags_NoBackground);
    ImDrawList* cdl = ImGui::GetWindowDrawList();
    ImVec2 c0 = ImGui::GetCursorScreenPos();
    float y = c0.y;
    float innerW = rw - 8 * u;
    int ti = TexIndex(m);
    if (e && ti >= 0) {
        TexturePanel(*e, *m, ti, c0.x, &y, innerW);
    } else if (e) {
        std::vector<InfoRow> rows = CardInfo(*e, m);
        float labelW = 0;
        for (const InfoRow& r : rows) {
            labelW = std::max(labelW, TextSize(fonts.desc, r.label.c_str()).x);
        }
        labelW += 16 * u;
        float lineH = fonts.desc->FontSize + 9 * u;
        for (const InfoRow& r : rows) {
            Text(cdl, fonts.desc, c0.x, y, col::hint, r.label.c_str());
            std::string value = Ellipsize(fonts.desc, r.value, innerW - labelW);
            bool mod = r.label.rfind("Substitu", 0) == 0;
            Text(cdl, fonts.desc, c0.x + labelW, y, mod ? col::Rgba(255, 196, 120) : col::text, value.c_str());
            y += lineH;
        }
        y += 8 * u;

        // Acoes
        float bh = 32 * u, gap = 8 * u;
        float half = std::floor((innerW - gap) * 0.5f);
        if (Button("##criar", "Criar no mundo (1:1)", c0.x, y, innerW, bh, true, m != nullptr)) {
            core::Spawn(S.cardId);
        }
        y += bh + gap;
        char del[64];
        snprintf(del, sizeof(del), "Apagar criados (%d)", core::SpawnedCount());
        if (Button("##apagar", del, c0.x, y, half, bh, false, core::SpawnedCount() > 0)) {
            core::DespawnAll();
            Toast("Trok DevTools", "Os modelos criados foram apagados.");
        }
        if (Button("##copiarId", "Copiar ID", c0.x + half + gap, y, innerW - half - gap, bh, false)) {
            char text[16];
            snprintf(text, sizeof(text), "%d", e->id);
            ImGui::SetClipboardText(text);
            Toast("Trok DevTools", "ID copiado.");
        }
        y += bh + gap;
        if (Button("##copiarDff", "Copiar DFF", c0.x, y, half, bh, false, !e->dff.empty())) {
            ImGui::SetClipboardText((e->dff + ".dff").c_str());
            Toast("Trok DevTools", "Nome do DFF copiado.");
        }
        if (Button("##copiarTxd", "Copiar TXD", c0.x + half + gap, y, innerW - half - gap, bh, false, !e->txd.empty())) {
            ImGui::SetClipboardText((e->txd + ".txd").c_str());
            Toast("Trok DevTools", "Nome da TXD copiado.");
        }
        y += bh + 18 * u;
        if (m) {
            TextureTiles(*m, c0.x, &y, innerW);
        }
    }
    ImGui::SetCursorScreenPos(c0);
    ImGui::Dummy(ImVec2(innerW, std::max(1.0f, y - c0.y)));
    ImGui::EndChild();

    // Rodape
    float footerY = pos.y + ph - footerH;
    dl->PushClipRect(pos, ImVec2(pos.x + pw, pos.y + ph), false);
    dl->AddRectFilled(ImVec2(pos.x + 1, footerY), ImVec2(pos.x + pw - 1, pos.y + ph - 1), IM_COL32(0, 0, 0, 46), 10 * u,
                      ImDrawFlags_RoundCornersBottom);
    dl->AddLine(ImVec2(pos.x + 1, footerY + 0.5f), ImVec2(pos.x + pw - 1, footerY + 0.5f), col::separator);
    dl->PopClipRect();
    const char* modelHints[][2] = {{"Arraste", "Girar"}, {"Roda", "Zoom"}, {"Duplo clique", "Centralizar"}, {"Esc", "Fechar"}};
    const char* texHints[][2] = {{"Clique num modelo", "Abrir com a textura"}, {"Esc", "Voltar ao modelo"}};
    std::vector<std::pair<const char*, const char*>> hints;
    if (TexIndex(m) >= 0) {
        for (const auto& h : texHints) {
            hints.push_back({h[0], h[1]});
        }
    } else {
        for (const auto& h : modelHints) {
            hints.push_back({h[0], h[1]});
        }
    }
    float hx = pos.x + padX, hy = footerY + (footerH - fonts.desc->FontSize) * 0.5f;
    for (const auto& h : hints) {
        float kw = TextSize(fonts.desc, h.first).x;
        Text(dl, fonts.desc, hx, hy, col::column, h.first);
        Text(dl, fonts.desc, hx + kw + 6 * u, hy, col::hint, h.second);
        hx += kw + 6 * u + TextSize(fonts.desc, h.second).x + 22 * u;
    }

    std::string message;
    bool ok = false;
    if (core::TakeSpawnMessage(&message, &ok)) {
        Toast(ok ? "Criado no mundo" : "N\xC3\xA3o deu para criar", message.c_str());
    }
    if (close) {
        ImGui::CloseCurrentPopup();
        S.cardOpen = false;
        core::Close();
    }
    ImGui::EndPopup();
}

// ---------------------------------------------------------------- aba Audios

// Icones desenhados no traco da casa (o kit nao tem play, stop, download e repetir).
void IconPlay(ImDrawList* dl, ImVec2 c, float s, ImU32 color) {
    dl->AddTriangleFilled(ImVec2(c.x - s * 0.32f, c.y - s * 0.42f), ImVec2(c.x - s * 0.32f, c.y + s * 0.42f),
                          ImVec2(c.x + s * 0.42f, c.y), color);
}

void IconStop(ImDrawList* dl, ImVec2 c, float s, ImU32 color) {
    dl->AddRectFilled(ImVec2(c.x - s * 0.3f, c.y - s * 0.3f), ImVec2(c.x + s * 0.3f, c.y + s * 0.3f), color, s * 0.08f);
}

void IconDownload(ImDrawList* dl, ImVec2 c, float s, ImU32 color) {
    float t = 1.5f * u;
    dl->AddLine(ImVec2(c.x, c.y - s * 0.42f), ImVec2(c.x, c.y + s * 0.12f), color, t);
    dl->AddLine(ImVec2(c.x - s * 0.26f, c.y - s * 0.12f), ImVec2(c.x, c.y + s * 0.15f), color, t);
    dl->AddLine(ImVec2(c.x + s * 0.26f, c.y - s * 0.12f), ImVec2(c.x, c.y + s * 0.15f), color, t);
    dl->AddLine(ImVec2(c.x - s * 0.4f, c.y + s * 0.42f), ImVec2(c.x + s * 0.4f, c.y + s * 0.42f), color, t);
}

void IconRepeat(ImDrawList* dl, ImVec2 c, float s, ImU32 color) {
    float t = 1.5f * u, w = s * 0.4f, h = s * 0.24f, a = s * 0.13f;
    dl->AddLine(ImVec2(c.x - w, c.y + h * 0.3f), ImVec2(c.x - w, c.y - h), color, t);
    dl->AddLine(ImVec2(c.x - w, c.y - h), ImVec2(c.x + w * 0.7f, c.y - h), color, t);
    dl->AddTriangleFilled(ImVec2(c.x + w * 0.6f, c.y - h - a), ImVec2(c.x + w * 0.6f, c.y - h + a), ImVec2(c.x + w * 0.6f + a * 1.6f, c.y - h),
                          color);
    dl->AddLine(ImVec2(c.x + w, c.y - h * 0.3f), ImVec2(c.x + w, c.y + h), color, t);
    dl->AddLine(ImVec2(c.x + w, c.y + h), ImVec2(c.x - w * 0.7f, c.y + h), color, t);
    dl->AddTriangleFilled(ImVec2(c.x - w * 0.6f, c.y + h - a), ImVec2(c.x - w * 0.6f, c.y + h + a), ImVec2(c.x - w * 0.6f - a * 1.6f, c.y + h),
                          color);
}

// Botao redondo com um icone. kind: 0 contorno (on = cheio), 1 cheio e claro, 2 so o icone.
bool RoundButton(const char* id, ImVec2 c, float r, int kind, bool on, void (*icon)(ImDrawList*, ImVec2, float, ImU32),
                 const char* tip = nullptr, bool enabled = true) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::SetCursorScreenPos(ImVec2(c.x - r, c.y - r));
    bool clicked = ImGui::InvisibleButton(id, ImVec2(2 * r, 2 * r)) && enabled;
    bool hovered = enabled && ImGui::IsItemHovered();
    if (hovered) {
        Hand();
        if (tip) {
            Tooltip(tip);
        }
    }
    ImU32 ink;
    if (!enabled) {
        dl->AddCircle(c, r - 0.5f, col::White(14), 32, 1.0f);
        ink = col::disabled;
    } else if (kind == 1) {
        dl->AddCircleFilled(c, r, hovered ? col::Rgba(245, 245, 245) : col::strong, 32);
        ink = col::ink;
    } else if (kind == 0) {
        dl->AddCircleFilled(c, r, on ? col::strong : col::White(hovered ? 12 : 4), 32);
        if (!on) {
            dl->AddCircle(c, r - 0.5f, col::White(hovered ? 60 : 30), 32, 1.0f);
        }
        ink = on ? col::ink : (hovered ? col::text : col::column);
    } else {
        ink = on || hovered ? col::text : col::hint;
    }
    icon(dl, c, kind == 2 ? r * 1.25f : r, ink);
    return clicked;
}

std::string Seconds(float s) {
    return Decimal(s, 2) + " s";
}

bool AudioFilterActive() {
    const sfx::Filter& f = S.audioFilter;
    return !f.query.empty() || f.kind >= 0 || f.vehicle >= 0 || f.weapon >= 0 || f.loop != 0;
}

// Os veiculos que tem banco de motor, em ordem de nome (para a lista do filtro).
const std::vector<std::pair<std::string, int>>& AudioVehicles() {
    static std::vector<std::pair<std::string, int>> list;
    if (list.empty() && sfx::Ready()) {
        for (int i = 0; i < 212; ++i) {
            const audio_data::VehicleBanks& v = audio_data::VEHICLE_BANKS[i];
            if (v.player < 0 && v.dummy < 0) {
                continue;
            }
            const catalog::Entry* e = catalog::Find(400 + i);
            std::string name = e && !e->dff.empty() ? e->dff : std::to_string(400 + i);
            name[0] = static_cast<char>(toupper(static_cast<unsigned char>(name[0])));
            list.push_back({name, 400 + i});
        }
        std::sort(list.begin(), list.end());
    }
    return list;
}

// Abre o caminho ate um som (pacote e banco abertos) e pede para a arvore rolar ate ele.
void RevealSound(int bank, int sound) {
    const sfx::Bank* b = sfx::FindBank(bank);
    if (!b) {
        return;
    }
    // Fora do filtro atual: limpa o filtro (a arvore volta inteira, com o som aberto)
    if (AudioFilterActive() && S.audioHasResult && static_cast<size_t>(bank) < S.audioResult.sounds.size()) {
        const std::vector<int>& in = S.audioResult.sounds[bank];
        if (std::find(in.begin(), in.end(), sound) == in.end()) {
            S.audioQuery[0] = '\0';
            S.audioFilter = sfx::Filter();
        }
    }
    S.audioOpenPack.resize(sfx::Packs().size(), false);
    S.audioOpenBank.resize(sfx::Banks().size(), false);
    S.audioOpenPack[b->pack] = true;
    S.audioOpenBank[bank] = true;
    S.audioSelBank = bank;
    S.audioSelSound = sound;
    S.audioReveal = true;
}

void PlayOrStop(int bank, int sound) {
    if (player::Playing() && player::Bank() == bank && player::Sound() == sound) {
        player::Stop();
        return;
    }
    S.audioSelBank = bank;
    S.audioSelSound = sound;
    if (!player::Play(bank, sound)) {
        Toast("Trok DevTools", (std::string("N\xC3\xA3o deu para tocar: ") + player::Error()).c_str());
    }
}

void SaveWav(int bank, int sound) {
    std::string path;
    if (sfx::ExportWav(bank, sound, &path)) {
        size_t cut = path.find_last_of('\\');
        Toast("Salvo em Trok DevTools\\sons", (cut == std::string::npos ? path : path.substr(cut + 1)).c_str());
    } else {
        Toast("Trok DevTools", "N\xC3\xA3o deu para salvar o .wav.");
    }
}

// Lista suspensa da casa (como a do .img), com rolagem para listas longas. values[i] e o valor de labels[i].
void ChoicePopup(const char* id, const std::vector<std::string>& labels, const std::vector<int>& values, int* value,
                 float x, float y) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6 * u, 6 * u));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8 * u);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(18, 18, 18, 250));
    if (ImGui::BeginPopup(id)) {
        float w = 220 * u, h = 28 * u;
        for (const std::string& l : labels) {
            w = std::max(w, TextSize(fonts.desc, l.c_str()).x + 44 * u);
        }
        float listH = std::min(static_cast<float>(labels.size()) * h, 380 * u);
        ImGui::BeginChild("##opcoes", ImVec2(w, listH), false);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (size_t i = 0; i < labels.size(); ++i) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            char bid[24];
            snprintf(bid, sizeof(bid), "##op%d", static_cast<int>(i));
            if (ImGui::InvisibleButton(bid, ImVec2(w - 10 * u, h))) {
                *value = values[i];
                ImGui::CloseCurrentPopup();
            }
            bool hovered = ImGui::IsItemHovered();
            if (hovered) {
                Hand();
                dl->AddRectFilled(p, ImVec2(p.x + w - 10 * u, p.y + h), col::White(10), 6 * u);
            }
            bool chosen = *value == values[i];
            ImVec2 ts = TextSize(fonts.desc, labels[i].c_str());
            Text(dl, fonts.desc, p.x + 12 * u, p.y + (h - ts.y) * 0.5f, chosen ? col::text : col::column, labels[i].c_str());
            if (chosen) {
                DrawIcon(dl, Icon::Check, ImVec2(p.x + w - 26 * u, p.y + h * 0.5f), col::text, true);
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

struct AudioRow {
    int type; // 0 pacote, 1 banco, 2 som
    int pack, bank, sound;
    float y, h;
};

void AudioTree(float x, float y, float w, float h) {
    const std::vector<sfx::Pack>& packs = sfx::Packs();
    const std::vector<sfx::Bank>& banks = sfx::Banks();
    S.audioOpenPack.resize(packs.size(), false);
    S.audioOpenBank.resize(banks.size(), false);
    bool filtered = AudioFilterActive();
    const sfx::Result& r = S.audioResult;

    // As linhas (so as que aparecem): pacote > banco > som
    std::vector<AudioRow> rows;
    float total = 0, revealY = -1;
    auto add = [&](int type, int p, int b, int s, float rh) {
        rows.push_back({type, p, b, s, total, rh});
        total += rh;
    };
    for (int p = 0; p < static_cast<int>(packs.size()); ++p) {
        if (filtered && r.bankCount[p] == 0) {
            continue;
        }
        add(0, p, -1, -1, 58 * u);
        if (!S.audioOpenPack[p]) {
            continue;
        }
        for (int id : packs[p].banks) {
            const sfx::Bank& b = banks[id];
            if (filtered && r.sounds[id].empty()) {
                continue;
            }
            bool sub = !b.text.empty() || !b.vehicleText.empty();
            add(1, p, id, -1, (sub ? 52 : 34) * u);
            if (!S.audioOpenBank[id]) {
                continue;
            }
            const std::vector<int>* list = filtered ? &r.sounds[id] : nullptr;
            int n = list ? static_cast<int>(list->size()) : static_cast<int>(b.sounds.size());
            for (int k = 0; k < n; ++k) {
                int s = list ? (*list)[k] : k;
                if (S.audioReveal && id == S.audioSelBank && s == S.audioSelSound) {
                    revealY = total;
                }
                add(2, p, id, s, 38 * u);
            }
            total += 6 * u;
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::BeginChild("##arvore", ImVec2(w, h), false, ImGuiWindowFlags_NoBackground);
    if (S.audioReveal) {
        if (revealY >= 0) {
            ImGui::SetScrollY(std::max(0.0f, revealY - h * 0.4f));
        }
        S.audioReveal = false;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float scroll = ImGui::GetScrollY();
    float rw = w - 14 * u;
    for (const AudioRow& row : rows) {
        if (row.y + row.h < scroll - 40 * u || row.y > scroll + h + 40 * u) {
            continue;
        }
        float rx = origin.x, ry = origin.y + row.y;
        char id[32];
        if (row.type == 0) {
            const sfx::Pack& p = packs[row.pack];
            snprintf(id, sizeof(id), "##pac%d", row.pack);
            ImGui::SetCursorScreenPos(ImVec2(rx, ry));
            if (ImGui::InvisibleButton(id, ImVec2(rw, row.h - 4 * u))) {
                S.audioOpenPack[row.pack] = !S.audioOpenPack[row.pack];
            }
            bool hovered = ImGui::IsItemHovered();
            if (hovered) {
                Hand();
            }
            Box(dl, ImVec2(rx, ry), ImVec2(rx + rw, ry + row.h - 4 * u), col::White(hovered ? 9 : 5), col::White(10), 8 * u);
            DrawIcon(dl, S.audioOpenPack[row.pack] ? Icon::ChevronDown : Icon::ChevronRight,
                     ImVec2(rx + 16 * u, ry + 18 * u), col::column, true);
            Text(dl, fonts.title, rx + 30 * u, ry + 8 * u, col::text, p.name.c_str());
            char counts[96];
            if (filtered) {
                snprintf(counts, sizeof(counts), "%d banks \xC2\xB7 %s sons no filtro", r.bankCount[row.pack],
                         Thousands(r.soundCount[row.pack]).c_str());
            } else {
                snprintf(counts, sizeof(counts), "%d banks \xC2\xB7 %s sons", static_cast<int>(p.banks.size()),
                         Thousands(p.sounds).c_str());
            }
            ImVec2 cs = TextSize(fonts.desc, counts);
            Text(dl, fonts.desc, rx + rw - 14 * u - cs.x, ry + 11 * u, col::hint, counts);
            Text(dl, fonts.desc, rx + 30 * u, ry + 8 * u + fonts.title->FontSize + 5 * u, col::column,
                 Ellipsize(fonts.desc, p.description, rw - 44 * u).c_str());
        } else if (row.type == 1) {
            const sfx::Bank& b = banks[row.bank];
            snprintf(id, sizeof(id), "##bank%d", row.bank);
            float bx = rx + 18 * u, bw = rw - 18 * u;
            ImGui::SetCursorScreenPos(ImVec2(bx, ry));
            if (ImGui::InvisibleButton(id, ImVec2(bw, row.h - 2 * u))) {
                S.audioOpenBank[row.bank] = !S.audioOpenBank[row.bank];
            }
            bool hovered = ImGui::IsItemHovered();
            if (hovered) {
                Hand();
                dl->AddRectFilled(ImVec2(bx, ry), ImVec2(bx + bw, ry + row.h - 2 * u), col::White(6), 6 * u);
            }
            DrawIcon(dl, S.audioOpenBank[row.bank] ? Icon::ChevronDown : Icon::ChevronRight, ImVec2(bx + 12 * u, ry + 15 * u),
                     col::hint, true);
            char title[96];
            snprintf(title, sizeof(title), "Bank_%03d  %s", b.number, b.name.c_str());
            Text(dl, fonts.body, bx + 26 * u, ry + 6 * u, col::text, title);
            char count[48];
            int n = filtered ? static_cast<int>(r.sounds[row.bank].size()) : static_cast<int>(b.sounds.size());
            snprintf(count, sizeof(count), b.ok ? "%d sons" : "sem cabe\xC3\xA7" "alho", n);
            ImVec2 cs = TextSize(fonts.desc, count);
            Text(dl, fonts.desc, bx + bw - 14 * u - cs.x, ry + 8 * u, col::hint, count);
            std::string sub = b.text;
            if (!b.vehicleText.empty()) {
                sub += (sub.empty() ? "" : "  \xC2\xB7  ") + b.vehicleText;
            }
            if (!sub.empty()) {
                Text(dl, fonts.desc, bx + 26 * u, ry + 6 * u + fonts.body->FontSize + 4 * u, col::hint,
                     Ellipsize(fonts.desc, sub, bw - 40 * u).c_str());
            }
        } else {
            const sfx::Bank& b = banks[row.bank];
            const sfx::Sound& s = b.sounds[row.sound];
            float sx = rx + 36 * u, sw = rw - 36 * u, rh = row.h - 4 * u;
            bool playing = player::Playing() && player::Bank() == row.bank && player::Sound() == row.sound;
            bool selected = S.audioSelBank == row.bank && S.audioSelSound == row.sound;
            // Primeiro o download (a direita), depois o resto da linha (o clique na linha toca)
            snprintf(id, sizeof(id), "##wav%d_%d", row.bank, row.sound);
            float dlX = sx + sw - 20 * u;
            if (RoundButton(id, ImVec2(dlX, ry + rh * 0.5f), 13 * u, 2, false, IconDownload, "Salvar em .wav")) {
                SaveWav(row.bank, row.sound);
            }
            snprintf(id, sizeof(id), "##som%d_%d", row.bank, row.sound);
            ImGui::SetCursorScreenPos(ImVec2(sx, ry));
            if (ImGui::InvisibleButton(id, ImVec2(sw - 40 * u, rh))) {
                PlayOrStop(row.bank, row.sound);
            }
            bool hovered = ImGui::IsItemHovered();
            if (hovered) {
                Hand();
            }
            if (playing || selected || hovered) {
                Box(dl, ImVec2(sx, ry), ImVec2(sx + sw, ry + rh), col::White(playing ? 16 : selected ? 10 : 6),
                    playing ? col::White(60) : 0, 8 * u);
            }
            ImVec2 c(sx + 18 * u, ry + rh * 0.5f);
            dl->AddCircleFilled(c, 13 * u, playing ? col::strong : col::White(hovered ? 14 : 5), 32);
            if (!playing) {
                dl->AddCircle(c, 12.5f * u, col::White(hovered ? 70 : 34), 32, 1.0f);
            }
            if (playing) {
                IconStop(dl, c, 13 * u, col::ink);
            } else {
                IconPlay(dl, ImVec2(c.x + 1.5f * u, c.y), 13 * u, hovered ? col::text : col::column);
            }
            char label[24];
            snprintf(label, sizeof(label), "sound_%03d", row.sound + 1);
            float tx = sx + 42 * u, ty = ry + (rh - fonts.body->FontSize) * 0.5f;
            Text(dl, fonts.body, tx, ty, col::text, label);
            tx += TextSize(fonts.body, label).x + 10 * u;
            if (s.loop) {
                const char* tag = "loop";
                ImVec2 ts = TextSize(fonts.desc, tag);
                float th = ts.y + 6 * u, tyy = ry + (rh - th) * 0.5f;
                dl->AddRectFilled(ImVec2(tx, tyy), ImVec2(tx + ts.x + 16 * u, tyy + th), col::White(12), th * 0.5f);
                Text(dl, fonts.desc, tx + 8 * u, tyy + 3 * u - 0.5f * u, col::column, tag);
                tx += ts.x + 26 * u;
            }
            std::string extra = s.name;
            if (!s.weapons.empty()) {
                extra += (extra.empty() ? "" : "  \xC2\xB7  ") + s.weapons;
            }
            std::string dur = Seconds(s.seconds);
            ImVec2 ds = TextSize(fonts.desc, dur.c_str());
            float durX = dlX - 22 * u - ds.x;
            if (!extra.empty()) {
                Text(dl, fonts.desc, tx, ry + (rh - fonts.desc->FontSize) * 0.5f, col::hint,
                     Ellipsize(fonts.desc, extra, durX - tx - 12 * u).c_str());
            }
            Text(dl, fonts.desc, durX, ry + (rh - ds.y) * 0.5f, col::column, dur.c_str());
        }
    }
    if (rows.empty()) {
        const char* none = "Nenhum som com esse filtro.";
        ImVec2 ts = TextSize(fonts.desc, none);
        Text(dl, fonts.desc, origin.x + (w - ts.x) * 0.5f, origin.y + 40 * u, col::hint, none);
    }
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(rw, std::max(1.0f, total)));
    ImGui::EndChild();
}

// "GENRL > 90S_P > sound_001" com o chevron do lucide entre as partes (a fonte nao tem a seta); a parte que nao cabe
// encurta e as seguintes ficam de fora.
void Crumbs(ImDrawList* dl, ImFont* font, float x, float y, ImU32 color, ImU32 arrow, const std::vector<std::string>& parts,
            float maxW) {
    float end = x + maxW, iw = IconWidth(Icon::ChevronRight, true), gap = 6 * u;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            if (x + gap + iw + gap >= end) {
                return;
            }
            DrawIcon(dl, Icon::ChevronRight, ImVec2(x + gap + iw * 0.5f, y + font->FontSize * 0.5f), arrow, true);
            x += gap + iw + gap;
        }
        std::string text = Ellipsize(font, parts[i], end - x);
        Text(dl, font, x, y, color, text.c_str());
        if (text != parts[i]) {
            return;
        }
        x += TextSize(font, text.c_str()).x;
    }
}

// O player embaixo da arvore: o som escolhido, tocar/parar, repetir, salvar e o progresso.
void AudioPlayerBar(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    Box(dl, ImVec2(x, y), ImVec2(x + w, y + h), col::White(5), col::White(12), 10 * u);
    int bank = S.audioSelBank, sound = S.audioSelSound;
    const sfx::Bank* b = sfx::FindBank(bank);
    const sfx::Sound* s = sfx::FindSound(bank, sound);
    bool playing = player::Playing() && player::Bank() == bank && player::Sound() == sound;
    float r = 22 * u;
    ImVec2 c(x + 14 * u + r, y + h * 0.5f);
    if (RoundButton("##tocar", c, r, 1, false, playing ? IconStop : IconPlay, nullptr, s != nullptr)) {
        PlayOrStop(bank, sound);
    }
    float tx = c.x + r + 14 * u;
    float right = x + w - 14 * u;
    float br = 17 * u;
    ImVec2 saveC(right - br, y + h * 0.5f - 6 * u), repC(saveC.x - 2 * br - 10 * u, saveC.y);
    bool repeat = player::Repeat();
    if (RoundButton("##repetir", repC, br, 0, repeat, IconRepeat, repeat ? "Repetir: ligado" : "Repetir: desligado")) {
        player::SetRepeat(!repeat);
        S.audioRepeat = !repeat;
        SaveIni();
    }
    if (RoundButton("##salvar", saveC, br, 0, false, IconDownload, "Salvar em .wav", s != nullptr)) {
        SaveWav(bank, sound);
    }
    float textW = repC.x - br - 14 * u - tx;
    char title[128], sub[160];
    if (s && b) {
        snprintf(title, sizeof(title), "sound_%03d", sound + 1);
        snprintf(sub, sizeof(sub), "%s  \xC2\xB7  Bank_%03d  \xC2\xB7  %s Hz%s%s%s", sfx::Packs()[b->pack].name.c_str(), b->number,
                 Thousands(s->rate).c_str(), s->loop ? "  \xC2\xB7  loop" : "", s->name.empty() ? "" : "  \xC2\xB7  ",
                 s->name.c_str());
    } else {
        snprintf(title, sizeof(title), "Escolha um som na lista");
        snprintf(sub, sizeof(sub), "ele toca aqui; d\xC3\xA1 para repetir e salvar em .wav");
    }
    if (s && b) {
        Crumbs(dl, fonts.body, tx, y + 12 * u, col::text, col::hint, {b->name, title}, textW);
    } else {
        Text(dl, fonts.body, tx, y + 12 * u, col::column, Ellipsize(fonts.body, title, textW).c_str());
    }
    Text(dl, fonts.desc, tx, y + 12 * u + fonts.body->FontSize + 3 * u, col::hint, Ellipsize(fonts.desc, sub, textW).c_str());
    // progresso
    float pos = playing ? player::Position() : 0.0f, len = s ? s->seconds : 0.0f;
    char time[48];
    snprintf(time, sizeof(time), "%s / %s", Seconds(pos).c_str(), Seconds(len).c_str());
    ImVec2 ts = TextSize(fonts.desc, time);
    float barY = y + h - 16 * u, barX1 = right - ts.x - 12 * u;
    dl->AddRectFilled(ImVec2(tx, barY), ImVec2(barX1, barY + 4 * u), col::White(14), 2 * u);
    if (len > 0 && pos > 0) {
        float f = std::min(1.0f, pos / len);
        dl->AddRectFilled(ImVec2(tx, barY), ImVec2(tx + (barX1 - tx) * f, barY + 4 * u), col::strong, 2 * u);
    }
    Text(dl, fonts.desc, right - ts.x, barY - ts.y * 0.5f + 2 * u, col::column, time);
}

std::vector<std::string> PlayedTitle(const audiospy::Played& p) {
    const sfx::Bank* b = sfx::FindBank(p.bank);
    char buf[32];
    if (b) {
        snprintf(buf, sizeof(buf), "sound_%03d", p.sound + 1);
        return {sfx::Packs()[b->pack].name, b->name, buf};
    }
    snprintf(buf, sizeof(buf), "som %d", p.sound);
    return {"slot " + std::to_string(p.slot), buf};
}

std::string PlayedSub(const audiospy::Played& p) {
    const sfx::Bank* b = sfx::FindBank(p.bank);
    const sfx::Sound* s = sfx::FindSound(p.bank, p.sound);
    std::string out;
    char buf[96];
    if (b) {
        snprintf(buf, sizeof(buf), "Bank_%03d", b->number);
        out = buf;
    } else {
        out = "banco n\xC3\xA3o identificado";
    }
    if (s) {
        out += "  \xC2\xB7  " + Seconds(s->seconds) + (s->loop ? "  \xC2\xB7  loop" : "");
        if (!s->name.empty()) {
            out += "  \xC2\xB7  " + s->name;
        }
    }
    if (p.count > 1) {
        snprintf(buf, sizeof(buf), "  \xC2\xB7  \xC3\x97%d", p.count);
        out += buf;
    }
    return out;
}

// O lado direito: as opcoes do "audio tocando agora" e o historico.
void AudioSide(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    Text(dl, fonts.title, x, y, col::text, "\xC3\x81udio tocando agora");
    Text(dl, fonts.desc, x, y + fonts.title->FontSize + 4 * u, col::hint, "o que o jogo toca, com o banco e o som");
    float top = y + fonts.title->FontSize + fonts.desc->FontSize + 16 * u;
    ImGui::SetCursorScreenPos(ImVec2(x - 4 * u, top));
    float optH = 4 * (36 * u) + 8 * u;
    ImGui::BeginChild("##audioOpcoes", ImVec2(w + 8 * u, optH), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    Rows rows;
    Keys none;
    rows.Begin("audio", x, w, &S.audioRowSel, none, false);
    if (rows.Toggle("Mostrar na tela (menu fechado)", &S.audioShow)) {
        SaveIni();
    }
    if (rows.Stepper("Tempo na tela", &S.audioDelay, 1, 15, 1, "%d s", 4)) {
        SaveIni();
    }
    if (rows.Toggle("Ignorar os passos", &S.audioIgnoreFeet)) {
        audiospy::SetIgnoreFeet(S.audioIgnoreFeet);
        SaveIni();
    }
    if (rows.Action("Limpar o hist\xC3\xB3rico", "Limpar")) {
        audiospy::Clear();
    }
    rows.End();
    ImGui::EndChild();

    float listY = top + optH + 10 * u;
    const std::deque<audiospy::Played>& hist = audiospy::History();
    char title[64];
    snprintf(title, sizeof(title), "Hist\xC3\xB3rico (%d)", static_cast<int>(hist.size()));
    Text(dl, fonts.desc, x, listY, col::column, title);
    if (!audiospy::BanksKnown() && !hist.empty()) {
        const char* warn = "sem os nomes dos bancos";
        ImVec2 ws = TextSize(fonts.desc, warn);
        Text(dl, fonts.desc, x + w - ws.x, listY, col::Rgba(255, 196, 120), warn);
    }
    listY += fonts.desc->FontSize + 8 * u;
    ImGui::SetCursorScreenPos(ImVec2(x, listY));
    ImGui::BeginChild("##historico", ImVec2(w, y + h - listY), false, ImGuiWindowFlags_NoBackground);
    ImDrawList* cdl = ImGui::GetWindowDrawList();
    ImVec2 o = ImGui::GetCursorScreenPos();
    float rowH = 48 * u, iw = w - 12 * u;
    float scroll = ImGui::GetScrollY(), viewH = y + h - listY;
    for (size_t i = 0; i < hist.size(); ++i) {
        float ry = o.y + i * rowH;
        if (i * rowH + rowH < scroll || i * rowH > scroll + viewH) {
            continue;
        }
        const audiospy::Played& p = hist[i];
        char id[24];
        snprintf(id, sizeof(id), "##hist%d", static_cast<int>(i));
        ImGui::SetCursorScreenPos(ImVec2(o.x, ry));
        if (ImGui::InvisibleButton(id, ImVec2(iw, rowH - 4 * u)) && p.bank >= 0) {
            RevealSound(p.bank, p.sound);
            PlayOrStop(p.bank, p.sound);
        }
        bool hovered = ImGui::IsItemHovered();
        if (hovered && p.bank >= 0) {
            Hand();
            Tooltip("Clique: achar na lista e tocar");
        }
        bool sel = p.bank >= 0 && p.bank == S.audioSelBank && p.sound == S.audioSelSound;
        Box(cdl, ImVec2(o.x, ry), ImVec2(o.x + iw, ry + rowH - 4 * u), col::White(sel ? 14 : hovered ? 9 : 4),
            col::White(sel ? 50 : 8), 7 * u);
        char clock[16];
        snprintf(clock, sizeof(clock), "%02d:%02d:%02d", p.time.wHour, p.time.wMinute, p.time.wSecond);
        ImVec2 cs = TextSize(fonts.desc, clock);
        Text(cdl, fonts.desc, o.x + iw - 10 * u - cs.x, ry + 7 * u, col::hint, clock);
        Crumbs(cdl, fonts.body, o.x + 10 * u, ry + 6 * u, col::text, col::hint, PlayedTitle(p), iw - cs.x - 30 * u);
        Text(cdl, fonts.desc, o.x + 10 * u, ry + 6 * u + fonts.body->FontSize + 3 * u, col::hint,
             Ellipsize(fonts.desc, PlayedSub(p), iw - 20 * u).c_str());
    }
    if (hist.empty()) {
        const char* none = "Nada ainda: os sons que o jogo tocar aparecem aqui.";
        Text(cdl, fonts.desc, o.x + 4 * u, o.y + 6 * u, col::hint, none);
    }
    ImGui::SetCursorScreenPos(o);
    ImGui::Dummy(ImVec2(iw, std::max(1.0f, hist.size() * rowH)));
    ImGui::EndChild();
}

void TabAudio(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (!sfx::Ready()) {
        ImVec2 c(x + w * 0.5f, y + h * 0.45f);
        Spinner(dl, c, 12 * u, static_cast<float>(ImGui::GetTime()));
        char text[128];
        snprintf(text, sizeof(text), "Lendo os sons: %s", sfx::Status());
        ImVec2 ts = TextSize(fonts.desc, text);
        Text(dl, fonts.desc, c.x - ts.x * 0.5f, c.y + 22 * u, col::hint, text);
        return;
    }
    // Barra: busca, tipo, veiculo, arma, loop e o "info do audio" na direita
    float barH = 32 * u, searchW = std::min(340 * u, w * 0.3f);
    SearchField("##buscaAudio", S.audioQuery, sizeof(S.audioQuery), "Buscar: ricochete, sultan, m4, 137", x, y, searchW, barH);
    S.audioFilter.query = S.audioQuery;
    float px = x + searchW + 14 * u, pw = 0, pillH = 28 * u, pillY = y + (barH - pillH) * 0.5f;
    struct Drop {
        const char* id;
        const char* popup;
        std::string label;
        bool on;
    };
    std::string kindLabel = S.audioFilter.kind < 0 ? "Tipo" : sfx::KindName(S.audioFilter.kind);
    std::string vehLabel = "Ve\xC3\xAD" "culo";
    for (const auto& v : AudioVehicles()) {
        if (v.second == S.audioFilter.vehicle) {
            vehLabel = v.first;
        }
    }
    int weaponCount = static_cast<int>(sizeof(audio_data::WEAPONS) / sizeof(audio_data::WEAPONS[0]));
    std::string weapLabel = S.audioFilter.weapon < 0 ? "Arma" : audio_data::WEAPONS[S.audioFilter.weapon].name;
    static const char* const loops[] = {"Loop", "S\xC3\xB3 com loop", "Sem loop"};
    Drop drops[] = {{"##tipoAudio", "##tipoPop", kindLabel, S.audioFilter.kind >= 0},
                    {"##veicAudio", "##veicPop", vehLabel, S.audioFilter.vehicle >= 0},
                    {"##armaAudio", "##armaPop", weapLabel, S.audioFilter.weapon >= 0},
                    {"##loopAudio", nullptr, loops[S.audioFilter.loop], S.audioFilter.loop != 0}};
    float popupX[3] = {0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        if (Pill(drops[i].id, drops[i].label.c_str(), drops[i].on, px, pillY, pillH, &pw)) {
            if (drops[i].popup) {
                ImGui::OpenPopup(drops[i].popup);
            } else {
                S.audioFilter.loop = (S.audioFilter.loop + 1) % 3;
            }
        }
        if (i < 3) {
            popupX[i] = px;
        }
        px += pw + 6 * u;
    }
    float popupY = pillY + pillH + 4 * u;
    {
        std::vector<std::string> labels = {"Todos os tipos"};
        std::vector<int> values = {-1};
        for (int k = 0; k < sfx::KIND_COUNT; ++k) {
            labels.push_back(sfx::KindName(k));
            values.push_back(k);
        }
        ChoicePopup("##tipoPop", labels, values, &S.audioFilter.kind, popupX[0], popupY);
    }
    {
        std::vector<std::string> labels = {"Todos os ve\xC3\xAD" "culos"};
        std::vector<int> values = {-1};
        for (const auto& v : AudioVehicles()) {
            labels.push_back(v.first);
            values.push_back(v.second);
        }
        ChoicePopup("##veicPop", labels, values, &S.audioFilter.vehicle, popupX[1], popupY);
    }
    {
        std::vector<std::string> labels = {"Todas as armas"};
        std::vector<int> values = {-1};
        for (int i = 0; i < weaponCount; ++i) {
            labels.push_back(audio_data::WEAPONS[i].name);
            values.push_back(i);
        }
        ChoicePopup("##armaPop", labels, values, &S.audioFilter.weapon, popupX[2], popupY);
    }
    const char* show = "Info do \xC3\xA1udio";
    float showW = TextSize(fonts.desc, show).x + 24 * u;
    if (Pill("##mostrarAudio", show, S.audioShow, x + w - showW, pillY, pillH, nullptr)) {
        S.audioShow = !S.audioShow;
        SaveIni();
        Toast("Trok DevTools", S.audioShow ? "Info do \xC3\xA1udio ligada: feche o menu e os sons aparecem na tela."
                                           : "Info do \xC3\xA1udio desligada.");
    }
    if (ImGui::IsItemHovered()) {
        Tooltip("Com o menu fechado, mostra o banco e o som de cada \xC3\xA1udio que o jogo toca.");
    }

    // Filtro (so quando muda)
    if (!(S.audioApplied == S.audioFilter) || !S.audioHasResult) {
        S.audioResult = sfx::Apply(S.audioFilter);
        S.audioHasResult = true;
        bool wasActive = !S.audioApplied.query.empty() || S.audioApplied.kind >= 0 || S.audioApplied.vehicle >= 0 ||
                         S.audioApplied.weapon >= 0 || S.audioApplied.loop != 0;
        S.audioApplied = S.audioFilter;
        // Com filtro: abre os pacotes com resultado (e os bancos, se forem poucos sons); sem filtro: fecha tudo
        S.audioOpenPack.assign(sfx::Packs().size(), false);
        S.audioOpenBank.assign(sfx::Banks().size(), false);
        if (AudioFilterActive()) {
            for (size_t p = 0; p < sfx::Packs().size(); ++p) {
                S.audioOpenPack[p] = S.audioResult.bankCount[p] > 0;
            }
            if (S.audioResult.total <= 120) {
                for (size_t b = 0; b < sfx::Banks().size(); ++b) {
                    S.audioOpenBank[b] = !S.audioResult.sounds[b].empty();
                }
            }
        } else if (wasActive && S.audioSelBank >= 0) {
            RevealSound(S.audioSelBank, S.audioSelSound);
        }
    }

    float statusY = y + barH + 10 * u;
    char status[200];
    snprintf(status, sizeof(status), "%d pacotes  \xC2\xB7  %s banks  \xC2\xB7  %s sons", static_cast<int>(sfx::Packs().size()),
             Thousands(sfx::BankCount()).c_str(), Thousands(sfx::SoundCount()).c_str());
    std::string line = status;
    if (AudioFilterActive()) {
        snprintf(status, sizeof(status), "  \xC2\xB7  %s no filtro", Thousands(S.audioResult.total).c_str());
        line += status;
    }
    if (sfx::Failed() > 0) {
        snprintf(status, sizeof(status), "  \xC2\xB7  %d bancos sem cabe\xC3\xA7" "alho", sfx::Failed());
        line += status;
    }
    Text(dl, fonts.desc, x + 2 * u, statusY, col::hint, line.c_str());

    float bodyY = statusY + fonts.desc->FontSize + 10 * u, bodyH = y + h - bodyY;
    float leftW = std::floor(w * 0.62f), gap = 18 * u, rightX = x + leftW + gap, rightW = w - leftW - gap;
    float playerH = 74 * u;
    AudioTree(x, bodyY, leftW, bodyH - playerH - 10 * u);
    AudioPlayerBar(x, bodyY + bodyH - playerH, leftW, playerH);
    dl->AddLine(ImVec2(x + leftW + gap * 0.5f, bodyY), ImVec2(x + leftW + gap * 0.5f, bodyY + bodyH), col::separator);
    AudioSide(rightX, bodyY, rightW, bodyH);
}

// Com o menu fechado: os ultimos sons, um cartao cada, a direita da tela; somem depois do tempo escolhido.
constexpr int OVERLAY_CARDS = 3; // so os ultimos na tela: o jogo toca muitos sons, e a lista toda fica no historico

bool AudioOverlayActive() {
    if (!S.audioShow || audiospy::History().empty()) {
        return false;
    }
    return GetTickCount() - audiospy::History().front().last < static_cast<DWORD>(S.audioDelay) * 1000u + 500u;
}

void AudioOverlay() {
    if (!AudioOverlayActive()) {
        return;
    }
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 screen = ImGui::GetIO().DisplaySize;
    DWORD now = GetTickCount(), life = static_cast<DWORD>(S.audioDelay) * 1000u;
    float cardW = 360 * u, cardH = 46 * u, gap = 6 * u;
    float x = screen.x - cardW - 24 * u, y = screen.y * 0.36f;
    int shown = 0;
    for (const audiospy::Played& p : audiospy::History()) {
        DWORD age = now - p.last;
        if (shown >= OVERLAY_CARDS || age > life + 500) {
            break;
        }
        float alpha = age <= life ? 1.0f : 1.0f - (age - life) / 500.0f;
        ImU32 a = static_cast<ImU32>(alpha * 236);
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cardW, y + cardH), IM_COL32(12, 12, 12, a), 9 * u);
        dl->AddRect(ImVec2(x, y), ImVec2(x + cardW, y + cardH), col::White(static_cast<int>(16 * alpha)), 9 * u);
        auto fade = [&](ImU32 c) { return (c & 0x00FFFFFF) | (static_cast<ImU32>(((c >> 24) & 0xFF) * alpha) << 24); };
        Crumbs(dl, fonts.body, x + 12 * u, y + 7 * u, fade(col::text), fade(col::hint), PlayedTitle(p), cardW - 24 * u);
        Text(dl, fonts.desc, x + 12 * u, y + 7 * u + fonts.body->FontSize + 3 * u, fade(col::hint),
             Ellipsize(fonts.desc, PlayedSub(p), cardW - 24 * u).c_str());
        y += cardH + gap;
        ++shown;
    }
}

// ---------------------------------------------------------------- aba Animacoes

constexpr float ANIM_SPEEDS[] = {0.25f, 0.5f, 1.0f, 2.0f};
constexpr int ANIM_SPEED_COUNT = 4;

// O selo de uma animacao (ou de um .ifp, com quantas): MOD cheio, NOVA so com a borda. Devolve a largura.
float TagBadge(ImDrawList* dl, float x, float y, int tag, int count, bool draw) {
    char label[24];
    snprintf(label, sizeof(label), count > 0 ? "%s %d" : "%s", tag == ifp::TAG_MOD ? "MOD" : "NOVA", count);
    ImVec2 ts = TextSize(fonts.desc, label);
    float w = ts.x + 12 * u, h = ts.y + 6 * u;
    if (!draw) {
        return w;
    }
    if (tag == ifp::TAG_MOD) {
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), col::strong, 5 * u);
        Text(dl, fonts.desc, x + 6 * u, y + 3 * u - 0.5f * u, col::ink, label);
    } else {
        Box(dl, ImVec2(x, y), ImVec2(x + w, y + h), 0, col::White(90), 5 * u);
        Text(dl, fonts.desc, x + 6 * u, y + 3 * u - 0.5f * u, col::text, label);
    }
    return w;
}

// Texto com quebra de linha nas palavras. Devolve a altura.
float Wrapped(ImDrawList* dl, ImFont* font, float x, float y, float w, ImU32 color, const std::string& text) {
    float lineH = font->FontSize + 4 * u, cy = y;
    std::string line;
    size_t i = 0;
    while (i <= text.size()) {
        size_t j = text.find(' ', i);
        if (j == std::string::npos) {
            j = text.size();
        }
        std::string word = text.substr(i, j - i);
        std::string next = line.empty() ? word : line + " " + word;
        if (!line.empty() && TextSize(font, next.c_str()).x > w) {
            Text(dl, font, x, cy, color, line.c_str());
            cy += lineH;
            line = word;
        } else {
            line = next;
        }
        i = j + 1;
    }
    if (!line.empty()) {
        Text(dl, font, x, cy, color, line.c_str());
        cy += lineH;
    }
    return cy - y;
}

bool AnimFilterActive() {
    const ifp::Filter& f = S.animFilter;
    return !f.query.empty() || f.file >= 0 || f.changed;
}

// O personagem da previa: o escolhido ou o do jogador (o CJ e montado pelo jogo com as roupas: no lugar dele, o
// primeiro personagem comum do catalogo).
int AnimSkin() {
    if (S.animSkin >= 0) {
        return S.animSkin;
    }
    int skin = core::PlayerSkin();
    if (skin > 0) {
        return skin;
    }
    for (const catalog::Entry& e : catalog::All()) {
        if (e.kind == catalog::K_PED && e.id > 0 && !e.noFile) {
            return e.id;
        }
    }
    return -1;
}

void SelectAnim(int anim) {
    S.animSel = anim;
    S.animTime = 0;
    S.animPlaying = true;
    S.clipOk = ifp::LoadClip(anim, &S.clip);
}

void RevealAnim(int anim) {
    const ifp::Anim* a = ifp::FindAnim(anim);
    if (!a) {
        return;
    }
    S.animOpen.resize(ifp::Files().size(), false);
    S.animOpen[a->file] = true;
    S.animReveal = true;
}

std::string Kilobytes(DWORD bytes) {
    return bytes >= 1024 * 1024 ? Decimal(bytes / 1048576.0f, 1) + " MB" : Thousands(static_cast<int>((bytes + 1023) / 1024)) + " KB";
}

struct AnimRow {
    int type; // 0 .ifp, 1 animacao
    int file, anim;
    float y, h;
};

// A lista: .ifp > animacoes. order = as animacoes na ordem da tela (para as setas).
void AnimTree(float x, float y, float w, float h, std::vector<int>* order) {
    const std::vector<ifp::File>& files = ifp::Files();
    const std::vector<ifp::Anim>& anims = ifp::Anims();
    S.animOpen.resize(files.size(), false);
    bool filtered = AnimFilterActive();
    const ifp::Result& r = S.animResult;
    std::vector<AnimRow> rows;
    float total = 0, revealY = -1;
    for (int f = 0; f < static_cast<int>(files.size()); ++f) {
        if (filtered && (f >= static_cast<int>(r.anims.size()) || r.anims[f].empty())) {
            continue;
        }
        rows.push_back({0, f, -1, total, 58 * u});
        total += 58 * u;
        if (!S.animOpen[f]) {
            continue;
        }
        const std::vector<int>& list = filtered ? r.anims[f] : files[f].anims;
        for (int a : list) {
            if (S.animReveal && a == S.animSel) {
                revealY = total;
            }
            rows.push_back({1, f, a, total, 34 * u});
            order->push_back(a);
            total += 34 * u;
        }
        total += 6 * u;
    }

    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::BeginChild("##animArvore", ImVec2(w, h), false, ImGuiWindowFlags_NoBackground);
    if (S.animReveal) {
        if (revealY >= 0) {
            float scroll = ImGui::GetScrollY();
            if (revealY < scroll || revealY + 34 * u > scroll + h) {
                ImGui::SetScrollY(std::max(0.0f, revealY - h * 0.4f));
            }
        }
        S.animReveal = false;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float scroll = ImGui::GetScrollY();
    float rw = w - 14 * u;
    int playing = core::PlayingAnim();
    for (const AnimRow& row : rows) {
        if (row.y + row.h < scroll - 40 * u || row.y > scroll + h + 40 * u) {
            continue;
        }
        float rx = origin.x, ry = origin.y + row.y;
        char id[32];
        if (row.type == 0) {
            const ifp::File& file = files[row.file];
            snprintf(id, sizeof(id), "##ifp%d", row.file);
            ImGui::SetCursorScreenPos(ImVec2(rx, ry));
            if (ImGui::InvisibleButton(id, ImVec2(rw, row.h - 4 * u))) {
                S.animOpen[row.file] = !S.animOpen[row.file];
            }
            bool hovered = ImGui::IsItemHovered();
            if (hovered) {
                Hand();
            }
            Box(dl, ImVec2(rx, ry), ImVec2(rx + rw, ry + row.h - 4 * u), col::White(hovered ? 9 : 5), col::White(10), 8 * u);
            DrawIcon(dl, S.animOpen[row.file] ? Icon::ChevronDown : Icon::ChevronRight, ImVec2(rx + 16 * u, ry + 18 * u),
                     col::column, true);
            Text(dl, fonts.title, rx + 30 * u, ry + 8 * u, col::text, file.lib.c_str());
            char count[64];
            int n = filtered ? static_cast<int>(r.anims[row.file].size()) : static_cast<int>(file.anims.size());
            snprintf(count, sizeof(count), n == 1 ? "%s anima\xC3\xA7\xC3\xA3o" : "%s anima\xC3\xA7\xC3\xB5" "es",
                     Thousands(n).c_str());
            ImVec2 cs = TextSize(fonts.desc, count);
            float right = rx + rw - 14 * u - cs.x;
            Text(dl, fonts.desc, right, ry + 11 * u, col::hint, count);
            float ty = ry + 9 * u;
            if (file.news > 0) {
                right -= TagBadge(dl, 0, 0, ifp::TAG_NEW, file.news, false) + 8 * u;
                TagBadge(dl, right, ty, ifp::TAG_NEW, file.news, true);
            }
            if (file.mods > 0) {
                right -= TagBadge(dl, 0, 0, ifp::TAG_MOD, file.mods, false) + 8 * u;
                TagBadge(dl, right, ty, ifp::TAG_MOD, file.mods, true);
            }
            std::vector<std::string> path = {file.source.substr(file.source.find('\\') + 1)};
            if (path[0] != file.entry) {
                path.push_back(file.entry);
            }
            float subY = ry + 8 * u + fonts.title->FontSize + 5 * u;
            Crumbs(dl, fonts.desc, rx + 30 * u, subY, col::column, col::hint, path, rw * 0.5f);
            std::string extra = Kilobytes(file.bytes);
            ImU32 extraColor = col::hint;
            if (file.broken) {
                extra = "n\xC3\xA3o deu para ler at\xC3\xA9 o fim";
                extraColor = col::Rgba(255, 196, 120);
            } else if (file.renamed) {
                extra = "nome interno: " + file.block;
                extraColor = col::Rgba(255, 196, 120);
            }
            ImVec2 es = TextSize(fonts.desc, extra.c_str());
            Text(dl, fonts.desc, rx + rw - 14 * u - es.x, subY, extraColor, extra.c_str());
        } else {
            const ifp::Anim& a = anims[row.anim];
            snprintf(id, sizeof(id), "##anim%d", row.anim);
            float bx = rx + 18 * u, bw = rw - 18 * u, bh = row.h - 2 * u;
            ImGui::SetCursorScreenPos(ImVec2(bx, ry));
            if (ImGui::InvisibleButton(id, ImVec2(bw, bh))) {
                SelectAnim(row.anim);
            }
            bool hovered = ImGui::IsItemHovered();
            bool selected = row.anim == S.animSel;
            if (hovered) {
                Hand();
            }
            if (selected || hovered) {
                Box(dl, ImVec2(bx, ry), ImVec2(bx + bw, ry + bh), col::White(selected ? 14 : 6), selected ? col::White(50) : 0,
                    7 * u);
            }
            float tx = bx + 12 * u;
            if (row.anim == playing) {
                // No personagem agora
                dl->AddCircleFilled(ImVec2(tx + 3 * u, ry + bh * 0.5f), 3.5f * u, col::strong, 12);
                tx += 14 * u;
            }
            std::string dur = Seconds(a.seconds);
            ImVec2 ds = TextSize(fonts.desc, dur.c_str());
            float right = bx + bw - 12 * u - ds.x;
            Text(dl, fonts.desc, right, ry + (bh - ds.y) * 0.5f, col::column, dur.c_str());
            if (a.tag != ifp::TAG_NONE) {
                right -= TagBadge(dl, 0, 0, a.tag, 0, false) + 10 * u;
                TagBadge(dl, right, ry + (bh - fonts.desc->FontSize - 6 * u) * 0.5f, a.tag, 0, true);
            }
            Text(dl, fonts.body, tx, ry + (bh - fonts.body->FontSize) * 0.5f, selected ? col::text : col::column,
                 Ellipsize(fonts.body, a.name, right - tx - 12 * u).c_str());
        }
    }
    if (rows.empty()) {
        const char* none = "Nenhuma anima\xC3\xA7\xC3\xA3o com esse filtro.";
        ImVec2 ts = TextSize(fonts.desc, none);
        Text(dl, fonts.desc, origin.x + (w - ts.x) * 0.5f, origin.y + 40 * u, col::hint, none);
    }
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(rw, std::max(1.0f, total)));
    ImGui::EndChild();
}

void IconPause(ImDrawList* dl, ImVec2 c, float s, ImU32 color) {
    float bw = s * 0.2f, bh = s * 0.36f, gap = s * 0.12f;
    dl->AddRectFilled(ImVec2(c.x - gap - bw, c.y - bh), ImVec2(c.x - gap, c.y + bh), color, 1.0f);
    dl->AddRectFilled(ImVec2(c.x + gap, c.y - bh), ImVec2(c.x + gap + bw, c.y + bh), color, 1.0f);
}

// O personagem da previa, na pose do instante, com as opcoes embaixo (como o visualizador do card).
void AnimViewer(ImVec2 a, float side) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 b(a.x + side, a.y + side);
    ImGuiIO& io = ImGui::GetIO();
    int skin = AnimSkin();
    if (skin >= 0) {
        core::PreviewPed(skin);
    }
    const mesh::Mesh* m = skin >= 0 ? core::PreviewMesh() : nullptr;
    float dur = S.clipOk ? std::max(S.clip.seconds, 0.001f) : 0.0f;
    if (S.clipOk && S.animPlaying && !S.animScrub) {
        S.animTime += io.DeltaTime * ANIM_SPEEDS[S.animSpeed];
        if (S.animTime >= dur) {
            if (S.animLoop) {
                S.animTime = std::fmod(S.animTime, dur);
            } else {
                S.animTime = dur;
                S.animPlaying = false;
            }
        }
    }
    if (!m) {
        dl->AddRectFilled(a, b, col::Rgba(20, 20, 20), 8 * u);
        ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
        std::string why;
        if (skin < 0 || core::PreviewFailed(&why)) {
            std::string text = skin < 0 ? "Nenhum personagem para a pr\xC3\xA9via" : "Sem pr\xC3\xA9via: " + why;
            ImVec2 ts = TextSize(fonts.desc, text.c_str());
            Text(dl, fonts.desc, c.x - ts.x * 0.5f, c.y - ts.y * 0.5f, col::hint, text.c_str());
        } else {
            Spinner(dl, c, 12 * u, static_cast<float>(ImGui::GetTime()));
        }
    } else {
        static const ifp::Clip none;
        pose::Apply(*m, S.clipOk ? S.clip : none, S.animTime, S.animInPlace, &S.posed);
        ImGui::SetCursorScreenPos(a);
        ImGui::InvisibleButton("##animVisual", ImVec2(side, side - 46 * u));
        bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f)) {
            S.animView.yaw -= io.MouseDelta.x * 0.4f;
            S.animView.pitch = std::max(-85.0f, std::min(85.0f, S.animView.pitch + io.MouseDelta.y * 0.4f));
            S.animSpin = false;
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }
        if (hovered && io.MouseWheel != 0.0f) {
            S.animView.zoom = std::max(0.3f, std::min(12.0f, S.animView.zoom * (io.MouseWheel > 0 ? 1.12f : 1 / 1.12f)));
        }
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            S.animView.yaw = render::View().yaw;
            S.animView.pitch = render::View().pitch;
            S.animView.zoom = 1.0f;
        }
        if (S.animSpin) {
            S.animView.yaw = std::fmod(S.animView.yaw - io.DeltaTime * 18.0f, 360.0f);
        }
        // Texturas, arame, grade e fundo sao os mesmos do visualizador do card
        S.animView.textures = S.view.textures;
        S.animView.wire = S.view.wire;
        S.animView.grid = S.view.grid;
        S.animView.collision = false;
        S.animView.background = S.view.background;
        int target = side * 1.5f > 1024 ? 2048 : 1024;
        ImTextureID tex = render::DrawViewer(S.posed, S.animView, target);
        if (tex) {
            dl->AddImageRounded(tex, a, b, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, 8 * u);
        } else {
            dl->AddRectFilled(a, b, col::Rgba(20, 20, 20), 8 * u);
        }
        if (S.clipOk && pose::Matched(*m, S.clip) == 0) {
            const char* warn = "Esta anima\xC3\xA7\xC3\xA3o n\xC3\xA3o mexe nos ossos de um personagem";
            ImVec2 ts = TextSize(fonts.desc, warn);
            Text(dl, fonts.desc, (a.x + b.x - ts.x) * 0.5f, a.y + 54 * u, col::Rgba(255, 196, 120), warn);
        }
    }
    dl->AddRect(a, b, col::White(14), 8 * u);

    // O personagem, no canto de cima
    std::string skinLabel = "Personagem: ";
    const catalog::Entry* e = skin >= 0 ? catalog::Find(skin) : nullptr;
    char num[16];
    snprintf(num, sizeof(num), "%d", skin);
    skinLabel += skin < 0 ? "nenhum" : std::string(num) + (e && !e->dff.empty() ? "  " + e->dff : "");
    if (S.animSkin < 0) {
        skinLabel += "  (o seu)";
    }
    float pillW = 0;
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    if (Pill("##animSkin", skinLabel.c_str(), false, a.x + 10 * u, a.y + 10 * u, 26 * u, &pillW)) {
        ImGui::OpenPopup("##skinPop");
    }
    if (ImGui::IsItemHovered()) {
        Tooltip("Trocar o personagem da pr\xC3\xA9via");
    }
    dl->ChannelsSetCurrent(0); // a faixa escura atras, como a das opcoes: legivel em qualquer fundo
    dl->AddRectFilled(ImVec2(a.x + 4 * u, a.y + 4 * u), ImVec2(a.x + 16 * u + pillW, a.y + 42 * u), IM_COL32(0, 0, 0, 120), 19 * u);
    dl->ChannelsMerge();
    {
        std::vector<std::string> labels = {"O do seu personagem"};
        std::vector<int> values = {-1};
        for (const catalog::Entry& p : catalog::All()) {
            if (p.kind == catalog::K_PED && p.id > 0 && !p.noFile) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%d  %s", p.id, p.dff.c_str());
                labels.push_back(buf);
                values.push_back(p.id);
            }
        }
        int before = S.animSkin;
        ChoicePopup("##skinPop", labels, values, &S.animSkin, a.x + 10 * u, a.y + 40 * u);
        if (S.animSkin != before) {
            SaveIni();
        }
    }

    // Opcoes, no canto de baixo
    struct Opt {
        const char* id;
        const char* label;
        bool* value;
    } opts[] = {{"##aTex", "Texturas", &S.view.textures},
                {"##aWire", "Arame", &S.view.wire},
                {"##aGrid", "Grade", &S.view.grid},
                {"##aSpin", "Girar", &S.animSpin},
                {"##aLugar", "No lugar", &S.animInPlace}};
    float ox = a.x + 10 * u, oy = b.y - 36 * u, pw = 0;
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    for (Opt& o : opts) {
        if (Pill(o.id, o.label, *o.value, ox, oy, 26 * u, &pw)) {
            *o.value = !*o.value;
            SaveIni();
        }
        if (o.value == &S.animInPlace && ImGui::IsItemHovered()) {
            Tooltip("Andar e correr sem sair do lugar (a raiz n\xC3\xA3o anda no ch\xC3\xA3o)");
        }
        ox += pw + 6 * u;
    }
    ox = BackgroundSwatch(dl, ox + 4 * u, oy) + 12 * u;
    dl->ChannelsSetCurrent(0);
    dl->AddRectFilled(ImVec2(a.x + 4 * u, oy - 6 * u), ImVec2(ox, oy + 32 * u), IM_COL32(0, 0, 0, 120), 19 * u);
    dl->ChannelsMerge();
}

// Embaixo do visualizador: tocar/pausar, a linha do tempo (arrastar = ir para o instante), o tempo, a velocidade e repetir.
void AnimTimeline(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    Box(dl, ImVec2(x, y), ImVec2(x + w, y + h), col::White(5), col::White(12), 10 * u);
    float dur = S.clipOk ? std::max(S.clip.seconds, 0.001f) : 0.0f;
    float r = 18 * u;
    ImVec2 c(x + 12 * u + r, y + h * 0.5f);
    if (RoundButton("##animTocar", c, r, 1, false, S.animPlaying ? IconPause : IconPlay, nullptr, S.clipOk)) {
        if (!S.animPlaying && S.animTime >= dur) {
            S.animTime = 0;
        }
        S.animPlaying = !S.animPlaying;
    }
    float right = x + w - 12 * u;
    float br = 15 * u;
    ImVec2 repC(right - br, y + h * 0.5f);
    if (RoundButton("##animRepetir", repC, br, 0, S.animLoop, IconRepeat, S.animLoop ? "Repetir: ligado" : "Repetir: desligado")) {
        S.animLoop = !S.animLoop;
        SaveIni();
    }
    char speed[16];
    snprintf(speed, sizeof(speed), "%s\xC3\x97", Decimal(ANIM_SPEEDS[S.animSpeed], S.animSpeed < 1 ? 2 : S.animSpeed < 2 ? 1 : 0).c_str());
    float sw = TextSize(fonts.desc, speed).x + 24 * u;
    float sx = repC.x - br - 10 * u - sw;
    if (Pill("##animVelocidade", speed, S.animSpeed != 2, sx, y + (h - 26 * u) * 0.5f, 26 * u, nullptr)) {
        S.animSpeed = (S.animSpeed + 1) % ANIM_SPEED_COUNT;
        SaveIni();
    }
    if (ImGui::IsItemHovered()) {
        Tooltip("Velocidade da pr\xC3\xA9via");
    }
    char time[48];
    snprintf(time, sizeof(time), "%s / %s", Seconds(S.animTime).c_str(), Seconds(dur).c_str());
    ImVec2 ts = TextSize(fonts.desc, time);
    float timeX = sx - 14 * u - ts.x;
    Text(dl, fonts.desc, timeX, y + (h - ts.y) * 0.5f, col::column, time);
    // A linha do tempo
    float bx0 = c.x + r + 16 * u, bx1 = timeX - 16 * u, by = y + h * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(bx0 - 6 * u, by - 10 * u));
    ImGui::InvisibleButton("##animLinha", ImVec2(std::max(1.0f, bx1 - bx0 + 12 * u), 20 * u));
    bool active = ImGui::IsItemActive() && S.clipOk;
    S.animScrub = active;
    if (active) {
        float f = std::max(0.0f, std::min(1.0f, (ImGui::GetIO().MousePos.x - bx0) / std::max(1.0f, bx1 - bx0)));
        S.animTime = f * dur;
    }
    if (ImGui::IsItemHovered() && S.clipOk) {
        Hand();
    }
    dl->AddRectFilled(ImVec2(bx0, by - 2 * u), ImVec2(bx1, by + 2 * u), col::White(14), 2 * u);
    if (dur > 0) {
        float f = std::min(1.0f, S.animTime / dur);
        float kx = bx0 + (bx1 - bx0) * f;
        dl->AddRectFilled(ImVec2(bx0, by - 2 * u), ImVec2(kx, by + 2 * u), col::strong, 2 * u);
        dl->AddCircleFilled(ImVec2(kx, by), (active ? 7.0f : 6.0f) * u, col::strong, 20);
    }
}

// O lado direito: a animacao escolhida, de onde vem, as marcas e o que fazer com ela.
void AnimInfo(float x, float y, float w, float h) {
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::BeginChild("##animInfo", ImVec2(w, h), false, ImGuiWindowFlags_NoBackground);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float cy = y;
    const ifp::Anim* a = ifp::FindAnim(S.animSel);
    const ifp::File* f = a ? ifp::FindFile(a->file) : nullptr;
    if (!a || !f) {
        Text(dl, fonts.title, x, cy, col::text, "Escolha uma anima\xC3\xA7\xC3\xA3o");
        cy += fonts.title->FontSize + 6 * u;
        cy += Wrapped(dl, fonts.desc, x, cy, w, col::hint,
                      "Ela toca no personagem do visualizador. D\xC3\xA1 para tocar no seu personagem tamb\xC3\xA9m, e "
                      "copiar o nome para um script.");
    } else {
        Text(dl, fonts.title, x, cy, col::text, Ellipsize(fonts.title, a->name, w).c_str());
        cy += fonts.title->FontSize + 6 * u;
        Crumbs(dl, fonts.desc, x, cy, col::column, col::hint, {f->source.substr(f->source.find('\\') + 1), f->entry}, w);
        cy += fonts.desc->FontSize + 12 * u;
        // A marca, explicada
        const char* tagText = nullptr;
        bool hasRef = ifp::Reference()[0] != 0;
        if (a->tag == ifp::TAG_MOD) {
            tagText = "Diferente da original: algum mod mexeu nesta anima\xC3\xA7\xC3\xA3o.";
        } else if (a->tag == ifp::TAG_NEW) {
            tagText = f->known ? "N\xC3\xA3o existe no jogo original: um mod adicionou." : "Este .ifp n\xC3\xA3o \xC3\xA9 do jogo original.";
        } else if (hasRef) {
            tagText = "Igual \xC3\xA0 original.";
        }
        if (tagText) {
            float tx = x;
            if (a->tag != ifp::TAG_NONE) {
                tx += TagBadge(dl, x, cy - 1 * u, a->tag, 0, true) + 8 * u;
            }
            cy += Wrapped(dl, fonts.desc, tx, cy + 1 * u, x + w - tx, a->tag == ifp::TAG_NONE ? col::column : col::text,
                          tagText) + 8 * u;
        }
        // Os dados
        struct Row {
            const char* label;
            std::string value;
        };
        char buf[96];
        std::vector<Row> rows;
        rows.push_back({"Dura\xC3\xA7\xC3\xA3o", Seconds(a->seconds)});
        snprintf(buf, sizeof(buf), "%d", a->tracks);
        std::string bones = buf;
        if (const mesh::Mesh* m = core::PreviewMesh()) {
            if (S.clipOk && S.clip.anim == a->index) {
                snprintf(buf, sizeof(buf), "  (%d no personagem)", pose::Matched(*m, S.clip));
                bones += buf;
            }
        }
        rows.push_back({"Ossos", bones});
        snprintf(buf, sizeof(buf), "%d", a->frames);
        rows.push_back({"Quadros-chave", buf});
        rows.push_back({"Formato", f->format + (a->compressed ? " comprimida" : "")});
        rows.push_back({"Tamanho", Thousands(static_cast<int>(a->size)) + " bytes"});
        rows.push_back({"Anda", a->moves ? "sim (a raiz sai do lugar)" : "n\xC3\xA3o"});
        if (a->sampId > 0) {
            snprintf(buf, sizeof(buf), "%d", a->sampId);
            rows.push_back({"\xC3\x8Dndice no SA-MP", buf});
        }
        if (f->renamed) {
            rows.push_back({"Nome interno", f->block});
        }
        float labelW = 0;
        for (const Row& r : rows) {
            labelW = std::max(labelW, TextSize(fonts.desc, r.label).x);
        }
        for (const Row& r : rows) {
            Text(dl, fonts.desc, x, cy, col::hint, r.label);
            Text(dl, fonts.desc, x + labelW + 16 * u, cy, col::text, Ellipsize(fonts.desc, r.value, w - labelW - 16 * u).c_str());
            cy += fonts.desc->FontSize + 8 * u;
        }
        cy += 8 * u;
        // Acoes
        float bh = 34 * u;
        if (Button("##animNoPersonagem", "Tocar no meu personagem", x, cy, w, bh, true)) {
            core::PlayAnim(a->index, S.animLoop, !S.animInPlace);
        }
        if (ImGui::IsItemHovered()) {
            Tooltip(S.animLoop ? "Repetindo, como na pr\xC3\xA9via. Parar: o bot\xC3\xA3o abaixo."
                               : "Uma vez, como na pr\xC3\xA9via (repetir: o bot\xC3\xA3o da linha do tempo).");
        }
        cy += bh + 8 * u;
        float half = (w - 8 * u) * 0.5f;
        bool playing = core::PlayingAnim() >= 0;
        if (Button("##animParar", "Parar no personagem", x, cy, w, bh, false, playing)) {
            core::StopAnim();
        }
        cy += bh + 8 * u;
        if (Button("##animCopiarNome", "Copiar nome", x, cy, half, bh, false)) {
            std::string text = "\"" + f->lib + "\", \"" + a->name + "\"";
            ImGui::SetClipboardText(text.c_str());
            Toast("Copiado", text.c_str());
        }
        if (Button("##animCopiarSamp", "Copiar ApplyAnimation", x + half + 8 * u, cy, half, bh, false)) {
            bool move = !S.animInPlace;
            snprintf(buf, sizeof(buf), "ApplyAnimation(playerid, \"%s\", \"%s\", 4.1, %d, %d, %d, 0, 0, 1);", f->lib.c_str(),
                     a->name.c_str(), S.animLoop ? 1 : 0, move ? 1 : 0, move ? 1 : 0);
            ImGui::SetClipboardText(buf);
            Toast("Copiado", buf);
        }
        cy += bh + 16 * u;
        // Avisos do .ifp
        if (f->renamed) {
            snprintf(buf, sizeof(buf), "O nome interno deste .ifp \xC3\xA9 \"%s\": o jogo guarda as anima\xC3\xA7\xC3\xB5" "es com esse "
                                       "nome, e os scripts que usam \"%s\" n\xC3\xA3o acham.",
                     f->block.c_str(), f->lib.c_str());
            cy += Wrapped(dl, fonts.desc, x, cy, w, col::Rgba(255, 196, 120), buf) + 6 * u;
        }
        if (!f->missing.empty()) {
            std::string text = "Faltam " + Thousands(static_cast<int>(f->missing.size())) + " originais neste .ifp: ";
            for (size_t i = 0; i < f->missing.size() && i < 6; ++i) {
                text += (i ? ", " : "") + f->missing[i];
            }
            if (f->missing.size() > 6) {
                text += "...";
            }
            cy += Wrapped(dl, fonts.desc, x, cy, w, col::Rgba(255, 196, 120), text) + 6 * u;
        }
    }
    // A referencia das originais
    if (!ifp::Reference()[0]) {
        cy += 6 * u;
        cy += Wrapped(dl, fonts.desc, x, cy, w, col::hint,
                      "Sem refer\xC3\xAAncia das originais: s\xC3\xB3 as novas s\xC3\xA3o marcadas (pelo nome). Para "
                      "marcar tamb\xC3\xA9m as modificadas, copie o anim.img e o ped.ifp de um jogo sem mods para a pasta "
                      "Trok DevTools\\originais.");
    }
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::Dummy(ImVec2(w, std::max(1.0f, cy - y + 8 * u)));
    ImGui::EndChild();
}

void TabAnims(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (!ifp::Ready()) {
        ImVec2 c(x + w * 0.5f, y + h * 0.45f);
        Spinner(dl, c, 12 * u, static_cast<float>(ImGui::GetTime()));
        char text[128];
        snprintf(text, sizeof(text), "Lendo as anima\xC3\xA7\xC3\xB5" "es: %s", ifp::Status());
        ImVec2 ts = TextSize(fonts.desc, text);
        Text(dl, fonts.desc, c.x - ts.x * 0.5f, c.y + 22 * u, col::hint, text);
        return;
    }
    std::string message;
    bool ok = false;
    if (core::TakeAnimMessage(&message, &ok)) {
        Toast(ok ? "No seu personagem" : "Trok DevTools", message.c_str());
    }
    // Barra: busca, o .ifp e so as modificadas
    float barH = 32 * u, searchW = std::min(340 * u, w * 0.3f);
    SearchField("##buscaAnim", S.animQuery, sizeof(S.animQuery), "Buscar: walk, dance, sprint, ped", x, y, searchW, barH);
    S.animFilter.query = S.animQuery;
    float px = x + searchW + 14 * u, pw = 0, pillH = 28 * u, pillY = y + (barH - pillH) * 0.5f;
    const std::vector<ifp::File>& files = ifp::Files();
    const ifp::File* chosen = ifp::FindFile(S.animFilter.file);
    std::string fileLabel = chosen ? chosen->entry : "Todos os .ifp";
    if (Pill("##animIfp", fileLabel.c_str(), chosen != nullptr, px, pillY, pillH, &pw)) {
        ImGui::OpenPopup("##animIfpPop");
    }
    float popupX = px;
    px += pw + 6 * u;
    if (Pill("##animMudadas", "S\xC3\xB3 modificadas e novas", S.animFilter.changed, px, pillY, pillH, &pw)) {
        S.animFilter.changed = !S.animFilter.changed;
    }
    {
        std::vector<std::string> labels = {"Todos os .ifp"};
        std::vector<int> values = {-1};
        for (size_t i = 0; i < files.size(); ++i) {
            std::string label = files[i].entry;
            if (files[i].mods + files[i].news > 0) {
                label += "  \xC2\xB7  " + Thousands(files[i].mods + files[i].news) + " marcadas";
            }
            labels.push_back(label);
            values.push_back(static_cast<int>(i));
        }
        ChoicePopup("##animIfpPop", labels, values, &S.animFilter.file, popupX, pillY + pillH + 4 * u);
    }

    // Filtro (so quando muda)
    if (!(S.animApplied == S.animFilter) || !S.animHasResult) {
        S.animResult = ifp::Apply(S.animFilter);
        S.animHasResult = true;
        S.animApplied = S.animFilter;
        S.animOpen.assign(files.size(), false);
        if (AnimFilterActive()) {
            for (size_t f = 0; f < files.size(); ++f) {
                S.animOpen[f] = !S.animResult.anims[f].empty() && (S.animResult.files <= 6 || S.animResult.total <= 200);
            }
        }
        if (S.animSel >= 0) {
            RevealAnim(S.animSel);
        }
    }

    float statusY = y + barH + 10 * u;
    char status[256];
    snprintf(status, sizeof(status), "%d .ifp  \xC2\xB7  %s anima\xC3\xA7\xC3\xB5" "es  \xC2\xB7  %s modificadas  \xC2\xB7  %s novas",
             static_cast<int>(files.size()), Thousands(static_cast<int>(ifp::Anims().size())).c_str(),
             Thousands(ifp::ModCount()).c_str(), Thousands(ifp::NewCount()).c_str());
    std::string line = status;
    if (AnimFilterActive()) {
        line += "  \xC2\xB7  " + Thousands(S.animResult.total) + " no filtro";
    }
    line += ifp::Reference()[0] ? std::string("  \xC2\xB7  refer\xC3\xAAncia: ") + ifp::Reference()
                                : "  \xC2\xB7  sem refer\xC3\xAAncia das originais";
    Text(dl, fonts.desc, x + 2 * u, statusY, col::hint, Ellipsize(fonts.desc, line, w).c_str());

    float bodyY = statusY + fonts.desc->FontSize + 10 * u, bodyH = y + h - bodyY;
    float listW = std::floor(std::max(360 * u, w * 0.33f)), gap = 18 * u;
    float infoW = std::floor(std::max(260 * u, w * 0.2f));
    float timelineH = 52 * u;
    float viewX = x + listW + gap;
    float side = std::floor(std::min(w - listW - infoW - 2 * gap, bodyH - timelineH - 10 * u));
    std::vector<int> order;
    AnimTree(x, bodyY, listW, bodyH, &order);
    AnimViewer(ImVec2(viewX, bodyY), side);
    AnimTimeline(viewX, bodyY + side + 10 * u, side, timelineH);
    float infoX = viewX + side + gap;
    AnimInfo(infoX, bodyY, x + w - infoX, bodyH);

    // Setas: a animacao de cima/de baixo na lista; espaco: tocar/pausar
    if (!ImGui::GetIO().WantTextInput && ImGui::GetCurrentContext()->OpenPopupStack.Size == 0 && !order.empty()) {
        bool up = ImGui::IsKeyPressed(ImGuiKey_UpArrow, true), down = ImGui::IsKeyPressed(ImGuiKey_DownArrow, true);
        if (up || down) {
            auto it = std::find(order.begin(), order.end(), S.animSel);
            int i = it == order.end() ? (down ? -1 : static_cast<int>(order.size())) : static_cast<int>(it - order.begin());
            i = std::max(0, std::min(static_cast<int>(order.size()) - 1, i + (down ? 1 : -1)));
            SelectAnim(order[i]);
            RevealAnim(order[i]);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && S.clipOk) {
            if (!S.animPlaying && S.animTime >= S.clip.seconds) {
                S.animTime = 0;
            }
            S.animPlaying = !S.animPlaying;
        }
    }
    dl->AddLine(ImVec2(x + listW + gap * 0.5f, bodyY), ImVec2(x + listW + gap * 0.5f, bodyY + bodyH), col::separator);
}

// ---------------------------------------------------------------- outras abas

void Placeholder(float x, float y, float w, float h, const char* title, const char* const* lines, int count) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float blockH = 30 * u + count * (fonts.desc->FontSize + 8 * u);
    float top = y + (h - blockH) * 0.4f;
    ImVec2 ts = TextSize(fonts.title, title);
    Text(dl, fonts.title, x + (w - ts.x) * 0.5f, top, col::text, title);
    const char* soon = "Em breve";
    ImVec2 ss = TextSize(fonts.desc, soon);
    float by = top + ts.y + 10 * u;
    dl->AddRectFilled(ImVec2(x + (w - ss.x) * 0.5f - 10 * u, by), ImVec2(x + (w + ss.x) * 0.5f + 10 * u, by + ss.y + 8 * u),
                      col::White(10), (ss.y + 8 * u) * 0.5f);
    Text(dl, fonts.desc, x + (w - ss.x) * 0.5f, by + 4 * u, col::column, soon);
    float ly = by + ss.y + 26 * u;
    for (int i = 0; i < count; ++i) {
        ImVec2 ls = TextSize(fonts.desc, lines[i]);
        Text(dl, fonts.desc, x + (w - ls.x) * 0.5f, ly, col::hint, lines[i]);
        ly += fonts.desc->FontSize + 8 * u;
    }
}

void TabGeneral(Rows& rows) {
    rows.Section("Informa\xC3\xA7\xC3\xB5" "es ao apontar");
    if (rows.Toggle("Mostrar o modelo do que est\xC3\xA1 na mira", &S.inspect)) {
        core::SetInspect(S.inspect);
        SaveIni();
    }
    rows.Info("Como funciona", "Com o menu fechado: id, nome, TXD, posi\xC3\xA7\xC3\xA3o e interior do que est\xC3\xA1 no "
                               "centro da tela, com a caixa dele.\nBot\xC3\xA3o do meio do mouse: abre o card.");
    rows.Section("Visualizador");
    ImU32 bg = S.background;
    if (rows.Color("Cor do fundo", &bg, BACKGROUND_DEFAULT)) {
        SetBackground(bg);
        SaveIni();
    }
    if (rows.Toggle("Grade no ch\xC3\xA3o", &S.view.grid)) {
        SaveIni();
    }
    rows.Section("\xC3\x81udio");
    if (rows.Toggle("Mostrar o \xC3\xA1udio tocando agora (menu fechado)", &S.audioShow)) {
        SaveIni();
    }
    if (rows.Stepper("Tempo na tela", &S.audioDelay, 1, 15, 1, "%d s", 4)) {
        SaveIni();
    }
    rows.Section("Miniaturas");
    if (rows.Stepper("Modelos carregados por quadro", &S.perFrame, 1, 8, 1, "%d", 3)) {
        core::SetThumbsPerFrame(S.perFrame);
        SaveIni();
    }
    char count[32];
    snprintf(count, sizeof(count), "%d", render::ThumbCount());
    rows.Value("Miniaturas prontas", count);
    if (rows.Action("Refazer as miniaturas", "Refazer")) {
        render::ForgetThumbs();
        Toast("Trok DevTools", "As miniaturas v\xC3\xA3o ser desenhadas de novo.");
    }
    rows.Section("Mundo");
    snprintf(count, sizeof(count), "%d", core::SpawnedCount());
    rows.Value("Modelos criados", count);
    if (rows.Action("Apagar os modelos criados", "Apagar")) {
        core::DespawnAll();
    }
    rows.Section("Sobre");
    rows.Value("Atalho", "/trokdevtools ou F10");
    rows.Value("Vers\xC3\xA3o", VERSION_TAG);
}

// ---------------------------------------------------------------- inspetor (menu fechado)

void Inspector() {
    const core::Look& look = core::Looking();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 screen = ImGui::GetIO().DisplaySize;
    ImVec2 c(screen.x * 0.5f, screen.y * 0.5f);
    dl->AddCircleFilled(c, 2.5f * u + 1, IM_COL32(0, 0, 0, 140));
    dl->AddCircleFilled(c, 2.0f * u, IM_COL32(255, 255, 255, 230));
    if (!look.valid) {
        return;
    }
    // A caixa na tela (para o cartao ficar ao lado dela, sem cobrir o objeto). Modelos grandes (bairros, terrenos)
    // passam por tras da camera: cada aresta e cortada no plano a kNear m da camera antes de ir para a tela, e com a camera
    // dentro da caixa ela nao e desenhada.
    bool boxOn = false;
    ImVec2 bmin(screen.x, screen.y), bmax(0, 0);
    if (look.hasBox) {
        const gta::Vec3* b = look.box;
        auto sub = [](const gta::Vec3& p, const gta::Vec3& q) { return gta::Vec3{p.x - q.x, p.y - q.y, p.z - q.z}; };
        auto dot = [](const gta::Vec3& p, const gta::Vec3& q) { return p.x * q.x + p.y * q.y + p.z * q.z; };
        gta::Vec3 rel = sub(look.camPos, b[0]), ax[3] = {sub(b[1], b[0]), sub(b[2], b[0]), sub(b[4], b[0])};
        bool inside = true;
        for (const gta::Vec3& a : ax) {
            float len2 = dot(a, a), t = len2 > 1e-6f ? dot(rel, a) / len2 : 0.0f;
            inside = inside && t > -0.02f && t < 1.02f;
        }
        const float kNear = 1.5f;
        static const int E[12][2] = {{0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7},
                                     {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (int k = 0; k < 12 && !inside; ++k) {
            gta::Vec3 p = b[E[k][0]], q = b[E[k][1]];
            float dp = dot(sub(p, look.camPos), look.camForward), dq = dot(sub(q, look.camPos), look.camForward);
            if (dp < kNear && dq < kNear) {
                continue;
            }
            if (dp < kNear || dq < kNear) {
                float t = (kNear - dp) / (dq - dp);
                gta::Vec3 c = {p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t, p.z + (q.z - p.z) * t};
                (dp < kNear ? p : q) = c;
            }
            ImVec2 sp, sq;
            if (!gta::Project(p, &sp.x, &sp.y) || !gta::Project(q, &sq.x, &sq.y)) {
                continue;
            }
            dl->AddLine(sp, sq, IM_COL32(255, 255, 255, 150), 1.2f);
            for (const ImVec2& v : {sp, sq}) {
                ImVec2 cv(std::max(0.0f, std::min(screen.x, v.x)), std::max(0.0f, std::min(screen.y, v.y)));
                bmin = ImVec2(std::min(bmin.x, cv.x), std::min(bmin.y, cv.y));
                bmax = ImVec2(std::max(bmax.x, cv.x), std::max(bmax.y, cv.y));
            }
            boxOn = true;
        }
    }
    const catalog::Entry* e = catalog::Find(look.model);
    static const char* const TYPES[] = {"?", "Pr\xC3\xA9" "dio", "Ve\xC3\xAD" "culo", "Personagem", "Objeto", "Dummy"};
    char title[96], line1[160], line2[160], line3[160], line4[96];
    snprintf(title, sizeof(title), "%d  \xC2\xB7  %s", look.model, e && !e->dff.empty() ? e->dff.c_str() : "?");
    snprintf(line1, sizeof(line1), "%s  \xC2\xB7  %s", TYPES[look.type < 6 ? look.type : 0],
             e && !e->imgName.empty() ? e->imgName.c_str() : "?");
    snprintf(line2, sizeof(line2), "TXD %s%s", e && !e->txd.empty() ? e->txd.c_str() : "?",
             e && e->replaced ? "  \xC2\xB7  substitu\xC3\xAD" "do" : "");
    snprintf(line3, sizeof(line3), "%s  %s  %s  \xC2\xB7  int %d  \xC2\xB7  %s m", Decimal(look.pos.x, 2).c_str(),
             Decimal(look.pos.y, 2).c_str(), Decimal(look.pos.z, 2).c_str(), look.area, Decimal(look.distance, 1).c_str());
    if (look.lodModel >= 0) {
        const catalog::Entry* lod = catalog::Find(look.lodModel);
        snprintf(line4, sizeof(line4), "LOD %d  %s", look.lodModel, lod ? lod->dff.c_str() : "");
    } else {
        line4[0] = 0;
    }
    const char* hint = "Bot\xC3\xA3o do meio: abrir o card";
    const char* lines[] = {line1, line2, line3, line4, hint};
    float w = TextSize(fonts.body, title).x;
    for (const char* l : lines) {
        w = std::max(w, TextSize(fonts.desc, l).x);
    }
    w += 24 * u;
    float lineH = fonts.desc->FontSize + 5 * u;
    float h = 14 * u + fonts.body->FontSize + 6 * u + (line4[0] ? 4 : 3) * lineH + 8 * u + lineH;
    ImVec2 a(c.x + 26 * u, c.y + 22 * u);
    if (boxOn) {
        // Do lado direito da caixa; se nao couber, do esquerdo; objeto enorme (predio na cara): perto do centro
        float gap = 14 * u, margin = 10 * u;
        bool beside = true;
        if (bmax.x + gap + w <= screen.x - margin) {
            a.x = bmax.x + gap;
        } else if (bmin.x - gap - w >= margin) {
            a.x = bmin.x - gap - w;
        } else {
            beside = false;
        }
        if (beside) {
            a.y = std::max(margin, (bmin.y + bmax.y) * 0.5f - h * 0.5f);
        }
    }
    a.x = std::max(10 * u, std::min(a.x, screen.x - w - 10 * u));
    a.y = std::max(10 * u, std::min(a.y, screen.y - h - 10 * u));
    dl->AddRectFilled(a, ImVec2(a.x + w, a.y + h), IM_COL32(12, 12, 12, 236), 9 * u);
    dl->AddRect(a, ImVec2(a.x + w, a.y + h), col::White(16), 9 * u);
    float y = a.y + 10 * u;
    Text(dl, fonts.body, a.x + 12 * u, y, col::text, title);
    y += fonts.body->FontSize + 6 * u;
    for (int i = 0; i < 4; ++i) {
        if (!lines[i][0]) {
            continue;
        }
        Text(dl, fonts.desc, a.x + 12 * u, y, i == 1 && e && e->replaced ? col::Rgba(255, 196, 120) : col::column,
             lines[i]);
        y += lineH;
    }
    y += 6 * u;
    dl->AddLine(ImVec2(a.x + 12 * u, y - 3 * u), ImVec2(a.x + w - 12 * u, y - 3 * u), col::separator);
    Text(dl, fonts.desc, a.x + 12 * u, y + 2 * u, col::hint, hint);
}

} // namespace

void Init(const char* iniPath, const char* gameDir) {
    snprintf(S.ini, sizeof(S.ini), "%s", iniPath ? iniPath : "");
    snprintf(S.gameDir, sizeof(S.gameDir), "%s", gameDir ? gameDir : "");
    if (S.ini[0]) {
        for (const IniFlag& f : IniFlags()) {
            *f.value = GetPrivateProfileIntA("devtools", f.key, *f.value ? 1 : 0, S.ini) != 0;
        }
        char hex[16] = "";
        GetPrivateProfileStringA("devtools", "visual_fundo", "", hex, sizeof(hex), S.ini);
        if (strlen(hex) == 6) {
            unsigned long rgb = strtoul(hex, nullptr, 16);
            SetBackground(IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 255));
        }
        S.audioDelay = std::max(1, std::min(15, static_cast<int>(GetPrivateProfileIntA("devtools", "audio_tempo", 4, S.ini))));
        S.animSkin = std::max(-1, std::min(19999, static_cast<int>(GetPrivateProfileIntA("devtools", "anim_personagem", -1, S.ini))));
        S.animSpeed = std::max(0, std::min(3, static_cast<int>(GetPrivateProfileIntA("devtools", "anim_velocidade", 2, S.ini))));
        S.perFrame = std::max(1, std::min(8, static_cast<int>(GetPrivateProfileIntA("devtools", "miniaturas_por_quadro", 3,
                                                                                     S.ini))));
    }
    core::SetGameDir(S.gameDir);
    core::SetInspect(S.inspect);
    core::SetThumbsPerFrame(S.perFrame);
    audiospy::SetIgnoreFeet(S.audioIgnoreFeet);
    player::SetRepeat(S.audioRepeat);
}

void Toggle() {
    S.open = !S.open;
    S.age = 0;
    if (!S.open) {
        core::SetBrowsing(false);
        player::Stop(); // o player e do menu
        if (S.cardOpen) {
            S.cardOpen = false;
            core::Close();
        }
    }
}

bool CapturesInput() {
    return S.open;
}

bool WantsFrame() {
    return S.open || core::InspectOn() || ToastsVisible() || AudioOverlayActive();
}

const char* Version() {
    return VERSION_TAG;
}

void Frame() {
    ImGuiIO& io = ImGui::GetIO();
    if (!S.open) {
        core::SetBrowsing(false);
        if (core::InspectOn()) {
            PushStyle();
            ImGui::PushFont(fonts.body);
            Inspector();
            ImGui::PopFont();
            PopStyle();
            const core::Look& look = core::Looking();
            if (look.valid && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                Toggle();
                S.tab = TAB_MODELS;
                OpenCard(look.model);
            }
        }
        if (AudioOverlayActive()) {
            PushStyle();
            AudioOverlay();
            PopStyle();
        }
        DrawToasts();
        return;
    }
    player::Update();
    bool fresh = S.age++ == 0;
    bool keysOk = !fresh && !io.WantTextInput && !CapturingKey();
    bool escape = keysOk && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    // Popups abertos (o card tambem e um): o Esc de um seletor de cor fecha so o seletor.
    int popups = ImGui::GetCurrentContext()->OpenPopupStack.Size;

    float width = std::floor(std::max(760.0f, std::min(io.DisplaySize.x * 0.84f, 1560.0f)));
    float height = std::floor(std::max(520.0f, std::min(io.DisplaySize.y * 0.86f, 1000.0f)));
    PushStyle();
    bool keep = BeginShell("##trokDevTools", "Trok DevTools", VERSION_TAG, width, height, S.shell);
    ImVec2 pos = ImGui::GetWindowPos();
    float padX = 18 * u;
    float tabsW = std::min(width - 2 * padX, 760 * u);
    Tabs("##abas", TABS, TAB_COUNT, &S.tab, pos.x + (width - tabsW) * 0.5f, pos.y + HeaderHeight(), tabsW);

    float top = pos.y + HeaderHeight() + TabsHeight() + 14 * u;
    float bodyH = height - (top - pos.y) - FooterHeight() - 12 * u;
    float x = pos.x + padX, w = width - 2 * padX;
    if (S.tab != TAB_MODELS) {
        core::SetBrowsing(false);
    }
    switch (S.tab) {
    case TAB_MODELS: TabModels(x, top, w, bodyH); break;
    case 1: TabAnims(x, top, w, bodyH); break;
    case 2: TabAudio(x, top, w, bodyH); break;
    case 3: {
        const char* lines[] = {"Localizar qualquer arquivo dos .img e da pasta do jogo.",
                               "Ver de onde o jogo est\xC3\xA1 lendo cada coisa (original, outro .img ou mod)."};
        Placeholder(x, top, w, bodyH, "Arquivos", lines, 2);
        break;
    }
    default: {
        ImGui::SetCursorScreenPos(ImVec2(x - 4 * u, top));
        ImGui::BeginChild("##geral", ImVec2(w + 8 * u, bodyH), false, ImGuiWindowFlags_NoBackground);
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float rowW = std::min(ImGui::GetContentRegionAvail().x - 10 * u, 760 * u);
        float rx = cp.x + (w - rowW) * 0.5f;
        Keys keys = ReadKeys(keysOk && !S.cardOpen);
        Rows rows;
        if (keys.up && S.rowCount > 0) {
            S.selected = (S.selected + S.rowCount - 2) % S.rowCount + 1;
            rows.keyboardMoved = true;
        } else if (keys.down && S.rowCount > 0) {
            S.selected = S.selected % S.rowCount + 1;
            rows.keyboardMoved = true;
        }
        rows.Begin("geral", rx, rowW, &S.selected, keys, true);
        TabGeneral(rows);
        S.rowCount = rows.End();
        ImGui::Dummy(ImVec2(0, 6 * u));
        ImGui::EndChild();
        break;
    }
    }

    bool cardWasOpen = S.cardOpen;
    CardPopup(pos, ImVec2(width, height), escape && S.cardOpen && popups <= 1);

    static const std::vector<Hint> hints = {{"Clique", "Abrir o card", false}, {"/trokdevtools", "Abrir e fechar", false},
                                            {"Esc", "Fechar", true}};
    static const std::vector<Hint> audioHints = {{"Clique", "Tocar ou parar", false}, {"/trokdevtools", "Abrir e fechar", false},
                                                 {"Esc", "Fechar", true}};
    static const std::vector<Hint> animHints = {{"Clique", "Ver a anima\xC3\xA7\xC3\xA3o", false}, {"Setas", "Trocar", false},
                                                {"Esc", "Fechar", true}};
    int clicked = EndShell(S.tab == 2 ? audioHints : S.tab == 1 ? animHints : hints);
    PopStyle();
    if (!keep || clicked == 2 || (escape && !cardWasOpen && popups == 0)) {
        Toggle();
    }
    DrawToasts();
}

} // namespace devtools

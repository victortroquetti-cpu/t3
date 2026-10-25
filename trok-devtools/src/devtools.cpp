// Trok DevTools (.asi) -- Victor_Trok
// A grade e virtual: so os cards da tela sao desenhados e so eles pedem miniatura. O card aberto e um popup modal da
// casa, com o visualizador a esquerda (arraste para girar, roda para zoom, duplo clique para centralizar) e as
// informacoes e texturas a direita.

#include "devtools.h"

#include "catalog.h"
#include "core.h"
#include "render.h"
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

constexpr const char* VERSION_TAG = "v0.2";
// As texturas ficam no card do modelo (clique numa textura): nao tem aba propria.
const char* const TABS[] = {"Modelos", "Anima\xC3\xA7\xC3\xB5" "es", "\xC3\x81udios", "Arquivos", "Geral"};
constexpr int TAB_COUNT = 5, TAB_MODELS = 0, TAB_GENERAL = 4;

// Fundos do visualizador: os atalhos do card (o Geral aceita qualquer cor). Verde e azul servem de chroma key.
struct Swatch {
    ImU32 color;
    const char* name;
};
const Swatch BACKGROUNDS[] = {{IM_COL32(20, 20, 20, 255), "Escuro"},          {IM_COL32(60, 63, 68, 255), "Cinza"},
                              {IM_COL32(217, 217, 217, 255), "Claro"},       {IM_COL32(255, 255, 255, 255), "Branco"},
                              {IM_COL32(0, 177, 64, 255), "Verde (chroma)"}, {IM_COL32(0, 71, 187, 255), "Azul (chroma)"}};
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
            {"visual_grade", &S.view.grid},       {"visual_girar", &S.spin}};
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
    // Fundo: bolinhas de cor ao lado das opcoes
    ox += 8 * u;
    float r = 9 * u, cy = oy + 13 * u;
    for (int i = 0; i < static_cast<int>(sizeof(BACKGROUNDS) / sizeof(BACKGROUNDS[0])); ++i) {
        ImVec2 c(ox + r, cy);
        char id[16];
        snprintf(id, sizeof(id), "##fundo%d", i);
        ImGui::SetCursorScreenPos(ImVec2(c.x - r - 2 * u, c.y - r - 2 * u));
        if (ImGui::InvisibleButton(id, ImVec2(2 * r + 4 * u, 2 * r + 4 * u))) {
            SetBackground(BACKGROUNDS[i].color);
            SaveIni();
        }
        bool hovered = ImGui::IsItemHovered();
        bool chosen = (S.background | IM_COL32_A_MASK) == BACKGROUNDS[i].color;
        if (hovered) {
            Hand();
            Tooltip((std::string("Fundo: ") + BACKGROUNDS[i].name).c_str());
        }
        dl->AddCircleFilled(c, r, BACKGROUNDS[i].color, 24);
        dl->AddCircle(c, r, col::White(hovered ? 120 : 60), 24, 1.0f);
        if (chosen) {
            dl->AddCircle(c, r + 3 * u, col::strong, 24, 1.6f * u);
        }
        ox += 2 * r + 8 * u;
    }
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
        S.perFrame = std::max(1, std::min(8, static_cast<int>(GetPrivateProfileIntA("devtools", "miniaturas_por_quadro", 3,
                                                                                     S.ini))));
    }
    core::SetGameDir(S.gameDir);
    core::SetInspect(S.inspect);
    core::SetThumbsPerFrame(S.perFrame);
}

void Toggle() {
    S.open = !S.open;
    S.age = 0;
    if (!S.open) {
        core::SetBrowsing(false);
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
    return S.open || core::InspectOn() || ToastsVisible();
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
        DrawToasts();
        return;
    }
    bool fresh = S.age++ == 0;
    bool keysOk = !fresh && !io.WantTextInput && !CapturingKey();
    bool escape = keysOk && ImGui::IsKeyPressed(ImGuiKey_Escape, false);

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
    case 1: {
        const char* lines[] = {"Os .ifp e as anima\xC3\xA7\xC3\xB5" "es de cada um.",
                               "Pr\xC3\xA9via no personagem, com velocidade e repeti\xC3\xA7\xC3\xA3o."};
        Placeholder(x, top, w, bodyH, "Anima\xC3\xA7\xC3\xB5" "es", lines, 2);
        break;
    }
    case 2: {
        const char* lines[] = {"Os bancos de som e as r\xC3\xA1" "dios, com player e busca.",
                               "Tocar um som pelo id do banco e do slot."};
        Placeholder(x, top, w, bodyH, "\xC3\x81udios", lines, 2);
        break;
    }
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
    CardPopup(pos, ImVec2(width, height), escape && S.cardOpen);

    static const std::vector<Hint> hints = {{"Clique", "Abrir o card", false}, {"/trokdevtools", "Abrir e fechar", false},
                                            {"Esc", "Fechar", true}};
    int clicked = EndShell(hints);
    PopStyle();
    if (!keep || clicked == 2 || (escape && !cardWasOpen)) {
        Toggle();
    }
    DrawToasts();
}

} // namespace devtools

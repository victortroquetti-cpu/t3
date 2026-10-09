// Trok Shadows Menu (.asi) -- Victor_Trok
// O menu das sombras: tudo do shadows.ini do Shadows Extender, com o jogo aberto. As barras param em limites
// saudaveis (nada de resolucao que derruba o FPS); as predefinicoes ajustam qualidade e distancia de uma vez e o
// "Peso estimado" mostra quanto a placa vai trabalhar. Cada mudanca vale na hora e e gravada no shadows.ini.

#include "menu.h"
#include "settings.h"
#include "trok_ui.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace menu {
namespace {

using namespace tui;

constexpr const char* VERSION_TAG = "v1.0";

struct State {
    bool open = false;
    int tab = 0;
    int selected[5] = {1, 1, 1, 1, 1};
    int rowCount[5] = {1, 1, 1, 1, 1};
    Shell shell;
    // Quadros desde que a tela abriu: no primeiro o teclado e ignorado (o Enter do /sombras nao aciona nada).
    int menuAge = 0;
    bool confirm = false;
    int confirmAge = 0;
    bool dirty = false; // mudou e ainda nao gravou
    double lastChange = 0;
    bool restartWarned = false;
} S;

const char* const TABS[] = {"Geral", "Sombra", "Ve\xC3\xAD" "culos", "Stencil", "Avan\xC3\xA7" "ado"};
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

// ---------------------------------------------------------------- qualidade

// Resolucao da sombra (RasterSize) de 128 a 1024 px; as outras tres ficam na metade, como no shadows.ini que vem com
// o Shadows Extender (7/6/6/6). 2048 fica de fora: com 16 sombras e mais de meio giga de memoria de video.
const char* const RESOLUTIONS[] = {"128", "256", "512", "1024"};

int ResolutionIndex(const Settings& s) {
    return std::max(0, std::min(3, s.raster - 7));
}

void SetResolution(Settings& s, int index) {
    s.raster = 7 + index;
    s.blurRaster = s.raster2 = s.blurRaster2 = s.raster - 1;
}

struct Preset {
    const char* name;
    int resolution; // indice em RESOLUTIONS
    int blur;
    float realtimeDistance;
    float stencilDistance;
};

const Preset PRESETS[] = {
    {"Leve", 0, 2, 30.0f, 60.0f},
    {"Equilibrado", 1, 4, 50.0f, 100.0f},
    {"Bonito", 2, 6, 80.0f, 150.0f},
};
constexpr int PRESET_COUNT = 3;
const char* const PRESET_NAMES[] = {"Leve", "Equilibrado", "Bonito", "Personalizado"};

int PresetOf(const Settings& s) {
    for (int i = 0; i < PRESET_COUNT; i++) {
        const Preset& p = PRESETS[i];
        Settings t = s;
        SetResolution(t, p.resolution);
        if (t.raster == s.raster && t.blurRaster == s.blurRaster && t.raster2 == s.raster2 &&
            t.blurRaster2 == s.blurRaster2 && s.blurLevel == p.blur && s.blur1 && s.blur2 &&
            std::fabs(s.realtimeDistance - p.realtimeDistance) < 0.5f &&
            std::fabs(s.stencilDistance - p.stencilDistance) < 0.5f) {
            return i;
        }
    }
    return PRESET_COUNT; // personalizado
}

void ApplyPreset(Settings& s, int index) {
    if (index < 0 || index >= PRESET_COUNT) {
        return;
    }
    const Preset& p = PRESETS[index];
    SetResolution(s, p.resolution);
    s.blur1 = true;
    s.blur2 = true;
    s.blurLevel = p.blur;
    s.realtimeDistance = p.realtimeDistance;
    s.stencilDistance = p.stencilDistance;
}

// Trabalho da placa por quadro com 8 sombras em tempo real: desenhar o dono na camera da sombra (R x R), reduzir,
// desfocar n vezes e o degrade (B x B cada). Em escala log: o "Leve" fica perto de 25% e 1024 com desfoque 8 em 90%.
float Weight(const Settings& s, const char** label) {
    const double r = std::pow(2.0, s.raster), b = std::pow(2.0, s.blurRaster);
    const double perShadow = r * r + (2.0 + s.blurLevel + (s.blur2 ? 1.0 : 0.0)) * b * b;
    const double total = perShadow * 8.0;
    float f = static_cast<float>(std::log(total / 50000.0) / std::log(60e6 / 50000.0));
    f = std::max(0.02f, std::min(1.0f, f));
    *label = f < 0.33f ? "Leve" : f < 0.6f ? "M\xC3\xA9" "dio" : f < 0.82f ? "Pesado" : "Muito pesado";
    return f;
}

// ---------------------------------------------------------------- linhas com conversao

bool SliderF(Rows& r, const char* label, float* value, int min, int max, int step, const char* fmt, float def,
             float scale = 1.0f) {
    int v = std::max(min, std::min(max, static_cast<int>(std::lround(*value * scale))));
    const int d = static_cast<int>(std::lround(def * scale));
    if (r.Slider(label, &v, min, max, step, fmt, d)) {
        *value = v / scale;
        return true;
    }
    return false;
}

// Forca de 0 a 255 mostrada em porcentagem.
bool Strength(Rows& r, const char* label, int* value, int def) {
    int p = static_cast<int>(std::lround(*value * 100.0 / 255.0));
    const int d = static_cast<int>(std::lround(def * 100.0 / 255.0));
    if (r.Slider(label, &p, 0, 100, 1, "%d%%", d)) {
        *value = static_cast<int>(std::lround(p * 255.0 / 100.0));
        return true;
    }
    return false;
}

bool ColorRgb(Rows& r, const char* label, int* rgb, const int* def) {
    ImU32 c = IM_COL32(rgb[0], rgb[1], rgb[2], 255);
    const ImU32 d = IM_COL32(def[0], def[1], def[2], 255);
    if (r.Color(label, &c, d)) {
        rgb[0] = (c >> IM_COL32_R_SHIFT) & 0xFF;
        rgb[1] = (c >> IM_COL32_G_SHIFT) & 0xFF;
        rgb[2] = (c >> IM_COL32_B_SHIFT) & 0xFF;
        return true;
    }
    return false;
}

// ShadowSunZLimit e a altura minima do sol (o z do vetor ate ele): mostrada em graus.
bool SunHeight(Rows& r, float* z, float def) {
    auto degrees = [](float v) { return static_cast<int>(std::lround(std::asin(std::max(0.0f, std::min(1.0f, v))) * 57.2958f)); };
    int deg = std::max(10, std::min(80, degrees(*z)));
    if (r.Slider("Altura m\xC3\xADnima do sol", &deg, 10, 80, 1, "%d\xC2\xB0", degrees(def))) {
        *z = std::sin(deg / 57.2958f);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- abas

void TabGeneral(Rows& r, Settings& s, const Settings& d) {
    r.Section("Qualidade");
    int preset = PresetOf(s);
    if (r.Dropdown("Predefini\xC3\xA7\xC3\xA3o", &preset, PRESET_NAMES, PRESET_COUNT + 1, 1)) {
        ApplyPreset(s, preset);
    }
    const char* weightLabel = "";
    const float weight = Weight(s, &weightLabel);
    char label[64];
    snprintf(label, sizeof(label), "Peso estimado  {9A9A9A}%s", weightLabel);
    r.Progress(label, weight);
    bool blur = !s.combine;
    if (r.Toggle("Sombra desfocada (blur)", &blur)) {
        s.combine = !blur;
    }
    SliderF(r, "Dist\xC3\xA2ncia (pessoas e ve\xC3\xAD" "culos)", &s.realtimeDistance, 10, 120, 5, "%d m",
            d.realtimeDistance);
    SliderF(r, "Dist\xC3\xA2ncia (pr\xC3\xA9" "dios e objetos)", &s.stencilDistance, 20, 250, 10, "%d m",
            d.stencilDistance);

    r.Section("Agora");
    char now[32];
    snprintf(now, sizeof(now), "%d de 16", backend::ActiveShadows());
    static const int kinds[] = {0};
    const char* badge[] = {now};
    r.Badges("Sombras em tempo real em uso", badge, kinds, 1);
    if (backend::RestartPending()) {
        r.Info("Algumas mudan\xC3\xA7" "as valem ao abrir o jogo de novo",
               "A quantidade de sombras dos pr\xC3\xA9" "dios, o shader e alguns liga/desliga s\xC3\xB3 mudam quando o "
               "jogo abre de novo. J\xC3\xA1 ficaram gravados no shadows.ini.");
    }
}

void TabShadow(Rows& r, Settings& s, const Settings& d) {
    r.Section("Qualidade");
    int res = ResolutionIndex(s);
    if (r.Segmented("Resolu\xC3\xA7\xC3\xA3o", &res, RESOLUTIONS, 4, ResolutionIndex(d))) {
        SetResolution(s, res);
    }
    r.Stepper("Desfoque", &s.blurLevel, 0, 8, 1, "%d", d.blurLevel);
    r.Toggle("Degrad\xC3\xAA nas bordas", &s.blur2);
    r.Slider("Degrad\xC3\xAA m\xC3\xA1ximo", &s.gradientMax, 0, 255, 5, "%d", d.gradientMax);
    r.Slider("Degrad\xC3\xAA m\xC3\xADnimo", &s.gradientMin, 0, 255, 5, "%d", d.gradientMin);

    if (s.combine) {
        // Junto do stencil a sombra em tempo real so marca o stencil: a cor e a forca sao as do stencil.
        r.Section("Cor (junto com a dos pr\xC3\xA9" "dios)");
        ColorRgb(r, "Cor", s.stencilColor, d.stencilColor);
        Strength(r, "For\xC3\xA7" "a", &s.stencilColor[3], d.stencilColor[3]);
    } else {
        r.Section("Cor (sombra desfocada)");
        ColorRgb(r, "Cor", s.realtimeColor, d.realtimeColor);
        Strength(r, "For\xC3\xA7" "a", &s.realtimeColor[3], d.realtimeColor[3]);
    }
    SliderF(r, "For\xC3\xA7" "a \xC3\xA0 noite", &s.night, 0, 100, 5, "%d%%", d.night, 100.0f);
    SliderF(r, "For\xC3\xA7" "a com nuvens", &s.clouds, 0, 100, 5, "%d%%", d.clouds, 100.0f);
    SunHeight(r, &s.sunZ, d.sunZ);
}

void TabVehicles(Rows& r, Settings& s) {
    r.Toggle("Quem est\xC3\xA1 no ve\xC3\xAD" "culo entra na sombra dele", &s.fixOccupants);
    if (!backend::FixInstalled()) {
        r.Info("A corre\xC3\xA7\xC3\xA3o n\xC3\xA3o p\xC3\xB4" "de ser ligada",
               "Outro mod mexeu nos mesmos lugares do jogo que o Shadows Extender. Veja o Trok Shadows Menu.log.");
    }
    r.Info("Por que isso existe",
           "Sem isso, piloto e moto t\xC3\xAAm duas sombras e onde elas se cruzam fica mais escuro. Com isso, a "
           "sombra do ve\xC3\xAD" "culo j\xC3\xA1 inclui quem est\xC3\xA1 dentro.");
    r.Toggle("Sombra simples junto com a em tempo real", &s.vehicleDefaultWithRealtime);
    r.Toggle("Sem sombra simples nos ve\xC3\xAD" "culos", &s.disableVehicleDefault);
    r.Toggle("Todos os jogadores (SA-MP)", &s.morePlayers);
    r.Toggle("Com gr\xC3\xA1" "ficos no baixo", &s.realtimeLow);
}

void TabStencil(Rows& r, Settings& s, const Settings& d) {
    r.Slider("Quantidade  {9A9A9A}(ao abrir o jogo)", &s.maxShadows, 64, 1024, 32, "%d", d.maxShadows);
    SliderF(r, "Dist\xC3\xA2ncia", &s.stencilDistance, 20, 250, 10, "%d m", d.stencilDistance);
    r.Toggle("Todos os objetos a cada quadro", &s.flagIgnoreSome);
    r.Toggle("Sem sombra de pr\xC3\xA9" "dios e objetos", &s.disableBuildings);
    r.Toggle("Com gr\xC3\xA1" "ficos no baixo", &s.stencilLow);
    ColorRgb(r, "Cor", s.stencilColor, d.stencilColor);
    Strength(r, "For\xC3\xA7" "a", &s.stencilColor[3], d.stencilColor[3]);
}

void TabAdvanced(Rows& r, Settings& s, const Settings& d) {
    SliderF(r, "Raio da proje\xC3\xA7\xC3\xA3o", &s.bound, 1, 12, 1, "%d m", d.bound);
    SliderF(r, "Raio no ar (helic\xC3\xB3ptero e avi\xC3\xA3o)", &s.boundAir, 1, 25, 1, "%d m", d.boundAir);
    SliderF(r, "Alcance at\xC3\xA9 o ch\xC3\xA3o", &s.zLimit, 1, 12, 1, "%d m", d.zLimit);
    SliderF(r, "Alcance no ar", &s.zLimitAir, 1, 25, 1, "%d m", d.zLimitAir);
    r.Toggle("Shader do Shadows Extender", &s.shader);
    if (!s.shader && s.combine) {
        // Sem o shader, o modo combinado desenharia a sombra em tempo real como um quadrado no stencil.
        r.Info("Sem o shader, ligue a sombra desfocada",
               "A sombra junto com a dos pr\xC3\xA9" "dios (blur desligado) precisa do shader do Shadows Extender. "
               "Sem ele, a sombra das pessoas e dos ve\xC3\xAD" "culos sai errada.");
    }
    if (r.Action("Padr\xC3\xB5" "es do Shadows Extender", "Restaurar")) {
        S.confirm = true;
        S.confirmAge = 0;
    }
}

void DrawUnavailable(Rows& r) {
    const char* problem = backend::Problem();
    r.Info(problem ? problem : "Esperando o Shadows Extender...",
           "Este menu muda o Shadows Extender 2.0 (shadows.asi, do DK22Pac). Ele precisa estar na pasta do GTA, "
           "junto com o shadows.ini e os dois .fx.");
}

void DrawMenu() {
    ImGuiIO& io = ImGui::GetIO();
    const bool fresh = S.menuAge++ == 0;
    const bool keysOk = !fresh && KeyboardFree() && !S.confirm;
    Keys keys = ReadKeys(keysOk);
    if (keysOk && ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
        S.tab = (S.tab + (io.KeyShift ? TAB_COUNT - 1 : 1)) % TAB_COUNT;
    }
    const bool escape = keysOk && ImGui::IsKeyPressed(ImGuiKey_Escape, false);

    const float width = 640 * u;
    // Cabe a aba mais longa (Sombra: 10 linhas e 2 secoes) sem sobrar um vazio embaixo das outras.
    const float height = std::min(io.DisplaySize.y * 0.72f, 500 * u);
    PushStyle();
    const bool keep = BeginShell("##trokShadows", "Trok Shadows", VERSION_TAG, width, height, S.shell);

    const ImVec2 pos = ImGui::GetWindowPos();
    const float padX = 18 * u;
    Tabs("##abas", TABS, TAB_COUNT, &S.tab, pos.x + padX, pos.y + HeaderHeight(), width - 2 * padX);

    const float listTop = pos.y + HeaderHeight() + TabsHeight() + 10 * u;
    const float listH = height - (listTop - pos.y) - FooterHeight() - 8 * u;
    ImGui::SetCursorScreenPos(ImVec2(pos.x + padX - 4 * u, listTop));
    ImGui::BeginChild("##lista", ImVec2(width - 2 * padX + 8 * u, listH), false, ImGuiWindowFlags_NoBackground);
    const ImVec2 cp = ImGui::GetCursorScreenPos();
    const float rowW = ImGui::GetContentRegionAvail().x - 10 * u;

    Rows rows;
    int* sel = &S.selected[S.tab];
    MoveSelection(sel, S.rowCount[S.tab], keys, rows);
    rows.Begin(TABS[S.tab], cp.x + 4 * u, rowW, sel, keys, true);
    Settings& s = backend::Current();
    const Settings& d = backend::Defaults();
    if (!backend::Attached()) {
        DrawUnavailable(rows);
    } else {
        switch (S.tab) {
        case 0: TabGeneral(rows, s, d); break;
        case 1: TabShadow(rows, s, d); break;
        case 2: TabVehicles(rows, s); break;
        case 3: TabStencil(rows, s, d); break;
        default: TabAdvanced(rows, s, d); break;
        }
    }
    S.rowCount[S.tab] = rows.End();
    ImGui::Dummy(ImVec2(0, 6 * u));
    ImGui::EndChild();

    static const std::vector<Hint> hints = {
        {"Setas", "Ajustar", false}, {"Tab", "Trocar de aba", false}, {"Esc", "Fechar", true}};
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
    const bool keep = BeginShell("##trokConfirmar", "Restaurar padr\xC3\xB5" "es?", nullptr, width, height, shell);
    const ImVec2 pos = ImGui::GetWindowPos();
    RichText(ImGui::GetWindowDrawList(), fonts.body, pos.x + 22 * u, pos.y + HeaderHeight() + 20 * u, col::text,
             "Tudo volta aos valores que v\xC3\xAAm com o Shadows Extender.\n"
             "{9A9A9A}O shadows.ini \xC3\xA9 gravado com eles.");
    static const std::vector<Hint> hints = {{"Enter", "Confirmar", true}, {"Esc", "Cancelar", true}};
    const int clicked = EndShell(hints, true);
    PopStyle();

    const bool keysOk = S.confirmAge++ > 0;
    const bool yes = clicked == 0 || (keysOk && ImGui::IsKeyPressed(ImGuiKey_Enter, false));
    const bool no = !keep || clicked == 1 || (keysOk && ImGui::IsKeyPressed(ImGuiKey_Escape, false));
    if (yes) {
        Settings& s = backend::Current();
        const bool fix = s.fixOccupants;
        s = backend::Defaults();
        s.fixOccupants = fix;
        S.confirm = false;
        Toast("Padr\xC3\xB5" "es restaurados", "Os valores do Shadows Extender voltaram.");
    } else if (no) {
        S.confirm = false;
    }
}

} // namespace

void BuildFonts(float screenHeight) {
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
    if (S.confirm) {
        S.confirm = false;
        return;
    }
    S.open = !S.open;
    S.menuAge = 0;
}

bool CapturesInput() {
    return S.open;
}

bool WantsFrame() {
    return S.open || S.dirty || ToastsVisible();
}

void Frame() {
    Settings& now = backend::Current();
    const Settings before = now;
    if (S.open) {
        DrawMenu();
        if (S.confirm) {
            DrawConfirm();
        }
    }
    if (before != now) {
        backend::Apply(before);
        S.dirty = true;
        S.lastChange = ImGui::GetTime();
    }
    // Grava um segundo depois da ultima mudanca, ou quando o menu fecha.
    if (S.dirty && (!S.open || ImGui::GetTime() - S.lastChange > 1.0)) {
        S.dirty = false;
        if (!backend::Save()) {
            Toast("Trok Shadows", "N\xC3\xA3o deu para gravar o shadows.ini (ele est\xC3\xA1 s\xC3\xB3 para leitura?).");
        }
        if (backend::RestartPending() && !S.restartWarned) {
            S.restartWarned = true;
            Toast("Trok Shadows", "Algumas mudan\xC3\xA7" "as valem quando o jogo abrir de novo.");
        }
    }
    DrawToasts();
}

} // namespace menu

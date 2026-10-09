// Teste de renderizacao do menu sem o jogo (Linux): roda menu.cpp no ImGui com um backend falso (os valores do
// shadows.ini do Victor_Trok), rasteriza o ImDrawData em software e grava PPMs. O rasterizador e o mesmo do teste
// da vitrine do Trok UI (trok-ui/asi/test/render_test.cpp).
// Uso: ./render_test <pasta de saida>
#include "imgui.h"
#include "menu.h"
#include "settings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// ------------------------------------------------------------------------------------------------ backend falso
namespace {
Settings g_cur, g_def;
bool g_attached = true;
bool g_restart = false;
int g_applies = 0, g_saves = 0;
} // namespace

namespace backend {
bool Attached() {
    return g_attached;
}
const char* Problem() {
    return g_attached ? nullptr : "O Shadows Extender 2.0 (shadows.asi) n\xC3\xA3o est\xC3\xA1 instalado.";
}
Settings& Current() {
    return g_cur;
}
const Settings& Defaults() {
    return g_def;
}
void Apply(const Settings& before) {
    g_applies++;
    if (g_cur.maxShadows != before.maxShadows) {
        g_restart = true;
    }
}
bool RestartPending() {
    return g_restart;
}
bool Save() {
    g_saves++;
    return true;
}
int ActiveShadows() {
    return 5;
}
bool FixInstalled() {
    return true;
}
} // namespace backend

// O shadows.ini do Victor_Trok (test/shadows.ini).
void UserValues(Settings& s) {
    s = Settings();
    s.maxShadows = 500;
    s.stencilDistance = 180.0f;
    s.flagIgnoreSome = s.disableBuildings = s.stencilLow = true;
    const int rgba[4] = {5, 12, 20, 80};
    for (int i = 0; i < 4; i++) {
        s.stencilColor[i] = s.realtimeColor[i] = rgba[i];
    }
    s.realtimeLow = true;
    s.combine = true;
    s.realtimeDistance = 100.0f;
    s.raster = 10;
    s.blurRaster = s.raster2 = s.blurRaster2 = 9;
    s.gradientMax = s.gradientMin = 128;
    s.bound = 10.0f;
    s.boundAir = 20.0f;
    s.zLimit = 8.0f;
    s.zLimitAir = 10.0f;
    s.vehicleDefaultWithRealtime = true;
    s.morePlayers = true;
}

// ------------------------------------------------------------------------------------------------ rasterizador
static int W = 1600, H = 900;
static std::vector<float> fb;
static unsigned char* tex;
static int tw, th;

static void Clear() {
    fb.assign(W * H * 3, 0);
    // Fundo de "jogo": gradiente para enxergar a transparencia.
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float* p = &fb[(y * W + x) * 3];
            p[0] = 0.25f + 0.25f * x / W;
            p[1] = 0.35f + 0.2f * y / H;
            p[2] = 0.30f;
        }
    }
}

static void Raster(const ImDrawVert& a, const ImDrawVert& b, const ImDrawVert& c, ImVec4 clip) {
    float minx = std::max(clip.x, std::floor(std::min({a.pos.x, b.pos.x, c.pos.x})));
    float maxx = std::min(clip.z, std::ceil(std::max({a.pos.x, b.pos.x, c.pos.x})));
    float miny = std::max(clip.y, std::floor(std::min({a.pos.y, b.pos.y, c.pos.y})));
    float maxy = std::min(clip.w, std::ceil(std::max({a.pos.y, b.pos.y, c.pos.y})));
    float area = (b.pos.x - a.pos.x) * (c.pos.y - a.pos.y) - (b.pos.y - a.pos.y) * (c.pos.x - a.pos.x);
    if (std::fabs(area) < 1e-6f) {
        return;
    }
    for (int y = (int)miny; y < (int)maxy; ++y) {
        for (int x = (int)minx; x < (int)maxx; ++x) {
            if (x < 0 || y < 0 || x >= W || y >= H) {
                continue;
            }
            float px = x + 0.5f, py = y + 0.5f;
            float w0 = ((b.pos.x - px) * (c.pos.y - py) - (b.pos.y - py) * (c.pos.x - px)) / area;
            float w1 = ((c.pos.x - px) * (a.pos.y - py) - (c.pos.y - py) * (a.pos.x - px)) / area;
            float w2 = 1 - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) {
                continue;
            }
            float u = w0 * a.uv.x + w1 * b.uv.x + w2 * c.uv.x, v = w0 * a.uv.y + w1 * b.uv.y + w2 * c.uv.y;
            int tx = std::min(tw - 1, std::max(0, (int)(u * tw))), ty = std::min(th - 1, std::max(0, (int)(v * th)));
            const unsigned char* t = &tex[(ty * tw + tx) * 4];
            float col[4];
            for (int i = 0; i < 4; ++i) {
                float ca = ((a.col >> (8 * i)) & 0xFF) / 255.f, cb = ((b.col >> (8 * i)) & 0xFF) / 255.f,
                      cc = ((c.col >> (8 * i)) & 0xFF) / 255.f;
                col[i] = (w0 * ca + w1 * cb + w2 * cc) * t[i] / 255.f;
            }
            float* p = &fb[(y * W + x) * 3];
            for (int i = 0; i < 3; ++i) {
                p[i] = p[i] * (1 - col[3]) + col[i] * col[3];
            }
        }
    }
}

static void Render(ImDrawData* dd) {
    for (int n = 0; n < dd->CmdListsCount; ++n) {
        const ImDrawList* cl = dd->CmdLists[n];
        for (const ImDrawCmd& cmd : cl->CmdBuffer) {
            if (cmd.UserCallback) {
                continue;
            }
            for (unsigned i = 0; i < cmd.ElemCount; i += 3) {
                const ImDrawIdx* idx = &cl->IdxBuffer[cmd.IdxOffset + i];
                Raster(cl->VtxBuffer[cmd.VtxOffset + idx[0]], cl->VtxBuffer[cmd.VtxOffset + idx[1]],
                       cl->VtxBuffer[cmd.VtxOffset + idx[2]], cmd.ClipRect);
            }
        }
    }
}

static void Save(const std::string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (float v : fb) {
        fputc((int)std::min(255.f, std::max(0.f, v * 255.f + 0.5f)), f);
    }
    fclose(f);
}

// ------------------------------------------------------------------------------------------------ roteiro
static std::string g_dir;

static void Frame(const char* shot = nullptr) {
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    menu::Frame();
    ImGui::Render();
    if (shot) {
        Clear();
        Render(ImGui::GetDrawData());
        Save(g_dir + "/" + shot + ".ppm");
    }
}

static void Key(ImGuiKey k, bool shift = false) {
    ImGuiIO& io = ImGui::GetIO();
    if (shift) {
        io.AddKeyEvent(ImGuiMod_Shift, true);
    }
    io.AddKeyEvent(k, true);
    Frame();
    io.AddKeyEvent(k, false);
    Frame();
    if (shift) {
        io.AddKeyEvent(ImGuiMod_Shift, false);
        Frame();
    }
}

static void Idle(int n) {
    for (int i = 0; i < n; ++i) {
        Frame();
    }
}

static int g_fail = 0;
static void Check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FALHA", what);
    g_fail += !ok;
}

int main(int argc, char** argv) {
    g_dir = argc > 1 ? argv[1] : ".";
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)W, (float)H);
    io.IniFilename = nullptr;
    menu::BuildFonts((float)H);
    io.Fonts->GetTexDataAsRGBA32(&tex, &tw, &th);
    io.Fonts->SetTexID((ImTextureID)1);
    UserValues(g_cur);
    io.AddMousePosEvent(1500, 860); // mouse fora do menu

    Idle(2);
    // Abre como o /sombras: o Enter do chat ainda esta "apertado" no quadro em que o menu abre.
    io.AddKeyEvent(ImGuiKey_Enter, true);
    menu::Toggle();
    Frame();
    io.AddKeyEvent(ImGuiKey_Enter, false);
    Idle(30);
    Check(g_applies == 0, "abrir com o Enter do chat nao muda nada");
    Frame("01_geral");

    // Predefinicao (linha 1): abre a lista, fecha e vai de "Personalizado" para "Bonito" e "Equilibrado" com a seta.
    Key(ImGuiKey_Enter);
    Idle(20);
    Frame("02_predefinicao");
    Key(ImGuiKey_Escape);
    Idle(5);
    Key(ImGuiKey_LeftArrow);
    Idle(5);
    Check(g_cur.raster == 9 && g_cur.blurLevel == 6 && g_cur.realtimeDistance == 80.0f, "Bonito: 512, desfoque 6, 80 m");
    Key(ImGuiKey_LeftArrow);
    Idle(30);
    Check(g_cur.raster == 8 && g_cur.blurRaster == 7 && g_cur.blurLevel == 4 && g_cur.realtimeDistance == 50.0f &&
              g_cur.stencilDistance == 100.0f,
          "Equilibrado: 256/128, desfoque 4, 50 m e 100 m");
    Frame("03_equilibrado");

    // Sombra desfocada (linha 3).
    Key(ImGuiKey_DownArrow);
    Key(ImGuiKey_DownArrow);
    Key(ImGuiKey_Enter);
    Idle(20);
    Check(!g_cur.combine, "blur ligado = CombineRealTimeShadowsWithStencil 0");
    Idle(70);
    Check(g_saves >= 1, "gravou um segundo depois da ultima mudanca");

    Key(ImGuiKey_Tab);
    Idle(30);
    Frame("04_sombra_desfocada");
    Key(ImGuiKey_Tab, true);
    Idle(10);
    Key(ImGuiKey_Enter); // blur desligado de novo
    Idle(10);
    Key(ImGuiKey_Tab);
    Idle(30);
    Frame("05_sombra_junto_do_stencil");

    // Cor (linha 6 com o blur desligado).
    for (int i = 0; i < 5; i++) {
        Key(ImGuiKey_DownArrow);
    }
    Key(ImGuiKey_Enter);
    Idle(20);
    Frame("06_cor");
    Key(ImGuiKey_Escape);
    Idle(10);

    Key(ImGuiKey_Tab);
    Idle(30);
    Frame("07_veiculos");
    Key(ImGuiKey_Tab);
    Idle(30);
    Frame("08_stencil");
    // Quantidade: 500 -> 532 (vale ao reiniciar).
    Key(ImGuiKey_RightArrow);
    Idle(10);
    Check(g_cur.maxShadows == 532 && g_restart, "quantidade do stencil: passo de 32, vale ao reiniciar");
    Key(ImGuiKey_Tab);
    Idle(30);
    Frame("09_avancado");
    for (int i = 0; i < 5; i++) {
        Key(ImGuiKey_DownArrow);
    }
    Key(ImGuiKey_Enter);
    Idle(20);
    Frame("10_confirmar");
    Key(ImGuiKey_Enter);
    Idle(20);
    Check(g_cur.raster == 7 && g_cur.maxShadows == 64 && g_cur.fixOccupants, "padroes restaurados (a correcao fica)");
    Frame("11_padroes_restaurados");

    Key(ImGuiKey_Tab);
    Idle(30);
    Frame("12_geral_reiniciar");

    // Sem o Shadows Extender.
    g_attached = false;
    Idle(5);
    Frame("13_sem_shadows_extender");
    g_attached = true;

    Key(ImGuiKey_Escape);
    Idle(5);
    Check(!menu::CapturesInput(), "Esc fecha");
    Idle(120);
    ImGui::DestroyContext();
    printf("render: %d falhas\n", g_fail);
    return g_fail ? 1 : 0;
}

// Teste de renderizacao sem o jogo: roda o showcase no ImGui, rasteriza o ImDrawData em
// software e grava PPMs. Uso: ./render_test <pasta de saida>
#include "imgui.h"
#include "showcase.h"
#include "trok_ui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int W = 1600, H = 900;
static std::vector<float> fb;
static unsigned char* tex; static int tw, th;

static void Clear() {
    fb.assign(W * H * 3, 0);
    // Fundo de "jogo": gradiente para enxergar a transparencia.
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        float* p = &fb[(y * W + x) * 3];
        p[0] = 0.25f + 0.25f * x / W; p[1] = 0.35f + 0.2f * y / H; p[2] = 0.30f;
    }
}

static void Raster(const ImDrawVert& a, const ImDrawVert& b, const ImDrawVert& c, ImVec4 clip) {
    float minx = std::max(clip.x, std::floor(std::min({a.pos.x, b.pos.x, c.pos.x})));
    float maxx = std::min(clip.z, std::ceil(std::max({a.pos.x, b.pos.x, c.pos.x})));
    float miny = std::max(clip.y, std::floor(std::min({a.pos.y, b.pos.y, c.pos.y})));
    float maxy = std::min(clip.w, std::ceil(std::max({a.pos.y, b.pos.y, c.pos.y})));
    float area = (b.pos.x - a.pos.x) * (c.pos.y - a.pos.y) - (b.pos.y - a.pos.y) * (c.pos.x - a.pos.x);
    if (std::fabs(area) < 1e-6f) return;
    for (int y = (int)miny; y < (int)maxy; ++y) for (int x = (int)minx; x < (int)maxx; ++x) {
        if (x < 0 || y < 0 || x >= W || y >= H) continue;
        float px = x + 0.5f, py = y + 0.5f;
        float w0 = ((b.pos.x - px) * (c.pos.y - py) - (b.pos.y - py) * (c.pos.x - px)) / area;
        float w1 = ((c.pos.x - px) * (a.pos.y - py) - (c.pos.y - py) * (a.pos.x - px)) / area;
        float w2 = 1 - w0 - w1;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        float u = w0 * a.uv.x + w1 * b.uv.x + w2 * c.uv.x, v = w0 * a.uv.y + w1 * b.uv.y + w2 * c.uv.y;
        int tx = std::min(tw - 1, std::max(0, (int)(u * tw))), ty = std::min(th - 1, std::max(0, (int)(v * th)));
        const unsigned char* t = &tex[(ty * tw + tx) * 4];
        float col[4];
        for (int i = 0; i < 4; ++i) {
            float ca = ((a.col >> (8 * i)) & 0xFF) / 255.f, cb = ((b.col >> (8 * i)) & 0xFF) / 255.f, cc = ((c.col >> (8 * i)) & 0xFF) / 255.f;
            col[i] = (w0 * ca + w1 * cb + w2 * cc) * t[i] / 255.f;
        }
        float* p = &fb[(y * W + x) * 3];
        for (int i = 0; i < 3; ++i) p[i] = p[i] * (1 - col[3]) + col[i] * col[3];
    }
}

static void Render(ImDrawData* dd) {
    for (int n = 0; n < dd->CmdListsCount; ++n) {
        const ImDrawList* cl = dd->CmdLists[n];
        for (const ImDrawCmd& cmd : cl->CmdBuffer) {
            if (cmd.UserCallback) continue;
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
    for (float v : fb) fputc((int)std::min(255.f, std::max(0.f, v * 255.f + 0.5f)), f);
    fclose(f);
}

static void Frame(bool draw = false, const std::string& out = "") {
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    showcase::Frame();
    ImGui::Render();
    if (draw) { Clear(); Render(ImGui::GetDrawData()); Save(out); }
}

static void Key(ImGuiKey k, bool shift = false) {
    ImGuiIO& io = ImGui::GetIO();
    if (shift) io.AddKeyEvent(ImGuiMod_Shift, true);
    io.AddKeyEvent(k, true); Frame(); io.AddKeyEvent(k, false); Frame();
    if (shift) { io.AddKeyEvent(ImGuiMod_Shift, false); Frame(); }
}
static void Mouse(float x, float y) { ImGui::GetIO().AddMousePosEvent(x, y); Frame(); Frame(); }
static void Click(float x, float y) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(x, y); Frame();
    io.AddMouseButtonEvent(0, true); Frame(); io.AddMouseButtonEvent(0, false); Frame();
}
static void Idle(int n) { for (int i = 0; i < n; ++i) Frame(); }
// Ponto dentro do menu dado na escala em que o roteiro foi medido (u = 900/1080*0.85) e levado para a escala atual:
// o menu fica no meio da tela e tudo nele escala com u, entao o mouse continua em cima do mesmo item.
static void MouseIn(float x, float y) {
    float k = tui::u / (900 / 1080.0f * 0.85f);
    Mouse(W * 0.5f + (x - W * 0.5f) * k, H * 0.5f + (y - H * 0.5f) * k);
}
// Troca de aba com um clique (o Tab e do SA-MP: abre o placar). As 5 abas dividem a largura do menu, que fica no
// meio da tela; depois o mouse volta para onde estava, para as telas seguintes nao mudarem. Leva 5 quadros (a tecla
// levava 2): quem chama espera 3 a menos depois, para as animacoes chegarem iguais as telas.
static void ClickTab(int i, float backX, float backY) {
    float u = tui::u;
    float width = 640 * u, height = std::min(H * 0.72f, 540 * u);
    float x = W * 0.5f - std::ceil(width) * 0.5f + 18 * u, y = H * 0.5f - std::ceil(height) * 0.5f + tui::HeaderHeight();
    float tabW = (width - 36 * u) / 5;
    Click(x + tabW * (i + 0.5f), y + tui::TabsHeight() * 0.5f);
    MouseIn(backX, backY);
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : ".";
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)W, (float)H);
    io.IniFilename = nullptr;
    showcase::BuildFonts((float)H, nullptr);
    io.Fonts->GetTexDataAsRGBA32(&tex, &tw, &th);
    io.Fonts->SetTexID((ImTextureID)1);

    Idle(2);
    // Abre como o /trokui: o Enter do chat ainda esta "apertado" no quadro em que o menu abre.
    io.AddKeyEvent(ImGuiKey_Enter, true);
    showcase::Toggle();
    Frame();
    io.AddKeyEvent(ImGuiKey_Enter, false);
    Idle(30);
    MouseIn(800, 330);
    Idle(20);
    Frame(true, dir + "/01_linhas.ppm");
    for (int i = 0; i < 6; ++i) Key(ImGuiKey_DownArrow);
    Key(ImGuiKey_RightArrow);
    Key(ImGuiKey_LeftArrow); Key(ImGuiKey_LeftArrow);
    Key(ImGuiKey_DownArrow); Key(ImGuiKey_RightArrow); Key(ImGuiKey_RightArrow);
    Idle(30);
    Frame(true, dir + "/02_linhas_teclado.ppm");
    for (int i = 0; i < 3; ++i) Key(ImGuiKey_DownArrow);
    Key(ImGuiKey_Enter); Idle(20);
    Frame(true, dir + "/03_tecla.ppm");
    Key(ImGuiKey_Escape); Key(ImGuiKey_DownArrow); Key(ImGuiKey_Enter); Idle(20);
    Frame(true, dir + "/04_cor.ppm");
    Key(ImGuiKey_Escape); Key(ImGuiKey_DownArrow); Key(ImGuiKey_Enter); Idle(20);
    Frame(true, dir + "/04b_dropdown.ppm");
    Key(ImGuiKey_Escape); Idle(5);
    ClickTab(1, 800, 330); Idle(27);
    Frame(true, dir + "/05_texto.ppm");
    Key(ImGuiKey_DownArrow); Key(ImGuiKey_Enter);
    io.AddInputCharactersUTF8("segredo123"); Idle(10);
    Frame(true, dir + "/06_texto_digitando.ppm");
    Key(ImGuiKey_Escape); Idle(5);
    ClickTab(2, 800, 330); Idle(27);
    Key(ImGuiKey_3); Idle(10);
    Frame(true, dir + "/07_listas.ppm");
    ClickTab(3, 800, 330); Idle(27);
    MouseIn(700, 480); Idle(40);
    Frame(true, dir + "/08_avisos.ppm");
    for (int i = 0; i < 6; ++i) Key(ImGuiKey_DownArrow);
    Key(ImGuiKey_Enter); Idle(20);
    Frame(true, dir + "/09_confirmar.ppm");
    Key(ImGuiKey_Escape); Idle(5);
    Key(ImGuiKey_UpArrow); Key(ImGuiKey_Enter); Idle(15);
    Frame(true, dir + "/10_toast.ppm");
    ClickTab(4, 700, 480); Idle(17);
    Key(ImGuiKey_DownArrow); Key(ImGuiKey_DownArrow); Key(ImGuiKey_Enter); Idle(20);
    io.AddInputCharactersUTF8("abc123"); Idle(5);
    Frame(true, dir + "/11_dialogo_senha.ppm");
    Key(ImGuiKey_Escape); Idle(10);
    Key(ImGuiKey_DownArrow); Key(ImGuiKey_Enter); Idle(15); Key(ImGuiKey_DownArrow); Idle(10);
    Frame(true, dir + "/12_dialogo_lista.ppm");
    Key(ImGuiKey_Escape); Idle(10);
    Key(ImGuiKey_DownArrow); Key(ImGuiKey_Enter); Idle(15);
    Frame(true, dir + "/13_dialogo_tabela.ppm");
    Key(ImGuiKey_Escape); Idle(10);
    Key(ImGuiKey_UpArrow); Key(ImGuiKey_UpArrow); Key(ImGuiKey_UpArrow); Key(ImGuiKey_UpArrow); Key(ImGuiKey_Enter); Idle(15);
    Frame(true, dir + "/14_dialogo_mensagem.ppm");
    Key(ImGuiKey_Enter); Idle(10);
    ClickTab(0, 700, 480); Idle(17);
    for (int i = 0; i < 12; ++i) Key(ImGuiKey_UpArrow);
    Key(ImGuiKey_Enter); Idle(10);
    Frame(true, dir + "/15_mover.ppm");
    // Arrasta a previa (canto superior direito por padrao) ate o meio da tela.
    float px = W - tui::u * (260 + 40) + 40, py = tui::u * 120 + 20;
    io.AddMousePosEvent(px, py); Frame();
    io.AddMouseButtonEvent(0, true); Frame();
    for (int i = 1; i <= 20; ++i) { io.AddMousePosEvent(px - i * 30, py + i * 15); Frame(); }
    Frame(true, dir + "/16_mover_arrastando.ppm");
    io.AddMouseButtonEvent(0, false); Idle(15);
    Frame(true, dir + "/17_mover_solto.ppm");
    ImGui::DestroyContext();
    return 0;
}

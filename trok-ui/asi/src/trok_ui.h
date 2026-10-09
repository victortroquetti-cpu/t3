#pragma once

// Trok UI -- kit de componentes da casa (Victor_Trok) para mods .asi com Dear ImGui 1.89.9.
// Tudo e desenhado a mao no draw list, sem os widgets cinza padrao do ImGui. As medidas saem
// dos mods publicados (Trok Kill List, Trok Dialogs e Trok Radar) e escalam com u = altura/1080*0.7225, 15% menor que
// a conta do Kill List.
// Fontes da casa: moonloader\resource\trok\font.ttf (Gotham Medium) e lucide.ttf (icones).

#include "imgui.h"
#include <string>
#include <vector>

namespace tui {

// ---------------------------------------------------------------- tokens

struct Fonts {
    ImFont* body = nullptr;      // 16u -- rotulos e textos
    ImFont* title = nullptr;     // 20u -- titulo do cabecalho
    ImFont* desc = nullptr;      // 13.5u -- numeros, valores, dicas, versao
    ImFont* icon = nullptr;      // 18u -- lucide.ttf
    ImFont* iconSmall = nullptr; // 13u -- lucide.ttf (check, lupa)
};

extern Fonts fonts;
extern float u; // escala da casa

namespace col {
ImU32 Rgba(int r, int g, int b, int a = 255);
ImU32 White(int a);
extern const ImU32 text;        // 240
extern const ImU32 column;      // 200 -- valores e teclas
extern const ImU32 hint;        // branco 118 -- acoes e textos de apoio
extern const ImU32 disabled;    // branco 42
extern const ImU32 version;     // 140
extern const ImU32 separator;   // branco 10
extern const ImU32 selection;   // branco 10
extern const ImU32 hover;       // branco 6
extern const ImU32 number;      // branco 78
extern const ImU32 numberHover; // branco 110
extern const ImU32 numberSel;   // branco 150
extern const ImU32 strong;      // 226 -- "ligado": trilho do toggle, preenchimentos
extern const ImU32 ink;         // 18 -- texto/marca sobre o "ligado"
extern const ImU32 marker;      // branco 70 -- marca do valor padrao
} // namespace col

float Scale(float screenHeight);
// gameDir termina em '\'. As fontes saem de <gameDir>moonloader\resource\trok\.
void BuildFonts(float screenHeight, const char* gameDir);

ImVec2 TextSize(ImFont* font, const char* text);
void Text(ImDrawList* dl, ImFont* font, float x, float y, ImU32 color, const char* text);
// Tracinho discreto que marca "este e o valor padrao", centrado em (x, y).
void DefaultMark(ImDrawList* dl, float x, float y, ImU32 color);
float Anim(ImGuiID id, float target, float speed = 14.0f);

// ---------------------------------------------------------------- icones (lucide)

enum class Icon {
    Close,
    ChevronLeft,
    ChevronRight,
    ChevronUp,
    ChevronDown,
    ChevronsRight,
    ArrowRight,
    Eye,
    EyeOff,
    Check,
    Search,
    Info,
};

// Desenha o icone do lucide.ttf centrado em c. Sem o glifo na fonte, usa o chevron-direita girado
// (para as outras direcoes) ou um desenho vetorial no mesmo traco.
void DrawIcon(ImDrawList* dl, Icon icon, ImVec2 c, ImU32 color, bool small = false);
float IconWidth(Icon icon, bool small = false);

// Texto rico do SA-MP: cores {RRGGBB} e as setas do Trok Dialogs (">", ">>", "»", "->", "=>" viram
// icones do lucide). Uma linha por \n. Devolve o tamanho; draw = false so mede.
ImVec2 RichText(ImDrawList* dl, ImFont* font, float x, float y, ImU32 color, const char* text, bool draw = true);

void Spinner(ImDrawList* dl, ImVec2 c, float radius, float time);

// ---------------------------------------------------------------- janela (casca)

struct Shell {
    bool moved = false;
    ImVec2 at = ImVec2(0, 0);
    bool dragging = false;
};

struct Hint {
    const char* key;
    const char* action;
    bool clickable;
};

void PushStyle();
void PopStyle();

// Cabecalho da casa (titulo + versao centralizados, divisor, X e alca de arrastar).
// Devolve false se o X foi clicado.
bool BeginShell(const char* id, const char* title, const char* version, float width, float height, Shell& shell,
                ImGuiWindowFlags extraFlags = 0);
// Rodape: faixa escura com as dicas. alignRight = estilo dos dialogos (Enter/Esc a direita).
// Devolve o indice da dica clicada ou -1.
int EndShell(const std::vector<Hint>& hints, bool alignRight = false);

float HeaderHeight();
float FooterHeight();

// ---------------------------------------------------------------- abas

int Tabs(const char* id, const char* const* labels, int count, int* current, float x, float y, float width);
float TabsHeight();

// ---------------------------------------------------------------- linhas

struct Keys {
    bool up = false, down = false, left = false, right = false, enter = false, space = false;
    int digit = 0; // 1..9
};

// Lista de linhas numeradas. Use dentro da janela (ou de um child com rolagem).
// Valor padrao: barras mostram uma linha vertical no padrao; setas, giro e segmentado mostram um
// tracinho; todos ganham "Restaurar" quando saem do padrao (a dica mostra qual e o padrao).
class Rows {
public:
    void Begin(const char* id, float x, float width, int* selected, const Keys& keys, bool showNumbers = true);
    int End(); // devolve quantas linhas foram desenhadas

    bool Action(const char* label, const char* button);
    bool Stepper(const char* label, int* value, int min, int max, int step, const char* fmt, int def);
    bool Cycle(const char* label, int* index, const char* const* options, int count, int def);
    bool Toggle(const char* label, bool* value);
    bool Segmented(const char* label, int* index, const char* const* options, int count, int def);
    bool Slider(const char* label, int* value, int min, int max, int step, const char* fmt, int def);
    bool Keybind(const char* label, ImGuiKey* key, ImGuiKey def);
    bool Color(const char* label, ImU32* color, ImU32 def);
    bool Dropdown(const char* label, int* index, const char* const* options, int count, int def);
    bool Input(const char* label, char* buffer, size_t size, const char* placeholder, bool password = false);
    bool Search(const char* label, char* buffer, size_t size, const char* placeholder);
    bool Multiline(const char* label, char* buffer, size_t size, const char* placeholder, int lines = 3);
    void Progress(const char* label, float fraction, bool indeterminate = false);
    void Loading(const char* label, const char* status);
    void Badges(const char* label, const char* const* badges, const int* kinds, int count); // 0 neutro, 1 forte, 2 contorno
    void Info(const char* label, const char* tooltip);
    void Section(const char* title);

    // Itens de lista (estilo Trok Dialogs). Devolvem true quando o item e ativado (Enter, clique duplo, 1-9).
    bool Item(const char* text);
    void TableHeader(const char* const* columns, const float* widths, int count);
    bool TableItem(const char* const* cells, const float* widths, int count);
    bool CheckItem(const char* text, bool* checked);
    bool RadioItem(const char* text, int* group, int value);

    int Count() const { return index_; }
    bool keyboardMoved = false;

private:
    void Start();
    bool Row(float height, bool controlHovered, bool selectable = true);
    void Label(const char* label, bool hovered);
    void Number(bool hovered, float centerY);
    bool Restore(bool differs, float controlLeft, const char* defaultText);
    void DefaultMark(float x, float y);
    void Finish(float height);
    bool Selected() const { return *selected_ == index_; }
    ImGuiID Id(const char* suffix) const;

    ImDrawList* dl_ = nullptr;
    float x_ = 0, w_ = 0, y_ = 0;
    float rowH_ = 0, gap_ = 0, recuo_ = 0, labelX_ = 0, numberW_ = 0;
    int* selected_ = nullptr;
    int index_ = 0;
    bool numbers_ = true;
    Keys keys_;
    // "Restaurar" da linha atual (desenhado depois do fundo da linha).
    bool restoreVisible_ = false, restoreHovered_ = false;
    float restoreX_ = 0;
};

// Campo de texto solto (dialogos): moldura da casa, olho na senha e menu do botao direito.
bool TextField(const char* id, char* buffer, size_t size, const char* placeholder, bool password, float x, float y,
               float w, float h, bool focus);

// ---------------------------------------------------------------- avisos

void Toast(const char* title, const char* message);
void DrawToasts();
bool ToastsVisible();

void Tooltip(const char* text);

// true enquanto uma linha de tecla espera o jogador apertar algo (as setas nao navegam).
bool CapturingKey();

} // namespace tui

// Trok Chat (.asi) -- Victor_Trok
// O chat na tela, com o kit da casa. Cada mensagem do SA-MP vira uma lista de pedacos (palavras com a cor do
// {RRGGBB} em vigor, tags e a hora), quebrada em linhas na largura do chat. Fechado, mostra as ultimas linhas por cima
// do jogo, como o SA-MP. Aberto (T ou F6), ganha o fundo, a rolagem e a barra de digitacao.

#include "chat.h"

#include "imgui.h"
#include "trok_ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <vector>

namespace chat {
namespace {

using namespace tui;

constexpr size_t MAX_MESSAGES = 300; // o SA-MP guarda 100
constexpr int MAX_INPUT = 128;       // caracteres que a caixa do SA-MP aceita
constexpr int MAX_TAG = 24;          // um [conteudo] maior que isso continua texto
constexpr size_t MAX_HISTORY = 50;

enum Kind : uint8_t { WORD, TAG, STAMP };

struct Token {
    Kind kind;
    bool space; // havia espaco antes
    std::string text;
    ImU32 color;
};

struct Placed {
    int line;
    float x, w;
    Kind kind;
    std::string text;
    ImU32 color;
};

struct Entry {
    Message msg;
    std::vector<Token> tokens;
    // Linhas na ultima largura e fonte usadas.
    std::vector<Placed> placed;
    int lines = 1;
    float layoutWidth = -1, layoutFont = -1;
    bool layoutStamp = false;
};

std::mutex g_lock;
std::deque<Entry> g_entries;

struct State {
    bool open = false;
    bool focus = false;
    char input[MAX_INPUT * 4 + 1] = "";
    std::vector<std::string> history;
    int historyPos = -1; // -1: o rascunho
    std::string draft;
    int scroll = 0;      // linhas de baixo para cima (0 = as mais recentes)
    unsigned lastAdded = 0; // g_added no quadro anterior
    unsigned seen = 0;   // mensagens que ja chegaram com a lista nas mais recentes
    bool gameWheel = false; // a roda do jogo ja respondeu: a do Windows (a mesma rodada, as vezes) fica de fora
    bool caretToEnd = false;
    int session = 0; // uma barra nova (outro id no ImGui) a cada vez que abre
    bool active = false;          // a barra estava com o teclado no quadro anterior
    std::vector<ImWchar> pending; // digitado antes de a barra pegar o teclado
} S;

unsigned g_added = 0; // mensagens que ja chegaram (com g_lock)

// ---------------------------------------------------------------- cores e texto

ImU32 FromArgb(uint32_t argb) {
    return IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, 255);
}

int Hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// {RRGGBB} em s[i].
bool ColorCode(const std::string& s, size_t i, ImU32* out) {
    if (i + 8 > s.size() || s[i] != '{' || s[i + 7] != '}') {
        return false;
    }
    int v[6];
    for (int k = 0; k < 6; k++) {
        if ((v[k] = Hex(s[i + 1 + k])) < 0) {
            return false;
        }
    }
    *out = IM_COL32(v[0] * 16 + v[1], v[2] * 16 + v[3], v[4] * 16 + v[5], 255);
    return true;
}

int Codepoints(const std::string& s) {
    int n = 0;
    for (unsigned char c : s) {
        n += (c & 0xC0) != 0x80;
    }
    return n;
}

std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && s[a] == ' ') a++;
    while (b > a && s[b - 1] == ' ') b--;
    return s.substr(a, b - a);
}

// [conteudo] em s[i]: o texto visivel (sem os {RRGGBB} de dentro), a cor do primeiro caractere dele e a cor que
// fica valendo depois.
bool Tag(const std::string& s, size_t i, ImU32 color, size_t* end, std::string* text, ImU32* tagColor, ImU32* after) {
    const size_t close = s.find(']', i + 1);
    if (close == std::string::npos) {
        return false;
    }
    const size_t open = s.find('[', i + 1);
    if (open != std::string::npos && open < close) {
        return false;
    }
    std::string visible;
    ImU32 current = color, first = color;
    bool any = false;
    for (size_t k = i + 1; k < close;) {
        ImU32 code;
        if (ColorCode(s, k, &code)) {
            current = code;
            k += 8;
            continue;
        }
        if (static_cast<unsigned char>(s[k]) >= 0x20) {
            if (!any && s[k] != ' ') {
                first = current;
                any = true;
            }
            visible += s[k];
        }
        k++;
    }
    visible = Trim(visible);
    const int length = Codepoints(visible);
    if (length < 1 || length > MAX_TAG) {
        return false;
    }
    *end = close + 1;
    *text = visible;
    *tagColor = first;
    *after = current;
    return true;
}

void ParseText(const std::string& s, ImU32 color, bool spaceFirst, std::vector<Token>& out) {
    ImU32 current = color;
    std::string word;
    bool space = spaceFirst;
    auto flush = [&]() {
        if (!word.empty()) {
            out.push_back({WORD, space, word, current});
            word.clear();
            space = false;
        }
    };
    for (size_t i = 0; i < s.size();) {
        ImU32 code;
        if (ColorCode(s, i, &code)) {
            flush(); // a cor muda no meio da palavra: dois pedacos colados
            current = code;
            i += 8;
            continue;
        }
        const char c = s[i];
        if (c == ' ' || static_cast<unsigned char>(c) < 0x20) {
            flush(); // tab e quebra de linha viram espaco, como no chat do SA-MP
            space = true;
            i++;
            continue;
        }
        if (c == '[') {
            size_t end;
            std::string text;
            ImU32 tagColor, after;
            if (Tag(s, i, current, &end, &text, &tagColor, &after)) {
                flush();
                out.push_back({TAG, space, text, tagColor});
                space = false;
                current = after;
                i = end;
                continue;
            }
        }
        word += c;
        i++;
    }
    flush();
}

std::vector<Token> Tokenize(const Message& m) {
    std::vector<Token> tokens;
    if (m.type == TYPE_CHAT && !m.prefix.empty()) {
        std::string nick = m.prefix;
        if (nick.back() != ':') {
            nick += ':';
        }
        tokens.push_back({WORD, false, nick, FromArgb(m.prefixColor)});
    }
    ParseText(m.text, FromArgb(m.textColor), !tokens.empty(), tokens);
    return tokens;
}

// Etiqueta: fundo na cor do [conteudo] e texto escuro ou claro conforme o fundo. Branco ou cinza: etiqueta neutra
// (clara no painel escuro do chat aberto; escura direto sobre o jogo, com o chat fechado).
void TagColors(ImU32 c, bool overGame, ImU32* fill, ImU32* text) {
    const int r = c & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF;
    const int hi = std::max(r, std::max(g, b)), lo = std::min(r, std::min(g, b));
    if (hi - lo < 40) {
        const int v = std::max(hi, 200);
        *fill = overGame ? IM_COL32(16, 16, 20, 180) : col::White(34);
        *text = IM_COL32(v, v, v, 255);
        return;
    }
    *fill = IM_COL32(r, g, b, 230);
    const float light = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;
    *text = light > 0.62f ? IM_COL32(18, 18, 18, 255) : IM_COL32(255, 255, 255, 255);
}

// ---------------------------------------------------------------- linhas

float TagPad() {
    return 5 * u;
}

float Measure(const Token& t) {
    switch (t.kind) {
    case TAG:
        return TextSize(fonts.desc, t.text.c_str()).x + 2 * TagPad();
    case STAMP:
        return TextSize(fonts.desc, t.text.c_str()).x;
    default:
        return TextSize(fonts.body, t.text.c_str()).x;
    }
}

// Quanto de uma palavra cabe em "room" (em bytes, sem cortar um caractere ao meio; pelo menos um caractere).
size_t Fit(const std::string& word, float room) {
    size_t fit = 0;
    for (size_t i = 0; i < word.size();) {
        size_t next = i + 1;
        while (next < word.size() && (static_cast<unsigned char>(word[next]) & 0xC0) == 0x80) next++;
        if (fit > 0 && TextSize(fonts.body, word.substr(0, next).c_str()).x > room) {
            break;
        }
        fit = next;
        i = next;
    }
    return fit;
}

void Layout(Entry& e, float width, bool stamps) {
    const float font = fonts.body->FontSize;
    if (e.layoutWidth == width && e.layoutFont == font && e.layoutStamp == stamps) {
        return;
    }
    e.layoutWidth = width;
    e.layoutFont = font;
    e.layoutStamp = stamps;
    e.placed.clear();

    std::vector<Token> tokens;
    if (stamps) {
        char clock[16] = "--:--:--";
        if (e.msg.time) {
            const std::tm* t = std::localtime(&e.msg.time);
            if (t) {
                std::snprintf(clock, sizeof(clock), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
            }
        }
        tokens.push_back({STAMP, false, clock, col::hint});
        tokens.insert(tokens.end(), e.tokens.begin(), e.tokens.end());
        if (tokens.size() > 1) {
            tokens[1].space = true;
        }
    }
    const std::vector<Token>& list = stamps ? tokens : e.tokens;

    const float space = TextSize(fonts.body, " ").x, gapTag = 3 * u;
    int line = 0;
    float x = 0;
    Kind previous = WORD;
    for (const Token& t : list) {
        float gap = 0;
        if (x > 0) {
            gap = t.space ? space : (t.kind == TAG || previous == TAG ? gapTag : 0);
        }
        float w = Measure(t);
        if (x > 0 && x + gap + w > width) {
            line++;
            x = 0;
            gap = 0;
        }
        if (t.kind == WORD && w > width) {
            // Palavra maior que a linha (um link): quebra onde couber.
            std::string rest = t.text;
            while (!rest.empty()) {
                const size_t n = Fit(rest, width - x);
                const std::string part = rest.substr(0, n);
                const float pw = TextSize(fonts.body, part.c_str()).x;
                e.placed.push_back({line, x + gap, pw, WORD, part, t.color});
                rest.erase(0, n);
                gap = 0;
                if (!rest.empty()) {
                    line++;
                    x = 0;
                } else {
                    x += pw;
                }
            }
        } else {
            e.placed.push_back({line, x + gap, w, t.kind, t.text, t.color});
            x += gap + w;
        }
        previous = t.kind;
    }
    e.lines = line + 1;
}

// overGame: chat fechado, direto sobre o jogo. outline: contorno preto (o modo normal do F7; o "sem sombra" nao tem).
void DrawPiece(ImDrawList* dl, const Placed& p, float x, float y, float lineH, bool overGame, bool outline) {
    if (p.kind == TAG) {
        const float h = std::floor(fonts.desc->FontSize + 5 * u);
        const float top = std::floor(y + (lineH - h) * 0.5f);
        ImU32 fill, text;
        TagColors(p.color, overGame, &fill, &text);
        if (outline) {
            dl->AddRect(ImVec2(x + p.x - 1, top - 1), ImVec2(x + p.x + p.w + 1, top + h + 1), IM_COL32(0, 0, 0, 90),
                        4 * u + 1);
        }
        dl->AddRectFilled(ImVec2(x + p.x, top), ImVec2(x + p.x + p.w, top + h), fill, 4 * u);
        Text(dl, fonts.desc, x + p.x + TagPad(), top + (h - fonts.desc->FontSize) * 0.5f, text, p.text.c_str());
        return;
    }
    ImFont* font = p.kind == STAMP ? fonts.desc : fonts.body;
    const float tx = std::floor(x + p.x), ty = std::floor(y + (lineH - font->FontSize) * 0.5f + (p.kind == STAMP ? 1 : 0));
    if (outline) {
        // Como o chat do SA-MP: legivel sobre o ceu e sobre o chao.
        static const float ring[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (const auto& o : ring) {
            Text(dl, font, tx + o[0], ty + o[1], IM_COL32(0, 0, 0, 150), p.text.c_str());
        }
        Text(dl, font, tx + 1, ty + 1, IM_COL32(0, 0, 0, 210), p.text.c_str());
    }
    Text(dl, font, tx, ty, p.color, p.text.c_str());
}

// ---------------------------------------------------------------- barra de digitacao

int InputCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        if (S.history.empty()) {
            return 0;
        }
        const int before = S.historyPos;
        if (data->EventKey == ImGuiKey_UpArrow) {
            if (S.historyPos == -1) {
                S.draft.assign(data->Buf, data->BufTextLen);
                S.historyPos = static_cast<int>(S.history.size()) - 1;
            } else if (S.historyPos > 0) {
                S.historyPos--;
            }
        } else if (data->EventKey == ImGuiKey_DownArrow && S.historyPos != -1) {
            if (++S.historyPos >= static_cast<int>(S.history.size())) {
                S.historyPos = -1;
            }
        }
        if (before != S.historyPos) {
            const std::string& text = S.historyPos == -1 ? S.draft : S.history[S.historyPos];
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, text.c_str());
        }
    } else if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
        // Ao pegar o teclado de volta, o ImGui pode selecionar o texto todo (depende da versao, e a proxima tecla
        // apagaria a mensagem): o cursor vai para o fim, sem selecao.
        if (S.caretToEnd) {
            S.caretToEnd = false;
            data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen;
        }
    } else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
        // No maximo MAX_INPUT caracteres, como a caixa do SA-MP.
        int count = 0;
        for (int i = 0; i < data->BufTextLen; i++) {
            if ((static_cast<unsigned char>(data->Buf[i]) & 0xC0) != 0x80 && ++count > MAX_INPUT) {
                data->DeleteChars(i, data->BufTextLen - i);
                break;
            }
        }
    }
    return 0;
}

// O texto a mandar, guardado no historico. O envio e feito depois de soltar a lista de mensagens: os comandos do
// proprio cliente (/pagesize, /timestamp, os dos mods) escrevem no chat na hora.
std::string Submit() {
    const std::string text = S.input;
    const bool empty = Trim(text).empty();
    if (!empty && (S.history.empty() || S.history.back() != text)) {
        S.history.push_back(text);
        if (S.history.size() > MAX_HISTORY) {
            S.history.erase(S.history.begin());
        }
    }
    Close();
    return empty ? std::string() : text;
}

} // namespace

void Add(const Message& m) {
    Entry e;
    e.msg = m;
    e.tokens = Tokenize(m);
    std::lock_guard<std::mutex> lock(g_lock);
    g_entries.push_back(std::move(e));
    g_added++;
    while (g_entries.size() > MAX_MESSAGES) {
        g_entries.pop_front();
    }
}

void Open() {
    if (S.open) {
        return;
    }
    S.open = true;
    S.focus = true;
    S.session++;
    S.active = false;
    S.pending.clear();
    S.input[0] = 0;
    S.historyPos = -1;
    S.scroll = 0;
    host::InputOpened(true);
}

void Close() {
    if (!S.open) {
        return;
    }
    S.open = false;
    S.input[0] = 0;
    S.scroll = 0;
    host::InputOpened(false);
}

bool IsOpen() {
    return S.open;
}

void Frame() {
    const host::ChatSettings cfg = host::Settings();
    if (S.open && host::DialogOpen()) {
        Close(); // um dialogo do servidor apareceu: o teclado e dele
    }
    const bool showLog = cfg.mode != 0;
    if (!S.open && !showLog) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    const float lineH = std::floor(fonts.body->FontSize + 6 * u);
    const float x0 = std::floor(34 * u), y0 = std::floor(26 * u);
    const float width = std::floor(std::min(io.DisplaySize.x - x0 - 40 * u, 760 * u));
    const int view = std::max(2, std::min(cfg.pageSize, 40));
    const float logH = showLog ? view * lineH : 0;
    const float pad = std::floor(12 * u);
    const float barH = std::floor(fonts.body->FontSize + 18 * u);
    const float barGap = showLog ? std::floor(10 * u) : 0;

    std::string send;
    std::unique_lock<std::mutex> lock(g_lock);
    int total = 0;
    for (Entry& e : g_entries) {
        Layout(e, width - 10 * u, cfg.timestamps); // a mesma aberto e fechado: as linhas nao mudam ao abrir
        total += e.lines;
    }
    // Rolagem: segura o ponto de leitura quando chega mensagem nova com a lista rolada para cima (contando as linhas
    // das mensagens novas, e nao o total: com 300 mensagens as antigas saem enquanto as novas entram).
    const unsigned arrived = g_added - S.lastAdded;
    S.lastAdded = g_added;
    if (S.open && S.scroll > 0 && arrived > 0) {
        int lines = 0;
        unsigned n = 0;
        for (auto it = g_entries.rbegin(); it != g_entries.rend() && n < arrived; ++it, ++n) {
            lines += it->lines;
        }
        S.scroll += lines;
    }
    const int maxScroll = std::max(0, total - view);
    if (S.scroll == 0) {
        S.seen = g_added;
    }

    ImVec2 panelMin(x0 - pad, y0 - pad);
    ImVec2 panelMax(x0 + width + pad, y0 + logH + (S.open ? barGap + barH : 0) + pad);
    ImGui::SetNextWindowPos(panelMin);
    ImGui::SetNextWindowSize(ImVec2(panelMax.x - panelMin.x, panelMax.y - panelMin.y));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    if (!S.open) {
        flags |= ImGuiWindowFlags_NoInputs;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##trokChat", nullptr, flags);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (S.open) {
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) S.scroll += view - 1;
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) S.scroll -= view - 1;
        // A roda pelo jogo (o GTA le o mouse pelo DirectInput; em tela cheia o Windows nem manda a mensagem dela) e,
        // enquanto ele nao responder, pelo Windows.
        int wheel = host::Wheel();
        if (wheel != 0) {
            S.gameWheel = true;
        } else if (!S.gameWheel) {
            wheel = static_cast<int>(std::lround(io.MouseWheel));
        }
        S.scroll += wheel * 3;
        S.scroll = std::max(0, std::min(S.scroll, maxScroll));
        dl->AddRectFilled(panelMin, panelMax, IM_COL32(10, 10, 12, 205), 10 * u);
        dl->AddRect(panelMin, panelMax, col::White(14), 10 * u);
    }

    if (showLog) {
        // De baixo para cima: a linha k (0 = a mais recente) fica na fileira view - 1 - (k - scroll).
        const bool overGame = !S.open, outline = overGame && cfg.mode == 2;
        dl->PushClipRect(ImVec2(x0 - 2, y0 - 2), ImVec2(x0 + width + 2, y0 + logH + 2), true);
        int k = 0;
        for (auto it = g_entries.rbegin(); it != g_entries.rend() && k < S.scroll + view; ++it) {
            const Entry& e = *it;
            const int first = k; // linha de baixo desta mensagem
            k += e.lines;
            if (k <= S.scroll) {
                continue;
            }
            for (const Placed& p : e.placed) {
                const int fromBottom = first + (e.lines - 1 - p.line);
                const int row = view - 1 - (fromBottom - S.scroll);
                if (row < 0 || row >= view) {
                    continue;
                }
                DrawPiece(dl, p, x0, y0 + row * lineH, lineH, overGame, outline);
            }
        }
        dl->PopClipRect();

        if (S.open && total > view) {
            // Barra de rolagem fina, como as listas dos menus da casa.
            const float bx = panelMax.x - 6 * u, bw = 3 * u;
            const float thumbH = std::max(18 * u, logH * view / total);
            const float travel = logH - thumbH;
            const float ty = y0 + travel * (1.0f - static_cast<float>(S.scroll) / maxScroll);
            dl->AddRectFilled(ImVec2(bx, y0), ImVec2(bx + bw, y0 + logH), col::White(10), bw);
            dl->AddRectFilled(ImVec2(bx, ty), ImVec2(bx + bw, ty + thumbH), col::White(80), bw);
        }
        if (S.open && S.scroll > 0) {
            // Rolado para cima: aviso das mensagens que chegaram desde entao (o Page Down desce).
            const unsigned unread = g_added - S.seen;
            char label[48];
            if (unread == 1) {
                std::snprintf(label, sizeof(label), "1 mensagem nova");
            } else if (unread > 1) {
                std::snprintf(label, sizeof(label), "%u mensagens novas", unread > 999 ? 999u : unread);
            } else {
                std::snprintf(label, sizeof(label), "Mais recentes");
            }
            const ImVec2 ts = TextSize(fonts.desc, label);
            const float h = std::floor(fonts.desc->FontSize + 10 * u), w = std::floor(ts.x + 34 * u);
            const ImVec2 a(std::floor(x0 + width - w - 10 * u), std::floor(y0 + logH - h - 4 * u)), b(a.x + w, a.y + h);
            const bool fresh = unread > 0;
            dl->AddRectFilled(a, b, fresh ? col::strong : IM_COL32(30, 30, 34, 235), h * 0.5f);
            if (!fresh) {
                dl->AddRect(a, b, col::White(30), h * 0.5f);
            }
            const ImU32 ink = fresh ? col::ink : col::text;
            DrawIcon(dl, Icon::ChevronDown, ImVec2(a.x + 13 * u, a.y + h * 0.5f), ink, true);
            Text(dl, fonts.desc, a.x + 24 * u, a.y + (h - ts.y) * 0.5f, ink, label);
        }
    }

    if (S.open) {
        const ImVec2 a(x0, y0 + logH + barGap), b(x0 + width, a.y + barH);
        dl->AddRectFilled(a, b, col::White(12), 8 * u);
        dl->AddRect(a, b, col::White(46), 8 * u);
        char counter[16];
        const int used = Codepoints(S.input);
        std::snprintf(counter, sizeof(counter), "%d/%d", used, MAX_INPUT);
        const ImVec2 cs = TextSize(fonts.desc, counter);
        const ImU32 counterColor = used >= MAX_INPUT ? IM_COL32(255, 110, 110, 255)
                                   : used >= MAX_INPUT - 16 ? IM_COL32(255, 200, 110, 255)
                                                            : col::hint;
        Text(dl, fonts.desc, b.x - 12 * u - cs.x, a.y + (barH - cs.y) * 0.5f, counterColor, counter);

        ImGui::PushFont(fonts.body);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, 0);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, 0);
        ImGui::PushStyleColor(ImGuiCol_Text, col::text);
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, col::hint);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, col::White(46));
        ImGui::SetCursorScreenPos(ImVec2(a.x + 12 * u, a.y + (barH - fonts.body->FontSize) * 0.5f));
        ImGui::SetNextItemWidth(b.x - a.x - 24 * u - cs.x - 12 * u);
        if (!S.active) {
            // O ImGui so entrega o teclado a barra um ou dois quadros depois do pedido e joga fora o que for digitado
            // antes: quem digita rapido logo depois do T perderia as primeiras letras. Elas ficam guardadas aqui.
            S.pending.insert(S.pending.end(), io.InputQueueCharacters.begin(), io.InputQueueCharacters.end());
            io.InputQueueCharacters.resize(0);
        }
        if (S.focus) {
            ImGui::SetKeyboardFocusHere();
            S.focus = false;
        }
        const ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                                               ImGuiInputTextFlags_CallbackHistory |
                                               ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackAlways;
        // Um id por abertura: o ImGui guarda a rolagem do texto e o desfazer da barra pelo id, e o texto anterior
        // (um longo, rolado para o lado) esconderia a dica e o cursor da barra vazia.
        ImGui::PushID(S.session);
        const bool enter = ImGui::InputTextWithHint("##mensagem", "Mensagem ou /comando", S.input, sizeof(S.input),
                                                    inputFlags, InputCallback);
        const bool active = ImGui::IsItemActive();
        ImGui::PopID();
        S.active = active;
        if (active && !S.pending.empty()) {
            for (ImWchar c : S.pending) {
                io.AddInputCharacter(c); // a barra recebe no proximo quadro, na ordem em que foram digitadas
            }
            S.pending.clear();
        }
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(2);
        ImGui::PopFont();
        if (enter) {
            send = Submit();
        } else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            Close();
        } else if (!active) {
            S.focus = true; // a barra nao perde o teclado enquanto estiver aberta
            S.caretToEnd = S.input[0] != 0;
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
    lock.unlock();
    if (!send.empty()) {
        host::Send(send.c_str());
    }
}

} // namespace chat

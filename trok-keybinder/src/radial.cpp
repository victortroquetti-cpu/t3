// Trok KeyBinder -- menu circular e bolinha da mira. Ver radial.h.

#include "radial.h"

#include "trok_ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace radial {
namespace {

using namespace tui;

constexpr float PI = 3.14159265f;

struct State {
    bool open = false;
    std::vector<Item> items;
    std::string title, subtitle;
    float openedAt = 0;
    int page = 0;
    bool released = false;
} S;

struct ListState {
    bool open = false;
    std::vector<Item> items;
    std::string title, subtitle;
    float openedAt = 0;
    float hiddenAt = -1; // quando o jogador sumiu da visao (-1 = esta na tela)
    ImVec2 lastAnchor;   // onde ele estava (a lista fica ali na folga)
    bool anchored = false;
    int selected = 0;
    bool released = false;
    std::vector<ImVec2> centers;
} L;

float Ease(float t) {
    t = std::min(1.0f, std::max(0.0f, t));
    return 1 - (1 - t) * (1 - t) * (1 - t);
}

ImU32 Alpha(ImU32 c, float a) {
    int alpha = static_cast<int>(((c >> IM_COL32_A_SHIFT) & 0xFF) * a);
    return (c & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}

// Ate 2 linhas que cabem em width (a segunda com "..." se nao couber).
std::vector<std::string> Wrap(ImFont* font, const std::string& text, float width) {
    std::vector<std::string> words;
    size_t a = 0;
    while (a < text.size()) {
        size_t e = text.find(' ', a);
        if (e == std::string::npos) {
            e = text.size();
        }
        if (e > a) {
            words.push_back(text.substr(a, e - a));
        }
        a = e + 1;
    }
    std::vector<std::string> lines(1);
    for (size_t i = 0; i < words.size(); ++i) {
        std::string tryLine = lines.back().empty() ? words[i] : lines.back() + " " + words[i];
        if (TextSize(font, tryLine.c_str()).x <= width || lines.back().empty()) {
            lines.back() = tryLine;
        } else if (lines.size() < 2) {
            lines.push_back(words[i]);
        } else {
            lines.back() = tryLine;
        }
    }
    for (std::string& l : lines) {
        if (TextSize(font, l.c_str()).x <= width) {
            continue;
        }
        while (l.size() > 1 && TextSize(font, (l + "...").c_str()).x > width) {
            do {
                l.pop_back();
            } while (!l.empty() && (static_cast<unsigned char>(l.back()) & 0xC0) == 0x80);
            if (!l.empty() && (static_cast<unsigned char>(l.back()) & 0xC0) == 0xC0) {
                l.pop_back();
            }
        }
        l += "...";
    }
    return lines;
}

// Fatia do anel (entre os raios r0 e r1, dos angulos a0 a a1) com bordas retas e um vao de largura constante
// (gap px) para a vizinha. Triangulos na mao (o anel nao e convexo) e um contorno fino por cima para suavizar.
void Sector(ImDrawList* dl, ImVec2 c, float r0, float r1, float a0, float a1, float gap, ImU32 color) {
    float o0 = a0 + std::asin(std::min(0.9f, gap * 0.5f / r1)), o1 = a1 - std::asin(std::min(0.9f, gap * 0.5f / r1));
    float i0 = a0 + std::asin(std::min(0.9f, gap * 0.5f / r0)), i1 = a1 - std::asin(std::min(0.9f, gap * 0.5f / r0));
    if (o1 <= o0 || i1 <= i0) {
        return;
    }
    int n = std::max(8, static_cast<int>((a1 - a0) / (2 * PI) * 128));
    std::vector<ImVec2> outer(n + 1), inner(n + 1);
    for (int k = 0; k <= n; ++k) {
        float t = static_cast<float>(k) / n;
        float ao = o0 + (o1 - o0) * t, ai = i0 + (i1 - i0) * t;
        outer[k] = ImVec2(c.x + std::cos(ao) * r1, c.y + std::sin(ao) * r1);
        inner[k] = ImVec2(c.x + std::cos(ai) * r0, c.y + std::sin(ai) * r0);
    }
    ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    dl->PrimReserve(n * 6, (n + 1) * 2);
    unsigned base = dl->_VtxCurrentIdx;
    for (int k = 0; k <= n; ++k) {
        dl->PrimWriteVtx(outer[k], uv, color);
        dl->PrimWriteVtx(inner[k], uv, color);
    }
    for (int k = 0; k < n; ++k) {
        ImDrawIdx a = static_cast<ImDrawIdx>(base + k * 2), b = static_cast<ImDrawIdx>(a + 1);
        ImDrawIdx c2 = static_cast<ImDrawIdx>(a + 2), d = static_cast<ImDrawIdx>(a + 3);
        dl->PrimWriteIdx(a);
        dl->PrimWriteIdx(c2);
        dl->PrimWriteIdx(b);
        dl->PrimWriteIdx(b);
        dl->PrimWriteIdx(c2);
        dl->PrimWriteIdx(d);
    }
    std::vector<ImVec2> edge(outer);
    edge.insert(edge.end(), inner.rbegin(), inner.rend());
    dl->AddPolyline(edge.data(), static_cast<int>(edge.size()), color, ImDrawFlags_Closed, 1.0f);
}

// 1..9 apertado neste quadro (fileira de cima ou teclado numerico); 0 = nenhum.
int DigitPressed() {
    for (int d = 1; d <= 9; ++d) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_0 + d), false) ||
            ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_Keypad0 + d), false)) {
            return d;
        }
    }
    return 0;
}

void Centered(ImDrawList* dl, ImFont* font, ImVec2 c, ImU32 color, const std::string& text) {
    ImVec2 ts = TextSize(font, text.c_str());
    Text(dl, font, c.x - ts.x * 0.5f, c.y - ts.y * 0.5f, color, text.c_str());
}

} // namespace

void Open(const std::vector<Item>& items, const std::string& title, const std::string& subtitle, float time) {
    S = State();
    S.open = true;
    S.items = items;
    S.title = title;
    S.subtitle = subtitle;
    S.openedAt = time;
}

void Close() {
    S.open = false;
    L.open = false;
}

bool IsOpen() {
    return S.open || L.open;
}

int Frame(ImDrawList* dl, ImVec2 center, float time, bool keyDown, bool pressedAgain, float size) {
    if (!S.open) {
        return -1;
    }
    ImGuiIO& io = ImGui::GetIO();
    float k = size / 100.0f;
    float appear = Ease((time - S.openedAt) / 0.14f);
    float sc = (0.9f + 0.1f * appear) * k;
    float outerR = 196 * u * sc, innerR = 82 * u * sc, hubR = 70 * u * sc;
    int n = static_cast<int>(S.items.size());
    int pages = std::max(1, (n + PAGE - 1) / PAGE);
    if (pages > 1 && io.MouseWheel != 0) {
        S.page = (S.page + (io.MouseWheel < 0 ? 1 : pages - 1)) % pages;
    }
    S.page = std::min(S.page, pages - 1);
    int first = S.page * PAGE;
    int count = std::min(PAGE, n - first);

    // Onde o mouse esta: no meio, numa fatia ou fora.
    float dx = io.MousePos.x - center.x, dy = io.MousePos.y - center.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    bool hubHover = dist < hubR;
    int hover = -1;
    float step = count > 0 ? 2 * PI / count : 2 * PI;
    if (count > 0 && dist >= innerR * 0.6f && dist <= outerR + 70 * u * k) {
        float angle = std::atan2(dy, dx) + PI * 0.5f; // 0 = em cima, sentido horario
        while (angle < 0) angle += 2 * PI;
        hover = static_cast<int>(std::floor((angle + step * 0.5f) / step)) % count;
    }
    if (hover >= 0 || hubHover) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    int chosen = -1;
    bool close = false;
    if (ImGui::IsMouseClicked(0)) {
        if (hover >= 0) {
            chosen = first + hover;
        } else {
            close = true; // no meio ou fora do circulo
        }
    }
    int digit = DigitPressed();
    if (digit > 0 && digit <= count) {
        chosen = first + digit - 1; // o numero da fatia, como nas listas da casa
    }
    if (ImGui::IsMouseClicked(1) || ImGui::IsKeyPressed(ImGuiKey_Escape, false) || pressedAgain) {
        close = true;
    }
    // Segurar o atalho, apontar e soltar. Um toque rapido deixa o circulo aberto para clicar.
    if (!keyDown && !S.released) {
        S.released = true;
        if (time - S.openedAt >= 0.35f) {
            if (hover >= 0) {
                chosen = first + hover;
            } else if (!hubHover) {
                close = true;
            }
        }
    }

    // Fundo, fatias, textos.
    dl->AddCircleFilled(center, outerR + 16 * u * sc, IM_COL32(0, 0, 0, static_cast<int>(70 * appear)), 96);
    float midR = (outerR + innerR) * 0.5f;
    float gap = count > 1 ? 6 * u * sc : 0.0f;
    for (int i = 0; i < count; ++i) {
        bool lit = i == hover;
        float grow = Anim(ImGui::GetID("##kbFatia") + static_cast<ImGuiID>(i), lit ? 1.0f : 0.0f, 18.0f);
        float out = outerR + 9 * u * sc * grow;
        float ac = -PI * 0.5f + i * step;
        float thick = out - innerR;
        ImU32 fill = lit ? IM_COL32(226, 226, 226, 246) : IM_COL32(14, 14, 14, 226);
        if (count == 1) {
            dl->AddCircle(center, (out + innerR) * 0.5f, Alpha(fill, appear), 96, thick);
        } else {
            Sector(dl, center, innerR, out, ac - step * 0.5f, ac + step * 0.5f, gap, Alpha(fill, appear));
        }
        const Item& it = S.items[first + i];
        ImVec2 at(center.x + std::cos(ac) * midR, center.y + std::sin(ac) * midR);
        float chord = count == 1 ? thick * 2.2f : 2 * midR * std::sin(std::min(step, PI) * 0.5f) * 0.82f;
        float maxW = std::min(chord, thick * 1.45f);
        std::vector<std::string> lines = Wrap(fonts.body, it.label.empty() ? std::string("(sem nome)") : it.label, maxW);
        float lineH = fonts.body->FontSize * 1.08f;
        float tagH = it.tag.empty() ? 0.0f : fonts.desc->FontSize * 1.2f;
        float numH = fonts.desc->FontSize * 1.15f;
        float total = numH + lines.size() * lineH + tagH;
        float y = at.y - total * 0.5f + lineH * 0.5f;
        char number[4];
        snprintf(number, sizeof(number), "%d", i + 1);
        Centered(dl, fonts.desc, ImVec2(at.x, y - lineH * 0.5f + numH * 0.5f),
                 Alpha(lit ? IM_COL32(18, 18, 18, 150) : col::number, appear), number);
        y += numH;
        ImU32 textColor = Alpha(lit ? col::ink : col::text, appear);
        for (const std::string& l : lines) {
            Centered(dl, fonts.body, ImVec2(at.x, y), textColor, l);
            y += lineH;
        }
        if (!it.tag.empty()) {
            ImU32 tagColor = lit ? IM_COL32(18, 18, 18, 170) : col::hint;
            Centered(dl, fonts.desc, ImVec2(at.x, y - lineH * 0.5f + tagH * 0.5f + 1 * u), Alpha(tagColor, appear), it.tag);
        }
    }

    // Meio: o alvo (ou "Binds") e as paginas; com o mouse em cima, "Fechar".
    dl->AddCircleFilled(center, hubR, Alpha(IM_COL32(12, 12, 12, 244), appear), 64);
    dl->AddCircle(center, hubR + 0.5f, Alpha(col::White(hubHover ? 40 : 16), appear), 64, 1.0f);
    if (hubHover) {
        DrawIcon(dl, Icon::Close, ImVec2(center.x, center.y - 9 * u * k), Alpha(col::text, appear));
        Centered(dl, fonts.desc, ImVec2(center.x, center.y + 13 * u * k), Alpha(col::hint, appear), "Fechar");
    } else if (count == 0) {
        Centered(dl, fonts.body, ImVec2(center.x, center.y - 9 * u * k), Alpha(col::text, appear), "Nenhuma bind");
        Centered(dl, fonts.desc, ImVec2(center.x, center.y + 11 * u * k), Alpha(col::hint, appear), "no menu");
    } else {
        bool sub = !S.subtitle.empty();
        float ty = center.y - (sub ? 9 * u * k : 0) - (pages > 1 ? 6 * u * k : 0);
        std::vector<std::string> t = Wrap(fonts.title, S.title, hubR * 1.7f);
        Centered(dl, fonts.title, ImVec2(center.x, ty), Alpha(col::text, appear), t[0]);
        if (sub) {
            Centered(dl, fonts.desc, ImVec2(center.x, ty + 20 * u * k), Alpha(col::hint, appear), S.subtitle);
        }
        if (pages > 1) {
            float dotsY = center.y + (sub ? 30 : 18) * u * k;
            float spacing = 10 * u * k;
            float x0 = center.x - (pages - 1) * spacing * 0.5f;
            for (int p = 0; p < pages; ++p) {
                dl->AddCircleFilled(ImVec2(x0 + p * spacing, dotsY), 2.6f * u * k,
                                    Alpha(p == S.page ? col::text : col::White(60), appear), 12);
            }
        }
    }
    if (pages > 1) {
        char hint[64];
        snprintf(hint, sizeof(hint), "Roda do mouse: p\xC3\xA1gina %d de %d", S.page + 1, pages);
        Centered(dl, fonts.desc, ImVec2(center.x, center.y + outerR + 34 * u * sc), Alpha(col::hint, appear), hint);
    }

    if (chosen >= 0 && chosen < n) {
        S.open = false;
        return S.items[chosen].bind;
    }
    if (close) {
        S.open = false;
    }
    return -1;
}

void OpenList(const std::vector<Item>& items, const std::string& title, const std::string& subtitle, float time) {
    S.open = false;
    L = ListState();
    L.open = true;
    L.items = items;
    L.title = title;
    L.subtitle = subtitle;
    L.openedAt = time;
}

bool ListOpen() {
    return L.open;
}

ImVec2 ListItemCenter(int i) {
    return i >= 0 && i < static_cast<int>(L.centers.size()) ? L.centers[i] : ImVec2(-1, -1);
}

int ListFrame(ImDrawList* dl, ImVec2 anchor, bool visible, float time, bool keyDown, bool pressedAgain, float size) {
    if (!L.open) {
        return -1;
    }
    // O jogador sumiu (saiu da tela, ficou longe): da 0,3 s de folga (um poste na frente) e fecha.
    if (!visible) {
        if (L.hiddenAt < 0) {
            L.hiddenAt = time;
        }
        if (time - L.hiddenAt > 0.3f) {
            L.open = false;
            return -1;
        }
    } else {
        L.hiddenAt = -1;
    }
    if (visible) {
        L.lastAnchor = anchor;
        L.anchored = true;
    } else if (L.anchored) {
        anchor = L.lastAnchor;
    }
    ImGuiIO& io = ImGui::GetIO();
    float k = size / 100.0f;
    float appear = Ease((time - L.openedAt) / 0.14f);
    int n = static_cast<int>(L.items.size());
    float rowH = 30 * u * k, pad = 12 * u * k, headH = 46 * u * k, gapX = 30 * u * k, radius = 8 * u * k;

    // Largura: o maior entre o nick e os itens (com a seta), entre 190u e 340u.
    float numW = TextSize(fonts.desc, "9").x + 8 * u * k; // o numero sutil antes do nome
    float width = TextSize(fonts.body, L.title.c_str()).x + 2 * pad;
    for (const Item& it : L.items) {
        width = std::max(width, TextSize(fonts.body, it.label.c_str()).x + 2 * pad + numW + 22 * u * k);
    }
    width = std::min(std::max(width, 190 * u * k), 340 * u * k);
    float height = headH + std::max(1, n) * rowH + pad * 0.5f;

    // Do lado direito da bolinha; sem espaco, do esquerdo. O cabecalho fica na altura dela.
    bool left = anchor.x + gapX + width > io.DisplaySize.x - 8 * u;
    float x = left ? anchor.x - gapX - width : anchor.x + gapX;
    float y = std::min(std::max(anchor.y - headH * 0.5f, 8 * u), io.DisplaySize.y - height - 8 * u);
    float slide = (1 - appear) * 10 * u * (left ? 1 : -1);
    x += slide;

    // Item sob o mouse; setas e Enter tambem andam.
    int hover = -1;
    L.centers.assign(n, ImVec2());
    for (int i = 0; i < n; ++i) {
        float ry = y + headH + i * rowH;
        L.centers[i] = ImVec2(x + width * 0.5f, ry + rowH * 0.5f);
        if (io.MousePos.x >= x && io.MousePos.x < x + width && io.MousePos.y >= ry && io.MousePos.y < ry + rowH) {
            hover = i;
        }
    }
    bool inside = io.MousePos.x >= x && io.MousePos.x < x + width && io.MousePos.y >= y && io.MousePos.y < y + height;
    if (hover >= 0) {
        L.selected = hover;
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if (n > 0 && ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) L.selected = (L.selected + 1) % n;
    if (n > 0 && ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) L.selected = (L.selected + n - 1) % n;

    int chosen = -1;
    bool close = false;
    if (ImGui::IsMouseClicked(0)) {
        if (hover >= 0) {
            chosen = hover;
        } else if (!inside) {
            close = true;
        }
    }
    if (n > 0 && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
        chosen = L.selected;
    }
    int digit = DigitPressed();
    if (digit > 0 && digit <= n) {
        chosen = digit - 1;
    }
    if (ImGui::IsMouseClicked(1) || ImGui::IsKeyPressed(ImGuiKey_Escape, false) || pressedAgain) {
        close = true;
    }
    if (!keyDown && !L.released) {
        L.released = true;
        if (time - L.openedAt >= 0.35f && hover >= 0) {
            chosen = hover;
        }
    }

    // A bolinha (ativa), o fio ate o cabecalho e o painel.
    float a = appear;
    ImVec2 joint(left ? x + width : x, y + headH * 0.5f);
    dl->AddLine(anchor, joint, Alpha(col::White(110), a), 1.5f * u);
    float r = 6.0f * u * k;
    dl->AddCircleFilled(anchor, r + 3.5f * u, IM_COL32(0, 0, 0, static_cast<int>(140 * a)), 24);
    dl->AddCircleFilled(anchor, r + 1.5f * u, Alpha(IM_COL32(240, 240, 240, 255), a), 24);
    dl->AddCircleFilled(anchor, r * 0.45f, Alpha(IM_COL32(18, 18, 18, 255), a), 16);
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), Alpha(IM_COL32(12, 12, 12, 244), a), radius);
    dl->AddRect(ImVec2(x - 0.5f, y - 0.5f), ImVec2(x + width + 0.5f, y + height + 0.5f), Alpha(col::White(18), a),
                radius, 0, 1.0f);
    Text(dl, fonts.body, x + pad, y + 9 * u * k, Alpha(col::text, a), L.title.c_str());
    if (!L.subtitle.empty()) {
        Text(dl, fonts.desc, x + pad, y + 9 * u * k + fonts.body->FontSize + 2 * u, Alpha(col::hint, a),
             L.subtitle.c_str());
    }
    dl->AddLine(ImVec2(x + pad, y + headH - 1), ImVec2(x + width - pad, y + headH - 1), Alpha(col::separator, a), 1.0f);
    if (n == 0) {
        Text(dl, fonts.desc, x + pad, y + headH + (rowH - fonts.desc->FontSize) * 0.5f, Alpha(col::hint, a),
             "Nenhuma bind da vis\xC3\xA3o no menu");
    }
    for (int i = 0; i < n; ++i) {
        float ry = y + headH + i * rowH;
        bool lit = i == L.selected;
        if (lit) {
            dl->AddRectFilled(ImVec2(x + 4 * u, ry + 1), ImVec2(x + width - 4 * u, ry + rowH - 1),
                              Alpha(i == hover ? col::White(16) : col::White(10), a), 6 * u);
        }
        float ty = ry + (rowH - fonts.body->FontSize) * 0.5f;
        if (i < 9) {
            char number[4];
            snprintf(number, sizeof(number), "%d", i + 1);
            Text(dl, fonts.desc, x + pad, ry + (rowH - fonts.desc->FontSize) * 0.5f,
                 Alpha(lit ? col::numberSel : col::number, a), number);
        }
        std::vector<std::string> one = Wrap(fonts.body, L.items[i].label, width - 2 * pad - numW - 22 * u * k);
        Text(dl, fonts.body, x + pad + numW, ty, Alpha(lit ? col::text : col::column, a), one[0].c_str());
        DrawIcon(dl, Icon::ChevronRight, ImVec2(x + width - pad - 4 * u, ry + rowH * 0.5f),
                 Alpha(lit ? col::text : col::hint, a), true);
    }

    if (chosen >= 0 && chosen < n) {
        L.open = false;
        return L.items[chosen].bind;
    }
    if (close) {
        L.open = false;
    }
    return -1;
}

void DrawDot(ImDrawList* dl, ImVec2 at, float time) {
    float phase = std::fmod(time, 1.4f) / 1.4f;
    float r = 6.0f * u;
    dl->AddCircle(at, r + 10 * u * phase, col::White(static_cast<int>(120 * (1 - phase))), 32, 1.6f * u);
    dl->AddCircleFilled(at, r + 2.6f * u, IM_COL32(0, 0, 0, 130), 24);
    dl->AddCircleFilled(at, r, IM_COL32(240, 240, 240, 255), 24);
}

} // namespace radial

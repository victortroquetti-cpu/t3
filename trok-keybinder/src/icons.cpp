// Trok KeyBinder -- icones das binds. Ver icons.h; os pontos vem de icons_data.h (tools/gen_icons.py).

#include "icons.h"

#include "icons_data.h"

#include <algorithm>
#include <vector>

namespace icons {

int Count() {
    return data::COUNT;
}

const char* Name(int i) {
    return i >= 0 && i < data::COUNT ? data::ICONS[i].name : "";
}

const char* Label(int i) {
    return i >= 0 && i < data::COUNT ? data::ICONS[i].label : "";
}

int Find(const std::string& name) {
    for (int i = 0; i < data::COUNT; ++i) {
        if (name == data::ICONS[i].name) {
            return i;
        }
    }
    return -1;
}

void Draw(ImDrawList* dl, int i, ImVec2 c, float size, ImU32 color) {
    if (i < 0 || i >= data::COUNT) {
        return;
    }
    float k = size / 24.0f / 64.0f;
    float thick = std::max(1.3f, 2.0f * size / 24.0f);
    const data::Icon& icon = data::ICONS[i];
    std::vector<ImVec2> pts;
    for (int s = 0; s < icon.strokeCount; ++s) {
        const data::Stroke& st = data::STROKES[icon.firstStroke + s];
        pts.resize(st.count);
        for (int p = 0; p < st.count; ++p) {
            const short* v = &data::POINTS[(st.first + p) * 2];
            pts[p] = ImVec2(c.x + (v[0] - 12 * 64) * k, c.y + (v[1] - 12 * 64) * k);
        }
        dl->AddPolyline(pts.data(), st.count, color, st.closed ? ImDrawFlags_Closed : 0, thick);
        if (!st.closed) {
            // As pontas redondas do Lucide.
            dl->AddCircleFilled(pts.front(), thick * 0.5f, color, 8);
            dl->AddCircleFilled(pts.back(), thick * 0.5f, color, 8);
        }
    }
}

} // namespace icons

// Trok Chat (.asi) -- Victor_Trok
// As opcoes do Trok Chat e os nomes das teclas (ver options.h).

#include "options.h"

#include <cctype>
#include <cstring>

namespace opt {
namespace {

Options g_options;
const Options g_defaults;

bool SameName(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b))) {
            return false;
        }
    }
    return *a == *b;
}

} // namespace

Options& Get() {
    return g_options;
}

const Options& Defaults() {
    return g_defaults;
}

bool IsOpenKey(ImGuiKey key) {
    return key != ImGuiKey_None && (key == g_options.keys[0] || key == g_options.keys[1]);
}

const char* KeyName(ImGuiKey key) {
    return key == ImGuiKey_None ? "Nenhuma" : ImGui::GetKeyName(key);
}

ImGuiKey KeyFromName(const char* name) {
    if (!name || !*name || SameName(name, "Nenhuma")) {
        return ImGuiKey_None;
    }
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
        const ImGuiKey key = static_cast<ImGuiKey>(k);
        const bool mouse = key >= ImGuiKey_MouseLeft && key <= ImGuiKey_MouseWheelY;
        const bool modifier = key >= ImGuiKey_ReservedForModCtrl;
        if (!mouse && !modifier && key != ImGuiKey_Escape && SameName(name, ImGui::GetKeyName(key))) {
            return key;
        }
    }
    return ImGuiKey_None;
}

} // namespace opt

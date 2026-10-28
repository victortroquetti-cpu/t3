#pragma once

// Trok KeyBinder -- os icones das binds (Lucide, desenhados em tracos: o lucide.ttf da casa e um recorte com poucos
// icones). A bind guarda o nome do Lucide ("car"); o circulo e a lista desenham o icone dela.

#include "imgui.h"

#include <string>

namespace icons {

int Count();
const char* Name(int i);  // o do Lucide
const char* Label(int i); // o que aparece no seletor (UTF-8)
int Find(const std::string& name); // -1 = nenhum

// O icone i centrado em c, com size px de lado (o grid de 24 do Lucide) e o traco de 2 do Lucide.
void Draw(ImDrawList* dl, int i, ImVec2 c, float size, ImU32 color);

} // namespace icons

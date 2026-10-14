// Trok Hitmarker (.asi) -- Victor_Trok
// "Trok Hitmarker.ini": [padrao] e uma secao por servidor ("ip:porta"), como no Trok Tags.
#pragma once

namespace cfg {

void Load(const char* path, const char* server); // padroes, [padrao] e a secao do servidor por cima
void Save(const char* path, const char* server); // grava no [padrao] e na secao do servidor

} // namespace cfg

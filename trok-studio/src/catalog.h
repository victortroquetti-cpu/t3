// Trok Studio (.asi) -- Victor_Trok
// O catalogo de modelos: todo id que o jogo conhece (a tabela de modelos, inclusive os do samp.img e de mods), com o
// nome do DFF e do TXD (dos .ide do gta.dat/default.dat e dos diretorios dos .img), o .ide que define, o .img de onde
// o jogo le e se esse arquivo e outro que nao a entrada com o mesmo nome no .img (modelo substituido).
#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace catalog {

enum Kind { K_OBJECT, K_VEHICLE, K_PED, K_WEAPON, KIND_COUNT };

struct Entry {
    int id = 0;
    int type = 0;          // gta::ModelType
    int kind = K_OBJECT;   // categoria do filtro
    std::string section;   // objs, tobj, anim, peds, cars, weap, hier ("" = fora dos .ide lidos)
    std::string dff, txd;  // sem extensao
    std::string ide;       // "DATA\MAPS\LA\LAE.IDE"
    std::string txdParent; // pai do TXD (txdp), se houver
    int img = -1;          // slot do .img no streaming
    std::string imgName;   // "gta3.img"
    DWORD posn = 0, size = 0;
    std::string imgEntry;  // a entrada do .img naquela posicao ("" = nenhuma)
    bool replaced = false; // o arquivo lido nao e a entrada do .img com o nome do .ide
    bool noFile = false;   // sem arquivo no streaming (nada para carregar)
    int txdIndex = -1;
    float drawDistance = 0;
    WORD flags = 0;
    std::string search; // minusculas: "1337 trashcan01 trashcan gta3.img"
};

// Comeca a ler os arquivos (gta.dat, default.dat, .ide, diretorios dos .img) numa thread. gameDir termina em '\'.
void Start(const char* gameDir);
// Chamado todo quadro na thread do jogo: quando os arquivos ficam prontos, monta o catalogo com a tabela de modelos.
void Update();
bool Ready();
const char* Status(); // o que esta fazendo (para a tela de espera)

const std::vector<Entry>& All();
const Entry* Find(int id);
// Indices em All() que passam no filtro. kind -1 = todos; archive -1 = todos (indice em Archives()).
std::vector<int> Filter(const char* query, int kind, int archive, bool onlyReplaced);
const std::vector<std::string>& Archives(); // os .img que aparecem no catalogo, em ordem
const char* KindName(int kind);
const char* TypeName(const Entry& e); // "Objeto", "Objeto de horario", "Veiculo"...

} // namespace catalog

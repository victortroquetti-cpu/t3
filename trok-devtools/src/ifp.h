// Trok DevTools (.asi) -- Victor_Trok
// As animacoes do jogo: o anim\ped.ifp e os .ifp de dentro do anim\anim.img, lidos numa thread (ANP3, ANP2 e ANPK, como
// o CAnimManager::LoadAnimFile). Cada animacao ganha uma impressao digital (os bytes dela no arquivo), para comparar com
// as originais:
//   - com uma referencia (a pasta Trok DevTools\originais com o anim.img e o ped.ifp de um jogo sem mods, ou o
//     Trok DevTools\animacoes_originais.txt gerado dela): MOD = diferente da original, NOVA = nao existe na original;
//   - sem referencia: NOVA = o nome nao esta na lista das animacoes do jogo (a tabela do open.mp, anim_names.h).
// Tambem decodifica os quadros-chave de uma animacao, para a previa.
#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace ifp {

enum Tag { TAG_NONE = 0, TAG_MOD = 1, TAG_NEW = 2 };

struct Anim {
    int index = 0; // em Anims()
    int file = 0;  // em Files()
    std::string name;
    float seconds = 0;
    int tracks = 0;  // ossos com quadros-chave
    int frames = 0;  // o maior numero de quadros-chave de um osso
    bool moves = false; // a raiz anda (andar, correr): na previa da para deixar no lugar
    bool compressed = true;
    DWORD offset = 0, size = 0; // no arquivo de origem (anim.img ou ped.ifp)
    DWORD hash = 0;
    int tag = TAG_NONE;
    int sampId = -1; // o indice do SA-MP (GetPlayerAnimationIndex); -1 = nao esta na lista do jogo
    std::string search;
};

struct File {
    std::string entry;  // "bar.ifp" (o nome no anim.img) ou "ped.ifp"
    std::string lib;    // "BAR": o nome que o jogo e os scripts usam (o nome do arquivo, sem o .ifp)
    std::string block;  // o nome interno do .ifp (o jogo guarda as animacoes no bloco com esse nome)
    std::string source; // "anim\\anim.img" ou "anim\\ped.ifp"
    std::string format; // "ANP3", "ANP2" ou "ANPK"
    DWORD offset = 0, bytes = 0; // dentro do arquivo de origem
    std::vector<int> anims;
    bool known = false;    // um .ifp do jogo original
    bool renamed = false;  // nome interno diferente do nome do arquivo (os scripts nao acham as animacoes)
    bool broken = false;   // nao deu para ler ate o fim
    int mods = 0, news = 0;
    std::vector<std::string> missing; // originais que nao estao no arquivo
    std::string search;
};

void Start(const char* gameDir); // thread
bool Ready();
const char* Status();
const std::vector<File>& Files();
const std::vector<Anim>& Anims();
const File* FindFile(int index);
const Anim* FindAnim(int index);
int ModCount();
int NewCount();
// De onde veio a referencia das originais: "" = sem referencia (so as novas pelos nomes)
const char* Reference();

// Filtro da aba: busca (todas as palavras), um .ifp so (-1 = todos) e so as modificadas/novas.
struct Filter {
    std::string query;
    int file = -1;
    bool changed = false;
    bool operator==(const Filter& o) const {
        return query == o.query && file == o.file && changed == o.changed;
    }
};
struct Result {
    std::vector<std::vector<int>> anims; // por .ifp (indice = arquivo): as animacoes que passam
    int files = 0, total = 0;
};
Result Apply(const Filter& f);

// Os quadros-chave de uma animacao (o rotacao como o jogo usa: o ANPK ja vem conjugado).
struct Key {
    float time;  // segundos, desde o comeco
    float q[4];  // x, y, z, w
    float t[3];  // so com translacao
};
struct Track {
    int bone = -1; // a etiqueta do osso (BONE_ROOT = 0, ...); -1 = pelo nome
    std::string name;
    bool translation = false;
    std::vector<Key> keys;
};
struct Clip {
    int anim = -1;
    float seconds = 0;
    std::vector<Track> tracks;
};
bool LoadClip(int anim, Clip* out); // le do arquivo

} // namespace ifp

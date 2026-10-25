// Trok DevTools (.asi) -- Victor_Trok
// O lado do gta_sa.exe 1.0 US: tabela de modelos, streaming (carregar e soltar um modelo), TXDs, camera, linha de
// visao, projecao na tela e os comandos de script que criam e apagam coisas no mundo. Tudo roda na thread do jogo.
#pragma once

#include <windows.h>

#include <string>

namespace gta {

struct Vec3 {
    float x, y, z;
};

bool VersionOk(); // gta_sa.exe 1.0 US
bool Readable(const void* p, size_t size);
inline bool Ptr(DWORD p) {
    return p >= 0x10000;
}
template <class T>
T At(DWORD base, DWORD offset) {
    return *reinterpret_cast<const T*>(base + offset);
}

// ---------------------------------------------------------------- modelos

constexpr int MODEL_COUNT = 20000;
constexpr int TXD_FIRST = 20000; // os TXDs vem logo depois dos modelos na tabela do streaming
enum ModelType { MT_NONE = 0, MT_ATOMIC = 1, MT_TIME = 3, MT_WEAPON = 4, MT_CLUMP = 5, MT_VEHICLE = 6, MT_PED = 7, MT_LOD = 8 };

DWORD ModelInfo(int id); // CBaseModelInfo* (0 = o id nao e usado)
int TypeOf(DWORD info);  // ModelType (pela vtable)
int TxdIndex(DWORD info);
float DrawDistance(DWORD info);
WORD Flags(DWORD info);
DWORD ColModel(DWORD info);
DWORD RwObjectOf(DWORD info); // RpAtomic* ou RpClump* (0 = nao carregado)

// ---------------------------------------------------------------- streaming

struct StreamInfo {
    int img = -1;      // indice em CStreaming::ms_files
    DWORD posn = 0;    // em setores de 2048 bytes dentro do .img
    DWORD size = 0;    // em setores (0 = sem arquivo)
    int loadState = 0; // 1 = carregado
    int flags = 0;
};
bool Streaming(int resource, StreamInfo* out);
int ImgSlots();                 // 8
std::string ImgPath(int index); // "MODELS\GTA3.IMG" (vazio = slot livre)

// Segura um modelo carregado: pede com a marca de missao (o streaming nao tira) e, no Release, devolve como estava.
struct Hold {
    int id = -1;
    bool added = false; // a marca foi o mod que pos
};
bool Load(int id, Hold* hold); // pede e carrega na hora (bloqueia); true = carregado com RwObject
void Release(Hold* hold);

// ---------------------------------------------------------------- TXDs

DWORD TxdDictionary(int txdIndex); // RwTexDictionary* (0 = nao carregado)
int TxdParent(int txdIndex);       // -1 = sem pai

// ---------------------------------------------------------------- mundo

DWORD PlayerPed();
bool EntityMatrix(DWORD entity, Vec3* right, Vec3* forward, Vec3* up, Vec3* pos);
int EntityModel(DWORD entity);
int EntityType(DWORD entity); // 1 predio, 2 veiculo, 3 personagem, 4 objeto, 5 dummy
int EntityArea(DWORD entity); // interior
DWORD EntityLod(DWORD entity);

struct Camera {
    Vec3 pos, forward, up, right;
};
bool CameraNow(Camera* out);
// Primeira coisa na linha (predios, veiculos, personagens, objetos, dummies). 0 = nada.
DWORD LineOfSight(const Vec3& from, const Vec3& to, Vec3* hit);
bool Project(const Vec3& world, float* x, float* y); // na tela; false = atras da camera

// ---------------------------------------------------------------- comandos de script

enum SpawnKind { SPAWN_OBJECT, SPAWN_VEHICLE, SPAWN_PED };
// O modelo tem que estar carregado. heading em graus.
bool Spawn(SpawnKind kind, int model, const Vec3& pos, float heading, int* handle);
void Despawn(SpawnKind kind, int handle);

} // namespace gta

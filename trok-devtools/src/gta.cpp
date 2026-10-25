// Trok DevTools (.asi) -- Victor_Trok
// Enderecos do gta_sa.exe 1.0 US (plugin-sdk e gta-reversed):
//   CModelInfo::ms_modelInfoPtrs: o endereco fica em 0x40CD67 (os limit adjusters mudam a tabela de lugar), padrao
//   0xA9B0C8; CBaseModelInfo: TXD +0xA, flags +0x12, colisao +0x14, distancia +0x18, RwObject +0x1C, GetModelType e a
//   5a funcao da vtable.
//   CStreaming: ms_files[8] 0x8E48D8 (0x30 cada, o nome no comeco), ms_aInfoForModel 0x8E4CC0 (0x14 cada: flags +6,
//   .img +7, posicao +8, tamanho +0xC, estado +0x10), RequestModel 0x4087E0, LoadAllRequestedModels 0x40EA10,
//   SetMissionDoesntRequireModel 0x409C90.
//   CTxdStore::ms_pTxdPool 0xC8800C (CPool de TxdDef: dicionario +0, pai +6, 0xC cada).
//   CWorld::ProcessLineOfSight 0x56BA00, CSprite::CalcScreenCoors 0x70CE30, TheCamera 0xB6F028 (CMatrix* +0x14),
//   CPad::NewMouseControllerState 0xB73418 (o botao do meio em +2), CWorld::Players[0].m_pPed 0xB7CD98, CEntity: modelo +0x22, interior +0x2F, LOD +0x30, tipo +0x36 (& 7).
//   CAnimManager::ms_aAnimBlocks 0xB5D4A0 (0x20 cada: nome[16], carregado +0x10), ms_numAnimBlocks 0xB4EA30.
//   CRunningScript::ProcessOneCommand 0x469EB0: o script de mentira roda um comando por vez (como o CLEO e o
//   MoonLoader), com as variaveis locais em +0x3C e o resultado da condicao em +0xC5.

#include "gta.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

void DevToolsLog(const char* fmt, ...);

namespace gta {
namespace {

constexpr DWORD VERSION_MARK = 0x82457C;
constexpr DWORD MODEL_PTRS_REF = 0x40CD67, MODEL_PTRS_DEFAULT = 0xA9B0C8;
constexpr DWORD MI_TXD = 0xA, MI_FLAGS = 0x12, MI_COL = 0x14, MI_DRAW_DIST = 0x18, MI_RWOBJECT = 0x1C;
constexpr int VT_GET_MODEL_TYPE = 4;

constexpr DWORD STREAM_FILES = 0x8E48D8, STREAM_FILE_SIZE = 0x30;
constexpr DWORD STREAM_INFO = 0x8E4CC0, STREAM_INFO_SIZE = 0x14;
constexpr int STREAM_RESOURCES = 26316, IMG_SLOTS = 8;
constexpr DWORD FN_REQUEST_MODEL = 0x4087E0, FN_LOAD_ALL = 0x40EA10, FN_MISSION_DOESNT_REQUIRE = 0x409C90;
constexpr int STREAMING_MISSION_REQUIRED = 0x4;

constexpr DWORD TXD_POOL = 0xC8800C;
constexpr DWORD POOL_OBJECTS = 0x0, POOL_FLAGS = 0x4, POOL_SIZE = 0x8;
constexpr DWORD TXD_DEF_SIZE = 0xC, TXD_DICT = 0x0, TXD_PARENT = 0x6;

constexpr DWORD FN_LINE_OF_SIGHT = 0x56BA00, FN_CALC_SCREEN = 0x70CE30;
constexpr DWORD CAMERA = 0xB6F028, PLAYER_PED = 0xB7CD98;
constexpr DWORD PAD_NEW_MOUSE = 0xB73418; // CPad::NewMouseControllerState
constexpr DWORD ENT_PLACEMENT = 0x04, ENT_MATRIX = 0x14, ENT_MODEL = 0x22, ENT_AREA = 0x2F, ENT_LOD = 0x30, ENT_TYPE = 0x36;

constexpr DWORD ANIM_BLOCKS = 0xB5D4A0, ANIM_BLOCK_COUNT = 0xB4EA30, ANIM_BLOCK_SIZE = 0x20;
constexpr int ANIM_BLOCKS_MAX = 180;

constexpr DWORD FN_PROCESS_ONE_COMMAND = 0x469EB0;
constexpr DWORD SCRIPT_SIZE = 0xE0, SC_NAME = 0x08, SC_BASE_IP = 0x10, SC_CURRENT_IP = 0x14, SC_LOCALS = 0x3C,
                SC_COND = 0xC5;

typedef int(__thiscall* TypeFn)(void*);
typedef void(__cdecl* RequestFn)(int, int);
typedef void(__cdecl* LoadAllFn)(bool);
typedef void(__cdecl* ModelFn)(int);
typedef bool(__cdecl* LineFn)(const Vec3&, const Vec3&, void*, DWORD*, bool, bool, bool, bool, bool, bool, bool, bool);
typedef bool(__cdecl* ScreenFn)(const Vec3&, Vec3*, float*, float*, bool, bool);
typedef char(__thiscall* CommandFn)(void*);

DWORD ModelTable() {
    DWORD base = At<DWORD>(MODEL_PTRS_REF, 0);
    return base >= 0x400000 && Readable(reinterpret_cast<void*>(base), MODEL_COUNT * 4) ? base : MODEL_PTRS_DEFAULT;
}

// ---------------------------------------------------------------- script de mentira

class Script {
public:
    void Op(WORD op) {
        Put(&op, 2);
    }
    void Int(int v) {
        BYTE t = 0x01;
        Put(&t, 1);
        Put(&v, 4);
    }
    void Float(float v) {
        BYTE t = 0x06;
        Put(&t, 1);
        Put(&v, 4);
    }
    void Text(const std::string& v) { // texto de tamanho variavel (1 byte com o tamanho)
        BYTE t = 0x0E, n = static_cast<BYTE>(std::min<size_t>(v.size(), 120));
        Put(&t, 1);
        Put(&n, 1);
        Put(v.data(), n);
    }
    void Local(WORD index) { // variavel local (para a saida)
        BYTE t = 0x03;
        Put(&t, 1);
        Put(&index, 2);
    }
    // Roda os comandos escritos ate aqui, um por vez. Devolve o script (para ler as variaveis e a condicao).
    const BYTE* Run(int commands) {
        memset(state_, 0, sizeof(state_));
        strcpy(reinterpret_cast<char*>(state_ + SC_NAME), "trokstd");
        *reinterpret_cast<BYTE**>(state_ + SC_BASE_IP) = code_;
        *reinterpret_cast<BYTE**>(state_ + SC_CURRENT_IP) = code_;
        for (int i = 0; i < commands; ++i) {
            reinterpret_cast<CommandFn>(FN_PROCESS_ONE_COMMAND)(state_);
        }
        size_ = 0;
        return state_;
    }

private:
    void Put(const void* data, int n) {
        if (size_ + n <= static_cast<int>(sizeof(code_))) {
            memcpy(code_ + size_, data, n);
            size_ += n;
        }
    }
    BYTE code_[128] = {};
    int size_ = 0;
    alignas(16) BYTE state_[SCRIPT_SIZE + 0x20] = {};
};

Script g_script;

int LocalVar(const BYTE* script, int index) {
    return *reinterpret_cast<const int*>(script + SC_LOCALS + index * 4);
}

} // namespace

bool Readable(const void* p, size_t size) {
    DWORD a = reinterpret_cast<DWORD>(p);
    if (a < 0x10000) {
        return false;
    }
    while (size > 0) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(reinterpret_cast<void*>(a), &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
            (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
            return false;
        }
        DWORD end = reinterpret_cast<DWORD>(mbi.BaseAddress) + mbi.RegionSize;
        DWORD step = end - a;
        if (step >= size) {
            return true;
        }
        size -= step;
        a = end;
    }
    return true;
}

bool VersionOk() {
    return Readable(reinterpret_cast<void*>(VERSION_MARK), 4) && At<DWORD>(VERSION_MARK, 0) == 0x94BF;
}

DWORD ModelInfo(int id) {
    if (id < 0 || id >= MODEL_COUNT) {
        return 0;
    }
    DWORD info = At<DWORD>(ModelTable(), id * 4);
    return Ptr(info) ? info : 0;
}

int TypeOf(DWORD info) {
    if (!info) {
        return MT_NONE;
    }
    DWORD vtable = At<DWORD>(info, 0);
    if (!Ptr(vtable) || !Readable(reinterpret_cast<void*>(vtable), (VT_GET_MODEL_TYPE + 1) * 4)) {
        return MT_NONE;
    }
    DWORD fn = At<DWORD>(vtable, VT_GET_MODEL_TYPE * 4);
    if (!Ptr(fn)) {
        return MT_NONE;
    }
    return reinterpret_cast<TypeFn>(fn)(reinterpret_cast<void*>(info)) & 0xFF;
}

int TxdIndex(DWORD info) {
    return info ? At<short>(info, MI_TXD) : -1;
}

float DrawDistance(DWORD info) {
    return info ? At<float>(info, MI_DRAW_DIST) : 0.0f;
}

WORD Flags(DWORD info) {
    return info ? At<WORD>(info, MI_FLAGS) : 0;
}

DWORD ColModel(DWORD info) {
    DWORD col = info ? At<DWORD>(info, MI_COL) : 0;
    return Ptr(col) ? col : 0;
}

DWORD RwObjectOf(DWORD info) {
    DWORD obj = info ? At<DWORD>(info, MI_RWOBJECT) : 0;
    return Ptr(obj) ? obj : 0;
}

bool Streaming(int resource, StreamInfo* out) {
    if (resource < 0 || resource >= STREAM_RESOURCES) {
        return false;
    }
    DWORD si = STREAM_INFO + resource * STREAM_INFO_SIZE;
    out->flags = At<BYTE>(si, 0x6);
    out->img = At<BYTE>(si, 0x7);
    out->posn = At<DWORD>(si, 0x8);
    out->size = At<DWORD>(si, 0xC);
    out->loadState = At<BYTE>(si, 0x10);
    return true;
}

int ImgSlots() {
    return IMG_SLOTS;
}

std::string ImgPath(int index) {
    if (index < 0 || index >= IMG_SLOTS) {
        return std::string();
    }
    const char* name = reinterpret_cast<const char*>(STREAM_FILES + index * STREAM_FILE_SIZE);
    return std::string(name, strnlen(name, 40));
}

// Varias travas no mesmo recurso (a previa, o card, uma miniatura): a marca de missao so sai quando a ultima solta.
std::unordered_map<int, int> g_holds;
std::unordered_set<int> g_added;

bool Acquire(int resource, Hold* hold) {
    StreamInfo si;
    if (!Streaming(resource, &si)) {
        return false;
    }
    if (hold->id != resource) {
        Release(hold);
        hold->id = resource;
        if (g_holds[resource]++ == 0 && !(si.flags & STREAMING_MISSION_REQUIRED)) {
            g_added.insert(resource);
        }
        hold->added = g_added.count(resource) > 0;
        if (si.size == 0 && si.loadState != 1) {
            return false; // sem arquivo no .img (os SPECIAL das cutscenes)
        }
        reinterpret_cast<RequestFn>(FN_REQUEST_MODEL)(resource, STREAMING_MISSION_REQUIRED);
    }
    if (Streaming(resource, &si) && si.loadState != 1) {
        reinterpret_cast<LoadAllFn>(FN_LOAD_ALL)(false);
    }
    return Streaming(resource, &si) && si.loadState == 1;
}

bool Load(int id, Hold* hold) {
    if (!ModelInfo(id)) {
        return false;
    }
    return Acquire(id, hold) && RwObjectOf(ModelInfo(id));
}

void Release(Hold* hold) {
    if (hold->id >= 0) {
        auto it = g_holds.find(hold->id);
        if (it != g_holds.end() && --it->second <= 0) {
            g_holds.erase(it);
            if (g_added.erase(hold->id)) {
                reinterpret_cast<ModelFn>(FN_MISSION_DOESNT_REQUIRE)(hold->id);
            }
        }
    }
    hold->id = -1;
    hold->added = false;
}

bool LoadResource(int resource, Hold* hold) {
    return Acquire(resource, hold);
}

int AnimBlockIndex(const std::string& name) {
    int count = At<int>(ANIM_BLOCK_COUNT, 0);
    if (count <= 0 || count > ANIM_BLOCKS_MAX || !Readable(reinterpret_cast<void*>(ANIM_BLOCKS), count * ANIM_BLOCK_SIZE)) {
        return -1;
    }
    for (int i = 0; i < count; ++i) {
        const char* n = reinterpret_cast<const char*>(ANIM_BLOCKS + i * ANIM_BLOCK_SIZE);
        if (strnlen(n, 16) == name.size() && _strnicmp(n, name.c_str(), name.size()) == 0) {
            return i;
        }
    }
    return -1;
}

bool AnimBlockLoaded(int index) {
    return index >= 0 && index < ANIM_BLOCKS_MAX && At<BYTE>(ANIM_BLOCKS + index * ANIM_BLOCK_SIZE, 0x10) != 0;
}

DWORD TxdDictionary(int txdIndex) {
    DWORD pool = At<DWORD>(TXD_POOL, 0);
    if (!Ptr(pool) || txdIndex < 0 || txdIndex >= At<int>(pool, POOL_SIZE)) {
        return 0;
    }
    BYTE flags = At<BYTE>(At<DWORD>(pool, POOL_FLAGS), txdIndex);
    if (flags & 0x80) {
        return 0; // slot vazio
    }
    DWORD dict = At<DWORD>(At<DWORD>(pool, POOL_OBJECTS) + txdIndex * TXD_DEF_SIZE, TXD_DICT);
    return Ptr(dict) ? dict : 0;
}

int TxdParent(int txdIndex) {
    DWORD pool = At<DWORD>(TXD_POOL, 0);
    if (!Ptr(pool) || txdIndex < 0 || txdIndex >= At<int>(pool, POOL_SIZE)) {
        return -1;
    }
    return At<short>(At<DWORD>(pool, POOL_OBJECTS) + txdIndex * TXD_DEF_SIZE, TXD_PARENT);
}

bool MiddleMouseDown() {
    return At<BYTE>(PAD_NEW_MOUSE, 2) != 0; // CMouseControllerState: esquerdo, direito, meio
}

DWORD PlayerPed() {
    DWORD ped = At<DWORD>(PLAYER_PED, 0);
    return Ptr(ped) ? ped : 0;
}

bool EntityMatrix(DWORD entity, Vec3* right, Vec3* forward, Vec3* up, Vec3* pos) {
    if (!entity) {
        return false;
    }
    DWORD m = At<DWORD>(entity, ENT_MATRIX);
    if (Ptr(m)) {
        *right = At<Vec3>(m, 0x00);
        *forward = At<Vec3>(m, 0x10);
        *up = At<Vec3>(m, 0x20);
        *pos = At<Vec3>(m, 0x30);
        return true;
    }
    // Sem matriz (predios simples): posicao e angulo do CSimpleTransform.
    *pos = At<Vec3>(entity, ENT_PLACEMENT);
    float heading = At<float>(entity, ENT_PLACEMENT + 0xC);
    float c = std::cos(heading), s = std::sin(heading);
    *right = {c, s, 0};
    *forward = {-s, c, 0};
    *up = {0, 0, 1};
    return true;
}

int EntityModel(DWORD entity) {
    return entity ? At<short>(entity, ENT_MODEL) : -1;
}

int EntityType(DWORD entity) {
    return entity ? At<BYTE>(entity, ENT_TYPE) & 7 : 0;
}

int EntityArea(DWORD entity) {
    return entity ? At<BYTE>(entity, ENT_AREA) : 0;
}

DWORD EntityLod(DWORD entity) {
    DWORD lod = entity ? At<DWORD>(entity, ENT_LOD) : 0;
    return Ptr(lod) ? lod : 0;
}

bool CameraNow(Camera* out) {
    DWORD m = At<DWORD>(CAMERA, ENT_MATRIX);
    if (!Ptr(m)) {
        return false;
    }
    out->right = At<Vec3>(m, 0x00);
    out->forward = At<Vec3>(m, 0x10);
    out->up = At<Vec3>(m, 0x20);
    out->pos = At<Vec3>(m, 0x30);
    return true;
}

DWORD LineOfSight(const Vec3& from, const Vec3& to, Vec3* hit) {
    alignas(16) BYTE colPoint[0x40] = {};
    DWORD entity = 0;
    bool found = reinterpret_cast<LineFn>(FN_LINE_OF_SIGHT)(from, to, colPoint, &entity, true, true, true, true, true,
                                                            false, false, false);
    if (!found || !Ptr(entity)) {
        return 0;
    }
    if (hit) {
        memcpy(hit, colPoint, sizeof(Vec3));
    }
    return entity;
}

bool Project(const Vec3& world, float* x, float* y) {
    Vec3 screen;
    float w, h;
    if (!reinterpret_cast<ScreenFn>(FN_CALC_SCREEN)(world, &screen, &w, &h, false, false)) {
        return false;
    }
    *x = screen.x;
    *y = screen.y;
    return true;
}

bool Spawn(SpawnKind kind, int model, const Vec3& pos, float heading, int* handle) {
    Script& s = g_script;
    switch (kind) {
    case SPAWN_OBJECT: // 0107 CREATE_OBJECT
        s.Op(0x0107);
        s.Int(model);
        break;
    case SPAWN_VEHICLE: // 00A5 CREATE_CAR
        s.Op(0x00A5);
        s.Int(model);
        break;
    default: // 009A CREATE_CHAR (tipo 4, civil)
        s.Op(0x009A);
        s.Int(4);
        s.Int(model);
        break;
    }
    s.Float(pos.x);
    s.Float(pos.y);
    s.Float(pos.z);
    s.Local(0);
    int h = LocalVar(s.Run(1), 0);
    if (h <= 0) {
        DevToolsLog("criar %d: o jogo nao devolveu o handle", model);
        return false;
    }
    switch (kind) {
    case SPAWN_OBJECT:
        s.Op(0x0177); // SET_OBJECT_HEADING
        s.Int(h);
        s.Float(heading);
        s.Op(0x0550); // FREEZE_OBJECT_POSITION: parado onde nasceu (objetos com fisica cairiam)
        s.Int(h);
        s.Int(1);
        s.Run(2);
        break;
    case SPAWN_VEHICLE:
        s.Op(0x0175); // SET_CAR_HEADING
        s.Int(h);
        s.Float(heading);
        s.Op(0x020A); // LOCK_CAR_DOORS: trancado (no SA-MP o carro nao existe no servidor)
        s.Int(h);
        s.Int(2);
        s.Run(2);
        break;
    default:
        s.Op(0x0173); // SET_CHAR_HEADING
        s.Int(h);
        s.Float(heading);
        s.Run(1);
        break;
    }
    *handle = h;
    return true;
}

void Despawn(SpawnKind kind, int handle) {
    Script& s = g_script;
    // So apaga o que ainda existe (o jogo pode ter tirado: interior, reconexao).
    s.Op(kind == SPAWN_OBJECT ? 0x03CA : kind == SPAWN_VEHICLE ? 0x056E : 0x056D); // DOES_*_EXIST
    s.Int(handle);
    if (!s.Run(1)[SC_COND]) {
        return;
    }
    s.Op(kind == SPAWN_OBJECT ? 0x0108 : kind == SPAWN_VEHICLE ? 0x00A6 : 0x009B); // DELETE_*
    s.Int(handle);
    s.Run(1);
}

int PlayerHandle() {
    Script& s = g_script;
    s.Op(0x01F5); // GET_PLAYER_CHAR
    s.Int(0);
    s.Local(0);
    return LocalVar(s.Run(1), 0);
}

bool PlayAnim(const std::string& block, const std::string& anim, bool loop, bool move, std::string* why) {
    Script& s = g_script;
    int ped = PlayerHandle();
    if (ped <= 0) {
        *why = "personagem n\xC3\xA3o encontrado";
        return false;
    }
    s.Op(0x00DF); // IS_CHAR_IN_ANY_CAR
    s.Int(ped);
    if (s.Run(1)[SC_COND]) {
        *why = "saia do ve\xC3\xAD" "culo primeiro";
        return false;
    }
    s.Op(0x0605); // TASK_PLAY_ANIM: personagem, animacao, bloco, transicao, repetir, lockX, lockY, lockF, tempo
    s.Int(ped);
    s.Text(anim);
    s.Text(block);
    s.Float(4.0f);
    s.Int(loop ? 1 : 0);
    s.Int(move ? 1 : 0);
    s.Int(move ? 1 : 0);
    s.Int(0);
    s.Int(-1);
    s.Run(1);
    return true;
}

void StopAnim() {
    int ped = PlayerHandle();
    if (ped > 0) {
        Script& s = g_script;
        s.Op(0x0792); // CLEAR_CHAR_TASKS_IMMEDIATELY
        s.Int(ped);
        s.Run(1);
    }
}

} // namespace gta

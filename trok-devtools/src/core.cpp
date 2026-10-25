// Trok DevTools (.asi) -- Victor_Trok
// Miniaturas: a cada quadro do jogo, os cards que pediram (os da tela, os mais recentes primeiro) carregam ate o
// limite do quadro (e ate ~6 ms); a malha e copiada e a miniatura e desenhada no Present seguinte. So depois disso o
// modelo e solto (a textura do Direct3D e do RenderWare e precisa ate a miniatura sair).
// Card aberto: o modelo fica preso enquanto o card estiver aberto (o visualizador e a lista de texturas usam as texturas
// dele) e e solto quando o card fecha. Soltar = tirar a marca de missao que o mod pos; se o jogo ja segurava o modelo,
// nada muda.
// Criar no mundo: 1:1, de frente para o personagem, a uma distancia que cabe o modelo inteiro. Objetos nascem com o meio
// na altura dos olhos (os altos, com a base no chao) e ficam parados; veiculos e personagens nascem no chao, e os
// veiculos trancados (no SA-MP o servidor nao sabe deles).
// Quem usa a textura: os candidatos sao os modelos do catalogo cuja TXD e a que tem a textura ou descende dela (txdp);
// cada um e carregado (se ja nao estava) e os materiais dizem se a textura e usada.

#include "core.h"

#include "catalog.h"
#include "render.h"
#include "sfx.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

void DevToolsLog(const char* fmt, ...);

namespace core {
namespace {

constexpr double THUMB_BUDGET_MS = 6.0;
constexpr int THUMB_WANT_LIFE = 3; // quadros: um card que saiu da tela deixa de pedir
constexpr double USERS_BUDGET_MS = 4.0;
constexpr int USERS_MAX = 600; // candidatos vistos por busca (uma TXD generica pode ter milhares)
constexpr float EYE_HEIGHT = 1.6f; // acima do pe

struct Ready {
    mesh::Mesh mesh;
    gta::Hold hold;
};

bool g_browsing = false;
int g_perFrame = 3;
std::unordered_map<int, int> g_wanted; // id -> quadro do ultimo pedido
std::vector<int> g_wantOrder;          // na ordem em que os cards pediram neste quadro
std::unordered_set<int> g_failed;
std::vector<Ready*> g_ready;
std::vector<gta::Hold> g_toRelease;
int g_frame = 0;

int g_openId = -1, g_wantOpen = -1;
gta::Hold g_openHold;
mesh::Mesh g_openMesh;
bool g_openReady = false, g_openFailed = false;
std::string g_openWhy;
bool g_closeRequested = false;

struct Spawned {
    gta::SpawnKind kind;
    int handle;
    int model;
};
std::vector<Spawned> g_spawned;
std::deque<int> g_spawnQueue;
bool g_despawnAll = false;
std::string g_spawnMessage;
bool g_spawnOk = false, g_spawnNews = false;

TextureUsers g_users;
std::vector<int> g_usersQueue;
size_t g_usersNext = 0;
bool g_usersSetup = false;

bool g_inspect = false;
Look g_look;
std::string g_gameDir;
bool g_catalogStarted = false;

double NowMs() {
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart) * 1000.0 / static_cast<double>(f.QuadPart);
}

bool IsVehicle(int id) {
    const catalog::Entry* e = catalog::Find(id);
    return e ? e->kind == catalog::K_VEHICLE : gta::TypeOf(gta::ModelInfo(id)) == gta::MT_VEHICLE;
}

void LoadThumbs() {
    if (!g_browsing || g_wantOrder.empty()) {
        g_wantOrder.clear();
        return;
    }
    double start = NowMs();
    int done = 0;
    for (int id : g_wantOrder) {
        if (done >= g_perFrame || NowMs() - start > THUMB_BUDGET_MS) {
            break;
        }
        if (render::HasThumb(id) || g_failed.count(id)) {
            continue;
        }
        const catalog::Entry* e = catalog::Find(id);
        if (e && e->noFile) {
            g_failed.insert(id);
            continue;
        }
        Ready* r = new Ready();
        ++done;
        if (!gta::Load(id, &r->hold) || !mesh::Extract(id, IsVehicle(id), &r->mesh)) {
            g_failed.insert(id);
            gta::Release(&r->hold);
            delete r;
            continue;
        }
        g_ready.push_back(r);
    }
    g_wantOrder.clear();
}

void LoadOpen() {
    if (g_closeRequested) {
        g_closeRequested = false;
        gta::Release(&g_openHold);
        g_openMesh = mesh::Mesh();
        g_openId = -1;
        g_openReady = g_openFailed = false;
    }
    if (g_wantOpen < 0 || (g_wantOpen == g_openId && (g_openReady || g_openFailed))) {
        return;
    }
    if (g_wantOpen != g_openId) {
        gta::Release(&g_openHold);
        g_openMesh = mesh::Mesh();
        g_openId = g_wantOpen;
        g_openReady = g_openFailed = false;
        g_openWhy.clear();
    }
    const catalog::Entry* e = catalog::Find(g_openId);
    if (e && e->noFile) {
        g_openFailed = true;
        g_openWhy = "sem arquivo no streaming (nada para carregar)";
        return;
    }
    if (!gta::Load(g_openId, &g_openHold)) {
        g_openFailed = true;
        g_openWhy = "o jogo n\xC3\xA3o carregou o modelo";
        return;
    }
    if (!mesh::Extract(g_openId, IsVehicle(g_openId), &g_openMesh)) {
        g_openFailed = true;
        g_openWhy = g_openMesh.error;
        return;
    }
    g_openReady = true;
}

gta::Vec3 Normalized2D(gta::Vec3 v) {
    float l = std::sqrt(v.x * v.x + v.y * v.y);
    return l > 1e-4f ? gta::Vec3{v.x / l, v.y / l, 0} : gta::Vec3{0, 1, 0};
}

void DoSpawn(int id) {
    const catalog::Entry* e = catalog::Find(id);
    int kind = e ? e->kind : catalog::K_OBJECT;
    gta::SpawnKind sk = kind == catalog::K_VEHICLE ? gta::SPAWN_VEHICLE
                        : kind == catalog::K_PED   ? gta::SPAWN_PED
                                                   : gta::SPAWN_OBJECT;
    g_spawnNews = true;
    g_spawnOk = false;
    if (id == 0) {
        g_spawnMessage = "O 0 \xC3\xA9 o CJ: a roupa dele \xC3\xA9 montada pelo jogo.";
        return;
    }
    static const int TRAINS[] = {449, 537, 538, 569, 570, 590};
    if (sk == gta::SPAWN_VEHICLE && std::find(std::begin(TRAINS), std::end(TRAINS), id) != std::end(TRAINS)) {
        g_spawnMessage = "Trens e bonde n\xC3\xA3o nascem assim.";
        return;
    }
    DWORD player = gta::PlayerPed();
    gta::Vec3 r, f, u, p;
    if (!player || !gta::EntityMatrix(player, &r, &f, &u, &p)) {
        g_spawnMessage = "Personagem n\xC3\xA3o encontrado.";
        return;
    }
    gta::Hold hold;
    bool loaded = id == g_openId && g_openReady ? true : gta::Load(id, &hold);
    if (!loaded) {
        g_spawnMessage = "O modelo n\xC3\xA3o carregou.";
        gta::Release(&hold);
        return;
    }
    mesh::Mesh temp;
    const mesh::Mesh* m = id == g_openId && g_openReady ? &g_openMesh : nullptr;
    if (!m && mesh::Extract(id, sk == gta::SPAWN_VEHICLE, &temp)) {
        m = &temp;
    }
    gta::Vec3 mn = m ? m->min : gta::Vec3{-0.5f, -0.5f, -0.5f}, mx = m ? m->max : gta::Vec3{0.5f, 0.5f, 0.5f};
    float radius = 0;
    for (float x : {mn.x, mx.x}) {
        for (float y : {mn.y, mx.y}) {
            radius = std::max(radius, std::sqrt(x * x + y * y));
        }
    }
    gta::Vec3 fwd = Normalized2D(f);
    float dist = std::max(2.5f, radius + 1.5f);
    float feet = p.z - 1.0f; // a posicao do personagem fica ~1 m acima do pe
    gta::Vec3 at = {p.x + fwd.x * dist, p.y + fwd.y * dist, 0};
    if (sk == gta::SPAWN_PED) {
        at.z = feet + 1.0f;
    } else if (sk == gta::SPAWN_VEHICLE) {
        at.z = feet - mn.z + 0.1f;
    } else {
        // O meio do modelo na altura dos olhos, sem a parte de baixo entrar no chao.
        at.z = std::max(feet + EYE_HEIGHT - (mn.z + mx.z) * 0.5f, feet - mn.z);
    }
    float playerHeading = std::atan2(-fwd.x, fwd.y) * 57.29578f;
    float heading = std::fmod(playerHeading + 180.0f + 360.0f, 360.0f); // de frente para voce
    int handle = 0;
    if (gta::Spawn(sk, id, at, heading, &handle)) {
        g_spawned.push_back({sk, handle, id});
        g_spawnOk = true;
        char text[160];
        snprintf(text, sizeof(text), "%d criado %.1f m \xC3\xA0 sua frente.", id, dist);
        for (char* c = text; *c; ++c) {
            if (*c == '.' && c[1] >= '0' && c[1] <= '9') {
                *c = ','; // decimal como no resto do menu
            }
        }
        g_spawnMessage = text;
        DevToolsLog("criado %d (%s) em %.1f %.1f %.1f, handle %d", id, sk == gta::SPAWN_OBJECT ? "objeto"
                  : sk == gta::SPAWN_VEHICLE ? "veiculo" : "personagem", at.x, at.y, at.z, handle);
    } else {
        g_spawnMessage = "O jogo n\xC3\xA3o criou o modelo.";
    }
    gta::Release(&hold); // o que nasceu segura o modelo sozinho
}

void UpdateLook() {
    g_look = Look();
    if (!g_inspect) {
        return;
    }
    gta::Camera cam;
    if (!gta::CameraNow(&cam)) {
        return;
    }
    g_look.camPos = cam.pos;
    g_look.camForward = cam.forward;
    // Comeca depois do personagem (a camera fica atras dele).
    float skip = 0.5f;
    DWORD player = gta::PlayerPed();
    gta::Vec3 r, f, u, p;
    if (player && gta::EntityMatrix(player, &r, &f, &u, &p)) {
        float along = (p.x - cam.pos.x) * cam.forward.x + (p.y - cam.pos.y) * cam.forward.y +
                      (p.z - cam.pos.z) * cam.forward.z;
        skip = std::max(skip, along + 0.8f);
    }
    gta::Vec3 from = {cam.pos.x + cam.forward.x * skip, cam.pos.y + cam.forward.y * skip,
                      cam.pos.z + cam.forward.z * skip};
    gta::Vec3 to = {cam.pos.x + cam.forward.x * 300, cam.pos.y + cam.forward.y * 300, cam.pos.z + cam.forward.z * 300};
    gta::Vec3 hit;
    DWORD entity = gta::LineOfSight(from, to, &hit);
    if (!entity || entity == player) {
        return;
    }
    g_look.valid = true;
    g_look.model = gta::EntityModel(entity);
    g_look.type = gta::EntityType(entity);
    g_look.area = gta::EntityArea(entity);
    g_look.hit = hit;
    float dx = hit.x - cam.pos.x, dy = hit.y - cam.pos.y, dz = hit.z - cam.pos.z;
    g_look.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    DWORD lod = gta::EntityLod(entity);
    g_look.lodModel = lod ? gta::EntityModel(lod) : -1;
    gta::Vec3 er, ef, eu;
    if (gta::EntityMatrix(entity, &er, &ef, &eu, &g_look.pos)) {
        DWORD col = gta::ColModel(gta::ModelInfo(g_look.model));
        if (col) {
            gta::Vec3 a = gta::At<gta::Vec3>(col, 0x00), b = gta::At<gta::Vec3>(col, 0x0C);
            for (int i = 0; i < 8; ++i) {
                gta::Vec3 l = {i & 1 ? b.x : a.x, i & 2 ? b.y : a.y, i & 4 ? b.z : a.z};
                g_look.box[i] = {g_look.pos.x + er.x * l.x + ef.x * l.y + eu.x * l.z,
                                 g_look.pos.y + er.y * l.x + ef.y * l.y + eu.y * l.z,
                                 g_look.pos.z + er.z * l.x + ef.z * l.y + eu.z * l.z};
            }
            g_look.hasBox = true;
        }
    }
}

bool UnderTxd(int txd, int base) {
    for (int depth = 0; txd >= 0 && depth < 8; ++depth) {
        if (txd == base) {
            return true;
        }
        txd = gta::TxdParent(txd);
    }
    return false;
}

void SetupTextureUsers() {
    TextureUsers& u = g_users;
    g_usersSetup = true;
    int own = gta::TxdIndex(gta::ModelInfo(u.model));
    for (int t = own, depth = 0; t >= 0 && depth < 8; t = gta::TxdParent(t), ++depth) {
        if (mesh::TxdHas(t, u.texture)) {
            u.txd = t;
            break;
        }
    }
    int base = u.txd >= 0 ? u.txd : own;
    for (const catalog::Entry& e : catalog::All()) {
        if (e.txdIndex >= 0 && e.txdIndex == base && !e.txd.empty() && u.txdName.empty()) {
            u.txdName = e.txd;
        }
        if (!e.noFile && e.id != u.model && UnderTxd(e.txdIndex, base)) {
            g_usersQueue.push_back(e.id);
        }
    }
    if (static_cast<int>(g_usersQueue.size()) > USERS_MAX - 1) {
        g_usersQueue.resize(USERS_MAX - 1);
        u.capped = true;
    }
    g_usersQueue.insert(g_usersQueue.begin(), u.model); // o do card primeiro (ja esta carregado)
    u.total = static_cast<int>(g_usersQueue.size());
    DevToolsLog("textura %s: TXD %d (%s), %d candidatos%s", u.texture.c_str(), u.txd, u.txdName.c_str(), u.total,
                u.capped ? " (limite)" : "");
}

void RunTextureUsers() {
    TextureUsers& u = g_users;
    if (u.model < 0 || u.done) {
        return;
    }
    if (!g_usersSetup) {
        SetupTextureUsers();
    }
    double start = NowMs();
    int loads = 0;
    while (g_usersNext < g_usersQueue.size() && loads < g_perFrame * 2 && NowMs() - start < USERS_BUDGET_MS) {
        int id = g_usersQueue[g_usersNext++];
        gta::Hold hold;
        bool loaded = gta::RwObjectOf(gta::ModelInfo(id)) != 0;
        if (!loaded) {
            ++loads;
            loaded = gta::Load(id, &hold);
        }
        if (loaded && mesh::UsesTexture(id, u.texture)) {
            u.users.push_back(id);
        }
        g_toRelease.push_back(hold); // solta no proximo quadro
        ++u.checked;
    }
    if (g_usersNext >= g_usersQueue.size()) {
        std::sort(u.users.begin(), u.users.end());
        u.done = true;
        DevToolsLog("textura %s: %d modelos usam (%d vistos)", u.texture.c_str(), static_cast<int>(u.users.size()),
                    u.checked);
    }
}

} // namespace

void FindTextureUsers(int model, const std::string& texture) {
    if (texture == g_users.texture &&
        (model == g_users.model || std::find(g_users.users.begin(), g_users.users.end(), model) != g_users.users.end())) {
        return;
    }
    g_users = TextureUsers();
    g_users.model = model;
    g_users.texture = texture;
    g_usersQueue.clear();
    g_usersNext = 0;
    g_usersSetup = false;
}

const TextureUsers& TextureUsersNow() {
    return g_users;
}

void SetGameDir(const char* dir) {
    g_gameDir = dir ? dir : "";
}

void GameTick() {
    ++g_frame;
    for (gta::Hold& h : g_toRelease) {
        gta::Release(&h);
    }
    g_toRelease.clear();
    if (!g_catalogStarted) {
        g_catalogStarted = true; // o streaming ja montou a lista dos .img
        catalog::Start(g_gameDir.c_str());
        sfx::Start(g_gameDir.c_str());
    }
    catalog::Update();
    sfx::Update();
    UpdateLook(); // so precisa dos ids; os nomes aparecem quando o catalogo fica pronto
    if (!catalog::Ready()) {
        return;
    }
    LoadOpen();
    LoadThumbs();
    // A busca comeca com o modelo do card carregado (a TXD dele precisa estar na memoria) e segue enquanto o card estiver
    // aberto, mesmo trocando de modelo pela lista.
    if (g_wantOpen >= 0 && (g_usersSetup || (g_openReady && g_openId == g_users.model))) {
        RunTextureUsers();
    }
    if (g_despawnAll) {
        g_despawnAll = false;
        for (const Spawned& s : g_spawned) {
            gta::Despawn(s.kind, s.handle);
        }
        DevToolsLog("%d criados apagados", static_cast<int>(g_spawned.size()));
        g_spawned.clear();
    }
    while (!g_spawnQueue.empty()) {
        int id = g_spawnQueue.front();
        g_spawnQueue.pop_front();
        DoSpawn(id);
    }
}

void BeforeUi(IDirect3DDevice9* device) {
    render::SetDevice(device);
    for (Ready* r : g_ready) {
        if (!render::DrawThumb(r->mesh)) {
            g_failed.insert(r->mesh.id);
        }
        g_toRelease.push_back(r->hold);
        delete r;
    }
    g_ready.clear();
}

void SetBrowsing(bool on) {
    g_browsing = on;
}

void WantThumb(int id) {
    g_wanted[id] = g_frame;
    g_wantOrder.push_back(id);
}

bool ThumbFailed(int id) {
    return g_failed.count(id) != 0;
}

int ThumbsQueued() {
    int n = 0;
    for (const auto& w : g_wanted) {
        n += g_frame - w.second <= THUMB_WANT_LIFE && !render::HasThumb(w.first) && !g_failed.count(w.first);
    }
    return n;
}

void SetThumbsPerFrame(int n) {
    g_perFrame = std::max(1, std::min(8, n));
}

void Open(int id) {
    g_wantOpen = id;
    g_closeRequested = false;
}

void Close() {
    g_wantOpen = -1;
    g_closeRequested = true;
    g_users = TextureUsers(); // a busca da textura para junto
    g_usersQueue.clear();
    g_usersNext = 0;
    g_usersSetup = false;
}

int OpenId() {
    return g_wantOpen;
}

const mesh::Mesh* OpenMesh() {
    return g_openReady && g_openId == g_wantOpen ? &g_openMesh : nullptr;
}

bool OpenFailed(std::string* why) {
    if (g_openFailed && g_openId == g_wantOpen) {
        *why = g_openWhy;
        return true;
    }
    return false;
}

void Spawn(int id) {
    g_spawnQueue.push_back(id);
}

int SpawnedCount() {
    return static_cast<int>(g_spawned.size());
}

void DespawnAll() {
    g_despawnAll = true;
}

bool TakeSpawnMessage(std::string* text, bool* ok) {
    if (!g_spawnNews) {
        return false;
    }
    g_spawnNews = false;
    *text = g_spawnMessage;
    *ok = g_spawnOk;
    return true;
}

void SetInspect(bool on) {
    g_inspect = on;
    if (!on) {
        g_look = Look();
    }
}

bool InspectOn() {
    return g_inspect;
}

const Look& Looking() {
    return g_look;
}

} // namespace core

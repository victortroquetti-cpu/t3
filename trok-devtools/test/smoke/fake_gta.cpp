// Smoke test do Trok DevTools fora do jogo (Wine 32 bits + Xvfb): um "GTA de mentira" que
//  - usa a memoria do gta_sa.exe 1.0 US que o fake_gta.exe (host.c) deixa em 0x405000..0xD65000, com a marca de versao, a
//    tabela de modelos, o streaming (ms_files e ms_aInfoForModel), o pool de TXDs, a camera, o personagem e funcoes de
//    mentira nos enderecos do jogo: RequestModel, LoadAllRequestedModels, SetMissionDoesntRequireModel, CGame::Process,
//    CWorld::ProcessLineOfSight, CSprite::CalcScreenCoors e CRunningScript::ProcessOneCommand;
//  - grava os arquivos que o catalogo le: DATA\DEFAULT.DAT, DATA\GTA.DAT, os .ide (e o SAMP\SAMP.IDE) e os .img VER2
//    (MODELS\GTA3.IMG, MODELS\GTA_INT.IMG, SAMP\SAMP.IMG) com as entradas nas mesmas posicoes do streaming;
//  - monta os modelos do RenderWare quando o streaming carrega (RpAtomic/RpClump, frames com nome, RpGeometry com
//    vertices, normais, UV e materiais, RwTexture com a textura do Direct3D no raster) e os "descarrega" quando o mod
//    solta (a memoria vira 0xDD: quem usar depois do tempo trava ou ve lixo);
//  - abre o menu (F10), espera as miniaturas, busca, abre cards, cria no mundo, apaga, mira (o inspetor), reinicia o
//    device e salva as telas.
//
// Uso: fake_gta.exe <asi> <pasta das capturas>   (o fake_gta.exe chama Run deste fake_core.dll)

#include <windows.h>
#include <d3d9.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

template <typename T> T& Mem(DWORD address) {
    return *reinterpret_cast<T*>(address);
}

struct Vec3 {
    float x, y, z;
};

constexpr int W = 1920, H = 1080;
constexpr D3DCOLOR CLEAR = D3DCOLOR_XRGB(40, 70, 90);

// gta_sa.exe 1.0 US
constexpr DWORD VERSION_MARK = 0x82457C, MODEL_PTRS_REF = 0x40CD67, MODEL_PTRS = 0xA9B0C8;
constexpr DWORD STREAM_FILES = 0x8E48D8, STREAM_INFO = 0x8E4CC0;
constexpr DWORD FN_REQUEST = 0x4087E0, FN_LOAD_ALL = 0x40EA10, FN_MISSION_DOESNT = 0x409C90;
constexpr DWORD TXD_POOL = 0xC8800C, NODE_NAME_OFFSET = 0xC87C5C, RASTER_EXT_OFFSET = 0xB4E9E0;
constexpr DWORD FN_LOS = 0x56BA00, FN_CALC_SCREEN = 0x70CE30, FN_PROCESS_ONE = 0x469EB0, FN_GAME_PROCESS = 0x53BEE0;
constexpr DWORD CAMERA = 0xB6F028, PLAYER_PED = 0xB7CD98;
constexpr DWORD GTA_DEVICE = 0xC97C28, GTA_D3D_WINDOW = 0xC9C05C, GTA_WINDOW = 0xC8CF88;
constexpr DWORD SCREEN_W = 0xC17044, SCREEN_H = 0xC17048;
constexpr int NAME_OFF = 0xA4, EXT_OFF = 0x34;

int g_failures = 0;
IDirect3DDevice9* g_dev = nullptr;
D3DPRESENT_PARAMETERS g_pp = {};
DWORD* g_shot = nullptr;
std::string g_out;

void Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "ok" : "FALHA", what);
    fflush(stdout);
    if (!ok) {
        ++g_failures;
    }
}

void CheckF(bool ok, const char* fmt, double a, double b = 0, double c = 0) {
    char text[300];
    snprintf(text, sizeof(text), fmt, a, b, c);
    Check(ok, text);
}

bool Redirect(DWORD at, void* to) {
    DWORD old;
    if (!VirtualProtect(reinterpret_cast<void*>(at & ~0xFFFu), 0x2000, PAGE_EXECUTE_READWRITE, &old)) {
        return false;
    }
    Mem<BYTE>(at) = 0xE9;
    Mem<DWORD>(at + 1) = reinterpret_cast<DWORD>(to) - (at + 5);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(at), 5);
    return true;
}

// ---------------------------------------------------------------- texturas do Direct3D

struct Tex {
    std::string name;
    IDirect3DTexture9* d3d = nullptr;
    bool alpha = false;
    BYTE* raster = nullptr;
};
std::map<std::string, Tex> g_textures;

DWORD Mix(DWORD a, DWORD b, float t) {
    auto ch = [&](int s) {
        float va = (a >> s) & 0xFF, vb = (b >> s) & 0xFF;
        return static_cast<DWORD>(va + (vb - va) * t) << s;
    };
    return ch(24) | ch(16) | ch(8) | ch(0);
}

void MakeTexture(const char* name, int kind) {
    const int S = 64;
    Tex t;
    t.name = name;
    if (kind == 9) { // DXT1 liso (para o formato aparecer na lista)
        if (SUCCEEDED(g_dev->CreateTexture(S, S, 1, 0, D3DFMT_DXT1, D3DPOOL_MANAGED, &t.d3d, nullptr))) {
            D3DLOCKED_RECT lr;
            if (SUCCEEDED(t.d3d->LockRect(0, &lr, nullptr, 0))) {
                for (int by = 0; by < S / 4; ++by) {
                    WORD* row = reinterpret_cast<WORD*>(static_cast<BYTE*>(lr.pBits) + by * lr.Pitch);
                    for (int bx = 0; bx < S / 4; ++bx) {
                        WORD c0 = ((bx + by) & 1) ? 0x7BEF : 0x39E7, c1 = 0x0000;
                        row[bx * 4] = c0;
                        row[bx * 4 + 1] = c1;
                        row[bx * 4 + 2] = 0;
                        row[bx * 4 + 3] = 0;
                    }
                }
                t.d3d->UnlockRect(0);
            }
        }
    } else if (SUCCEEDED(g_dev->CreateTexture(S, S, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t.d3d, nullptr))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(t.d3d->LockRect(0, &lr, nullptr, 0))) {
            for (int y = 0; y < S; ++y) {
                DWORD* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(lr.pBits) + y * lr.Pitch);
                for (int x = 0; x < S; ++x) {
                    DWORD c = 0xFFFFFFFF;
                    switch (kind) {
                    case 0: c = ((y / 8) & 1) ? 0xFF2E7D4F : 0xFF3B9663; break;                // lixeira: faixas verdes
                    case 1: c = ((x % 16 < 3) || (y % 16 < 3)) ? 0xFFB8B8B8 : 0x00000000; break; // grade com furos
                    case 2: c = ((y % 16 < 2) || ((x + (y / 16) * 8) % 16 < 2)) ? 0xFFD8D0C0 : 0xFFA4513A; break;
                    case 3: c = Mix(0xFFF2F2F2, 0xFFC8C8C8, y / 63.0f); break;                  // lataria
                    case 4: c = 0xFF202326; break;                                             // vidro escuro
                    case 5: c = ((x - 32) * (x - 32) + (y - 32) * (y - 32) < 400) ? 0xFF9A9A9A : 0xFF1A1A1A; break;
                    case 6: c = Mix(0xFFE0B090, 0xFFC08060, y / 63.0f); break;                  // pele
                    case 7: c = ((x / 8 + y / 8) & 1) ? 0xFFFF2D9B : 0xFF2D2D2D; break;        // samp
                    default: c = ((x / 8 + y / 8) & 1) ? 0xFF6E8CA8 : 0xFF3A4A5A; break;
                    }
                    row[x] = c;
                }
            }
            t.d3d->UnlockRect(0);
        }
        t.alpha = kind == 1;
    }
    // O raster do RenderWare: largura, altura, profundidade e a extensao do D3D9 com a textura e o alfa.
    t.raster = static_cast<BYTE*>(calloc(1, EXT_OFF + 0x40));
    *reinterpret_cast<int*>(t.raster + 0x0C) = S;
    *reinterpret_cast<int*>(t.raster + 0x10) = S;
    *reinterpret_cast<int*>(t.raster + 0x14) = 32;
    t.raster[0x20] = 4; // rwRASTERTYPETEXTURE
    *reinterpret_cast<IDirect3DTexture9**>(t.raster + EXT_OFF) = t.d3d;
    t.raster[EXT_OFF + 8] = t.alpha;
    g_textures[name] = t;
}

// ---------------------------------------------------------------- TXDs

struct Txd {
    std::string name;
    std::vector<std::string> textures;
    BYTE* dict = nullptr;
    std::map<std::string, BYTE*> rwTex;
    DWORD posn = 0, size = 0;
    int img = 0;
};
std::vector<Txd> g_txds;
BYTE* g_txdObjects = nullptr;
BYTE* g_txdFlags = nullptr;

int AddTxd(const char* name, std::vector<std::string> textures, int img) {
    Txd t;
    t.name = name;
    t.textures = textures;
    t.img = img;
    g_txds.push_back(t);
    return static_cast<int>(g_txds.size()) - 1;
}

void LinkAfter(BYTE* head, BYTE* link) { // RwLLLink: next, prev
    BYTE* last = *reinterpret_cast<BYTE**>(head + 4);
    *reinterpret_cast<BYTE**>(link) = head;
    *reinterpret_cast<BYTE**>(link + 4) = last;
    *reinterpret_cast<BYTE**>(last) = link;
    *reinterpret_cast<BYTE**>(head + 4) = link;
}

void InitList(BYTE* head) {
    *reinterpret_cast<BYTE**>(head) = head;
    *reinterpret_cast<BYTE**>(head + 4) = head;
}

void BuildTxds() {
    g_txdObjects = static_cast<BYTE*>(calloc(g_txds.size() + 4, 0xC));
    g_txdFlags = static_cast<BYTE*>(calloc(g_txds.size() + 4, 1));
    for (size_t i = 0; i < g_txds.size(); ++i) {
        Txd& t = g_txds[i];
        t.dict = static_cast<BYTE*>(calloc(1, 0x20));
        t.dict[0] = 6; // rwTEXDICTIONARY
        InitList(t.dict + 0x08);
        for (const std::string& name : t.textures) {
            BYTE* rw = static_cast<BYTE*>(calloc(1, 0x60));
            *reinterpret_cast<BYTE**>(rw) = g_textures[name].raster;
            *reinterpret_cast<BYTE**>(rw + 4) = t.dict;
            strncpy(reinterpret_cast<char*>(rw + 0x10), name.c_str(), 31);
            if (name == "fence") {
                strcpy(reinterpret_cast<char*>(rw + 0x30), "fencea");
            }
            LinkAfter(t.dict + 0x08, rw + 0x08);
            t.rwTex[name] = rw;
        }
        *reinterpret_cast<BYTE**>(g_txdObjects + i * 0xC) = t.dict;
        *reinterpret_cast<short*>(g_txdObjects + i * 0xC + 6) = -1;
        g_txdFlags[i] = 1;
    }
    for (size_t i = g_txds.size(); i < g_txds.size() + 4; ++i) {
        g_txdFlags[i] = 0x80; // vazio
    }
    BYTE* pool = static_cast<BYTE*>(calloc(1, 0x14));
    *reinterpret_cast<BYTE**>(pool) = g_txdObjects;
    *reinterpret_cast<BYTE**>(pool + 4) = g_txdFlags;
    *reinterpret_cast<int*>(pool + 8) = static_cast<int>(g_txds.size()) + 4;
    Mem<BYTE*>(TXD_POOL) = pool;
}

// ---------------------------------------------------------------- geometria

struct Tri {
    WORD a, b, c, mat;
};
struct Geo {
    std::vector<Vec3> v, n;
    std::vector<float> uv;
    std::vector<Tri> t;
    void Quad(Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 nn, WORD mat) {
        WORD base = static_cast<WORD>(v.size());
        Vec3 ps[4] = {p0, p1, p2, p3};
        float uvs[8] = {0, 0, 1, 0, 1, 1, 0, 1};
        for (int i = 0; i < 4; ++i) {
            v.push_back(ps[i]);
            n.push_back(nn);
            uv.push_back(uvs[i * 2]);
            uv.push_back(uvs[i * 2 + 1]);
        }
        t.push_back({base, static_cast<WORD>(base + 1), static_cast<WORD>(base + 2), mat});
        t.push_back({base, static_cast<WORD>(base + 2), static_cast<WORD>(base + 3), mat});
    }
    void Box(Vec3 c, Vec3 s, WORD mat) {
        float x0 = c.x - s.x / 2, x1 = c.x + s.x / 2, y0 = c.y - s.y / 2, y1 = c.y + s.y / 2, z0 = c.z - s.z / 2,
              z1 = c.z + s.z / 2;
        Quad({x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, {0, 0, 1}, mat);
        Quad({x0, y1, z0}, {x1, y1, z0}, {x1, y0, z0}, {x0, y0, z0}, {0, 0, -1}, mat);
        Quad({x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, {0, -1, 0}, mat);
        Quad({x1, y1, z0}, {x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}, {0, 1, 0}, mat);
        Quad({x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}, {1, 0, 0}, mat);
        Quad({x0, y1, z0}, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {-1, 0, 0}, mat);
    }
    // Cilindro em pe (eixo Z) ou deitado no eixo X (rodas).
    void Cylinder(Vec3 c, float r, float h, int seg, WORD mat, bool alongX, WORD capMat) {
        for (int i = 0; i < seg; ++i) {
            float a0 = i * 6.2831853f / seg, a1 = (i + 1) * 6.2831853f / seg;
            float c0 = std::cos(a0), s0 = std::sin(a0), c1 = std::cos(a1), s1 = std::sin(a1);
            auto P = [&](float ca, float sa, float k) {
                return alongX ? Vec3{c.x + k * h / 2, c.y + ca * r, c.z + sa * r} : Vec3{c.x + ca * r, c.y + sa * r, c.z + k * h / 2};
            };
            Vec3 nn = alongX ? Vec3{0, (c0 + c1) / 2, (s0 + s1) / 2} : Vec3{(c0 + c1) / 2, (s0 + s1) / 2, 0};
            Quad(P(c0, s0, -1), P(c1, s1, -1), P(c1, s1, 1), P(c0, s0, 1), nn, mat);
            // tampas
            Vec3 top = alongX ? Vec3{c.x + h / 2, c.y, c.z} : Vec3{c.x, c.y, c.z + h / 2};
            Vec3 bottom = alongX ? Vec3{c.x - h / 2, c.y, c.z} : Vec3{c.x, c.y, c.z - h / 2};
            Vec3 nt = alongX ? Vec3{1, 0, 0} : Vec3{0, 0, 1};
            Vec3 nb = alongX ? Vec3{-1, 0, 0} : Vec3{0, 0, -1};
            Quad(top, top, P(c0, s0, 1), P(c1, s1, 1), nt, capMat);
            Quad(bottom, bottom, P(c1, s1, -1), P(c0, s0, -1), nb, mat);
        }
    }
};

struct Material {
    const char* texture; // nullptr = sem textura
    DWORD rgba;          // r | g<<8 | b<<16 | a<<24
};

DWORD Rgba(int r, int g, int b, int a = 255) {
    return r | (g << 8) | (b << 16) | (a << 24);
}

// ---------------------------------------------------------------- modelos

enum Shape { S_TRASHCAN, S_FENCE, S_HOUSE, S_CAR, S_PED, S_GUN, S_SAMP, S_NATIVE, S_CHAIR, S_FILLER };

struct Model {
    int id = 0;
    int type = 1;
    Shape shape = S_FILLER;
    int txd = 0;
    int img = 0;
    DWORD posn = 0, size = 0;
    std::string name;       // o nome no .ide
    std::string imgName;    // a entrada do .img naquela posicao ("" = o mesmo do .ide)
    const char* section = "objs";
    const char* ide = "DATA\\MAPS\\TROK\\TROK.IDE";
    BYTE* info = nullptr;
    BYTE* col = nullptr;
    std::vector<BYTE*> allocs;
    int requests = 0, releases = 0, loads = 0, unloads = 0;
    int variant = 0;
};
std::vector<Model> g_models;
std::map<int, int> g_modelIndex;

void* g_vtables[9][12];
int __fastcall TypeAtomic(void*, void*) { return 1; }
int __fastcall TypeTime(void*, void*) { return 3; }
int __fastcall TypeWeapon(void*, void*) { return 4; }
int __fastcall TypeClump(void*, void*) { return 5; }
int __fastcall TypeVehicle(void*, void*) { return 6; }
int __fastcall TypePed(void*, void*) { return 7; }

Model* FindModel(int id) {
    auto it = g_modelIndex.find(id);
    return it == g_modelIndex.end() ? nullptr : &g_models[it->second];
}

BYTE* Alloc(Model& m, size_t size) {
    BYTE* p = static_cast<BYTE*>(calloc(1, size));
    m.allocs.push_back(p);
    m.allocs.push_back(reinterpret_cast<BYTE*>(size)); // o tamanho logo depois (para envenenar)
    return p;
}

BYTE* MakeFrame(Model& m, BYTE* parent, Vec3 pos, const char* name) {
    BYTE* f = Alloc(m, 0xC0);
    *reinterpret_cast<BYTE**>(f + 4) = parent;
    float* mat = reinterpret_cast<float*>(f + 0x10);
    mat[0] = 1;  // direita
    mat[5] = 1;  // cima
    mat[10] = 1; // frente
    mat[12] = pos.x;
    mat[13] = pos.y;
    mat[14] = pos.z;
    strncpy(reinterpret_cast<char*>(f + NAME_OFF), name, 23);
    return f;
}

BYTE* MakeGeometry(Model& m, const Geo& g, const std::vector<Material>& mats, bool native) {
    BYTE* geom = Alloc(m, 0x60);
    *reinterpret_cast<int*>(geom + 0x10) = static_cast<int>(g.t.size());
    *reinterpret_cast<int*>(geom + 0x14) = static_cast<int>(g.v.size());
    *reinterpret_cast<int*>(geom + 0x18) = 1;
    *reinterpret_cast<int*>(geom + 0x1C) = 1;
    BYTE** matList = reinterpret_cast<BYTE**>(Alloc(m, mats.size() * 4 + 4));
    for (size_t i = 0; i < mats.size(); ++i) {
        BYTE* mat = Alloc(m, 0x20);
        if (mats[i].texture) {
            const Txd& t = g_txds[FindModel(m.id)->txd];
            auto it = t.rwTex.find(mats[i].texture);
            *reinterpret_cast<BYTE**>(mat) = it != t.rwTex.end() ? it->second : nullptr;
        }
        *reinterpret_cast<DWORD*>(mat + 4) = mats[i].rgba;
        matList[i] = mat;
    }
    *reinterpret_cast<BYTE***>(geom + 0x20) = matList;
    *reinterpret_cast<int*>(geom + 0x24) = static_cast<int>(mats.size());
    BYTE* tris = Alloc(m, g.t.size() * 8);
    memcpy(tris, g.t.data(), g.t.size() * 8);
    *reinterpret_cast<BYTE**>(geom + 0x2C) = tris;
    BYTE* uv = Alloc(m, g.uv.size() * 4);
    memcpy(uv, g.uv.data(), g.uv.size() * 4);
    *reinterpret_cast<BYTE**>(geom + 0x34) = uv;
    BYTE* morph = Alloc(m, 0x1C);
    if (!native) {
        BYTE* verts = Alloc(m, g.v.size() * 12);
        memcpy(verts, g.v.data(), g.v.size() * 12);
        BYTE* normals = Alloc(m, g.n.size() * 12);
        memcpy(normals, g.n.data(), g.n.size() * 12);
        *reinterpret_cast<BYTE**>(morph + 0x14) = verts;
        *reinterpret_cast<BYTE**>(morph + 0x18) = m.shape == S_HOUSE ? nullptr : normals; // a casa sem normais
    }
    *reinterpret_cast<BYTE**>(geom + 0x5C) = morph;
    return geom;
}

BYTE* MakeAtomic(Model& m, BYTE* frame, BYTE* geom, BYTE flags) {
    BYTE* a = Alloc(m, 0x70);
    a[0] = 1; // rpATOMIC
    a[2] = flags;
    *reinterpret_cast<BYTE**>(a + 4) = frame;
    *reinterpret_cast<BYTE**>(a + 0x18) = geom;
    return a;
}

void Load(Model& m) {
    BYTE* rw = nullptr;
    Geo g;
    std::vector<Material> mats;
    switch (m.shape) {
    case S_TRASHCAN:
        g.Cylinder({0, 0, 0.5f}, 0.35f, 1.0f, 14, 0, false, 1);
        mats = {{"trashcan", Rgba(255, 255, 255)}, {"metal", Rgba(255, 255, 255)}};
        break;
    case S_FENCE:
        g.Box({0, 0, 1.0f}, {3.0f, 0.06f, 2.0f}, 0);
        g.Box({-1.5f, 0, 1.0f}, {0.12f, 0.12f, 2.1f}, 1);
        g.Box({1.5f, 0, 1.0f}, {0.12f, 0.12f, 2.1f}, 1);
        mats = {{"fence", Rgba(255, 255, 255)}, {"metal", Rgba(255, 255, 255)}};
        break;
    case S_HOUSE:
        g.Box({0, 0, 3.0f}, {10, 8, 6}, 0);
        g.Box({0, 0, 6.6f}, {11, 9, 1.2f}, 1);
        g.Box({0, -4.05f, 1.2f}, {1.4f, 0.2f, 2.4f}, 2);
        mats = {{"brick", Rgba(255, 255, 255)}, {"dxtroof", Rgba(255, 255, 255)}, {nullptr, Rgba(80, 50, 30)}};
        break;
    case S_GUN:
        g.Box({0, 0.1f, 0}, {0.04f, 0.25f, 0.06f}, 0);
        g.Box({0, -0.02f, -0.07f}, {0.035f, 0.06f, 0.12f}, 0);
        mats = {{"metal", Rgba(90, 90, 95)}};
        break;
    case S_SAMP:
        g.Box({0, 0, 0.5f}, {1, 1, 1}, 0);
        g.Box({0, 0, 1.25f}, {0.5f, 0.5f, 0.5f}, 0);
        mats = {{"samplogo", Rgba(255, 255, 255)}};
        break;
    case S_CHAIR:
        g.Box({0, 0, 0.45f}, {0.5f, 0.5f, 0.06f}, 0);
        g.Box({0, 0.22f, 0.8f}, {0.5f, 0.06f, 0.7f}, 0);
        for (float x : {-0.22f, 0.22f}) {
            for (float y : {-0.22f, 0.22f}) {
                g.Box({x, y, 0.22f}, {0.05f, 0.05f, 0.45f}, 1);
            }
        }
        mats = {{"wood", Rgba(255, 255, 255)}, {"metal", Rgba(255, 255, 255)}};
        break;
    case S_NATIVE:
    case S_FILLER: {
        int k = m.variant;
        float s = 0.6f + (k % 5) * 0.35f;
        if (k % 3 == 0) {
            g.Box({0, 0, s / 2}, {s, s * 0.8f, s}, 0);
        } else if (k % 3 == 1) {
            g.Cylinder({0, 0, s / 2}, s * 0.4f, s, 10, 0, false, 0);
        } else {
            g.Box({0, 0, s * 0.15f}, {s * 1.6f, s * 0.6f, s * 0.3f}, 0);
            g.Box({0, 0, s * 0.6f}, {s * 0.4f, s * 0.4f, s * 0.6f}, 1);
        }
        static const DWORD colors[] = {Rgba(230, 120, 60), Rgba(90, 160, 220), Rgba(220, 200, 80), Rgba(140, 200, 120),
                                       Rgba(200, 120, 200)};
        mats = {{k % 2 ? "metal" : nullptr, colors[k % 5]}, {"wood", Rgba(255, 255, 255)}};
        break;
    }
    default:
        break;
    }
    if (m.shape == S_CAR || m.shape == S_PED) {
        // Clump: frame raiz, pecas com nome, atomicos ligados na lista do clump (+0x08; link do atomico em +0x40).
        rw = Alloc(m, 0x30);
        rw[0] = 2; // rpCLUMP
        BYTE* root = MakeFrame(m, nullptr, {0, 0, 0}, m.name.c_str());
        *reinterpret_cast<BYTE**>(rw + 4) = root;
        InitList(rw + 0x08);
        auto add = [&](BYTE* atomic) { LinkAfter(rw + 0x08, atomic + 0x40); };
        if (m.shape == S_CAR) {
            Geo body;
            body.Box({0, 0, 0.0f}, {1.9f, 4.4f, 0.55f}, 0);
            body.Box({0, -0.3f, 0.5f}, {1.6f, 2.0f, 0.5f}, 1);
            body.Box({-0.6f, 2.21f, 0.05f}, {0.4f, 0.04f, 0.15f}, 2);
            body.Box({0.6f, 2.21f, 0.05f}, {0.4f, 0.04f, 0.15f}, 3);
            body.Box({-0.6f, -2.21f, 0.05f}, {0.4f, 0.04f, 0.15f}, 4);
            body.Box({0.6f, -2.21f, 0.05f}, {0.4f, 0.04f, 0.15f}, 5);
            body.Box({0, 0, -0.2f}, {1.8f, 3.9f, 0.1f}, 6);
            std::vector<Material> bodyMats = {
                {"carbody", Rgba(60, 255, 0)},  {"glass", Rgba(255, 255, 255, 200)}, {nullptr, Rgba(255, 175, 0)},
                {nullptr, Rgba(0, 255, 200)},   {nullptr, Rgba(185, 255, 0)},          {nullptr, Rgba(255, 60, 0)},
                {nullptr, Rgba(255, 0, 175)}};
            BYTE* chassis = MakeFrame(m, root, {0, 0, 0.3f}, "chassis");
            add(MakeAtomic(m, chassis, MakeGeometry(m, body, bodyMats, false), 0x05));
            // A versao de longe (azul puro) e a porta amassada escondida (amarela): nenhuma pode aparecer.
            Geo lo;
            lo.Box({0, 0, 0.3f}, {2.2f, 4.8f, 1.4f}, 0);
            BYTE* vlo = MakeFrame(m, root, {0, 0, 0.3f}, "chassis_vlo");
            add(MakeAtomic(m, vlo, MakeGeometry(m, lo, {{nullptr, Rgba(0, 0, 255)}}, false), 0x05));
            Geo dam;
            dam.Box({-0.95f, 0.3f, 0.4f}, {0.1f, 1.2f, 0.8f}, 0);
            BYTE* door = MakeFrame(m, root, {0, 0, 0}, "door_lf_dam");
            add(MakeAtomic(m, door, MakeGeometry(m, dam, {{nullptr, Rgba(255, 255, 0)}}, false), 0x00));
            Geo wheel;
            wheel.Cylinder({0, 0, 0}, 0.34f, 0.26f, 14, 0, true, 1);
            std::vector<Material> wheelMats = {{"tyre", Rgba(255, 255, 255)}, {"metal", Rgba(255, 255, 255)}};
            const struct {
                const char* name;
                Vec3 pos;
            } wheels[] = {{"wheel_rf_dummy", {0.85f, 1.35f, -0.05f}}, {"wheel_lf_dummy", {-0.85f, 1.35f, -0.05f}},
                          {"wheel_rb_dummy", {0.85f, -1.35f, -0.05f}}, {"wheel_lb_dummy", {-0.85f, -1.35f, -0.05f}}};
            BYTE* wheelGeom = MakeGeometry(m, wheel, wheelMats, false);
            for (const auto& w : wheels) {
                add(MakeAtomic(m, MakeFrame(m, root, w.pos, w.name), wheelGeom, 0x05));
            }
        } else {
            Geo body;
            body.Box({0, 0, 0.35f}, {0.45f, 0.25f, 0.6f}, 1);    // tronco
            body.Box({0, 0, 0.8f}, {0.22f, 0.24f, 0.26f}, 0);    // cabeca
            body.Box({-0.12f, 0, -0.35f}, {0.16f, 0.18f, 0.8f}, 2); // pernas
            body.Box({0.12f, 0, -0.35f}, {0.16f, 0.18f, 0.8f}, 2);
            body.Box({-0.32f, 0, 0.3f}, {0.12f, 0.12f, 0.6f}, 0); // bracos
            body.Box({0.32f, 0, 0.3f}, {0.12f, 0.12f, 0.6f}, 0);
            std::vector<Material> pedMats = {{"skin", Rgba(255, 255, 255)}, {nullptr, Rgba(60, 90, 160)},
                                             {nullptr, Rgba(40, 40, 50)}};
            BYTE* frame = MakeFrame(m, root, {0, 0, 0}, "normal");
            add(MakeAtomic(m, frame, MakeGeometry(m, body, pedMats, false), 0x05));
        }
    } else {
        BYTE* frame = MakeFrame(m, nullptr, {0, 0, 0}, m.name.c_str());
        rw = MakeAtomic(m, frame, MakeGeometry(m, g, mats, m.shape == S_NATIVE), 0x05);
    }
    *reinterpret_cast<BYTE**>(m.info + 0x1C) = rw;
    Mem<BYTE>(STREAM_INFO + m.id * 0x14 + 0x10) = 1;
    ++m.loads;
}

void Unload(Model& m) {
    for (size_t i = 0; i + 1 < m.allocs.size(); i += 2) {
        memset(m.allocs[i], 0xDD, reinterpret_cast<size_t>(m.allocs[i + 1])); // quem usar depois ve lixo
    }
    m.allocs.clear();
    *reinterpret_cast<BYTE**>(m.info + 0x1C) = nullptr;
    Mem<BYTE>(STREAM_INFO + m.id * 0x14 + 0x10) = 0;
    ++m.unloads;
}

// ---------------------------------------------------------------- funcoes do jogo de mentira

int g_requests = 0, g_loadAlls = 0, g_releases = 0, g_gameFrames = 0;

void __cdecl FakeRequestModel(int id, int flags) {
    ++g_requests;
    Model* m = FindModel(id);
    if (!m) {
        return;
    }
    ++m->requests;
    Mem<BYTE>(STREAM_INFO + id * 0x14 + 6) |= static_cast<BYTE>(flags);
    if (Mem<BYTE>(STREAM_INFO + id * 0x14 + 0x10) != 1) {
        Mem<BYTE>(STREAM_INFO + id * 0x14 + 0x10) = 2;
    }
}

void __cdecl FakeLoadAll(bool) {
    ++g_loadAlls;
    for (Model& m : g_models) {
        if (Mem<BYTE>(STREAM_INFO + m.id * 0x14 + 0x10) == 2) {
            if (Mem<DWORD>(STREAM_INFO + m.id * 0x14 + 0xC) == 0) {
                Mem<BYTE>(STREAM_INFO + m.id * 0x14 + 0x10) = 0;
                continue;
            }
            Load(m);
        }
    }
}

void __cdecl FakeMissionDoesntRequire(int id) {
    ++g_releases;
    Model* m = FindModel(id);
    if (!m) {
        return;
    }
    ++m->releases;
    BYTE& flags = Mem<BYTE>(STREAM_INFO + id * 0x14 + 6);
    flags &= ~4;
    if (flags == 0 && Mem<BYTE>(STREAM_INFO + id * 0x14 + 0x10) == 1) {
        Unload(*m); // streaming apressado: solta na hora
    }
}

void __cdecl FakeGameProcess() {
    ++g_gameFrames;
}

// CWorld::ProcessLineOfSight
BYTE g_losEntity[0x80];
float g_losMatrix[16];
bool g_losOn = false;
Vec3 g_losFrom = {0, 0, 0}, g_losTo = {0, 0, 0};
int g_losCalls = 0;
bool __cdecl FakeLineOfSight(const Vec3& from, const Vec3& to, BYTE* colPoint, DWORD* entity, bool, bool, bool, bool,
                             bool, bool, bool, bool) {
    ++g_losCalls;
    g_losFrom = from;
    g_losTo = to;
    if (!g_losOn) {
        return false;
    }
    Vec3 hit = {0.5f, 8.0f - 0.35f, 0.6f};
    memcpy(colPoint, &hit, sizeof(hit));
    *entity = reinterpret_cast<DWORD>(g_losEntity);
    return true;
}

// CSprite::CalcScreenCoors: camera fixa em (0, -6, 1.8) olhando para +Y.
bool __cdecl FakeCalcScreenCoors(const Vec3& in, Vec3* out, float* w, float* h, bool, bool) {
    float depth = in.y - (-6.0f);
    if (depth < 0.2f) {
        return false;
    }
    float focal = W * 0.5f;
    out->x = W * 0.5f + (in.x - 0.0f) / depth * focal;
    out->y = H * 0.5f - (in.z - 1.8f) / depth * focal;
    out->z = depth;
    *w = *h = focal / depth;
    return true;
}

// CRunningScript::ProcessOneCommand: le um comando (opcode + parametros) e faz o que o jogo faria.
struct Command {
    int op;
    std::vector<float> f;
    std::vector<int> i;
};
std::vector<Command> g_commands;
std::map<int, int> g_entities; // handle -> opcode que criou
int g_nextHandle = 100;

char __fastcall FakeProcessOneCommand(BYTE* script, void*) {
    BYTE*& ip = *reinterpret_cast<BYTE**>(script + 0x14);
    Command c;
    c.op = *reinterpret_cast<WORD*>(ip);
    ip += 2;
    int params = 0, outIndex = -1;
    switch (c.op) {
    case 0x0107: params = 5; break; // modelo, x, y, z, saida
    case 0x00A5: params = 5; break;
    case 0x009A: params = 6; break; // tipo, modelo, x, y, z, saida
    case 0x0177: case 0x0175: case 0x0173: params = 2; break;
    case 0x0550: case 0x020A: params = 2; break;
    case 0x03CA: case 0x056E: case 0x056D: case 0x0108: case 0x00A6: case 0x009B: params = 1; break;
    default:
        printf("FALHA: opcode inesperado 0x%04X\n", c.op);
        ++g_failures;
        return 0;
    }
    for (int k = 0; k < params; ++k) {
        BYTE type = *ip++;
        if (type == 0x01) {
            c.i.push_back(*reinterpret_cast<int*>(ip));
            c.f.push_back(static_cast<float>(*reinterpret_cast<int*>(ip)));
            ip += 4;
        } else if (type == 0x06) {
            c.f.push_back(*reinterpret_cast<float*>(ip));
            c.i.push_back(static_cast<int>(*reinterpret_cast<float*>(ip)));
            ip += 4;
        } else if (type == 0x03) {
            outIndex = *reinterpret_cast<WORD*>(ip);
            ip += 2;
        } else {
            printf("FALHA: tipo de parametro inesperado 0x%02X\n", type);
            ++g_failures;
            return 0;
        }
    }
    int* locals = reinterpret_cast<int*>(script + 0x3C);
    switch (c.op) {
    case 0x0107:
    case 0x00A5:
    case 0x009A:
        if (outIndex >= 0) {
            locals[outIndex] = g_nextHandle;
            g_entities[g_nextHandle] = c.op;
            ++g_nextHandle;
        }
        break;
    case 0x03CA:
    case 0x056E:
    case 0x056D:
        script[0xC5] = g_entities.count(c.i[0]) ? 1 : 0;
        break;
    case 0x0108:
    case 0x00A6:
    case 0x009B:
        g_entities.erase(c.i[0]);
        break;
    default:
        break;
    }
    g_commands.push_back(c);
    return 0;
}

// ---------------------------------------------------------------- arquivos do jogo

void WriteText(const char* path, const std::string& text) {
    FILE* f = fopen(path, "wb");
    if (f) {
        fwrite(text.data(), 1, text.size(), f);
        fclose(f);
    }
}

void WriteImg(const char* path, int img) {
    struct Entry {
        DWORD posn;
        WORD size;
        std::string name;
    };
    std::vector<Entry> entries;
    for (const Model& m : g_models) {
        if (m.img == img && m.size) {
            entries.push_back({m.posn, static_cast<WORD>(m.size), (m.imgName.empty() ? m.name : m.imgName) + ".dff"});
        }
    }
    for (const Txd& t : g_txds) {
        if (t.img == img) {
            entries.push_back({t.posn, static_cast<WORD>(t.size), t.name + ".txd"});
        }
    }
    FILE* f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fwrite("VER2", 1, 4, f);
    DWORD count = static_cast<DWORD>(entries.size());
    fwrite(&count, 4, 1, f);
    for (const Entry& e : entries) {
        BYTE rec[32] = {};
        memcpy(rec, &e.posn, 4);
        memcpy(rec + 4, &e.size, 2);
        strncpy(reinterpret_cast<char*>(rec + 8), e.name.c_str(), 23);
        fwrite(rec, 32, 1, f);
    }
    fclose(f);
}

void WriteGameFiles() {
    CreateDirectoryA("data", nullptr);
    CreateDirectoryA("data\\maps", nullptr);
    CreateDirectoryA("data\\maps\\trok", nullptr);
    CreateDirectoryA("models", nullptr);
    CreateDirectoryA("SAMP", nullptr);
    WriteText("data\\default.dat", "# arquivos padrao\r\nIDE DATA\\VEHICLES.IDE\r\nIDE DATA\\PEDS.IDE\r\nIDE DATA\\DEFAULT.IDE\r\n");
    WriteText("data\\gta.dat", "IMG MODELS\\GTA_INT.IMG\r\nIDE DATA\\MAPS\\TROK\\TROK.IDE\r\n");
    std::map<std::string, std::string> ides;
    std::map<std::string, std::map<std::string, std::string>> sections;
    for (const Model& m : g_models) {
        char line[160];
        if (std::string(m.section) == "cars") {
            snprintf(line, sizeof(line), "%d, %s, %s, car, INFERNUS, INFERNU, null, executive, 10, 0, 0, -1, 0.7, 0.7, -1\r\n",
                     m.id, m.name.c_str(), g_txds[m.txd].name.c_str());
        } else if (std::string(m.section) == "peds") {
            snprintf(line, sizeof(line), "%d, %s, %s, CIVMALE, STAT_STREET_GUY, man, 1FFF, 0, null, 3, 4, PED_TYPE_GEN, VOICE_GEN_RANDOM\r\n",
                     m.id, m.name.c_str(), g_txds[m.txd].name.c_str());
        } else if (std::string(m.section) == "weap") {
            snprintf(line, sizeof(line), "%d, %s, %s, colt45, 1, 30, 0\r\n", m.id, m.name.c_str(), g_txds[m.txd].name.c_str());
        } else if (std::string(m.section) == "tobj") {
            snprintf(line, sizeof(line), "%d, %s, %s, 100, 0, 20, 6\r\n", m.id, m.name.c_str(), g_txds[m.txd].name.c_str());
        } else {
            snprintf(line, sizeof(line), "%d, %s, %s, 150, 0\r\n", m.id, m.name.c_str(), g_txds[m.txd].name.c_str());
        }
        sections[m.ide][m.section] += line;
    }
    for (auto& ide : sections) {
        std::string text = "# feito pelo teste\r\n";
        for (auto& sec : ide.second) {
            text += sec.first + "\r\n" + sec.second + "end\r\n";
        }
        if (ide.first == "DATA\\MAPS\\TROK\\TROK.IDE") {
            text += "txdp\r\ntrashcan, generic\r\nend\r\n";
        }
        WriteText(ide.first.c_str(), text);
    }
    WriteImg("models\\gta3.img", 0);
    WriteImg("models\\gta_int.img", 1);
    WriteImg("SAMP\\SAMP.IMG", 2);
}

// ---------------------------------------------------------------- o mundo

void AddModel(int id, int type, Shape shape, const char* name, int txd, int img, const char* section, const char* ide) {
    Model m;
    m.id = id;
    m.type = type;
    m.shape = shape;
    m.name = name;
    m.txd = txd;
    m.img = img;
    m.section = section;
    m.ide = ide;
    g_modelIndex[id] = static_cast<int>(g_models.size());
    g_models.push_back(m);
}

void BuildWorld() {
    MakeTexture("trashcan", 0);
    MakeTexture("fence", 1);
    MakeTexture("brick", 2);
    MakeTexture("carbody", 3);
    MakeTexture("glass", 4);
    MakeTexture("tyre", 5);
    MakeTexture("skin", 6);
    MakeTexture("samplogo", 7);
    MakeTexture("metal", 8);
    MakeTexture("wood", 2);
    MakeTexture("dxtroof", 9);
    MakeTexture("unused_logo", 7);
    int tTrash = AddTxd("trashcan", {"trashcan", "metal", "unused_logo"}, 0);
    int tFence = AddTxd("fences", {"fence", "metal"}, 0);
    int tHouse = AddTxd("lahouse", {"brick", "dxtroof"}, 1);
    int tCar = AddTxd("infernus", {"carbody", "glass", "tyre", "metal"}, 0);
    int tPed = AddTxd("wmybu", {"skin"}, 0);
    int tGun = AddTxd("colt45", {"metal"}, 0);
    int tSamp = AddTxd("samp_objects", {"samplogo"}, 2);
    int tFill = AddTxd("trok_misc", {"metal", "wood"}, 0);
    AddModel(46, 7, S_PED, "wmybu", tPed, 0, "peds", "DATA\\PEDS.IDE");
    AddModel(290, 7, S_PED, "special01", tPed, 0, "peds", "DATA\\PEDS.IDE"); // sem arquivo
    AddModel(346, 4, S_GUN, "colt45", tGun, 0, "weap", "DATA\\DEFAULT.IDE");
    AddModel(411, 6, S_CAR, "infernus", tCar, 0, "cars", "DATA\\VEHICLES.IDE");
    AddModel(1337, 1, S_TRASHCAN, "trashcan01", tTrash, 0, "objs", "DATA\\MAPS\\TROK\\TROK.IDE");
    AddModel(1338, 3, S_FENCE, "fence01", tFence, 0, "tobj", "DATA\\MAPS\\TROK\\TROK.IDE");
    AddModel(1500, 1, S_NATIVE, "native01", tFill, 0, "objs", "DATA\\MAPS\\TROK\\TROK.IDE"); // sem vertices na memoria
    AddModel(2000, 1, S_CHAIR, "chair01", tFill, 0, "objs", "DATA\\MAPS\\TROK\\TROK.IDE");   // substituido
    AddModel(3000, 1, S_HOUSE, "lahouse01", tHouse, 1, "objs", "DATA\\MAPS\\TROK\\TROK.IDE");
    AddModel(19475, 1, S_SAMP, "samp_cube", tSamp, 2, "objs", "SAMP\\SAMP.IDE");
    for (int i = 0; i < 180; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "trok_obj_%03d", i);
        AddModel(5000 + i, 1, S_FILLER, name, tFill, 0, "objs", "DATA\\MAPS\\TROK\\TROK.IDE");
        g_models.back().variant = i;
    }
    FindModel(2000)->imgName = "chair02"; // o jogo le outra entrada: substituido

    // Posicoes no .img e o streaming.
    DWORD next[3] = {1, 1, 1};
    for (size_t i = 0; i < g_txds.size(); ++i) {
        Txd& t = g_txds[i];
        t.posn = next[t.img];
        t.size = 4;
        next[t.img] += t.size;
        DWORD si = STREAM_INFO + (20000 + static_cast<DWORD>(i)) * 0x14;
        Mem<BYTE>(si + 7) = static_cast<BYTE>(t.img);
        Mem<DWORD>(si + 8) = t.posn;
        Mem<DWORD>(si + 0xC) = t.size;
        Mem<BYTE>(si + 0x10) = 1;
    }
    for (Model& m : g_models) {
        m.posn = next[m.img];
        m.size = m.id == 290 ? 0 : 2 + m.id % 7;
        next[m.img] += m.size;
        DWORD si = STREAM_INFO + m.id * 0x14;
        Mem<BYTE>(si + 7) = static_cast<BYTE>(m.img);
        Mem<DWORD>(si + 8) = m.posn;
        Mem<DWORD>(si + 0xC) = m.size;
        // A informacao do modelo: vtable (GetModelType na 5a), TXD, flags, colisao, distancia, RwObject.
        m.info = static_cast<BYTE*>(calloc(1, 0x80));
        *reinterpret_cast<void***>(m.info) = g_vtables[m.type];
        *reinterpret_cast<short*>(m.info + 0xA) = static_cast<short>(m.txd);
        *reinterpret_cast<WORD*>(m.info + 0x12) = m.shape == S_FENCE ? 0x0002 : 0;
        *reinterpret_cast<float*>(m.info + 0x18) = m.shape == S_HOUSE ? 300.0f : 150.0f;
        Mem<BYTE*>(MODEL_PTRS + m.id * 4) = m.info;
    }
    // Colisao: a lixeira com caixa, esferas e triangulos; a casa so com a caixa (os dados nao carregaram).
    {
        Model& t = *FindModel(1337);
        BYTE* col = static_cast<BYTE*>(calloc(1, 0x30));
        Vec3 mn = {-0.36f, -0.36f, 0}, mx = {0.36f, 0.36f, 1.0f};
        memcpy(col, &mn, 12);
        memcpy(col + 0xC, &mx, 12);
        BYTE* data = static_cast<BYTE*>(calloc(1, 0x30));
        *reinterpret_cast<WORD*>(data + 0) = 1; // esferas
        *reinterpret_cast<WORD*>(data + 2) = 1; // caixas
        *reinterpret_cast<WORD*>(data + 4) = 2; // triangulos
        BYTE* spheres = static_cast<BYTE*>(calloc(1, 0x14));
        Vec3 sc = {0, 0, 1.0f};
        memcpy(spheres, &sc, 12);
        *reinterpret_cast<float*>(spheres + 0xC) = 0.3f;
        BYTE* boxes = static_cast<BYTE*>(calloc(1, 0x1C));
        Vec3 b0 = {-0.35f, -0.35f, 0}, b1 = {0.35f, 0.35f, 0.9f};
        memcpy(boxes, &b0, 12);
        memcpy(boxes + 0xC, &b1, 12);
        short* verts = static_cast<short*>(calloc(4, 6));
        short vv[12] = {-64, -64, 0, 64, -64, 0, 64, 64, 0, -64, 64, 0};
        memcpy(verts, vv, sizeof(vv));
        WORD* tris = static_cast<WORD*>(calloc(2, 8));
        WORD tt[8] = {0, 1, 2, 0, 0, 2, 3, 0};
        memcpy(tris, tt, sizeof(tt));
        *reinterpret_cast<BYTE**>(data + 0x08) = spheres;
        *reinterpret_cast<BYTE**>(data + 0x0C) = boxes;
        *reinterpret_cast<short**>(data + 0x14) = verts;
        *reinterpret_cast<WORD**>(data + 0x18) = tris;
        *reinterpret_cast<BYTE**>(col + 0x2C) = data;
        *reinterpret_cast<BYTE**>(t.info + 0x14) = col;
        Model& h = *FindModel(3000);
        BYTE* hc = static_cast<BYTE*>(calloc(1, 0x30));
        Vec3 hm = {-5.5f, -4.5f, 0}, hx = {5.5f, 4.5f, 7.2f};
        memcpy(hc, &hm, 12);
        memcpy(hc + 0xC, &hx, 12);
        *reinterpret_cast<BYTE**>(h.info + 0x14) = hc;
    }
    // ms_files
    const char* files[] = {"MODELS\\GTA3.IMG", "MODELS\\GTA_INT.IMG", "SAMP\\SAMP.IMG"};
    for (int i = 0; i < 3; ++i) {
        strcpy(reinterpret_cast<char*>(STREAM_FILES + i * 0x30), files[i]);
    }
    BuildTxds();
    WriteGameFiles();
}

// ---------------------------------------------------------------- quadros, entrada e capturas

HWND g_hwnd = nullptr;

void Frame() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    reinterpret_cast<void(__cdecl*)()>(FN_GAME_PROCESS)();
    g_dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, CLEAR, 1.0f, 0);
    if (SUCCEEDED(g_dev->BeginScene())) {
        g_dev->EndScene();
    }
    g_dev->Present(nullptr, nullptr, nullptr, nullptr);
    Sleep(8);
}

void Frames(int n) {
    for (int i = 0; i < n; ++i) {
        Frame();
    }
}

void MoveTo(int x, int y) {
    SetCursorPos(x, y);
    PostMessageA(g_hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
    Frames(2);
}

void Click(int x, int y, UINT down = WM_LBUTTONDOWN, UINT up = WM_LBUTTONUP) {
    MoveTo(x, y);
    PostMessageA(g_hwnd, down, down == WM_MBUTTONDOWN ? MK_MBUTTON : MK_LBUTTON, MAKELPARAM(x, y));
    Frames(2);
    PostMessageA(g_hwnd, up, 0, MAKELPARAM(x, y));
    Frames(3);
}

void Key(WPARAM vk, bool sys = false) {
    PostMessageA(g_hwnd, sys ? WM_SYSKEYDOWN : WM_KEYDOWN, vk, 1);
    Frames(2);
    PostMessageA(g_hwnd, sys ? WM_SYSKEYUP : WM_KEYUP, vk, 0xC0000001);
    Frames(3);
}

void Type(const char* text) {
    for (const char* p = text; *p; ++p) {
        PostMessageA(g_hwnd, WM_CHAR, static_cast<WPARAM>(*p), 1);
        Frames(1);
    }
    Frames(3);
}

bool Capture(const char* name) {
    IDirect3DSurface9* back = nullptr;
    IDirect3DSurface9* sys = nullptr;
    bool ok = false;
    if (SUCCEEDED(g_dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) &&
        SUCCEEDED(g_dev->CreateOffscreenPlainSurface(W, H, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, nullptr)) &&
        SUCCEEDED(g_dev->GetRenderTargetData(back, sys))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
            for (int y = 0; y < H; ++y) {
                memcpy(&g_shot[y * W], static_cast<BYTE*>(lr.pBits) + y * lr.Pitch, W * 4);
            }
            sys->UnlockRect();
            ok = true;
        }
    }
    if (sys) {
        sys->Release();
    }
    if (back) {
        back->Release();
    }
    if (!ok) {
        return false;
    }
    std::string path = g_out + "\\" + name;
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }
    BITMAPFILEHEADER fh = {};
    BITMAPINFOHEADER ih = {};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + W * H * 3;
    ih.biSize = sizeof(ih);
    ih.biWidth = W;
    ih.biHeight = H;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    static BYTE row[W * 3];
    for (int y = H - 1; y >= 0; --y) {
        for (int x = 0; x < W; ++x) {
            DWORD p = g_shot[y * W + x];
            row[x * 3] = p & 0xFF;
            row[x * 3 + 1] = (p >> 8) & 0xFF;
            row[x * 3 + 2] = (p >> 16) & 0xFF;
        }
        fwrite(row, sizeof(row), 1, f);
    }
    fclose(f);
    return true;
}

DWORD Pixel(int x, int y) {
    return g_shot[y * W + x] & 0xFFFFFF;
}

// Quantas cores diferentes (em 32 niveis) num retangulo: uma miniatura de verdade tem varias.
int Variety(int x0, int y0, int x1, int y1) {
    std::vector<bool> seen(32 * 32 * 32, false);
    int n = 0;
    for (int y = y0; y < y1; y += 2) {
        for (int x = x0; x < x1; x += 2) {
            DWORD p = Pixel(x, y);
            int k = ((p >> 19) & 31) << 10 | ((p >> 11) & 31) << 5 | ((p >> 3) & 31);
            if (!seen[k]) {
                seen[k] = true;
                ++n;
            }
        }
    }
    return n;
}

// Pixels "azul puro" (a versao de longe do carro) e "amarelo puro" (a porta amassada).
int Count(int x0, int y0, int x1, int y1, bool blue) {
    int n = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            DWORD p = Pixel(x, y);
            int r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
            n += blue ? (b > 120 && b > 3 * (r + g) + 30) : (r > 150 && g > 150 && b < 40);
        }
    }
    return n;
}

int LogCount(const char* text) {
    FILE* f = fopen("Trok DevTools.log", "r");
    if (!f) {
        return 0;
    }
    char line[512];
    int n = 0;
    while (fgets(line, sizeof(line), f)) {
        n += strstr(line, text) != nullptr;
    }
    fclose(f);
    return n;
}

bool LogHas(const char* text) {
    return LogCount(text) > 0;
}

// O botao claro "Criar no mundo": 12 pixels claros seguidos na coluna x 1045 (dentro do botao, antes do texto escuro).
int FindButton() {
    for (int y = 300; y < 560; ++y) {
        bool run = true;
        for (int k = 0; k < 12 && run; ++k) {
            DWORD p = Pixel(1045, y + k);
            run = ((p >> 16) & 0xFF) > 200 && ((p >> 8) & 0xFF) > 200 && (p & 0xFF) > 200;
        }
        if (run) {
            return y + 8;
        }
    }
    return 0;
}

const Command* LastCommand(int op) {
    for (auto it = g_commands.rbegin(); it != g_commands.rend(); ++it) {
        if (it->op == op) {
            return &*it;
        }
    }
    return nullptr;
}

int CountCommands(int op) {
    int n = 0;
    for (const Command& c : g_commands) {
        n += c.op == op;
    }
    return n;
}

} // namespace

// A linha de comando do fake_gta.exe, com aspas: fake_gta.exe "<asi>" <pasta>.
std::vector<std::string> Args() {
    std::vector<std::string> out;
    const char* p = GetCommandLineA();
    while (*p) {
        while (*p == ' ') {
            ++p;
        }
        if (!*p) {
            break;
        }
        std::string a;
        bool quoted = *p == '"';
        if (quoted) {
            ++p;
        }
        while (*p && (quoted ? *p != '"' : *p != ' ')) {
            a += *p++;
        }
        if (quoted && *p == '"') {
            ++p;
        }
        out.push_back(a);
    }
    return out;
}

extern "C" __declspec(dllexport) int Run(unsigned memory, unsigned size) {
    std::vector<std::string> args = Args();
    int argc = static_cast<int>(args.size());
    std::vector<char*> argvList;
    for (std::string& a : args) {
        argvList.push_back(&a[0]);
    }
    char** argv = argvList.data();
    if (argc < 3) {
        printf("uso: fake_gta.exe <asi> <pasta das capturas>\n");
        return 2;
    }
    g_out = argv[2];
    if (memory > FN_REQUEST || memory + size < SCREEN_H + 0x100000) {
        printf("FALHA: a memoria falsa do GTA (0x%08X..0x%08X) nao cobre 0x4087E0..0xC9C060\n", memory, memory + size);
        return 2;
    }
    Mem<DWORD>(VERSION_MARK) = 0x94BF;
    Mem<DWORD>(MODEL_PTRS_REF) = MODEL_PTRS;
    Mem<int>(NODE_NAME_OFFSET) = NAME_OFF;
    Mem<int>(RASTER_EXT_OFFSET) = EXT_OFF;
    for (int t = 0; t < 9; ++t) {
        for (int k = 0; k < 12; ++k) {
            g_vtables[t][k] = reinterpret_cast<void*>(TypeAtomic);
        }
    }
    g_vtables[3][4] = reinterpret_cast<void*>(TypeTime);
    g_vtables[4][4] = reinterpret_cast<void*>(TypeWeapon);
    g_vtables[5][4] = reinterpret_cast<void*>(TypeClump);
    g_vtables[6][4] = reinterpret_cast<void*>(TypeVehicle);
    g_vtables[7][4] = reinterpret_cast<void*>(TypePed);
    Redirect(FN_REQUEST, reinterpret_cast<void*>(FakeRequestModel));
    Redirect(FN_LOAD_ALL, reinterpret_cast<void*>(FakeLoadAll));
    Redirect(FN_MISSION_DOESNT, reinterpret_cast<void*>(FakeMissionDoesntRequire));
    Redirect(FN_GAME_PROCESS, reinterpret_cast<void*>(FakeGameProcess));
    Redirect(FN_LOS, reinterpret_cast<void*>(FakeLineOfSight));
    Redirect(FN_CALC_SCREEN, reinterpret_cast<void*>(FakeCalcScreenCoors));
    Redirect(FN_PROCESS_ONE, reinterpret_cast<void*>(FakeProcessOneCommand));

    // Camera em (0, -6, 1.8) olhando para +Y; o personagem em (0, 0, 1) olhando para +Y.
    static float camera[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -6, 1.8f, 1};
    Mem<float*>(CAMERA + 0x14) = camera;
    static BYTE ped[0x800] = {};
    static float pedMatrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1};
    *reinterpret_cast<float**>(ped + 0x14) = pedMatrix;
    Mem<BYTE*>(PLAYER_PED) = ped;
    // O que a mira acha: a lixeira 1337, em (0.5, 8, 0).
    memset(g_losEntity, 0, sizeof(g_losEntity));
    float lm[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.5f, 8.0f, 0.0f, 1};
    memcpy(g_losMatrix, lm, sizeof(lm));
    *reinterpret_cast<float**>(g_losEntity + 0x14) = g_losMatrix;
    *reinterpret_cast<short*>(g_losEntity + 0x22) = 1337;
    g_losEntity[0x36] = 4;

    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "Grand theft auto San Andreas";
    RegisterClassA(&wc);
    g_hwnd = CreateWindowExA(0, wc.lpszClassName, "GTA: San Andreas", WS_POPUP | WS_VISIBLE, 0, 0, W, H, nullptr,
                             nullptr, wc.hInstance, nullptr);
    SetForegroundWindow(g_hwnd);
    Mem<int>(SCREEN_W) = W;
    Mem<int>(SCREEN_H) = H;
    Mem<HWND>(GTA_D3D_WINDOW) = g_hwnd;
    Mem<HWND>(GTA_WINDOW) = g_hwnd;

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    g_pp.BackBufferWidth = W;
    g_pp.BackBufferHeight = H;
    g_pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    g_pp.BackBufferCount = 1;
    g_pp.SwapEffect = D3DSWAPEFFECT_COPY;
    g_pp.hDeviceWindow = g_hwnd;
    g_pp.Windowed = TRUE;
    g_pp.EnableAutoDepthStencil = TRUE;
    g_pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    g_pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    HRESULT hr = d3d ? d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                         &g_pp, &g_dev)
                     : E_FAIL;
    Check(SUCCEEDED(hr) && g_dev, "device d3d9 criado");
    if (!g_dev) {
        return 1;
    }
    Mem<IDirect3DDevice9*>(GTA_DEVICE) = g_dev;
    g_shot = static_cast<DWORD*>(VirtualAlloc(nullptr, W * H * 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    BuildWorld();

    HMODULE asi = LoadLibraryA(argv[1]);
    Check(asi != nullptr, "asi carregado");
    if (!asi) {
        return 1;
    }
    for (int i = 0; i < 400 && !LogHas("CGame::Process desviado"); ++i) {
        Frame();
    }
    Check(LogHas("gta_sa.exe 1.0 US confirmado"), "log: 1.0 US confirmado");
    Check(LogHas("Present e Reset desviados"), "log: Present e Reset desviados");
    Check(LogHas("CGame::Process desviado"), "log: CGame::Process desviado");
    for (int i = 0; i < 300 && !LogHas("catalogo pronto"); ++i) {
        Frame();
    }
    Check(LogHas("catalogo: 5 .ide (190 modelos), 1 txdp"), "catalogo: 5 .ide lidos (o do SA-MP junto, 190 modelos) e 1 txdp");
    Check(LogHas("catalogo pronto: 190 modelos, 3 .img, 1 substituidos"),
          "catalogo: 190 modelos, 3 .img e 1 substituido, ja com o menu fechado");

    // 1) Informacoes ao apontar (ligado pelo .ini), menu fechado.
    g_losOn = true;
    Frames(10);
    Check(g_losCalls > 0, "a linha de visao roda com o menu fechado");
    CheckF(std::fabs(g_losFrom.y - 0.8f) < 0.05f && std::fabs(g_losTo.y - 294.0f) < 0.5f,
           "a linha comeca depois do personagem (y %.2f) e vai 300 m pela camera (ate y %.1f)", g_losFrom.y, g_losTo.y);
    Capture("05-apontar.bmp");
    {
        // A caixa da lixeira em (0.5, 8, 0): na tela, perto de x 1028 e y 696..540.
        int lit = 0;
        for (int y = 500; y < 760; ++y) {
            DWORD p = Pixel(1000, y) | Pixel(1060, y);
            lit += ((p >> 16) & 0xFF) > 150;
        }
        Check(lit > 0, "inspetor: a caixa do objeto desenhada em volta dele");
        CheckF(Variety(1000, 580, 1400, 760) > 6, "inspetor: o cartao com as informacoes ao lado do centro (%.0f cores)",
               Variety(1000, 580, 1400, 760));
    }

    // 2) Botao do meio: abre o menu no card da lixeira.
    Click(W / 2, H / 2, WM_MBUTTONDOWN, WM_MBUTTONUP);
    Frames(30);
    g_losOn = false;
    Model& trash = *FindModel(1337);
    CheckF(trash.requests > 0, "botao do meio: o card da lixeira carrega o modelo (%.0f pedidos)", trash.requests);
    Capture("06-card-objeto.bmp");
    CheckF(Variety(232, 150, 1016, 934) > 20, "card: o visualizador mostra o modelo (%.0f cores)", Variety(232, 150, 1016, 934));
    // Criar no mundo: o primeiro botao do lado direito.
    int createY = FindButton();
    CheckF(createY > 0, "card: o botao Criar no mundo (claro) na altura %.0f", createY);
    if (createY) {
        Click(1356, createY);
        Frames(5);
    }
    const Command* create = LastCommand(0x0107);
    Check(create && create->i[0] == 1337, "criar: CREATE_OBJECT (0107) com o modelo 1337");
    if (create) {
        CheckF(std::fabs(create->f[1]) < 0.01f && std::fabs(create->f[2] - 2.5f) < 0.05f && std::fabs(create->f[3]) < 0.05f,
               "criar: a 2,5 m na frente do personagem, com a base no chao (%.2f %.2f %.2f)", create->f[1],
               create->f[2], create->f[3]);
    }
    const Command* heading = LastCommand(0x0177);
    Check(heading && std::fabs(heading->f[1] - 180.0f) < 0.5f, "criar: de frente para o personagem (0177, 180 graus)");
    const Command* freeze = LastCommand(0x0550);
    Check(freeze && freeze->i[1] == 1, "criar: parado onde nasceu (0550)");
    Check(LogHas("criado 1337 (objeto)"), "log: criado 1337");
    Key(VK_ESCAPE);
    Frames(5);
    CheckF(trash.releases > 0, "fechar o card solta o modelo (%.0f)", trash.releases);

    // 3) A grade com as miniaturas.
    Frames(240);
    Capture("01-grade.bmp");
    int thumbs = 0;
    for (int c = 0; c < 11; ++c) {
        int x = 193 + c * 139 + 66, y = 190 + 66;
        thumbs += Variety(x - 40, y - 40, x + 40, y + 40) > 6;
    }
    CheckF(thumbs >= 9, "grade: miniaturas desenhadas na primeira linha (%.0f de 11)", thumbs);
    int released = 0, loaded = 0;
    for (const Model& m : g_models) {
        loaded += m.loads > 0;
        released += m.releases > 0;
    }
    CheckF(loaded >= 30 && released >= loaded - 2, "grade: %.0f modelos carregados e %.0f soltos depois da miniatura",
           loaded, released);
    Check(FindModel(290)->loads == 0, "grade: o 290 (sem arquivo) nem e pedido");

    // 4) Busca: "infernus" e abrir o card.
    Click(250, 154);
    Type("infernus");
    Frames(20);
    Capture("02-busca.bmp");
    Click(259, 272);
    Frames(40);
    Model& car = *FindModel(411);
    CheckF(car.requests > 0, "busca + clique: o card do Infernus carrega (%.0f pedidos)", car.requests);
    Capture("03-card-veiculo.bmp");
    CheckF(Variety(232, 150, 1016, 934) > 20, "card do veiculo: o visualizador mostra o carro (%.0f cores)",
           Variety(232, 150, 1016, 934));
    CheckF(Count(232, 150, 1016, 934, true) < 30, "veiculo: a versao de longe (_vlo, azul) fica de fora (%.0f pixels)",
           Count(232, 150, 1016, 934, true));
    CheckF(Count(232, 150, 1016, 934, false) < 30, "veiculo: a peca amassada escondida (amarela) fica de fora (%.0f)",
           Count(232, 150, 1016, 934, false));
    // Criar o carro.
    createY = FindButton();
    if (createY) {
        Click(1356, createY);
        Frames(5);
    }
    const Command* carCmd = LastCommand(0x00A5);
    Check(carCmd && carCmd->i[0] == 411, "criar veiculo: CREATE_CAR (00A5) com o 411");
    if (carCmd) {
        CheckF(carCmd->f[2] > 3.4f && carCmd->f[2] < 5.0f && carCmd->f[3] > 0.0f && carCmd->f[3] < 1.0f,
               "criar veiculo: na frente, longe o bastante para caber (y %.2f, z %.2f)", carCmd->f[2], carCmd->f[3]);
    }
    const Command* lock = LastCommand(0x020A);
    Check(lock && lock->i[1] == 2, "criar veiculo: trancado (020A, 2)");
    // Uma textura em destaque: o titulo "Texturas usadas" e a primeira linha com texto abaixo dos botoes; o primeiro
    // quadrinho fica logo embaixo dele.
    int texY = 0;
    for (int y = createY + 75; y < 700 && !texY; ++y) {
        if (Variety(1032, y, 1080, y + 2) > 2) {
            texY = y + 40;
        }
    }
    if (texY) {
        Click(1056, texY);
        Frames(10);
    }
    Capture("04-textura.bmp");
    Key(VK_ESCAPE);
    Frames(5);
    CheckF(car.releases > 0, "fechar o card do veiculo solta o modelo (%.0f)", car.releases);

    // 5) Apagar os criados (pelo Geral nao: pelo card de novo) -- o carro e a lixeira.
    Click(259, 272);
    Frames(30);
    int deleteY = createY ? createY + 29 : 0;
    if (deleteY) {
        Click(1180, deleteY);
        Frames(5);
    }
    Check(CountCommands(0x0108) == 1 && CountCommands(0x00A6) == 1 && g_entities.empty(),
          "apagar criados: DELETE_OBJECT (0108) e DELETE_CAR (00A6), depois de conferir que existem");
    Key(VK_ESCAPE);
    Frames(5);

    // 6) Reset do device (Alt+Tab): os alvos do Trok DevTools saem antes e voltam depois.
    hr = g_dev->Reset(&g_pp);
    CheckF(SUCCEEDED(hr), "Reset do device aceito (0x%08X): nada do Trok DevTools ficou no pool padrao",
           static_cast<double>(static_cast<unsigned>(hr)));
    Frames(120);
    Capture("07-depois-do-reset.bmp");
    CheckF(Variety(193, 190, 330, 330) > 6, "depois do Reset a miniatura volta (%.0f cores)", Variety(193, 190, 330, 330));

    // 7) Fechar o menu.
    Key(VK_F10, true);
    Frames(5);
    int stillHeld = 0;
    for (const Model& m : g_models) {
        stillHeld += (Mem<BYTE>(STREAM_INFO + m.id * 0x14 + 6) & 4) != 0;
    }
    CheckF(stillHeld == 0, "menu fechado: nenhum modelo ficou preso pelo Trok DevTools (%.0f)", stillHeld);
    Check(!LogHas("recusad"), "log: nada recusado");

    printf(g_failures ? "FALHOU (%d)\n" : "PASSOU\n", g_failures);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), g_failures ? 1 : 0);
    return 0;
}

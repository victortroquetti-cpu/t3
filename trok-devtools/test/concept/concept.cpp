// Imagens de conceito do Trok DevTools, sem o jogo: o menu de verdade (devtools.cpp e o kit da casa) desenhado com o
// ImGui e rasterizado em software sobre a rua de mentira do assets.py, com as miniaturas, o visualizador e as texturas
// tambem de la. O catalogo, o "core" e o "render" do .asi sao trocados por versoes que leem o conceito.txt.
// Uso: ./concept <pasta do assets.py> <pasta de saida>
#include "../../src/devtools.cpp"

#include <cstdarg>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>

void DevToolsLog(const char*, ...) {}

namespace concept {

constexpr int W = 1920, H = 1080;
constexpr intptr_t FONT = 1, THUMB = 100000, VIEWER = 300000, VIEWER_COL = 400000, TEX = 500000, VIEWER_GREEN = 600000;

struct Image {
    std::vector<unsigned char> rgba;
    int w = 0, h = 0;
};
std::map<intptr_t, Image> images;

struct MeshData {
    int vertices = 0, triangles = 0, materials = 0, atomics = 1;
    float lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};
};
std::map<int, MeshData> meshes;

struct TexData {
    int model = 0;
    std::string name, format;
    int w = 0, h = 0, levels = 1;
    bool used = false, alpha = false;
    intptr_t id = 0;
};
std::vector<TexData> texs;

struct ColData {
    int boxes = 0, spheres = 0, tris = 0;
};
std::map<int, ColData> cols;
float cam[16];
core::Look target;
core::TextureUsers users; // o que a busca acharia para a textura dos farois do Infernus
float spawnDist = 4.0f;
int failures = 0;

void Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "ok" : "FALHOU", what);
    if (!ok) {
        ++failures;
    }
}

bool LoadImage(const std::string& path, intptr_t id) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    int wh[2];
    Image img;
    bool ok = fread(wh, sizeof(int), 2, f) == 2;
    if (ok) {
        img.w = wh[0], img.h = wh[1];
        img.rgba.resize(static_cast<size_t>(img.w) * img.h * 4);
        ok = fread(img.rgba.data(), 1, img.rgba.size(), f) == img.rgba.size();
    }
    fclose(f);
    if (ok) {
        images[id] = std::move(img);
    }
    return ok;
}

std::vector<std::string> Split(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, '|')) {
        out.push_back(part);
    }
    if (!s.empty() && s.back() == '|') {
        out.push_back("");
    }
    return out;
}

std::string Lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

} // namespace concept

// ---------------------------------------------------------------- o catalogo, o core e o render do conceito

namespace catalog {

std::vector<Entry> all;
std::map<int, int> index;
std::vector<std::string> archives = {"gta3.img", "gta_int.img", "samp.img"};

void Start(const char*) {}
void Update() {}
bool Ready() { return true; }
const char* Status() { return ""; }
const std::vector<Entry>& All() { return all; }
const Entry* Find(int id) {
    auto it = index.find(id);
    return it == index.end() ? nullptr : &all[it->second];
}

std::vector<int> Filter(const char* query, int kind, int archive, bool onlyReplaced) {
    std::vector<std::string> terms;
    std::stringstream ss(concept::Lower(query ? query : ""));
    std::string t;
    while (ss >> t) {
        terms.push_back(t);
    }
    std::vector<int> out;
    for (int i = 0; i < static_cast<int>(all.size()); ++i) {
        const Entry& e = all[i];
        if ((kind >= 0 && e.kind != kind) || (archive >= 0 && e.imgName != archives[archive]) || (onlyReplaced && !e.replaced)) {
            continue;
        }
        bool ok = true;
        for (const std::string& term : terms) {
            ok = ok && e.search.find(term) != std::string::npos;
        }
        if (ok) {
            out.push_back(i);
        }
    }
    return out;
}

const std::vector<std::string>& Archives() { return archives; }

const char* KindName(int kind) {
    static const char* const names[] = {"Objetos", "Ve\xC3\xAD" "culos", "Skins", "Armas"};
    return kind >= 0 && kind < KIND_COUNT ? names[kind] : "Todos";
}

const char* TypeName(const Entry& e) {
    switch (e.kind) {
    case K_VEHICLE: return "Ve\xC3\xAD" "culo";
    case K_PED: return "Skin";
    case K_WEAPON: return "Arma";
    default: break;
    }
    return "Objeto";
}

} // namespace catalog

namespace core {

bool inspect = false;
int openId = -1;
mesh::Mesh openMesh;
int spawned = 0;
std::string message;
bool messageNew = false;

void SetGameDir(const char*) {}
void GameTick() {}
void BeforeUi(IDirect3DDevice9*) {}
void SetBrowsing(bool) {}
void WantThumb(int) {}
bool ThumbFailed(int) { return false; }
int ThumbsQueued() { return 0; }
void SetThumbsPerFrame(int) {}

void Open(int id) {
    openId = id;
    openMesh = mesh::Mesh();
    auto it = concept::meshes.find(id);
    if (it == concept::meshes.end()) {
        return;
    }
    const concept::MeshData& d = it->second;
    openMesh.ok = true;
    openMesh.id = id;
    openMesh.min = {d.lo[0], d.lo[1], d.lo[2]};
    openMesh.max = {d.hi[0], d.hi[1], d.hi[2]};
    openMesh.vertices = d.vertices, openMesh.triangles = d.triangles;
    openMesh.materials = d.materials, openMesh.atomics = d.atomics;
    for (const concept::TexData& t : concept::texs) {
        if (t.model == id) {
            mesh::Texture tex;
            tex.name = t.name;
            tex.d3d = reinterpret_cast<void*>(t.id);
            tex.width = t.w, tex.height = t.h, tex.depth = 32;
            tex.alpha = t.alpha, tex.used = t.used;
            openMesh.textures.push_back(tex);
        }
    }
    auto c = concept::cols.find(id);
    openMesh.hasCol = true;
    openMesh.colLoaded = true;
    if (c != concept::cols.end()) {
        openMesh.colBoxes = c->second.boxes, openMesh.colSpheres = c->second.spheres, openMesh.colTriangles = c->second.tris;
    } else {
        openMesh.colBoxes = 1, openMesh.colSpheres = 10; // como os carros do jogo: esferas nas rodas e uma caixa
    }
}

void Close() { openId = -1; }
int OpenId() { return openId; }
const mesh::Mesh* OpenMesh() { return openId >= 0 && openMesh.ok ? &openMesh : nullptr; }
bool OpenFailed(std::string*) { return false; }

void Spawn(int id) {
    ++spawned;
    char text[96];
    snprintf(text, sizeof(text), "%d criado %.1f m \xC3\xA0 sua frente.", id, concept::spawnDist);
    for (char* c = text; *c; ++c) {
        if (*c == '.' && c[1] >= '0' && c[1] <= '9') {
            *c = ',';
        }
    }
    message = text;
    messageNew = true;
}
int SpawnedCount() { return spawned; }
void DespawnAll() { spawned = 0; }
bool TakeSpawnMessage(std::string* text, bool* ok) {
    if (!messageNew) {
        return false;
    }
    messageNew = false;
    *text = message;
    *ok = true;
    return true;
}

void FindTextureUsers(int, const std::string&) {}
const TextureUsers& TextureUsersNow() { return concept::users; }

void SetInspect(bool on) { inspect = on; }
bool InspectOn() { return inspect; }
const Look& Looking() {
    static Look none;
    return inspect ? concept::target : none;
}

// A aba Animacoes fica de fora do conceito (o .ifp "ainda lendo")
void PreviewPed(int) {}
const mesh::Mesh* PreviewMesh() { return nullptr; }
bool PreviewFailed(std::string*) { return false; }
int PlayerSkin() { return -1; }
void PlayAnim(int, bool, bool) {}
void StopAnim() {}
int PlayingAnim() { return -1; }
bool TakeAnimMessage(std::string*, bool*) { return false; }

} // namespace core

namespace gta {

bool Project(const Vec3& p, float* x, float* y) {
    const float* M = concept::cam;
    float v[4] = {p.x, p.y, p.z, 1.0f}, c[4];
    for (int r = 0; r < 4; ++r) {
        c[r] = M[r * 4] * v[0] + M[r * 4 + 1] * v[1] + M[r * 4 + 2] * v[2] + M[r * 4 + 3] * v[3];
    }
    if (c[3] < 0.05f) {
        return false;
    }
    *x = (c[0] / c[3] * 0.5f + 0.5f) * concept::W;
    *y = (1.0f - (c[1] / c[3] * 0.5f + 0.5f)) * concept::H;
    return true;
}

} // namespace gta

namespace render {

void SetDevice(IDirect3DDevice9*) {}
void DeviceLost() {}
bool HasThumb(int id) { return concept::images.count(concept::THUMB + id) > 0; }
bool Thumb(int id, ImTextureID* tex, ImVec2* uv0, ImVec2* uv1) {
    if (!HasThumb(id)) {
        return false;
    }
    *tex = reinterpret_cast<ImTextureID>(concept::THUMB + id);
    *uv0 = ImVec2(0, 0), *uv1 = ImVec2(1, 1);
    return true;
}
bool DrawThumb(const mesh::Mesh&) { return false; }
void ForgetThumbs() {}
int ThumbCount() {
    int n = 0;
    for (const auto& kv : concept::images) {
        n += kv.first >= concept::THUMB && kv.first < concept::VIEWER;
    }
    return n;
}
ImTextureID DrawViewer(const mesh::Mesh& m, const View& v, int) {
    if ((v.background & 0xFFFFFF) == 0x00B140 && concept::images.count(concept::VIEWER_GREEN + m.id)) {
        return reinterpret_cast<ImTextureID>(concept::VIEWER_GREEN + m.id);
    }
    intptr_t id = (v.collision ? concept::VIEWER_COL : concept::VIEWER) + m.id;
    if (!concept::images.count(id)) {
        id = concept::VIEWER + m.id;
    }
    return concept::images.count(id) ? reinterpret_cast<ImTextureID>(id) : nullptr;
}
bool Describe(void* d3d, TexInfo* out) {
    for (const concept::TexData& t : concept::texs) {
        if (reinterpret_cast<void*>(t.id) == d3d) {
            out->width = t.w, out->height = t.h, out->levels = t.levels;
            out->format = t.format.c_str();
            return true;
        }
    }
    return false;
}

} // namespace render

// A aba Audios fica de fora do conceito: o SFX "ainda lendo" e nada tocando.
namespace sfx {
namespace {
std::vector<Pack> noPacks;
std::vector<Bank> noBanks;
} // namespace
bool Ready() { return false; }
const char* Status() { return "conceito"; }
int Failed() { return 0; }
const std::vector<Pack>& Packs() { return noPacks; }
const std::vector<Bank>& Banks() { return noBanks; }
const Bank* FindBank(int) { return nullptr; }
const Sound* FindSound(int, int) { return nullptr; }
const char* KindName(int) { return ""; }
int BankCount() { return 0; }
int SoundCount() { return 0; }
Result Apply(const Filter&) { return Result(); }
bool ExportWav(int, int, std::string*) { return false; }
} // namespace sfx

namespace ifp {
namespace {
std::vector<File> noFiles;
std::vector<Anim> noAnims;
} // namespace
bool Ready() { return false; }
const char* Status() { return "conceito"; }
const std::vector<File>& Files() { return noFiles; }
const std::vector<Anim>& Anims() { return noAnims; }
const File* FindFile(int) { return nullptr; }
const Anim* FindAnim(int) { return nullptr; }
int ModCount() { return 0; }
int NewCount() { return 0; }
const char* Reference() { return ""; }
Result Apply(const Filter&) { return Result(); }
bool LoadClip(int, Clip*) { return false; }
} // namespace ifp

namespace pose {
int Matched(const mesh::Mesh&, const ifp::Clip&) { return 0; }
bool Apply(const mesh::Mesh&, const ifp::Clip&, float, bool, mesh::Mesh*) { return false; }
} // namespace pose

namespace player {
bool Play(int, int) { return false; }
void Stop() {}
void Update() {}
void SetRepeat(bool) {}
bool Repeat() { return false; }
bool Playing() { return false; }
int Bank() { return -1; }
int Sound() { return -1; }
float Position() { return 0; }
const char* Error() { return ""; }
} // namespace player

namespace audiospy {
namespace {
std::deque<Played> noHistory;
} // namespace
void SetIgnoreFeet(bool) {}
const std::deque<Played>& History() { return noHistory; }
void Clear() {}
bool BanksKnown() { return false; }
} // namespace audiospy

// ---------------------------------------------------------------- dados, rasterizador e cenas

namespace concept {

std::vector<float> fb;
std::vector<unsigned char> scene, sceneSpawned;
const Image* cur = nullptr;
Image font;

void LoadData(const std::string& dir) {
    std::ifstream in(dir + "/conceito.txt");
    std::string line;
    int nextTex = 0;
    while (std::getline(in, line)) {
        std::vector<std::string> f = Split(line);
        if (f.empty()) {
            continue;
        }
        if (f[0] == "entrada" && f.size() >= 14) {
            catalog::Entry e;
            e.id = std::stoi(f[1]);
            e.type = std::stoi(f[2]);
            e.kind = std::stoi(f[3]);
            e.section = f[4], e.dff = f[5], e.txd = f[6], e.ide = f[7], e.imgName = f[8];
            e.img = e.imgName == "gta3.img" ? 0 : e.imgName == "gta_int.img" ? 1 : 2;
            e.size = std::stoul(f[9]);
            e.flags = static_cast<WORD>(std::stoi(f[10]));
            e.drawDistance = std::stof(f[11]);
            e.replaced = f[12] == "1";
            e.imgEntry = f[13];
            e.search = Lower(f[1] + " " + e.dff + " " + e.txd + " " + e.imgName);
            catalog::index[e.id] = static_cast<int>(catalog::all.size());
            catalog::all.push_back(e);
        } else if (f[0] == "malha" && f.size() >= 12) {
            MeshData d;
            int id = std::stoi(f[1]);
            d.vertices = std::stoi(f[2]), d.triangles = std::stoi(f[3]), d.materials = std::stoi(f[4]), d.atomics = std::stoi(f[5]);
            for (int k = 0; k < 3; ++k) {
                d.lo[k] = std::stof(f[6 + k]), d.hi[k] = std::stof(f[9 + k]);
            }
            meshes[id] = d;
            LoadImage(dir + "/thumb_" + f[1] + ".rgba", THUMB + id);
            LoadImage(dir + "/viewer_" + f[1] + ".rgba", VIEWER + id);
            LoadImage(dir + "/viewer_" + f[1] + "_col.rgba", VIEWER_COL + id);
            LoadImage(dir + "/viewer_" + f[1] + "_verde.rgba", VIEWER_GREEN + id);
        } else if (f[0] == "textura" && f.size() >= 9) {
            TexData t;
            t.model = std::stoi(f[1]);
            t.name = f[2];
            t.w = std::stoi(f[3]), t.h = std::stoi(f[4]);
            t.used = f[5] == "1", t.alpha = f[6] == "1";
            t.format = f[7];
            t.levels = std::stoi(f[8]);
            t.id = TEX + nextTex++;
            LoadImage(dir + "/tex_" + t.name + ".rgba", t.id);
            texs.push_back(t);
        } else if (f[0] == "usuarios" && f.size() >= 5) {
            users.model = std::stoi(f[1]);
            users.texture = f[2];
            users.txd = 1; // a vehicle.txd, mae da TXD de cada carro
            users.txdName = f[3];
            for (size_t k = 4; k < f.size(); ++k) {
                users.users.push_back(std::stoi(f[k]));
            }
            users.checked = users.total = static_cast<int>(users.users.size());
            users.done = true;
        } else if (f[0] == "colisao" && f.size() >= 5) {
            cols[std::stoi(f[1])] = {std::stoi(f[2]), std::stoi(f[3]), std::stoi(f[4])};
        } else if (f[0] == "camera" && f.size() >= 17) {
            for (int k = 0; k < 16; ++k) {
                cam[k] = std::stof(f[1 + k]);
            }
        } else if (f[0] == "criado" && f.size() >= 3) {
            spawnDist = std::stof(f[2]);
        } else if (f[0] == "mira" && f.size() >= 18) {
            target.valid = true;
            target.model = std::stoi(f[1]);
            target.type = std::stoi(f[2]);
            target.area = std::stoi(f[3]);
            target.lodModel = std::stoi(f[4]);
            target.pos = {std::stof(f[5]), std::stof(f[6]), std::stof(f[7])};
            target.hit = {std::stof(f[8]), std::stof(f[9]), std::stof(f[10])};
            target.distance = std::stof(f[11]);
            float lo[3] = {std::stof(f[12]), std::stof(f[13]), std::stof(f[14])};
            float hi[3] = {std::stof(f[15]), std::stof(f[16]), std::stof(f[17])};
            for (int i = 0; i < 8; ++i) {
                target.box[i] = {target.pos.x + (i & 1 ? hi[0] : lo[0]), target.pos.y + (i & 2 ? hi[1] : lo[1]),
                                 target.pos.z + (i & 4 ? hi[2] : lo[2])};
            }
            target.hasBox = true;
        }
    }
    std::sort(catalog::all.begin(), catalog::all.end(), [](const catalog::Entry& a, const catalog::Entry& b) { return a.id < b.id; });
    catalog::index.clear();
    for (int i = 0; i < static_cast<int>(catalog::all.size()); ++i) {
        catalog::index[catalog::all[i].id] = i;
    }
}

std::vector<unsigned char> LoadRgb(const std::string& path) {
    std::vector<unsigned char> raw(static_cast<size_t>(W) * H * 3);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f || fread(raw.data(), 1, raw.size(), f) != raw.size()) {
        raw.assign(raw.size(), 40);
    }
    if (f) {
        fclose(f);
    }
    return raw;
}

void Clear(const std::vector<unsigned char>& bg) {
    fb.resize(static_cast<size_t>(W) * H * 3);
    for (size_t i = 0; i < fb.size(); ++i) {
        fb[i] = bg[i] / 255.0f;
    }
}

// Amostra bilinear (o D3D9 do jogo tambem filtra as imagens do menu).
void Sample(float u, float v, float out[4]) {
    const Image& t = *cur;
    float x = u * t.w - 0.5f, y = v * t.h - 0.5f;
    int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
    float fx = x - x0, fy = y - y0;
    float acc[4] = {0, 0, 0, 0};
    for (int k = 0; k < 4; ++k) {
        int xi = std::min(t.w - 1, std::max(0, x0 + (k & 1))), yi = std::min(t.h - 1, std::max(0, y0 + (k >> 1)));
        float wgt = ((k & 1) ? fx : 1 - fx) * ((k >> 1) ? fy : 1 - fy);
        const unsigned char* p = &t.rgba[(static_cast<size_t>(yi) * t.w + xi) * 4];
        for (int c = 0; c < 4; ++c) {
            acc[c] += p[c] / 255.0f * wgt;
        }
    }
    for (int c = 0; c < 4; ++c) {
        out[c] = acc[c];
    }
}

void Raster(const ImDrawVert& a, const ImDrawVert& b, const ImDrawVert& c, ImVec4 clip) {
    float minx = std::max(clip.x, std::floor(std::min({a.pos.x, b.pos.x, c.pos.x})));
    float maxx = std::min(clip.z, std::ceil(std::max({a.pos.x, b.pos.x, c.pos.x})));
    float miny = std::max(clip.y, std::floor(std::min({a.pos.y, b.pos.y, c.pos.y})));
    float maxy = std::min(clip.w, std::ceil(std::max({a.pos.y, b.pos.y, c.pos.y})));
    float area = (b.pos.x - a.pos.x) * (c.pos.y - a.pos.y) - (b.pos.y - a.pos.y) * (c.pos.x - a.pos.x);
    if (std::fabs(area) < 1e-6f) {
        return;
    }
    for (int y = static_cast<int>(miny); y < static_cast<int>(maxy); ++y) {
        for (int x = static_cast<int>(minx); x < static_cast<int>(maxx); ++x) {
            if (x < 0 || y < 0 || x >= W || y >= H) {
                continue;
            }
            float px = x + 0.5f, py = y + 0.5f;
            float w0 = ((b.pos.x - px) * (c.pos.y - py) - (b.pos.y - py) * (c.pos.x - px)) / area;
            float w1 = ((c.pos.x - px) * (a.pos.y - py) - (c.pos.y - py) * (a.pos.x - px)) / area;
            float w2 = 1 - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) {
                continue;
            }
            float t[4];
            Sample(w0 * a.uv.x + w1 * b.uv.x + w2 * c.uv.x, w0 * a.uv.y + w1 * b.uv.y + w2 * c.uv.y, t);
            float col[4];
            for (int i = 0; i < 4; ++i) {
                float ca = ((a.col >> (8 * i)) & 0xFF) / 255.f, cb = ((b.col >> (8 * i)) & 0xFF) / 255.f,
                      cc = ((c.col >> (8 * i)) & 0xFF) / 255.f;
                col[i] = (w0 * ca + w1 * cb + w2 * cc) * t[i];
            }
            float* p = &fb[(static_cast<size_t>(y) * W + x) * 3];
            for (int i = 0; i < 3; ++i) {
                p[i] = p[i] * (1 - col[3]) + col[i] * col[3];
            }
        }
    }
}

void Render(ImDrawData* dd) {
    for (int n = 0; n < dd->CmdListsCount; ++n) {
        const ImDrawList* cl = dd->CmdLists[n];
        for (const ImDrawCmd& cmd : cl->CmdBuffer) {
            if (cmd.UserCallback) {
                continue;
            }
            intptr_t id = reinterpret_cast<intptr_t>(cmd.GetTexID());
            auto it = images.find(id);
            cur = it != images.end() ? &it->second : &font;
            for (unsigned i = 0; i < cmd.ElemCount; i += 3) {
                const ImDrawIdx* idx = &cl->IdxBuffer[cmd.IdxOffset + i];
                Raster(cl->VtxBuffer[cmd.VtxOffset + idx[0]], cl->VtxBuffer[cmd.VtxOffset + idx[1]],
                       cl->VtxBuffer[cmd.VtxOffset + idx[2]], cmd.ClipRect);
            }
        }
    }
}

void Save(const std::string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (float v : fb) {
        fputc(static_cast<int>(std::min(255.f, std::max(0.f, v * 255.f + 0.5f))), f);
    }
    fclose(f);
}

ImVec2 mouse(-100, -100);
bool clickNext = false;

void Frames(int n) {
    ImGuiIO& io = ImGui::GetIO();
    for (int i = 0; i < n; ++i) {
        io.DeltaTime = 1.0f / 60.0f;
        io.AddMousePosEvent(mouse.x, mouse.y);
        if (clickNext && i == 0) {
            io.AddMouseButtonEvent(0, true);
        } else if (clickNext && i == 1) {
            io.AddMouseButtonEvent(0, false);
            clickNext = false;
        }
        ImGui::NewFrame();
        devtools::Frame();
        ImGui::Render();
    }
}

void Key(ImGuiKey key) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(key, true);
    Frames(1);
    io.AddKeyEvent(key, false);
    Frames(1);
}

void Shot(const std::string& path, const std::vector<unsigned char>& bg) {
    Clear(bg);
    Render(ImGui::GetDrawData());
    Save(path);
}

// O retangulo de um card na grade (a mesma conta do TabModels).
ImVec4 CardRect(int filteredIndex) {
    ImGuiWindow* win = ImGui::FindWindowByName("##trokDevTools");
    ImVec2 pos = win ? win->Pos : ImVec2(0, 0);
    ImGuiIO& io = ImGui::GetIO();
    float width = std::floor(std::max(760.0f, std::min(io.DisplaySize.x * 0.84f, 1560.0f)));
    float top = pos.y + tui::HeaderHeight() + tui::TabsHeight() + 14 * tui::u;
    float x = pos.x + 18 * tui::u, w = width - 36 * tui::u;
    float statusY = top + 32 * tui::u + 10 * tui::u;
    float gridY = statusY + tui::fonts.desc->FontSize + 10 * tui::u;
    float avail = w - 6 * tui::u, gap = 10 * tui::u, targetW = 176 * tui::u;
    int ncols = std::max(1, static_cast<int>((avail + gap) / (targetW + gap)));
    float cw = std::floor((avail - gap * (ncols - 1)) / ncols), ch = cw + 44 * tui::u;
    int r = filteredIndex / ncols, c = filteredIndex % ncols;
    float cx = x + c * (cw + gap), cy = gridY + r * (ch + gap);
    return ImVec4(cx, cy, cx + cw, cy + ch);
}

int FilteredIndexOf(int id) {
    const auto& all = catalog::All();
    for (int i = 0; i < static_cast<int>(devtools::S.filtered.size()); ++i) {
        if (all[devtools::S.filtered[i]].id == id) {
            return i;
        }
    }
    return -1;
}

int Lit(int x0, int y0, int x1, int y1, float floor) {
    int n = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const float* p = &fb[(static_cast<size_t>(y) * W + x) * 3];
            n += p[0] + p[1] + p[2] > floor * 3;
        }
    }
    return n;
}

} // namespace concept

int main(int argc, char** argv) {
    using namespace concept;
    std::string in = argc > 1 ? argv[1] : ".", out = argc > 2 ? argv[2] : in;
    LoadData(in);
    scene = LoadRgb(in + "/cena.rgb");
    sceneSpawned = LoadRgb(in + "/cena_criado.rgb");
    Check(catalog::all.size() > 10000 && meshes.size() > 50, "conceito.txt lido (catalogo e malhas)");

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(W), static_cast<float>(H));
    io.IniFilename = nullptr;
    tui::BuildFonts(static_cast<float>(H), "");
    io.Fonts->Build();
    unsigned char* px;
    io.Fonts->GetTexDataAsRGBA32(&px, &font.w, &font.h);
    font.rgba.assign(px, px + static_cast<size_t>(font.w) * font.h * 4);
    io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(FONT));
    devtools::Init((out + "/Trok DevTools.ini").c_str(), "");

    // 1. Aba Modelos, todos os modelos, o mouse em cima do card do Infernus
    devtools::Toggle();
    Frames(30);
    io.MouseDrawCursor = true;
    ImVec4 r = CardRect(FilteredIndexOf(411));
    mouse = ImVec2((r.x + r.z) * 0.5f, r.y + (r.w - r.y) * 0.35f);
    Frames(50);
    Shot(out + "/01-modelos.ppm", scene);
    Check(Lit(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.z), static_cast<int>(r.w), 0.5f) > 200,
          "card do Infernus com a miniatura");

    // 2. So veiculos
    io.MouseDrawCursor = false;
    mouse = ImVec2(-100, -100);
    devtools::S.kind = catalog::K_VEHICLE;
    devtools::S.filterDirty = true;
    Frames(10);
    Shot(out + "/02-veiculos.ppm", scene);
    Check(devtools::S.filtered.size() == 20, "filtro Veiculos: os 20 carros do conceito");

    // 3. Card do Infernus (substituido, com o visualizador e as texturas)
    devtools::OpenCard(411);
    Frames(20);
    Shot(out + "/03-card-infernus.ppm", scene);
    Check(devtools::S.cardOpen && core::OpenMesh() != nullptr, "card do Infernus aberto");

    // 3b. A textura dos farois em destaque: quem mais usa (os carros, pela vehicle.txd)
    devtools::S.texName = "vehiclelights128";
    Frames(10);
    Shot(out + "/10-textura-quem-usa.ppm", scene);
    Check(devtools::TexIndex(core::OpenMesh()) >= 0 && users.users.size() > 10, "textura dos farois: a lista dos carros");

    // 3c. Fundo verde (chroma key) no visualizador
    devtools::S.texName.clear();
    devtools::SetBackground(IM_COL32(0, 177, 64, 255));
    Frames(5);
    Shot(out + "/11-fundo-verde.ppm", scene);

    // 3d. O botao Fundo (depois da pilula Girar) abre o seletor de cor da casa, para cima
    int girarEnd = 0;
    for (int x = 640; x > 300 && !girarEnd; --x) {
        const float* p = &fb[(static_cast<size_t>(916) * W + x) * 3];
        girarEnd = p[0] > 0.84f && p[1] > 0.84f ? x : 0;
    }
    mouse = ImVec2(girarEnd + 19.0f, 916.0f);
    io.MouseDrawCursor = true;
    clickNext = true;
    Frames(8);
    Shot(out + "/12-seletor-de-cor.ppm", scene);
    Check(girarEnd > 0 && ImGui::GetCurrentContext()->OpenPopupStack.Size == 2, "botao Fundo: o seletor de cor abre sobre o card");
    Key(ImGuiKey_Escape);
    Frames(3);
    Check(devtools::S.cardOpen && ImGui::GetCurrentContext()->OpenPopupStack.Size == 1, "Esc fecha so o seletor");
    io.MouseDrawCursor = false;
    mouse = ImVec2(-100, -100);
    devtools::SetBackground(devtools::BACKGROUND_DEFAULT);

    // 4. Criar no mundo: o aviso e o carro na frente do CJ, com o menu fechado
    core::Spawn(411);
    Frames(3);
    devtools::Toggle();
    Frames(20);
    Shot(out + "/04-criado-no-mundo.ppm", sceneSpawned);
    Check(core::SpawnedCount() == 1, "Criar no mundo conta o criado");
    Frames(600); // o aviso some antes das proximas imagens

    // 5. Card da palmeira com a colisao ligada
    devtools::Toggle();
    devtools::S.kind = -1;
    devtools::S.filterDirty = true;
    Frames(10);
    devtools::S.view.collision = true;
    devtools::OpenCard(621);
    Frames(20);
    Shot(out + "/05-card-palmeira-colisao.ppm", scene);

    // 6. A folha da palmeira em destaque (o alfa sobre o xadrez)
    devtools::S.texName = "palm_leaves";
    Frames(5);
    Shot(out + "/06-textura.ppm", scene);
    Check(!core::openMesh.textures.empty() && core::openMesh.textures[0].alpha, "a folha da palmeira tem alfa");
    devtools::S.view.collision = false;

    // 7. Info ao apontar, com o menu fechado: o sultan parado na guia
    Key(ImGuiKey_Escape);
    Frames(3);
    devtools::Toggle();
    devtools::S.inspect = true;
    core::SetInspect(true);
    Frames(10);
    Shot(out + "/07-info-ao-apontar.ppm", scene);
    float cx, cy;
    gta::Project(target.hit, &cx, &cy);
    Check(std::fabs(cx - W * 0.5f) < 120 && std::fabs(cy - H * 0.5f) < 120, "o sultan esta no meio da tela");
    core::SetInspect(false);
    devtools::S.inspect = false;

    // 8. Aba Geral
    devtools::Toggle();
    devtools::S.tab = devtools::TAB_GENERAL;
    Frames(10);
    Shot(out + "/08-geral.ppm", scene);

    ImGui::DestroyContext();
    printf(failures ? "FALHOU\n" : "PASSOU\n");
    return failures ? 1 : 0;
}

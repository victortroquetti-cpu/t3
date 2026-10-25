// Trok DevTools (.asi) -- Victor_Trok
// Pipeline fixo do Direct3D 9 (sem shader): uma luz direcional de cima e da esquerda da camera, a cor do material como
// difusa e ambiente, a textura do RenderWare modulando. Sem descarte de faces (os modelos do GTA tem faces dos dois
// lados). Coordenadas do GTA (Z para cima, mao direita): visao e projecao "RH", como o D3DX faria.

#include "render.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

void DevToolsLog(const char* fmt, ...);

namespace render {
namespace {

constexpr int ATLAS = 2048, CELL = 128, PER_ROW = ATLAS / CELL, CELLS = PER_ROW * PER_ROW, STAGE = 256;
constexpr DWORD MESH_FVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;
constexpr DWORD LINE_FVF = D3DFVF_XYZ | D3DFVF_DIFFUSE;
constexpr DWORD THUMB_BG = 0xFF141414;
constexpr float PI = 3.14159265f;

struct LineVertex {
    float x, y, z;
    DWORD color;
};

IDirect3DDevice9* g_dev = nullptr;
IDirect3DTexture9* g_atlas = nullptr;
IDirect3DTexture9* g_stage = nullptr;
IDirect3DSurface9* g_stageDepth = nullptr;
IDirect3DTexture9* g_viewer = nullptr;
IDirect3DSurface9* g_viewerDepth = nullptr;
int g_viewerSize = 0;
bool g_failed = false; // os alvos nao puderam ser criados: nao tenta todo quadro

struct Cell {
    int id = -1;
    int used = -1;
};
Cell g_cells[CELLS];
std::unordered_map<int, int> g_cellOf;

template <class T>
void SafeRelease(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

struct V3 {
    float x, y, z;
};
V3 Sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 Add(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 Mul(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 Cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V3 Norm(V3 a) {
    float l = std::sqrt(Dot(a, a));
    return l > 1e-8f ? Mul(a, 1.0f / l) : V3{0, 0, 1};
}

D3DMATRIX Identity() {
    D3DMATRIX m = {};
    m._11 = m._22 = m._33 = m._44 = 1.0f;
    return m;
}

D3DMATRIX LookAtRH(V3 eye, V3 at, V3 up) {
    V3 z = Norm(Sub(eye, at));
    V3 x = Norm(Cross(up, z));
    V3 y = Cross(z, x);
    D3DMATRIX m = Identity();
    m._11 = x.x; m._12 = y.x; m._13 = z.x;
    m._21 = x.y; m._22 = y.y; m._23 = z.y;
    m._31 = x.z; m._32 = y.z; m._33 = z.z;
    m._41 = -Dot(x, eye); m._42 = -Dot(y, eye); m._43 = -Dot(z, eye);
    return m;
}

D3DMATRIX PerspectiveRH(float fovY, float aspect, float zn, float zf) {
    float ys = 1.0f / std::tan(fovY * 0.5f);
    float xs = ys / aspect;
    D3DMATRIX m = {};
    m._11 = xs;
    m._22 = ys;
    m._33 = zf / (zn - zf);
    m._34 = -1.0f;
    m._43 = zn * zf / (zn - zf);
    return m;
}

// Salva o estado do device inteiro (estados, texturas, transformacoes, sombreadores) e os alvos.
class Saved {
public:
    bool Save() {
        if (FAILED(g_dev->CreateStateBlock(D3DSBT_ALL, &block_)) || !block_) {
            return false;
        }
        g_dev->GetRenderTarget(0, &rt_);
        g_dev->GetDepthStencilSurface(&ds_);
        g_dev->GetViewport(&vp_);
        return true;
    }
    ~Saved() {
        if (!block_) {
            return;
        }
        g_dev->SetRenderTarget(0, rt_);
        g_dev->SetDepthStencilSurface(ds_);
        g_dev->SetViewport(&vp_);
        block_->Apply();
        SafeRelease(block_);
        SafeRelease(rt_);
        SafeRelease(ds_);
    }

private:
    IDirect3DStateBlock9* block_ = nullptr;
    IDirect3DSurface9* rt_ = nullptr;
    IDirect3DSurface9* ds_ = nullptr;
    D3DVIEWPORT9 vp_ = {};
};

bool CreateTarget(int size, IDirect3DTexture9** tex, IDirect3DSurface9** depth) {
    HRESULT hr = g_dev->CreateTexture(size, size, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, tex,
                                      nullptr);
    if (FAILED(hr)) {
        DevToolsLog("render: alvo de %d px recusado (0x%08X)", size, static_cast<unsigned>(hr));
        return false;
    }
    if (depth) {
        hr = g_dev->CreateDepthStencilSurface(size, size, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, depth, nullptr);
        if (FAILED(hr)) {
            hr = g_dev->CreateDepthStencilSurface(size, size, D3DFMT_D16, D3DMULTISAMPLE_NONE, 0, TRUE, depth, nullptr);
        }
        if (FAILED(hr)) {
            DevToolsLog("render: profundidade de %d px recusada (0x%08X)", size, static_cast<unsigned>(hr));
            SafeRelease(*tex);
            return false;
        }
    }
    return true;
}

bool EnsureThumbTargets() {
    if (g_atlas && g_stage) {
        return true;
    }
    if (g_failed || !g_dev) {
        return false;
    }
    if (!CreateTarget(ATLAS, &g_atlas, nullptr) || !CreateTarget(STAGE, &g_stage, &g_stageDepth)) {
        SafeRelease(g_atlas);
        SafeRelease(g_stage);
        SafeRelease(g_stageDepth);
        g_failed = true;
        return false;
    }
    // O atlas comeca com o fundo dos cards.
    IDirect3DSurface9* surface = nullptr;
    if (SUCCEEDED(g_atlas->GetSurfaceLevel(0, &surface))) {
        g_dev->ColorFill(surface, nullptr, THUMB_BG);
        surface->Release();
    }
    return true;
}

void SetCommonStates() {
    IDirect3DDevice9* d = g_dev;
    d->SetVertexShader(nullptr);
    d->SetPixelShader(nullptr);
    d->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    d->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    d->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    d->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    d->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    d->SetRenderState(D3DRS_LIGHTING, TRUE);
    d->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
    d->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    d->SetRenderState(D3DRS_FOGENABLE, FALSE);
    d->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    d->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    d->SetRenderState(D3DRS_CLIPPING, TRUE);
    d->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    d->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
    d->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
    d->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    d->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    d->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    d->SetRenderState(D3DRS_ALPHAREF, 8);
    d->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    d->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    d->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
    d->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
    d->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
    d->SetRenderState(D3DRS_COLORVERTEX, FALSE);
    d->SetRenderState(D3DRS_AMBIENT, 0xFF5C5C5C);
    d->SetRenderState(D3DRS_DEPTHBIAS, 0);
    d->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
    d->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);
    d->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, FALSE);
    d->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    d->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    d->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    d->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    d->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    d->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    d->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    d->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    d->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    d->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 0);
    for (DWORD i = 1; i < 8; ++i) {
        d->SetTexture(i, nullptr);
    }
    D3DMATRIX world = Identity();
    d->SetTransform(D3DTS_WORLD, &world);
}

void StageTexture(bool texture) {
    IDirect3DDevice9* d = g_dev;
    DWORD source = texture ? D3DTA_TEXTURE : D3DTA_DIFFUSE;
    d->SetTextureStageState(0, D3DTSS_COLOROP, texture ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
    d->SetTextureStageState(0, D3DTSS_COLORARG1, source);
    d->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    d->SetTextureStageState(0, D3DTSS_ALPHAOP, texture ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
    d->SetTextureStageState(0, D3DTSS_ALPHAARG1, source);
    d->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
}

void StageFactor(DWORD color) {
    IDirect3DDevice9* d = g_dev;
    d->SetRenderState(D3DRS_TEXTUREFACTOR, color);
    d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
    d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
}

D3DCOLORVALUE Color(DWORD argb) {
    D3DCOLORVALUE c;
    c.a = ((argb >> 24) & 0xFF) / 255.0f;
    c.r = ((argb >> 16) & 0xFF) / 255.0f;
    c.g = ((argb >> 8) & 0xFF) / 255.0f;
    c.b = (argb & 0xFF) / 255.0f;
    return c;
}

void DrawLines(const std::vector<LineVertex>& lines, bool depth) {
    if (lines.size() < 2) {
        return;
    }
    IDirect3DDevice9* d = g_dev;
    d->SetRenderState(D3DRS_LIGHTING, FALSE);
    d->SetRenderState(D3DRS_ZENABLE, depth ? D3DZB_TRUE : D3DZB_FALSE);
    d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    d->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    d->SetTexture(0, nullptr);
    d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    d->SetFVF(LINE_FVF);
    const UINT MAX_PER_CALL = 30000;
    for (size_t i = 0; i + 1 < lines.size(); i += MAX_PER_CALL * 2) {
        UINT count = static_cast<UINT>(std::min<size_t>(MAX_PER_CALL, (lines.size() - i) / 2));
        d->DrawPrimitiveUP(D3DPT_LINELIST, count, &lines[i], sizeof(LineVertex));
    }
}

// Desenha a malha com a camera da vista no alvo atual (ja limpo).
void DrawMesh(const mesh::Mesh& m, const View& v, float aspect, bool viewer) {
    IDirect3DDevice9* d = g_dev;
    V3 mn = {m.min.x, m.min.y, m.min.z}, mx = {m.max.x, m.max.y, m.max.z};
    V3 center = Mul(Add(mn, mx), 0.5f);
    float radius = std::max(0.05f, std::sqrt(Dot(Sub(mx, mn), Sub(mx, mn))) * 0.5f);
    float fov = (viewer ? 34.0f : 28.0f) * PI / 180.0f;
    float fit = std::min(fov, 2.0f * std::atan(std::tan(fov * 0.5f) * aspect));
    float dist = radius / std::sin(fit * 0.5f) * 1.04f / std::max(0.2f, v.zoom);
    float yaw = v.yaw * PI / 180.0f;
    float pitch = std::max(-85.0f, std::min(85.0f, v.pitch)) * PI / 180.0f;
    V3 dir = {std::sin(yaw) * std::cos(pitch), std::cos(yaw) * std::cos(pitch), std::sin(pitch)};
    V3 eye = Add(center, Mul(dir, dist));
    V3 up = {0, 0, 1};
    D3DMATRIX view = LookAtRH(eye, center, up);
    float zn = std::max(dist * 0.01f, std::max(0.01f, dist - radius * 1.6f));
    float zf = dist + radius * 3.0f + 1.0f;
    D3DMATRIX proj = PerspectiveRH(fov, aspect, zn, zf);
    d->SetTransform(D3DTS_VIEW, &view);
    d->SetTransform(D3DTS_PROJECTION, &proj);

    // Luz: de cima e da esquerda de quem olha.
    V3 right = Norm(Cross(Mul(dir, -1.0f), up));
    V3 lightPos = Add(Add(eye, Mul(right, -dist * 0.7f)), Mul(up, dist * 0.9f));
    V3 lightDir = Norm(Sub(center, lightPos));
    D3DLIGHT9 light = {};
    light.Type = D3DLIGHT_DIRECTIONAL;
    light.Diffuse = {0.82f, 0.82f, 0.82f, 1.0f};
    light.Direction = {lightDir.x, lightDir.y, lightDir.z};
    d->SetLight(0, &light);
    d->LightEnable(0, TRUE);
    for (DWORD i = 1; i < 4; ++i) {
        d->LightEnable(i, FALSE);
    }

    // Grade no chao (so no visualizador): da a escala.
    if (viewer && v.grid) {
        float step = radius < 3 ? 0.5f : radius < 12 ? 1.0f : radius < 60 ? 5.0f : 20.0f;
        int n = static_cast<int>(std::ceil(radius * 1.6f / step));
        std::vector<LineVertex> grid;
        float z = mn.z;
        float cx = std::round(center.x / step) * step, cy = std::round(center.y / step) * step;
        for (int i = -n; i <= n; ++i) {
            DWORD c = i == 0 ? 0x30FFFFFF : 0x16FFFFFF;
            grid.push_back({cx + i * step, cy - n * step, z, c});
            grid.push_back({cx + i * step, cy + n * step, z, c});
            grid.push_back({cx - n * step, cy + i * step, z, c});
            grid.push_back({cx + n * step, cy + i * step, z, c});
        }
        DrawLines(grid, true);
        d->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        d->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        d->SetRenderState(D3DRS_LIGHTING, TRUE);
        d->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    }

    d->SetFVF(MESH_FVF);
    // Duas passadas: os materiais opacos e depois os com transparencia.
    for (int pass = 0; pass < 2; ++pass) {
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, pass == 1);
        d->SetRenderState(D3DRS_ZWRITEENABLE, pass == 0);
        for (const mesh::Batch& b : m.batches) {
            for (const mesh::Part& p : b.parts) {
                if (p.alpha != (pass == 1)) {
                    continue;
                }
                D3DMATERIAL9 mat = {};
                mat.Diffuse = Color(p.color);
                mat.Ambient = Color(p.color);
                d->SetMaterial(&mat);
                bool textured = v.textures && p.texture;
                d->SetTexture(0, textured ? static_cast<IDirect3DBaseTexture9*>(p.texture) : nullptr);
                StageTexture(textured);
                d->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(b.verts.size()), p.count / 3,
                                          &b.indices[p.first], D3DFMT_INDEX16, b.verts.data(), sizeof(mesh::Vertex));
            }
        }
    }

    if (v.wire) {
        d->SetRenderState(D3DRS_LIGHTING, FALSE);
        d->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
        d->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        d->SetTexture(0, nullptr);
        StageFactor(0x70FFFFFF);
        for (const mesh::Batch& b : m.batches) {
            for (const mesh::Part& p : b.parts) {
                d->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(b.verts.size()), p.count / 3,
                                          &b.indices[p.first], D3DFMT_INDEX16, b.verts.data(), sizeof(mesh::Vertex));
            }
        }
        d->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    }

    if (v.collision && !m.colLines.empty()) {
        std::vector<LineVertex> lines;
        lines.reserve(m.colLines.size());
        for (const gta::Vec3& p : m.colLines) {
            lines.push_back({p.x, p.y, p.z, 0xD0FF9A3C});
        }
        DrawLines(lines, false);
    }
}

int CellFor(int id, int frame) {
    auto it = g_cellOf.find(id);
    if (it != g_cellOf.end()) {
        return it->second;
    }
    int best = -1;
    for (int i = 0; i < CELLS; ++i) {
        if (g_cells[i].id < 0) {
            best = i;
            break;
        }
        if (g_cells[i].used != frame && (best < 0 || g_cells[i].used < g_cells[best].used)) {
            best = i;
        }
    }
    if (best < 0) {
        return -1; // tudo em uso neste quadro
    }
    if (g_cells[best].id >= 0) {
        g_cellOf.erase(g_cells[best].id);
    }
    g_cells[best].id = id;
    g_cellOf[id] = best;
    return best;
}

} // namespace

void SetDevice(IDirect3DDevice9* device) {
    if (device != g_dev) {
        DeviceLost();
        g_dev = device;
        g_failed = false;
    }
}

void DeviceLost() {
    SafeRelease(g_atlas);
    SafeRelease(g_stage);
    SafeRelease(g_stageDepth);
    SafeRelease(g_viewer);
    SafeRelease(g_viewerDepth);
    g_viewerSize = 0;
    ForgetThumbs();
    g_failed = false;
}

bool HasThumb(int id) {
    return g_cellOf.count(id) != 0;
}

bool Thumb(int id, ImTextureID* tex, ImVec2* uv0, ImVec2* uv1) {
    auto it = g_cellOf.find(id);
    if (it == g_cellOf.end() || !g_atlas) {
        return false;
    }
    int cell = it->second;
    g_cells[cell].used = ImGui::GetFrameCount();
    float x = static_cast<float>(cell % PER_ROW * CELL), y = static_cast<float>(cell / PER_ROW * CELL);
    *tex = static_cast<ImTextureID>(g_atlas);
    *uv0 = ImVec2(x / ATLAS, y / ATLAS);
    *uv1 = ImVec2((x + CELL) / ATLAS, (y + CELL) / ATLAS);
    return true;
}

bool DrawThumb(const mesh::Mesh& m) {
    if (!g_dev || !m.ok || !EnsureThumbTargets()) {
        return false;
    }
    int cell = CellFor(m.id, ImGui::GetFrameCount());
    if (cell < 0) {
        return false;
    }
    Saved saved;
    if (!saved.Save()) {
        return false;
    }
    IDirect3DSurface9* stage = nullptr;
    IDirect3DSurface9* atlas = nullptr;
    bool ok = false;
    if (SUCCEEDED(g_stage->GetSurfaceLevel(0, &stage)) && SUCCEEDED(g_atlas->GetSurfaceLevel(0, &atlas))) {
        g_dev->SetRenderTarget(0, stage);
        g_dev->SetDepthStencilSurface(g_stageDepth);
        D3DVIEWPORT9 vp = {0, 0, STAGE, STAGE, 0.0f, 1.0f};
        g_dev->SetViewport(&vp);
        g_dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, THUMB_BG, 1.0f, 0);
        if (SUCCEEDED(g_dev->BeginScene())) {
            SetCommonStates();
            View v;
            v.grid = false;
            DrawMesh(m, v, 1.0f, false);
            g_dev->EndScene();
            int x = cell % PER_ROW * CELL, y = cell / PER_ROW * CELL;
            RECT dst = {x, y, x + CELL, y + CELL};
            ok = SUCCEEDED(g_dev->StretchRect(stage, nullptr, atlas, &dst, D3DTEXF_LINEAR));
        }
    }
    SafeRelease(stage);
    SafeRelease(atlas);
    if (!ok) {
        g_cellOf.erase(m.id);
        g_cells[cell] = Cell();
    } else {
        g_cells[cell].used = ImGui::GetFrameCount();
    }
    return ok;
}

void ForgetThumbs() {
    g_cellOf.clear();
    for (Cell& c : g_cells) {
        c = Cell();
    }
}

int ThumbCount() {
    return static_cast<int>(g_cellOf.size());
}

ImTextureID DrawViewer(const mesh::Mesh& m, const View& v, int size) {
    if (!g_dev || g_failed) {
        return nullptr;
    }
    size = std::max(128, std::min(2048, size));
    if (g_viewer && g_viewerSize != size) {
        SafeRelease(g_viewer);
        SafeRelease(g_viewerDepth);
    }
    if (!g_viewer) {
        if (!CreateTarget(size, &g_viewer, &g_viewerDepth)) {
            g_failed = true;
            return nullptr;
        }
        g_viewerSize = size;
    }
    Saved saved;
    if (!saved.Save()) {
        return nullptr;
    }
    IDirect3DSurface9* surface = nullptr;
    if (FAILED(g_viewer->GetSurfaceLevel(0, &surface))) {
        return nullptr;
    }
    g_dev->SetRenderTarget(0, surface);
    g_dev->SetDepthStencilSurface(g_viewerDepth);
    D3DVIEWPORT9 vp = {0, 0, static_cast<DWORD>(size), static_cast<DWORD>(size), 0.0f, 1.0f};
    g_dev->SetViewport(&vp);
    g_dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, v.background, 1.0f, 0);
    if (m.ok && SUCCEEDED(g_dev->BeginScene())) {
        SetCommonStates();
        DrawMesh(m, v, 1.0f, true);
        g_dev->EndScene();
    }
    surface->Release();
    return static_cast<ImTextureID>(g_viewer);
}

bool Describe(void* d3dTexture, TexInfo* out) {
    if (!d3dTexture) {
        return false;
    }
    IDirect3DTexture9* t = static_cast<IDirect3DTexture9*>(d3dTexture);
    D3DSURFACE_DESC desc;
    if (FAILED(t->GetLevelDesc(0, &desc))) {
        return false;
    }
    out->width = static_cast<int>(desc.Width);
    out->height = static_cast<int>(desc.Height);
    out->levels = static_cast<int>(t->GetLevelCount());
    switch (desc.Format) {
    case D3DFMT_DXT1: out->format = "DXT1"; break;
    case D3DFMT_DXT2: out->format = "DXT2"; break;
    case D3DFMT_DXT3: out->format = "DXT3"; break;
    case D3DFMT_DXT4: out->format = "DXT4"; break;
    case D3DFMT_DXT5: out->format = "DXT5"; break;
    case D3DFMT_A8R8G8B8: out->format = "8888"; break;
    case D3DFMT_X8R8G8B8: out->format = "888"; break;
    case D3DFMT_R5G6B5: out->format = "565"; break;
    case D3DFMT_A1R5G5B5: out->format = "1555"; break;
    case D3DFMT_X1R5G5B5: out->format = "555"; break;
    case D3DFMT_A4R4G4B4: out->format = "4444"; break;
    case D3DFMT_P8: out->format = "PAL8"; break;
    case D3DFMT_L8: out->format = "LUM8"; break;
    case D3DFMT_A8L8: out->format = "LUM8A8"; break;
    default: out->format = "outro"; break;
    }
    return true;
}

} // namespace render

// Trok DevTools (.asi) -- Victor_Trok
// Leitura das estruturas do RenderWare 3.6 do GTA SA (rpworld.h / rwcore.h do gta-reversed):
//   RwObject: tipo +0 (1 atomico, 2 clump), flags +2 (0x04 = desenhar), pai +4 (o RwFrame).
//   RpClump: lista de atomicos +0x08 (o link do atomico fica em +0x40 dele).
//   RpAtomic: geometria +0x18.
//   RwFrame: matriz local +0x10 (direita, cima, frente, posicao, 16 bytes cada); o nome (plugin NodeName) fica no
//   deslocamento guardado em 0xC87C5C.
//   RpGeometry: triangulos +0x10, vertices +0x14, materiais (ponteiros +0x20, quantos +0x24), triangulos +0x2C (8 bytes:
//   3 vertices e o material), UV +0x34, morph target +0x5C (vertices +0x14, normais +0x18).
//   RpMaterial: textura +0, cor RGBA +4. RwTexture: raster +0, nome +0x10, mascara +0x30, link na TXD +0x08.
//   RwRaster: largura +0xC, altura +0x10, profundidade +0x14; a extensao do D3D9 fica no deslocamento guardado em
//   0xB4E9E0 (textura do Direct3D +0, alfa +8). RwTexDictionary: lista de texturas +0x08.
//   Pele (rpskin do gta-reversed): RpSkinGeometryGetSkin 0x7C7550, RpSkinGetNumBones 0x7C77E0, RpSkinGetVertexBoneWeights
//   0x7C77F0 (4 floats por vertice), RpSkinGetVertexBoneIndices 0x7C7800 (4 bytes por vertice),
//   RpSkinGetSkinToBoneMatrices 0x7C7810, RpSkinAtomicGetHAnimHierarchy 0x7C7540. RpHAnimHierarchy: quantos nos +4,
//   nos +0x10 (16 bytes: id, indice, flags (1 = volta ao pai guardado, 2 = guarda o pai), frame).
//   CColModel: caixa +0 (min, max), dados +0x2C. CCollisionData: esferas +0, caixas +2, triangulos +4 (quantidades);
//   esferas +8 (0x14 cada), caixas +0xC (0x1C cada), vertices +0x14 (int16 x3 / 128), triangulos +0x18 (8 bytes).
// Os veiculos: as pecas "_vlo" (a versao de longe) ficam de fora, as rodas da esquerda giram 180 graus (como o jogo faz)
// e as cores de marcacao do material (verde = cor 1, magenta = cor 2, as dos farois) viram cores neutras.

#include "mesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>

void DevToolsLog(const char* fmt, ...);

namespace mesh {
namespace {

using gta::At;
using gta::Ptr;
using gta::Vec3;

constexpr DWORD NODE_NAME_OFFSET = 0xC87C5C, RASTER_EXT_OFFSET = 0xB4E9E0;
constexpr BYTE RW_ATOMIC = 1, RW_CLUMP = 2, ATOMIC_RENDER = 0x04;
constexpr int MAX_VERTS = 65535, MAX_TRIS = 400000, MAX_ATOMICS = 512, MAX_COL_TRIS = 40000;
constexpr DWORD FN_GEOMETRY_SKIN = 0x7C7550, FN_SKIN_BONES = 0x7C77E0, FN_SKIN_WEIGHTS = 0x7C77F0,
                FN_SKIN_INDICES = 0x7C7800, FN_SKIN_MATRICES = 0x7C7810, FN_ATOMIC_HIERARCHY = 0x7C7540;
constexpr int MAX_BONES = 64;
typedef DWORD(__cdecl* PtrFn)(DWORD);

struct Xform {
    Vec3 r = {1, 0, 0}, u = {0, 1, 0}, a = {0, 0, 1}, p = {0, 0, 0};
    Vec3 Point(const Vec3& v) const {
        return {r.x * v.x + u.x * v.y + a.x * v.z + p.x, r.y * v.x + u.y * v.y + a.y * v.z + p.y,
                r.z * v.x + u.z * v.y + a.z * v.z + p.z};
    }
    Vec3 Dir(const Vec3& v) const {
        return {r.x * v.x + u.x * v.y + a.x * v.z, r.y * v.x + u.y * v.y + a.y * v.z, r.z * v.x + u.z * v.y + a.z * v.z};
    }
};

// child primeiro, depois parent.
Xform Then(const Xform& child, const Xform& parent) {
    Xform out;
    out.r = parent.Dir(child.r);
    out.u = parent.Dir(child.u);
    out.a = parent.Dir(child.a);
    out.p = parent.Point(child.p);
    return out;
}

bool Finite(const Vec3& v) {
    auto ok = [](float f) { return f == f && f > -1e7f && f < 1e7f; };
    return ok(v.x) && ok(v.y) && ok(v.z);
}

Xform FrameLocal(DWORD frame) {
    Xform x;
    x.r = At<Vec3>(frame, 0x10);
    x.u = At<Vec3>(frame, 0x20);
    x.a = At<Vec3>(frame, 0x30);
    x.p = At<Vec3>(frame, 0x40);
    if (!Finite(x.r) || !Finite(x.u) || !Finite(x.a) || !Finite(x.p)) {
        return Xform();
    }
    return x;
}

// Do frame do atomico ate a raiz.
Xform FrameToModel(DWORD frame) {
    Xform total;
    for (int depth = 0; Ptr(frame) && depth < 64; ++depth) {
        if (!gta::Readable(reinterpret_cast<void*>(frame), 0x50)) {
            break;
        }
        total = Then(total, FrameLocal(frame));
        frame = At<DWORD>(frame, 0x04);
    }
    return total;
}

std::string FrameName(DWORD frame) {
    int offset = At<int>(NODE_NAME_OFFSET, 0);
    if (!Ptr(frame) || offset <= 0 || offset > 0x1000 ||
        !gta::Readable(reinterpret_cast<void*>(frame + offset), 24)) {
        return std::string();
    }
    const char* name = reinterpret_cast<const char*>(frame + offset);
    return std::string(name, strnlen(name, 24));
}

std::string Lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// RwRGBA (r, g, b, a na memoria) -> ARGB
DWORD Argb(DWORD rgba) {
    DWORD r = rgba & 0xFF, g = (rgba >> 8) & 0xFF, b = (rgba >> 16) & 0xFF, a = rgba >> 24;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// As cores que o jogo troca na hora de desenhar um veiculo.
DWORD VehicleColor(DWORD argb) {
    DWORD rgb = argb & 0xFFFFFF, a = argb & 0xFF000000;
    switch (rgb) {
    case 0x3CFF00: return a | 0xD7D7DC; // cor 1
    case 0xFF00AF: return a | 0x5A5A5F; // cor 2
    case 0x00FFFF: return a | 0xA0A0A5; // cor 3
    case 0xFF00FF: return a | 0x3C3C41; // cor 4
    case 0xFFAF00: // farol esquerdo
    case 0x00FFC8: return a | 0xE6E6DC; // farol direito
    case 0xB9FF00: // lanterna esquerda
    case 0xFF3C00: return a | 0xAA1E1E; // lanterna direita
    default: return argb;
    }
}

struct TexRef {
    void* d3d = nullptr;
    bool alpha = false;
};

TexRef ReadTexture(DWORD tex, Mesh* out, bool used) {
    TexRef ref;
    if (!Ptr(tex) || !gta::Readable(reinterpret_cast<void*>(tex), 0x58)) {
        return ref;
    }
    for (Texture& t : out->textures) {
        if (t.name == std::string(reinterpret_cast<const char*>(tex + 0x10), strnlen(reinterpret_cast<const char*>(tex + 0x10), 32))) {
            t.used = t.used || used;
            ref.d3d = t.d3d;
            ref.alpha = t.alpha;
            return ref;
        }
    }
    Texture t;
    t.name.assign(reinterpret_cast<const char*>(tex + 0x10), strnlen(reinterpret_cast<const char*>(tex + 0x10), 32));
    t.mask.assign(reinterpret_cast<const char*>(tex + 0x30), strnlen(reinterpret_cast<const char*>(tex + 0x30), 32));
    t.used = used;
    DWORD raster = At<DWORD>(tex, 0);
    int ext = At<int>(RASTER_EXT_OFFSET, 0);
    if (Ptr(raster) && ext > 0 && ext < 0x1000 && gta::Readable(reinterpret_cast<void*>(raster), ext + 12)) {
        t.width = At<int>(raster, 0x0C);
        t.height = At<int>(raster, 0x10);
        t.depth = At<int>(raster, 0x14);
        DWORD d3d = At<DWORD>(raster + ext, 0);
        t.d3d = Ptr(d3d) ? reinterpret_cast<void*>(d3d) : nullptr;
        t.alpha = At<BYTE>(raster + ext, 8) != 0;
    }
    out->textures.push_back(t);
    ref.d3d = t.d3d;
    ref.alpha = t.alpha;
    return ref;
}

DWORD Call(DWORD fn, DWORD arg) {
    return reinterpret_cast<PtrFn>(fn)(arg);
}

// A pele do atomico (se tiver): os ossos (a primeira vez) e os pesos de cada vertice, na ordem da geometria.
void ReadSkin(DWORD atomic, DWORD geom, int numVerts, const Vec3* pos, const Vec3* normal, const Xform& toModel, Mesh* out) {
    DWORD skin = Call(FN_GEOMETRY_SKIN, geom);
    if (!Ptr(skin)) {
        return;
    }
    int numBones = static_cast<int>(Call(FN_SKIN_BONES, skin));
    DWORD weights = Call(FN_SKIN_WEIGHTS, skin), indices = Call(FN_SKIN_INDICES, skin), matrices = Call(FN_SKIN_MATRICES, skin);
    if (numBones < 1 || numBones > MAX_BONES || !Ptr(weights) || !Ptr(indices) || !Ptr(matrices) ||
        !gta::Readable(reinterpret_cast<void*>(weights), numVerts * 16) ||
        !gta::Readable(reinterpret_cast<void*>(indices), numVerts * 4) ||
        !gta::Readable(reinterpret_cast<void*>(matrices), numBones * 64)) {
        return;
    }
    Skin& s = out->skin;
    if (s.bones.empty()) {
        DWORD hier = Call(FN_ATOMIC_HIERARCHY, atomic);
        if (!Ptr(hier) || !gta::Readable(reinterpret_cast<void*>(hier), 0x14)) {
            return;
        }
        int numNodes = At<int>(hier, 0x04);
        DWORD nodes = At<DWORD>(hier, 0x10);
        if (numNodes != numBones || !Ptr(nodes) || !gta::Readable(reinterpret_cast<void*>(nodes), numNodes * 16)) {
            return;
        }
        // O pai de cada no pela pilha do RpHAnim: guarda o pai (2) antes de um filho com irmaos, volta (1) depois de uma folha
        std::vector<Bone> bones(numBones);
        int stack[MAX_BONES + 1], top = 0, current = -1;
        for (int i = 0; i < numBones; ++i) {
            DWORD node = nodes + i * 16;
            int flags = At<int>(node, 8);
            bones[i].tag = At<int>(node, 0);
            bones[i].parent = current;
            bones[i].name = FrameName(At<DWORD>(node, 0xC));
            memcpy(bones[i].skinToBone, reinterpret_cast<const void*>(matrices + i * 64), 64);
            if ((flags & 2) && top < MAX_BONES) {
                stack[top++] = current;
            }
            current = (flags & 1) ? (top > 0 ? stack[--top] : -1) : i;
        }
        s.bones = bones;
    } else if (static_cast<int>(s.bones.size()) != numBones) {
        return; // outro esqueleto no mesmo modelo: so o primeiro anima
    }
    SkinBatch b;
    b.batch = static_cast<int>(out->batches.size());
    b.pos.assign(pos, pos + numVerts);
    b.normal.assign(normal, normal + numVerts);
    b.index.resize(numVerts * 4);
    b.weight.resize(numVerts * 4);
    for (int i = 0; i < numVerts; ++i) {
        DWORD packed = At<DWORD>(indices, i * 4);
        for (int k = 0; k < 4; ++k) {
            BYTE bone = static_cast<BYTE>(packed >> (8 * k));
            b.index[i * 4 + k] = bone < numBones ? bone : 0;
            b.weight[i * 4 + k] = At<float>(weights, i * 16 + k * 4);
        }
    }
    b.toModel[0] = toModel.r;
    b.toModel[1] = toModel.u;
    b.toModel[2] = toModel.a;
    b.toModel[3] = toModel.p;
    s.batches.push_back(std::move(b));
}

bool ReadAtomic(DWORD atomic, const Xform& toModel, bool flipWheel, bool vehicle, Mesh* out) {
    DWORD geom = At<DWORD>(atomic, 0x18);
    if (!Ptr(geom) || !gta::Readable(reinterpret_cast<void*>(geom), 0x60)) {
        return false;
    }
    int numTris = At<int>(geom, 0x10), numVerts = At<int>(geom, 0x14);
    DWORD mats = At<DWORD>(geom, 0x20);
    int numMats = At<int>(geom, 0x24);
    DWORD tris = At<DWORD>(geom, 0x2C), uvs = At<DWORD>(geom, 0x34), morph = At<DWORD>(geom, 0x5C);
    if (numVerts <= 0 || numVerts > MAX_VERTS || numTris <= 0 || numTris > MAX_TRIS || numMats < 0 || numMats > 4096 ||
        !Ptr(morph) || !gta::Readable(reinterpret_cast<void*>(morph), 0x1C)) {
        return false;
    }
    DWORD verts = At<DWORD>(morph, 0x14), normals = At<DWORD>(morph, 0x18);
    if (!Ptr(verts) || !gta::Readable(reinterpret_cast<void*>(verts), numVerts * 12) || !Ptr(tris) ||
        !gta::Readable(reinterpret_cast<void*>(tris), numTris * 8)) {
        return false; // sem a copia na memoria (geometria nativa): nada para desenhar
    }
    bool hasNormals = Ptr(normals) && gta::Readable(reinterpret_cast<void*>(normals), numVerts * 12);
    bool hasUv = Ptr(uvs) && gta::Readable(reinterpret_cast<void*>(uvs), numVerts * 8);
    bool hasMats = numMats > 0 && Ptr(mats) && gta::Readable(reinterpret_cast<void*>(mats), numMats * 4);

    Batch b;
    b.verts.resize(numVerts);
    const Vec3* src = reinterpret_cast<const Vec3*>(verts);
    const Vec3* nsrc = hasNormals ? reinterpret_cast<const Vec3*>(normals) : nullptr;
    const float* uv = hasUv ? reinterpret_cast<const float*>(uvs) : nullptr;
    std::vector<Vec3> faceNormals;
    if (!nsrc) {
        faceNormals.assign(numVerts, Vec3{0, 0, 0});
    }
    const WORD* tri = reinterpret_cast<const WORD*>(tris);
    for (int i = 0; nsrc == nullptr && i < numTris; ++i) {
        WORD a = tri[i * 4], c1 = tri[i * 4 + 1], c2 = tri[i * 4 + 2];
        if (a >= numVerts || c1 >= numVerts || c2 >= numVerts) {
            continue;
        }
        Vec3 e1 = {src[c1].x - src[a].x, src[c1].y - src[a].y, src[c1].z - src[a].z};
        Vec3 e2 = {src[c2].x - src[a].x, src[c2].y - src[a].y, src[c2].z - src[a].z};
        Vec3 n = {e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
        for (WORD k : {a, c1, c2}) {
            faceNormals[k].x += n.x;
            faceNormals[k].y += n.y;
            faceNormals[k].z += n.z;
        }
    }
    for (int i = 0; i < numVerts; ++i) {
        Vec3 p = src[i];
        Vec3 n = nsrc ? nsrc[i] : faceNormals[i];
        if (flipWheel) {
            p = {-p.x, -p.y, p.z};
            n = {-n.x, -n.y, n.z};
        }
        Vec3 wp = toModel.Point(p), wn = toModel.Dir(n);
        float len = std::sqrt(wn.x * wn.x + wn.y * wn.y + wn.z * wn.z);
        if (len > 1e-6f) {
            wn = {wn.x / len, wn.y / len, wn.z / len};
        } else {
            wn = {0, 0, 1};
        }
        Vertex& v = b.verts[i];
        v.x = wp.x;
        v.y = wp.y;
        v.z = wp.z;
        v.nx = wn.x;
        v.ny = wn.y;
        v.nz = wn.z;
        v.u = uv ? uv[i * 2] : 0.0f;
        v.v = uv ? uv[i * 2 + 1] : 0.0f;
        if (Finite(wp)) {
            out->min = {std::min(out->min.x, wp.x), std::min(out->min.y, wp.y), std::min(out->min.z, wp.z)};
            out->max = {std::max(out->max.x, wp.x), std::max(out->max.y, wp.y), std::max(out->max.z, wp.z)};
        }
    }
    // Uma parte por material, na ordem dos materiais.
    int partsCount = hasMats ? numMats : 1;
    for (int m = 0; m < partsCount; ++m) {
        Part part;
        part.first = static_cast<int>(b.indices.size());
        for (int i = 0; i < numTris; ++i) {
            const WORD* t = &tri[i * 4];
            if ((hasMats && t[3] != m) || t[0] >= numVerts || t[1] >= numVerts || t[2] >= numVerts) {
                continue;
            }
            b.indices.push_back(t[0]);
            b.indices.push_back(t[1]);
            b.indices.push_back(t[2]);
        }
        part.count = static_cast<int>(b.indices.size()) - part.first;
        if (hasMats) {
            DWORD mat = At<DWORD>(mats, m * 4);
            if (Ptr(mat) && gta::Readable(reinterpret_cast<void*>(mat), 8)) {
                part.color = Argb(At<DWORD>(mat, 4));
                if (vehicle) {
                    part.color = VehicleColor(part.color);
                }
                TexRef tex = ReadTexture(At<DWORD>(mat, 0), out, true);
                part.texture = tex.d3d;
                part.alpha = tex.alpha || (part.color >> 24) < 255;
            }
        }
        if (part.count > 0) {
            b.parts.push_back(part);
        }
    }
    if (!vehicle) {
        std::vector<Vec3> rawNormals(numVerts);
        for (int i = 0; i < numVerts; ++i) {
            rawNormals[i] = nsrc ? nsrc[i] : faceNormals[i];
        }
        ReadSkin(atomic, geom, numVerts, src, rawNormals.data(), toModel, out);
    }
    out->vertices += numVerts;
    out->triangles += numTris;
    out->materials += hasMats ? numMats : 0;
    out->atomics += 1;
    out->batches.push_back(std::move(b));
    return true;
}

void ReadTxd(int txdIndex, Mesh* out) {
    DWORD dict = gta::TxdDictionary(txdIndex);
    if (!dict || !gta::Readable(reinterpret_cast<void*>(dict), 0x18)) {
        return;
    }
    DWORD head = dict + 0x08;
    DWORD link = At<DWORD>(head, 0);
    for (int i = 0; Ptr(link) && link != head && i < 4096; ++i) {
        ReadTexture(link - 0x08, out, false);
        if (!gta::Readable(reinterpret_cast<void*>(link), 4)) {
            break;
        }
        link = At<DWORD>(link, 0);
    }
}

void AddLine(Mesh* out, const Vec3& a, const Vec3& b) {
    out->colLines.push_back(a);
    out->colLines.push_back(b);
}

void ReadCollision(DWORD info, Mesh* out) {
    DWORD col = gta::ColModel(info);
    if (!col || !gta::Readable(reinterpret_cast<void*>(col), 0x30)) {
        return;
    }
    out->hasCol = true;
    out->colMin = At<Vec3>(col, 0x00);
    out->colMax = At<Vec3>(col, 0x0C);
    DWORD data = At<DWORD>(col, 0x2C);
    if (!Ptr(data) || !gta::Readable(reinterpret_cast<void*>(data), 0x30)) {
        return; // a colisao dos mapas so carrega perto dela
    }
    out->colLoaded = true;
    int spheres = At<WORD>(data, 0x0), boxes = At<WORD>(data, 0x2), tris = At<WORD>(data, 0x4);
    out->colSpheres = spheres;
    out->colBoxes = boxes;
    out->colTriangles = tris;
    DWORD sp = At<DWORD>(data, 0x08), bx = At<DWORD>(data, 0x0C), vx = At<DWORD>(data, 0x14), tr = At<DWORD>(data, 0x18);
    if (spheres > 0 && Ptr(sp) && gta::Readable(reinterpret_cast<void*>(sp), spheres * 0x14)) {
        for (int i = 0; i < spheres; ++i) {
            Vec3 c = At<Vec3>(sp + i * 0x14, 0);
            float r = At<float>(sp + i * 0x14, 0xC);
            const int SEG = 16;
            for (int k = 0; k < SEG; ++k) {
                float a0 = k * 6.2831853f / SEG, a1 = (k + 1) * 6.2831853f / SEG;
                float c0 = std::cos(a0) * r, s0 = std::sin(a0) * r, c1 = std::cos(a1) * r, s1 = std::sin(a1) * r;
                AddLine(out, {c.x + c0, c.y + s0, c.z}, {c.x + c1, c.y + s1, c.z});
                AddLine(out, {c.x + c0, c.y, c.z + s0}, {c.x + c1, c.y, c.z + s1});
                AddLine(out, {c.x, c.y + c0, c.z + s0}, {c.x, c.y + c1, c.z + s1});
            }
        }
    }
    if (boxes > 0 && Ptr(bx) && gta::Readable(reinterpret_cast<void*>(bx), boxes * 0x1C)) {
        for (int i = 0; i < boxes; ++i) {
            Vec3 a = At<Vec3>(bx + i * 0x1C, 0), b = At<Vec3>(bx + i * 0x1C, 0xC);
            Vec3 p[8] = {{a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, b.y, a.z}, {a.x, b.y, a.z},
                         {a.x, a.y, b.z}, {b.x, a.y, b.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}};
            static const int E[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                         {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
            for (const auto& e : E) {
                AddLine(out, p[e[0]], p[e[1]]);
            }
        }
    }
    if (tris > 0 && Ptr(tr) && Ptr(vx) && gta::Readable(reinterpret_cast<void*>(tr), tris * 8)) {
        int maxIndex = 0;
        const WORD* t = reinterpret_cast<const WORD*>(tr);
        int count = std::min(tris, MAX_COL_TRIS);
        for (int i = 0; i < count; ++i) {
            maxIndex = std::max({maxIndex, static_cast<int>(t[i * 4]), static_cast<int>(t[i * 4 + 1]),
                                 static_cast<int>(t[i * 4 + 2])});
        }
        if (gta::Readable(reinterpret_cast<void*>(vx), (maxIndex + 1) * 6)) {
            const short* v = reinterpret_cast<const short*>(vx);
            auto P = [&](int k) { return Vec3{v[k * 3] / 128.0f, v[k * 3 + 1] / 128.0f, v[k * 3 + 2] / 128.0f}; };
            for (int i = 0; i < count; ++i) {
                Vec3 a = P(t[i * 4]), b = P(t[i * 4 + 1]), c = P(t[i * 4 + 2]);
                AddLine(out, a, b);
                AddLine(out, b, c);
                AddLine(out, c, a);
            }
        }
    }
}

bool SameName(DWORD tex, const std::string& name) {
    if (!Ptr(tex) || !gta::Readable(reinterpret_cast<void*>(tex), 0x30)) {
        return false;
    }
    const char* n = reinterpret_cast<const char*>(tex + 0x10);
    size_t len = strnlen(n, 32);
    return len == name.size() && _strnicmp(n, name.c_str(), len) == 0;
}

bool AtomicUses(DWORD atomic, const std::string& name) {
    DWORD geom = At<DWORD>(atomic, 0x18);
    if (!Ptr(geom) || !gta::Readable(reinterpret_cast<void*>(geom), 0x28)) {
        return false;
    }
    DWORD mats = At<DWORD>(geom, 0x20);
    int numMats = At<int>(geom, 0x24);
    if (numMats <= 0 || numMats > 4096 || !Ptr(mats) || !gta::Readable(reinterpret_cast<void*>(mats), numMats * 4)) {
        return false;
    }
    for (int m = 0; m < numMats; ++m) {
        DWORD mat = At<DWORD>(mats, m * 4);
        if (Ptr(mat) && gta::Readable(reinterpret_cast<void*>(mat), 4) && SameName(At<DWORD>(mat, 0), name)) {
            return true;
        }
    }
    return false;
}

} // namespace

bool TxdHas(int txdIndex, const std::string& name) {
    DWORD dict = gta::TxdDictionary(txdIndex);
    if (!dict || !gta::Readable(reinterpret_cast<void*>(dict), 0x18)) {
        return false;
    }
    DWORD head = dict + 0x08;
    DWORD link = At<DWORD>(head, 0);
    for (int i = 0; Ptr(link) && link != head && i < 4096; ++i) {
        if (SameName(link - 0x08, name)) {
            return true;
        }
        if (!gta::Readable(reinterpret_cast<void*>(link), 4)) {
            break;
        }
        link = At<DWORD>(link, 0);
    }
    return false;
}

bool UsesTexture(int id, const std::string& name) {
    DWORD obj = gta::RwObjectOf(gta::ModelInfo(id));
    if (!obj || !gta::Readable(reinterpret_cast<void*>(obj), 0x30)) {
        return false;
    }
    BYTE type = At<BYTE>(obj, 0);
    if (type == RW_ATOMIC) {
        return AtomicUses(obj, name);
    }
    if (type != RW_CLUMP) {
        return false;
    }
    DWORD head = obj + 0x08;
    DWORD link = At<DWORD>(head, 0);
    for (int i = 0; Ptr(link) && link != head && i < MAX_ATOMICS; ++i) {
        DWORD atomic = link - 0x40;
        if (!gta::Readable(reinterpret_cast<void*>(atomic), 0x70)) {
            break;
        }
        if (AtomicUses(atomic, name)) {
            return true;
        }
        link = At<DWORD>(link, 0);
    }
    return false;
}

bool Extract(int id, bool vehicle, Mesh* out) {
    *out = Mesh();
    out->id = id;
    out->min = {1e9f, 1e9f, 1e9f};
    out->max = {-1e9f, -1e9f, -1e9f};
    DWORD info = gta::ModelInfo(id);
    DWORD obj = gta::RwObjectOf(info);
    if (!obj || !gta::Readable(reinterpret_cast<void*>(obj), 0x30)) {
        out->error = "modelo n\xC3\xA3o carregado";
        return false;
    }
    BYTE type = At<BYTE>(obj, 0);
    if (type == RW_ATOMIC) {
        ReadAtomic(obj, FrameToModel(At<DWORD>(obj, 0x04)), false, vehicle, out);
    } else if (type == RW_CLUMP) {
        DWORD head = obj + 0x08;
        DWORD link = At<DWORD>(head, 0);
        for (int i = 0; Ptr(link) && link != head && i < MAX_ATOMICS; ++i) {
            DWORD atomic = link - 0x40;
            if (!gta::Readable(reinterpret_cast<void*>(atomic), 0x70)) {
                break;
            }
            DWORD frame = At<DWORD>(atomic, 0x04);
            std::string name = Lower(FrameName(frame));
            bool visible = (At<BYTE>(atomic, 0x02) & ATOMIC_RENDER) != 0;
            if (visible && name.find("_vlo") == std::string::npos) {
                bool leftWheel = vehicle && name.rfind("wheel_l", 0) == 0;
                ReadAtomic(atomic, FrameToModel(frame), leftWheel, vehicle, out);
            }
            link = At<DWORD>(link, 0);
        }
    } else {
        out->error = "tipo de objeto do RenderWare desconhecido";
        return false;
    }
    ReadTxd(gta::TxdIndex(info), out);
    ReadCollision(info, out);
    // As usadas pelo modelo primeiro, depois o resto da TXD.
    std::stable_sort(out->textures.begin(), out->textures.end(),
                     [](const Texture& a, const Texture& b) { return a.used && !b.used; });
    if (out->batches.empty()) {
        out->min = out->max = {0, 0, 0};
        out->error = "sem malha na mem\xC3\xB3ria";
        return false;
    }
    out->ok = true;
    return true;
}

} // namespace mesh

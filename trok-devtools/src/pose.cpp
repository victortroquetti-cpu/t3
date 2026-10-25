// Trok DevTools (.asi) -- Victor_Trok
// As contas do RenderWare (vetor-linha): um ponto vai p * direita/cima/frente + posicao; "a depois b" = Mul(a, b).
// A pose de cada osso no espaco da pele: G = local * G(pai). A pele: vertice * skinToBone * G, somado pelos pesos.
// A pose do modelo (de cada osso, em relacao ao pai) sai das skinToBone: o osso no espaco da pele e a inversa dela.

#include "pose.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace pose {
namespace {

struct M {
    float r[3], u[3], a[3], p[3];
};

M FromRw(const float* m) {
    M o;
    for (int k = 0; k < 3; ++k) {
        o.r[k] = m[k];
        o.u[k] = m[4 + k];
        o.a[k] = m[8 + k];
        o.p[k] = m[12 + k];
    }
    return o;
}

M FromVecs(const gta::Vec3* v) {
    M o;
    const float* src[4] = {&v[0].x, &v[1].x, &v[2].x, &v[3].x};
    for (int k = 0; k < 3; ++k) {
        o.r[k] = src[0][k];
        o.u[k] = src[1][k];
        o.a[k] = src[2][k];
        o.p[k] = src[3][k];
    }
    return o;
}

void Dir(const M& m, const float* v, float* out) {
    for (int k = 0; k < 3; ++k) {
        out[k] = v[0] * m.r[k] + v[1] * m.u[k] + v[2] * m.a[k];
    }
}

void Point(const M& m, const float* v, float* out) {
    Dir(m, v, out);
    for (int k = 0; k < 3; ++k) {
        out[k] += m.p[k];
    }
}

M Mul(const M& a, const M& b) {
    M o;
    Dir(b, a.r, o.r);
    Dir(b, a.u, o.u);
    Dir(b, a.a, o.a);
    Point(b, a.p, o.p);
    return o;
}

M Inverse(const M& m) {
    // As linhas r, u, a formam R; ponto' = ponto * R + p  =>  ponto = (ponto' - p) * R^-1
    const float* R[3] = {m.r, m.u, m.a};
    float c[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            int i1 = (i + 1) % 3, i2 = (i + 2) % 3, j1 = (j + 1) % 3, j2 = (j + 2) % 3;
            c[i][j] = R[i1][j1] * R[i2][j2] - R[i1][j2] * R[i2][j1];
        }
    }
    float det = R[0][0] * c[0][0] + R[0][1] * c[0][1] + R[0][2] * c[0][2];
    M o;
    if (std::fabs(det) < 1e-12f) {
        o = m;
        return o;
    }
    float* out[3] = {o.r, o.u, o.a};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            out[i][j] = c[j][i] / det;
        }
    }
    float np[3] = {-m.p[0], -m.p[1], -m.p[2]};
    Dir(o, np, o.p);
    return o;
}

// CQuaternion::Get (o mesmo do RtQuatUnitConvertToMatrix)
M FromQuat(const float* q, const float* t) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    float len = std::sqrt(x * x + y * y + z * z + w * w);
    if (len > 1e-6f) {
        x /= len, y /= len, z /= len, w /= len;
    }
    float x2 = x + x, y2 = y + y, z2 = z + z;
    M o;
    o.r[0] = 1 - (z * z2 + y * y2), o.r[1] = z2 * w + y2 * x, o.r[2] = z2 * x - y2 * w;
    o.u[0] = y2 * x - z2 * w, o.u[1] = 1 - (z * z2 + x * x2), o.u[2] = x2 * w + z2 * y;
    o.a[0] = y2 * w + z2 * x, o.a[1] = z2 * y - x2 * w, o.a[2] = 1 - (y * y2 + x * x2);
    o.p[0] = t[0], o.p[1] = t[1], o.p[2] = t[2];
    return o;
}

void Sample(const ifp::Track& tr, float time, float* q, float* t) {
    const std::vector<ifp::Key>& k = tr.keys;
    size_t i = 0;
    while (i + 1 < k.size() && k[i + 1].time <= time) {
        ++i;
    }
    const ifp::Key& a = k[i];
    if (i + 1 >= k.size() || time <= a.time || k[i + 1].time - a.time < 1e-6f) {
        std::copy(a.q, a.q + 4, q);
        std::copy(a.t, a.t + 3, t);
        return;
    }
    const ifp::Key& b = k[i + 1];
    float f = (time - a.time) / (b.time - a.time);
    float bq[4] = {b.q[0], b.q[1], b.q[2], b.q[3]};
    float cosine = a.q[0] * bq[0] + a.q[1] * bq[1] + a.q[2] * bq[2] + a.q[3] * bq[3];
    if (cosine < 0) {
        cosine = -cosine;
        for (float& v : bq) {
            v = -v;
        }
    }
    float wa = 1 - f, wb = f;
    if (cosine < 0.9995f) {
        float theta = std::acos(std::min(1.0f, cosine)), s = std::sin(theta);
        wa = std::sin((1 - f) * theta) / s;
        wb = std::sin(f * theta) / s;
    }
    for (int c = 0; c < 4; ++c) {
        q[c] = a.q[c] * wa + bq[c] * wb;
    }
    for (int c = 0; c < 3; ++c) {
        t[c] = a.t[c] + (b.t[c] - a.t[c]) * f;
    }
}

bool SameName(const std::string& a, const std::string& b) {
    if (a.size() != b.size() || a.empty()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (tolower(static_cast<unsigned char>(a[i])) != tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// O osso de cada quadro-chave: pela etiqueta (o jogo) ou, sem etiqueta, pelo nome.
int TrackFor(const mesh::Bone& bone, const ifp::Clip& clip) {
    for (size_t t = 0; t < clip.tracks.size(); ++t) {
        const ifp::Track& tr = clip.tracks[t];
        if (tr.keys.empty()) {
            continue;
        }
        if ((tr.bone >= 0 && tr.bone == bone.tag) || (tr.bone < 0 && SameName(tr.name, bone.name))) {
            return static_cast<int>(t);
        }
    }
    return -1;
}

// A copia ainda e deste modelo carregado? (o mesmo id carregado de novo traz outras texturas)
bool SameModel(const mesh::Mesh& base, const mesh::Mesh& out) {
    if (out.id != base.id || out.batches.size() != base.batches.size() || out.vertices != base.vertices) {
        return false;
    }
    for (size_t b = 0; b < base.batches.size(); ++b) {
        const std::vector<mesh::Part>& p = base.batches[b].parts;
        const std::vector<mesh::Part>& q = out.batches[b].parts;
        if (p.size() != q.size() || base.batches[b].verts.size() != out.batches[b].verts.size()) {
            return false;
        }
        for (size_t i = 0; i < p.size(); ++i) {
            if (p[i].texture != q[i].texture) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

int Matched(const mesh::Mesh& base, const ifp::Clip& clip) {
    int n = 0;
    for (const mesh::Bone& b : base.skin.bones) {
        n += TrackFor(b, clip) >= 0;
    }
    return n;
}

bool Apply(const mesh::Mesh& base, const ifp::Clip& clip, float time, bool inPlace, mesh::Mesh* out) {
    const mesh::Skin& skin = base.skin;
    size_t n = skin.bones.size();
    if (n == 0 || skin.batches.empty()) {
        return false;
    }
    if (!SameModel(base, *out)) {
        *out = base;
        gta::Vec3 size = {base.max.x - base.min.x, base.max.y - base.min.y, base.max.z - base.min.z};
        float pad = 0.3f * std::max(size.x, std::max(size.y, size.z));
        out->min.x -= pad, out->min.y -= pad;
        out->max.x += pad, out->max.y += pad, out->max.z += pad * 0.5f;
    }
    std::vector<M> S(n), bind(n), G(n), K(n);
    for (size_t i = 0; i < n; ++i) {
        S[i] = FromRw(skin.bones[i].skinToBone);
        bind[i] = Inverse(S[i]);
    }
    for (size_t i = 0; i < n; ++i) {
        int parent = skin.bones[i].parent;
        bool hasParent = parent >= 0 && parent < static_cast<int>(i);
        M local = hasParent ? Mul(bind[i], Inverse(bind[parent])) : bind[i];
        int t = TrackFor(skin.bones[i], clip);
        if (t >= 0) {
            const ifp::Track& tr = clip.tracks[t];
            float q[4], tt[3];
            Sample(tr, time, q, tt);
            if (!tr.translation) {
                // Sem translacao: o osso fica onde o modelo poe (a raiz, na origem: o BonePos dela e zero no jogo)
                for (int c = 0; c < 3; ++c) {
                    tt[c] = hasParent ? local.p[c] : 0.0f;
                }
            } else if (inPlace && !hasParent) {
                tt[0] = tr.keys.front().t[0];
                tt[1] = tr.keys.front().t[1];
            }
            local = FromQuat(q, tt);
        }
        G[i] = hasParent ? Mul(local, G[parent]) : local;
        K[i] = Mul(S[i], G[i]);
    }
    for (const mesh::SkinBatch& sb : skin.batches) {
        if (sb.batch < 0 || sb.batch >= static_cast<int>(out->batches.size())) {
            continue;
        }
        std::vector<mesh::Vertex>& verts = out->batches[sb.batch].verts;
        M toModel = FromVecs(sb.toModel);
        size_t count = std::min(verts.size(), sb.pos.size());
        for (size_t v = 0; v < count; ++v) {
            float p[3] = {0, 0, 0}, nrm[3] = {0, 0, 0}, sum = 0;
            const float* src = &sb.pos[v].x;
            const float* nsrc = &sb.normal[v].x;
            for (int k = 0; k < 4; ++k) {
                float w = sb.weight[v * 4 + k];
                if (w <= 0) {
                    continue;
                }
                float tp[3], tn[3];
                const M& m = K[sb.index[v * 4 + k]];
                Point(m, src, tp);
                Dir(m, nsrc, tn);
                for (int c = 0; c < 3; ++c) {
                    p[c] += tp[c] * w;
                    nrm[c] += tn[c] * w;
                }
                sum += w;
            }
            if (sum < 1e-4f) {
                std::copy(src, src + 3, p);
                std::copy(nsrc, nsrc + 3, nrm);
            }
            float wp[3], wn[3];
            Point(toModel, p, wp);
            Dir(toModel, nrm, wn);
            float len = std::sqrt(wn[0] * wn[0] + wn[1] * wn[1] + wn[2] * wn[2]);
            mesh::Vertex& dst = verts[v];
            dst.x = wp[0], dst.y = wp[1], dst.z = wp[2];
            if (len > 1e-6f) {
                dst.nx = wn[0] / len, dst.ny = wn[1] / len, dst.nz = wn[2] / len;
            }
        }
    }
    return true;
}

} // namespace pose

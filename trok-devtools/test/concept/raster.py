# Imagens de conceito do Trok DevTools: um rasterizador 3D pequeno (numpy) para desenhar os modelos de mentira do
# conceito com a mesma camera, luz e grade do visualizador do .asi (render.cpp). Nao e o jogo: os modelos sao desenhos
# low-poly feitos aqui (models.py).
import math

import numpy as np
from PIL import Image, ImageDraw


def normalize(v):
    v = np.asarray(v, dtype=np.float64)
    n = np.linalg.norm(v, axis=-1, keepdims=True)
    return v / np.maximum(n, 1e-9)


class Mesh:
    """Triangulos com normais por vertice, UV, cor por face e textura opcional (nome em TEXTURES)."""

    def __init__(self):
        self.v, self.n, self.uv, self.c, self.t, self.ds = [], [], [], [], [], []

    def tri(self, a, b, c, color, normals=None, uv=None, tex=None, ds=False):
        p = np.array([a, b, c], dtype=np.float64)
        if normals is None:
            fn = np.cross(p[1] - p[0], p[2] - p[0])
            if np.linalg.norm(fn) < 1e-12:
                return
            normals = np.repeat(normalize(fn)[None], 3, axis=0)
        self.v.append(p)
        self.n.append(normalize(np.array(normals, dtype=np.float64)))
        self.uv.append(np.array(uv if uv is not None else [(0, 0), (0, 0), (0, 0)], dtype=np.float64))
        self.c.append(np.array(color[:3], dtype=np.float64) / (255.0 if max(color[:3]) > 1.0 else 1.0))
        self.t.append(tex)
        self.ds.append(ds)

    def quad(self, a, b, c, d, color, uv=((0, 0), (1, 0), (1, 1), (0, 1)), tex=None, ds=False, normals=None):
        na = nb = None
        if normals is not None:
            na, nb = [normals[0], normals[1], normals[2]], [normals[0], normals[2], normals[3]]
        self.tri(a, b, c, color, na, [uv[0], uv[1], uv[2]], tex, ds)
        self.tri(a, c, d, color, nb, [uv[0], uv[2], uv[3]], tex, ds)

    def add(self, other, rot=None, offset=(0, 0, 0), scale=1.0):
        R = np.eye(3) if rot is None else np.asarray(rot)
        o = np.asarray(offset, dtype=np.float64)
        for v, n, uv, c, t, ds in zip(other.v, other.n, other.uv, other.c, other.t, other.ds):
            self.v.append(v @ R.T * scale + o)
            self.n.append(normalize(n @ R.T))
            self.uv.append(uv)
            self.c.append(c)
            self.t.append(t)
            self.ds.append(ds)
        return self

    def bounds(self):
        allv = np.concatenate(self.v, axis=0)
        return allv.min(axis=0), allv.max(axis=0)

    def count(self):
        return len(self.v) * 3, len(self.v)


def rot_x(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])


def rot_y(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])


def rot_z(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])


# ---------------------------------------------------------------- primitivas


def box(m, lo, hi, color, tex=None, uvs=1.0, skip=(), colors=None):
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    faces = {
        'top': ((x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)),
        'bottom': ((x0, y1, z0), (x1, y1, z0), (x1, y0, z0), (x0, y0, z0)),
        'front': ((x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)),  # +y
        'back': ((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)),  # -y
        'right': ((x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)),  # +x
        'left': ((x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)),  # -x
    }
    for name, (a, b, c, d) in faces.items():
        if name in skip:
            continue
        w = np.linalg.norm(np.subtract(b, a)) * uvs
        h = np.linalg.norm(np.subtract(d, a)) * uvs
        col = colors.get(name, color) if colors else color
        m.quad(a, b, c, d, col, ((0, h), (w, h), (w, 0), (0, 0)), tex)


def cylinder(m, base, r, h, color, seg=12, r_top=None, caps=(True, True), smooth=True, tex=None, cap_color=None,
             uv_u=1.0, uv_v=1.0, rot=None, ds=False):
    """Cilindro (ou tronco de cone) em z, base no ponto dado; rot gira em volta da base."""
    r_top = r if r_top is None else r_top
    R = np.eye(3) if rot is None else np.asarray(rot)
    o = np.asarray(base, dtype=np.float64)

    def P(x, y, z):
        return np.asarray((x, y, z)) @ R.T + o

    slope = (r - r_top) / max(h, 1e-6)
    for i in range(seg):
        a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
        c0, s0, c1, s1 = math.cos(a0), math.sin(a0), math.cos(a1), math.sin(a1)
        p = [P(r * c0, r * s0, 0), P(r * c1, r * s1, 0), P(r_top * c1, r_top * s1, h), P(r_top * c0, r_top * s0, h)]
        if smooth:
            n0 = normalize(np.asarray((c0, s0, slope)) @ R.T)
            n1 = normalize(np.asarray((c1, s1, slope)) @ R.T)
            normals = (n0, n1, n1, n0)
        else:
            normals = None
        u0, u1 = i / seg * uv_u, (i + 1) / seg * uv_u
        m.quad(p[0], p[1], p[2], p[3], color, ((u0, uv_v), (u1, uv_v), (u1, 0), (u0, 0)), tex, ds, normals)
    cc = cap_color or color
    for top, rr, z in ((False, r, 0.0), (True, r_top, h)):
        if not caps[1 if top else 0] or rr <= 1e-6:
            continue
        center = P(0, 0, z)
        for i in range(seg):
            a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
            p0, p1 = P(rr * math.cos(a0), rr * math.sin(a0), z), P(rr * math.cos(a1), rr * math.sin(a1), z)
            uvc = (0.5, 0.5)
            uv0 = (0.5 + 0.5 * math.cos(a0), 0.5 + 0.5 * math.sin(a0))
            uv1 = (0.5 + 0.5 * math.cos(a1), 0.5 + 0.5 * math.sin(a1))
            if top:
                m.tri(center, p0, p1, cc, uv=(uvc, uv0, uv1), tex=None)
            else:
                m.tri(center, p1, p0, cc, uv=(uvc, uv1, uv0), tex=None)


def sphere(m, center, r, color, seg=12, rings=8, scale=(1, 1, 1), tex=None, z_from=-1.0):
    sx, sy, sz = scale
    cx, cy, cz = center

    def P(t, p):
        return (cx + r * sx * math.sin(t) * math.cos(p), cy + r * sy * math.sin(t) * math.sin(p), cz + r * sz * math.cos(t))

    def N(t, p):
        return normalize((math.sin(t) * math.cos(p) / sx, math.sin(t) * math.sin(p) / sy, math.cos(t) / sz))

    t_max = math.acos(max(-1.0, min(1.0, z_from))) if z_from > -1.0 else math.pi
    for j in range(rings):
        t0, t1 = t_max * j / rings, t_max * (j + 1) / rings
        for i in range(seg):
            p0, p1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
            a, b, c, d = P(t0, p0), P(t1, p0), P(t1, p1), P(t0, p1)
            m.quad(a, b, c, d, color, ((i / seg, j / rings), (i / seg, (j + 1) / rings), ((i + 1) / seg, (j + 1) / rings),
                                       ((i + 1) / seg, j / rings)), tex, False, (N(t0, p0), N(t1, p0), N(t1, p1), N(t0, p1)))


def oquad(m, a, b, c, d, color, inside, uv=((0, 0), (1, 0), (1, 1), (0, 1)), tex=None, ds=False):
    """Quad com a normal para fora: se ela apontar para o ponto 'inside', a ordem e invertida."""
    a, b, c, d = (np.asarray(p, dtype=np.float64) for p in (a, b, c, d))
    n = np.cross(c - a, d - b)
    if n @ ((a + b + c + d) / 4 - np.asarray(inside)) < 0:
        m.quad(a, d, c, b, color, (uv[0], uv[3], uv[2], uv[1]), tex, ds)
    else:
        m.quad(a, b, c, d, color, uv, tex, ds)


def otri(m, a, b, c, color, inside, tex=None, uv=None):
    a, b, c = (np.asarray(p, dtype=np.float64) for p in (a, b, c))
    n = np.cross(b - a, c - a)
    if n @ ((a + b + c) / 3 - np.asarray(inside)) < 0:
        m.tri(a, c, b, color, uv=None if uv is None else (uv[0], uv[2], uv[1]), tex=tex)
    else:
        m.tri(a, b, c, color, uv=uv, tex=tex)


def loft(m, rings, color, tex=None, cap_start=True, cap_end=True, colors=None, cap_colors=(None, None), uvs=None):
    """Liga aneis de pontos (mesma quantidade) em sequencia, com as normais para fora do eixo dos aneis.
    colors[k] da cor da faixa k (entre o ponto k e k+1); None nao desenha a faixa."""
    rings = [np.asarray(r, dtype=np.float64) for r in rings]
    k = len(rings[0])
    centers = [r.mean(axis=0) for r in rings]
    for i in range(len(rings) - 1):
        a, b = rings[i], rings[i + 1]
        inside = (centers[i] + centers[i + 1]) / 2
        for j in range(k):
            j1 = (j + 1) % k
            col = colors[j] if colors else color
            if col is None:
                continue
            oquad(m, a[j], a[j1], b[j1], b[j], col, inside, tex=tex)
    for idx, nxt, on, cc in ((0, 1, cap_start, cap_colors[0]), (-1, -2, cap_end, cap_colors[1])):
        if not on:
            continue
        ring, c = rings[idx], centers[idx]
        inside = c + (centers[nxt] - c) * 0.5
        for j in range(k):
            j1 = (j + 1) % k
            otri(m, c, ring[j], ring[j1], cc or color, inside)


def extrude(m, poly, x0, x1, color, side_color=None, tex=None):
    """Poligono (y, z) estrelado em volta do centroide, extrudado de x0 a x1 (perfil de arma, placa, etc.)."""
    poly = np.asarray(poly, dtype=np.float64)
    c = poly.mean(axis=0)
    n = len(poly)
    mid = (x0 + x1) / 2
    for j in range(n):
        j1 = (j + 1) % n
        y0, z0 = poly[j]
        y1, z1 = poly[j1]
        oquad(m, (x1, y0, z0), (x1, y1, z1), (x0, y1, z1), (x0, y0, z0), side_color or color, (mid, c[0], c[1]))
        otri(m, (x1, c[0], c[1]), (x1, y1, z1), (x1, y0, z0), color, (mid, c[0], c[1]), tex,
             ((c[0], c[1]), (y1, z1), (y0, z0)))
        otri(m, (x0, c[0], c[1]), (x0, y0, z0), (x0, y1, z1), color, (mid, c[0], c[1]), tex,
             ((c[0], c[1]), (y0, z0), (y1, z1)))


# ---------------------------------------------------------------- camera e desenho


class Camera:
    def __init__(self, eye, center, fov_deg, aspect, near, far, up=(0, 0, 1)):
        self.eye, self.center = np.asarray(eye, float), np.asarray(center, float)
        self.fov, self.aspect, self.near, self.far = math.radians(fov_deg), aspect, near, far
        f = normalize(self.center - self.eye)
        s = normalize(np.cross(f, up))
        u = np.cross(s, f)
        self.view = np.array([[*s, -s @ self.eye], [*u, -u @ self.eye], [*(-f), f @ self.eye], [0, 0, 0, 1]])
        ys = 1 / math.tan(self.fov / 2)
        xs = ys / aspect
        self.proj = np.array([[xs, 0, 0, 0], [0, ys, 0, 0], [0, 0, far / (near - far), near * far / (near - far)],
                              [0, 0, -1, 0]])
        self.right, self.up, self.forward = s, u, f

    def clip(self, pts):
        p = np.concatenate([pts, np.ones(pts.shape[:-1] + (1,))], axis=-1)
        return p @ (self.proj @ self.view).T

    def project(self, pt, W, H):
        c = self.clip(np.asarray(pt, float)[None])[0]
        if c[3] <= self.near * 0.5:
            return None
        return ((c[0] / c[3] * 0.5 + 0.5) * W, (1 - (c[1] / c[3] * 0.5 + 0.5)) * H)


def fit_camera(lo, hi, yaw=35.0, pitch=18.0, fov=34.0, aspect=1.0, zoom=1.0):
    """A mesma conta do DrawMesh do render.cpp."""
    lo, hi = np.asarray(lo, float), np.asarray(hi, float)
    center = (lo + hi) / 2
    radius = max(0.05, np.linalg.norm(hi - lo) / 2)
    f = math.radians(fov)
    fit = min(f, 2 * math.atan(math.tan(f / 2) * aspect))
    dist = radius / math.sin(fit / 2) * 1.04 / max(0.2, zoom)
    y, p = math.radians(yaw), math.radians(pitch)
    d = np.array([math.sin(y) * math.cos(p), math.cos(y) * math.cos(p), math.sin(p)])
    eye = center + d * dist
    near = max(dist * 0.01, max(0.01, dist - radius * 1.6))
    far = dist + radius * 3 + 1
    cam = Camera(eye, center, fov, aspect, near, far)
    right = normalize(np.cross(-d, (0, 0, 1)))
    light_pos = eye + right * (-dist * 0.7) + np.array([0, 0, 1.0]) * dist * 0.9
    cam.light = normalize(center - light_pos)
    cam.radius, cam.dist = radius, dist
    return cam


class Target:
    def __init__(self, W, H, bg, ssaa=2):
        self.W, self.H, self.s = W, H, ssaa
        self.w, self.h = W * ssaa, H * ssaa
        if isinstance(bg, np.ndarray):
            img = Image.fromarray(bg).resize((self.w, self.h), Image.BILINEAR) if bg.shape[1] != self.w else Image.fromarray(bg)
            self.color = np.asarray(img, dtype=np.float32)[..., :3] / 255.0
        else:
            self.color = np.empty((self.h, self.w, 3), np.float32)
            self.color[:] = np.asarray(bg[:3], np.float32) / 255.0
        self.alpha = np.zeros((self.h, self.w), np.float32)
        self.depth = np.full((self.h, self.w), np.inf, np.float32)

    def lines(self, cam, segments, rgba, width=1.0):
        """Linhas 3D por cima (sem profundidade), como o DrawLines do .asi."""
        over = Image.new('RGBA', (self.w, self.h), (0, 0, 0, 0))
        d = ImageDraw.Draw(over)
        for a, b in segments:
            pa, pb = cam.project(a, self.w, self.h), cam.project(b, self.w, self.h)
            if pa and pb:
                d.line([pa, pb], fill=tuple(rgba), width=max(1, int(round(width * self.s))))
        o = np.asarray(over, dtype=np.float32) / 255.0
        a = o[..., 3:4]
        self.color = self.color * (1 - a) + o[..., :3] * a
        self.alpha = np.maximum(self.alpha, o[..., 3])

    def draw(self, mesh, cam, textures, ambient=0.36, diffuse=0.82, use_textures=True, fog=None):
        if not mesh.v:
            return
        V = np.stack(mesh.v)
        N = np.stack(mesh.n)
        UV = np.stack(mesh.uv)
        C = np.stack(mesh.c).astype(np.float32)
        clip = cam.clip(V)
        self._L = -cam.light
        self._shade = (ambient, diffuse, use_textures, fog, textures)
        for i in range(len(V)):
            w = clip[i, :, 3]
            if (w >= cam.near).all():
                self._tri(clip[i], N[i], UV[i], C[i], mesh.t[i], mesh.ds[i])
                continue
            if (w < cam.near).all():
                continue
            # corta no plano perto (w = near) e desenha o que sobra em leque
            poly = []
            for k in range(3):
                a, b = k, (k + 1) % 3
                wa, wb = w[a], w[b]
                if wa >= cam.near:
                    poly.append((clip[i, a], N[i, a], UV[i, a]))
                if (wa >= cam.near) != (wb >= cam.near):
                    t = (cam.near - wa) / (wb - wa)
                    poly.append((clip[i, a] + (clip[i, b] - clip[i, a]) * t, N[i, a] + (N[i, b] - N[i, a]) * t,
                                 UV[i, a] + (UV[i, b] - UV[i, a]) * t))
            for k in range(1, len(poly) - 1):
                tri = (poly[0], poly[k], poly[k + 1])
                self._tri(np.stack([p[0] for p in tri]), np.stack([p[1] for p in tri]), np.stack([p[2] for p in tri]),
                          C[i], mesh.t[i], mesh.ds[i])

    def _tri(self, clip, Nn, UVv, color, tex, ds):
        ambient, diffuse, use_textures, fog, textures = self._shade
        w = clip[:, 3]
        sx = (clip[:, 0] / w * 0.5 + 0.5) * self.w
        sy = (1 - (clip[:, 1] / w * 0.5 + 0.5)) * self.h
        sz = clip[:, 2] / w
        x0, x1 = int(max(0, math.floor(sx.min()))), int(min(self.w, math.ceil(sx.max())))
        y0, y1 = int(max(0, math.floor(sy.min()))), int(min(self.h, math.ceil(sy.max())))
        if x0 >= x1 or y0 >= y1:
            return
        area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sy[1] - sy[0]) * (sx[2] - sx[0])
        if abs(area) < 1e-9:
            return
        px, py = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
        w0 = ((sx[1] - px) * (sy[2] - py) - (sy[1] - py) * (sx[2] - px)) / area
        w1 = ((sx[2] - px) * (sy[0] - py) - (sy[2] - py) * (sx[0] - px)) / area
        w2 = 1 - w0 - w1
        inside = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
        if not inside.any():
            return
        z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2]
        zb = self.depth[y0:y1, x0:x1]
        ok = inside & (z < zb)
        if not ok.any():
            return
        iw = 1 / w
        q0, q1, q2 = w0 * iw[0], w1 * iw[1], w2 * iw[2]
        qs = q0 + q1 + q2
        b0, b1, b2 = q0 / qs, q1 / qs, q2 / qs
        n = b0[..., None] * Nn[0] + b1[..., None] * Nn[1] + b2[..., None] * Nn[2]
        n /= np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-9)
        lam = n @ self._L
        if ds:
            lam = np.abs(lam)
        light = ambient + diffuse * np.clip(lam, 0, 1)
        col = np.broadcast_to(color, ok.shape + (3,)).copy()
        if tex and use_textures:
            t = textures[tex]
            u = b0 * UVv[0, 0] + b1 * UVv[1, 0] + b2 * UVv[2, 0]
            v = b0 * UVv[0, 1] + b1 * UVv[1, 1] + b2 * UVv[2, 1]
            th, tw = t.shape[:2]
            tx = (np.floor(u * tw).astype(np.int64)) % tw
            ty = (np.floor(v * th).astype(np.int64)) % th
            s = t[ty, tx]
            if s.shape[-1] == 4:
                ok = ok & (s[..., 3] > 0.5)
            col = col * s[..., :3]
        out = np.clip(col * light[..., None], 0, 1)
        if fog is not None:
            dist = 1 / (b0 * iw[0] + b1 * iw[1] + b2 * iw[2])
            f = np.clip((dist - fog[0]) / (fog[1] - fog[0]), 0, 1)[..., None] * fog[3]
            out = out * (1 - f) + np.asarray(fog[2], np.float32) * f
        region = self.color[y0:y1, x0:x1]
        region[ok] = out[ok]
        zb[ok] = z[ok]
        self.alpha[y0:y1, x0:x1][ok] = 1.0

    def image(self, alpha=False):
        c = self.color.reshape(self.H, self.s, self.W, self.s, 3).mean(axis=(1, 3))
        if not alpha:
            return Image.fromarray((np.clip(c, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGB')
        a = self.alpha.reshape(self.H, self.s, self.W, self.s).mean(axis=(1, 3))
        rgba = np.concatenate([c, a[..., None]], axis=-1)
        return Image.fromarray((np.clip(rgba, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA')


def grid_segments(lo, hi, radius):
    """A grade do visualizador: passo pelo tamanho, centrada no modelo, no z de baixo."""
    step = 0.5 if radius < 3 else 1.0 if radius < 12 else 5.0 if radius < 60 else 20.0
    n = int(math.ceil(radius * 1.6 / step))
    cx = round((lo[0] + hi[0]) / 2 / step) * step
    cy = round((lo[1] + hi[1]) / 2 / step) * step
    z = lo[2]
    seg = []
    for i in range(-n, n + 1):
        strong = i == 0
        seg.append((((cx + i * step, cy - n * step, z), (cx + i * step, cy + n * step, z)), strong))
        seg.append((((cx - n * step, cy + i * step, z), (cx + n * step, cy + i * step, z)), strong))
    return seg

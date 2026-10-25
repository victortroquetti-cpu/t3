# Imagens de conceito do Trok DevTools: gera o que o programa de conceito (concept.cpp) mostra no lugar do jogo.
# Uso: python3 -I assets.py <pasta de saida>
#   thumb_<id>.rgba     miniaturas 256x256 (camera, luz e fundo das miniaturas do render.cpp)
#   viewer_<id>.rgba    o visualizador do card (1024, com a grade; _col = com a colisao)
#   tex_<nome>.rgba     as texturas dos cards abertos
#   cena.rgb            a rua de fundo (1920x1080)
#   conceito.txt        o catalogo, as malhas, as texturas, a camera da cena e o que esta na mira
import math
import os
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import models  # noqa: E402
import raster  # noqa: E402
from raster import Mesh, box, oquad, rot_z  # noqa: E402

OUT = sys.argv[1] if len(sys.argv) > 1 else 'out'
os.makedirs(OUT, exist_ok=True)
T = models.make_textures()


def save_rgba(path, img):
    img = img.convert('RGBA')
    with open(path, 'wb') as f:
        f.write(struct.pack('<ii', *img.size))
        f.write(img.tobytes())


def tex_image(name):
    a = T[name]
    if a.shape[-1] == 3:
        a = np.concatenate([a, np.ones(a.shape[:2] + (1,), np.float32)], -1)
    return Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA')


def stats(m):
    keys = set()
    verts = set()
    for v, c, t in zip(m.v, m.c, m.t):
        keys.add((tuple(np.round(c, 3)), t))
        for p in v:
            verts.add(tuple(np.round(p, 4)))
    return len(verts), len(m.v), len(keys)


lines = []
built = {}
for mid, dff, txd, kind, build in models.MODELS:
    m = build()
    built[mid] = m
    lo, hi = m.bounds()
    cam = raster.fit_camera(lo, hi, fov=28)
    tg = raster.Target(256, 256, (20, 20, 20), ssaa=2)
    tg.draw(m, cam, T)
    save_rgba(os.path.join(OUT, 'thumb_%d.rgba' % mid), tg.image())
    nv, nt, nm = stats(m)
    lines.append('malha|%d|%d|%d|%d|%d|%s' % (mid, nv, nt, nm, getattr(m, 'atomics', 1),
                                             '|'.join('%.3f' % x for x in (*lo, *hi))))

# ---------------------------------------------------------------- catalogo
IDE = {models.PED: 'DATA\\PEDS.IDE', models.WEAP: 'DATA\\DEFAULT.IDE', models.VEH: 'DATA\\VEHICLES.IDE'}
TYPE = {models.PED: 7, models.WEAP: 4, models.VEH: 6, models.OBJ: 1}
SECTION = {models.PED: 'peds', models.WEAP: 'weap', models.VEH: 'cars', models.OBJ: 'objs'}
for mid, dff, txd, kind, build in models.MODELS:
    ide = IDE.get(kind)
    img = 'gta3.img'
    dd = 0
    if kind == models.OBJ:
        ide = 'DATA\\MAPS\\VEGEPART.IDE' if mid < 1000 else 'DATA\\MAPS\\GENERIC\\DYNAMIC.IDE'
        dd = 150 if mid < 1000 else 100
        if mid >= 18000:
            ide, img, dd = 'SAMP\\SAMP.IDE', 'samp.img', 300
    nv, nt, nm = stats(built[mid])
    size = max(2, nt // 18 + (40 if kind == models.VEH else 12 if kind == models.PED else 0))
    replaced = 1 if mid == 411 else 0
    lines.append('entrada|%d|%d|%d|%s|%s|%s|%s|%s|%d|%d|%d|%d|%s' % (
        mid, TYPE[kind], kind, SECTION[kind], dff, txd, ide, img, size, 0, dd, replaced, ''))
# o resto do jogo (para a contagem): ids altos que nao aparecem na primeira tela
rng = np.random.default_rng(11)
filler = 0
for mid in range(3000, 18000):
    if rng.random() < 0.93:
        img = 'gta_int.img' if 14000 <= mid < 15000 else 'gta3.img'
        lines.append('entrada|%d|1|0|objs|obj_%05d|txd_%04d|DATA\\MAPS\\MAP.IDE|%s|8|0|200|0|' % (mid, mid, mid // 12, img))
        filler += 1
for mid in range(18631, 19999):
    if rng.random() < 0.95:
        lines.append('entrada|%d|1|0|objs|samp_%05d|samp|SAMP\\SAMP.IDE|samp.img|4|0|300|0|' % (mid, mid))

# ---------------------------------------------------------------- cards abertos: visualizador e texturas
VIEW_TEX = {
    411: [('vehiclelights128', 1), ('vehicletyres128', 1), ('vehiclegeneric256', 1), ('vehiclelightson128', 0),
          ('vehiclegrunge256', 0)],
    621: [('palm_leaves', 1), ('palm_trunk', 1)],
}
FORMAT = {'palm_leaves': 'DXT3', 'vehiclelights128': 'DXT1', 'vehicletyres128': 'DXT1', 'vehiclegeneric256': 'DXT1',
          'vehiclelightson128': 'DXT1', 'vehiclegrunge256': 'DXT1', 'palm_trunk': 'DXT1'}
for mid, texs in VIEW_TEX.items():
    m = built[mid]
    lo, hi = m.bounds()
    for col in (False, True):
        cam = raster.fit_camera(lo, hi, fov=34, yaw=35 if mid != 411 else 40, pitch=18)
        tg = raster.Target(1024, 1024, (20, 20, 20), ssaa=2)
        segs = raster.grid_segments(lo, hi, cam.radius)
        tg.lines(cam, [s for s, strong in segs if not strong], (255, 255, 255, 0x16), 1.0)
        tg.lines(cam, [s for s, strong in segs if strong], (255, 255, 255, 0x30), 1.0)
        tg.draw(m, cam, T)
        if col and hasattr(m, 'col_spheres'):
            segs = []
            for c, r in m.col_spheres:
                c = np.asarray(c, float)
                for axis in range(3):
                    pts = []
                    for k in range(25):
                        a = 2 * math.pi * k / 24
                        p = np.zeros(3)
                        p[(axis + 1) % 3], p[(axis + 2) % 3] = math.cos(a) * r, math.sin(a) * r
                        pts.append(c + p)
                    segs += list(zip(pts, pts[1:]))
            for a, b in m.col_boxes:
                xs, ys, zs = (a[0], b[0]), (a[1], b[1]), (a[2], b[2])
                corners = [(xs[i & 1], ys[(i >> 1) & 1], zs[(i >> 2) & 1]) for i in range(8)]
                for i in range(8):
                    for bit in (1, 2, 4):
                        if not i & bit:
                            segs.append((corners[i], corners[i | bit]))
            tg.lines(cam, segs, (255, 154, 60, 0xD0), 1.4)
        save_rgba(os.path.join(OUT, 'viewer_%d%s.rgba' % (mid, '_col' if col else '')), tg.image())
    for name, used in texs:
        img = tex_image(name)
        save_rgba(os.path.join(OUT, 'tex_%s.rgba' % name), img)
        alpha = 1 if T[name].shape[-1] == 4 else 0
        levels = int(math.log2(max(img.size))) + 1
        lines.append('textura|%d|%s|%d|%d|%d|%d|%s|%d' % (mid, name, img.size[0], img.size[1], used, alpha,
                                                        FORMAT.get(name, 'DXT1'), levels))
    if hasattr(m, 'col_spheres'):
        lines.append('colisao|%d|%d|%d|%d' % (mid, len(m.col_boxes), len(m.col_spheres), 0))

# ---------------------------------------------------------------- a rua (fundo e "Info ao apontar")
W, H = 1920, 1080
scene = Mesh()


def tiles(x0, x1, y0, y1, z, step, tex, uvs, color=(255, 255, 255)):
    for xa in np.arange(x0, x1, step):
        for ya in np.arange(y0, y1, step):
            xb, yb = min(x1, xa + step), min(y1, ya + step)
            scene.quad((xa, ya, z), (xb, ya, z), (xb, yb, z), (xa, yb, z), color,
                       ((xa * uvs, ya * uvs), (xb * uvs, ya * uvs), (xb * uvs, yb * uvs), (xa * uvs, yb * uvs)), tex)


tiles(-7, 7, -12, 140, 0.0, 3.5, 'asphalt', 0.25)
for x0, x1 in ((7, 11.5), (-11.5, -7)):
    tiles(x0, x1, -12, 140, 0.15, 4.5, 'sidewalk', 0.5)
for x in (7, -7):
    for ya in np.arange(-12, 140, 4.0):
        oquad(scene, (x, ya, 0), (x, ya + 4, 0), (x, ya + 4, 0.15), (x, ya, 0.15), (150, 146, 136), (0, ya + 2, 0.1))
for x in (-0.14, 0.14):
    for ya in np.arange(-12, 140, 6.0):
        scene.quad((x - 0.06, ya, 0.01), (x + 0.06, ya, 0.01), (x + 0.06, ya + 6, 0.01), (x - 0.06, ya + 6, 0.01), (220, 180, 40))
for x in (3.5, -3.5):
    for ya in np.arange(-12, 140, 9.0):
        scene.quad((x - 0.07, ya, 0.01), (x + 0.07, ya, 0.01), (x + 0.07, ya + 3, 0.01), (x - 0.07, ya + 3, 0.01), (225, 225, 225))

BUILD_R = [(-12, 8, 9, 'brick'), (8, 24, 15, 'concrete'), (24, 38, 7, 'brick'), (38, 60, 22, 'concrete'), (60, 84, 11, 'brick'),
           (84, 110, 28, 'concrete'), (110, 140, 14, 'brick')]
BUILD_L = [(-12, 12, 12, 'concrete'), (12, 30, 8, 'brick'), (30, 52, 18, 'concrete'), (52, 70, 9, 'brick'), (70, 100, 24, 'concrete'),
           (100, 140, 12, 'brick')]
for side, plan in ((1, BUILD_R), (-1, BUILD_L)):
    for y0, y1, h, tex in plan:
        x0, x1 = (11.5, 24) if side > 0 else (-24, -11.5)
        tint = (255, 255, 255) if tex == 'brick' else (255, 250, 240)
        box(scene, (x0, y0, 0.15), (x1, y1, h), tint, tex=tex, uvs=0.25, skip=('bottom',))
        xf = x0 - 0.02 if side > 0 else x1 + 0.02
        right = (0, -1, 0) if side > 0 else (0, 1, 0)
        for yw in np.arange(y0 + 1.6, y1 - 1.0, 3.0):
            for zw in np.arange(3.6, h - 1.4, 3.2):
                models.decal(scene, (xf, yw, zw), right, (0, 0, 1), 1.6, 1.9, 'window')
        if tex == 'brick' and y1 - y0 > 10:
            models.decal(scene, (xf, (y0 + y1) / 2, 1.5), right, (0, 0, 1), min(10, y1 - y0 - 2), 2.6, 'shopfront')
        box(scene, (x0 - 0.1 if side > 0 else x0, y0, h), (x1 if side > 0 else x1 + 0.1, y1, h + 0.4), (120, 116, 108))
box(scene, (-24, 140, 0), (24, 150, 30), (255, 255, 255), tex='concrete', uvs=0.25)


def place(mid, pos, heading=0.0, mesh=None):
    m = mesh if mesh is not None else built[mid]
    scene.add(m, rot_z(heading), pos)
    return m


for y in (6, 22, 38, 54, 70, 86):
    place(621, (10.3, y, 0.15), y * 0.7)
    place(621, (-10.3, y + 8, 0.15), y * 0.3)
for y in (14, 34, 54, 74):
    place(1297, (7.7, y, 0.15), math.pi / 2)
    place(1297, (-7.7, y + 10, 0.15), -math.pi / 2)
place(1280, (10.7, 18.0, 0.15), math.pi / 2)
place(1337, (8.0, 25.5, 0.15))
place(1211, (7.6, 9.0, 0.15))
place(1302, (11.05, 29.5, 0.15), math.pi / 2)
place(1216, (8.0, 43.0, 0.15), math.pi / 2)
place(1238, (6.4, 3.0, 0.0))
place(1238, (6.4, 4.3, 0.0))

# carros: o alvo (sultan) parado na guia da direita
TARGET = (560, (5.55, 16.0, 0.0), 0.0)
cars = [TARGET, (420, (5.55, 30.5, 0.0), 0.0), (415, (-3.5, 26.0, 0.0), math.pi), (400, (-5.5, 11.0, 0.0), math.pi),
        (431, (-3.6, 58.0, 0.0), math.pi), (405, (3.5, 46.0, 0.0), 0.0)]
for mid, pos, hd in cars:
    m = built[mid]
    lo, hi = m.bounds()
    sh = Mesh()
    sh.quad((lo[0] - 0.05, lo[1] - 0.05, 0.012), (hi[0] + 0.05, lo[1] - 0.05, 0.012), (hi[0] + 0.05, hi[1] + 0.05, 0.012),
            (lo[0] - 0.05, hi[1] + 0.05, 0.012), (34, 34, 36))
    place(mid, pos, hd, sh)
    place(mid, pos, hd)

# o jogador (CJ de regata), de costas para a camera, a esquerda da mira
cj = models.ped((236, 236, 232), (60, 86, 140), (110, 72, 50), (16, 16, 16), tank=True, relaxed=True)
place(0, (1.45, 0.4, 0.0), 0.0, cj)

eye = np.array((2.3, -5.6, 2.45))
target_pos = np.array(TARGET[1])
tm = built[560]
tlo, thi = tm.bounds()
aim = target_pos + (tlo + thi) / 2
cam = raster.Camera(eye, aim, 52.0, W / H, 0.3, 400.0)
cam.light = raster.normalize((0.45, 0.65, -0.62))

sky = np.zeros((H * 2, W * 2, 3), np.float32)
t = np.linspace(0, 1, H * 2)[:, None]
top, hor = np.array((84, 140, 206)) / 255, np.array((206, 216, 226)) / 255
sky[:] = top[None, None] * (1 - t[..., None] ** 0.8) + hor[None, None] * t[..., None] ** 0.8
tg = raster.Target(W, H, (sky * 255).astype(np.uint8), ssaa=2)
tg.draw(scene, cam, T, ambient=0.5, diffuse=0.62, fog=(35.0, 170.0, (0.80, 0.84, 0.88), 0.85))
img = tg.image()
img.save(os.path.join(OUT, 'cena.png'))
with open(os.path.join(OUT, 'cena.rgb'), 'wb') as f:
    f.write(img.convert('RGB').tobytes())

# depois do "Criar no mundo (1:1)" no card do infernus: na frente do jogador, de frente para ele (como o core.cpp faz)
inf = built[411]
ilo, ihi = inf.bounds()
radius_xy = math.hypot(max(abs(ilo[0]), abs(ihi[0])), max(abs(ilo[1]), abs(ihi[1])))
spawn_dist = max(2.5, radius_xy + 1.5)
sh = Mesh()
sh.quad((ilo[0] - 0.05, ilo[1] - 0.05, 0.012), (ihi[0] + 0.05, ilo[1] - 0.05, 0.012), (ihi[0] + 0.05, ihi[1] + 0.05, 0.012),
        (ilo[0] - 0.05, ihi[1] + 0.05, 0.012), (34, 34, 36))
place(411, (1.45, 0.4 + spawn_dist, 0.0), math.pi, sh)
place(411, (1.45, 0.4 + spawn_dist, 0.0), math.pi)
tg = raster.Target(W, H, (sky * 255).astype(np.uint8), ssaa=2)
tg.draw(scene, cam, T, ambient=0.5, diffuse=0.62, fog=(35.0, 170.0, (0.80, 0.84, 0.88), 0.85))
img = tg.image()
img.save(os.path.join(OUT, 'cena_criado.png'))
with open(os.path.join(OUT, 'cena_criado.rgb'), 'wb') as f:
    f.write(img.convert('RGB').tobytes())
lines.append('criado|411|%.2f' % spawn_dist)

M = cam.proj @ cam.view
lines.append('camera|' + '|'.join('%.6f' % x for x in M.flatten()))
hit = target_pos + np.array((tlo[0], (tlo[1] + thi[1]) * 0.3, (tlo[2] + thi[2]) * 0.55))
dist = float(np.linalg.norm(hit - eye))
lines.append('mira|560|2|0|-1|%s|%s|%.3f|%s' % ('|'.join('%.3f' % x for x in target_pos), '|'.join('%.3f' % x for x in hit), dist,
                                               '|'.join('%.3f' % x for x in (*tlo, *thi))))
with open(os.path.join(OUT, 'conceito.txt'), 'w') as f:
    f.write('\n'.join(lines) + '\n')
print('ok: %d modelos, %d entradas a mais, cena %dx%d' % (len(models.MODELS), filler, W, H))

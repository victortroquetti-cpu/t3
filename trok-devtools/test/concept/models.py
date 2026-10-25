# Imagens de conceito do Trok DevTools: os modelos de mentira (desenhos low-poly feitos aqui, no estilo do SA) e as
# texturas deles. Os ids e nomes sao os do jogo; a geometria e as texturas nao.
import math

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

from raster import Mesh, box, cylinder, extrude, loft, normalize, oquad, rot_x, rot_y, rot_z, sphere

FONT = '/usr/share/fonts/opentype/inter/Inter-ExtraBold.otf'

# ---------------------------------------------------------------- texturas

TEXTURES = {}


def _arr(img):
    a = np.asarray(img, dtype=np.float32) / 255.0
    return a


def _noise(w, h, seed, scale=1.0):
    r = np.random.default_rng(seed)
    return r.random((h, w)).astype(np.float32) * scale


def make_textures():
    T = {}
    rng = np.random.default_rng(3)

    # vehiclelights128: em cima o farol (lente clara com refletor), embaixo a lanterna (vermelha)
    im = Image.new('RGB', (128, 128), (40, 40, 44))
    d = ImageDraw.Draw(im)
    d.rectangle([2, 2, 125, 61], fill=(230, 232, 236))
    for k in range(6):
        d.ellipse([10 + k * 3, 8 + k * 2, 118 - k * 3, 56 - k * 2], outline=(200 - k * 12, 205 - k * 12, 215 - k * 10))
    d.ellipse([46, 18, 82, 46], fill=(255, 255, 250))
    d.rectangle([2, 66, 125, 125], fill=(150, 18, 20))
    for y in range(70, 124, 6):
        d.line([4, y, 123, y], fill=(185, 30, 32))
    d.rectangle([84, 70, 121, 121], fill=(235, 140, 40))
    T['vehiclelights128'] = _arr(im)

    im = Image.new('RGB', (128, 128), (60, 60, 60))
    d = ImageDraw.Draw(im)
    d.rectangle([2, 2, 125, 61], fill=(255, 250, 220))
    d.ellipse([30, 10, 98, 54], fill=(255, 255, 255))
    d.rectangle([2, 66, 125, 125], fill=(255, 60, 50))
    d.rectangle([84, 70, 121, 121], fill=(255, 190, 60))
    T['vehiclelightson128'] = _arr(im)

    # vehicletyres128: metade esquerda o pneu (sulcos), metade direita a roda (raios)
    im = Image.new('RGB', (128, 128), (26, 26, 28))
    d = ImageDraw.Draw(im)
    for y in range(0, 128, 8):
        d.polygon([(4, y), (28, y + 4), (4, y + 6)], fill=(14, 14, 15))
        d.polygon([(60, y), (36, y + 4), (60, y + 6)], fill=(14, 14, 15))
    d.ellipse([66, 2, 126, 62], fill=(30, 30, 32))
    d.ellipse([72, 8, 120, 56], fill=(170, 172, 178))
    for k in range(5):
        a = k * 2 * math.pi / 5
        d.polygon([(96, 32), (96 + 22 * math.cos(a - 0.22), 32 + 22 * math.sin(a - 0.22)),
                   (96 + 22 * math.cos(a + 0.22), 32 + 22 * math.sin(a + 0.22))], fill=(205, 207, 212))
    d.ellipse([90, 26, 102, 38], fill=(120, 120, 126))
    T['vehicletyres128'] = _arr(im)

    # vehiclegeneric256: grade do radiador, interior e placa
    im = Image.new('RGB', (256, 256), (34, 34, 36))
    d = ImageDraw.Draw(im)
    for x in range(0, 128, 8):
        d.line([x, 0, x, 127], fill=(12, 12, 13), width=3)
    for y in range(0, 128, 8):
        d.line([0, y, 127, y], fill=(12, 12, 13), width=3)
    d.rectangle([128, 0, 255, 127], fill=(52, 50, 48))
    d.rectangle([140, 20, 243, 60], fill=(70, 66, 62))
    d.rectangle([0, 160, 255, 223], fill=(236, 236, 230))
    d.text((40, 170), 'GRANTH', fill=(30, 40, 120), font=ImageFont.truetype(FONT, 40))
    T['vehiclegeneric256'] = _arr(im)

    g = (rng.random((256, 256)) * 60 + 80).astype(np.uint8)
    im = Image.fromarray(g).filter(ImageFilter.GaussianBlur(2)).convert('RGB')
    T['vehiclegrunge256'] = _arr(im)

    # palmeira: folha com alfa e tronco com aneis
    im = Image.new('RGBA', (128, 256), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.line([64, 0, 64, 255], fill=(96, 120, 40, 255), width=5)
    for y in range(6, 250, 7):
        t = y / 255
        L = 58 * math.sin(math.pi * min(1, t * 1.1)) + 6
        col = (int(60 + 50 * t), int(118 + 30 * t), int(34 + 10 * t), 255)
        d.polygon([(64, y), (64 - L, y + 16), (64 - L + 6, y + 19), (64, y + 6)], fill=col)
        d.polygon([(64, y), (64 + L, y + 16), (64 + L - 6, y + 19), (64, y + 6)], fill=col)
    T['palm_leaves'] = _arr(im)

    im = Image.new('RGB', (64, 128), (120, 96, 70))
    d = ImageDraw.Draw(im)
    for y in range(0, 128, 10):
        d.line([0, y, 63, y + 3], fill=(84, 66, 48), width=3)
        d.line([0, y + 5, 63, y + 6], fill=(138, 112, 84), width=1)
    T['palm_trunk'] = _arr(im)

    g = rng.random((64, 64))
    im = Image.fromarray((np.stack([40 + g * 40, 90 + g * 60, 30 + g * 25], -1)).astype(np.uint8))
    T['leaves'] = _arr(im.filter(ImageFilter.GaussianBlur(0.6)))

    im = Image.new('RGB', (64, 64), (96, 74, 52))
    d = ImageDraw.Draw(im)
    for x in range(0, 64, 9):
        d.line([x, 0, x + 2, 63], fill=(70, 52, 36), width=2)
    T['bark'] = _arr(im)

    # tijolo, concreto, asfalto, calcada, madeira, papelao, metal
    im = Image.new('RGB', (128, 128), (150, 70, 52))
    d = ImageDraw.Draw(im)
    for r in range(0, 128, 16):
        d.line([0, r, 127, r], fill=(190, 180, 168), width=2)
        off = 0 if (r // 16) % 2 == 0 else 16
        for c in range(off, 128, 32):
            d.line([c, r, c, r + 16], fill=(190, 180, 168), width=2)
    a = _arr(im) * (0.85 + 0.15 * _noise(128, 128, 5))[..., None]
    T['brick'] = a

    g = 0.6 + 0.12 * _noise(128, 128, 6)
    T['concrete'] = np.stack([g * 0.98, g * 0.96, g * 0.92], -1)
    g = 0.22 + 0.08 * _noise(128, 128, 7)
    T['asphalt'] = np.stack([g, g, g * 1.04], -1)
    im = Image.new('RGB', (64, 64), (178, 172, 160))
    d = ImageDraw.Draw(im)
    d.line([0, 0, 63, 0], fill=(130, 126, 118), width=2)
    d.line([0, 0, 0, 63], fill=(130, 126, 118), width=2)
    T['sidewalk'] = _arr(im) * (0.9 + 0.1 * _noise(64, 64, 8))[..., None]

    im = Image.new('RGB', (128, 64), (150, 104, 60))
    d = ImageDraw.Draw(im)
    for y in range(0, 64, 16):
        d.line([0, y, 127, y], fill=(96, 64, 34), width=2)
        for k in range(5):
            yy = y + 3 + k * 2.5
            d.line([0, yy, 127, yy + rng.integers(-2, 3)], fill=(132, 90, 50), width=1)
    T['wood'] = _arr(im)

    im = Image.new('RGB', (64, 64), (176, 136, 86))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 26, 63, 37], fill=(196, 168, 120))
    T['cardboard'] = _arr(im)

    g = 0.55 + 0.1 * _noise(64, 64, 9)
    T['metal'] = np.stack([g, g * 1.01, g * 1.04], -1)

    # maquina de refrigerante: frente com a vitrine e os botoes
    im = Image.new('RGB', (128, 256), (176, 24, 30))
    d = ImageDraw.Draw(im)
    d.rectangle([10, 14, 84, 170], fill=(30, 34, 40))
    for r in range(4):
        for c in range(3):
            x, y = 16 + c * 22, 22 + r * 37
            d.rectangle([x, y, x + 16, y + 28], fill=[(230, 60, 40), (60, 170, 70), (240, 200, 60)][(r + c) % 3])
    for k in range(6):
        d.rectangle([94, 30 + k * 18, 116, 42 + k * 18], fill=(230, 230, 230))
    d.rectangle([20, 196, 108, 230], fill=(20, 20, 22))
    d.text((18, 174), 'SPRUNK', fill=(255, 255, 255), font=ImageFont.truetype(FONT, 18))
    T['vend_front'] = _arr(im)

    im = Image.new('RGB', (256, 64), (20, 22, 30))
    d = ImageDraw.Draw(im)
    d.text((14, 6), 'SA-MP', fill=(255, 140, 30), font=ImageFont.truetype(FONT, 46))
    T['samp_logo'] = _arr(im)

    im = Image.new('RGB', (64, 64), (34, 92, 54))
    d = ImageDraw.Draw(im)
    for x in range(0, 64, 8):
        d.line([x, 0, x, 63], fill=(26, 74, 42), width=3)
    T['bin_nt'] = _arr(im)

    im = Image.new('RGB', (64, 16), (255, 255, 255))
    d = ImageDraw.Draw(im)
    for x in range(-16, 64, 16):
        d.polygon([(x, 16), (x + 8, 16), (x + 16, 0), (x + 8, 0)], fill=(230, 90, 20))
    T['barrier'] = _arr(im)

    im = Image.new('RGB', (64, 64), (255, 255, 255))
    d = ImageDraw.Draw(im)
    for k in range(8):
        a0, a1 = k * math.pi / 4, (k + 0.5) * math.pi / 4
        d.polygon([(32, 32), (32 + 46 * math.cos(a0), 32 + 46 * math.sin(a0)), (32 + 46 * math.cos(a1), 32 + 46 * math.sin(a1))],
                  fill=(200, 30, 30))
    T['umbrella'] = _arr(im)

    im = Image.new('RGB', (64, 64), (240, 200, 30))
    d = ImageDraw.Draw(im)
    d.polygon([(32, 8), (58, 54), (6, 54)], outline=(20, 20, 20), width=4)
    d.rectangle([29, 22, 35, 40], fill=(20, 20, 20))
    d.rectangle([29, 44, 35, 49], fill=(20, 20, 20))
    T['hazard'] = _arr(im)

    # janelas de predio (vidro com moldura) e a vitrine da loja
    im = Image.new('RGB', (64, 64), (150, 140, 128))
    d = ImageDraw.Draw(im)
    d.rectangle([10, 8, 54, 50], fill=(60, 80, 100))
    d.rectangle([10, 8, 54, 20], fill=(100, 130, 156))
    d.line([32, 8, 32, 50], fill=(150, 140, 128), width=3)
    T['window'] = _arr(im)

    im = Image.new('RGB', (128, 64), (40, 50, 62))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 127, 10], fill=(200, 60, 40))
    d.text((8, 0), '24-7', fill=(255, 255, 255), font=ImageFont.truetype(FONT, 10))
    for x in range(0, 128, 32):
        d.line([x, 10, x, 63], fill=(150, 150, 150), width=2)
    T['shopfront'] = _arr(im)
    return T


# ---------------------------------------------------------------- ajudantes


def decal(m, center, right, up, w, h, tex, uv=(0, 0, 1, 1), color=(255, 255, 255)):
    """Retangulo w x h no plano (right, up) em volta de center, com a regiao uv da textura."""
    c = np.asarray(center, float)
    r = normalize(right) * w / 2
    u = normalize(up) * h / 2
    u0, v0, u1, v1 = uv
    m.quad(c - r - u, c + r - u, c + r + u, c - r + u, color, ((u0, v1), (u1, v1), (u1, v0), (u0, v0)), tex)


def disc(m, center, normal, radius, color, tex=None, uv_center=(0.5, 0.5), uv_r=0.5, seg=16):
    n = normalize(normal)
    a = normalize(np.cross(n, (0, 0, 1) if abs(n[2]) < 0.9 else (1, 0, 0)))
    b = np.cross(n, a)
    c = np.asarray(center, float)
    for i in range(seg):
        t0, t1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
        p0 = c + (a * math.cos(t0) + b * math.sin(t0)) * radius
        p1 = c + (a * math.cos(t1) + b * math.sin(t1)) * radius
        uv = (uv_center, (uv_center[0] + uv_r * math.cos(t0), uv_center[1] + uv_r * math.sin(t0)),
              (uv_center[0] + uv_r * math.cos(t1), uv_center[1] + uv_r * math.sin(t1)))
        tri_n = np.cross(p0 - c, p1 - c)
        if tri_n @ n < 0:
            m.tri(c, p1, p0, color, uv=(uv[0], uv[2], uv[1]), tex=tex)
        else:
            m.tri(c, p0, p1, color, uv=uv, tex=tex)


def tube(m, a, b, r, color, seg=8, r_end=None, tex=None, caps=(True, True)):
    """Cilindro entre dois pontos."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    d = b - a
    L = np.linalg.norm(d)
    z = d / L
    x = normalize(np.cross(z, (0, 0, 1) if abs(z[2]) < 0.95 else (1, 0, 0)))
    y = np.cross(z, x)
    R = np.stack([x, y, z], axis=1)
    cylinder(m, a, r, L, color, seg, r_top=r_end, rot=R, tex=tex, caps=caps)


def torus(m, center, R, r, color, seg=20, sides=8, tex=None):
    cx, cy, cz = center
    for i in range(seg):
        for j in range(sides):
            pts, nrm, uvs = [], [], []
            for di, dj in ((0, 0), (1, 0), (1, 1), (0, 1)):
                u = 2 * math.pi * (i + di) / seg
                v = 2 * math.pi * (j + dj) / sides
                nx, ny, nz = math.cos(v) * math.cos(u), math.cos(v) * math.sin(u), math.sin(v)
                pts.append((cx + (R + r * math.cos(v)) * math.cos(u), cy + (R + r * math.cos(v)) * math.sin(u), cz + r * nz))
                nrm.append((nx, ny, nz))
                uvs.append(((i + di) / seg * 4 * 0.5, (j + dj) / sides))
            m.quad(pts[0], pts[1], pts[2], pts[3], color, uvs, tex, False, nrm)


# ---------------------------------------------------------------- veiculos

GLASS = (24, 30, 38)
TIRE = (255, 255, 255)
DARK = (28, 28, 30)

# perfis (y, z de baixo, z de cima, meia largura) da frente (+y) para tras
STYLES = {
    'sports': dict(L=4.5, W=1.96, wr=0.33, wy=(1.38, -1.42), body=[(2.25, .34, .50, .82), (2.05, .26, .62, .93), (1.4, .22, .76, .98),
                                                                      (0.5, .22, .84, .98), (-1.0, .22, .90, .98), (-1.9, .24, .88, .96), (-2.2, .30, .80, .90)],
                   cab=[(0.75, .82), (0.15, 1.14), (-0.75, 1.17), (-1.65, .89)], cab_w=(.86, .62)),
    'roadster': dict(L=4.3, W=1.9, wr=0.33, wy=(1.3, -1.35), body=[(2.15, .34, .52, .80), (1.95, .26, .64, .92), (1.2, .22, .78, .95),
                                                                     (0.0, .22, .84, .95), (-1.2, .22, .86, .95), (-2.1, .30, .80, .88)],
                     cab=[(0.45, .82), (0.05, 1.08), (-0.6, 1.10), (-1.0, .86)], cab_w=(.82, .70)),
    'sedan': dict(L=4.6, W=1.88, wr=0.34, wy=(1.42, -1.4), body=[(2.3, .38, .70, .84), (2.12, .30, .86, .93), (1.2, .28, .92, .94),
                                                                  (-1.3, .28, .94, .94), (-2.1, .32, .90, .92), (-2.3, .40, .82, .86)],
                  cab=[(0.95, .92), (0.35, 1.40), (-0.9, 1.42), (-1.55, .95)], cab_w=(.88, .72)),
    'muscle': dict(L=4.9, W=1.95, wr=0.36, wy=(1.5, -1.55), body=[(2.45, .36, .78, .88), (2.25, .30, .92, .96), (1.0, .28, .98, .97),
                                                                    (-1.4, .28, .98, .97), (-2.3, .32, .94, .95), (-2.45, .40, .84, .90)],
                   cab=[(0.7, .98), (0.1, 1.38), (-0.9, 1.40), (-1.6, 1.0)], cab_w=(.90, .74)),
    'suv': dict(L=4.7, W=2.0, wr=0.42, wy=(1.45, -1.45), body=[(2.35, .50, .96, .90), (2.15, .42, 1.12, .98), (1.2, .40, 1.18, 1.0),
                                                                 (-2.2, .40, 1.20, 1.0), (-2.35, .48, 1.14, .96)],
                cab=[(1.05, 1.16), (0.55, 1.82), (-2.15, 1.84), (-2.32, 1.20)], cab_w=(.94, .86)),
    'van': dict(L=5.0, W=2.0, wr=0.38, wy=(1.6, -1.55), body=[(2.5, .40, .90, .92), (2.3, .34, 1.06, .98), (1.6, .32, 1.14, 1.0),
                                                                (-2.35, .32, 1.16, 1.0), (-2.5, .40, 1.12, .96)],
                cab=[(1.75, 1.12), (1.15, 2.08), (-2.42, 2.10), (-2.5, 1.14)], cab_w=(.98, .94), boxy=True),
    'small': dict(L=3.9, W=1.75, wr=0.31, wy=(1.18, -1.2), body=[(1.95, .36, .66, .78), (1.78, .28, .80, .86), (1.0, .26, .86, .87),
                                                                   (-1.6, .28, .88, .87), (-1.95, .36, .82, .82)],
                  cab=[(0.75, .86), (0.2, 1.36), (-1.05, 1.38), (-1.55, .90)], cab_w=(.82, .68)),
}


def _interp(prof, y):
    ys = [p[0] for p in prof]
    if y >= ys[0]:
        return prof[0][1:]
    if y <= ys[-1]:
        return prof[-1][1:]
    for i in range(len(prof) - 1):
        if ys[i] >= y >= ys[i + 1]:
            t = (ys[i] - y) / (ys[i] - ys[i + 1])
            return tuple(a + (b - a) * t for a, b in zip(prof[i][1:], prof[i + 1][1:]))


def _ring(y, zb, zt, hw):
    h = zt - zb
    return [(hw * .93, y, zb), (-hw * .93, y, zb), (-hw, y, zb + .2 * h), (-hw, y, zb + .7 * h), (-hw * .9, y, zt),
            (hw * .9, y, zt), (hw, y, zb + .7 * h), (hw, y, zb + .2 * h)]


def wheel(m, x, y, wr, tw, right):
    a = math.pi / 2 if right else -math.pi / 2
    base = (x - tw / 2 if right else x + tw / 2, y, wr)
    cylinder(m, base, wr, tw, (60, 60, 64), seg=16, rot=rot_y(a), tex='vehicletyres128', uv_u=0.48, uv_v=1.0,
             caps=(False, False))
    out = (x + tw / 2 + 0.002 if right else x - tw / 2 - 0.002, y, wr)
    disc(m, out, (1 if right else -1, 0, 0), wr, (255, 255, 255), 'vehicletyres128', (0.75, 0.25), 0.235)
    inner = (x - tw / 2 if right else x + tw / 2, y, wr)
    disc(m, inner, (-1 if right else 1, 0, 0), wr, DARK)


def vehicle(style, paint, accent=None, stripe=None, lightbar=False, roofsign=None):
    S = STYLES[style]
    m = Mesh()
    hw_max = S['W'] / 2
    wr = S['wr']
    # o corpo: perfil base + os arcos das rodas
    ys = set(p[0] for p in S['body'])
    R = wr + 0.07
    for wy in S['wy']:
        for k in range(-6, 7):
            ys.add(wy + R * k / 6 * 1.02)
    rings = []
    for y in sorted(ys, reverse=True):
        zb, zt, hwf = _interp(S['body'], y)
        for wy in S['wy']:
            dy = y - wy
            if abs(dy) < R:
                zb = max(zb, wr + math.sqrt(R * R - dy * dy) * 0.95)
        zb = min(zb, zt - 0.12)
        rings.append(_ring(y, zb, zt, hwf * hw_max))
    under = tuple(int(c * 0.35) for c in paint)
    loft(m, rings, paint, colors=[under, paint, paint, paint, paint, paint, paint, paint], cap_colors=(paint, paint))
    # cabine: para-brisa, teto e vigia
    cab = S['cab']
    wb, wt = S['cab_w']
    cr = []
    for y, zt in cab:
        zb = _interp(S['body'], y)[1] - 0.03
        cr.append([(wb * hw_max, y, zb), (-wb * hw_max, y, zb), (-wt * hw_max, y, zt), (wt * hw_max, y, zt)])
    roof = paint
    boxy = S.get('boxy')
    for i in range(len(cr) - 1):
        top = GLASS if (i == 0 or i == len(cr) - 2) and not boxy else roof
        if boxy and i == len(cr) - 2:
            top = roof
        side = GLASS if not boxy else paint
        loft(m, [cr[i], cr[i + 1]], GLASS, colors=[None, side, top, side], cap_start=False, cap_end=False)
    if boxy:
        # janelas laterais da van como faixa de vidro
        for sx in (1, -1):
            x = sx * (wb * hw_max + 0.006)
            for y0, y1 in ((cab[1][0] - 0.05, cab[1][0] - 0.95), (cab[1][0] - 1.15, cab[1][0] - 2.4)):
                z0, z1 = 1.35, 1.80
                oquad(m, (x, y0, z0), (x, y1, z0), (x, y1, z1), (x, y0, z1), GLASS, (0, (y0 + y1) / 2, 1.5))
        # para-brisa da van
        y0, z0 = cab[0]
        y1, z1 = cab[1]
        oquad(m, (wb * hw_max * .96, y0 + 0.01, z0 + 0.05), (-wb * hw_max * .96, y0 + 0.01, z0 + 0.05),
              (-wt * hw_max * .96, y1 + 0.01, z1 - 0.06), (wt * hw_max * .96, y1 + 0.01, z1 - 0.06), GLASS, (0, 0, 1.0))
    # farois e lanternas
    yf = S['body'][0][0]
    zbf, ztf, hwf = _interp(S['body'], yf)
    zl = zbf + (ztf - zbf) * 0.62
    for sx in (1, -1):
        decal(m, (sx * hwf * hw_max * 0.62, yf + 0.006, zl), (-sx, 0, 0), (0, 0, 1), hwf * hw_max * 0.48, (ztf - zbf) * 0.42,
              'vehiclelights128', (0.02, 0.02, 0.98, 0.48))
    yr = S['body'][-1][0]
    zbr, ztr, hwr = _interp(S['body'], yr)
    zl = zbr + (ztr - zbr) * 0.6
    for sx in (1, -1):
        decal(m, (sx * hwr * hw_max * 0.62, yr - 0.006, zl), (sx, 0, 0), (0, 0, 1), hwr * hw_max * 0.5, (ztr - zbr) * 0.36,
              'vehiclelights128', (0.02, 0.52, 0.98, 0.98))
    if style in ('sedan', 'muscle', 'suv', 'van', 'small'):
        decal(m, (0, yf + 0.007, zbf + (ztf - zbf) * 0.35), (-1, 0, 0), (0, 0, 1), hwf * hw_max * 0.7, (ztf - zbf) * 0.34,
              'vehiclegeneric256', (0, 0, 0.5, 0.5))
    decal(m, (0, yr - 0.007, zbr + (ztr - zbr) * 0.28), (1, 0, 0), (0, 0, 1), 0.5, 0.11, 'vehiclegeneric256',
          (0, 0.63, 1, 0.87))
    if stripe:
        # faixa no capo e no teto (como uma pintura de corrida)
        sc = stripe
        prof = S['body']
        steps = np.linspace(yf - 0.1, prof[-1][0] + 0.1, 16)
        for i in range(len(steps) - 1):
            ya, yb = steps[i], steps[i + 1]
            za = _interp(prof, ya)[1] + 0.006
            zb_ = _interp(prof, yb)[1] + 0.006
            oquad(m, (0.18, ya, za), (-0.18, ya, za), (-0.18, yb, zb_), (0.18, yb, zb_), sc, (0, (ya + yb) / 2, 0))
    if accent:
        # portas de outra cor (policia): faixa no meio do corpo
        for sx in (1, -1):
            x = sx * (hw_max + 0.004)
            oquad(m, (x, 0.9, 0.42), (x, -1.3, 0.42), (x, -1.3, 0.78), (x, 0.9, 0.78), accent, (0, -0.2, 0.6))
    if lightbar:
        zr = max(z for _, z in cab)
        ym = (cab[1][0] + cab[2][0]) / 2
        box(m, (-0.55, ym - 0.12, zr), (0, ym + 0.12, zr + 0.1), (210, 30, 30))
        box(m, (0, ym - 0.12, zr), (0.55, ym + 0.12, zr + 0.1), (40, 70, 230))
    if roofsign:
        zr = max(z for _, z in cab)
        ym = (cab[1][0] + cab[2][0]) / 2
        box(m, (-0.3, ym - 0.08, zr), (0.3, ym + 0.08, zr + 0.18), roofsign)
    tw = 0.24 if style != 'suv' else 0.28
    for wy in S['wy']:
        for right in (True, False):
            x = (hw_max - tw / 2 - 0.02) * (1 if right else -1)
            wheel(m, x, wy, wr, tw, right)
    m.atomics = 7
    return m


def bus(paint=(236, 236, 230), band=(230, 120, 30)):
    m = Mesh()
    L, hw, wr = 12.0, 1.3, 0.5
    box(m, (-hw, -L / 2, 0.45), (hw, L / 2, 3.1), paint, skip=('bottom',))
    box(m, (-hw * .98, -L / 2, 0.3), (hw * .98, L / 2, 0.45), (40, 40, 40))
    for sx in (1, -1):
        x = sx * (hw + 0.005)
        oquad(m, (x, L / 2 - 1.6, 1.55), (x, -L / 2 + 0.4, 1.55), (x, -L / 2 + 0.4, 2.55), (x, L / 2 - 1.6, 2.55), GLASS, (0, 0, 1.7))
        oquad(m, (x, L / 2 - 0.2, 0.9), (x, -L / 2 + 0.2, 0.9), (x, -L / 2 + 0.2, 1.2), (x, L / 2 - 0.2, 1.2), band, (0, 0, 1.7))
    oquad(m, (hw * .95, L / 2 + 0.006, 1.3), (-hw * .95, L / 2 + 0.006, 1.3), (-hw * .95, L / 2 + 0.006, 2.8),
          (hw * .95, L / 2 + 0.006, 2.8), GLASS, (0, 0, 1.7))
    for sx in (1, -1):
        decal(m, (sx * 0.85, L / 2 + 0.008, 0.85), (-sx, 0, 0), (0, 0, 1), 0.45, 0.25, 'vehiclelights128', (0, 0, 1, .48))
        decal(m, (sx * 0.95, -L / 2 - 0.008, 1.0), (sx, 0, 0), (0, 0, 1), 0.3, 0.5, 'vehiclelights128', (0, .52, 1, .98))
    for wy in (L / 2 - 2.4, -L / 2 + 2.6):
        for right in (True, False):
            wheel(m, (hw - 0.17) * (1 if right else -1), wy, wr, 0.3, right)
    m.atomics = 7
    return m


def truck(paint, fire=False):
    m = Mesh()
    hw, wr = 1.25, 0.55
    # cabine na frente, chassi atras
    box(m, (-hw, 1.4, 0.7), (hw, 3.4, 3.0), paint, skip=('bottom',))
    oquad(m, (hw * .9, 3.406, 1.9), (-hw * .9, 3.406, 1.9), (-hw * .9, 3.406, 2.8), (hw * .9, 3.406, 2.8), GLASS, (0, 2, 2))
    for sx in (1, -1):
        x = sx * (hw + 0.005)
        oquad(m, (x, 3.2, 1.9), (x, 2.2, 1.9), (x, 2.2, 2.7), (x, 3.2, 2.7), GLASS, (0, 2.5, 2))
    decal(m, (0, 3.408, 1.2), (-1, 0, 0), (0, 0, 1), 1.6, 0.6, 'vehiclegeneric256', (0, 0, .5, .5))
    for sx in (1, -1):
        decal(m, (sx * 0.95, 3.41, 1.2), (-sx, 0, 0), (0, 0, 1), 0.36, 0.26, 'vehiclelights128', (0, 0, 1, .48))
    box(m, (-0.5, -3.6, 0.55), (0.5, 1.4, 0.85), (50, 50, 52))
    if fire:
        box(m, (-hw, -3.6, 0.85), (hw, 1.3, 2.4), paint)
        box(m, (-0.3, -3.2, 2.4), (0.3, 1.2, 2.6), (200, 200, 205))
        for y in np.arange(-3.0, 1.2, 0.35):
            box(m, (-0.3, y, 2.6), (0.3, y + 0.05, 2.66), (200, 200, 205))
        for sx in (1, -1):
            x = sx * (hw + 0.004)
            oquad(m, (x, 1.2, 1.2), (x, -3.5, 1.2), (x, -3.5, 1.35), (x, 1.2, 1.35), (235, 235, 235), (0, -1, 1.5))
    else:
        box(m, (-0.45, -0.2, 0.85), (0.45, 1.3, 1.0), (60, 60, 64))
        tube(m, (0.9, 1.35, 1.0), (0.9, 1.35, 3.4), 0.07, (180, 180, 186))
    for wy in (2.4, -1.4, -2.7):
        for right in (True, False):
            wheel(m, (hw - 0.2) * (1 if right else -1), wy, wr, 0.34, right)
    m.atomics = 8
    return m


def motorbike(paint, accent=(230, 230, 230)):
    m = Mesh()
    wr = 0.32
    for y in (0.72, -0.72):
        cylinder(m, (-0.06, y, wr), wr, 0.12, (60, 60, 64), 16, rot=rot_y(math.pi / 2), tex='vehicletyres128', uv_u=.48,
                 caps=(False, False))
        disc(m, (0.062, y, wr), (1, 0, 0), wr, (255, 255, 255), 'vehicletyres128', (0.75, 0.25), 0.235)
        disc(m, (-0.062, y, wr), (-1, 0, 0), wr, (255, 255, 255), 'vehicletyres128', (0.75, 0.25), 0.235)
    body = [[(0.13, y, z0), (-0.13, y, z0), (-0.1, y, z1), (0.1, y, z1)] for y, z0, z1 in
            ((0.82, 0.66, 0.8), (0.55, 0.5, 0.98), (0.1, 0.42, 0.86), (-0.45, 0.66, 0.84), (-0.85, 0.8, 0.9))]
    loft(m, body, paint, colors=[DARK, paint, paint, paint])
    box(m, (-0.13, -0.6, 0.86), (0.13, -0.05, 0.94), (24, 24, 26))
    tube(m, (0, 0.72, wr), (0, 0.52, 1.05), 0.035, (150, 150, 156))
    tube(m, (-0.36, 0.5, 1.05), (0.36, 0.5, 1.05), 0.02, (30, 30, 30))
    oquad(m, (0.1, 0.97, 0.66), (-0.1, 0.97, 0.66), (-0.08, 0.86, 0.86), (0.08, 0.86, 0.86), accent, (0, 0.7, 0.7))
    decal(m, (0, 0.982, 0.72), (-1, 0, 0), (0, 0, 1), 0.12, 0.08, 'vehiclelights128', (0, 0, 1, .48))
    tube(m, (0.12, -0.3, 0.42), (0.12, -0.95, 0.6), 0.045, (170, 170, 176))
    m.atomics = 5
    return m


def helicopter(paint, accent):
    m = Mesh()
    rings = []
    for y, zb, zt, hw in ((2.4, 1.2, 1.7, 0.2), (2.1, 0.85, 2.0, 0.6), (1.2, 0.7, 2.25, 0.82), (-0.6, 0.72, 2.25, 0.82),
                          (-1.4, 1.0, 2.1, 0.6), (-1.9, 1.4, 1.95, 0.25)):
        rings.append(_ring(y, zb, zt, hw))
    loft(m, rings, paint, colors=[accent, paint, paint, paint, paint, paint, paint, paint])
    for sx in (1, -1):
        x = sx * 0.83
        oquad(m, (x, 1.9, 1.5), (x, 0.6, 1.5), (x, 0.6, 2.1), (x, 1.9, 2.1), GLASS, (0, 1, 1.6))
    oquad(m, (0.5, 2.25, 1.35), (-0.5, 2.25, 1.35), (-0.4, 1.9, 2.05), (0.4, 1.9, 2.05), GLASS, (0, 1, 1.5))
    tube(m, (0, -1.8, 1.75), (0, -6.6, 2.05), 0.22, paint, 10, r_end=0.09)
    box(m, (-0.04, -6.8, 1.9), (0.04, -6.3, 2.9), accent)
    disc(m, (0.12, -6.55, 2.35), (1, 0, 0), 0.55, (40, 40, 40))
    cylinder(m, (0, 0.2, 2.25), 0.12, 0.35, (60, 60, 64), 8)
    for k in range(4):
        R = rot_z(k * math.pi / 2 + 0.3)
        b = Mesh()
        box(b, (-0.12, 0.1, -0.02), (0.12, 4.3, 0.02), (40, 40, 42))
        m.add(b, R, (0, 0.2, 2.62))
    for sx in (1, -1):
        tube(m, (sx * 0.85, 2.2, 0.1), (sx * 0.85, -1.6, 0.1), 0.05, (70, 70, 72))
        tube(m, (sx * 0.85, 1.2, 0.1), (sx * 0.6, 1.2, 0.75), 0.04, (70, 70, 72))
        tube(m, (sx * 0.85, -0.8, 0.1), (sx * 0.6, -0.8, 0.75), 0.04, (70, 70, 72))
    m.atomics = 9
    return m


# ---------------------------------------------------------------- personagens


def ped(shirt, pants, skin, hair, shoes=(30, 30, 32), helmet=None, vest=None, cap=None, bandana=None, tank=False, relaxed=False):
    m = Mesh()
    for sx in (1, -1):
        x = sx * 0.1
        box(m, (x - 0.07, -0.08, 0.0), (x + 0.07, 0.16, 0.09), shoes)
        tube(m, (x, 0.0, 0.08), (x, 0.0, 0.5), 0.075, pants, 8)
        tube(m, (x, 0.0, 0.5), (x * 1.05, 0.0, 0.95), 0.088, pants, 8)
    rings = [[(0.18, -0.11, 0.9), (-0.18, -0.11, 0.9), (-0.18, 0.11, 0.9), (0.18, 0.11, 0.9)],
             [(0.2, -0.12, 1.15), (-0.2, -0.12, 1.15), (-0.2, 0.13, 1.15), (0.2, 0.13, 1.15)],
             [(0.23, -0.11, 1.42), (-0.23, -0.11, 1.42), (-0.23, 0.11, 1.42), (0.23, 0.11, 1.42)],
             [(0.12, -0.08, 1.5), (-0.12, -0.08, 1.5), (-0.12, 0.08, 1.5), (0.12, 0.08, 1.5)]]
    loft(m, [rings[0], rings[1]], pants if not tank else shirt)
    loft(m, rings[1:], shirt)
    box(m, (-0.19, -0.115, 0.88), (0.19, 0.135, 0.95), (30, 30, 30))
    if vest:
        box(m, (-0.215, -0.135, 1.0), (0.215, 0.15, 1.4), vest)
    tube(m, (0, 0, 1.48), (0, 0, 1.58), 0.05, skin, 8)
    sphere(m, (0, 0.01, 1.68), 0.115, skin, 10, 7, scale=(0.9, 1.0, 1.15))
    sphere(m, (0, -0.005, 1.7), 0.122, hair, 10, 4, scale=(0.92, 1.02, 1.12))
    if helmet:
        sphere(m, (0, 0, 1.71), 0.14, helmet, 10, 4, scale=(1, 1.05, 1.0))
    if cap:
        sphere(m, (0, 0, 1.72), 0.13, cap, 10, 4)
        box(m, (-0.09, 0.08, 1.73), (0.09, 0.22, 1.75), cap)
    if bandana:
        sphere(m, (0, 0, 1.73), 0.128, bandana, 10, 3)
    for sx in (1, -1):
        sh = np.array((sx * 0.25, 0.0, 1.4))
        if relaxed:
            el = sh + np.array((sx * 0.05, -0.02, -0.3))
            wr = el + np.array((sx * 0.02, 0.06, -0.26))
        else:
            el = sh + np.array((sx * 0.2, 0.02, -0.24))
            wr = el + np.array((sx * 0.18, 0.04, -0.22))
        arm = skin if tank else shirt
        tube(m, sh, el, 0.065, arm, 8)
        tube(m, el, wr, 0.055, skin, 8)
        sphere(m, wr + np.array((sx * 0.04, 0.0, -0.05)), 0.05, skin, 8, 5)
    for sx in (1, -1):
        sphere(m, (sx * 0.045, 0.115, 1.7), 0.014, (20, 20, 20), 6, 3)
    m.atomics = 1
    return m


# ---------------------------------------------------------------- armas (ao longo de +x)


def profile(m, poly, t, color, side=None, y=0.0):
    p = Mesh()
    extrude(p, poly, -t / 2, t / 2, color, side)
    # extrude faz o perfil em (y, z) ao longo de x: gira para o perfil ficar em (x, z) ao longo de y
    m.add(p, rot_z(-math.pi / 2), (0, y, 0))


BLACK = (46, 46, 50)
GUNMETAL = (70, 72, 78)
WOOD = (122, 70, 36)


def pistol(big=False):
    m = Mesh()
    s = 1.25 if big else 1.0
    col = (176, 178, 184) if big else BLACK
    profile(m, [(-0.09 * s, 0.07 * s), (0.11 * s, 0.07 * s), (0.11 * s, 0.03 * s), (-0.09 * s, 0.03 * s)], 0.028 * s, col)
    profile(m, [(-0.08 * s, 0.035 * s), (-0.035 * s, 0.035 * s), (-0.05 * s, -0.07 * s), (-0.1 * s, -0.07 * s)], 0.03 * s,
            (40, 34, 30) if big else (24, 24, 26))
    profile(m, [(-0.03 * s, 0.03 * s), (0.02 * s, 0.03 * s), (0.02 * s, 0.0), (-0.01 * s, -0.01 * s)], 0.008 * s, col)
    if big:
        profile(m, [(0.0, 0.075 * s), (0.1 * s, 0.075 * s), (0.1 * s, 0.065 * s)], 0.012, (150, 150, 156))
    m.atomics = 1
    return m


def rifle(kind):
    m = Mesh()
    if kind == 'ak47':
        profile(m, [(-0.42, 0.02), (-0.18, 0.04), (-0.18, 0.0), (-0.42, -0.07)], 0.04, WOOD)
        profile(m, [(-0.18, 0.05), (0.12, 0.05), (0.12, 0.0), (-0.18, -0.005)], 0.045, GUNMETAL)
        profile(m, [(0.12, 0.045), (0.32, 0.04), (0.32, 0.0), (0.12, 0.0)], 0.05, WOOD)
        profile(m, [(-0.12, 0.0), (-0.08, 0.0), (-0.1, -0.08), (-0.14, -0.08)], 0.035, (60, 40, 24))
        mag = [(0.0, 0.0), (0.05, 0.0), (0.075, -0.07), (0.1, -0.15), (0.06, -0.165), (0.035, -0.08)]
        profile(m, mag, 0.03, GUNMETAL)
        tube(m, (0.32, 0, 0.025), (0.62, 0, 0.025), 0.011, GUNMETAL, 8)
        tube(m, (0.12, 0, 0.06), (0.4, 0, 0.06), 0.012, GUNMETAL, 8)
        profile(m, [(0.56, 0.03), (0.58, 0.03), (0.58, 0.07), (0.57, 0.07)], 0.01, GUNMETAL)
    elif kind == 'm4':
        profile(m, [(-0.42, 0.04), (-0.2, 0.04), (-0.2, -0.01), (-0.42, -0.06)], 0.045, BLACK)
        profile(m, [(-0.2, 0.06), (0.1, 0.06), (0.1, -0.005), (-0.2, -0.005)], 0.05, (44, 44, 46))
        profile(m, [(-0.12, 0.06), (0.06, 0.06), (0.05, 0.1), (-0.11, 0.1)], 0.02, (44, 44, 46))
        tube(m, (0.1, 0, 0.03), (0.34, 0, 0.03), 0.028, BLACK, 10)
        tube(m, (0.34, 0, 0.03), (0.56, 0, 0.03), 0.01, BLACK, 8)
        profile(m, [(-0.1, 0.0), (-0.06, 0.0), (-0.08, -0.08), (-0.12, -0.08)], 0.035, BLACK)
        profile(m, [(0.0, 0.0), (0.05, 0.0), (0.065, -0.14), (0.02, -0.145)], 0.03, (54, 54, 56))
    elif kind == 'mp5':
        profile(m, [(-0.3, 0.04), (-0.14, 0.04), (-0.14, 0.0), (-0.3, -0.03)], 0.04, BLACK)
        profile(m, [(-0.14, 0.055), (0.12, 0.055), (0.12, 0.0), (-0.14, 0.0)], 0.045, BLACK)
        tube(m, (0.12, 0, 0.03), (0.26, 0, 0.03), 0.025, BLACK, 10)
        tube(m, (0.26, 0, 0.03), (0.33, 0, 0.03), 0.01, BLACK, 8)
        profile(m, [(-0.08, 0.0), (-0.04, 0.0), (-0.06, -0.08), (-0.1, -0.08)], 0.035, BLACK)
        profile(m, [(0.02, 0.0), (0.06, 0.0), (0.09, -0.13), (0.055, -0.135)], 0.025, BLACK)
    elif kind == 'uzi':
        profile(m, [(-0.1, 0.05), (0.12, 0.05), (0.12, 0.0), (-0.1, 0.0)], 0.045, BLACK)
        tube(m, (0.12, 0, 0.03), (0.17, 0, 0.03), 0.012, BLACK, 8)
        profile(m, [(-0.02, 0.0), (0.03, 0.0), (0.02, -0.16), (-0.03, -0.16)], 0.035, (40, 40, 42))
    elif kind == 'shotgun':
        profile(m, [(-0.45, 0.02), (-0.2, 0.04), (-0.2, -0.01), (-0.45, -0.08)], 0.045, WOOD)
        profile(m, [(-0.2, 0.05), (0.0, 0.05), (0.0, -0.01), (-0.2, -0.01)], 0.05, (150, 152, 160))
        tube(m, (0.0, 0, 0.03), (0.55, 0, 0.03), 0.016, (150, 152, 160), 10)
        tube(m, (0.08, 0, -0.005), (0.5, 0, -0.005), 0.014, (150, 152, 160), 10)
        tube(m, (0.12, 0, -0.005), (0.3, 0, -0.005), 0.026, WOOD, 10)
    elif kind == 'sniper':
        profile(m, [(-0.5, 0.02), (-0.25, 0.04), (0.2, 0.03), (0.2, -0.01), (-0.2, -0.02), (-0.5, -0.07)], 0.045, WOOD)
        tube(m, (0.2, 0, 0.02), (0.75, 0, 0.02), 0.012, BLACK, 8)
        tube(m, (-0.18, 0, 0.1), (0.15, 0, 0.1), 0.022, BLACK, 10)
        tube(m, (-0.2, 0, 0.1), (-0.18, 0, 0.1), 0.026, BLACK, 10)
        tube(m, (0.15, 0, 0.1), (0.2, 0, 0.1), 0.03, BLACK, 10)
        box(m, (-0.04, -0.01, 0.04), (0.0, 0.01, 0.08), BLACK)
    elif kind == 'rocket':
        tube(m, (-0.55, 0, 0.0), (0.55, 0, 0.0), 0.075, (86, 96, 60), 12)
        tube(m, (0.5, 0, 0.0), (0.62, 0, 0.0), 0.09, (60, 66, 44), 12)
        profile(m, [(-0.05, -0.07), (0.0, -0.07), (-0.01, -0.2), (-0.06, -0.2)], 0.04, BLACK)
        profile(m, [(0.15, -0.07), (0.2, -0.07), (0.19, -0.18), (0.14, -0.18)], 0.04, BLACK)
        box(m, (0.05, 0.07, 0.06), (0.12, 0.09, 0.14), BLACK)
    m.atomics = 1
    return m


def melee(kind):
    m = Mesh()
    if kind == 'bat':
        tube(m, (-0.42, 0, 0), (0.0, 0, 0), 0.016, (90, 60, 34), 10, r_end=0.022)
        tube(m, (0.0, 0, 0), (0.45, 0, 0), 0.022, (160, 112, 64), 12, r_end=0.034)
        cylinder(m, (-0.44, 0, 0), 0.024, 0.02, (60, 40, 22), 10, rot=rot_y(math.pi / 2))
    elif kind == 'katana':
        pts = [(x, 0.012 * (x / 0.75) ** 2 * 3) for x in np.linspace(0.0, 0.72, 10)]
        top = [(x, z + 0.016) for x, z in pts]
        bot = [(x, z - 0.012) for x, z in pts][::-1]
        prof = top + [(0.76, pts[-1][1] + 0.02)] + bot
        profile(m, prof, 0.006, (205, 208, 214))
        disc(m, (0.0, 0, 0.0), (1, 0, 0), 0.045, (60, 50, 30))
        disc(m, (-0.002, 0, 0.0), (-1, 0, 0), 0.045, (60, 50, 30))
        tube(m, (-0.27, 0, 0), (0.0, 0, 0), 0.016, (30, 24, 40), 8)
    elif kind == 'knife':
        profile(m, [(0.0, 0.012), (0.2, 0.01), (0.24, 0.03), (0.16, -0.02), (0.0, -0.018)], 0.006, (200, 204, 210))
        profile(m, [(-0.12, 0.016), (0.0, 0.02), (0.0, -0.022), (-0.12, -0.016)], 0.022, (40, 32, 26))
    elif kind == 'grenade':
        sphere(m, (0, 0, 0), 0.05, (86, 100, 60), 10, 8, scale=(1, 1, 1.25))
        box(m, (-0.015, -0.012, 0.055), (0.015, 0.012, 0.08), (120, 120, 120))
        box(m, (0.01, -0.006, 0.0), (0.03, 0.006, 0.08), (150, 150, 150))
        torus(m, (-0.03, 0, 0.085), 0.018, 0.003, (180, 180, 180), 10, 4)
    m.atomics = 1
    return m


# ---------------------------------------------------------------- objetos


def fire_hydrant():
    m = Mesh()
    yel = (226, 178, 30)
    cylinder(m, (0, 0, 0), 0.16, 0.06, (90, 90, 90), 12)
    cylinder(m, (0, 0, 0.06), 0.11, 0.48, yel, 12)
    cylinder(m, (0, 0, 0.5), 0.135, 0.05, yel, 12)
    sphere(m, (0, 0, 0.55), 0.11, yel, 12, 5, z_from=0.0)
    cylinder(m, (0, 0, 0.66), 0.03, 0.05, yel, 6)
    tube(m, (0.1, 0, 0.36), (0.19, 0, 0.36), 0.04, yel, 8)
    tube(m, (-0.1, 0, 0.36), (-0.19, 0, 0.36), 0.04, yel, 8)
    tube(m, (0, 0.1, 0.33), (0, 0.2, 0.33), 0.06, (190, 40, 30), 10)
    return m


def traffic_cone():
    m = Mesh()
    org = (240, 100, 20)
    box(m, (-0.19, -0.19, 0), (0.19, 0.19, 0.035), (200, 80, 16))
    cylinder(m, (0, 0, 0.035), 0.15, 0.22, org, 14, r_top=0.105, caps=(False, False))
    cylinder(m, (0, 0, 0.255), 0.105, 0.14, (238, 238, 238), 14, r_top=0.077, caps=(False, False))
    cylinder(m, (0, 0, 0.395), 0.077, 0.27, org, 14, r_top=0.02, caps=(False, True))
    return m


def barrel():
    m = Mesh()
    red = (176, 30, 24)
    cylinder(m, (0, 0, 0), 0.3, 0.9, red, 16, cap_color=(120, 20, 18))
    for z in (0.28, 0.6):
        cylinder(m, (0, 0, z), 0.312, 0.035, (140, 24, 20), 16)
    decal(m, (0.0, 0.302, 0.45), (1, 0, 0), (0, 0, 1), 0.22, 0.22, 'hazard', color=(255, 255, 255))
    return m


def trash_bin():
    m = Mesh()
    cylinder(m, (0, 0, 0), 0.3, 0.86, (255, 255, 255), 14, tex='bin_nt', uv_u=6, uv_v=1, cap_color=(30, 60, 36))
    cylinder(m, (0, 0, 0.86), 0.33, 0.06, (36, 70, 44), 14)
    sphere(m, (0, 0, 0.92), 0.33, (40, 78, 48), 14, 4, scale=(1, 1, 0.35), z_from=0.0)
    cylinder(m, (0, 0, 1.03), 0.08, 0.03, (30, 30, 30), 8)
    return m


def dumpster():
    m = Mesh()
    blue = (40, 92, 120)
    box(m, (-0.95, -0.65, 0.18), (0.95, 0.65, 1.25), blue)
    for sx in (1, -1):
        box(m, (sx * 0.96 - 0.02, -0.66, 0.4), (sx * 0.96 + 0.02, 0.66, 0.46), (30, 70, 92))
    lid = (36, 40, 44)
    oquad(m, (0.97, 0.67, 1.25), (0.0, 0.67, 1.25), (0.0, -0.69, 1.33), (0.97, -0.69, 1.33), lid, (0.5, 0, 0))
    oquad(m, (0.0, 0.67, 1.25), (-0.97, 0.67, 1.25), (-0.97, -0.69, 1.33), (0.0, -0.69, 1.33), (44, 48, 52), (-0.5, 0, 0))
    for sx in (0.75, -0.75):
        for sy in (0.48, -0.48):
            cylinder(m, (sx - 0.04, sy, 0.09), 0.09, 0.08, (24, 24, 24), 10, rot=rot_y(math.pi / 2))
    decal(m, (0, 0.653, 0.75), (-1, 0, 0), (0, 0, 1), 0.6, 0.3, 'hazard')
    return m


def bench():
    m = Mesh()
    iron = (38, 40, 42)
    for x in (-0.75, 0.75):
        box(m, (x - 0.03, -0.25, 0), (x + 0.03, -0.2, 0.42), iron)
        box(m, (x - 0.03, 0.2, 0), (x + 0.03, 0.25, 0.42), iron)
        box(m, (x - 0.03, -0.27, 0.4), (x + 0.03, 0.27, 0.44), iron)
        b = Mesh()
        box(b, (x - 0.03, -0.02, 0), (x + 0.03, 0.02, 0.5), iron)
        m.add(b, rot_x(-0.25), (0, -0.25, 0.42))
    for k in range(4):
        y = -0.24 + k * 0.13
        box(m, (-0.95, y, 0.44), (0.95, y + 0.1, 0.48), (255, 255, 255), tex='wood', uvs=1.0)
    for k in range(2):
        b = Mesh()
        box(b, (-0.95, -0.02, 0.18 + k * 0.17), (0.95, 0.02, 0.3 + k * 0.17), (255, 255, 255), tex='wood')
        m.add(b, rot_x(-0.25), (0, -0.27, 0.42))
    return m


def lamppost():
    m = Mesh()
    pole = (70, 84, 80)
    cylinder(m, (0, 0, 0), 0.16, 0.5, pole, 12, r_top=0.12)
    cylinder(m, (0, 0, 0.5), 0.09, 5.6, pole, 10, r_top=0.06)
    pts = [(0, 0, 6.05), (0, 0.4, 6.35), (0, 1.0, 6.45), (0, 1.55, 6.38)]
    for a, b in zip(pts, pts[1:]):
        tube(m, a, b, 0.05, pole, 8)
    box(m, (-0.18, 1.35, 6.2), (0.18, 1.95, 6.38), (50, 56, 54))
    oquad(m, (0.16, 1.37, 6.195), (-0.16, 1.37, 6.195), (-0.16, 1.93, 6.195), (0.16, 1.93, 6.195), (250, 240, 200), (0, 1.6, 6.3))
    return m


def phonebooth():
    m = Mesh()
    box(m, (-0.08, -0.08, 0), (0.08, 0.08, 1.2), (110, 112, 118))
    box(m, (-0.32, -0.2, 1.2), (0.32, 0.2, 2.05), (30, 80, 170))
    box(m, (-0.36, -0.24, 2.05), (0.36, 0.24, 2.12), (20, 60, 140))
    box(m, (-0.26, 0.2, 1.3), (0.26, 0.22, 1.95), (210, 210, 214))
    box(m, (-0.12, 0.22, 1.5), (0.06, 0.28, 1.85), (34, 34, 36))
    box(m, (0.09, 0.22, 1.45), (0.17, 0.3, 1.82), (24, 24, 26))
    return m


def vending():
    m = Mesh()
    box(m, (-0.45, -0.38, 0), (0.45, 0.38, 1.9), (176, 24, 30), skip=('front',))
    decal(m, (0, 0.381, 0.95), (-1, 0, 0), (0, 0, 1), 0.9, 1.9, 'vend_front')
    return m


def crate(kind):
    m = Mesh()
    if kind == 'wood':
        box(m, (-0.4, -0.4, 0), (0.4, 0.4, 0.8), (255, 255, 255), tex='wood', uvs=1.25)
    else:
        box(m, (-0.32, -0.26, 0), (0.32, 0.26, 0.44), (255, 255, 255), tex='cardboard', uvs=1.6)
    return m


def tyre():
    m = Mesh()
    torus(m, (0, 0, 0.14), 0.3, 0.13, (150, 150, 158), 20, 8, tex='vehicletyres128')
    return m


def road_barrier():
    m = Mesh()
    for x in (-0.8, 0.8):
        for sy in (1, -1):
            tube(m, (x, sy * 0.35, 0), (x, 0, 1.0), 0.03, (230, 230, 230), 6)
    box(m, (-1.0, -0.06, 0.72), (1.0, 0.06, 0.95), (255, 255, 255), tex='barrier', uvs=0.5)
    return m


def hotdog_cart():
    m = Mesh()
    box(m, (-0.8, -0.45, 0.3), (0.8, 0.45, 1.1), (190, 194, 200))
    box(m, (-0.82, -0.47, 1.1), (0.82, 0.47, 1.14), (220, 220, 224))
    for sx in (1, -1):
        cylinder(m, (sx * 0.47, 0.0, 0.3), 0.3, 0.06, (40, 40, 40), 14, rot=rot_y(sx * math.pi / 2))
    tube(m, (0, 0, 1.14), (0, 0, 2.3), 0.025, (180, 180, 180), 6)
    cylinder(m, (0, 0, 2.15), 1.05, 0.28, (255, 255, 255), 16, r_top=0.03, tex='umbrella', caps=(True, False))
    tube(m, (-0.8, 0.0, 0.9), (-1.2, 0.0, 0.95), 0.025, (180, 180, 180), 6)
    return m


def skip_bin():
    m = Mesh()
    yel = (220, 170, 30)
    rings = [[(1.1, 0.7, 0.1), (-1.1, 0.7, 0.1), (-1.1, -0.7, 0.1), (1.1, -0.7, 0.1)],
             [(1.6, 0.9, 1.1), (-1.6, 0.9, 1.1), (-1.6, -0.9, 1.1), (1.6, -0.9, 1.1)]]
    loft(m, rings, yel, cap_end=False)
    rings2 = [[(x * 0.98, y * 0.98, z + 0.002) for x, y, z in rings[1]], [(x * 0.68, y * 0.75, 0.25) for x, y, z in rings[0]]]
    loft(m, rings2, (120, 96, 30), cap_start=False)
    for x in (-0.8, 0.0, 0.8):
        box(m, (x - 0.06, 0.7, 0.1), (x + 0.06, 0.95, 1.12), (200, 150, 24))
    return m


def bbq():
    m = Mesh()
    sphere(m, (0, 0, 0.8), 0.32, (24, 24, 26), 14, 6, z_from=-1.0, scale=(1, 1, 0.75))
    for k in range(3):
        a = k * 2 * math.pi / 3
        tube(m, (0.24 * math.cos(a), 0.24 * math.sin(a), 0.65), (0.4 * math.cos(a), 0.4 * math.sin(a), 0.0), 0.02,
             (140, 140, 144), 6)
    cylinder(m, (0, 0, 1.03), 0.03, 0.05, (60, 60, 60), 6)
    return m


def palm():
    m = Mesh()
    pts = [np.array((0.0, 0.0, 0.0))]
    for k in range(1, 9):
        t = k / 8
        pts.append(np.array((0.0, 0.9 * t * t, 7.5 * t)))
    for i in range(len(pts) - 1):
        r0 = 0.24 - 0.1 * i / 8
        tube(m, pts[i], pts[i + 1], r0, (255, 255, 255), 10, r_end=0.24 - 0.1 * (i + 1) / 8, tex='palm_trunk',
             caps=(i == 0, False))
    top = pts[-1]
    sphere(m, top, 0.22, (90, 80, 40), 8, 5)
    for k in range(10):
        a = k * 2 * math.pi / 10 + (0.2 if k % 2 else 0)
        droop = 0.55 + 0.25 * (k % 3) / 2
        L = 3.2 + 0.4 * (k % 2)
        d = np.array((math.cos(a), math.sin(a), 0.0))
        side = np.array((-math.sin(a), math.cos(a), 0.0))
        segs = 5
        prev = None
        for s in range(segs + 1):
            t = s / segs
            p = top + d * L * t + np.array((0, 0, 0.6 * t - droop * 2.2 * t * t))
            w = 0.62 * math.sin(math.pi * min(1, t * 1.05 + 0.05))
            cur = (p + side * w, p - side * w)
            if prev is not None:
                v0, v1 = (s - 1) / segs, t
                m.quad(prev[1], prev[0], cur[0], cur[1], (255, 255, 255), ((1, v0), (0, v0), (0, v1), (1, v1)), 'palm_leaves',
                       ds=True)
            prev = cur
    m.col_spheres = [((0, 0.05, 1.2), 0.32), ((0, 0.35, 4.6), 0.28)]
    m.col_boxes = [((-0.3, -0.3, 0.0), (0.3, 0.3, 0.8))]
    return m


def tree():
    m = Mesh()
    tube(m, (0, 0, 0), (0.1, 0.05, 3.2), 0.24, (255, 255, 255), 8, r_end=0.14, tex='bark')
    for c, r in (((0.1, 0.05, 4.2), 1.6), ((0.9, 0.4, 3.6), 1.1), ((-0.8, -0.3, 3.7), 1.15), ((0.2, -0.9, 3.9), 1.0)):
        sphere(m, c, r, (255, 255, 255), 9, 6, tex='leaves', scale=(1, 1, 0.85))
    return m


def samp_logo():
    m = Mesh()
    box(m, (-1.0, -0.04, 0.0), (1.0, 0.04, 0.5), (20, 22, 30), skip=('front',))
    decal(m, (0, 0.041, 0.25), (-1, 0, 0), (0, 0, 1), 2.0, 0.5, 'samp_logo')
    return m


# ---------------------------------------------------------------- catalogo do conceito

# (id, dff, txd, tipo (0 objeto, 1 veiculo, 2 skin, 3 arma), .img, .ide, construtor)
OBJ, VEH, PED, WEAP = 0, 1, 2, 3
MODELS = [
    (7, 'male01', 'male01', PED, lambda: ped((236, 236, 232), (52, 74, 120), (214, 170, 130), (60, 40, 28))),
    (102, 'ballas1', 'ballas1', PED, lambda: ped((120, 50, 150), (30, 30, 34), (110, 72, 50), (20, 20, 20), bandana=(130, 60, 160))),
    (105, 'fam1', 'fam1', PED, lambda: ped((40, 140, 60), (180, 160, 120), (110, 72, 50), (20, 20, 20), tank=True)),
    (108, 'lsv1', 'lsv1', PED, lambda: ped((230, 200, 50), (34, 34, 36), (190, 140, 100), (30, 24, 20))),
    (265, 'tenpen', 'tenpen', PED, lambda: ped((34, 40, 64), (30, 34, 54), (100, 66, 46), (24, 24, 24))),
    (270, 'sweet', 'sweet', PED, lambda: ped((44, 120, 60), (40, 50, 70), (100, 66, 46), (20, 20, 20), tank=True)),
    (280, 'lapd1', 'lapd1', PED, lambda: ped((40, 50, 90), (34, 40, 66), (214, 170, 130), (60, 40, 28), cap=(30, 36, 60))),
    (285, 'swat', 'swat', PED, lambda: ped((30, 32, 40), (30, 32, 40), (214, 170, 130), (20, 20, 20), helmet=(26, 28, 34),
                                             vest=(40, 44, 54))),
    (287, 'army', 'army', PED, lambda: ped((96, 104, 66), (90, 96, 60), (214, 170, 130), (60, 40, 28), helmet=(80, 88, 56),
                                             vest=(70, 78, 50))),
    (335, 'knifecur', 'knifecur', WEAP, lambda: melee('knife')),
    (336, 'bat', 'bat', WEAP, lambda: melee('bat')),
    (339, 'katana', 'katana', WEAP, lambda: melee('katana')),
    (342, 'grenade', 'grenade', WEAP, lambda: melee('grenade')),
    (346, 'colt45', 'colt45', WEAP, lambda: pistol()),
    (348, 'desert_eagle', 'desert_eagle', WEAP, lambda: pistol(True)),
    (349, 'chromegun', 'chromegun', WEAP, lambda: rifle('shotgun')),
    (352, 'micro_uzi', 'micro_uzi', WEAP, lambda: rifle('uzi')),
    (353, 'mp5lng', 'mp5lng', WEAP, lambda: rifle('mp5')),
    (355, 'ak47', 'ak47', WEAP, lambda: rifle('ak47')),
    (356, 'm4', 'm4', WEAP, lambda: rifle('m4')),
    (358, 'sniper', 'sniper', WEAP, lambda: rifle('sniper')),
    (359, 'rocketla', 'rocketla', WEAP, lambda: rifle('rocket')),
    (400, 'landstal', 'landstal', VEH, lambda: vehicle('suv', (40, 70, 64))),
    (402, 'buffalo', 'buffalo', VEH, lambda: vehicle('muscle', (170, 24, 26), stripe=(20, 20, 20))),
    (403, 'linerun', 'linerun', VEH, lambda: truck((220, 222, 226))),
    (405, 'sentinel', 'sentinel', VEH, lambda: vehicle('sedan', (150, 154, 160))),
    (407, 'firetruk', 'firetruk', VEH, lambda: truck((176, 26, 24), fire=True)),
    (410, 'manana', 'manana', VEH, lambda: vehicle('small', (200, 186, 150))),
    (411, 'infernus', 'infernus', VEH, lambda: vehicle('sports', (196, 22, 30))),
    (415, 'cheetah', 'cheetah', VEH, lambda: vehicle('sports', (230, 190, 30))),
    (416, 'ambulan', 'ambulan', VEH, lambda: vehicle('van', (236, 236, 232), accent=(200, 30, 30))),
    (418, 'moonbeam', 'moonbeam', VEH, lambda: vehicle('van', (130, 170, 190))),
    (420, 'taxi', 'taxi', VEH, lambda: vehicle('sedan', (232, 184, 28), roofsign=(240, 240, 240))),
    (429, 'banshee', 'banshee', VEH, lambda: vehicle('roadster', (40, 80, 180))),
    (431, 'bus', 'bus', VEH, lambda: bus()),
    (451, 'turismo', 'turismo', VEH, lambda: vehicle('sports', (230, 110, 20))),
    (487, 'maverick', 'maverick', VEH, lambda: helicopter((236, 236, 236), (40, 80, 160))),
    (522, 'nrg500', 'nrg500', VEH, lambda: motorbike((200, 30, 30))),
    (541, 'bullet', 'bullet', VEH, lambda: vehicle('sports', (40, 120, 70))),
    (560, 'sultan', 'sultan', VEH, lambda: vehicle('sedan', (90, 110, 150))),
    (562, 'elegy', 'elegy', VEH, lambda: vehicle('roadster', (60, 62, 66))),
    (596, 'copcarla', 'copcarla', VEH, lambda: vehicle('sedan', (20, 20, 22), accent=(236, 236, 236), lightbar=True)),
    (615, 'veg_tree3', 'gta_tree_boak', OBJ, tree),
    (621, 'veg_palm02', 'gta_tree_palm', OBJ, palm),
    (1211, 'fire_hydrant', 'fire_hydrant', OBJ, fire_hydrant),
    (1216, 'phonebooth1', 'phonebooth1', OBJ, phonebooth),
    (1220, 'cardboardbox2', 'cardboardbox', OBJ, lambda: crate('card')),
    (1224, 'woodenbox', 'woodenbox', OBJ, lambda: crate('wood')),
    (1225, 'barrel4', 'barrel4', OBJ, barrel),
    (1228, 'roadworkbarrier1', 'roadworkbarrier1', OBJ, road_barrier),
    (1238, 'trafficcone', 'trafficcone', OBJ, traffic_cone),
    (1280, 'parkbench1', 'parkbench1', OBJ, bench),
    (1297, 'lamppost1', 'lamppost1', OBJ, lamppost),
    (1302, 'vendmachfd', 'vendmachfd', OBJ, vending),
    (1327, 'junk_tyre', 'junk_tyre', OBJ, tyre),
    (1337, 'BinNt07_LA', 'binnt07_la', OBJ, trash_bin),
    (1340, 'chillidogcart', 'chillidogcart', OBJ, hotdog_cart),
    (1365, 'CJ_BIG_SKIP1', 'cj_skip', OBJ, skip_bin),
    (1415, 'DYN_DUMPSTER', 'dyn_dumpster', OBJ, dumpster),
    (1481, 'DYN_BAR_B_Q', 'dyn_bar_b_q', OBJ, bbq),
    (18749, 'SAMPLogoSmall', 'samplogos', OBJ, samp_logo),
]

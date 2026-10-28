#!/usr/bin/env python3
"""Gera src/icons_data.h: os icones do Lucide (ISC, lucide.dev) que as binds podem usar, como tracos.

O lucide.ttf da casa e um recorte com poucos icones, entao o mod desenha estes com o draw list: cada icone e um
conjunto de linhas (path, circle, rect, line, polyline, polygon, ellipse do SVG), achatadas em pontos no grid de 24
do Lucide, guardados em 1/64 de unidade.

Uso: gen_icons.py <pasta com os .svg do Lucide> <saida/icons_data.h>
"""
import math
import os
import re
import sys
import xml.etree.ElementTree as ET

# (nome no Lucide, nome que aparece no seletor). A ordem e a da grade.
ICONS = [
    ('car', 'Carro'), ('car-front', 'Carro de frente'), ('bus', 'Ônibus'), ('truck', 'Caminhão'),
    ('bike', 'Bicicleta'), ('ambulance', 'Ambulância'), ('fuel', 'Combustível'), ('wrench', 'Reparar'),
    ('power', 'Motor'), ('key-round', 'Chave'), ('lock', 'Trancar'), ('lock-open', 'Destrancar'),
    ('door-open', 'Porta'), ('house', 'Casa'), ('shield', 'Escudo'), ('badge', 'Distintivo'),
    ('siren', 'Sirene'), ('link', 'Algemas'), ('search', 'Revistar'), ('id-card', 'Documento'),
    ('gavel', 'Martelo de juiz'), ('crosshair', 'Mira'), ('megaphone', 'Megafone'), ('flashlight', 'Lanterna'),
    ('user', 'Pessoa'), ('users', 'Grupo'), ('user-check', 'Pessoa ok'), ('user-x', 'Pessoa x'),
    ('hand', 'Mão'), ('handshake', 'Aperto de mão'), ('thumbs-up', 'Joinha'), ('eye', 'Olhar'),
    ('heart', 'Vida'), ('heart-pulse', 'Pulso'), ('cross', 'Cruz'), ('stethoscope', 'Estetoscópio'),
    ('pill', 'Remédio'), ('syringe', 'Seringa'), ('skull', 'Caveira'), ('triangle-alert', 'Alerta'),
    ('message-circle', 'Mensagem'), ('phone', 'Telefone'), ('smartphone', 'Celular'), ('radio', 'Rádio'),
    ('mic', 'Microfone'), ('bell', 'Sino'), ('camera', 'Câmera'), ('file-text', 'Ficha'),
    ('clipboard', 'Prancheta'), ('briefcase', 'Maleta'), ('banknote', 'Dinheiro'), ('dollar-sign', 'Cifrão'),
    ('wallet', 'Carteira'), ('shopping-cart', 'Compras'), ('package', 'Pacote'), ('gift', 'Presente'),
    ('hammer', 'Martelo'), ('zap', 'Energia'), ('map-pin', 'Local'), ('flag', 'Bandeira'),
    ('star', 'Estrela'), ('clock', 'Relógio'), ('beer', 'Cerveja'), ('coffee', 'Café'),
    ('utensils', 'Comer'), ('cigarette', 'Cigarro'), ('paw-print', 'Animal'), ('info', 'Info'),
    ('check', 'Ok'), ('x', 'Fechar'), ('ban', 'Proibido'),
]

NUM = re.compile(r'[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?')


def tokens(d):
    """Comandos e numeros do path. Nos arcos (A/a) as duas bandeiras sao um caractere cada: o SVG deixa escrever
    "a1 1 0 011 1" (bandeiras 0 e 1 grudadas no x)."""
    i = 0
    out = []
    cmd = None
    in_arc = 0  # posicao dentro dos 7 valores do arco
    while i < len(d):
        c = d[i]
        if c.isalpha() and c not in 'eE':
            out.append(c)
            cmd = c
            in_arc = 0
            i += 1
        elif c in ' ,\t\n\r':
            i += 1
        elif cmd in ('A', 'a') and in_arc in (3, 4):
            if c not in '01':
                raise ValueError('arco: bandeira %r em %r' % (c, d[i:i + 20]))
            out.append(float(c))
            i += 1
            in_arc += 1
        else:
            m = NUM.match(d, i)
            if not m:
                raise ValueError('path: %r' % d[i:i + 20])
            out.append(float(m.group()))
            i = m.end()
            if cmd in ('A', 'a'):
                in_arc = (in_arc + 1) % 7
    return out


def arc_points(p0, rx, ry, phi, large, sweep, p1):
    """Arco do SVG (F.6.5 da especificacao) em pontos, sem o primeiro."""
    if rx == 0 or ry == 0:
        return [p1]
    x1, y1 = p0
    x2, y2 = p1
    cp, sp = math.cos(math.radians(phi)), math.sin(math.radians(phi))
    dx, dy = (x1 - x2) / 2, (y1 - y2) / 2
    x1p, y1p = cp * dx + sp * dy, -sp * dx + cp * dy
    rx, ry = abs(rx), abs(ry)
    lam = x1p ** 2 / rx ** 2 + y1p ** 2 / ry ** 2
    if lam > 1:
        rx, ry = rx * math.sqrt(lam), ry * math.sqrt(lam)
    num = rx ** 2 * ry ** 2 - rx ** 2 * y1p ** 2 - ry ** 2 * x1p ** 2
    den = rx ** 2 * y1p ** 2 + ry ** 2 * x1p ** 2
    co = math.sqrt(max(0.0, num / den)) if den else 0.0
    if large == sweep:
        co = -co
    cxp, cyp = co * rx * y1p / ry, -co * ry * x1p / rx
    cx, cy = cp * cxp - sp * cyp + (x1 + x2) / 2, sp * cxp + cp * cyp + (y1 + y2) / 2

    def ang(ux, uy, vx, vy):
        a = math.atan2(ux * vy - uy * vx, ux * vx + uy * vy)
        return a

    t1 = ang(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    dt = ang((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry)
    if not sweep and dt > 0:
        dt -= 2 * math.pi
    elif sweep and dt < 0:
        dt += 2 * math.pi
    n = max(2, int(math.ceil(abs(dt) / (math.pi / 12) * max(1.0, max(rx, ry) / 4) ** 0.5)))
    pts = []
    for k in range(1, n + 1):
        t = t1 + dt * k / n
        x, y = rx * math.cos(t), ry * math.sin(t)
        pts.append((cp * x - sp * y + cx, sp * x + cp * y + cy))
    pts[-1] = p1
    return pts


def bezier(p0, c1, c2, p1, n=10):
    pts = []
    for k in range(1, n + 1):
        t = k / n
        a = (1 - t) ** 3
        b = 3 * (1 - t) ** 2 * t
        c = 3 * (1 - t) * t ** 2
        d = t ** 3
        pts.append((a * p0[0] + b * c1[0] + c * c2[0] + d * p1[0], a * p0[1] + b * c1[1] + c * c2[1] + d * p1[1]))
    return pts


def path_strokes(d):
    t = tokens(d)
    strokes = []
    cur = None  # [pontos, fechado]
    pos = (0.0, 0.0)
    start = (0.0, 0.0)
    last_ctrl = None
    last_q = None
    cmd = None
    i = 0

    def take(n):
        nonlocal i
        v = t[i:i + n]
        if len(v) < n or any(isinstance(x, str) for x in v):
            raise ValueError('faltam numeros em %r' % d)
        i += n
        return v

    def begin(p):
        nonlocal cur
        if cur and len(cur[0]) > 1:
            strokes.append(cur)
        cur = [[p], False]

    while i < len(t):
        if isinstance(t[i], str):
            cmd = t[i]
            i += 1
        rel = cmd.islower()
        c = cmd.upper()
        ox, oy = pos if rel else (0.0, 0.0)
        if c == 'M':
            x, y = take(2)
            pos = (ox + x, oy + y)
            start = pos
            begin(pos)
            cmd = 'l' if rel else 'L'
            last_ctrl = last_q = None
        elif c == 'L':
            x, y = take(2)
            pos = (ox + x, oy + y)
            cur[0].append(pos)
            last_ctrl = last_q = None
        elif c == 'H':
            (x,) = take(1)
            pos = ((pos[0] + x) if rel else x, pos[1])
            cur[0].append(pos)
            last_ctrl = last_q = None
        elif c == 'V':
            (y,) = take(1)
            pos = (pos[0], (pos[1] + y) if rel else y)
            cur[0].append(pos)
            last_ctrl = last_q = None
        elif c == 'C':
            x1, y1, x2, y2, x, y = take(6)
            c1, c2, p = (ox + x1, oy + y1), (ox + x2, oy + y2), (ox + x, oy + y)
            cur[0] += bezier(pos, c1, c2, p)
            pos, last_ctrl, last_q = p, c2, None
        elif c == 'S':
            x2, y2, x, y = take(4)
            c1 = (2 * pos[0] - last_ctrl[0], 2 * pos[1] - last_ctrl[1]) if last_ctrl else pos
            c2, p = (ox + x2, oy + y2), (ox + x, oy + y)
            cur[0] += bezier(pos, c1, c2, p)
            pos, last_ctrl, last_q = p, c2, None
        elif c == 'Q':
            x1, y1, x, y = take(4)
            q, p = (ox + x1, oy + y1), (ox + x, oy + y)
            c1 = (pos[0] + 2 / 3 * (q[0] - pos[0]), pos[1] + 2 / 3 * (q[1] - pos[1]))
            c2 = (p[0] + 2 / 3 * (q[0] - p[0]), p[1] + 2 / 3 * (q[1] - p[1]))
            cur[0] += bezier(pos, c1, c2, p)
            pos, last_q, last_ctrl = p, q, None
        elif c == 'T':
            x, y = take(2)
            q = (2 * pos[0] - last_q[0], 2 * pos[1] - last_q[1]) if last_q else pos
            p = (ox + x, oy + y)
            c1 = (pos[0] + 2 / 3 * (q[0] - pos[0]), pos[1] + 2 / 3 * (q[1] - pos[1]))
            c2 = (p[0] + 2 / 3 * (q[0] - p[0]), p[1] + 2 / 3 * (q[1] - p[1]))
            cur[0] += bezier(pos, c1, c2, p)
            pos, last_q, last_ctrl = p, q, None
        elif c == 'A':
            rx, ry, phi = take(3)
            flags = []
            while len(flags) < 2:  # as bandeiras podem vir grudadas ("011")
                v = t[i]
                if isinstance(v, float) and v in (0.0, 1.0):
                    flags.append(int(v))
                    i += 1
                else:
                    raise ValueError('arco: bandeira %r' % v)
            x, y = take(2)
            p = (ox + x, oy + y)
            cur[0] += arc_points(pos, rx, ry, phi, flags[0], flags[1], p)
            pos, last_ctrl, last_q = p, None, None
        elif c == 'Z':
            cur[1] = True
            pos = start
            if cur and len(cur[0]) > 1:
                strokes.append(cur)
            cur = [[pos], False]
            last_ctrl = last_q = None
        else:
            raise ValueError('comando %r' % cmd)
    if cur and len(cur[0]) > 1:
        strokes.append(cur)
    return strokes


def ellipse(cx, cy, rx, ry):
    n = max(12, int(math.ceil(max(rx, ry) * 6)))
    return [[(cx + rx * math.cos(2 * math.pi * k / n), cy + ry * math.sin(2 * math.pi * k / n)) for k in range(n)], True]


def rect(x, y, w, h, rx, ry):
    if rx == 0 and ry == 0:
        return [[(x, y), (x + w, y), (x + w, y + h), (x, y + h)], True]
    rx = min(rx or ry, w / 2)
    ry = min(ry or rx, h / 2)
    pts = []
    for (cx, cy, a0) in ((x + w - rx, y + ry, -90), (x + w - rx, y + h - ry, 0), (x + rx, y + h - ry, 90),
                         (x + rx, y + ry, 180)):
        for k in range(7):
            a = math.radians(a0 + 90 * k / 6)
            pts.append((cx + rx * math.cos(a), cy + ry * math.sin(a)))
    return [pts, True]


def fnum(e, k, default=0.0):
    v = e.get(k)
    return float(v) if v is not None else default


def icon_strokes(path):
    root = ET.parse(path).getroot()
    out = []
    for e in root.iter():
        tag = e.tag.split('}')[-1]
        if tag == 'path':
            out += path_strokes(e.get('d'))
        elif tag == 'circle':
            r = fnum(e, 'r')
            out.append(ellipse(fnum(e, 'cx'), fnum(e, 'cy'), r, r))
        elif tag == 'ellipse':
            out.append(ellipse(fnum(e, 'cx'), fnum(e, 'cy'), fnum(e, 'rx'), fnum(e, 'ry')))
        elif tag == 'rect':
            out.append(rect(fnum(e, 'x'), fnum(e, 'y'), fnum(e, 'width'), fnum(e, 'height'), fnum(e, 'rx'), fnum(e, 'ry')))
        elif tag == 'line':
            out.append([[(fnum(e, 'x1'), fnum(e, 'y1')), (fnum(e, 'x2'), fnum(e, 'y2'))], False])
        elif tag in ('polyline', 'polygon'):
            v = [float(x) for x in NUM.findall(e.get('points'))]
            out.append([list(zip(v[0::2], v[1::2])), tag == 'polygon'])
    return out


def dedupe(pts, closed):
    clean = []
    for p in pts:
        if not clean or abs(p[0] - clean[-1][0]) > 1e-3 or abs(p[1] - clean[-1][1]) > 1e-3:
            clean.append(p)
    if closed and len(clean) > 2 and abs(clean[0][0] - clean[-1][0]) < 1e-3 and abs(clean[0][1] - clean[-1][1]) < 1e-3:
        clean.pop()
    return clean


def main():
    src, out_path = sys.argv[1], sys.argv[2]
    points, strokes, icons = [], [], []
    for name, label in ICONS:
        first = len(strokes)
        for pts, closed in icon_strokes(os.path.join(src, name + '.svg')):
            pts = dedupe(pts, closed)
            if len(pts) < 2:
                continue
            strokes.append((len(points), len(pts), closed))
            points += pts
        icons.append((name, label, first, len(strokes) - first))
    fixed = []
    for x, y in points:
        fixed += [int(round(x * 64)), int(round(y * 64))]
    lines = [
        '// Gerado por tools/gen_icons.py a partir dos SVG do Lucide (https://lucide.dev, licenca ISC:',
        '// Copyright (c) Lucide Icons and Contributors). Nao edite a mao.',
        '#pragma once',
        '',
        'namespace icons {',
        'namespace data {',
        '',
        '// Pontos no grid de 24 do Lucide, em 1/64 de unidade (x, y).',
        'static const short POINTS[] = {',
    ]
    for k in range(0, len(fixed), 16):
        lines.append('    ' + ', '.join(str(v) for v in fixed[k:k + 16]) + ',')
    lines += ['};', '', 'struct Stroke {', '    unsigned short first, count; // em pontos', '    bool closed;', '};', '',
              'static const Stroke STROKES[] = {']
    for first, count, closed in strokes:
        lines.append('    {%d, %d, %s},' % (first, count, 'true' if closed else 'false'))
    lines += ['};', '', 'struct Icon {', '    const char* name;  // o do Lucide (fica no arquivo das binds)',
              '    const char* label; // UTF-8', '    unsigned short firstStroke, strokeCount;', '};', '',
              'static const Icon ICONS[] = {']
    for name, label, first, count in icons:
        esc = ''.join(c if ord(c) < 128 else ''.join('\\x%02X' % b for b in c.encode('utf-8')) for c in label)
        esc = re.sub(r'(\\x[0-9A-F]{2})(?=[0-9A-Fa-f])', r'\1" "', esc)
        lines.append('    {"%s", "%s", %d, %d},' % (name, esc, first, count))
    lines += ['};', '', 'constexpr int COUNT = %d;' % len(icons), '', '} // namespace data', '} // namespace icons', '']
    with open(out_path, 'w') as f:
        f.write('\n'.join(lines))
    print('ok: %d icones, %d tracos, %d pontos' % (len(icons), len(strokes), len(points)))


if __name__ == '__main__':
    main()

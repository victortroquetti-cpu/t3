# Fundo das imagens de conceito do Trok Hitmarker: rua de noite desfocada, com silhuetas por cima (sem arte do jogo).
# Uso: python3 -I scene.py <saida.png>  (1920x1080; a mira do GTA fica em 53% x 40% da tela)
import random
import sys

from PIL import Image, ImageDraw, ImageFilter

W, H = 1920, 1080
HORIZON = 470
random.seed(7)


def gradient(img, top, bottom, y0, y1):
    d = ImageDraw.Draw(img)
    for y in range(y0, y1):
        t = (y - y0) / max(1, y1 - y0 - 1)
        d.line([(0, y), (W, y)], fill=tuple(int(a + (b - a) * t) for a, b in zip(top, bottom)))


img = Image.new('RGB', (W, H))
gradient(img, (14, 16, 30), (44, 34, 52), 0, HORIZON)
gradient(img, (34, 32, 38), (16, 16, 18), HORIZON, H)
d = ImageDraw.Draw(img)

# predios dos dois lados, com janelas acesas
for side in (-1, 1):
    x = 960 + side * 140
    while 0 < x < W:
        w = random.randint(120, 260)
        top = random.randint(60, 300)
        x0, x1 = (x, x + w) if side > 0 else (x - w, x)
        shade = random.randint(22, 34)
        d.rectangle([x0, top, x1, HORIZON + 40], fill=(shade, shade + 2, shade + 8))
        for wy in range(top + 20, HORIZON + 10, 34):
            for wx in range(x0 + 14, x1 - 14, 30):
                if random.random() < 0.33:
                    c = random.choice([(255, 196, 120), (255, 226, 170), (150, 200, 255), (255, 170, 110)])
                    d.rectangle([wx, wy, wx + 12, wy + 18], fill=c)
        x += side * (w + random.randint(6, 30))

# rua em perspectiva
d.polygon([(0, H), (W, H), (1030, HORIZON + 6), (890, HORIZON + 6)], fill=(28, 28, 32))
for k in range(12):
    t0, t1 = k / 12, (k + 0.45) / 12
    y0, y1 = HORIZON + 6 + (H - HORIZON) * t0 ** 2, HORIZON + 6 + (H - HORIZON) * t1 ** 2
    d.polygon([(960 - 2 - 14 * t0 ** 2 * 8, y0), (960 + 2 + 14 * t0 ** 2 * 8, y0),
               (960 + 2 + 14 * t1 ** 2 * 8, y1), (960 - 2 - 14 * t1 ** 2 * 8, y1)], fill=(120, 110, 70))
img = img.filter(ImageFilter.GaussianBlur(7))

# luzes de poste e de carro (bokeh)
glow = Image.new('RGB', (W, H))
g = ImageDraw.Draw(glow)
for _ in range(26):
    x, y = random.randint(0, W), random.randint(150, HORIZON + 60)
    r = random.randint(14, 46)
    c = random.choice([(255, 170, 80), (255, 210, 140), (120, 170, 255), (255, 90, 120)])
    g.ellipse([x - r, y - r, x + r, y + r], fill=tuple(int(v * 0.55) for v in c))
glow = glow.filter(ImageFilter.GaussianBlur(18))
img = Image.blend(img, Image.eval(Image.merge('RGB', [Image.eval(a, lambda v: v) for a in img.split()]), lambda v: v), 0)
img = Image.composite(img, img, Image.new('L', (W, H), 255))
from PIL import ImageChops
img = ImageChops.add(img, glow)

# silhuetas: cada pessoa vira uma mascara (2x para suavizar a borda) com um contorno fino por fora
S = 2


def person(mask, cx, foot, h, arm=None):
    m = ImageDraw.Draw(mask)
    top = foot - h
    P = lambda x, y: (x * S, y * S)
    hw, hh = 0.065 * h, 0.077 * h
    hy = top + hh
    m.ellipse([P(cx - hw, hy - hh), P(cx + hw, hy + hh)], fill=255)
    m.rectangle([P(cx - 0.03 * h, hy), P(cx + 0.03 * h, top + 0.2 * h)], fill=255)
    sy = top + 0.19 * h
    m.rounded_rectangle([P(cx - 0.155 * h, sy), P(cx + 0.155 * h, sy + 0.09 * h)], radius=0.045 * h * S, fill=255)
    wy = top + 0.53 * h
    m.polygon([P(cx - 0.15 * h, sy + 0.04 * h), P(cx + 0.15 * h, sy + 0.04 * h), P(cx + 0.11 * h, wy),
               P(cx - 0.11 * h, wy)], fill=255)
    for side in (-1, 1):
        if arm and side > 0:
            ax, ay = arm
            m.line([P(cx + 0.13 * h, sy + 0.04 * h), P(ax, ay)], fill=255, width=int(0.06 * h * S))
            m.ellipse([P(ax - 0.035 * h, ay - 0.035 * h), P(ax + 0.035 * h, ay + 0.035 * h)], fill=255)
        else:
            m.line([P(cx + side * 0.15 * h, sy + 0.05 * h), P(cx + side * 0.175 * h, top + 0.5 * h)], fill=255,
                   width=int(0.055 * h * S))
            m.ellipse([P(cx + side * 0.175 * h - 0.03 * h, top + 0.47 * h), P(cx + side * 0.175 * h + 0.03 * h,
                       top + 0.53 * h)], fill=255)
        m.polygon([P(cx + side * 0.005 * h, wy - 0.02 * h), P(cx + side * 0.115 * h, wy - 0.02 * h),
                   P(cx + side * 0.085 * h, foot - 0.02 * h), P(cx + side * 0.02 * h, foot - 0.02 * h)], fill=255)
        m.ellipse([P(cx + side * 0.055 * h - 0.04 * h, foot - 0.035 * h), P(cx + side * 0.055 * h + 0.04 * h, foot)],
                  fill=255)


def paste(img, people, fill, rim):
    mask = Image.new('L', (W * S, H * S), 0)
    for args in people:
        person(mask, *args[:3], **args[3] if len(args) > 3 else {})
    grown = mask.filter(ImageFilter.MaxFilter(5))
    mask, grown = mask.resize((W, H), Image.LANCZOS), grown.resize((W, H), Image.LANCZOS)
    img.paste(Image.new('RGB', (W, H), rim), (0, 0), grown)
    img.paste(Image.new('RGB', (W, H), fill), (0, 0), mask)


img = img.convert('RGB')
paste(img, [(1440, 650, 220), (420, 770, 340), (1018, 700, 300)], (10, 11, 14), (66, 70, 84))  # alvo: cabeca na mira
paste(img, [(690, 1260, 860, {'arm': (905, 760)})], (8, 9, 12), (52, 56, 68))                   # voce, de costas
img.save(sys.argv[1])

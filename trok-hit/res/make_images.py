# As imagens padrao do Trok Hitmarker (a pasta "Trok Hitmarker\mira" que vai junto do .asi). Quem quiser outro
# visual edita estes PNG; o mod so faz o pulo, o sumir e o tamanho.
#   mira, acerto, colete, cabeca, abate: caixa de 40 px numa tela de 1080 (vezes o Tamanho); todas na mesma caixa, o
#   efeito fica no lugar da mira enquanto aparece (por isso ja trazem o ponto).
#   corpo: o X do local do acerto, caixa de 48 px.
# Uso: python3 -I make_images.py <pasta de saida>
import math
import os
import sys

from PIL import Image, ImageDraw

N = 128      # pixels de cada imagem
SS = 4       # desenha 4x maior e reduz (borda lisa)
WHITE = (255, 255, 255, 255)
OUTLINE = (0, 0, 0, 200)
RED = (255, 64, 64, 255)
SILVER = (217, 222, 230, 255)
BLUE = (111, 182, 255, 255)
KILL = (255, 59, 59, 255)


def canvas():
    return Image.new('RGBA', (N * SS, N * SS), (0, 0, 0, 0))


def finish(img, name, out):
    img.resize((N, N), Image.LANCZOS).save(os.path.join(out, name + '.png'))


def dot(d, r, color, outline=1.6 * 3.2):
    c = N * SS / 2
    R = (r + outline) * SS
    d.ellipse([c - R, c - R, c + R, c + R], fill=OUTLINE)
    R = r * SS
    d.ellipse([c - R, c - R, c + R, c + R], fill=color)


def bar(d, angle, r0, r1, width, color):
    c = N * SS / 2
    dx, dy = math.cos(angle), math.sin(angle)
    nx, ny = -dy * width * SS / 2, dx * width * SS / 2
    x0, y0, x1, y1 = c + dx * r0 * SS, c + dy * r0 * SS, c + dx * r1 * SS, c + dy * r1 * SS
    d.polygon([(x0 + nx, y0 + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (x0 - nx, y0 - ny)], fill=color)


def bars(d, angles, r0, r1, width, color, outline=2.6 * 3.2):
    for a in angles:
        bar(d, a, r0 - outline / 2, r1 + outline / 2, width + outline, OUTLINE)
    for a in angles:
        bar(d, a, r0, r1, width, color)


def ring(d, r, width, color):
    c = N * SS / 2
    R = r * SS
    d.ellipse([c - R, c - R, c + R, c + R], outline=color, width=int(width * SS))


def main(out):
    os.makedirs(out, exist_ok=True)
    f = 3.2  # 1 px numa tela de 1080 = 3.2 px da imagem (caixa de 40 px)
    diag = [math.pi / 4 + k * math.pi / 2 for k in range(4)]
    cross = [k * math.pi / 2 for k in range(4)]

    img = canvas(); d = ImageDraw.Draw(img)
    dot(d, 3.4 * f, WHITE)
    finish(img, 'mira', out)

    for name, color in (('acerto', RED), ('colete', SILVER)):
        img = canvas(); d = ImageDraw.Draw(img)
        ring(d, 9 * f, 1.6 * f, color[:3] + (150,))
        dot(d, 3.9 * f, color)
        finish(img, name, out)

    img = canvas(); d = ImageDraw.Draw(img)
    bars(d, cross, 6.4 * f, 12.4 * f, 3.0 * f, BLUE)
    dot(d, 3.9 * f, BLUE)
    finish(img, 'cabeca', out)

    img = canvas(); d = ImageDraw.Draw(img)
    bars(d, diag, 7.1 * f, 15.0 * f, 3.6 * f, KILL)
    dot(d, 3.4 * f, WHITE)
    finish(img, 'abate', out)

    g = 128 / 48.0  # o corpo: caixa de 48 px
    img = canvas(); d = ImageDraw.Draw(img)
    bars(d, diag, 8 * g, 19 * g, 2.6 * g, WHITE, outline=2.4 * g)
    finish(img, 'corpo', out)


if __name__ == '__main__':
    main(sys.argv[1])

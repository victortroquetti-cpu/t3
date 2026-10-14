# Um PNG de teste: retangulo de uma cor com um disco opaco no meio e o resto transparente.
# Uso: python3 -I make_png.py <saida.png> <largura> <altura> <r> <g> <b>
import sys

from PIL import Image, ImageDraw


def main(out, w, h, r, g, b):
    img = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    s = min(w, h) * 0.35
    d.ellipse([w / 2 - s, h / 2 - s, w / 2 + s, h / 2 + s], fill=(r, g, b, 255))
    img.save(out)


if __name__ == '__main__':
    a = sys.argv
    main(a[1], int(a[2]), int(a[3]), int(a[4]), int(a[5]), int(a[6]))

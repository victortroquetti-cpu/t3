#!/usr/bin/env python3
"""
Desenha as imagens do app (ícones do Smart Hub e do clima) com o Pillow.
As imagens prontas já estão no repositório; rode só se quiser mudar o desenho:

    pip install pillow
    python ferramentas/gerar_imagens.py
"""

import math
import os

from PIL import Image, ImageDraw

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ESCALA = 4  # desenha 4x maior e reduz: bordas suaves

AMARELO = (255, 201, 74, 255)
LUA = (230, 233, 242, 255)
NUVEM = (232, 236, 242, 255)
NUVEM_FUNDO = (154, 163, 178, 255)
NUVEM_ESCURA = (120, 128, 145, 255)
AZUL = (111, 168, 245, 255)
BRANCO = (255, 255, 255, 255)


def tela(tamanho):
    return Image.new("RGBA", (tamanho * ESCALA, tamanho * ESCALA), (0, 0, 0, 0))


def salvar(img, tamanho, caminho):
    os.makedirs(os.path.dirname(caminho), exist_ok=True)
    img.resize((tamanho, tamanho), Image.LANCZOS).save(caminho, optimize=True)


def s(v):
    return int(round(v * ESCALA))


def sol(d, cx, cy, r):
    for i in range(8):
        a = i * math.pi / 4
        x1, y1 = cx + math.cos(a) * (r + 5), cy + math.sin(a) * (r + 5)
        x2, y2 = cx + math.cos(a) * (r + 12), cy + math.sin(a) * (r + 12)
        d.line([(s(x1), s(y1)), (s(x2), s(y2))], fill=AMARELO, width=s(3.2))
    d.ellipse([s(cx - r), s(cy - r), s(cx + r), s(cy + r)], fill=AMARELO)


def lua(img, cx, cy, r):
    camada = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(camada)
    d.ellipse([s(cx - r), s(cy - r), s(cx + r), s(cy + r)], fill=LUA)
    d.ellipse([s(cx - r + r * 0.75), s(cy - r - r * 0.35), s(cx + r + r * 0.75), s(cy + r - r * 0.35)], fill=(0, 0, 0, 0))
    img.alpha_composite(camada)


def nuvem(d, x, y, largura, cor):
    """Nuvem com base em (x, y) e a largura dada."""
    a = largura * 0.36
    d.rounded_rectangle([s(x), s(y - a), s(x + largura), s(y)], radius=s(a / 2), fill=cor)
    d.ellipse([s(x + largura * 0.12), s(y - a * 1.55), s(x + largura * 0.52), s(y - a * 0.35)], fill=cor)
    d.ellipse([s(x + largura * 0.36), s(y - a * 2.05), s(x + largura * 0.86), s(y - a * 0.45)], fill=cor)


def icone_clima(nome):
    img = tela(64)
    d = ImageDraw.Draw(img)
    if nome == "sol":
        sol(d, 32, 32, 13)
    elif nome == "lua":
        lua(img, 30, 33, 17)
    elif nome == "parcial":
        sol(d, 24, 24, 10)
        nuvem(d, 16, 52, 40, NUVEM)
    elif nome == "parcial-noite":
        lua(img, 24, 24, 13)
        d = ImageDraw.Draw(img)
        nuvem(d, 16, 52, 40, NUVEM)
    elif nome == "nublado":
        nuvem(d, 20, 38, 36, NUVEM_FUNDO)
        nuvem(d, 8, 50, 44, NUVEM)
    elif nome == "chuva":
        nuvem(d, 10, 38, 44, NUVEM)
        for x in (20, 31, 42):
            d.line([(s(x), s(44)), (s(x - 4), s(56))], fill=AZUL, width=s(3.5))
    elif nome == "tempestade":
        nuvem(d, 10, 36, 44, NUVEM_ESCURA)
        d.polygon([(s(33), s(36)), (s(23), s(50)), (s(31), s(50)), (s(26), s(62)), (s(42), s(45)), (s(34), s(45)), (s(39), s(36))], fill=AMARELO)
    elif nome == "neblina":
        nuvem(d, 12, 32, 40, NUVEM_FUNDO)
        for y, x1, x2 in ((40, 10, 54), (48, 16, 50), (56, 10, 46)):
            d.line([(s(x1), s(y)), (s(x2), s(y))], fill=NUVEM, width=s(4))
    elif nome == "neve":
        nuvem(d, 10, 38, 44, NUVEM)
        for x, y in ((20, 48), (32, 52), (44, 48), (26, 58), (38, 58)):
            d.ellipse([s(x - 2.6), s(y - 2.6), s(x + 2.6), s(y + 2.6)], fill=BRANCO)
    return img


def icone_app(largura, altura):
    """Ícone do Smart Hub: casinha branca num quadro azul."""
    img = Image.new("RGBA", (largura * ESCALA, altura * ESCALA), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for y in range(altura * ESCALA):
        t = y / float(altura * ESCALA)
        cor = (int(29 + (47 - 29) * t), int(42 + (111 - 42) * t), int(68 + (214 - 68) * t), 255)
        d.line([(0, y), (largura * ESCALA, y)], fill=cor)
    mascara = Image.new("L", img.size, 0)
    ImageDraw.Draw(mascara).rounded_rectangle([0, 0, img.size[0] - 1, img.size[1] - 1], radius=s(min(largura, altura) * 0.18), fill=255)
    img.putalpha(mascara)

    cx, cy = largura / 2.0, altura / 2.0 + altura * 0.04
    lado = min(largura, altura) * 0.46
    topo = cy - lado * 0.62
    base = cy + lado * 0.5
    d.polygon([(s(cx), s(topo)), (s(cx - lado * 0.62), s(cy - lado * 0.08)), (s(cx + lado * 0.62), s(cy - lado * 0.08))], fill=BRANCO)
    d.rectangle([s(cx - lado * 0.44), s(cy - lado * 0.1), s(cx + lado * 0.44), s(base)], fill=BRANCO)
    d.rectangle([s(cx - lado * 0.12), s(cy + lado * 0.14), s(cx + lado * 0.12), s(base)], fill=(47, 111, 214, 255))
    return img.resize((largura, altura), Image.LANCZOS)


def main():
    for nome in ("sol", "lua", "parcial", "parcial-noite", "nublado", "chuva", "tempestade", "neblina", "neve"):
        salvar(icone_clima(nome), 64, os.path.join(RAIZ, "app", "img", "clima", nome + ".png"))
    for largura, altura in ((106, 87), (115, 95), (85, 70), (95, 78)):
        caminho = os.path.join(RAIZ, "app", "icon", "icon_%dx%d.png" % (largura, altura))
        icone_app(largura, altura).save(caminho, optimize=True)
    print("Imagens geradas em app/img/clima e app/icon.")


if __name__ == "__main__":
    main()

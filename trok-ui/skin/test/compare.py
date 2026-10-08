#!/usr/bin/env python3
"""Confere as saidas do test/run.sh (Trok Skin no Wine). Uso: compare.py <pasta de saida>."""
import os
import sys

from PIL import Image, ImageChops

OUT = sys.argv[1]
failures = 0


def check(ok, msg):
    global failures
    print(('ok: ' if ok else 'FALHA: ') + msg)
    if not ok:
        failures += 1


def rects(run):
    """{(secao, script|item): (x0, y0, x1, y1)}; secao 0 = primeira carga, 1 = depois do recarregamento."""
    out, section = {}, 0
    with open(os.path.join(OUT, run, 'out_rects.txt'), encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if line.startswith('---'):
                section = 1
                continue
            if not line:
                continue
            parts = line.rsplit(' ', 4)  # o nome do script tem espaco: os 4 numeros ficam no fim
            out[(section, parts[0])] = tuple(float(v) for v in parts[1:])
    return out


def image(run, name='out.bmp'):
    return Image.open(os.path.join(OUT, run, name)).convert('RGB')


def log(run):
    path = os.path.join(OUT, run, 'Trok Skin.log')
    return open(path, encoding='latin-1').read() if os.path.exists(path) else ''


def same_image(a, b):
    return ImageChops.difference(a, b).getbbox() is None


def dark(px):
    return all(c < 30 for c in px)


def reddish_pixels(img, box):
    x0, y0, x1, y1 = (int(v) for v in box)
    count = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            r, g, b = img.getpixel((x, y))
            count += r > 150 and g < 120 and b < 120
    return count


base, tema, completo = rects('base'), rects('tema'), rects('completo')
desligado, manter = rects('desligado'), rects('manter')
img = {run: image(run) for run in ('base', 'tema', 'completo', 'desligado', 'manter')}

for run in ('tema', 'completo', 'desligado', 'manter'):
    text = log(run)
    check('skin ligada no mimgui' in text, f'{run}: a skin ligou no cimguidx9.dll (log)')
    check('-- skin desligada' not in text and 'nao deu para desviar' not in text, f'{run}: nenhum erro no log')

# Tema: so visual, nenhum controle muda de lugar ou de tamanho.
check(tema == base, f'tema: os {len(base)} retangulos sao identicos aos sem a skin')
diffs = [k for k in base if tema.get(k) != base[k]]
if diffs:
    print('   diferentes:', diffs[:6])

# Desligado: igual ao original, pixel a pixel.
check(desligado == base, 'desligado: retangulos identicos aos sem a skin')
check(same_image(img['desligado'], img['base']), 'desligado: tela identica a sem a skin, pixel a pixel')

# Cores: janelas no fundo da casa; HUD transparente; cores com significado mantidas.
check(img['base'].getpixel((240, 530))[0] > 200, 'base: o Painel A do script e claro (tema dele)')
check(img['base'].getpixel((680, 530))[0] > 100, 'base: o Painel B do script e vermelho (push dele)')
check(dark(img['tema'].getpixel((240, 530))), 'tema: Painel A no fundo escuro da casa')
check(dark(img['tema'].getpixel((680, 530))), 'tema: Painel B no fundo escuro da casa (o push do fundo foi padronizado)')
for run in ('base', 'tema', 'completo'):
    check(img[run].getpixel((1230, 150)) == (20, 90, 40), f'{run}: HUD continua transparente')
check(reddish_pixels(img['tema'], tema[(0, 'Painel B|texto_vermelho')]) > 20, 'tema: texto vermelho do script ficou vermelho')
bx0, by0, bx1, by1 = tema[(0, 'Painel B|botao_vermelho')]
check(img['tema'].getpixel((int(bx0) + 3, int((by0 + by1) / 2)))[0] > 150, 'tema: botao vermelho do script ficou vermelho')

# Mod da casa com outro nome (casa.lua, janela ##trokCasa): a skin reconhece pelo que ele tem dentro, nao pelo nome,
# e nao muda nada nele, nem a fonte.
casa = (940, 200, 1241, 501)
check(same_image(img['tema'].crop(casa), img['base'].crop(casa)), 'casa: janela ##trok identica a sem a skin, pixel a pixel')
check(same_image(img['completo'].crop(casa), img['base'].crop(casa)), 'casa: com fonte=1 o script da casa segue identico (fonte dele intocada)')
check('script do mimgui: casa.lua (da casa (tem janela ##trok): fica como esta)' in log('tema'), 'casa: mod da casa sem "trok" no nome reconhecido pela janela ##trok (log)')
check(not any('de casa.lua ->' in line for line in log('completo').splitlines()), 'casa: nenhuma fonte do script da casa trocada (log)')
# "trok" no nome nao basta: Trok_Painel_A.lua nao usa o kit e e padronizado.
check('script do mimgui: Trok_Painel_A.lua (padronizado)' in log('tema'), 'nome: script com "trok" no nome, sem o kit, e padronizado (log)')

# Log que explica: cada script e cada janela com a decisao.
for line in ('script do mimgui: Trok_Painel_A.lua (padronizado)', 'janela "Painel A" de Trok_Painel_A.lua: padronizada',
             'janela "HUD" de hud.lua: fundo transparente (HUD): fica como esta',
             'fonte C:\\windows\\Fonts\\trebucbd.ttf 14.0 px de Trok_Painel_A.lua -> fonte da casa'):
    check(line in log('completo'), f'log: "{line}"')

# Script inteiro mantido (manter_scripts=painel_b.lua): Painel B igual ao original, fonte incluida.
painel_b = (480, 40, 881, 561)
check(same_image(image('manter_script').crop(painel_b), img['base'].crop(painel_b)), 'manter_scripts: Painel B identico ao original, pixel a pixel')
check(dark(image('manter_script').getpixel((240, 530))), 'manter_scripts: Painel A segue padronizado')

# Janela mantida (manter=Painel B): exatamente como o autor fez; a outra padronizada.
box = (480, 40, 881, 561)
check(same_image(img['manter'].crop(box), img['base'].crop(box)), 'manter: Painel B identico ao original, pixel a pixel')
check(dark(img['manter'].getpixel((240, 530))), 'manter: Painel A segue padronizado')
check(manter == base, 'manter: retangulos identicos aos sem a skin')

# Fonte da casa: texto com a mesma largura (dentro de 6%).
def width(r):
    return r[2] - r[0]


worst = 1.0
for key in ('texto', 'cirilico'):
    for script in ('Painel A', 'Painel B'):
        k = (0, f'{script}|{key}')
        ratio = width(completo[k]) / width(base[k])
        worst = max(worst, abs(1 - ratio) + 1)
        check(0.94 <= ratio <= 1.06, f'fonte: largura de "{script}|{key}" {ratio:.3f} da original')
k = (0, 'HUD|largura_explicita')
ratio = completo[k][0] / base[k][0]
check(0.94 <= ratio <= 1.06, f'fonte: texto com tamanho explicito (AddText/CalcTextSize) {ratio:.3f} da largura original')
check('-> fonte da casa' in log('completo'), 'fonte: a Trebuchet do mimgui virou a fonte da casa (log)')
check('-> fonte da casa' not in log('tema'), 'fonte=0: nenhuma fonte trocada')

# Ctrl+R: a DLL descarrega e carrega de novo; a skin continua e o resultado e o mesmo.
for run in ('base', 'tema', 'completo', 'manter'):
    again = image(run, 'out_recarregado.bmp')
    check(same_image(again, img[run]), f'{run}: depois de recarregar os scripts a tela e a mesma')
    first = {k[1]: v for k, v in rects(run).items() if k[0] == 0}
    second = {k[1]: v for k, v in rects(run).items() if k[0] == 1}
    check(first == second, f'{run}: depois de recarregar os retangulos sao os mesmos')

# mimgui com outra versao do ImGui: a skin nao encosta em nada.
check('desligada para nao arriscar' in log('versao'), 'versao: ImGui 1.99 recusado (log)')
check(same_image(image('versao'), img['base']), 'versao: tela identica a sem a skin, pixel a pixel')
check(rects('versao') == base, 'versao: retangulos identicos aos sem a skin')

# /trokskin na hora: desligado volta ao visual de cada script (tema claro do A, push vermelho do B), religado
# volta ao da casa, sem recarregar nada.
check(same_image(image('alternar', 'out_desligado.bmp'), img['base']), '/trokskin: desligado na hora = sem a skin, pixel a pixel')
check(same_image(image('alternar', 'out_religado.bmp'), img['tema']), '/trokskin: religado = tema, pixel a pixel')
check('/trokskin: tema desligado' in log('alternar') and '/trokskin: tema ligado' in log('alternar'), '/trokskin: registrado e usado (log)')
check('  Trok_Painel_A.lua: janela "Painel A" -> padronizada' in log('alternar'), '/trokskin: resumo por script e janela no log')

# Pasta do GTA com acento: a fonte da casa abre do mesmo jeito (caminho em UTF-8 para o ImGui).
check('-> fonte da casa' in log('acentua\u00e7\u00e3o'), 'acento: fonte da casa carregou numa pasta com acento (log)')
check(same_image(image('acentua\u00e7\u00e3o'), img['completo']), 'acento: tela identica a da pasta sem acento')

print('PASSOU' if failures == 0 else f'FALHOU ({failures})')
sys.exit(1 if failures else 0)

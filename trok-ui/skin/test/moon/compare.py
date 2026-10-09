#!/usr/bin/env python3
"""Confere as saidas do test/moon/run.sh (Trok Skin no imgui antigo, no Wine). Uso: compare.py <pasta de saida>."""
import math
import os
import sys

from PIL import Image, ImageChops

OUT = sys.argv[1]
failures = 0
RUNS = ('base', 'tema', 'completo', 'desligado', 'manter', 'versao', 'alternar', 'acentua\u00e7\u00e3o', 'manter_script',
        'layout', 'cliques', 'cliques_normal')


def check(ok, msg):
    global failures
    print(('ok: ' if ok else 'FALHA: ') + msg)
    if not ok:
        failures += 1


def finished(run):
    return os.path.exists(os.path.join(OUT, run, 'out_recarregado.bmp'))


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


# Todo cenario termina (um assert do ImGui 1.52 travaria o host numa caixa de mensagem).
for run in RUNS:
    check(finished(run), f'{run}: o host terminou (sem assert nem erro de Lua)')
if failures:
    print(f'FALHOU ({failures})')
    sys.exit(1)

base, tema, completo = rects('base'), rects('tema'), rects('completo')
desligado, manter = rects('desligado'), rects('manter')
img = {run: image(run) for run in ('base', 'tema', 'completo', 'desligado', 'manter')}

for run in ('tema', 'completo', 'desligado', 'manter', 'alternar', 'manter_script'):
    text = log(run)
    check('skin ligada no imgui antigo do moonloader' in text, f'{run}: a skin ligou no MoonImGui.dll (log)')
    check('erro' not in text and 'nao carregou' not in text and 'nao deu' not in text, f'{run}: nenhum erro no log')

# Tema: so visual, nenhum controle muda de lugar ou de tamanho (a borda do 1.52 nao mexe no layout).
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
# Campos no fundo da casa (o 1.52 vem com campos a 30%: nao e transparencia escolhida pelo script).
sx0, sy0, sx1, sy1 = tema[(0, 'Painel B|campo')]
check(dark(img['tema'].getpixel((int(sx0) + 200, int((sy0 + sy1) / 2)))), 'tema: campo do Painel B no fundo da casa')
# Borda da casa (ShowBorders): o contorno da janela e o divisor embaixo do titulo aparecem sobre o fundo.
check(img['tema'].getpixel((40, 300))[0] > img['tema'].getpixel((44, 300))[0], 'tema: janela padronizada com borda')
check(img['tema'].getpixel((240, 60))[0] > img['tema'].getpixel((240, 61))[0], 'tema: divisor embaixo do titulo')
check(dark(img['tema'].getpixel((100, 50))), 'tema: barra de titulo no fundo da casa')

# Mod da casa com outro nome (casa.lua): a skin reconhece pela pasta resource\\trok que ele usa, nao pelo nome, e
# nao muda nada nele, nem a fonte.
casa = (940, 200, 1241, 501)
check(same_image(img['tema'].crop(casa), img['base'].crop(casa)), 'casa: janela ##trok identica a sem a skin, pixel a pixel')
check(same_image(img['completo'].crop(casa), img['base'].crop(casa)), 'casa: com fonte=1 o script da casa segue identico (fonte dele intocada)')
check('script do imgui antigo: casa.lua (da casa (usa a pasta resource\\trok): fica como esta)' in log('tema'), 'casa: mod da casa sem "trok" no nome reconhecido pela pasta da casa (log)')
check(not any('de casa.lua ->' in line for line in log('completo').splitlines()), 'casa: nenhuma fonte do script da casa trocada (log)')
# "trok" no nome nao basta: Trok_Painel_A.lua nao usa o kit e e padronizado.
check('script do imgui antigo: Trok_Painel_A.lua (padronizado)' in log('tema'), 'nome: script com "trok" no nome, sem o kit, e padronizado (log)')

# Log que explica: cada script e cada janela com a decisao.
for line in ('script do imgui antigo: Trok_Painel_A.lua (padronizado)', 'janela "Painel A" de Trok_Painel_A.lua: padronizada',
             'janela "HUD" de hud.lua: fundo transparente (HUD): fica como esta',
             'fonte C:\\windows\\Fonts\\trebucbd.ttf 14.0 px de Trok_Painel_A.lua -> fonte da casa'):
    check(line in log('completo'), f'log: "{line}"')

# Script inteiro mantido (manter_scripts=painel_b.lua): Painel B igual ao original, fonte incluida.
painel_b = (480, 40, 881, 561)
check(same_image(image('manter_script').crop(painel_b), img['base'].crop(painel_b)), 'manter_scripts: Painel B identico ao original, pixel a pixel')
check(dark(image('manter_script').getpixel((240, 530))), 'manter_scripts: Painel A segue padronizado')

# Janela mantida (manter=Painel B): exatamente como o autor fez; a outra padronizada.
check(same_image(img['manter'].crop(painel_b), img['base'].crop(painel_b)), 'manter: Painel B identico ao original, pixel a pixel')
check(dark(img['manter'].getpixel((240, 530))), 'manter: Painel A segue padronizado')
check(manter == base, 'manter: retangulos identicos aos sem a skin')


# Fonte da casa: texto com a mesma largura (dentro de 6%).
def width(r):
    return r[2] - r[0]


for key in ('texto', 'cirilico'):
    for script in ('Painel A', 'Painel B'):
        k = (0, f'{script}|{key}')
        ratio = width(completo[k]) / width(base[k])
        check(0.94 <= ratio <= 1.06, f'fonte: largura de "{script}|{key}" {ratio:.3f} da original')
check('-> fonte da casa' in log('completo'), 'fonte: a Trebuchet do moon_imgui virou a fonte da casa (log)')
check('-> fonte da casa' not in log('tema'), 'fonte=0: nenhuma fonte trocada')

# Ctrl+R: todos os scripts fecham e carregam de novo; a skin continua e o resultado e o mesmo.
for run in ('base', 'tema', 'completo', 'manter'):
    again = image(run, 'out_recarregado.bmp')
    check(same_image(again, img[run]), f'{run}: depois de recarregar os scripts a tela e a mesma')
    first = {k[1]: v for k, v in rects(run).items() if k[0] == 0}
    second = {k[1]: v for k, v in rects(run).items() if k[0] == 1}
    check(first == second, f'{run}: depois de recarregar os retangulos sao os mesmos')

# moon_imgui com outra versao do ImGui: a skin nao encosta em nada.
check('feita para o 1.52 -- fica como esta' in log('versao'), 'versao: ImGui 1.99 recusado (log)')
check(same_image(image('versao'), img['base']), 'versao: tela identica a sem a skin, pixel a pixel')
check(rects('versao') == base, 'versao: retangulos identicos aos sem a skin')

# /trokskin na hora: desligado volta ao visual de cada script, religado volta ao da casa, sem recarregar nada.
check(same_image(image('alternar', 'out_desligado.bmp'), img['base']), '/trokskin: desligado na hora = sem a skin, pixel a pixel')
check(same_image(image('alternar', 'out_religado.bmp'), img['tema']), '/trokskin: religado = tema, pixel a pixel')
check('/trokskin: tema desligado' in log('alternar') and '/trokskin: tema ligado' in log('alternar'), '/trokskin: registrado e usado (log)')
check('  Trok_Painel_A.lua: janela "Painel A" -> padronizada' in log('alternar'), '/trokskin: resumo por script e janela no log')

# Pasta do GTA com acento: a fonte da casa abre do mesmo jeito (caminho em UTF-8 para o ImGui).
check('-> fonte da casa' in log('acentua\u00e7\u00e3o'), 'acento: fonte da casa carregou numa pasta com acento (log)')
check(same_image(image('acentua\u00e7\u00e3o'), img['completo']), 'acento: tela identica a da pasta sem acento')

# Versao de teste (Trok Skin Layout.asi, layout=1): fonte e espacamentos da casa, os mesmos numeros do kit.
lay = rects('layout')
u = max(0.4675, 800 / 1080 * 0.7225)  # escala da casa na tela do teste (15% menor que a do Kill List)
text, field, save = lay[(0, 'Painel A|texto')], lay[(0, 'Painel A|campo')], lay[(0, 'Painel A|salvar')]
check('layout LIGADO' in log('layout'), 'layout: a versao Layout ja vem com layout=1 (log)')
check('erro' not in log('layout') and 'nao conferiu' not in log('layout'), 'layout: nenhum erro no log (e a leitura rapida conferiu)')
check(abs((text[3] - text[1]) - 16 * u) < 0.1, f'layout: texto com o tamanho da casa ({text[3] - text[1]:.2f} px = 16 x escala)')
check(abs((field[3] - field[1]) - 26 * u) < 0.1, f'layout: campo com a altura da casa ({field[3] - field[1]:.2f} px = 26 x escala)')
# O ImGui arredonda para baixo a posicao de cada linha nova: o botao cai em floor(fim do texto + espaco entre linhas).
check(save[1] == math.floor(text[3] + 6 * u),
      f'layout: espaco entre linhas da casa (6 x escala: botao em {save[1]:.0f}; com o espaco padrao do ImGui seria '
      f'{math.floor(text[3] + 4):.0f})')
check(abs(text[0] - (40 + 18 * u)) < 0.5, 'layout: margem da janela da casa (18 x escala)')
check(lay[(0, 'HUD|texto')][:2] == base[(0, 'HUD|texto')][:2], 'layout: HUD no mesmo lugar (espacamentos do autor)')
check(image('layout').getpixel((1230, 150)) == (20, 90, 40), 'layout: HUD continua transparente')
check(all(lay[k] == base[k] for k in base if k[1].startswith('Casa|')), 'layout: mod da casa intocado (retangulos)')
check(same_image(image('layout').crop(casa), img['base'].crop(casa)), 'layout: mod da casa intocado, pixel a pixel')
again = {k[1]: v for k, v in lay.items() if k[0] == 1}
check(again == {k[1]: v for k, v in lay.items() if k[0] == 0}, 'layout: depois de recarregar os retangulos sao os mesmos')

# Cara do kit (versao Layout): cabecalho do kit no lugar da barra de titulo, X, so a linha do cabecalho (sem rodape),
# botao, interruptor e slider do kit. As medidas sao as do kit vezes a escala da tela.
kit = image('layout')
bg = kit.getpixel((240, 530))  # fundo da janela padronizada


def brighter(px, ref, by):
    return sum(px) >= sum(ref) + 3 * by


hy = 40 + 44 * u
check('kit desligado' not in log('layout') + log('cliques'), 'kit: nenhum erro do kit no log (o kit seguiu ligado)')
title = [x for y in range(41, int(hy)) for x in range(41, 410) if min(kit.getpixel((x, y))) > 150]
tx = (min(title) + max(title)) / 2 if title else 0
check(bool(title) and abs(tx - 240) <= 2, f'kit: titulo centralizado no cabecalho (centro em x={tx:.1f}; janela em 240)')
check(lay[(0, 'Painel A|texto')][1] >= hy, 'kit: o conteudo comeca embaixo do cabecalho (44 x escala)')
check(brighter(kit.getpixel((240, int(hy - 0.5))), bg, 5), 'kit: linha embaixo do cabecalho')
xbox = [kit.getpixel((x, y)) for x in range(418, 430) for y in range(48, 60)]
check(max(max(p) for p in xbox) > 100, 'kit: X no cabecalho, a direita (18 da borda)')
check(all(kit.getpixel((240, y)) == bg for y in range(500, 555)),
      'kit: sem a faixa do rodape (o fundo da janela vai ate embaixo, sem linha)')


def gap(img, r):
    """Pixels em x=240 entre a lista e o item selecionavel (onde o mod pos um Separator)."""
    top, bottom = r[(0, 'Painel A|combo')][3], r[(0, 'Painel A|selecionavel')][1]
    return [img.getpixel((240, y)) for y in range(int(top) + 1, int(bottom))]


check(all(sum(p) <= sum(bg) + 6 for p in gap(kit, lay)), 'kit: sem linha separadora no meio (so a do cabecalho)')
check(any(sum(p) >= sum(bg) + 30 for p in gap(img['completo'], completo)), 'versao normal: a linha separadora do mod continua')


def bright(img, r, w):
    x0, y0, x1, y1 = r
    cy = int((y0 + y1) / 2)
    return sum(1 for x in range(int(x0), int(x0) + w) for y in range(cy - 7, cy + 8) if min(img.getpixel((x, y))) > 200)


track = bright(kit, lay[(0, 'Painel A|checkbox')], 26)
box = bright(img['completo'], completo[(0, 'Painel A|checkbox')], 26)
area = 44 * u * 24 * u  # trilha do interruptor do kit
check(track > 0.45 * area and box < 0.3 * area,
      f'kit: caixa de marcar virou o interruptor do kit, ligado ({track} px claros de {area:.0f}; a caixa do ImGui tem {box})')
sx0, sy0, sx1, sy1 = lay[(0, 'Painel A|slider')]
nx0, ny0, nx1, ny1 = completo[(0, 'Painel A|slider')]
check(min(kit.getpixel((int(sx0) + 6, int((sy0 + sy1) / 2)))) > 200 and
      max(img['completo'].getpixel((int(nx0) + 6, int((ny0 + ny1) / 2)))) < 60,
      'kit: slider virou a trilha fina do kit (a parte preenchida e clara; no ImGui e o fundo do campo)')



def text_height(img, r):
    """Altura do texto (pixels claros) dentro de um botao, fora do contorno."""
    x0, y0, x1, y1 = (int(v) for v in r)
    ys = [y for y in range(y0 + 2, y1 - 1) for x in range(x0 + 2, x1 - 2) if min(img.getpixel((x, y))) > 120]
    return max(ys) - min(ys) + 1 if ys else 0


sv = lay[(0, 'Painel A|salvar')]
check(abs((sv[3] - sv[1]) - 26 * u) < 0.1, f'kit: botao com a altura do kit ({sv[3] - sv[1]:.2f} px = 26 x escala)')
kit_text, normal_text = text_height(kit, sv), text_height(img['completo'], completo[(0, 'Painel A|salvar')])
check(0 < kit_text < 0.85 * normal_text,
      f'kit: texto do botao na fonte pequena do kit ({kit_text} px de altura; no botao do ImGui, {normal_text})')
bx0, by0, bx1, by1 = lay[(0, 'Painel B|botao_vermelho')]
r_, g_, b_ = kit.getpixel((int(bx0) + 4, int((by0 + by1) / 2)))
check(r_ > 150 and g_ < 80 and b_ < 80, 'kit: botao vermelho do script continua vermelho')

# Kit com mouse e teclado (cenario cliques): o interruptor e o slider mudam os valores do mod, o X e o Esc fecham.
# Esc: primeiro a lista aberta, depois a janela; nunca com o chat do SA-MP aberto; nunca o menu de outro script
# quando o jogador mexeu por ultimo em outro; a tecla pega nao chega ao jogo (nem as repeticoes), a solta chega.


def clicks(run):
    with open(os.path.join(OUT, run, 'out_cliques.txt'), encoding='utf-8') as f:
        return [line.rstrip('\n') for line in f]


def expect(run, steps):
    got = clicks(run)
    for i, (line, what) in enumerate(steps):
        ok = i < len(got) and got[i] == line
        check(ok, f'{run}: {what}' + ('' if ok else f' (esperado "{line}", veio "{got[i] if i < len(got) else "nada"}")'))
    check(len(got) == len(steps), f'{run}: {len(steps)} passos registrados')


A = 'Trok_Painel_A.lua'
common = [
    ('antes check=1 volume=65 aberto=1', 'valores iniciais do mod'),
    ('interruptor check=0 volume=65 aberto=1', 'clique no interruptor desliga a opcao do mod'),
    ('slider_apertado check=0 volume=0 aberto=1', 'apertar na ponta esquerda do slider leva ao minimo'),
    ('slider_solto check=0 volume=100 aberto=1', 'arrastar ate a ponta direita leva ao maximo'),
    ('botao salvo=1', 'o clique no botao chega ao mod'),
    ('esc_outro_script jogo', 'Esc depois de mexer na janela sem X: nao fecha o menu de outro script'),
    ('esc_outro_script painel_b check=1 volume=65 aberto=1', 'o Painel B (de outro script, ainda em foco no ImGui dele) continua aberto'),
]
expect('cliques', common + [
    (f'esc_lista {A}', 'Esc com a lista aberta fica com o menu (o jogo nao recebe)'),
    ('esc_lista check=0 volume=100 aberto=1', 'Esc com a lista aberta fecha so a lista'),
    ('esc_chat jogo', 'Esc com o chat do SA-MP aberto fica com o SA-MP'),
    ('esc_chat check=0 volume=100 aberto=1', 'o chat aberto nao deixa o Esc fechar o menu'),
    (f'esc {A}', 'Esc fica com o menu (o menu de pausa nao abre)'),
    ('esc check=0 volume=100 aberto=0', 'Esc fecha a janela, como o X'),
    (f'esc_repeticao {A}', 'a repeticao do Esc tambem nao chega ao jogo'),
    ('esc_solto jogo', 'a tecla solta chega ao jogo'),
    ('esc_sem_foco jogo', 'Esc sem menu em foco vai para o jogo (menu de pausa)'),
    ('x_painel_b check=1 volume=65 aberto=0', 'X do cabecalho do kit fecha a janela'),
])
expect('cliques_normal', common + [
    ('esc_lista jogo', 'Esc com a lista aberta segue para o jogo'),
    ('esc_lista check=0 volume=100 aberto=1', 'Esc nao fecha nada'),
    ('esc_chat jogo', 'Esc com o chat segue para o SA-MP'),
    ('esc_chat check=0 volume=100 aberto=1', 'janela aberta'),
    ('esc jogo', 'o Esc continua so do jogo'),
    ('esc check=0 volume=100 aberto=1', 'o Esc nao fecha a janela'),
    ('esc_repeticao jogo', 'repeticao do Esc vai para o jogo'),
    ('esc_solto jogo', 'tecla solta vai para o jogo'),
    ('esc_sem_foco jogo', 'Esc sem foco vai para o jogo'),
    ('x_painel_b check=1 volume=65 aberto=0', 'o X do ImGui fecha a janela'),
])
crc = rects('cliques')
below = (41, int(crc[(0, 'Painel A|combo')][3]) + 2, 439, 530)  # embaixo da lista, onde ela abre
start = image('cliques').crop(below)
check(not same_image(image('cliques', 'out_lista.bmp').crop(below), start), 'cliques: o clique abriu a lista')
check(same_image(image('cliques', 'out_lista_fechada.bmp').crop(below), start), 'cliques: o Esc fechou a lista (igual a antes de abrir, pixel a pixel)')
check(not same_image(image('cliques_normal', 'out_lista_fechada.bmp').crop(below), image('cliques_normal').crop(below)),
      'versao normal: o Esc nao fecha a lista')
end = image('cliques', 'out_cliques.bmp')
check(end.getpixel((240, 300)) == (20, 90, 40) and end.getpixel((680, 300)) == (20, 90, 40), 'cliques: Painel A (Esc) e Painel B (X) sumiram da tela')
check(dark(end.getpixel((240, 700))), 'cliques: a janela sem X continua aberta')
check(same_image(end.crop(casa), img['base'].crop(casa)), 'cliques: mod da casa intocado, pixel a pixel')
check('Esc fechou a janela "Painel A" de Trok_Painel_A.lua' in log('cliques'), 'cliques: o fechamento pelo Esc aparece no log')
check('Esc fechou' not in log('cliques_normal') and 'Esc do kit' not in log('cliques_normal'), 'versao normal: sem o Esc do kit')

print('PASSOU' if failures == 0 else f'FALHOU ({failures})')
sys.exit(1 if failures else 0)

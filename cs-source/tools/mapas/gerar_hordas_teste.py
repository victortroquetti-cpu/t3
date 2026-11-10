#!/usr/bin/env python3
"""Gera o mapa de teste do modo Hordas: game/mod_trok/mapsrc/trok_hordas_teste.vmf.

Uso:  python cs-source/tools/mapas/gerar_hordas_teste.py
Depois compile pelo MCP trok (mapa_compilar) ou no Hammer, e gere o nav (veja o fim deste texto).

Planta (vista de cima, x para leste, y para o norte; medidas do CA, corpo 100u, porta 128 de altura):

   x=-320      x=0          x=1024|1040        x=2064   x=2400
   +----------+-------------+-----+-------------+--------+
   | alcova   |  ÁREA 1     |porta|  ÁREA 2     | alcova |
   | oeste    |  (início)   |paga |  (energia,  | leste  |
   | zumbis   |=janela      |750  |  máquina)   |janela= | zumbis
   | área 0   |  barricada  |     |             | área 2 |
   +----------+-------------+-----+-------------+--------+
   y=0..1024, pé-direito 320, tudo selado numa casca (não vaza).

Área 1: 4 nascimentos de jogador, parede da submetralhadora (1000) e da escopeta (1200), caixa
        misteriosa, caixas para pular. Janela com barricada (func_trok_grade, 6 tábuas) para a alcova oeste.
Área 2: abre pela porta paga (área 2): parede do fuzil (1500), chave de energia, máquina de melhoria
        (precisa de energia), bancada de colete (1500) e de torreta (2000). Janela com barricada para a
        alcova leste (nascedouros da área 2, só valem depois da porta).

Nav: os bots precisam passar pelas janelas e pela porta. Antes de "nav_generate", abra tudo:
   ent_fire func_trok_grade Abrir; ent_fire func_trok_porta_paga Abrir; nav_generate
(o nav_generate recarrega o mapa no fim, e tudo volta fechado).
Texturas: só as de desenvolvimento do HL2 (dev/...), que vêm com o SDK Base.
"""

import os

AQUI = os.path.dirname(os.path.abspath(__file__))
SAIDA = os.path.normpath(os.path.join(AQUI, "..", "..", "game", "mod_trok", "mapsrc", "trok_hordas_teste.vmf"))

PAREDE = "DEV/DEV_MEASUREWALL01D"
CHAO = "DEV/DEV_MEASUREGENERIC01B"
TETO = "DEV/DEV_MEASUREGENERIC01B"
CAIXA = "DEV/DEV_MEASUREGENERIC01"
ESTACAO = "DEV/DEV_MEASUREGENERIC01"
GRADE = "DEV/DEV_MEASUREWALL01C"

_id = [0]


def nid():
    _id[0] += 1
    return _id[0]


def caixa(x1, y1, z1, x2, y2, z2, mat):
    """Bloco alinhado aos eixos (o mesmo texto que o Hammer grava para um cubo)."""
    faces = [
        ((x1, y2, z2), (x2, y2, z2), (x2, y1, z2), "[1 0 0 0] 0.25", "[0 -1 0 0] 0.25"),  # cima
        ((x1, y1, z1), (x2, y1, z1), (x2, y2, z1), "[1 0 0 0] 0.25", "[0 -1 0 0] 0.25"),  # baixo
        ((x1, y2, z2), (x1, y1, z2), (x1, y1, z1), "[0 1 0 0] 0.25", "[0 0 -1 0] 0.25"),  # oeste
        ((x2, y2, z1), (x2, y1, z1), (x2, y1, z2), "[0 1 0 0] 0.25", "[0 0 -1 0] 0.25"),  # leste
        ((x2, y2, z2), (x1, y2, z2), (x1, y2, z1), "[1 0 0 0] 0.25", "[0 0 -1 0] 0.25"),  # norte
        ((x2, y1, z1), (x1, y1, z1), (x1, y1, z2), "[1 0 0 0] 0.25", "[0 0 -1 0] 0.25"),  # sul
    ]
    linhas = ["\tsolid", "\t{", '\t\t"id" "%d"' % nid()]
    for a, b, c, u, v in faces:
        linhas += [
            "\t\tside", "\t\t{",
            '\t\t\t"id" "%d"' % nid(),
            '\t\t\t"plane" "(%g %g %g) (%g %g %g) (%g %g %g)"' % (a + b + c),
            '\t\t\t"material" "%s"' % mat,
            '\t\t\t"uaxis" "%s"' % u,
            '\t\t\t"vaxis" "%s"' % v,
            '\t\t\t"rotation" "0"',
            '\t\t\t"lightmapscale" "16"',
            '\t\t\t"smoothing_groups" "0"',
            "\t\t}",
        ]
    linhas.append("\t}")
    return "\n".join(linhas)


MUNDO = []      # blocos do mapa (world)
ENTIDADES = []  # texto das entidades


def bloco(x1, y1, z1, x2, y2, z2, mat=PAREDE):
    MUNDO.append(caixa(x1, y1, z1, x2, y2, z2, mat))


def entidade(classe, chaves, origem=None, solido=None):
    linhas = ["entity", "{", '\t"id" "%d"' % nid(), '\t"classname" "%s"' % classe]
    for k, v in chaves.items():
        linhas.append('\t"%s" "%s"' % (k, v))
    if origem is not None:
        linhas.append('\t"origin" "%g %g %g"' % origem)
    if solido is not None:
        x1, y1, z1, x2, y2, z2, mat = solido
        linhas.append(caixa(x1, y1, z1, x2, y2, z2, mat).replace("\n\t", "\n\t"))
    linhas.append("}")
    ENTIDADES.append("\n".join(linhas))


def parede_com_vao(x1, x2, y_vao1, y_vao2, z_vao, altura=320):
    """Parede norte-sul (de y=0 a 1024) entre x1 e x2, com um vão de y_vao1..y_vao2 e altura z_vao."""
    bloco(x1, 0, 0, x2, y_vao1, altura)
    bloco(x1, y_vao2, 0, x2, 1024, altura)
    bloco(x1, y_vao1, z_vao, x2, y_vao2, altura)


def gerar():
    # Casca: chão, teto e paredes de fora (sela o mapa).
    bloco(-336, -16, -16, 2416, 1040, 0, CHAO)
    bloco(-336, -16, 320, 2416, 1040, 336, TETO)
    bloco(-336, -16, 0, -320, 1040, 320)
    bloco(2400, -16, 0, 2416, 1040, 320)
    bloco(-320, -16, 0, 2400, 0, 320)
    bloco(-320, 1024, 0, 2400, 1040, 320)

    # Paredes de dentro: janela oeste (área 1), porta do meio, janela leste (área 2).
    parede_com_vao(-16, 0, 448, 576, 128)
    parede_com_vao(1024, 1040, 448, 576, 192)
    parede_com_vao(2064, 2080, 448, 576, 128)

    # Caixas para pular e se cobrir (64 = caixa baixa do CA; 128 = pulo com escalada).
    bloco(400, 160, 0, 464, 224, 64, CAIXA)
    bloco(464, 160, 0, 528, 224, 128, CAIXA)
    bloco(620, 760, 0, 700, 840, 64, CAIXA)
    bloco(1400, 600, 0, 1480, 680, 64, CAIXA)
    bloco(1480, 600, 0, 1544, 664, 128, CAIXA)
    bloco(1650, 250, 0, 1730, 330, 64, CAIXA)

    # Barricadas (func_trok_grade; no Hordas, tábuas que os zumbis quebram e E conserta).
    for nome, x1, x2 in (("barricada_oeste", -16, 0), ("barricada_leste", 2064, 2080)):
        entidade("func_trok_grade", {
            "targetname": nome, "vida": "600", "tabuas": "6", "inicio_fechada": "1",
            "liberar_nos_abrigos": "0", "rendermode": "0", "renderamt": "255", "rendercolor": "255 255 255",
        }, solido=(x1, 448, 0, x2, 576, 128, GRADE))

    # Porta paga entre as áreas.
    entidade("func_trok_porta_paga", {"targetname": "porta_area2", "preco": "750", "area": "2", "rotulo": "Porta"},
             solido=(1024, 448, 0, 1040, 576, 192, ESTACAO))

    # Área 1
    entidade("func_trok_compra_parede", {"arma": "weapon_trok_submetralhadora", "preco": "1000", "precisa_energia": "0"},
             solido=(256, 1008, 48, 384, 1024, 112, ESTACAO))
    entidade("func_trok_compra_parede", {"arma": "weapon_trok_escopeta", "preco": "1200", "precisa_energia": "0"},
             solido=(640, 0, 48, 768, 16, 112, ESTACAO))
    entidade("func_trok_caixa_misteriosa", {"preco": "0", "precisa_energia": "0"},
             solido=(800, 960, 0, 896, 1008, 48, ESTACAO))

    # Área 2
    entidade("func_trok_compra_parede", {"arma": "weapon_trok_fuzil", "preco": "1500", "precisa_energia": "0"},
             solido=(1300, 1008, 48, 1428, 1024, 112, ESTACAO))
    entidade("func_trok_energia", {"rotulo": "Energia"}, solido=(1500, 0, 64, 1532, 16, 112, ESTACAO))
    entidade("func_trok_maquina_upgrade", {"precisa_energia": "1"}, solido=(1800, 944, 0, 1872, 1008, 96, ESTACAO))
    entidade("func_trok_bancada", {"item": "colete", "preco": "1500", "precisa_energia": "0"},
             solido=(1200, 16, 0, 1296, 64, 40, ESTACAO))
    entidade("func_trok_bancada", {"item": "weapon_trok_torreta", "preco": "2000", "precisa_energia": "0"},
             solido=(1700, 16, 0, 1796, 64, 40, ESTACAO))
    entidade("func_trok_bancada", {"item": "kit_reanimacao", "preco": "2500", "precisa_energia": "1"},
             solido=(1950, 400, 0, 2046, 448, 40, ESTACAO))

    # Nascedouros de zumbi: alcova oeste (desde o começo) e leste (depois da porta).
    for y in (250, 512, 774):
        entidade("info_trok_hordas_spawn", {"area": "0", "angles": "0 0 0"}, origem=(-200, y, 8))
        entidade("info_trok_hordas_spawn", {"area": "2", "angles": "0 180 0"}, origem=(2260, y, 8))
    entidade("info_trok_hordas_spawn", {"area": "0", "chefe": "1", "angles": "0 0 0"}, origem=(-200, 100, 8))

    # Jogadores nascem na área 1 olhando para a janela.
    for x, y in ((300, 400), (300, 624), (520, 400), (520, 624)):
        entidade("info_player_deathmatch", {"angles": "0 180 0"}, origem=(x, y, 8))

    # Luz e lógica.
    for x in (-160, 300, 750, 1300, 1800, 2240):
        for y in (300, 724):
            entidade("light", {"_light": "255 240 220 220", "_lightHDR": "-1 -1 -1 1", "_quadratic_attn": "1"}, origem=(x, y, 290))
    entidade("trok_hordas_logica", {"targetname": "hordas"}, origem=(500, 512, 300))

    corpo = [
        "versioninfo", "{", '\t"editorversion" "400"', '\t"editorbuild" "8864"', '\t"mapversion" "1"',
        '\t"formatversion" "100"', '\t"prefab" "0"', "}",
        "visgroups", "{", "}",
        "viewsettings", "{", '\t"bSnapToGrid" "1"', '\t"bShowGrid" "1"', '\t"nGridSpacing" "16"', "}",
        "world", "{", '\t"id" "%d"' % nid(), '\t"mapversion" "1"', '\t"classname" "worldspawn"',
        '\t"skyname" "sky_day01_01"', '\t"maxpropscreenwidth" "-1"', '\t"detailvbsp" "detail.vbsp"',
        '\t"detailmaterial" "detail/detailsprites"',
    ]
    corpo += MUNDO
    corpo.append("}")
    corpo += ENTIDADES
    corpo += ["cameras", "{", '\t"activecamera" "-1"', "}", "cordon", "{",
              '\t"mins" "(-1024 -1024 -1024)"', '\t"maxs" "(1024 1024 1024)"', '\t"active" "0"', "}"]
    os.makedirs(os.path.dirname(SAIDA), exist_ok=True)
    with open(SAIDA, "w", encoding="utf-8", newline="\r\n") as f:
        f.write("\n".join(corpo) + "\n")
    print("gerado: %s (%d blocos de mundo, %d entidades)" % (SAIDA, len(MUNDO), len(ENTIDADES)))


if __name__ == "__main__":
    gerar()

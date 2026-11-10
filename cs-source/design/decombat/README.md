# DeCombat: especificações do Combat Arms de 2009

Cópia de parte da documentação do projeto **DeCombat** (https://github.com/argallo/DeCombat, commit
`19b7cec`, 08/11/2026). É um emulador independente do Combat Arms EU de 2009, escrito do zero, com licença
**MIT ou Apache-2.0** (os dois textos estão nesta pasta, junto com `NOTICE.md`, `AUTHORS` e `CLEANROOM.md`, como a
licença pede). Os autores mediram o jogo real: capturas de rede, observação e análise do cliente que cada um tem.

Para o Rearmed, isto serve como **fonte dos números e das sensações** do CA (regra 2 do `CLAUDE.md`). Nenhum asset
do jogo vem junto.

## O que usar e para quê

| Arquivo | Serve para |
|---|---|
| `specs/MOVEMENT_2009.md` + adendos (`_edges`, `_sprintjump`, `_servervars`, ...) | **Fase 3, movimento:** gravidade, velocidades de andar, correr, sprint (cerca de 1,39× a corrida) e agachar, aceleração, atrito, pulo, escada, degrau, quinas. Converter para o Source (unidades e eixo: o LithTech usa Y para cima) e preencher `cfg/movimento.cfg` |
| `specs/WEAPON_FIRE_2009.md` + `_cone` + `_recoil`, `specs/CAPTURE_tdm4_WEAPON_FORMS.md` | **Fase 3, armas:** cone de dispersão, recuo, cadência e chumbos da escopeta → `scripts/weapon_trok_*.txt` |
| `specs/GAME_MODES_2009.md` | regras dos modos (TDM, OMA, Elimination, S&D, CTF, Spy Hunt, **Quarantine**, Fireteam): tempos, metas, rodadas → `cfg/equipe.cfg`, `solo.cfg`, `surto.cfg` |
| `specs/RADAR_2009.md` | como o radar do CA mostra aliados e inimigos → comparar com o nosso (`hud_trok.cpp`) |
| `specs/AUDIO_2009.md` | como o CA toca passos, tiros e distância → referência para os nossos soundscripts |
| `specs/FIRETEAM_2009.md`, `docs/FIRETEAM_AI_DESIGN.md`, `docs/BOTS.md` | modo cooperativo e IA dos inimigos e bots → ideias para os nossos bots e um futuro modo co-op |
| `specs/FMT_DTX.md`, `FMT_LTB.md`, `FMT_DAT85*.md`, `FMT_SPR_LTC_FX.md`, `FMT_RENDERSTYLE_2009.md`, `RENDER_2009.md` | formatos de textura, modelo, mapa e efeitos do LithTech: base para os conversores do `tools/ca_extrator.py` (para versões com `.rez` no formato padrão) |
| `docs/MODERN_LOOK.md` | o que eles mudaram para o CA parecer mais moderno sem perder a cara |

## O que ficou de fora, e por quê

- **Protocolo de rede, lobby, servidor e testes** (`PROTOCOL_2009`, `UDP_*`, `GAME_MESSAGES_*`, `DSERVER_*`, capturas
  de rede): não servem para um jogo no Source.
- **`HISEC_2009` e `FMT_CRES_UI`:** tratam da proteção do cliente e de chaves. O projeto não contorna proteção
  (regra 2).
- **Código Rust:** fica no GitHub deles. A leitura de `.rez` do DeCombat inclui decodificação Blowfish com a chave
  tirada do `Engine.exe` do jogador, e isso não entra aqui.

## Como citar

Quando um número daqui for para um cfg ou script, anote ao lado `// DeCombat: <arquivo> §<seção>` e mantenha a
linha do DeCombat em `game/mod_trok/CREDITOS.txt`.

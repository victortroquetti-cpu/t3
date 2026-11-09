---
name: source-mod
description: Trabalho no mod Trok, a conversão total de CS:Source feita sobre o Source SDK 2013 Multiplayer, em cs-source/. Use para qualquer coisa de Source engine 1 neste repositório - código C++ de game/server, game/client e game/shared, arquivos .vpc, gameinfo.txt, bots com NextBot, regras de jogo (gamerules), armas, movimento, HUD e VGUI, rede e predição, ConVars, eventos, user messages, entidades de mapa e FGD, e conteúdo (VPK, modelos QC, materiais VMT/VTF, sons, mapas e nav mesh). Também para montar, gerar projetos, checar e compilar o mod. Triggers - Source SDK, Source engine, CS:S, Counter-Strike Source, HL2MP, HL2DM, NextBot, VPC, gameinfo, Hammer, VMT, VTF, QC, studiomdl, sourcemod, total conversion, conversão total.
---

# Mod Trok (CS:Source, Source SDK 2013 MP)

O objetivo do dono é uma **conversão total**: refazer vários sistemas do jogo
(IA dos bots, modos de jogo, recursos de FPS moderno). **Só implemente sistema
ou modo quando ele pedir.** Objetivos ditos de passagem são contexto, não tarefa.

## Regra zero: só código legal

- O código de jogo do CS:S (compra, bomba, movimento do CS, bots `CCSBot`) **não é público**.
  Tudo o que for "do CS" é refeito do zero sobre o código do SDK.
- **Nunca** use, copie, consulte ou "se inspire linha a linha" em código vazado:
  vazamento de 2020 (CS:GO/TF2), `nillerusr/source-engine`, `kisak-strike`, `cstrike15_src`,
  ou qualquer repositório que contenha `game/server/cstrike` ou o motor (`engine/`, `materialsystem/` completos).
  Se o usuário apontar um, explique o motivo e recuse.
- Conteúdo do CS:S (modelos, sons, mapas) é **montado** do jogo instalado (`|appid_240|` no gameinfo).
  Redistribuir esses arquivos não é permitido.
- Licença: Source 1 SDK License (uso gratuito, mod não comercial). Ver `cs-source/sdk/LICENSE`.

## Mapa da área

```
cs-source/
  trok.py              setup | gen | check | save-patch | status | refs
  sdk.lock             commit fixo do ValveSoftware/source-sdk-2013
  patches/sdk.patch    TODAS as mudanças em arquivos da Valve (um patch só)
  src/game/server/trok/   código só do servidor     -> ligado em sdk/src/game/server/trok
  src/game/client/trok/   código só do cliente      -> ligado em sdk/src/game/client/trok
  src/game/shared/trok/   código dos dois lados     -> ligado em sdk/src/game/shared/trok
  game/mod_trok/          pasta do jogo (gameinfo, cfg, scripts, resource, materials...)
  sdk/                 (fora do git) SDK baixado, com o patch e os links
  refs/                (fora do git) repositórios de consulta
```

O mod hoje é o HL2MP com `CTrokRules` (subclasse de `CHL2MPRules`) instalada em
`InstallGameRules()` via patch. Os .vpc `server_trok.vpc`, `client_trok.vpc` e
`launcher_main_mod_trok.vpc` são **gerados** a partir dos do HL2MP pelo `trok.py`
(define `TROK`, pasta de saída `game/mod_trok/bin`, arquivos de `trok/` listados sozinhos).

## Fluxo de trabalho

1. Sem `cs-source/sdk/`? Rode `python cs-source/trok.py setup` (baixa ~600 MB, uns 10 s).
2. Código novo vai em `cs-source/src/game/{server,client,shared}/trok/`. Depois de criar ou
   apagar arquivo: `python cs-source/trok.py gen`.
3. Precisa mudar arquivo da Valve? Edite dentro de `cs-source/sdk/`, cerque com
   `#ifdef TROK` quando fizer sentido e rode `python cs-source/trok.py save-patch`.
   Prefira sempre subclasse/arquivo novo em `trok/` a editar arquivo da Valve.
4. Verifique: `python cs-source/trok.py check` (g++ `-fsyntax-only` com as flags do Linux 64 bits).
   Pega erro de sintaxe e de tipo nos dois lados; **não** liga nem roda.
5. Build de verdade:
   - Windows: `cs-source\sdk\src\createtrok.bat`, abrir `trok.sln` (VS 2022, MSVC v143, Python 3.13).
   - Linux: `cs-source/sdk/src/buildtrok` (podman + Steam Runtime sniper).
   - Para rodar: Steam com **Source SDK Base 2013 Multiplayer** e **Counter-Strike: Source**
     instalados; executável `cs-source/sdk/game/mod_trok(.exe)`.
6. Ambiente na nuvem do Claude: não tem podman nem daemon do docker, e rodar o `vpc` pré-compilado
   da Valve pode ser barrado pela permissão. Então o que dá para provar aqui é o `check`.
   Diga sempre o que foi checado (sintaxe) e o que não foi (link, jogo rodando).

## Convenções

- Classes `CTrok*`; no cliente, `#define CTrokX C_TrokX` dentro de `#ifdef CLIENT_DLL` (padrão do SDK).
- ConVars e comandos com prefixo `trok_`. Comentários e textos em português, como o resto do repositório.
- Todo .cpp: `#include "cbase.h"` primeiro e `#include "tier0/memdbgon.h"` por último.
- Código só de servidor: `#ifndef CLIENT_DLL` (ou `GAME_DLL`); só de cliente: `#ifdef CLIENT_DLL`.
- Não renomeie a pasta `trok`/`mod_trok` à mão: os nomes estão em `trok.py` (MOD, GAMEDIR, TITLE).

## Onde está cada coisa no SDK (`cs-source/sdk/src/game/`)

| Sistema | Arquivos de partida |
|---|---|
| Regras de jogo | `shared/hl2mp/hl2mp_gamerules.*`, `shared/multiplay_gamerules.*`, `shared/teamplay_gamerules.*`, `shared/teamplayroundbased_gamerules.*` (rodadas do TF2) |
| Jogador | `server/hl2mp/hl2mp_player.*`, `client/hl2mp/c_hl2mp_player.*`, `shared/hl2mp/hl2mp_player_shared.*`, `server/player.*` |
| Movimento | `shared/gamemovement.*`, `shared/hl2/hl_gamemovement.*` |
| Armas | `shared/hl2mp/weapon_hl2mpbase*.*`, `shared/basecombatweapon_shared.*`, `shared/weapon_parse.*`, scripts em `game/mod_hl2mp/scripts/weapon_*.txt` |
| Bots (jogador) | `server/hl2mp/bot/` (NextBot, `CHL2MPBot`), `server/NextBot/` |
| NPC NextBot | `server/tf/halloween/zombie/` (`CZombie : NextBotCombatCharacter`), `server/NextBot/simple_bot.*` |
| HUD | `client/hl2mp/*hud*`, `client/hud*.cpp`, `game/mod_hl2mp/scripts/HudLayout.res` |
| Eventos | `game/mod_hl2mp/resource/modevents.res` |
| Referência de jogo com classes, rodadas, bots táticos | `shared/tf/`, `server/tf/`, `server/tf/bot/` |

## Referências (leia só a que a tarefa pede)

- `references/rede-e-predicao.md` — tabelas de rede, CNetworkVar, predição, proxy de gamerules, user messages, eventos, ConVars.
- `references/nextbot.md` — IA de bot: componentes, `Action<T>`, caminhos, nav mesh, depuração.
- `references/entidades-e-regras.md` — entidades de mapa (datadesc, inputs/outputs, FGD), gamerules, rodadas, times.
- `references/armas-movimento-hud.md` — armas (classe + script + acttable), movimento, HUD e VGUI.
- `references/conteudo-e-ferramentas.md` — gameinfo e montagem do CS:S, VPK, modelos, materiais, sons, mapas, ferramentas.
- `references/repositorios.md` — o que tem em `cs-source/refs/` e onde procurar cada padrão.

## Antes de dizer que terminou

- `trok.py gen` rodou depois de criar/apagar arquivo; `trok.py check` passou nos dois lados.
- Mudou arquivo da Valve? `trok.py save-patch` e o patch entrou no commit.
- Nada de `cs-source/sdk/` ou `cs-source/refs/` no commit (estão no .gitignore).
- Relate o que foi verificado e o que só será provado com build e jogo rodando.

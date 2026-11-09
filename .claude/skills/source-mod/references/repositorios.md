# Repositórios de consulta (`cs-source/refs/`)

Baixados por `python cs-source/trok.py refs` (rasos, fora do git, uns 1,8 GB no total).
Servem para **ler padrões**; nada daqui entra no build. Antes de copiar trecho, confira a
licença do repositório e cite a origem no comentário.

| Pasta | O que é | Licença | Use para |
|---|---|---|---|
| `refs/zmr-game` | Zombie Master: Reborn, mod MP completo sobre o SDK 2013 (Linux e Windows) | Source 1 SDK License | estrutura de um mod MP grande, regras próprias, jogador próprio, armas próprias, HUD, NPCs em massa |
| `refs/mapbase` | Mapbase, fork do SDK com VScript, entidades e correções (layout antigo `sp/` e `mp/`) | Source 1 SDK License | VScript (Squirrel) no SDK 2013, entidades de lógica, correções de bugs conhecidos |
| `refs/sourcemod` | SourceMod (plugins de servidor) | GPLv3 | **só consulta**: nomes de eventos e props do CS:S original (`gamedata/sm-cstrike.games`, `plugins/include/cstrike.inc`) para imitar comportamento. Não copie código (GPL). |
| `refs/zombiereloaded` | Zombie:Reloaded, infecção para CS:S | GPLv3 | regras e números do modo infecção (classes, coice por arma e parte do corpo); **só leitura** |
| `refs/zombie-riot` | Zombie Riot, survival com bots no CS:S | sem licença declarada; traz assets de terceiros | ondas por "dias"; **só leitura**, nenhum asset |
| `refs/zombie-escape-amxx` | Zombie Escape (CS 1.6, base ZP 5.0) | GPLv3 | regras do modo fuga; **só leitura** |

Análise de cada mod de zumbi da lista do dono: `cs-source/design/zumbi-mods-analise.md`.

## Onde procurar

- **Mod MP inteiro, ponta a ponta** (`refs/zmr-game/src/game/`):
  - regras: `shared/zmr/zmr_gamerules.*`; jogador: `shared/zmr/zmr_player_shared.*`, `client/zmr/c_zmr_player.*`
  - movimento próprio: `shared/zmr/zmr_gamemovement.*`; usercmd estendido: `shared/zmr/zmr_usercmd.*`
  - armas e parse próprio: `shared/zmr/weapons/`, `shared/zmr/zmr_weapon_parse.*`
  - user messages: `shared/zmr/zmr_usermsgs.cpp`; colisão suave entre jogadores: `shared/zmr/zmr_softcollisions.*`
  - IA: framework próprio **NPCR** (`server/npcr/`: motor, caminho, chase) e zumbis em
    `server/zmr/npcs/` (`zmr_zombiebase.*`, `zmr_playerbot.*`). É outra abordagem além do
    NextBot, boa para comparar custo com muitos NPCs (há compensação de lag para NPC em `zmr_npclagcomp.h`).
  - build: `src/creategameprojects*` e os .vpc deles mostram como um mod registra o próprio jogo no VPC.
- **VScript** (`refs/mapbase/sp/src/vscript/`, `refs/mapbase/sp/src/game/shared/mapbase/vscript_*`):
  como expor funções do jogo a scripts. Útil se o mod quiser modos e armas configuráveis sem recompilar.
- **Comportamento do CS:S para recriar** (`refs/sourcemod`): eventos (`round_start`, `bomb_planted`,
  `player_death` com `headshot`), nomes de props de rede (`m_iAccount`, `m_bHasDefuser`...).
  Use como especificação de comportamento, nunca como código.

## No SDK oficial (`cs-source/sdk/src/game/`), as melhores "referências" já estão lá

- TF2 completo: `shared/tf/`, `server/tf/`, `client/tf/` — classes, rodadas (`teamplayroundbased_gamerules`),
  bots táticos (`server/tf/bot/`, com esquadrões e missões), NPCs NextBot (`server/tf/halloween/`).
- HL2MP: base atual do mod, incluindo os bots NextBot em `server/hl2mp/bot/`.

## Não use (código vazado ou derivado)

`nillerusr/source-engine`, `kisak-strike`, `cstrike15_src`, `source-sdk-2020-leak`, qualquer coisa com
"leak" no nome, ou repositório que traga `engine/` completo ou `game/server/cstrike/`. Isso vale também
para "só olhar": o mod precisa poder ser publicado.

# Conteúdo e ferramentas

## Como o jogo acha os arquivos (`game/mod_trok/gameinfo.txt`)

- A lista `SearchPaths` é lida de cima para baixo; o primeiro que tiver o arquivo ganha.
  No console, `path` mostra a lista final.
- Ordem atual: `custom/` -> a pasta do mod -> `mod_hl2mp` do SDK (scripts e HUD que o código do HL2MP
  ainda usa) -> VPKs do HL2MP -> **CS:S** (`|appid_240|cstrike/cstrike_pak.vpk` e a pasta `cstrike`) -> base do HL2.
- O CS:S está **abaixo** do HL2MP de propósito: ele tem `scripts/HudLayout.res`, `scripts/weapon_*.txt`
  e `resource/*.res` próprios, que quebram o código do HL2MP. Quando o mod tiver os seus em
  `game/mod_trok/`, eles ganham de todos.
- `|appid_N|` é a pasta de instalação do jogo N na Steam (243750 = Source SDK Base 2013 MP,
  240 = CS:S). O jogo precisa estar instalado.
- Redistribuição: o mod publicado não pode levar arquivos do CS:S; ele monta do jogo do jogador.

## Tipos de arquivo

| Coisa | Formato | Onde | Ferramenta |
|---|---|---|---|
| Modelo | `.mdl` + `.vvd` + `.vtx` + `.phy`, fonte `.qc` + `.smd`/`.dmx` | `models/` | `studiomdl` (SDK Base `bin/`), Crowbar, Blender Source Tools |
| Material | `.vmt` (texto) + `.vtf` (textura) | `materials/` | VTFEdit / VTFLib, `vtex` |
| Som | `.wav` (PCM 16 bits, 44,1 kHz) ou `.mp3`, scripts em `scripts/game_sounds_*.txt` | `sound/` | qualquer editor de áudio |
| Mapa | `.vmf` (fonte) -> `.bsp`; malha dos bots `.nav` | `maps/` | Hammer (ou Hammer++), `vbsp`, `vvis`, `vrad` (o SDK compila esses três) |
| Partículas | `.pcf` + `particles/particles_manifest.txt` | `particles/` | editor de partículas do motor (`-tools`) |
| Pacote | `.vpk` | `custom/` ou junto do gameinfo | `vpk.exe` (SDK Base `bin/`), VPKEdit |
| Texto da interface | `resource/mod_trok_english.txt` e `_brazilian.txt` (UTF-16 LE) | `resource/` | editor de texto |

- Sons de arma e passos vêm de "soundscripts" (`scripts/game_sounds_manifest.txt` lista os arquivos).
  No código: `EmitSound( "Trok.Arma.Tiro" )` e `PrecacheScriptSound` no `Precache()`.
- Modelos e materiais do CS:S podem ser descompilados com o Crowbar para estudo e para usar de base
  em conteúdo **seu**; publicar o derivado continua sujeito à licença do CS:S.

## Ferramentas (instalar no Windows de quem desenvolve)

| Ferramenta | Para quê | Onde |
|---|---|---|
| Visual Studio 2022 (Desktop C++, MSVC v143, Windows SDK) + Python 3.13 | compilar o mod | visualstudio.microsoft.com, python.org |
| Steam: Source SDK Base 2013 Multiplayer + Counter-Strike: Source | rodar o mod, `bin/` com studiomdl, vpk, Hammer, FGDs | Steam > Biblioteca > Ferramentas |
| Crowbar | descompilar/compilar modelos (.mdl <-> .qc) | github.com/ZeqMacaw/Crowbar |
| Blender Source Tools | exportar SMD/DMX do Blender | github.com/Artfunkel/BlenderSourceTools |
| VTFEdit / VTFLib | criar e ver .vtf | github.com/NeilJed/VTFLib |
| VPKEdit | abrir e montar .vpk, extrair do CS:S | github.com/craftablescience/VPKEdit |
| Hammer++ | Hammer melhorado para o SDK 2013 | ficool2.github.io/HammerPlusPlus-Website (gratuito, código fechado) |

## Testar rápido

- Linha de comando útil: `-dev -console -novid -windowed -w 1600 -h 900 +sv_cheats 1 +map de_dust2`.
- `developer 1`, `con_filter_enable`, `cl_showfps 1`, `net_graph 1`, `mat_wireframe 1`.
- Servidor dedicado de teste: Source SDK Base 2013 Dedicated Server (appid 244310), `srcds -game mod_trok`.

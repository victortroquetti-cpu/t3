# Conteúdo próprio: trocar tudo que é do CS:S e do HL2

Decisão do dono (09/11/2026): **tem que ser outro jogo.** Sons, animações, modelos e mapas
próprios. Estilo: sucessor de alma do Combat Arms, arcade, moderno, sem fotorrealismo, visual
bonito e iluminação boa sem exagero. Referência de nível de acabamento: NEOTOKYO Rebuild.

**Direção de arte e lore** (o que criar e com que cara): `design/lore-e-arte.md`.

O código já está pronto para a troca: cada item abaixo diz **onde** se troca. Quando tudo
estiver trocado, o CS:S sai do `gameinfo.txt` e o Trok roda só com o Source SDK Base 2013
(grátis para qualquer um na Steam).

## Inventário (o que ainda é de outro jogo)

| # | Item | Hoje vem de | Onde trocar | Prioridade |
|---|---|---|---|---|
| 1 | Sons de tiro | CS:S montado | `scripts/game_sounds_trok.txt` (`Trok.<Arma>.Tiro`) | alta, fácil |
| 2 | Som de arma vazia | CS:S | `game_sounds_trok.txt` (`Trok.<Arma>.Vazia`) | fácil |
| 3 | Sons de recarga e manuseio | eventos de animação dos modelos do CS:S | vêm junto com o modelo de arma novo (eventos no `.qc`) | com as armas |
| 4 | Armas em 1ª pessoa (com mãos) | `v_*.mdl` do CS:S | `viewmodel` no `scripts/weapon_trok_*.txt` | alta, difícil |
| 5 | Armas em 3ª pessoa / no chão | `w_*.mdl` do CS:S | `playermodel` no script da arma | junto com a 4 |
| 6 | Faca | pé de cabra do HL2 (`weapon_crowbar`) | arma nova `weapon_trok_faca` (base `CBaseHL2MPBludgeonWeapon`) + modelo + sons | média |
| 7 | Modelos de jogador e animações de 3ª pessoa | HL2MP (Combine/Rebeldes, esqueleto ValveBiped) | modelo próprio + `.qc` com as atividades do HL2MP; a lista de modelos fica em `hl2mp_player.cpp` (patch) | alta, difícil |
| 8 | Passos | superfícies do HL2 (`Default.StepLeft`...) | sobrescrever os nomes em `game_sounds_trok.txt` (exemplo no arquivo) | média, fácil |
| 9 | Dor, morte, respiração do jogador | HL2MP | sobrescrever os nomes no `game_sounds_trok.txt` | média |
| 10 | Sons do menu | HL2 (`ui/buttonclick.wav`) | ConVars `trok_ui_som_clique` / `trok_ui_som_passar` | fácil |
| 11 | Som do marcador de acerto | HL2 (`buttons/*.wav`) | ConVars `trok_hitmarker_som` / `_som_abate` | fácil |
| 12 | Mapas | CS:S / HL2MP | mapas próprios em `maps/` (Hammer), com nav mesh | alta, o maior trabalho |
| 13 | Texturas dos mapas | CS:S / HL2 | materiais próprios (`materials/trok/...`) | junto com os mapas |
| 14 | Efeitos (fogo do cano, rastro, sangue, impacto) | partículas e sprites do HL2 | `particles/` e `materials/effects/` próprios | média |
| 15 | Fundo do menu | nenhum (fundo liso) | um `background01.bsp` próprio + `scripts/chapterbackgrounds.txt` | baixa |
| 16 | Fontes, HUD, menu, placar, carregamento | **já são do Trok** (Barlow OFL, código próprio) | — | feito |

## De onde tirar (anotar no `CREDITOS.txt`)

- **Sons de arma, impacto, ambiente:** pacotes "Sonniss GDC Game Audio Bundle" (sem crédito
  obrigatório), Freesound ou gravar e editar. Formato: WAV 44,1 kHz 16 bits (mono para som no mundo).
- **Sons de interface:** Kenney (CC0).
- **Modelos e animações estilo arcade:** Quaternius (CC0, low-poly com animação), Kenney (CC0),
  Sketchfab; Mixamo para animações de jogador (redirecionar no Blender). Arma em 1ª pessoa boa
  normalmente é feita sob encomenda ou comprada.
- **Texturas:** Poly Haven e ambientCG (CC0); para o visual arcade, simplificar (menos detalhe
  fino, cores um pouco mais saturadas).

Os sites acima estão bloqueados na rede da nuvem; baixar e escolher fica para o Claude Code local.

## Pipeline (local)

1. Som: editar no Audacity, exportar WAV, pôr em `mod_trok/sound/trok/...`, apontar o nome no
   `game_sounds_trok.txt`. Testar com `playgamesound Trok.Fuzil.Tiro` no console.
2. Modelo: Blender + Blender Source Tools → SMD/DMX → `.qc` → `studiomdl` (vem no SDK Base 2013
   em `bin/`). Arma em 1ª pessoa: atividades `ACT_VM_DRAW`, `ACT_VM_PRIMARYATTACK`,
   `ACT_VM_RELOAD`, `ACT_VM_IDLE`; eventos de som no `.qc` (`{ event AE_CL_PLAYSOUND <frame> "Trok.Fuzil.Pente" }`).
3. Jogador: modelo com as atividades de 3ª pessoa que o HL2MP usa (as tabelas `acttable` das
   armas do Trok) para os bots e jogadores animarem certo.
4. Mapa: Hammer com as medidas de `design/medidas-mapas.md`; `vbsp`/`vvis`/`vrad` com HDR;
   `env_tonemap_controller` e `color_correction` para a cor; depois `nav_generate`.
5. Quando os itens 1 a 14 estiverem trocados: tirar as linhas do CS:S (`appid_240`) do
   `gameinfo.txt` e testar o mod sem o CS:S instalado.

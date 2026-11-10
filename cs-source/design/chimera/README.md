# Chimera: cópia das especificações e dos dados

- **Origem:** https://github.com/r0pi0x/Chimera, commit `e84e415ff7a431e6b45c86656b989f692bbae202` (copiado em 10/11/2026).
- **Licença:** MIT ou Apache-2.0 (`LICENSE-MIT`, `LICENSE-APACHE`). Crédito em `game/mod_trok/CREDITOS.txt`.
- **Decisão do dono (10/11/2026):** usar como referência de números e regras, como o DeCombat.
- **Atenção:** conferir com o DeCombat (`../decombat/`) e com o jogo quando os números brigarem.
- **O que ficou de fora:**
  - o código Rust do motor dele;
  - `games/originals`, que são minigames próprios dele.
- O `README-original.md` é o README do projeto.
- Unidades: as specs do CA usam as unidades do LithTech (Y para cima, mundo de cerca de 40 u por metro). Ver `specs/combat_arms/movement.md`.

## Onde achar cada coisa (o que mais serve ao Rearmed)
| Assunto | Arquivo |
|---|---|
| Movimento do CA (já portado em `trok_gamemovement.cpp`) | `specs/combat_arms/movement.md`, `games/combat_arms/data/movement.toml` |
| Armas: tiro, recuo, dispersão, dano | `specs/combat_arms/weapon_firing.md`, `weapon_classes.md`, `games/combat_arms/data/weapons/*.toml` |
| Granadas, minas, claymore, lança-granadas | `specs/combat_arms/grenades.md`, `special_weapons.md` e os `.toml` de cada uma |
| Atributos (vida, empurrão `DamageMovePenalty`, colete) | `specs/combat_arms/attribute_data.md`, `gear_perks.md`, `games/combat_arms/data/gear.toml` |
| Modos (inclusive Quarentena/infecção) | `specs/combat_arms/game_modes.md`, `game_modes_2.md`, `games/combat_arms/data/modes.toml` |
| Killstreak e frases de abate especial | `specs/combat_arms/kill_streaks.md`, `games/combat_arms/data/announcer.toml` |
| Bots (perfis por dificuldade) | `specs/combat_arms/bots.md`, `bots_objectives.md`, `games/combat_arms/data/bots.toml` |
| Animação de personagem e de primeira pessoa | `specs/combat_arms/animation.md`, `view_models.md`, `characters.md` |
| HUD, mira, radar, menus, opções | `hud_crosshair.md`, `radar_minimap.md`, `ui_system.md`, `options.md`, `lobby.md` |
| Morte, câmera de morte, killcam | `specs/combat_arms/death_killcam.md` |
| Sons, música, rádio | `effects_sounds.md`, `music_ambience.md`, `radio_chat.md` |
| Formatos de arquivo do CA (`.rez`, `.ltb`, `.dtx`, `.dat`) | `rez_archive.md`, `models_ltb.md`, `textures_dtx.md`, `world_dat.md` |
| CS:S medido (armas, movimento, granadas, queda, `.nav`, sons) | `specs/cs_source/` e `games/cs_source/data/` |
| HL2DM: o que muda em relação ao CS:S (nossa base de código) | `specs/hl2dm/source_deltas.md` e os outros de `specs/hl2dm/` |
| Entidades do Source (gatilhos, portas, I/O, física, fogo) | `specs/source/` |

## Para o Claude Code local
- Os números servem para preencher os valores MEDIR dos `cfg` e dos `scripts/weapon_trok_*.txt` (Fase 3).
- Dados do CA em `.toml`: um arquivo por arma em `games/combat_arms/data/weapons/`. Os campos seguem os atributos do
  jogo (ver `attribute_data.md`).

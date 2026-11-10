# Catálogo de assets e código grátis

Pesquisa de 09/11/2026 para o Claude Code local não começar do zero. Tudo grátis. Feita só por busca na web.
Complementa `design/assets-gratuitos.md` e `design/conteudo-proprio.md` (o que trocar e onde).

## Regras antes de baixar qualquer coisa

1. **Crédito:** todo CC-BY vai para `game/mod_trok/CREDITOS.txt` (nome, autor, link, licença) no mesmo
   commit em que o asset entra.
2. **Visual:** sóbrio, gasto, militar (ver `design/lore-e-arte.md`). Pacote low-poly ou estilizado é **só
   provisório**, para testar o pipeline.
3. **Polígonos:** viewmodel de arma no Source 1 fica bem em 5 a 15 mil triângulos. Modelos de 30 mil ou mais
   precisam de redução (decimate) no Blender, e as texturas baixam para 2K.

---

## 1. Armas (modelos 3D)

### Primárias
| Prioridade | Item | Licença | Nota |
|---|---|---|---|
| ★ | **Gintoki1234**: M16A4, MP5 A2, Remington 870, Barrett M82A1 (gintoki1234.itch.io: `/m16-a4`, `/mp5-a2`, `/remington-pump-action-shotgun`, `/barrett-m82`) | CC BY 4.0 | Realistas e de alta qualidade. Precisam de redução (Barrett ~31,7k tris, texturas 4K) |
| ★ | **3DModelsCC0: Free CC0 Guns & Explosives Pack** (3dmodelscc0.itch.io/free-cc0-guns-explosives-pack): AK-47, M4A1, sniper, escopeta, Suomi, Luger, granadas, claymore, C4 | CC0 | Cobre muita coisa de uma vez. Qualidade não verificada |
| ★ | **Stein Games: Free Classic Weapons Pack** (stein-indie.itch.io/classic-weapons-pack): AK-47, MP5, Mac10, M700 (ferrolho), M14, Tec9, 1911 | CC0 provável | Realista, 2K, alto poly |
| | KRISS Vector (Antonio Castro), sketchfab.com/3d-models/kriss-vector-79c37c9eedc444d8a146043045e7cf3e | CC-BY | 16,8k tris |
| | AK-47 Classic, sketchfab.com/3d-models/ak-47-classic-assault-rifle-kalashnikov-cf9c771eb89a47f797f55e6520f50cb6 | CC-BY | ~10k vértices, bom tamanho |
| | dan741vlasov: "Used AK 47 GameReady" e "AKM Lowpoly", sketchfab.com/dan741vlasov/models | CC (BY ou BY-NC) | Visual **gasto**, combina |
| | **Firearms_Kit 1.0**, sketchfab.com/3d-models/firearms-kit--10-cc0-386f0582c28d4dff96da967f42ba20f7 | CC0 segundo o autor, CC-BY no Sketchfab | Peças para montar armas **originais** (kitbash). ~1,3M tris |
| prov. | Chilly-durango Low Poly Firearms; Quaternius Ultimate Gun Pack; Pichuliru Flat Guns West/East + Attachments | CC0 | Estilizados: só blockout |

**Lacunas:** M249/LMG, FAMAS, MP7, UMP, G36, AUG e SCAR sem opção CC. Para G36, AUG e SCAR, só peças no Firearms Kit.

### Secundárias
| Item | Licença | Nota |
|---|---|---|
| Gintoki1234 Glock 19 Gen 4 e S&W 629 .44 (revólver) | provável CC BY 4.0 | Realistas |
| 1911 do pacote Stein | CC0 provável | |
| M1911 Game Ready (fadlparlak), sketchfab.com/3d-models/m1911-pistol-game-ready-bdc0fe27306d48b0b2b23967b159971c | — | 8,5k tris, ideal |
| M1911 "Post Apocalyptic" (creationwasteland) | CC-BY (de agregador) | Visual gasto |
| Desert Eagle Mark I (TheDevilsEye, Sketchfab) | CC BY 4.0 (de segunda mão) | Achar a página original |
| Glock 17 low-poly (katachromai.itch.io/glock-17) | CC0 | Provisório |

### Corpo a corpo
| Item | Licença | Nota |
|---|---|---|
| ★ **3DModelsCC0: Free CC0 Melee Weapons Pack**: faca de combate, machete, tomahawk, machado de incêndio, pé de cabra, cano | CC0 | O melhor da categoria |
| ShareTextures knife_1 a knife_5 (sharetextures.com/models/weapons/knife_1) | CC0 | Realistas, PBR |
| OpenGameArt "Various Melee Weapons", "Knife Low-poly", "PBR Machete" | CC0 | Reserva |

### Explosivos e equipamentos
| Item | Licença | Nota |
|---|---|---|
| ★ Pacote 3DModelsCC0 (acima): granada de fragmentação, flash, fumaça, **claymore**, **C4** | CC0 | |
| ShareTextures landmine_1 a landmine_4 | CC0 | Mina terrestre realista |
| Summer Engine: frag, claymore e **C4 com detonador** (summerengine.com/asset-store/library-explosive-c4.md etc.) | CC0 1.0 | Qualidade não verificada. Único detonador achado |
| OpenGameArt "Low Poly RPG7" | CC0 | Low-poly. Alternativa: Flipped Normals RPG-7 grátis |
| Sentinela/torreta: "large caliber turret", sketchfab.com/3d-models/large-caliber-turret-d62842657df7495b948778d92044c3fe | CC-BY | Metralhadora animada. **Não há sentinela CC melhor** |
| Kenney Weapon Pack (opengameart.org/node/53259) | CC0 | Estilizado, provisório |

### Armas da lore (corporação e experimentais)
| Item | Licença | Nota |
|---|---|---|
| ★ **AR-H470** (Frostoise), sketchfab.com/3d-models/ar-h470-4b38c160243743d681d6fdaf3bd77ddb | CC-BY | 4,7k tris. **Protótipo da corporação** (linhas limpas; pôr detalhes em ciano) |
| plopsis "Retro scifi gun" e "Scifi blaster" (plopsis.itch.io) | CC0 | **Candidatos à arma "brisa"** (tipo raygun) |
| Sci-Fi Challenger (a1mercet.itch.io/sci-fi-challenger) | CC0 | Conferir visual |
| OpenGameArt "sci-fi gun", "Sci-Fi Rifle"; Peu Game Assets Blasters; Quaternius Sci-Fi Essentials | CC0 | Bases para retrabalhar |
| rcorre SciFi Energy Weapon Pack; Penumbra Free Sci-Fi Gun Pack | — | Reserva |

### Braços em primeira pessoa
| Item | Licença | Nota |
|---|---|---|
| ★ **MakeHuman** (gerar corpo e cortar os braços) | exportação CC0 | Melhor a longo prazo: braços próprios, vestir luva e manga táticas, rig no ValveBiped |
| OpenGameArt "fps arms (rigged only)"; DevMops Low Poly Arms | CC0 | Bases cruas |
| Provisório atual | — | `c_arms` do HL2DM montado |

---

## 2. Personagens, zumbis e animações

| Prioridade | Item | Licença | Nota |
|---|---|---|---|
| ★ | **Mixamo** (mixamo.com) | Livre em jogos | Rig e animações em 3ª pessoa: fuzil, pistola, morrer, granada, faca, **zumbi** |
| ★ | **Quaternius Universal Animation Library 1 e 2** (quaternius.itch.io) | CC0 | 120+ animações; a 2 tem **parkour** (bom para bots e movimento) |
| ★ | **Quaternius Universal Base Characters** | CC0 | Corpos base com o mesmo rig. Precisam de roupa e equipamento próprios |
| | Quaternius Zombie Apocalypse Kit | livre / CC0 | 4 sobreviventes, 4 zumbis, 2 cães: provisório do Surto (cartunesco) |
| | Sketchfab "Character Type Tactical Man 1" (CC-BY) | CC-BY | Candidato a mercenário; conferir qualidade |
| | Cientista de hazmat (benng.gumroad.com/l/BSNOQ) | "Creative Commons" (qual?) | Rigado; laboratório da corporação |
| | Zumbis Sketchfab CC-BY: "Zombie animation test for the game" (4K), "Zombie corpse", "Walking zombie", "Zombi" | CC-BY | Os mais realistas; ver rig e qualidade um a um |
| | MoCap Online Free Demo (mocaponline.itch.io/mocap-online-demo) | royalty-free | Amostras realistas de fuzil e zumbi |
| | CMU Motion Capture (mocap.cs.cmu.edu) | pode usar | Locomoção e mortes; dá trabalho limpar |
| | Rokoko (150 animações grátis), Truebones | royalty-free; Truebones pede crédito | Complementos |

**Retarget para o ValveBiped:** não existe script pronto Mixamo→ValveBiped. Partir de
`enziop/mixamo_converter` ou do gist de renomear ossos (gist.github.com/naelstrof/d2747f7838ef2f0fb626a420ef50c8ce),
usando a lista de ossos de wiki.facepunch.com/gmod/ValveBiped_Bones. **Não basta renomear: a orientação dos
ossos precisa bater** (ou ajustar `$definebones` no QC). Ferramentas: Blender Source Tools e Crowbar.
Animação de arma em 1ª pessoa não existe pronta e boa: vai ser feita à mão no Blender.

**Fazer e editar animação (grátis):**
- **Blender:** animação completa (keyframes, curvas, NLA, IK, retarget, limpeza de mocap). Exporta as
  sequências em SMD pelo Blender Source Tools.
- **Blender MCP:** roda Python no Blender e tira print da viewport.
  - Rende bem no trabalho mecânico: retarget e renomear ossos, cortar, fazer loop, mudar velocidade, exportar
    em lote e animações simples por código (balanço parado, chute do recuo, sacar e guardar a arma).
  - Animação artística (recarga com os dedos, atuação) sai dura. Ela pede iteração com prints ou a mão do dono.
- **Rokoko** (plano Starter grátis): texto vira movimento (clipes ilimitados, 5 importações por mês) e vídeo
  vira mocap (cerca de 30 s por mês). Dá para filmar com o celular os movimentos do zumbi.
- **Cascadeur grátis:** só exporta no formato dele, sem FBX.

**Sem opção boa e grátis:** personagem militar realista e rigado. Caminho: base
(MakeHuman, Mixamo ou Quaternius) mais equipamento próprio.

---

## 3. Sons e música

| Prioridade | Item | Licença | Uso |
|---|---|---|---|
| ★ | **The Free Firearm Sound Library** (opengameart.org/content/the-free-firearm-sound-library) | **CC0** | **Base dos tiros**: centenas de armas reais gravadas |
| ★ | **Sonniss GDC Bundles** (sonniss.com/gameaudiogdc) | royalty-free, sem crédito | Explosões, foley, impactos, ambiências. Procurar armas na lista de cada ano |
| | Cyrex Studios Gun Handling Sound Pack | CC-BY 4.0 | Recarga e ferrolho (AR, AK, pistola) |
| | OpenGameArt "Gun Reload Sound Effects", "gunshot sounds" | CC0 | Complemento |
| ★ | Zumbis OpenGameArt: "Zombies Sound Pack" (24), "Zombie noises and moans", "Enemy sounds" | CC0 | Vozes do Surto |
| | Zumbis itch: Catastic Zombie Sound Pack Free (40+), Endalon, VoiceBosch, Dark Fantasy Studio | varia | Complemento |
| ★ | Alarme: OpenGameArt "Sirens and alarm noise"; BigSoundBank "Evacuation Alarm #1" | CC0 | Alarme do Surto |
| | BigSoundBank (ambiência industrial, sirenes) | varia por som | Laboratório, fábrica |
| ★ | Kenney Interface Sounds + Impact Sounds | CC0 | Cliques do menu e do HUD |
| | Passos: OpenGameArt "Footsteps on different surfaces" | CC-BY 3.0 | Concreto, cascalho, metal |
| | ZapSplat (conta grátis) | **crédito obrigatório** | Acervo grande de armas e impactos |
| | Pixabay, Mixkit | livre | Complemento |
| | Música do menu: Kevin MacLeod (incompetech, CC BY 4.0); Zhelanov "Frontier" (CC-BY 3.0); Eric Matyas "Mech Game Menu" (CC-BY 4.0) | CC-BY | Menu e carregamento |

**Lacunas:** zunido de bala passando, pino e quique de granada, tiro com supressor. Garimpar no Sonniss ou no ZapSplat.

---

## 4. Código aberto de sistemas

**Descoberta principal:** o SDK de 2025 que já usamos traz **o código inteiro do TF2**, com HL2 e HL2MP.
Muito do que falta já está em `cs-source/sdk/src/game/`. O código do TF2 depende de `CTFPlayer`, itens e atributos:
reaproveitar é adaptar, não copiar e colar.

| Sistema | Onde (no SDK ou em `refs/`) | Nota |
|---|---|---|
| Granada de fragmentação | `shared/hl2mp/weapon_frag.cpp`, `server/hl2/grenade_frag.cpp` | Base HL2MP, a mais direta |
| C4 remoto, mina a laser (base de claymore) | `shared/hl2mp/weapon_slam.cpp`, `server/hl2/grenade_tripmine.cpp`, `grenade_satchel.cpp`; NT;RE `shared/neo/weapons/weapon_detpack`, `weapon_proxmine` | NT;RE já é MP de 64 bits |
| Fumaça | NT;RE `weapon_smokegrenade` | |
| RPG | `shared/hl2mp/weapon_rpg.cpp`; TF2 `tf_weapon_rocketlauncher.cpp` (com predição) | |
| Sentinela | TF2 `server/tf/tf_obj_sentrygun.cpp` + `client/tf/c_obj_sentrygun.cpp`; HL2 `npc_turret_floor.cpp` | |
| Faca com backstab | TF2 `shared/tf/tf_weapon_knife.cpp`; NT;RE `weapon_knife` | |
| Luneta | TF2 `tf_weapon_sniperrifle.cpp` + `client/tf/tf_hud_scope.cpp` | |
| Granadas com script (luz/concussão etc.) | Fortress Forever `game_shared/ff/ff_grenade_*` + `ff_grenade_parse` | Organização antiga do SDK; adaptar |
| Penetração de parede | NT;RE `shared/neo/neo_penetration_resistance.cpp` | A melhor opção |
| Contagem de killstreak | TF2 `shared/tf/tf_player_shared.h` (`ETFStreak`, `m_nStreaks`), medidor `client/tf/tf_hud_itemeffectmeter.cpp` | Recompensas: esperar o dono definir |
| Coisa que cai do céu (base para ataque aéreo e pacote) | `server/hl2/env_headcrabcanister.cpp`; chuva de meteoros em `shared/tf/halloween/tf_weapon_spellbook.cpp` | |
| UAV | — | Mais simples: revelar inimigos no nosso radar |
| Pedaços do corpo, decapitação | TF2 `client/tf/c_tf_player.cpp` (`CreateTFGibs`) | |
| **Zumbi completo em NextBot** | TF2 `server/tf/halloween/zombie/` (behaviors, spawner, ataque especial) | Ótimo para os zumbis bots |
| Diretor de ondas | TF2 `server/tf/player_vs_environment/tf_populators.cpp` (MvM) | Para o modo em ondas futuro |
| Lag compensation de NPC | ZMR `src/game/server/zmr/npcs/zmr_npclagcomp.h` | O SDK só compensa jogadores |
| Tela "morto por", resultado da rodada | TF2 `client/tf/tf_hud_freezepanel.cpp`, `tf_hud_winpanel.cpp`, `vgui/tf_statsummary.cpp` | "Tela de resultado" ainda não foi pedida |
| Menu, opções, lista de servidores, placar | NT;RE `src/game/client/neo/ui/` (`neo_root_settings.cpp`, `neo_root_serverbrowser.cpp`...) | "Opções próprias" ainda não foi pedido |
| Ferramentas de depuração | `Source-SDK-Resources/source-2013-imgui` | MIT |
| **PBR (gráfico melhor)** | ZMR `src/materialsystem/stdshaders/pbr_dx9.cpp` + `game_shader_zmr.vpc` | Provavelmente já funciona em 64 bits; o caminho mais curto |
| Color correction por estado | ZMR `client/zmr/c_zmr_cc_*.cpp` | Morte, vida baixa |

---

## 5. UI

| Item | Licença | Uso |
|---|---|---|
| Kenney Crosshair Pack (200 miras), Kenney Game Icons | CC0 | Mira e ícones |
| game-icons.net | CC BY 3.0 (crédito por autor) | Ícones de arma, caveira, explosivo |
| Google Material Symbols / Lucide / Tabler / Phosphor | Apache-2.0 / ISC / MIT / MIT | Ícones de menu |
| Fontes OFL (Google Fonts) | OFL | Já usamos Barlow. Candidatas: **Saira Stencil One**, **Big Shoulders Stencil**, Rajdhani, Teko, Chakra Petch, Share Tech Mono, Oswald, Bebas Neue. **Evitar** (comerciais): Eurostile, Bank Gothic, Purista |
| Game UI Database, interfaceingame.com | — | Só referência visual |

---

## Ordem sugerida quando o jogo rodar

1. Pipeline com um item CC0 simples (faca do pacote 3DModelsCC0): Blender → Blender Source Tools → QC →
   `modelo_compilar` (MCP `trok`) → ver no jogo.
2. Sons de tiro (Free Firearm Sound Library) e de zumbi (OpenGameArt) no lugar dos do CS:S, por
   `game_sounds_trok.txt`. É o maior ganho com o menor trabalho.
3. Uma arma realista (Gintoki1234 M16A4 ou MP5) reduzida e com braços próprios.
4. Explosivos e o resto, conforme o dono pedir as armas novas.

# Animações: como reformar todas (3ª pessoa, zumbis e 1ª pessoa)

Pesquisa de 10/11/2026 (nuvem). Nada aqui foi testado no jogo. O que não foi confirmado está
marcado como **a conferir**. Complementa `design/catalogo-assets.md` (seção 2, que já lista Mixamo,
Quaternius, CMU, Rokoko, Blender e Blender MCP) e `design/ca-ferramentas.md` (conversão de `.ltb`).

## Resumo (o mais importante)

1. **Hoje o HL2MP anima o jogador do jeito antigo.** O servidor escolhe a sequência (`SetAnimation`
   em `hl2mp_player.cpp`). Existe **uma só corrida** (`ACT_HL2MP_RUN`), mais lenta ou mais rápida
   conforme a velocidade. Não há andar, sprint, aterrissagem, reação a dano nem animação de morte (morre e
   vira ragdoll na hora).
2. **O zumbi já pode ter cara de zumbi sem código.** As garras usam as atividades `*_MELEE`. Um modelo de
   zumbi cujas sequências de zumbi tenham essas atividades no `.qc` já anda e bate como zumbi.
3. **Para animação boa (9 direções, sprint, pulo em 3 fases, reação a dano) é preciso trocar o
   animstate** pelo `CMultiPlayerAnimState` (o do TF2, que está no SDK). Já existem dois portes com licença
   compatível: o `hl2mp_playeranimstate` do NEOTOKYO;Rebuild e o do Zombie Master: Reborn.
4. **Viewmodel:** as armas de hoje são `v_*.mdl` do CS:S, com os braços embutidos. O sprint da arma é
   feito em código (`AddViewmodelBob`). Para braços próprios trocando de roupa, o ZMR tem código pronto
   de "braços separados" (segundo viewmodel com bonemerge).
5. **O CA tem números bons de referência**: duração de cada animação de arma, tempos de corrida e
   pulo, regras de mira e de morte. Estão na seção 2.
6. **A fonte de animação mais segura é CC0** (Quaternius, Kenney), mais o **100STYLE** (CC BY 4.0,
   centenas de estilos de andar e correr) e o CMU. O **Mixamo** pode ser usado, mas o FBX cru fica fora do
   git. Fab e Unity só servem com cuidado (seção 4).

---

## 0. Estado de hoje (o que o código usa)

| Parte | Hoje | Onde |
|---|---|---|
| Jogador (3ª pessoa) | modelos do HL2MP (Combine e Rebeldes, esqueleto ValveBiped) | lista em `hl2mp_player.cpp` (`g_ppszRandomCitizenModels`, `g_ppszRandomCombineModels`) |
| Animação do jogador | `CPlayerAnimState` simples do HL2MP | `sdk/src/game/shared/hl2mp/hl2mp_player_shared.cpp` e `SetAnimation` em `server/hl2mp/hl2mp_player.cpp` |
| Tabela de atividades das armas | `acttable` só no servidor (`#ifndef CLIENT_DLL`) | `src/game/shared/trok/weapon_trok_gun.cpp` (AR2, SMG1, PISTOL, SHOTGUN), `weapon_trok_garras.cpp` (MELEE) |
| Arma em 1ª pessoa | `v_*.mdl` do CS:S (braços embutidos) | `viewmodel` em `game/mod_trok/scripts/weapon_trok_*.txt` |
| Animações de arma usadas | `ACT_VM_DRAW` (padrão do SDK), `ACT_VM_PRIMARYATTACK`, `ACT_VM_RELOAD`; garras: `ACT_VM_HITCENTER` e `ACT_VM_MISSCENTER` (base `CBaseHL2MPBludgeonWeapon`) | `weapon_trok_gun.cpp`, `weapon_hl2mpbasebasebludgeon.cpp` |
| Correr com a arma abaixada, balanço, mira | **procedural** (código), não animação | `CWeaponTrokGun::AddViewmodelBob` |
| Zumbi do Surto | modelo do Combine com tom verde e faca do CS:S como garras | `design/modo-surto.md` |

Obs.: o `catalogo-assets.md` diz "`c_arms` do HL2DM montado" como provisório dos braços. No código do
SDK local não há nada de braços separados: o HL2MP só pré-carrega `models/weapons/v_hands.mdl`.
**A conferir** se o HL2DM de hoje traz `c_arms`.

---

## 1. Como o Source faz

### 1.1 Conceitos (curto)

- **Sequência** = uma animação compilada no `.mdl` (`$sequence` no `.qc`). Pode ser simples ou uma
  **mistura** (blend) de várias, controlada por pose parameters.
- **Atividade (activity)** = um rótulo de intenção (`ACT_HL2MP_RUN_AR2`). O código pede a atividade; o
  modelo diz qual sequência atende a ela (`$sequence ... ACT_X 1`). Várias sequências podem ter a mesma
  atividade com peso: o motor sorteia (`SelectWeightedSequence`). **Trocar o modelo troca a animação sem
  mexer no código**, desde que as atividades batam.
- **Tradução por arma**: o código pede `ACT_HL2MP_RUN` e a `acttable` da arma troca para
  `ACT_HL2MP_RUN_AR2`. Se o modelo não tiver a traduzida, o HL2MP tenta a genérica e, por fim, a
  sequência 0 (T-pose ou primeira animação).
- **Pose parameters** = números que o código ajusta a cada quadro e o modelo usa para misturar:
  - `move_yaw` (HL2MP: direção da corrida, −180 a 180, mistura em 8 direções);
  - `move_x` / `move_y` (TF2 e `CMultiPlayerAnimState`: mistura em **9 direções**, com o centro parado);
  - `aim_pitch` / `aim_yaw` (HL2MP) ou `body_pitch` / `body_yaw` (TF2): matriz de mira do tronco;
  - `head_yaw` / `head_pitch` (o cliente do HL2MP vira a cabeça para quem está perto e pisca);
  - `move_scale` (TF2, opcional).
- **Camadas (gestos)**: animações por cima da principal, em "slots" (`AddGesture`, `RestartGesture`).
  É assim que se atira e recarrega correndo. Normalmente são **delta** (só a diferença para uma pose
  base) e pesam só no tronco (`$weightlist`). No TF2 os slots são: ataque/recarga, granada, pulo, nado,
  dano (flinch), cena (VCD) e livre (`GESTURE_SLOT_*` em `multiplayer_animstate.h`).
- **Velocidade da passada**: a sequência de corrida tem velocidade própria (o movimento do osso raiz,
  `LX LY`). O código ajusta a velocidade de reprodução = velocidade real ÷ velocidade da sequência
  (`ComputePlaybackRate`). Assim o pé não escorrega.
- **`$includemodel`**: o modelo do personagem não guarda as animações; ele inclui um `.mdl` só de
  animações (no HL2: `humans/male_shared.mdl`, `male_ss.mdl`, `male_gestures.mdl`, `male_postures.mdl` e
  `player/male_anims.mdl`). Vários corpos com o mesmo esqueleto usam a mesma biblioteca. É o equivalente
  do "child model" do CA.
- **Ragdoll de morte**: o `.mdl` tem física por osso (`$collisionjoints` / `$jointconstrain`, gera o
  `.phy`). O HL2MP cria um `CHL2MPRagdoll` (`CreateRagdollEntity` em `Event_Killed`) que começa na pose do
  momento e leva a força do tiro. **Não toca animação de morte.**
- **Viewmodel (1ª pessoa)**: outro `.mdl` (arma + braços) com atividades `ACT_VM_*`. A arma chama
  `SendWeaponAnim(ACT_VM_...)`. Eventos de som e efeito vão no `.qc` (`{ event AE_CL_PLAYSOUND <quadro> "Som" }`).

### 1.2 O que o HL2MP espera de um modelo de jogador (lista real)

Tirada de `SetAnimation` (`server/hl2mp/hl2mp_player.cpp`) e das `acttable` do Trok e do HL2MP.

**Atividades base** (o código pede estas e a arma traduz):

| Situação | Atividade base |
|---|---|
| parado | `ACT_HL2MP_IDLE` |
| andando ou correndo (qualquer velocidade) | `ACT_HL2MP_RUN` |
| agachado parado | `ACT_HL2MP_IDLE_CROUCH` |
| agachado andando | `ACT_HL2MP_WALK_CROUCH` |
| no ar (pulo, queda) | `ACT_HL2MP_JUMP` |
| tiro (gesto) | `ACT_HL2MP_GESTURE_RANGE_ATTACK` |
| recarga (gesto) | `ACT_HL2MP_GESTURE_RELOAD` |

**Sufixos por tipo de arma** (cada base acima ganha um destes; existem no `ai_activity.h`):
`_PISTOL`, `_SMG1`, `_AR2`, `_SHOTGUN`, `_RPG`, `_GRENADE`, `_PHYSGUN`, `_CROSSBOW`, `_MELEE`, `_SLAM`.
O Trok usa hoje:

| Arma do Trok | Sufixo |
|---|---|
| fuzil | `_AR2` |
| submetralhadora | `_SMG1` |
| pistola | `_PISTOL` |
| escopeta | `_SHOTGUN` |
| garras do zumbi | `_MELEE` |
| faca e granada (futuro) | `_MELEE` e `_GRENADE` |

Ou seja, por tipo de arma: `ACT_HL2MP_IDLE_X`, `ACT_HL2MP_RUN_X`, `ACT_HL2MP_IDLE_CROUCH_X`,
`ACT_HL2MP_WALK_CROUCH_X`, `ACT_HL2MP_JUMP_X`, `ACT_HL2MP_GESTURE_RANGE_ATTACK_X`,
`ACT_HL2MP_GESTURE_RELOAD_X`: **7 por arma**. Para as 5 armas de hoje, 35 atividades.

**Pose parameters que o HL2MP procura**: `move_yaw`, `aim_pitch` (−90 a 90), `aim_yaw` (o código
limita a ±60), `head_yaw`, `head_pitch`. Se não existirem, a parte correspondente só não mexe.

**Regras do HL2MP que dão o "jeito" atual:**
- o corpo só gira atrás da mira depois de 45° de diferença ou depois de `mp_facefronttime` parado;
- a sequência é escolhida no **servidor** e vai pela rede (o cliente só ajusta os pose parameters);
- o pulo é uma animação só, até tocar o chão;
- tiro e recarga são gestos (camada) e não cortam as pernas.

### 1.3 Estender o animstate (com `#ifdef TROK`)

Dois caminhos:

**A) Manter o sistema do HL2MP e só acrescentar** (pouco código):
- patch em `SetAnimation` (`#ifdef TROK`) para pedir atividades novas: andar (C), sprint (Shift),
  escalar, aterrissar;
- atividades novas como **privadas**, registradas no código do mod (`REGISTER_PRIVATE_ACTIVITY`
  ou `ActivityList_IndexForName("ACT_TROK_SPRINT")`). Assim o `ai_activity.h` da Valve não muda. O
  nome que aparece no `.qc` vira atividade privada ao carregar o modelo. **A conferir** se o número da
  atividade bate entre cliente e servidor; como a rede manda a sequência e não a atividade, não deve
  importar;
- continua sem 9 direções, sem pulo em fases e com o atraso de rede na animação.

**B) Trocar pelo `CMultiPlayerAnimState`** (recomendado, mais trabalho):
- `src/game/shared/Multiplayer/multiplayer_animstate.*` já está no SDK de 2025 (base do TF2). Ele traz:
  - 9 direções (`move_x`, `move_y`);
  - pulo em 3 fases (`ACT_MP_JUMP_START`, `ACT_MP_JUMP_FLOAT`, `ACT_MP_JUMP_LAND`);
  - `ACT_MP_WALK`, `ACT_MP_RUN`, `ACT_MP_SPRINT`, agachar, nadar;
  - slots de gesto e reação a dano por parte do corpo (`PLAYERANIMEVENT_FLINCH_*`);
  - eventos `PLAYERANIMEVENT_*` mandados pela rede.
- Roda nos dois lados (cliente e servidor). O cliente anima os outros jogadores a partir da velocidade,
  sem esperar a sequência do servidor. O servidor roda o mesmo cálculo para as hitboxes baterem.
- **Já portado para o HL2MP** (licença Source 1 SDK, pode copiar com crédito em `CREDITOS.txt`):
  - `refs/neotokyo-rebuild/src/game/shared/hl2mp/hl2mp_playeranimstate.*` (código da Valve; parece ter vindo
    do SDK 2013 antigo, que o SDK de 2025 não traz mais: **a conferir**);
  - `refs/zmr-game/src/game/shared/zmr/zmr_playeranimstate.*`. É o melhor ponto de partida: detecta se o
    modelo é de 9 direções (`move_x`/`move_y`) ou de 8 (`move_yaw`) e funciona com os dois. Usa os nomes do
    HL2MP (`aim_pitch`, `aim_yaw`), vira a cabeça e traduz `ACT_MP_*` para `ACT_HL2MP_*` na tabela das armas
    (`zmr/weapons/zmr_weaponconfig_anims.cpp`).
- **O que muda no código:**
  - classe nova `CTrokPlayerAnimState` em `src/game/shared/trok/` (arquivo nosso, sem `#ifdef`);
  - em `hl2mp_player.*` e `c_hl2mp_player.*` (`#ifdef TROK`): trocar o `CPlayerAnimState`, fazer
    `SetAnimation` virar `DoAnimationEvent` e mandar o evento aos outros clientes (TE de evento, como no TF2
    e no ZMR);
  - `acttable` das armas também no cliente (tirar o `#ifndef CLIENT_DLL`) e com linhas `ACT_MP_*` → `ACT_HL2MP_*`;
  - depois `python cs-source/trok.py save-patch`;
  - **números em cvar** (regra 4): tempo para o corpo virar, limite do giro do tronco, velocidade
    que conta como andar ou correr, mistura da aterrissagem.
- **Atividades nossas** que o caminho B permite (nomes provisórios): sprint por arma, escalar
  (`ACT_TROK_ESCALAR`), virada no ar, aterrissagem pesada, mortes direcionais (se o dono quiser
  animação antes do ragdoll).
- **Riscos:** predição do pulo e do gesto do próprio jogador, hitboxes iguais nos dois lados, custo de
  CPU com 32 jogadores (o TF2 aguenta). Testar com `cl_showerror 1` e `sv_showhitboxes` (**a conferir**
  o nome do comando de hitbox no SDK de 2025).

Bots: os bots NextBot do HL2MP são jogadores. Usam o mesmo animstate, então nada muda para eles.

---

## 2. O que o Combat Arms faz (só números e sensações)

Fonte: `design/chimera/specs/combat_arms/animation.md`, `characters.md`, `view_models.md`. "US" =
cliente de referência americano, "Steam" = instalação medida. O DeCombat (`design/decombat/`) não
cobre animação: só diz que o movimento não tem máquina de animação própria.

### 2.1 Como o CA anima (diferenças do Source)
- **Corpo dividido em 4 camadas**: base fixa, tronco (`U...`), pernas (`L...`) e agachar (peso =
  fração do agachamento, 0 a 1). Pesos: tronco 1 no tronco, braços e cabeça; pernas 1 no quadril e nas pernas;
  os dois ossos do meio misturam 0,7/0,3.
- **Pernas sempre na velocidade 1**: a passada **não** acompanha a velocidade real (no Source acompanha).
- **Nomes por arma × postura × ação × direção**: 8 direções por perna (andar, correr, agachado, pulo)
  e o sprint só para a frente e diagonais da frente.
- **Tronco e pernas em sincronia**: quando a direção muda, o tempo do tronco copia o das pernas.
- **Mira nos ossos depois da animação**: inclinação limitada a ±45°, giro do tronco a ±45° (0 enquanto
  anda).
  - Giro: 7/17 na coluna de baixo e 10/17 no peito.
  - Inclinação olhando para cima: tronco 0,15, peito 0,15, pescoço 0,25, ombros 0,45, cabeça 0,50.
  - Olhando para baixo: 0,35 / 0,45 / 0,10 / 0,10 / 0,35.
  - No Source isso equivale à matriz de mira (`aim_pitch`); dá para imitar o limite e a sensação.
- **Mistura entre animações**: quase sempre **0 ms** no masculino (corte seco), 200 ms nas reações a dano.
  No feminino, 100 a 200 ms em parado, agachado, mira, saque e recarga.
- **Reação a dano**: só no tronco, duas variantes sorteadas (`D_HIT` 467 ms, `D_HITT` 400 ms). Interrompe
  a pose de tiro durante esse tempo. Nos outros jogadores, um "coice" de inclinação de até 0,5 rad.
- **Morte sem ragdoll**: uma animação escolhida pela direção do tiro (frente, costas, lados; frente
  e costas com "forte" além de 45°), pela parte atingida (cabeça, virilha), pelo tipo de dano (explosão,
  queda, veneno, faca) e por cara ou coroa. O corpo fica até o dono renascer e some 5 s depois nos
  modos comuns.
- **Infectado**: pernas próprias (prefixo `V`: `VLST`, `VLRF`...), reação própria (`V_HIT`, `V_HITT`),
  "mãos" como arma branca. Há também um conjunto de pernas de "infectado com porrete" (`VG`).

### 2.2 Durações de 3ª pessoa (US, masculino; ms)

| Animação | ms | Nota |
|---|---|---|
| parado / agachado parado (`LST`, `LC`) | 4000 | laço |
| andar para a frente (`LWF`) | 1000 | laço |
| correr (`LRF`, `LRB`, `LRL`, `LRR`) | 600 | passos em 50 e 350 ms |
| agachado andando (`LCF`) | 1400 | |
| pulo (`LSJ`, `LJJ`, `LJB`) | 666 | não é laço; depois vira "caindo" |
| sprint (`LRRF`) | 500 (feminino 533) | |
| tronco: fuzil mirando (`URFRAmS`) | 4000 | laço |
| tronco: tiro do fuzil (`URFRFiS`) | 100 | |
| tronco: saque do fuzil (`URFRSSeS`) | 733 | |
| tronco: recarga do fuzil (`URFRSReS`) | 1866 | o servidor usa ÷ 1,2 como tempo da recarga |
| faca (`UKRFiS`, `UKRFiS2`) | 500, 800 | |
| granada (`UTRFiS_A`, `_C`) | 267, 900 | |
| mortes | 1233 a 3033 | `D_W` (queda) 7800 |

### 2.3 Primeira pessoa (viewmodel)

Regras:
- **Uma animação por ação, achada pelo nome**:
  - `select` (sacar), `idle_0` (parado), `idle_1` (parado completo, a cada 5 a 15 s sem andar);
  - `prefire`, `fire_0`, `postfire` (tiro), `dryfire`, `reload`;
  - `PreReload` / `reload` por cartucho / `PostReload` (escopeta);
  - `PreRush` / `rush` (laço) / `PostRush` (sprint).
- **Sem guardar a arma**: nenhum arquivo tem `deselect`, então a troca é instantânea; o tempo é só o do saque.
- **Todas com mistura de 0 ms** (corte seco). Velocidade acima de 2× volta para 1×.
- **Os tempos de jogo vêm das animações**: tempo de saque = duração do `select`; cadência = duração do
  `fire_0`; recarga = duração do `reload`; os golpes da faca e o som da recarga são marcas dentro da animação.
- **Quase nada de código**: a arma vira junto com a câmera, sem atraso, sem afundar ao aterrissar e sem
  balanço da cabeça. Só um balanço mínimo em "oito" ao andar (1,4 ciclo/s correndo, 0,6 andando ou
  agachado; amplitude de centésimos de unidade, isto é, quase parado). Recuo, recarga e sprint são animação.
- **FOV do viewmodel**: 50° vertical (o mundo usa 75° vertical).
- **Sem animação de inspecionar** e sem animação de mirar: a luneta só esconde a arma.

Durações (ms; S = Steam, B = cliente BR):

| Arma | sacar | parado | tiro | pós-tiro | recarga | entra no sprint | laço do sprint | sai do sprint |
|---|---|---|---|---|---|---|---|---|
| pistola M92FS | 650 (S) | 1566 | 180 | 150 | 1500 (S) | 250 | 1500 | 400 |
| fuzil M4A1 | 1200 (B) | 1833 | 105 | 0 | 2300 (B) | 233 | 620 | 367 |
| faca M9 | 450 (B) | 2000 | corte 650, golpes em 155 e 402; estocada 1100, golpe em 367 | — | — | 167 | 666 | 167 |
| granada M67 | 400 (B) | 933 | puxar pino 700, segurar 67 (laço), arremesso 366 (solta em 133) | — | 400 | 167 | 666 | 267 |

Sensação a copiar: a arma **reage pela animação, não pelo código**, e tudo é rápido e seco. Hoje o Trok faz
o contrário (sprint e balanço procedurais). Mudar isso é decisão do dono: **sugerir, não fazer**.

**Exceção do CA (regra 2):** as animações de arma dos `.rez` **abertos** (146 arquivos `ANI_` legíveis)
poderiam servir de provisório pessoal em `custom/ca_local/`, convertidas como diz `design/ca-ferramentas.md`.
As de personagem estão em pacote criptografado: **não abrir**.

---

## 3. Zumbis

### 3.1 No Trok (o atalho)
O zumbi do Surto é **jogador** (ou bot jogador) com `weapon_trok_garras`, que traduz tudo para `_MELEE`.
Então um **modelo de zumbi próprio** basta, com estas atividades apontando para animações de zumbi:

| Atividade | Animação de zumbi |
|---|---|
| `ACT_HL2MP_IDLE_MELEE` | parado curvado, balançando |
| `ACT_HL2MP_RUN_MELEE` | corrida de infectado (8 direções em `move_yaw`) |
| `ACT_HL2MP_IDLE_CROUCH_MELEE`, `ACT_HL2MP_WALK_CROUCH_MELEE` | agachado (pode repetir o parado e a corrida, mais baixos) |
| `ACT_HL2MP_JUMP_MELEE` | salto de zumbi (braços à frente) |
| `ACT_HL2MP_GESTURE_RANGE_ATTACK_MELEE` | garrada (gesto só do tronco, para bater correndo); duas ou três variantes com a mesma atividade, e o motor sorteia |

Sem código novo. Com o animstate novo (1.3 B), entram a reação a empurrão (gesto de flinch quando a bala
empurra, `design/modo-surto.md`), a aterrissagem e a transformação ("virar zumbi no lugar", uma
sequência tocada uma vez).

### 3.2 TF2 (código no SDK de 2025)
`sdk/src/game/server/tf/halloween/zombie/` (os esqueletos do Halloween, NextBot). Código da Valve, licença do SDK:
- `zombie_body.cpp`: escolhe a sequência pela atividade, ajusta `move_x`/`move_y` com o vetor de
  movimento do NextBot e a velocidade da animação = velocidade ÷ velocidade da sequência;
- `zombie_attack.cpp`: corre com `ACT_MP_RUN_MELEE`, e a garrada é **gesto** (`ACT_MP_ATTACK_STAND_MELEE`)
  por cima da corrida quando o alvo está a menos de 150 unidades; o dano sai a cada 0,8 a 1,2 s (números do
  TF2, não nossos);
- `zombie.cpp`: ao levar dano, gesto `ACT_MP_GESTURE_FLINCH_CHEST`; ao nascer, `ACT_TRANSITION` (sai do chão).
- Serve de **modelo de como fazer** (bater andando, reação por gesto). Como o nosso zumbi é jogador, o código
  vai para o animstate, não para um NPC.

### 3.3 HL2 (código no SDK)
`npc_BaseZombie.cpp`, `npc_fastzombie.cpp`, `npc_zombine.cpp`: atividades de zumbi da Valve
(`ACT_ZOM_SWATLEFTMID`, `ACT_FASTZOMBIE_LEAP_STRIKE`, `ACT_FASTZOMBIE_FRENZY`, `ACT_FASTZOMBIE_LAND_*`).
Servem de **ideia** para os golpes (bater em objeto, salto com golpe, frenesi). Os modelos são arte do HL2:
só montados, provisório.

### 3.4 Zombie Master: Reborn (`refs/zmr-game`, licença Source 1 SDK, pode copiar com crédito)
- Zumbis são NPCs nos modelos do HL2. Têm um animstate próprio (`zmr_zombieanimstate.*`) e eventos de
  animação pela rede (`zmr_zombieanimevent.*`).
- Criaram atividades próprias (`ACT_ZM_*`) acrescentando ao `ai_activity.h` + `REGISTER_SHARED_ACTIVITY`.
  Para nós seria arquivo da Valve com `#ifdef TROK`; atividade privada evita isso.
- O animstate de jogador e os braços separados (seção 1.3 e 5.4) são o que mais serve.

### 3.5 Left 4 Dead (só o que é público)
- Apresentação "The AI Systems of Left 4 Dead" (Michael Booth, Valve, 2009): a escalada é **algorítmica**,
  com técnica parecida com a de desviar de obstáculos, dentro do "Reactive Path Following" usado por todos
  os atores. É ideia para o parkour dos zumbis bots, não código.
- Como as animações de escalar e correr dos infectados se misturam: **não achei fonte pública** (a conferir
  na apresentação inteira).

### 3.6 Mods de zumbi GPL ou sem licença
Zombie:Reloaded, Zombie Escape AMXX, ZP Special e ZM Riot: **só ideia** (ver `design/zumbi-mods-analise.md`).
Os modelos e sons desses pacotes nunca entram.

---

## 4. Fontes de animação (complemento do catálogo)

Regra do repositório público: arquivo cru que a licença proíbe redistribuir (FBX do Mixamo, lojas) fica
**fora do git**. Só o `.mdl` compilado vai no pacote. Guardar o cru numa pasta local no `.gitignore`.

| Fonte | Licença | No git público? | Formato | Qualidade | FPS/armas | Zumbi |
|---|---|---|---|---|---|---|
| **Mixamo** | uso livre em jogos (comercial também); proibido redistribuir o arquivo cru | **não** (só o compilado) | FBX | mocap boa; deslizes de pé | 3ª pessoa com fuzil e pistola, sem viewmodel | sim (vários) |
| **Quaternius Universal Animation Library 1 e 2** | CC0 (a parte grátis; a versão Pro/Source é paga e também CC0) | sim | FBX/GLB | estilizada, boa para arcade | a 2 tem "weapon handling"; **a conferir** se tem fuzil | a 2 tem locomoção "zombie-like" |
| **Kenney Animated Characters 1, 2, 3** | CC0 | sim | FBX/GLB | simples (parado, pulo, corrida) | não | o 3 tem pele de zumbi |
| **100STYLE** (Ian Mason) | CC BY 4.0 (dar crédito "The 100STYLE Dataset - Ian Mason") | sim, com crédito | BVH, 60 fps (Xsens) | mocap boa | não | estilos de andar e correr; **a conferir** se há estilo de zumbi |
| **CMU Motion Capture** (versão BVH do cgspeed) | CMU e o cgspeed dizem que não põem restrição; pedem citar a fonte | provavelmente sim; **a conferir** o texto da CMU | BVH | crua, dá trabalho limpar | não | não |
| **Rokoko** (biblioteca grátis: 150 e depois 263 animações) | uso comercial permitido segundo a Rokoko; termos antigos tinham prazo | **não** (sem licença de redistribuição clara) | FBX/BVH | mocap boa | poucos | **a conferir** |
| **ActorCore** (Reallusion), animações grátis | jogo exige pedir a "Mass Distribution License" (grátis); em 2025 a Reallusion disse que só vale dentro do AccuRig | **não** | FBX | AAA | algumas | **a conferir**; evitar até ficar claro |
| **MoCap Online** (demo grátis; zumbi custa US$ 4,99) | royalty-free para quem compra; redistribuição do cru não está clara | **não** | FBX, BIP, Blender | mocap profissional | sim (pacotes pagos) | pago |
| **Fab** (antigo Unreal Marketplace) | Standard License: qualquer motor, **exceto** o que for "UE-Only" e o conteúdo da própria Epic (Paragon, Mannequin, Lyra: a conferir); há limite para projeto totalmente open source | **não** | FBX/uasset | varia | sim | sim |
| **Unity Asset Store** | a Unity diz que pode usar em outros motores; exceções: "Unity Companion License" e o que só pode ser baixado pelo editor da Unity | **não** | FBX/unitypackage | varia | sim | sim |
| **OpenGameArt** | cada item tem a sua (CC0, CC-BY, GPL) | só CC0/CC-BY | vários | varia | braços crus (ver catálogo) | poucos |
| **Truebones** | royalty-free, pede crédito, proíbe redistribuir | **não** | BVH/FBX | boa | — | — |
| **Bandai Namco Motion Dataset** | CC BY-NC-ND | **não usar** | BVH | — | — | — |
| **Ubisoft LaFAN1** | **a conferir** (lembrança de licença não comercial e sem derivados) | evitar | BVH | ótima | — | — |

Mocap por vídeo (gravar o dono ou amigos fazendo zumbi, recarga, morte):

| Ferramenta | Grátis serve para o jogo? | Nota |
|---|---|---|
| **Rokoko Video** | **a conferir** (o catálogo fala em cerca de 30 s por mês no plano grátis) | celular, exporta FBX/BVH |
| **DeepMotion** (Animate 3D) | **não**: o grátis é só pessoal e não comercial; o pago proíbe revender a animação crua | 60 s por mês grátis |
| **Move One / Move AI** | **não confirmado**: o plano Personal é não comercial; o comercial é pago | iPhone, exporta FBX/BVH/USD |
| **FreeMoCap** | **sim**: o software é AGPL, mas a animação gravada é nossa (a licença do programa não passa para o arquivo gerado; **a conferir** no repositório) | código aberto, 1 a várias câmeras baratas; sai cena do Blender e CSV, FBX/BVH **a conferir** |

O mod é grátis, mas licença "não comercial" ainda pode dar problema: o Trok fica público e pode ter
doação um dia. Na dúvida, preferir CC0 e captura própria.

**Primeira pessoa:** nenhuma fonte grátis tem viewmodel bom com licença limpa (o catálogo já diz). O caminho
é animar à mão no Blender (o Blender MCP ajuda nas partes mecânicas), usando as durações do CA da seção 2.3
como ponto de partida.

---

## 5. Pipeline Blender → Source

### 5.1 Ferramentas
- **Blender Source Tools** (importa e exporta SMD e DMX, com animação). A wiki da Valve pede Blender 4.1+.
  Há um fork ativo (Compucolor, para Blender 4.2+, versão 3.4.4 de março de 2026, com conserto de animação
  em DMX e opção "Export All Actions"). **A conferir** qual usar.
- **Crowbar**: decompila `.mdl` para ver como os modelos do HL2MP fazem (`$includemodel`, blends, pose
  parameters). Só para estudo; nada decompilado entra no repositório.
- **Retarget (passar a animação de um esqueleto para outro):**
  - **Rokoko plugin** para Blender: grátis, sem plano pago (pode pedir conta grátis);
  - **Expy Kit** (grátis; há um fork "Retarget" para Blender 5+; licença **a conferir**);
  - **Auto-Rig Pro** (pago; não precisa);
  - ponto de partida para o ValveBiped: os scripts citados no catálogo (`mixamo_converter`, gist de renomear
    ossos) e a lista de ossos do ValveBiped (wiki.facepunch.com/gmod/ValveBiped_Bones).
- **studiomdl** do SDK Base: o MCP `trok` já chama com `modelo_compilar` (`cs-source/tools/mcp/trok_mcp.py`).

### 5.2 Esqueleto
- **Usar o ValveBiped** para os humanos e os zumbis. Assim, no começo, o modelo novo pode **incluir as
  animações do HL2MP montadas** (`$includemodel "player/male_anims.mdl"` e os `humans/male_*.mdl`) e só
  trocar uma atividade por vez. **A conferir:** se esses `.mdl` vêm no SDK Base 2013 MP (o HL2MP roda só com
  ele, então devem vir). Isso não copia nada: só aponta para o que está montado.
- O retarget do Mixamo, do Quaternius ou de BVH vai para o ValveBiped no Blender, e então se exporta SMD.
  Não basta renomear: a orientação dos ossos precisa bater (o catálogo já avisa).
- Escala: jogador de 72 unidades; frente do modelo em +X (ver `tools/mcp/LEIA-ME.md`).

### 5.3 O `.qc` (esqueleto do arquivo; conferir sintaxe na wiki da Valve)
Biblioteca de animações separada (`trok_anims_humano.qc`), incluída por todos os corpos:

```
$modelname "trok/jogador/anims_humano.mdl"
$poseparameter move_yaw -180 180 loop 360     // HL2MP (8 direções)
$poseparameter aim_pitch -90 90
$poseparameter aim_yaw -60 60
// corrida do fuzil: 9 animações misturadas pelo move_yaw (ordem a conferir no modelo do HL2DM com o Crowbar)
$sequence run_fuzil { "run_fuzil_S" "run_fuzil_SE" ... "run_fuzil_S" blendwidth 9 blend move_yaw -180 180
                      ACT_HL2MP_RUN_AR2 1 fps 30 loop }
$sequence idle_fuzil "idle_fuzil" ACT_HL2MP_IDLE_AR2 1 loop
$sequence tiro_fuzil "tiro_fuzil" delta ACT_HL2MP_GESTURE_RANGE_ATTACK_AR2 1   // gesto; peso só no tronco ($weightlist)
$sequence recarga_fuzil "recarga_fuzil" delta ACT_HL2MP_GESTURE_RELOAD_AR2 1 { event AE_CL_PLAYSOUND 12 "Trok.Fuzil.Pente" }
```

Corpo (`trok_mercenario_01.qc`): `$model`, `$includemodel "trok/jogador/anims_humano.mdl"`,
`$collisionjoints` (ragdoll) e `$hbox`.

- **Velocidade da corrida**: a animação precisa andar de verdade (movimento da raiz, `LX LY` no `$sequence`,
  ou a raiz animada). Senão o HL2MP não consegue ajustar a passada e o pé escorrega.
- **9 direções (animstate novo)**: trocar `move_yaw` por `blend move_x -1 1 blend move_y -1 1` com 3×3 animações.
- **Matriz de mira**: 9 poses (cima, meio e baixo × esquerda, centro e direita) como `delta`, misturadas por
  `aim_pitch`/`aim_yaw`. **A conferir** o jeito exato (o HL2 usa `$animation` + `subtract`).

### 5.4 Viewmodel (1ª pessoa)
- **Agora (braços embutidos, como o CS:S):** um `.mdl` por arma com braços + arma. Atividades mínimas:
  - `ACT_VM_DRAW`, `ACT_VM_IDLE`, `ACT_VM_PRIMARYATTACK`, `ACT_VM_RELOAD`;
  - opcionais: `ACT_VM_DRYFIRE`, `ACT_VM_HOLSTER`, `ACT_VM_RELOAD_EMPTY`;
  - escopeta: `ACT_SHOTGUN_RELOAD_START`, `ACT_VM_RELOAD` (um cartucho), `ACT_SHOTGUN_RELOAD_FINISH`;
  - faca e garras: `ACT_VM_HITCENTER`, `ACT_VM_MISSCENTER` (+ `...2` no segundo botão), que é o que a base
    das armas brancas pede;
  - sprint como animação, se o dono quiser trocar o procedural: `ACT_VM_SPRINT_ENTER`, `ACT_VM_SPRINT_IDLE`,
    `ACT_VM_SPRINT_LEAVE` (existem no `ai_activity.h`, mas nenhuma arma usa hoje), ou
    `ACT_VM_IDLE_TO_LOWERED` / `ACT_VM_IDLE_LOWERED` / `ACT_VM_LOWERED_TO_IDLE`, que a base do HL2MP já usa
    para arma abaixada.
- **Depois (braços separados, troca de luva e manga por facção):** um segundo viewmodel só com os braços,
  grudado nos ossos da arma (bonemerge). Código pronto:
  - ZMR (`zmr_player.cpp`, `zmr_viewmodel.*`, `VMINDEX_ARMS`, braços por modelo de jogador);
  - Mapbase (`player.cpp`, `protagonist_system.*`; licença Source 1 SDK).
  - Cada arma passa a ter animação de "braços + arma" num esqueleto comum (o do `c_arms`); os braços
    mudam por facção sem refazer as armas.
- Número de mira e posição continuam em script (`trok_mira_ajuste`, `trok_mira_imprimir`).

### 5.5 Onde entra o MCP `trok`
1. Blender (Blender MCP): retarget, cortar, laço, exportar SMD em lote.
2. `modelo_compilar` com o `.qc` (sai no `mod_trok`).
3. `jogo_abrir` + `partida_teste` + `print_tela` para ver o modelo, e `console`/`erros` para avisos do
   studiomdl e do modelo (atividade que falta, sequência 0).
4. No jogo: `thirdperson`, `ent_text` / `cl_showpos`, e `sv_cheats 1` + `bot_mimic` (**a conferir** no
   HL2MP de 2025) para ver o boneco de fora; `r_drawviewmodel` e `viewmodel_fov` para a 1ª pessoa.
5. HLMV (visualizador de modelos do SDK Base) para conferir sequências, pose parameters e eventos antes de
   abrir o jogo.

---

## 6. Plano recomendado (curto)

**Fase 1: impacto rápido, quase sem código.**
1. Viewmodel do **fuzil** próprio (braços + arma, braços embutidos), com draw, idle, tiro, recarga.
   Usar as durações do CA como ponto de partida (seção 2.3). É o que o jogador vê o tempo todo.
2. **Zumbi**: modelo próprio no ValveBiped com as 7 atividades `_MELEE` de zumbi (seção 3.1). Fontes:
   Mixamo (fora do git) ou Quaternius UAL2, ou captura própria.
3. **Humano**: corpo próprio no ValveBiped com `$includemodel` das animações do HL2MP montadas. Trocar
   primeiro o fuzil e a pistola (`_AR2`, `_PISTOL`); a submetralhadora e a escopeta podem apontar para as do
   fuzil.

**Fase 2: animstate novo (código, `#ifdef TROK`).** Portar o do ZMR (1.3 B). Ganha 9 direções, andar,
sprint, pulo em fases, reação a dano e eventos de rede. Depois acrescentar escalar e virada no ar.

**Fase 3: acabamento.** Resto das armas, braços separados por facção, faca e granada (se o dono
pedir), mortes (ragdoll com força direcional, ou animação curta antes do ragdoll: **o dono decide**).

### Animações mínimas

**Humano, 3ª pessoa (por tipo de arma; começar por fuzil, pistola e faca):**
- parado;
- correr em 8 direções (9 no animstate novo) e andar (C) em 8 direções (Fase 2);
- sprint para a frente (Fase 2);
- agachado parado e agachado andando em 8 direções;
- pulo: início, no ar e aterrissagem (Fase 2; na Fase 1, uma só);
- matriz de mira (9 poses);
- gestos: tiro, recarga, sacar;
- reação a dano: cabeça, peito, braço e perna (Fase 2);
- morte: ragdoll (já existe; só precisa do `.phy`).

**Zumbi:**
- parado;
- correr em 8 direções;
- agachado (pode reaproveitar as de cima);
- pulo (início, no ar, aterrissagem);
- garrada esquerda, garrada direita e golpe com as duas mãos (gestos);
- reação a empurrão (gesto);
- virar zumbi (uma vez);
- morte: ragdoll.

**Viewmodel (por arma):**
- sacar, parado, tiro (1 a 3 variantes), recarga (e recarga vazia, opcional);
- entrar, laço e sair do sprint (só se o dono trocar o procedural);
- faca: sacar, parado, corte, estocada;
- garras: sacar, parado, ataque esquerdo, ataque direito;
- granada (se pedida): puxar o pino, segurar, arremessar.

---

## Fontes

No repositório:
- `cs-source/sdk/src/game/server/hl2mp/hl2mp_player.cpp` (`SetAnimation`, `CreateRagdollEntity`);
  `sdk/src/game/shared/hl2mp/hl2mp_player_shared.cpp` (`CPlayerAnimState`);
  `sdk/src/game/shared/Multiplayer/multiplayer_animstate.*`; `sdk/src/game/shared/ai_activity.h`;
  `sdk/src/game/shared/activitylist.*`; `sdk/src/game/server/tf/halloween/zombie/`.
- `cs-source/refs/zmr-game/src/game/shared/zmr/` (animstate, braços); `cs-source/refs/neotokyo-rebuild/src/game/shared/hl2mp/hl2mp_playeranimstate.*`.
- `cs-source/design/chimera/specs/combat_arms/animation.md`, `characters.md`, `view_models.md`.

Na web (consultados em 10/11/2026; vários só por resumo de busca, porque a rede da nuvem bloqueia os sites):
- Mixamo FAQ: https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html e fórum da Adobe:
  https://community.adobe.com/t5/mixamo-discussions/mixamo-faq-licensing-royalties-ownership-eula-and-tos/td-p/13234775
- Quaternius UAL: https://quaternius.com/packs/universalanimationlibrary.html ,
  https://quaternius.com/packs/universalanimationlibrary2.html , https://opengameart.org/content/universal-animation-library-2
- Kenney: https://kenney.nl/assets/animated-characters-2 , https://kenney-assets.itch.io/animated-characters-3
- 100STYLE: https://ianxmason.github.io/100style , artigo https://arxiv.org/abs/2201.04439
- CMU / cgspeed: http://mocap.cs.cmu.edu , https://groups.google.com/g/bvhacker/c/r906SGjjcTA
- Rokoko: https://www.rokoko.com/resources/download-263-rokoko-motion-capture-assets ,
  https://www.cgchannel.com/2020/03/get-150-free-mocap-moves-from-rokokos-motion-library ,
  https://support.rokoko.com/hc/en-us/articles/4410463481489-Retarget-an-animation-in-Blender
- ActorCore: https://www.cgchannel.com/2025/07/rig-and-animate-3d-characters-for-free-with-accurig-2-0/ ,
  https://forum.reallusion.com/Topic542027.aspx
- MoCap Online: https://mocaponline.itch.io/zombie-starter-animations , https://mocaponline.itch.io/mocap-online-demo
- Fab: https://www.fab.com/eula , https://www.cgchannel.com/2024/11/epic-games-to-share-new-free-assets-on-fab-every-two-weeks ,
  Epic Content License: https://www.unrealengine.com/eula/content
- Unity Asset Store em outros motores: https://support.unity.com/hc/en-us/articles/34387186019988-Can-I-use-assets-from-the-Asset-Store-with-other-engines
- DeepMotion: https://www.deepmotion.com/faq ; Move AI: https://docs.move.ai/knowledge/move-ai-pricing-plans-credits ;
  FreeMoCap: https://pypi.org/project/freemocap/ , https://freemocap.github.io/documentation/about_us
- Blender Source Tools: https://developer.valvesoftware.com/wiki/Blender_Source_Tools ; fork:
  https://github.com/compucolor/BlenderSourceTools (endereço **a conferir**)
- Expy Kit: https://github.com/pKrime/Expy-Kit , https://extensions.blender.org/add-ons/retarget/
- Modelos de jogador do HL2DM: https://developer.valvesoftware.com/wiki/Replacing_HL2DM_player_models_with_XSI_Mod_Tool ,
  https://developer.valvesoftware.com/wiki/$includemodel , https://wiki.facepunch.com/gmod/Player_Animations ,
  https://developer.valvesoftware.com/wiki/$bonemerge
- L4D: https://cdn.fastly.steamstatic.com/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf

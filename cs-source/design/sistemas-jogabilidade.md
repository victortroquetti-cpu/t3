# Sistemas de jogabilidade: onde está cada número

Feito na nuvem (09/11/2026) para o Claude Code local, que tem o Combat Arms instalado e as
skills de referência, preencher os valores. **Tudo é número em cfg ou script:** nenhum ajuste
de jogabilidade precisa recompilar.

**Regra:** do Combat Arms (e do CoD) entram **números medidos e sensações descritas**:
velocidade, altura de pulo, dano, cadência, tempo de mira, recuo. **Não entram arquivos**:
nada de modelos, sons, texturas, mapas ou dados extraídos do jogo.

## Mapa dos números

| Sistema | Arquivo | O que tem |
|---|---|---|
| Vida, colete, dano por parte do corpo, armas iniciais | `game/mod_trok/cfg/balanceamento.cfg` | `trok_vida`, `trok_colete`, `sk_player_*`, `trok_armas_iniciais`, `trok_municao_inicial` |
| Movimento | `game/mod_trok/cfg/movimento.cfg` | velocidades (andar/correr/sprint), pulo, deslizar, escalar, gravidade, aceleração, controle no ar (`trok_ar_controle`), velocidade com faca (`trok_faca_velocidade`) |
| Cada arma | `game/mod_trok/scripts/weapon_trok_*.txt`, bloco `"Trok"` e sub-bloco `"CA"` | cadência, auto/semi, balas por tiro, mira pelo cano, peso; no `"CA"`: dano, faixas, cone, recuo, tempos (ver abaixo) |
| Lei de tiro do CA e inventário | `game/mod_trok/cfg/balanceamento.cfg` | `trok_arma_ca`, `trok_ca_dano_costas`, `trok_pegar_*`, `trok_morte_largar`, `trok_arma_chao_tempo`, `trok_armas_max` |
| Teclas padrão | `game/mod_trok/cfg/config_default.cfg` | Shift corre, C caminha quieto, botão 2 mira, 1-6 classes, Q anterior, E usa/pega |
| Bots | `game/mod_trok/scripts/trok_bot_perfis.txt` | reação, mira, percepção, estilo (ver `design/bots.md`) |
| Marcador de acerto | ConVars `trok_hitmarker_*` (cliente) | duração, tamanho, sons |
| Modos equipe e solo | `game/mod_trok/cfg/equipe.cfg` e `solo.cfg` | meta de abates, tempo, renascer, proteção ao nascer (ver `design/modos.md`) |
| Modo Surto (infecção) | `game/mod_trok/cfg/surto.cfg` + `empurrao`/`segurar` nas armas | tempos da rodada, hospedeiros, vida e regeneração do zumbi, garras, empurrão (ver `design/modo-surto.md`) |
| Série de abates (killstreak do CA) | `game/mod_trok/cfg/killstreak.cfg` + `scripts/trok_anunciador.txt` | janela do multi-abate, vingança, primeiro abate, tiro de sorte, voz; no script: textos, vozes, níveis, cores e tempos da faixa e da lista (ver abaixo) |
| Economia estilo CS (desligada) | `game/mod_trok/cfg/economia.cfg` + `scripts/trok_loja.txt` | dinheiro inicial, teto, ganhos, tempo e zona de compra, kit inicial; na loja: itens, preços, coletes, capacetes, mutagênicos, ganho por classe (ver `design/economia.md`) |

## Campos do bloco "Trok" das armas

| Campo | Unidade | O que faz | Como medir no jogo de referência |
|---|---|---|---|
| `tiros_por_minuto` | RPM | cadência | contar tiros de um pente inteiro / tempo |
| `automatica` | 0/1 | segura o gatilho ou um tiro por clique | — |
| `balas_por_tiro` | n | escopeta > 1 | — |
| `dano` | pontos | dano por bala até `queda_inicio` | tiros para matar a curta distância (corpo) × vida |
| `queda_inicio` / `queda_fim` / `queda_minimo` | u, u, fração | dano cai linearmente até `queda_minimo × dano` | tiros para matar de longe |
| `dispersao_parado` / `_andando` / `_ar` | graus (cone total) | precisão por estado | abertura da mira / espalhamento na parede |
| `dispersao_agachado` / `_mira` | multiplicador | agachado e mirando pelo cano | — |
| `dispersao_por_tiro` / `dispersao_tiros_max` / `recuperacao` | graus, graus, graus/s | mira abre a cada tiro seguido e volta parada | spray numa parede |
| `recuo_vertical` / `recuo_lateral` | graus por tiro | chute da câmera (padrão lateral em seno) | quanto a mira sobe por tiro |
| `mira_fov` | graus | zoom ao mirar (FOV normal 90) | — |
| `mira_tempo` | s | tempo para entrar na mira | — |
| `mira_posicao` / `mira_angulos` | u / graus | onde a arma fica mirando (alça no centro) | ajustar ao vivo: `trok_mira_ajuste` + `trok_mira_imprimir` |
| `mira_velocidade` | multiplicador | lentidão andando mirando | — |
| `velocidade` | multiplicador | peso da arma na velocidade | correr com cada arma |
| `animacao` | ar2 / smg1 / pistol / shotgun | animação do jogador em 3ª pessoa | — |

Arma nova: copiar um `weapon_trok_*.txt` e acrescentar uma linha `TROK_GUN_CLASS(...)` em
`src/game/shared/trok/weapon_trok_gun.cpp` (o cliente precisa de uma classe por nome de
entidade).

Com `"modelo_ca" "1"` (todas as armas de fogo hoje) e `trok_arma_ca 1`, os campos `dispersao_*`,
`recuo_*`, `queda_*` e `dano` acima **não valem**: vale o sub-bloco `"CA"` abaixo. `tiros_por_minuto`,
`automatica`, `balas_por_tiro`, `velocidade` e os `mira_*` valem nos dois modelos.

### Sub-bloco "CA" (lei de tiro do Combat Arms)

Fonte: `design/chimera/specs/combat_arms/weapon_firing.md`, `weapon_classes.md`, `inventory.md` e
`design/chimera/games/combat_arms/data/weapons/*.toml`. Distâncias em unidades do CA 1:1; tempos em s
(o CA guarda em ms); recuo em graus como nos dados do CA; os dois limiares "lado" em radianos.
Campo que falta usa o padrão do CA (`trok_arma_ca.cpp`).

| Campo | Campo do CA | O que faz |
|---|---|---|
| `dano`, `dano_variacao` | InstDamage, DamageRFacter | dano por raio × sorteio uniforme ±k |
| `alcance` | Range | o raio acaba aqui (escopeta: 1000 u) |
| `faixas`, `faixas_dano` | Effectrange0..2, Rangedamage0..2 | **degraus** de dano pela distância (não curva) |
| `cone`, `cone_mira` | Min/MaxPerturb, ZoomedMin/Max | cone em milésimos; mirando pelo cano usa o `cone_mira` |
| `cone_velocidade`, `cone_velocidade_mira`, `cone_fator` | PerturbInc/DecSpeed, DynamicPerturbFactor | quanto os medidores sobem/descem; fator do cone |
| `teto_tiro` | Fire/FireMove/FireDuck/FireMoveDuckPerturb | teto do medidor de tiro (parado, andando, agachado, agachado andando) |
| `alvo_movimento` | Walk/Run/JumpPerturb | alvo do medidor de movimento |
| `espera_recuperar` | DecDelay | tempo depois do tiro até o cone começar a fechar |
| `recuo`, `recuo_acel`, `recuo_max` | BaseCamRecoilPitch, AccelPitch, MaxCamRecoilPitch | chute vertical por tiro; a vista sobe até 2 × `recuo_max` |
| `recuo_lado_inicio`, `recuo_lado`, `recuo_lado_max`, `recuo_lado_troca` | MaxRandomPitch, WagR/L, MaxCamRecoilWag, MaxRandomwag | chute lateral (começa depois do limiar, troca de lado 50%) |
| `recuo_volta_espera`, `recuo_volta` | JitterDecDelay, JitterWaitRecover/JitterRecover | volta do recuo (lenta logo depois do tiro, depois rápida, rad/s) |
| `tempo_recarga`, `tempo_sacar` | animações reload / select | o pente enche e a arma atira só quando acabam |
| `recarga_por_bala` | PreReload, reload por bala, PostReload | escopeta: abrir, cada cartucho, fechar; atirar com bala no pente interrompe |
| `saida_corrida` | PostRush | tempo para atirar depois de correr (sem o campo: `trok_arma_correndo_atraso`) |
| `faca_alcance`, `faca_golpe`, `faca_estocada` | M9 | faca: alcance; duração, dano e momentos dos acertos |

Como funciona:
- **Cone quadrado:** cada raio sai em `[-n, n]/1000` à direita e para cima da mira, sorteados à parte
  (escopeta: 8 raios, cada um com o seu sorteio). `n = n'min + floor((n'max − n'min) × p) × fator`,
  com `p` = o maior dos dois medidores **antes** deste tiro.
- **Dois medidores** (0..1), em subpassos fixos de 1/80 s: o de tiro sobe a cada tiro até o `teto_tiro`
  do estado e, depois de `espera_recuperar`, desce; o de movimento persegue `velocidade/limite ×
  alvo` (correr, andar com C/agachado, pulo). No ar depois de pular ele sobe até o alvo do pulo. A mira
  do HUD abre com o cone. Virar o mouse não abre (no CA também não, Q11).
- **Dano:** base × sorteio × degrau da distância × 1,3 vindo de trás (`trok_ca_dano_costas`, setor
  de 150° do CA, torto de propósito) × parte do corpo (`sk_player_*`: cabeça 6, braço e perna 0,9).
- **Recuo de câmera do jogador** (não da arma): mexe a vista de verdade (não volta sozinho como o
  `ViewPunch`); a subida tem teto, depois o lado; a volta puxa a vista para baixo mesmo que o jogador já
  tenha corrigido. Bots: chute pelo `ViewPunch`.
- **Cadência exata:** segurando, o próximo tiro conta de quando este podia sair (750/min de verdade na
  MP5, não arredondado para o tique).

| Arma (script) | CA | Dano / faixas | Cadência | Pente / reserva | Cone (mira) | Recuo | Recarga / sacar | Velocidade |
|---|---|---|---|---|---|---|---|---|
| Fuzil (`weapon_trok_fuzil`) | M4A1 | 35 ±5%; ×1 / 0,7 / 0,35 até 2500 / 3000 / 3500 | 571/min, auto | 30 / 90 | 3–100 (2–30) | 1,3° (teto 5) | 2,3 s / 1,2 s | 228 u/s |
| Submetralhadora | MP5A4 | 29 ±10%; ×1,1 / 0,7 / 0,35 até 500 / 2000 / 2500 | 750/min, auto | 30 / 90 | 8–100 (7–17) | 1,4° (teto 7) | 2,0 s / 1,0 s | 236,55 u/s |
| Escopeta | Remington 870 | 8 × 9 ±10%; até 600 / 800 / 1000 | 100/min, semi | 7 / 28 | 35–100 ×1,2 = 42 fixo | 5° (teto 25) | 0,333 + 0,7/cartucho + 0,9 s / 1,0 s | 233,7 u/s |
| Pistola | Beretta M92FS | 26 ±10%; até 1000 / 2000 / 2500 | 333/min, semi | 15 / 30 | 10–100 ×1,1 (9–18) | 3,5° (teto 14) | 1,5 s / 0,65 s | 247,95 u/s |
| Faca (`weapon_trok_faca`) | M9 | golpe 2 × 45 (0,155 e 0,402 s de 0,65 s); estocada 75 (0,367 s de 1,1 s); ±10%; alcance 105 | — | — | — | — | — / 0,45 s | sem perda |

A velocidade é o campo `velocidade` = (1 − colete − perda da arma) / (1 − colete), porque o movimento
multiplica por (1 − `trok_ca_colete`); se o colete mudar, refazer a conta.

## Inventário como no Combat Arms

Código: `src/game/shared/trok/trok_inventario.cpp` (ganchos de uma linha em `hl2mp_player.cpp` e em
`hud_weaponselection.cpp`), aviso no HUD em `src/game/client/trok/hud_trok_pegar.cpp`.

- **Classes = `bucket` do script + 1:** 1 principal (bucket 0: fuzil, submetralhadora, escopeta),
  2 secundária (bucket 1: pistola), 3 branca (bucket 2: faca, garras), **4 arremesso (bucket 3:
  granadas e minas; reservado)**, 5 especial (bucket 4), 6 objetivo (bucket 5).
- **Teclas 1–6** escolhem a classe; repetir a tecla passa para a próxima arma da classe; a tecla de
  outra classe volta na arma dessa classe usada por último. **Q** volta à anterior. A troca é
  instantânea; a arma nova só atira depois do `tempo_sacar`. **Rodinha:** como no CA, não troca de arma.
- **Pegar:** não pega andando por cima. Olhar para a arma no chão (o raio do olho entra na caixa dela
  a até `trok_pegar_distancia` = 100 u, sem parede antes) mostra **"E — PEGAR <ARMA>"**; o E troca pela
  sua da mesma classe, que cai no lugar com o pente e a reserva; você fica com exatamente o pente e a
  reserva que estavam na arma do chão, e ela é sacada na hora. Vale para **principal e secundária**
  (`trok_pegar_trocar "1 2"`); nas outras classes, a mesma arma soma munição. Sem arma na mira, o E
  segue para o uso normal (grades do Surto). Zumbi não pega. Máximo de 16 armas.
- **Largar:** sem tecla. **Ao morrer caem a principal e a secundária** selecionadas por último
  (`trok_morte_largar "1 2"`, decisão do dono; no CA era só a principal), cada uma com o pente e a
  reserva, no pé, paradas. Armas no chão somem em 20 s (`trok_arma_chao_tempo`).
- **Munição:** no Source a reserva é do jogador por tipo (SMG1, Pistol, Buckshot). Quando uma arma
  cai, a reserva do tipo dela vai junto. Supõe uma arma carregada por tipo de munição (fuzil e
  submetralhadora usam SMG1: com as duas ao mesmo tempo, a reserva cairia toda com a primeira).

## Como os sistemas funcionam (resumo técnico)

- **Corrida (Shift):** é o sprint do HL2 (gasta energia do traje). Correndo acima de
  `trok_arma_correndo_vel`, a arma do Trok abaixa (animação procedural: desce, gira e balança
  no ritmo da passada) e não atira. Ao parar, espera `trok_arma_correndo_atraso`.
- **Fluidez (caixas, troca de lado):** `trok_ar_controle` é quanto o jogador consegue mudar a
  direção no ar (o Source limita a 30 u/s; mais alto = curva no meio do pulo). `sv_accelerate` e
  `sv_friction` deixam o arranque e a troca de lado no chão mais secos. Com arma branca na mão a
  velocidade multiplica por `trok_faca_velocidade` (armas do Trok usam o campo `velocidade`).
  Pular de novo no tique em que encosta no chão não perde velocidade (o atrito só entra depois).
  Extras do pulo (Momentum Mod / Fortress Forever, previstos em rede):
  `trok_pulo_virada` (até `trok_pulo_virada_tempo` depois do pulo, apertar outra direção gira a
  velocidade para ela sem acelerar: o "caixa a caixa"), `trok_pulo_coyote` (pula um instante depois
  de sair da borda), `trok_pulo_antecipado` (apertou um pouco antes de pousar, pula ao pousar),
  `trok_pulo_teto`/`_duro` (corta o excesso de velocidade ao pular: nada de bunny hop infinito).
  Os bots usam o mesmo movimento, então ganham tudo isso também.
  Deslize: desligado (`trok_deslizar 0`), decisão do dono.
- **Caminhar (C):** `+walk` anda devagar (`hl2_walkspeed`) e **não faz barulho de passo**
  (patch em `CBasePlayer::UpdateStepSound`). Os bots também não ouvem (só ouvem acima de ~160 u/s).
- **Mira pelo cano, estilo CoD:**
  - **Botão:** segurar o botão 2 (`trok_mira_segurar 1`, padrão, com opção em CONFIGURAÇÕES); com 0, alterna. Os bots sempre
    alternam. Segurando, correr ou recarregar tira da mira e, ao acabar, volta sozinho.
  - **Zoom:** com `mira_zoom` ele é relativo ao campo de visão do jogador (o mesmo aumento com FOV 75 ou 90); o `mira_fov`
    fixo só vale com `mira_zoom 0`.
  - **Tiro mirando:** não toca a animação de tiro do modelo, que tira a alça do centro (`mira_animacao_tiro 0`). No lugar,
    um coice procedural (`mira_coice`, `mira_coice_angulo`, cerca de 0,1 s) e o clarão do próprio modelo. O recuo mirando
    é `mira_recuo` (×).
  - **Calibração:** `trok_mira_calibrar 1` prende a arma na posição de mira e desenha linhas no centro.
    `trok_mira_mexer <direita|cima|frente|pitch|yaw|roll> <valor>` ajusta, e `trok_mira_imprimir` dá as linhas para o script.
- **Mira pelo cano (resto):** FOV vai para o zoom em `mira_tempo`, a arma vai
  para `mira_posicao`/`mira_angulos`, a dispersão multiplica por `dispersao_mira`, o recuo cai
  30%, o jogador anda a `mira_velocidade`. Sensibilidade do mouse escala com o zoom
  (`zoom_sensitivity_ratio` no `config_default.cfg`). Correr ou recarregar sai da mira.
  `mira_fov_arma` (FOV da arma na tela mirando) é aplicado pelo `ClientModeShared::GetViewModelFOV`
  (patch). Mirando, o atraso da arma ao virar e o deslocamento pelo pitch somem (patch em
  `CBaseViewModel::CalcViewModelLag`): a alça fica no centro olhando para cima ou para baixo.
- **Recuo (modelo antigo, `trok_arma_ca 0`):** `trok_recuo_fixo` (padrão 0,5) é a parte que **fica** na mira (sobe e não volta,
  como no CoD: o jogador puxa para baixo); o resto é o chute (`ViewPunch`) que volta sozinho. A parte
  fixa mexe nos ângulos de visão do jogador local uma vez por tiro previsto; bots levam o chute inteiro.
  Com o modelo do CA, o recuo é o descrito em "Sub-bloco CA".
- **Arma na tela andando:** balanço leve em "oito" pela distância percorrida (parado contra a parede
  não balança), quase nada mirando (`trok_arma_balanco`, cliente).
- **Dano (modelo antigo):** calculado pela distância até onde a mira aponta no momento do tiro; partes do
  corpo multiplicam pelos `sk_player_*`. (Antes o `FireBullets` do HL2MP trocava esse dano pelo
  `damage` do script; o patch agora deixa a arma do Trok mandar o dano.)

## Série de abates (killstreak) como no Combat Arms

Fonte: `design/chimera/specs/combat_arms/kill_streaks.md` (regras e números do CA, com os casos de teste).
Código: servidor em `src/game/shared/trok/trok_killstreak.cpp` (ganchos de uma linha em `trok_gamerules.cpp`),
cliente em `src/game/client/trok/hud_trok_killstreak.cpp`, evento `trok_serie` em `resource/modevents.res`.

- **Multi-abate:** cada abate de inimigo a **menos** de `trok_serie_janela` (5 s) do anterior sobe 1; senão volta
  a 1. Abate de aliado e suicídio não sobem nem zeram. Morrer não zera (é por tempo).
- **Um evento por abate**, nesta ordem: primeiro abate (`trok_serie_primeiro`: do mapa ou da rodada) > vingança
  (a vítima te abateu `trok_serie_vinganca` vezes sem resposta) > passo da série (2 ABATE DUPLO, 3 ABATE MÚLTIPLO,
  4 ULTRA ABATE, 5 FANTÁSTICO, 6 INACREDITÁVEL, 7+ INACREDITÁVEL+) > na cabeça (só abate sem série) > abate simples
  ("1 ABATE", "N ABATES" com o total do mapa). Cabeça numa série vira a marca "CABEÇA" sobre a faixa.
- **Faixa** (só quem matou vê): entra 5× maior e transparente em 0,1 s, solta um eco que cresce a 3× tremendo
  ±5 px por 0,15 s, segura, sai crescendo a 5× sumindo de 1,2 a 1,4 s. Uma por vez: nível menor que o da tela é
  descartado, igual ou maior troca na hora (abate 1, série e cabeça 5, primeiro e vingança 6, sorte 11).
- **Voz:** no máximo uma por abate (a do evento; senão a da cabeça), som de interface, abates seguidos se
  sobrepõem. Provisórias do HL2/CS:S (`trok_serie_voz 0` desliga). Vingança e abate simples sem voz (como no CA).
- **Lista à esquerda** (todos veem): "NOME  ABATE DUPLO" no primeiro abate e nos passos; até 5 linhas, 3,75 s.
- **Série desta vida** (sem limite de tempo) e a **maior do mapa** vão em rede (`trok_placar_extra`) para o placar
  e a tela de morte (o CA mostra a maior série na tela de morte e no resultado). `trok_serie_info` lista tudo.
- **Sem recompensa:** o CA não dá prêmio por série (só o perk Victory Lap, fora daqui). Sem chamada de faca ou
  granada (o CA não tem). Sem "nutshot" (o HL2 não tem caixa de acerto da virilha).
- **Testar sem matar:** `trok_serie_teste 2 cabeca`, `trok_serie_teste 1 primeiro`, `trok_serie_teste 1 abates 12`.

## Economia estilo CS (base)

Ver `design/economia.md` (regras, números do CS:S, loja, menu da tecla B, o que o dono decide e o que testar).
Código: `src/game/server/trok/trok_economia.cpp`, `src/game/shared/trok/trok_loja.cpp`,
`src/game/client/trok/ui/trok_loja.cpp`, `src/game/client/trok/hud_trok_economia.cpp`. Desligada por padrão.

## Modelos e sons de protótipo

Plano para trocar tudo por conteúdo próprio: `design/conteudo-proprio.md`. Os sons das armas já
passam por nomes do Trok (`scripts/game_sounds_trok.txt`).


As armas usam modelos e sons do **CS:S montado** (do jogo instalado do jogador; não vão no
pacote). Pendências que só se resolvem olhando o jogo:
- sons de recarga e de manuseio vêm de eventos de animação dos modelos do CS:S, com nomes
  de soundscript do CS:S que o mod não carrega: podem ficar mudos (ver avisos no console);
- `mira_posicao` padrão é chute: alinhar cada arma com `trok_mira_ajuste`;
- trocar pelos modelos e sons próprios do Trok quando existirem.

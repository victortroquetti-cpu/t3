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
| Cada arma | `game/mod_trok/scripts/weapon_trok_*.txt`, bloco `"Trok"` | cadência, auto/semi, dano e queda, dispersão por estado, recuo, mira pelo cano, peso |
| Teclas padrão | `game/mod_trok/cfg/config_default.cfg` | Shift corre, C caminha quieto, botão 2 mira |
| Bots | `game/mod_trok/scripts/trok_bot_perfis.txt` | reação, mira, percepção, estilo (ver `design/bots.md`) |
| Marcador de acerto | ConVars `trok_hitmarker_*` (cliente) | duração, tamanho, sons |

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
- **Mira pelo cano:** botão 2 alterna. FOV vai para `mira_fov` em `mira_tempo`, a arma vai
  para `mira_posicao`/`mira_angulos`, a dispersão multiplica por `dispersao_mira`, o recuo cai
  30%, o jogador anda a `mira_velocidade`. Sensibilidade do mouse escala com o zoom
  (`zoom_sensitivity_ratio` no `config_default.cfg`). Correr ou recarregar sai da mira.
  `mira_fov_arma` (FOV da arma na tela mirando) é aplicado pelo `ClientModeShared::GetViewModelFOV`
  (patch). Mirando, o atraso da arma ao virar e o deslocamento pelo pitch somem (patch em
  `CBaseViewModel::CalcViewModelLag`): a alça fica no centro olhando para cima ou para baixo.
- **Recuo:** `trok_recuo_fixo` (padrão 0,5) é a parte que **fica** na mira (sobe e não volta,
  como no CoD: o jogador puxa para baixo); o resto é o chute (`ViewPunch`) que volta sozinho. A parte
  fixa mexe nos ângulos de visão do jogador local uma vez por tiro previsto; bots levam o chute inteiro.
- **Arma na tela andando:** balanço leve em "oito" pela distância percorrida (parado contra a parede
  não balança), quase nada mirando (`trok_arma_balanco`, cliente).
- **Dano:** calculado pela distância até onde a mira aponta no momento do tiro; partes do
  corpo multiplicam pelos `sk_player_*`.

## Modelos e sons de protótipo

Plano para trocar tudo por conteúdo próprio: `design/conteudo-proprio.md`. Os sons das armas já
passam por nomes do Trok (`scripts/game_sounds_trok.txt`).


As armas usam modelos e sons do **CS:S montado** (do jogo instalado do jogador; não vão no
pacote). Pendências que só se resolvem olhando o jogo:
- sons de recarga e de manuseio vêm de eventos de animação dos modelos do CS:S, com nomes
  de soundscript do CS:S que o mod não carrega: podem ficar mudos (ver avisos no console);
- `mira_posicao` padrão é chute: alinhar cada arma com `trok_mira_ajuste`;
- trocar pelos modelos e sons próprios do Trok quando existirem.

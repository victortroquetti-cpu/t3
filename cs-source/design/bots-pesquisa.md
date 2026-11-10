# Bots: material de pesquisa (parkour, mira, tática)

Material de consulta para melhorar os bots. **Não é um plano**: são técnicas, números, receitas
e onde está o código de referência. A versão local dos bots do dono pode já ter resolvido parte
disto; use o que servir.

- Código do NextBot citado: `cs-source/sdk/src/game/server/` (SDK 2013 MP de 2025, já com o patch do Trok).
- Bots do repo: `cs-source/src/game/server/trok/bot/`; design em `design/bots.md`.
- "A conferir" = não confirmei na fonte ou depende de testar no jogo.

## 0. O mais importante em 10 linhas

1. O NextBot já sabe 4 tipos de trecho: andar, **cair de borda** (`DROP_DOWN`), **pular vão** (`JUMP_OVER_GAP`), **subir borda** (`CLIMB_UP`) e escada. Tudo sai de `Path::ComputePathDetails` e é executado pelo `PathFollower`.
2. Os limites do bot jogador são **fixos no código** (`PlayerLocomotion`): degrau 18, pulo 57, queda 200. O Rearmed usa degrau 30 (`sv_stepsize`), pulo ~50 e pulo agachado ~67 (`movimento.cfg`). Os bots planejam com números errados.
3. O pulo de vão do NextBot **trapaceia**: no instante do pulo ele põe a velocidade horizontal direto na corrida (`PlayerLocomotion::Update`). Não simula se o pulo alcança.
4. O custo de caminho **não sabe o tamanho do vão**: um `nav_connect` feito à mão sobre um vão de 300u entra no caminho como se fosse andável.
5. Parkour de verdade pede **links de pulo com dados** (decolagem, pouso, velocidade, se precisa agachar ou sprint) e um **simulador de pulo** com a física do Rearmed antes de pular.
6. O jogo tem uma regra que afeta pulos em sequência: depois de cada pulo o próximo fica a 30% e recupera 1,2/s (`trok_ca_pulo_min`, `trok_ca_pulo_recupera`). Pular de novo logo depois de subir numa caixa sai fraco.
7. Errar de forma humana = sortear erro no **tempo** do pulo e na **velocidade**, e depois **lembrar** onde falhou (custo extra no link), tentar de novo umas vezes e então pegar outro caminho.
8. Mira humana: reação ex-gaussiana (mínimo ~150 ms), primeiro movimento rápido que **fica curto** do alvo, depois 1–2 correções pequenas; erro maior com alvo rápido. Os números do TF2/HL2MP (1,0/0,5/0,3/0,2 s) são um piso razoável.
9. Tática: utility AI (pontuar opções) combina com o que já existe; behavior tree e HTN servem para sequências longas; para o Surto, um "diretor" no estilo L4D e cerco por entradas diferentes.
10. A melhor referência de código pronta é o **NEOTOKYO Rebuild** (`refs/neotokyo-rebuild/src/game/server/neo/bot/`): TF2 bot portado para outro mod, com custo de caminho, memória de travamento, fogo de supressão e correções do `PathFollower` (parte já veio para o Trok).

---

## 1. NextBot no SDK de 2025

### 1.1 Peças e arquivos

| Peça | Arquivo | Para quê |
|---|---|---|
| `ILocomotion` | `NextBot/NextBotLocomotionInterface.h/.cpp` | interface de movimento: `Approach`, `Jump`, `ClimbUpToLedge`, `JumpAcrossGap`, `IsGap`, `HasPotentialGap`, `IsPotentiallyTraversable`, `GetStepHeight`, `GetMaxJumpHeight`, `GetDeathDropHeight`, monitor de travamento (`StuckMonitor`) |
| `PlayerLocomotion` | `NextBot/Player/NextBotPlayerLocomotion.h/.cpp` | versão para bot jogador: aperta botões (`PressJumpButton`) em vez de mover a entidade; escada por estados |
| `NextBotGroundLocomotion` | `NextBot/NextBotGroundLocomotion.cpp` | versão para NPC (move a entidade com física própria; tem `GetGravity`). **Não** é a dos bots jogadores; serve de exemplo de pulo calculado (`m_velocity.z = sqrt(2·g·altura)`) |
| `Path` | `NextBot/Path/NextBotPath.cpp` | A* no nav (`Compute`) e transformação em segmentos (`ComputePathDetails`) |
| `PathFollower` | `NextBot/Path/NextBotPathFollow.cpp` | segue os segmentos: `Update`, `Climbing`, `JumpOverGaps`, `LadderUpdate`, `CheckProgress`, `Avoid` |
| `ChasePath` | `NextBot/Path/NextBotChasePath.cpp` | perseguir uma entidade, com `LEAD_SUBJECT` (corta caminho prevendo para onde o alvo vai: `PredictSubjectPosition`) |
| `RetreatPath` | `NextBot/Path/NextBotRetreatPath.h` | fugir de uma ameaça pelo nav |
| `PlayerBody` | `NextBot/Player/NextBotPlayerBody.cpp` | cabeça: sacada, "mousepad virtual", rastreio do alvo |
| `IVision` | `NextBot/NextBotVisionInterface.cpp` | cone de visão, tempo para reconhecer (`GetMinRecognizeTime`) |
| Bot do HL2MP | `hl2mp/bot/` | `CHL2MPBotLocomotion`, `CHL2MPBotPathCost` (em `hl2mp_bot.h`), comportamentos em `behavior/` |
| Bot do TF2 | `tf/bot/` | `CTFBotLocomotion`, `CTFBotPathCost` (em `tf_bot.h`), comportamentos por classe e modo |
| Nav | `nav.h`, `nav_area.cpp`, `nav_generate.cpp`, `nav_edit.cpp`, `nav_ladder.cpp`, `nav_mesh.cpp` | áreas, ligações, escadas, geração e edição |

### 1.2 Como um caminho vira trechos (`Path::ComputePathDetails`)

O A* devolve uma lista de áreas. Esta função põe um ponto em cada passagem e marca o tipo:

- **Andar** (`ON_GROUND`): ponto no meio do portal entre as áreas (`ComputeAreaCrossing`).
- **Cair** (`DROP_DOWN`): se o desnível pela rampa passa do degrau, empurra o ponto para fora da borda
  (de 10 em 10u, até 2 larguras do corpo) até a caixa do corpo caber, e insere um ponto no chão de baixo.
- **Escada** (`LADDER_UP` / `LADDER_DOWN`): procura a `CNavLadder` que liga as duas áreas.
- **Pular vão** (`JUMP_OVER_GAP`): segunda passada. Se as áreas estão separadas mais de `1,9 × 25 = 47,5u`
  na horizontal e o vão é mais largo que metade do desnível, cria um ponto de decolagem na borda de
  cá e põe o pouso na borda de lá (meia largura do corpo para dentro).
- **Subir borda** (`CLIMB_UP`): se as áreas encostam mas o desnível passa do degrau, cria um ponto de
  decolagem na base e o destino no **centro** da área de cima.

O que isso significa:
- O vão **não tem limite de tamanho** aqui: se o nav tem ligação, vira pulo.
- O pouso de um `CLIMB_UP` é o centro da área de cima: em caixa grande o bot "mira" longe da borda.
- Nada aqui olha velocidade, gravidade ou se precisa de sprint.

### 1.3 Como o `PathFollower` executa

- `PathFollower::Update` chama, em ordem: escada (`LadderUpdate`), progresso (`CheckProgress`),
  subir (`Climbing`), vão (`JumpOverGaps`), desvio de obstáculo (`Avoid`) e por fim `mover->Approach`.
- `Climbing`: com `CLIMB_UP` a menos de uma largura de corpo, chama `ClimbUpToLedge`. Sem link
  planejado, ainda procura borda com traços para cima (até `ClimbUpHeight`) quando está travado.
  Debug: `nb_debug_climbing 1`.
- `JumpOverGaps`: se há `JUMP_OVER_GAP` a menos de 2 larguras de corpo e `IsGap` confirma buraco à
  frente, chama `JumpAcrossGap(pouso)`. Só pula **em pé** (`IsActualPosture(STAND)`).
- `IsGap`: traço de caixa da altura do degrau para baixo até `GetMaxJumpHeight`; se não achar chão, é vão.
- Liga e desliga: `nb_allow_climbing`, `nb_allow_gap_jumping`, `nb_allow_avoiding`.
- Olhar à frente: `nb_speed_look_ahead_range 150`, `nb_goal_look_ahead_range 50`, `nb_ladder_align_range 50`.

### 1.4 O que o bot jogador faz no pulo (`PlayerLocomotion`)

- `Jump()`: aperta pular e liga um cronômetro de 0,5 s (`IsClimbingOrJumping` fica verdadeiro até
  o cronômetro acabar **e** ele estar no chão).
- `ClimbUpToLedge`: só aceita se há `CLIMB_UP` a menos de 75u no caminho (`IsDiscontinuityAhead`) ou
  se o bot está travado ou o obstáculo é um prop físico móvel (`IsClimbPossible`). Depois aperta pular.
- `JumpAcrossGap`: aperta pular e vira a cabeça para o pouso.
- `Update` durante pulo ou subida: força correr (`SetMinimumSpeedLimit(GetRunSpeed())`), vira para o
  pouso e, **no pulo de vão, põe a velocidade horizontal direto em `GetRunSpeed()` na direção do pouso**
  (comentário no código: "cheat and max our velocity"). Ao tocar o chão, encerra.
- Números fixos no `.h`: `GetStepHeight` 18, `GetMaxJumpHeight` 57, `GetDeathDropHeight` 200.
- HL2MP e TF2 sempre fazem **pulo agachado** (`Update` da locomoção: no ar, `PressCrouchButton(0.3)`).
- HL2MP: `GetRunSpeed` = `hl2_normspeed` (256 no `movimento.cfg`, mas a corrida real do modo CA é 285);
  `GetDeathDropHeight` = sem limite se `falldamage` não for 1 e vida > 10. TF2: queda 1000.
- Escada: máquina de estados `ApproachAscendingLadder` → `AscendLadder` → `DismountLadderTop` (e o
  inverso para descer), com cronômetro de desistência `m_ladderTimer`.

### 1.5 O nav e os pulos

Constantes em `nav.h` (fora do CS:S, que é o nosso caso):

| Constante | Valor | Uso |
|---|---|---|
| `GenerationStepSize` | 25 | grade da geração |
| `StepHeight` | 18 | degrau na geração e no caminho (o Rearmed usa `sv_stepsize 30`) |
| `JumpHeight` | 41,8 | pulo em pé |
| `JumpCrouchHeight` | 64 (58 só no CS:S) | maior desnível que a geração liga como pulo para cima |
| `ClimbUpHeight` | 200 | altura em que o `PathFollower` procura borda para subir |
| `DeathDrop` | 400 (200 no CS:S) | queda que a geração ainda liga |
| `CliffHeight` | 300 | marca `NAV_MESH_CLIFF` |

- `design/medidas-mapas.md` diz que o nav liga até 58; esse é o valor do CS:S. No build HL2MP deve ser 64 (a conferir).
- Atributos úteis (`NAV_MESH_*`): `JUMP` (só na geração), `NO_JUMP`, `CROUCH`, `PRECISE` (não cortar
  caminho), `STOP`, `RUN`, `WALK`, `AVOID`, `STAIRS`, `OBSTACLE_TOP`, `CLIFF`, e bits livres a partir
  de `NAV_MESH_FIRST_CUSTOM` (0x10000) até `NAV_MESH_LAST_CUSTOM` (0x4000000) para o mod.
- **Jump connections** (`JumpConnector` em `nav_generate.cpp`, cvar `nav_generate_jump_connections 1`):
  áreas íngremes demais viram áreas `JUMP`; a geração liga direto as áreas dos dois lados dela se
  estiverem a menos de `3 × 25 = 75u` na horizontal. Depois as áreas `JUMP` podem ser removidas
  (`nav_remove_jump_areas`). Não liga vãos sobre o vazio nem pilastra a pilastra.
- Comandos de edição para pulos: `nav_edit 1`, `nav_mark`, `nav_connect` (liga a marcada à mirada,
  só de ida), `nav_disconnect`, `nav_jump`, `nav_no_jump`, `nav_crouch`, `nav_precise`,
  `nav_mark_attribute`/`nav_clear_attribute`, `nav_splice`, `nav_build_ladder`, `nav_show_area_info`.
- A ligação do nav **não guarda nada sobre o pulo** (nem velocidade, nem se é vão). Só "A liga em B na direção X".

### 1.6 Custo de caminho

- `CHL2MPBotPathCost` (`hl2mp/bot/hl2mp_bot.h`) e `CTrokBotPathCost` (`trok_bot_team.cpp`):
  - desnível ≥ degrau: distância × 2; ≥ pulo máximo (57): proibido; queda maior que a mortal: proibido;
  - o desnível vem de `ComputeAdjacentConnectionHeightChange` (altura entre as bordas dos portais);
  - **vão não pesa nada**: o tamanho horizontal não entra no custo.
- HL2MP sorteia preferência de rota por bot (`FastCos(entindex × área × tempo/10)`); o Trok usa células de 512u.
- NEOTOKYO (`neo_bot_path_cost.cpp`): pulo custa `1000 × (desnível / pulo máximo)²` (cvar
  `neo_bot_path_penalty_jump_multiplier`), escada ×3, exposição pela arma.

### 1.7 O que já veio para o repo

- `NextBotPathFollow.cpp` já tem blocos `#ifdef TROK` com correções do NEOTOKYO: repath no meio do
  vão aponta para o pouso; queda já começada segue para baixo; `HasFallenBelowPath` (caiu da borda →
  repath ao pousar, com contador); não cortar caminho em área `PRECISE`.
- `CTrokBotLocomotion` (`trok_bot_body.cpp`): agacha no ar quando a borda passa do pulo em pé
  (`trok_bot_pulo_agachado`), faixa no corredor, `OnStuck` → `TrokBot_NoteStuck` (memória de travamento
  por área, usada no custo).

### 1.8 O que falta para parkour de verdade

| Falta | Por quê | Onde mexer |
|---|---|---|
| Limites lidos da física do jogo | 18/57/200 fixos; o Rearmed sobe 30 andando, ~50 pulando, ~67 com pulo agachado, até 80 escalando (se `trok_escalar 1`) | sobrescrever `GetStepHeight`, `GetMaxJumpHeight`, `GetDeathDropHeight` na locomoção do bot lendo `sv_stepsize`, `sv_gravity`, `trok_ca_pulo`, `trok_ca_agachar_ar` |
| Saber se o vão alcança | `JumpAcrossGap` só pula e trapaceia a velocidade | simulador de pulo (seção 2.3) antes de aceitar o link e antes de pular |
| Pulo com corrida | sem distância de corrida antes da decolagem; a velocidade é "colada" | ponto de **embalo** antes da decolagem, sprint se o link pedir |
| Caixa a caixa | cada pulo é decidido sozinho; o próximo pulo não é planejado | olhar 2–3 links à frente; esperar recuperar o fator de pulo (`trok_ca_pulo_recupera`) |
| Quinas e bordas estreitas | pouso no centro da área de cima ou em `landing + meia largura`; em pilastra pequena erra | pouso no ponto mais seguro da área (mais longe das bordas), decolagem perpendicular à borda |
| Custo de vão e de risco | vão grande custa igual a andar | custo por dificuldade do pulo e pelo histórico de falhas no link |
| Links que o gerador não faz | vãos e pilastras não ligam sozinhos | gerar links por simulação (offline ou ao carregar o mapa), entidade de mapa, ou aprender do jogador |
| Escada e escalar do Trok | `trok_escalar` (segurar pular contra a borda) não existe no NextBot | tratar como tipo de link "escalar" com altura até `trok_escalar_altura_max` |
| Controle no ar | agachado no ar o controle cai a 10% (`trok_ca_ar 0.1`) | agachar só depois de passar a borda, não logo na decolagem |

---

## 2. Parkour e navegação 3D

### 2.1 A física do Rearmed em números (modo CA, `cfg/movimento.cfg`)

Fórmulas (g = `sv_gravity`, v0 = `trok_ca_pulo`, v = velocidade horizontal):
- altura do pulo: `h = v0² / (2g)`; tempo até o topo: `v0 / g`; tempo no ar até o mesmo nível: `2·v0 / g`;
- tempo até cruzar a altura H subindo numa caixa: `t = (v0 + √(v0² − 2gH)) / g` (descendo, o mais tarde possível);
- tempo para cair até D abaixo: `t = (v0 + √(v0² + 2gD)) / g`; alcance = `v × t`.

Com `sv_gravity 1100`, `trok_ca_pulo 330`, corrida 285, sprint 399 (285 × 1,4), andar 70:

| Medida | Valor (a conferir no jogo) |
|---|---|
| Altura do pulo em pé | ~49,5u (0,3 s até o topo) |
| Tempo no ar (mesmo nível) | 0,6 s |
| Pulo agachado | ~67u (as pernas sobem 18u: `trok_ca_agachar_ar 0.5` × 36) |
| Vão no mesmo nível, correndo | ~171u |
| Vão no mesmo nível, sprint | ~239u |
| Subir numa caixa de 40u, correndo | alcança ~123u de distância horizontal |
| Cair numa caixa 64u abaixo, correndo | ~216u |
| Degrau sem pular | 30u (`sv_stepsize`) |
| Queda sem dano | até 300u do CA (`trok_ca_queda_min`) |

Coisas da física que mudam a decisão do bot:
- **Fator de pulo**: depois de cada pulo, o próximo e a aceleração caem a `trok_ca_pulo_min` (0,3) e
  recuperam `trok_ca_pulo_recupera` (1,2/s), voltando ao normal em ~0,6 s (`CTrokGameMovement::CAHop`).
  Subiu numa caixa em 0,43 s? O pulo seguinte sai com ~0,82 (a conferir se o fator multiplica a velocidade
  do pulo ou a altura). Para caixa a caixa: esperar ~0,15 s no topo ou planejar com o fator.
- **Aceleração em rampa**: 500 → 3000 em ~0,4 s (`trok_ca_acel_*`). Parado na borda, o bot não chega à
  corrida num passo: precisa de embalo de ~0,4 s (~60–80u) para pulo longo.
- **Controle no ar**: forte em pé (`trok_ca_ar_rampa 1`), quase nenhum agachado (`trok_ca_ar 0.1`).
- **Pulo antecipado e "coyote"**: `trok_pulo_coyote 0.12` e `trok_pulo_antecipado 0.12` (no modo Source)
  dão margem de erro também para o bot. Conferir se valem no modo CA.

### 2.2 Como fazer o nav entender saltos

Ideias usadas em outros motores, da mais simples à mais completa:

1. **Ligação à mão no nav** (já existe): `nav_mark` na área de decolagem, mirar o pouso, `nav_connect`;
   marcar `nav_jump` onde é pulo. Problema: a ligação não diz tamanho, velocidade nem tipo.
2. **Tabela de links do mod (sidecar)**: um arquivo por mapa, ex. `maps/<mapa>.trokpulos.txt`, com
   `área_de, área_para, ponto_decolagem, ponto_pouso, tipo (vão/subir/cair/escalar), precisa_sprint,
   precisa_agachar, velocidade_mínima, dificuldade`. O A* usa o link como ligação extra (custo próprio) e
   o `PathFollower` usa os dados. É o que o Detour chama de **off-mesh connection**: dois pontos, raio,
   direção (ida ou ida e volta), área e um id do usuário (`dtOffMeshConnection`).
3. **Links gerados por simulação** (como as "reachabilities" do Quake 3): para cada borda de área,
   testar pousos nas áreas próximas com o simulador da seção 2.3. O AAS do Q3 guarda tipos como
   `TRAVEL_WALKOFFLEDGE`, `TRAVEL_BARRIERJUMP`, `TRAVEL_JUMP`, `TRAVEL_ROCKETJUMP`, `TRAVEL_LADDER`
   (`botlib/aasfile.h`, `be_aas_reach.c`). Rodar uma vez por mapa (comando tipo `trok_nav_gerar_pulos`)
   e salvar no sidecar.
4. **Entidade de mapa**: um ponto de decolagem e um de pouso colocados no Hammer (como os "jump
   waypoints" do Xonotic, que ficam 60–80u antes da borda, e os Nav Link Proxy do Unreal).
   Bom para o mapa de teste do Surto.
5. **Aprender do jogador**: quando um humano decola de uma área e pousa em outra que não tem ligação,
   guardar decolagem, pouso e velocidade como link candidato; confirmar com o simulador. Lembra o
   "Human Trace Replay" do bot UT^2 (BotPrize), que repetia trajetos humanos para sair de onde travava.
6. **Áreas de pilastra pequenas**: o gerador junta ou descarta áreas pequenas. Em topo de pilastra, criar a
   área à mão (`nav_begin_area`/`nav_end_area`) e marcar `nav_precise` para o bot não cortar caminho.

### 2.3 Planejar o pulo com física (simular antes de pular)

Por link (na geração) e de novo na hora (com a velocidade real do bot):

1. Ler `sv_gravity`, `trok_ca_pulo`, fator de pulo atual (`CAHop`), velocidade atual e as caixas do
   corpo em pé (32×32×72) e agachado (32×32×36).
2. Para algumas velocidades de saída (andar, correr, sprint) e direções (direto ao pouso ± 10°):
   - simular passos de 1/66 s (tick): `vel.z -= g·dt`; `pos += vel·dt`;
   - a cada passo, `UTIL_TraceHull` do passo anterior ao atual com a caixa do momento
     (trocar para agachado a partir do ponto em que o bot vai agachar);
   - bateu na parede de frente: tentar agachar mais cedo ou desistir dessa velocidade;
   - pousou: conferir se é a área de destino e se sobra margem de borda (≥ meia largura).
3. Guardar a **menor** velocidade que funciona e a **margem** (quanto sobra de vão e de altura).
   Dificuldade do link = 1 − margem normalizada. Ela entra no custo e na chance de erro.
4. Na hora do pulo, simular de novo com a posição e velocidade reais; se não alcança, recuar para
   pegar embalo (ponto de embalo = decolagem − direção × distância de aceleração).

Exemplo pronto de "achar a altura de pulo por traços": `CZMBansheeFollowPath::FindJumpHeight` e
`CanPotentiallyJumpTo` em `refs/zmr-game/src/game/server/zmr/npcs/zmr_banshee_path.cpp` (sobe a caixa de
16 em 16u até o traço passar; pulo máximo 512u de distância e 768u de altura para o zumbi banshee).

### 2.4 Errar como humano

Fontes de erro (sortear por pulo, escala pelo nível do bot):

| Erro | Como sortear | Ponto de partida |
|---|---|---|
| Atraso de reação antes de um pulo não planejado | ex-gaussiana | mínimo 0,15 s, média 0,25–0,45 s |
| Tempo da decolagem | normal em torno da borda | desvio 20–60 ms (≈ 6–17u a 285 u/s) |
| Direção | normal | desvio 2–6° |
| Velocidade (não segurou sprint, não embalou) | chance por pulo | 5–20% dos pulos longos saem com corrida em vez de sprint |
| Agachar no ar tarde | normal | 30–100 ms de atraso |
| Hesitar na borda | chance quando a margem é pequena | parar 0,2–0,8 s olhando o pouso |

Depois do erro:
- **caiu**: o `HasFallenBelowPath` (já no repo) faz repath ao pousar;
- **tentar de novo**: no máximo 1–3 tentativas no mesmo link, mais para bot "teimoso" ou agressivo;
- **desistir**: depois de N falhas, custo do link × grande por um tempo, ou proibido na rodada;
- **memória**: falhas e sucessos por link, por bot e por time, com decaimento (ex.: meia-vida de 60 s);
  o NEOTOKYO soma 1000 no custo da área onde travou e trata como "propensa a travar" depois de algumas vezes
  (`neo_bot_path_reservation.*`, cvar `neo_bot_path_reservation_onstuck_penalty 1000`);
- **aprender**: a chance de erro do bot cai um pouco a cada sucesso no mesmo link na partida (gente
  também acerta o pulo na terceira vez).

Importante: errar **não pode** virar bot parado tentando o mesmo pulo para sempre. Prever limite de
tentativas e um caminho alternativo sempre.

---

## 3. Mira e percepção humanas

### 3.1 O que o NextBot já tem (cvars do `PlayerBody` e da visão)

| Cvar / função | Valor | O que é |
|---|---|---|
| `nb_saccade_speed` | 1000 °/s | giro máximo da cabeça (`GetMaxHeadAngularVelocity`) |
| `nb_saccade_time` | 0,1 s | duração da sacada |
| `nb_head_aim_settle_duration` | 0,3 s | tempo parado para "assentar" a mira |
| `nb_head_aim_steady_max_rate` | 100 °/s | abaixo disso a cabeça conta como firme |
| `nb_head_aim_resettle_angle` / `_time` | 100° / 0,3 s | depois de girar 100°, pausa 0,3 s para "levantar o mouse" |
| `GetHeadAimTrackingInterval` | HL2MP/TF2: expert 0,05; difícil 0,1; normal 0,25 s | de quanto em quanto tempo reamostra posição e velocidade do alvo |
| `GetMinRecognizeTime` | fácil 1,0; normal 0,5; difícil 0,3; expert 0,2 s | reação visual mínima (`hl2mp_bot_vision.cpp`, `tf_bot_vision.cpp`) |
| `tf_bot_notice_gunfire_range` / `_quiet_` | 3000 / 500u | alcance para ouvir tiro e tiro com silenciador (iguais no NEOTOKYO) |

O Trok já troca reação, giro e intervalo pelo perfil (`trok_bot_profile.*`, `scripts/trok_bot_perfis.txt`).

### 3.2 Como bots conhecidos fazem

- **Bot oficial do CS (Booth, 2004)**: o bot só reage ao que um humano perceberia; cabeça mola-amortecedor
  mais rígida em combate; desvio de mira sorteado de tempos em tempos e menor com a habilidade. Perfis
  em `botprofile.db` (ReactionTime, AttackDelay, AimFocusInitial/Decay/OffsetScale/Interval). Os valores
  "Expert" que circulam na internet discordam (ReactionTime de 0,1 a 0,0001; a conferir no arquivo do jogo).
- **CS:GO depois de 2019**: bots com **behavior tree** em KeyValues3 (`scripts/ai/*.kv3`, `mp_bot_ai_bt`),
  usados no Deathmatch, Guardian e Co-op; um `bt_config.kv3` por modo com reação e combate por nível.
- **TF2 e HL2MP (NextBot)**: reação mínima fixa por nível, intervalo de rastreio por nível, giro limitado.
  Simples e previsível: bom piso, ruim como "humano".
- **L4D (Booth, 2009)**: "Reactive Path Following" para todos os atores (segue o caminho olhando à frente e
  desviando localmente); zumbis comuns **escalam** de forma algorítmica, parecida com o desvio de
  obstáculo; o Director controla ritmo.
- **Quake 3 (van Waveren, 2001)**: características por bot (`chars.h`): tempo de reação (0–5 s),
  precisão geral e por arma, habilidade de mira (prever movimento do alvo só nos níveis médios e altos).
  Níveis 2 e 3 são interpolação dos níveis 1 e 4.
- **Estudos com o bot do Q3**: tempo de decisão e habilidade de mira são os parâmetros que mais pesam para
  o bot parecer humano (busca, ver fontes; a conferir no artigo).
- **UT2004 / BotPrize**: o vencedor UT^2 (Univ. do Texas) limitava a mira e o giro a valores humanos e
  repetia trajetos humanos gravados para sair de lugares onde travava.

### 3.3 Modelo humano (o que dá para copiar)

- **Reação**: distribuição **ex-gaussiana** (normal + cauda exponencial). Jogador médio ~0,30 s, bom
  ~0,25 s, elite 0,15–0,20 s (números já usados em `design/bots.md`). Nunca abaixo de ~0,15 s.
  Reação a estímulo esperado (ele já sabia que você ia aparecer ali) é mais curta que a surpresa.
- **Mover a mira (lei de Fitts)**: tempo = `a + b · log2(D/L + 1)` (D = ângulo até o alvo, L = largura
  angular do alvo). Alvo longe na tela ou pequeno (longe no mapa) leva mais tempo. Em tarefas de FPS
  (Aim Lab) a lei de Fitts explica mal o total, mas a forma geral vale (a conferir: Frontiers 2022).
- **Submovimentos**: o primeiro movimento é balístico e rápido e tende a **ficar curto** (undershoot),
  porque corrigir para frente custa menos que voltar; depois vêm 1–2 correções pequenas. Overshoot existe,
  mais em flick rápido e em jogador agressivo. Receita: primeiro movimento cobre 85–95% do ângulo
  (a conferir), com 10–20% de chance de passar do alvo; correção depois de 80–150 ms.
- **Rastreio**: o erro cresce com a velocidade angular do alvo; o humano segue com atraso (reamostra a
  posição a cada ~0,1–0,25 s) e "adivinha" a velocidade. Na troca de direção do alvo (ADAD) o erro salta.
- **Micro-correções**: com o alvo parado, a mira não fica cravada: pequenos ajustes de 0,1–0,5° a cada
  100–300 ms (a conferir). Ruído suave (passeio aleatório), nunca tremor de quadro a quadro.
- **Visão**: cone útil ~90–130° (campo do jogo), centro (±10°) muito melhor que a periferia; de longe a
  periferia quase não vê; movimento chama atenção. O `CTrokBotVision` já faz isso com a barra de percepção.
- **Audição**: passos e tiros dão **direção e região**, não ponto; parede abafa. Distâncias do repo: passos
  correndo ~1100u, andando rápido ~600u; tiro 3000u e silenciado 500u (TF2/NEOTOKYO).

### 3.4 Números para começar (por nível; ajustar no teste)

| Parâmetro | Fácil | Normal | Difícil | Expert |
|---|---|---|---|---|
| Reação (média ex-gaussiana) | 0,55 s | 0,40 s | 0,30 s | 0,22 s |
| Mínimo de reação | 0,25 | 0,20 | 0,17 | 0,15 |
| Primeiro movimento (fração do ângulo) | 0,75–0,90 | 0,82–0,95 | 0,88–0,98 | 0,92–1,0 |
| Chance de passar do alvo | 25% | 18% | 12% | 8% |
| Atraso da correção | 0,20 s | 0,15 | 0,12 | 0,08 |
| Intervalo de rastreio | 0,40 | 0,20 | 0,12 | 0,08 (já em `bots.md`) |
| Giro máximo | 400 °/s | 600 | 800 | 950 (já em `bots.md`) |

Regra de equilíbrio de `bots.md` continua valendo: o nível muda **reação, mira e tática**, nunca
informação (ver através de parede) nem dano.

---

## 4. Tática e comportamento

### 4.1 Arquiteturas

| Arquitetura | Como é | Bom para | Cuidado |
|---|---|---|---|
| FSM / pilha de ações (NextBot `Action<T>`) | estados com `OnStart/Update/OnEnd`, eventos, `SuspendFor`/`ChangeTo` | o que já existe; fácil de depurar com `nb_debug BEHAVIOR` | cresce em "espaguete" se tudo virar estado |
| **Utility AI** | cada opção ganha nota de 0 a 1 por "considerações" (vida, munição, distância...) passadas por curvas; multiplica; escolhe a maior ou sorteia entre as melhores | escolher **o que** fazer agora (segurar, caçar, recuar, apoiar); dá variedade natural | zero numa consideração veta a opção; precisa de curvas em cfg |
| Behavior tree | árvore de seletores e sequências | rotinas com passos (ex.: "checar cantos", "entrar no abrigo") | árvore grande fica rígida; o CS:GO usou para modos novos |
| **HTN** (Killzone 2/3, Decima) | tarefa grande se decompõe em subtarefas por métodos com pré-condições | planos de time ("atacar o abrigo por duas entradas") | mais código |
| **GOAP** (F.E.A.R.) | objetivos + ações com pré-condições e efeitos; A* monta a sequência | variedade sem roteiro | o "flanco" famoso do F.E.A.R. era só ir de cobertura em cobertura |

Sugestão de mistura (comum em jogos): **utility** para escolher o objetivo, **ações do NextBot** para
executar, e um **comandante de time** em cima (Killzone usa camadas: comandante → líder de grupo → bot).

### 4.2 Director do L4D (para o Surto)

- O jogo mede a **intensidade** de cada jogador (dano levado, inimigos perto, abates) e alterna fases:
  **construir** (manda inimigos), **pico** (segura alguns segundos), **relaxar** (para de mandar até a
  intensidade cair). Tempos exatos: a conferir no PDF do Booth.
- Inimigos nascem **fora da vista** e pelo caminho que os jogadores vão seguir ("flow").
- Para o Surto: um diretor dos zumbis bots que decide **quando** atacar em onda e **por onde**, com
  pausas para os humanos respirarem. Liga bem com `trok_bot_zumbi_grupo` (esperar juntar 3 e atacar junto).

### 4.3 Peças táticas

- **Influence map**: grade ou nav com valores que se espalham (perigo, controle do time, onde o inimigo
  provavelmente está) e decaem. Serve para escolher rota segura, flanco (caminho onde a influência
  inimiga é baixa) e onde procurar. O nav já tem `GetDanger`/`IncreaseDanger` por time.
- **Cobertura**: área com `hiding spots` (`nav_analyze`) e baixa visibilidade das áreas de onde vem o
  inimigo (`IsPotentiallyVisible`). Já usado no mapa tático do Trok.
- **Flanco**: custo de caminho que penaliza áreas vistas pelo inimigo conhecido; o Trok já penaliza
  a rota dos colegas (lurker).
- **Killzone (avaliação de posição)**: soma ponderada de fatores por ponto (distância ideal da arma,
  cobertura, linha de tiro, altura). O mapa tático do Trok segue essa ideia.
- **Comunicação**: avisos com erro (som dá região), trava de ângulos, "trade". Já no Trok (`trok_bot_team.*`).
  O NEOTOKYO soma a amostra de sons de combate em grupos de 500u numa janela de 3 s
  (`neo_bot_memory_sound_combat.cpp`).
- **Fogo de supressão** (NEOTOKYO `neo_bot_suppressive_fire.cpp`): atirar 2–3 s onde o inimigo foi visto
  há menos de 3 s, com erro de 10° e 20% na distância, varrendo ±64u.

### 4.4 Por modo

- **Equipe**: utility para segurar/caçar/apoiar; comandante leve que distribui entradas e evita todos
  no mesmo corredor; supressão e trade. Parkour serve para chegar em lugar alto e flanquear.
- **Solo (todos contra todos)**: sem comunicação; mais peso para som e para ir onde houve tiro
  (terceiro na briga); variar rota por bot. Influence map por bot, não por time.
- **Surto: zumbis**:
  - perseguir com `ChasePath` em `LEAD_SUBJECT` (corta caminho prevendo o humano);
  - **cerco**: cada zumbi recebe uma entrada diferente da região do humano (o mapa tático já lista entradas);
    custo extra em áreas que outro zumbi já reservou (como `neo_bot_path_reservation`);
  - esperar juntar e atacar junto (já existe); diretor para ondas;
  - zumbi pode ter pulo maior (`trok_surto_zumbi_pulo`): os links de pulo precisam de **perfil de pulo
    por tipo de bot** (humano e zumbi alcançam lugares diferentes);
  - referência de zumbi que salta e embosca no teto: `zmr_banshee*.cpp` no ZMR.
- **Surto: humanos**:
  - recuar atirando com `RetreatPath` e para lugar alto ou abrigo; parkour vira fuga (o zumbi pode não
    alcançar o topo da pilastra);
  - defender abrigo: cada humano vigia uma entrada da grade (`info_trok_abrigo`, `func_trok_grade`);
  - empurrão por arma: recuar quando o zumbi encosta (`trok_bot_recuo_dist`).

---

## 5. Código aberto de referência

### 5.1 No repo (`cs-source/refs/`)

| Projeto | Onde | O que serve |
|---|---|---|
| NEOTOKYO Rebuild ([github.com/NeotokyoRebuild/neo](https://github.com/NeotokyoRebuild/neo)) | `neotokyo-rebuild/src/game/server/neo/bot/` e `.../NextBot/` | **o mais útil**: TF2 bot portado. Custo de caminho com pulo quadrático e exposição pela arma (`neo_bot_path_cost.cpp`), reserva de caminho e penalidade de travou/morreu (`neo_bot_path_reservation.*`), contorno de props e quebráveis (`neo_bot_path_obstacles.*`, `neo_bot_prop_detour.*`), agachar sob viga (`neo_bot_locomotion.cpp`, `IsOnlyCrouchClearAhead`), supressão, memória de som, escada (`behavior/neo_bot_ladder_*`), perfis. Ver `design/estudo-neotokyo.md` |
| Zombie Master: Reborn ([github.com/zm-reborn/zmr-game](https://github.com/zm-reborn/zmr-game)) | `zmr-game/src/game/server/npcr/` e `.../zmr/npcs/` | NPCs zumbis com sistema próprio ("NPCR", parecido com NextBot): caminho, perseguição (`npcr_path_chase`), sentidos, motor de jogador; **banshee** com pulo planejado por traços e emboscada no teto; `zmr_playerbot` |
| Double Action ([github.com/BSVino/DoubleAction](https://github.com/BSVino/DoubleAction)) | `double-action/mp/src/game/server/sdk/bots/` | bot simples antigo do SDK (`sdk_bot_navigation.cpp`, `sdk_bot_combat.cpp`), sem NextBot; mod de ação com mergulho e acrobacia, serve de ideia de movimento |
| Fortress Forever, Momentum | `fortress-forever/`, `momentum-mod/` | sem bots; o Momentum serve para física de movimento (bhop, surf) e ferramentas de replay de trajeto |
| Mapbase | `mapbase/` | sem bots jogadores; NPCs do HL2 e entidades extras |

### 5.2 Na web

| Projeto | Link | O que serve |
|---|---|---|
| NavBot | [github.com/caxanga334/NavBot](https://github.com/caxanga334/NavBot) | bot de Source como extensão de SourceMod, com nav mesh da Valve modificado e independente do jogo (TF2, DoD:S, ZPS, CS:S, HL2DM...). Ver como trata pulos e escadas em vários jogos (a conferir nos `docs/`) |
| RCBot2 | [github.com/APGRoboCop/rcbot2](https://github.com/APGRoboCop/rcbot2) · [wiki](https://developer.valvesoftware.com/wiki/Rcbot2) | bot por waypoints feitos à mão, com tipos de waypoint (pular, agachar, escada, sniper); ideia de "ponto especial" para pulo |
| NextBot (wiki da Valve) | [developer.valvesoftware.com/wiki/NextBot](https://developer.valvesoftware.com/wiki/NextBot) | visão geral; código liberado no SDK 2013 em fevereiro de 2025 |
| Quake 3 botlib (AAS) | [ioquake3](https://github.com/ioquake/ioq3) (`code/botlib/`) · [OpenArena no Debian](https://sources.debian.org/src/openarena/0.8.8%2Bdfsg-2/code/botlib/aasfile.h) | **reachabilities**: tipos de travessia (andar, cair, pulo, pulo de barreira, escada, rocket jump) calculadas por simulação; características de bot (`chars.h`); [análise do código por Fabien Sanglard](https://fabiensanglard.net/quake3/a.i.php) |
| Recast/Detour | [github.com/recastnavigation/recastnavigation](https://github.com/recastnavigation/recastnavigation) | **off-mesh connections** (`dtOffMeshConnection`: dois pontos, raio, ida/volta, id do usuário); DetourCrowd para desvio entre agentes |
| Xonotic (havocbot) | [gitlab.com/xonotic/xonotic-data.pk3dir](https://gitlab.com/xonotic/xonotic-data.pk3dir) · [MR de jump waypoints](https://gitlab.com/xonotic/xonotic-data.pk3dir/merge_requests/670) | waypoint de pulo 60–80u antes da borda + waypoint de pouso; links "hardwired" num arquivo por mapa |
| Unvanquished | [wiki: Bot design](https://wiki.unvanquished.net/wiki/Bot_design) · [github.com/Unvanquished/Unvanquished](https://github.com/Unvanquished/Unvanquished) | bots com navmesh (Recast/Detour) + behavior tree em arquivo de texto; a maioria dos travamentos vinha do navmesh |
| OpenArena | [sources.debian.org/src/openarena](https://sources.debian.org/src/openarena/0.8.8+dfsg-5/code/game/ai_main.c/) | mesma botlib do Q3, código de IA do jogo (`ai_main.c`, `ai_dmq3.c`) |
| Left 4 Bots 2 | [github.com/smilz0/left4bots](https://github.com/smilz0/left4bots) | VScript que melhora os sobreviventes bots do L4D2 "para parecer gente": ideias de prioridade e de cooperação |
| CS:GO behavior trees | [wiki: Bot Behavior Trees](https://developer.valvesoftware.com/wiki/Mp_bot_ai_bt) | formato de árvore em `.kv3`, nós, `bt_config` por nível |

---

## 6. Testes: o bot parece humano?

### 6.1 Métricas

| Métrica | Como medir | Alvo (ponto de partida) |
|---|---|---|
| Tempo de reação | do momento em que o alvo fica visível ao primeiro tiro | expert 0,3–0,5 s; fácil > 0,8 s (`bots.md`) |
| Distribuição da reação | histograma de 50+ amostras | cauda para a direita; nunca abaixo de 0,15 s |
| Precisão por distância | % de acerto a 300, 800, 1500u, alvo parado e em ADAD | cair com a distância e com ADAD; expert ~80–90% a 300u parado |
| Erro do primeiro movimento | ângulo no fim do primeiro giro ÷ ângulo inicial | 0,85–0,95 em média, com algumas passadas |
| Correções por engajamento | contar trocas de direção do giro | 1–3 |
| Quedas por pulo | falhas ÷ tentativas por link | 0% em link fácil; 5–25% em link difícil, por nível |
| Tentativas antes de desistir | por link | ≤ limite do cfg |
| Tempo no percurso | tempo do bot ÷ tempo de um humano no mesmo percurso de parkour | 1,1–1,6 (mais lento que o dono, mas chega) |
| Travamentos | eventos `OnStuck` por minuto | ~0 em mapa com nav bom |
| Teste cego | alguém assiste a demo e marca quem é bot | errar perto de 50% |

### 6.2 Roteiro com o MCP `trok` (`cs-source/tools/mcp/LEIA-ME.md`)

1. `diagnostico`, depois `jogo_abrir` (sobe com `-dev -console -condebug -netconport 2121`).
2. `comando`: `map <mapa_de_teste>; sv_cheats 1`; se não houver nav: `nav_generate`, depois `nav_analyze`.
3. Conferir os links: `comando` com `nav_edit 1`, andar até as caixas, `print_tela` para ver as ligações.
4. Bot: `comando` com `hl2mp_bot_add`, `nb_select` (mirando nele), `nb_debug PATH`, `nb_debug_climbing 1`,
   `nb_debug BEHAVIOR`; `print_tela` durante o pulo.
5. Parkour atrás do jogador: subir no ponto alto do mapa de teste; `cvar` para ligar o modo de seguir
   (ex.: um `trok_bot_seguir_jogador 1`, se a versão local tiver); `console` com filtro do log de pulos.
6. Medir: o bot deve escrever **uma linha por pulo** no console, fácil de filtrar, por exemplo
   `TROKPULO bot=3 link=17 tipo=vao sim_margem=22 vel=301 resultado=ok tentativa=1`; e por reação
   `TROKREACAO bot=3 alvo=1 dist=812 t=0.31 primeiro=0.91 correcoes=2`. A ferramenta `console` com filtro
   `TROKPULO` devolve as linhas; o agente soma taxas.
7. Repetir com `hl2mp_bot_difficulty 0..3` e com os erros desligados (cvar de erro em 0) para comparar.
8. `erros` no fim, para pegar avisos de modelo, som ou nav.
9. Comparar com o simulador `tools/botsim/` quando der (ele não tem física de pulo; dá para acrescentar
   o simulador de pulo da seção 2.3 lá, em Python, e testar os números sem o jogo).

---

## 7. Receita: bot que faz parkour atrás do jogador e às vezes erra

### 7.1 Preparar o mapa (uma vez)

1. `nav_generate`, `nav_analyze`.
2. Gerar candidatos de pulo: para cada área, para cada área a até `pulo_busca_raio` (ex.: 300u) sem
   ligação a pé, rodar o simulador (2.3) com andar, correr e sprint, em pé e agachando no ar.
3. Guardar os que funcionam: decolagem, pouso, tipo, velocidade mínima, agachar, margem, dificuldade.
   Salvar no sidecar do mapa. Links feitos à mão (`nav_connect`) também passam pelo simulador.

### 7.2 A cada decisão de caminho

1. Alvo = posição do jogador (ou a prevista, como no `ChasePath` com `LEAD_SUBJECT`).
2. A* com links de pulo: custo = distância × (1 + `pulo_custo` × dificuldade) × (1 + `pulo_custo_falha` ×
   falhas recentes no link) + `pulo_custo_fixo`. Bot cauteloso ou fraco paga mais pelo pulo difícil.
3. Bot que não alcança (link pede sprint e ele está sem fôlego, ou dificuldade > `pulo_dificuldade_max`
   do nível): o link fica fora do caminho.

### 7.3 Executar um link (pseudocódigo)

```
ao_chegar_perto_do_link(link):
    se link exige embalo e velocidade_atual < link.vel_min:
        ir para ponto_embalo = decolagem - direção * dist_embalo   # ~60-80u no modo CA
    se fator_pulo_atual < pulo_fator_min:                           # CAHop ainda recuperando
        esperar até recuperar (ou até tempo_max_espera)
    se link.margem < pulo_hesitar_margem e sorteio < pulo_hesitar_chance[nível]:
        parar e olhar o pouso por sorteio(0.2, 0.8) s
    sortear erros: dt_decolagem ~ N(0, pulo_erro_tempo[nível])
                   dang ~ N(0, pulo_erro_angulo[nível])
                   sem_sprint = sorteio < pulo_erro_vel_chance[nível]
    simular de novo com posição, velocidade e erros reais
    se não alcança e não há erro sorteado de propósito: corrigir (mais embalo) ou trocar de caminho
    apertar pular no tick em que (distância até a borda / velocidade) = dt_decolagem
    no ar: virar para o pouso; agachar ao passar a altura da borda + atraso sorteado
    sem trapaça de velocidade: quem move é o gamemovement, como no jogador

ao_pousar:
    se pousou na área do link: link.sucessos++ ; reduzir um pouco o erro deste bot neste link
    senão: link.falhas++ ; tentativas++
        se tentativas < pulo_tentativas_max[nível]: refazer caminho até a decolagem
        senão: link proibido para este bot por pulo_desiste_tempo s; refazer caminho por outro lado
```

### 7.4 Parâmetros (cfg/cvar, nunca no código)

| Parâmetro | Exemplo | O que faz |
|---|---|---|
| `trok_bot_pulo_busca_raio` | 300 | raio da busca de links na geração |
| `trok_bot_pulo_margem_min` | 8 | margem mínima (u) para um link valer |
| `trok_bot_pulo_custo` | 3 | peso da dificuldade no custo |
| `trok_bot_pulo_custo_falha` | 2 | peso de cada falha recente |
| `trok_bot_pulo_custo_fixo` | 50 | custo fixo por pulo (gente evita pular sem motivo) |
| `trok_bot_pulo_falha_meia_vida` | 60 | s para a memória de falha cair pela metade |
| `trok_bot_pulo_dificuldade_max` | 0,5 / 0,7 / 0,85 / 1 | por nível |
| `trok_bot_pulo_erro_tempo` | 0,06 / 0,045 / 0,03 / 0,02 | s (desvio da decolagem) |
| `trok_bot_pulo_erro_angulo` | 6 / 4 / 3 / 2 | graus |
| `trok_bot_pulo_erro_vel_chance` | 0,2 / 0,12 / 0,07 / 0,04 | chance de não embalar ou não segurar sprint |
| `trok_bot_pulo_agachar_atraso` | 0,1 / 0,07 / 0,05 / 0,03 | s |
| `trok_bot_pulo_hesitar_margem` | 20 | u; abaixo disso pode hesitar |
| `trok_bot_pulo_hesitar_chance` | 0,4 / 0,25 / 0,15 / 0,05 | por nível |
| `trok_bot_pulo_tentativas_max` | 1 / 2 / 3 / 3 | por nível (agressivo +1) |
| `trok_bot_pulo_desiste_tempo` | 45 | s com o link proibido para aquele bot |
| `trok_bot_pulo_fator_min` | 0,9 | espera o fator de pulo (`CAHop`) passar disto |
| `trok_bot_pulo_dist_embalo` | 70 | u de corrida antes de pulo longo |
| `trok_bot_pulo_aprende` | 0,15 | quanto o erro cai a cada sucesso no mesmo link |
| `trok_bot_pulo_log` | 0 | 1 = linha `TROKPULO` por pulo no console |
| `trok_bot_pulo_erros` | 1 | 0 = desliga os erros sorteados (para comparar) |

Os quatro valores separados por barra são fácil / normal / difícil / expert e podem ir para
`scripts/trok_bot_perfis.txt` em vez de cvars.

---

## 8. Fontes

Código (no repo):
- `cs-source/sdk/src/game/server/NextBot/` (Path, PathFollow, PlayerLocomotion, PlayerBody, Vision), `nav.h`, `nav_generate.cpp`, `nav_area.cpp`
- `cs-source/sdk/src/game/server/hl2mp/bot/`, `cs-source/sdk/src/game/server/tf/bot/`
- `cs-source/src/game/server/trok/bot/`, `cs-source/src/game/shared/trok/trok_gamemovement.cpp`, `cs-source/game/mod_trok/cfg/movimento.cfg`
- `cs-source/refs/neotokyo-rebuild/`, `cs-source/refs/zmr-game/`, `cs-source/refs/double-action/`

Web:
- [Valve Developer Wiki: NextBot](https://developer.valvesoftware.com/wiki/NextBot) · [Implementing NextBot into your mod](https://developer.valvesoftware.com/wiki/Implementing_NextBot_into_your_mod) · [Navigation Mesh Editing](https://developer.valvesoftware.com/wiki/Nav_edit) · [Atributos de nav do L4D](https://developer.valvesoftware.com/wiki/Navigation_Meshes_(L4D)) · [NAV_MESH enums (Garry's Mod)](https://wiki.facepunch.com/gmod/Enums/NAV_MESH)
- [Booth, *The AI Systems of Left 4 Dead* (2009)](https://steamcdn-a.akamaihd.net/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf) · [Booth, *The Official Counter-Strike Bot* (GDC 2004)](https://www.cs.rit.edu/~jdb/tmp/making_of_official.pdf)
- [CS:GO Bot Behavior Trees (wiki)](https://developer.valvesoftware.com/wiki/Mp_bot_ai_bt) · [botprofile.db comentado (gist, comunidade)](https://gist.github.com/Jezternz/5a99a343914fcc3418b5bf5b08e882b8)
- [van Waveren, *The Quake III Arena Bot* (tese)](http://www.kbs.twi.tudelft.nl/docs/MSc/2001/Waveren_Jean-Paul_van/thesis.pdf) · [chars.h (ioquake3)](https://sources.debian.org/src/ioquake3/1.36%2Bsvn2287-1/code/game/chars.h) · [Características de bot do Q3 (manual)](https://www.icculus.org/gtkradiant/documentation/bot_manual/characteristics.html) · [aasfile.h (OpenArena)](https://sources.debian.org/src/openarena/0.8.8%2Bdfsg-2/code/botlib/aasfile.h) · [Fabien Sanglard: IA do Quake 3](https://fabiensanglard.net/quake3/a.i.php)
- [Recast Navigation](https://github.com/recastnavigation/recastnavigation) · [dtOffMeshLinkCreateParams (docs da Epic)](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Navmesh/dtOffMeshLinkCreateParams) · [DetourNavMesh.h](https://sources.debian.org/src/recastnavigation/1.6.0%2Bdfsg-3/Detour/Include/DetourNavMesh.h) · [Unity: Off-Mesh Links automáticos](https://docs.unity3d.com/ja/2022.1/Manual/nav-BuildingOffMeshLinksAutomatically.html)
- [Xonotic: jump waypoints (MR 670)](https://gitlab.com/xonotic/xonotic-data.pk3dir/merge_requests/670) · [Unvanquished: Bot design](https://wiki.unvanquished.net/wiki/Bot_design)
- [NavBot](https://github.com/caxanga334/NavBot) · [RCBot2](https://github.com/APGRoboCop/rcbot2) · [Left 4 Bots 2](https://github.com/smilz0/left4bots)
- [Orkin, *Three States and a Plan: The AI of F.E.A.R.* (GDC 2006)](https://pages.cs.wisc.edu/~dyer/cs540/handouts/gdc2006_orkin_jeff_fear.pdf) · [Verweij, *A Hierarchically-Layered Multiplayer Bot System* (Killzone 2)](https://www.guerrilla-games.com/read/a-hierarchically-layered-multiplayer-bot-system-for-a-first-person-shooter) · [Guerrilla: HTN planning in Decima](https://www.guerrilla-games.com/read/htn-planning-in-decima) · [Straatman et al., *Killzone's AI*](http://cse.unl.edu/~choueiry/Documents/straatman_remco_killzone_ai.pdf)
- [Dave Mark: Infinite Axis Utility System](https://gameai.com/iaus.php) · [Utility system (Wikipedia)](https://en.wikipedia.org/wiki/Utility_system) · [Mark, *Modular Tactical Influence Maps* (Game AI Pro 2)](https://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter30_Modular_Tactical_Influence_Maps.pdf)
- [Claypool et al., tempo de reação em jogos (WPI)](https://web.cs.wpi.edu/~claypool/papers/reaction-time/paper.pdf) · [parâmetros ex-gaussianos](https://pmc.ncbi.nlm.nih.gov/articles/PMC4915076/) · [Boudaoud et al., mira em FPS e lei de Fitts](https://arxiv.org/abs/2203.12050) · [Frontiers 2022: cinemática da mira em tarefas do Aim Lab](https://www.frontiersin.org/journals/human-neuroscience/articles/10.3389/fnhum.2022.979293/pdf) · [submovimentos em FPS (arXiv 2604.20673)](https://arxiv.org/html/2604.20673) · [Roberts et al.: undershoot e correções](https://hira.hope.ac.uk/1882/1/Fitts_violation_Roberts%2C%20Blinch%20et%20al.%2C%202016%20-%20Copy.doc) · [Chen et al.: estrutura dos movimentos de mouse](https://researchportal.hkust.edu.hk/en/publications/structure-of-handmouse-movements/)
- [Schrum, Karpov, Miikkulainen: UT^2, bot humano no BotPrize](https://www.cs.utexas.edu/~ai-lab/pubs/schrum.cig11competition.pdf)

Vários PDFs não abriram deste ambiente (rede bloqueada). Detalhes vieram de trechos indexados e do
código no repo; números de artigos marcados "a conferir" devem ser checados no original.

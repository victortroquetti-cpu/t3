# Bots no parkour: perseguir pulando e errar como gente

Resultado de um protótipo no navegador (`bots.html`, feito a partir do laboratório de movimento):
um bot persegue o jogador entre caixas usando **a mesma função de movimento** do jogador (lei do
Combat Arms: rampa 500 → 3000, pulo 330, gravidade 1100, fator de pulo 0,3 → 1 em 0,58 s, agachar
em 0,15 s). Aqui fica o que funcionou e como levar para o NextBot.

## Regra de ouro

- O bot só **aperta teclas** (frente, trás, lados, pulo, agachar, sprint, caminhar) e **gira o yaw**.
  Nada de teletransporte nem física especial.
- Os **mesmos controles** rodam na simulação (para gerar as ligações) e no bot de verdade. Se o
  controle mudar, as ligações precisam ser geradas de novo.
- Prova: cada uma das 376 ligações de pulo, executada sozinha pelo bot "Perfeito", deu certo
  376 vezes de 376.

## 1. Grafo

- **Nós:** o chão é dividido em células de 24 u e cada topo de caixa em células de ~32 u. Célula
  do chão a menos de 17 u de uma caixa fica de fora.
- **Ligações:**
  - **andar:** vizinhas na mesma superfície (8 direções; diagonal só se as duas retas existirem);
  - **degrau:** chão → caixa de até 30 u, simulando a caminhada;
  - **queda:** de cada célula da borda, andar para fora e ver onde pousa;
  - **pulo:** só existe se a simulação mostrar que dá.
- Protótipo: 1.604 nós e 11.404 ligações (376 pulos, 186 quedas, 18 degraus). Gerar levou de 2
  a 3,5 s, em fatias de 30 ms para a página não travar.

## 2. Gerar uma ligação de pulo por simulação

1. **Candidatos:**
   - chão → caixa: para cada lado da caixa e cada célula encostada nele, corrida perpendicular
     de 100 u (correndo) e de 170 u (sprint);
   - caixa → caixa: para cada célula do destino, o ponto mais próximo da borda da origem
     (subida de até 60 u, distância até 230 u). A corrida recua dentro da caixa até 4 u da borda
     (no máximo 160 u).
2. **Corrida uma vez:** parte parado em `R0`, olha para a direção `d` e segue a reta com frente
   (e sprint, se for o caso). Guarda o estado de cada tick até sair do chão ou bater na parede.
3. **Pulo em cada tick:** dos últimos 46 estados, simula apertar o pulo naquele tick e voar com o
   controle do ar. Sucesso = pousou na superfície certa e, 15 ticks parado, continua nela.
4. **Agachar no ar:** só se nenhum tick serviu e a subida passa de 36 u. Testa agachar 3, 6, …,
   24 ticks depois do pulo e fica com o que tem a maior janela.
5. **Janela:** a maior sequência de ticks seguidos que dão certo. O pulo ideal é o meio dela.
   Folga cedo e tarde = ticks até as pontas (× 15,15 ms). A folga do agachar é medida igual.
6. **Guardado na ligação:** `R0`, direção, ponto de pouso, posição do gatilho (distância ao longo
   da reta), velocidade no gatilho, tempo de corrida, sprint, tick do agachar, folgas e trajetória.
7. **Risco:** chance de errar de um humano médio (desvio de 35 ms, curva normal):
   `risco = 1 − P(acertar o pulo) × P(acertar o agachar)`.

O que a física do CA ensinou:

- Agachar no ar com mais da metade do agachamento **corta a velocidade para 40 u/s**. Por isso
  o agachar vem tarde (chão → caixa 48: ~18 ticks, 0,27 s depois do pulo).
- Do chão o bot sobe até 48 u (com agachar). Caixa de 56 u ou mais só a partir de outra caixa.
- O fator de pulo cai para 0,3 ao pular. Antes de uma ligação de pulo o bot espera
  `0,6 s − tempo de corrida` desde o último pulo.

## 3. A*

- Custo: distância + extra por tipo:
  - pulo: `35 + risco × 260`;
  - queda: 12;
  - degrau: 6.
- Memória: com `falhas ≥ N`, soma `500 × (falhas − N + 1)`.
- Heurística: distância horizontal.
- Se o jogador está fora de alcance, vai ao nó mais perto dele.
- Replaneja a cada 0,5 s ou quando percebe que o jogador mudou de célula. Não replaneja no meio
  de um pulo (correndo, no ar, hesitando ou descendo).

## 4. Executar o pulo

| Fase | O que faz |
|---|---|
| ir | Anda até `R0`; caminha (devagar) nos últimos 36 u. Se o alvo está a mais de ~50° do rosto, solta a frente e gira primeiro (com giro baixo ele ficava rodando em volta do ponto). |
| alinhar | Para, gira até a direção da ligação (±2°), espera a velocidade cair e o fator de pulo voltar. Sorteia os erros desta tentativa. |
| correr | Frente (+ sprint) seguindo a reta, mirando 48 u à frente. Calcula `τ = (gatilho − posição) / velocidade` e pula quando `τ ≤ meio tick − erro`. |
| ar | Mira o ponto de pouso. Fora da caixa: segura frente. Sobre a caixa: casa a velocidade com `(pouso − posição) / tempo até pousar` (frente ou trás). Corrige de lado com A/D. Agacha no tick guardado. |
| firmar | Depois de pousar, solta tudo até a velocidade cair abaixo de 35 u/s (no máximo 12 ticks). Sem isso o bot caía do muro de 32 u de largura. |
| hesitar | Freia por 0,12 s antes da borda, espera de 0,4 a 1 s e volta para `R0`. |

- **Pousou na superfície certa:** acerto, segue o caminho.
- **Pousou em outra:** queda; soma uma falha na ligação e replaneja dali ("volta e tenta de novo").
- **Passou da borda sem pular:** conta como pulo tentado e queda.

## 5. Modelo de erro humano

Sorteado uma vez por tentativa:

- **pulo:** `viés + desvio × normal()` em ms (negativo = cedo);
- **agachar:** `desvio × normal()`, arredondado em ticks;
- **mira na corrida:** `desvio × normal()` em graus, somado ao yaw;
- **no ar:** fica `erro de mira × (1 − correção)`. A/D só corrige erro lateral acima de
  `12 + (100 − correção) × 3` u/s;
- **hesitar:** sorteio com a chance do nível.

Também:

- **reação:** o bot só "vê" que o jogador mudou de célula depois do tempo de reação;
- **giro:** limite de graus por segundo.

## 6. Valores por nível

| | Fácil | Médio | Difícil | Perfeito |
|---|---|---|---|---|
| Reação | 600 ms | 350 ms | 200 ms | 0 |
| Erro no pulo (desvio) | 70 ms | 40 ms | 20 ms | 0 |
| Tendência do pulo | +20 ms | +10 ms | 0 | 0 |
| Erro de mira | 6° | 3° | 1,5° | 0 |
| Correção no ar | 35% | 65% | 85% | 100% |
| Hesitar na borda | 20% | 10% | 4% | 0 |
| Falhas até evitar (N) | 3 | 2 | 2 | 1 |
| Giro | 360°/s | 540°/s | 900°/s | 1440°/s |

Resultados com esses valores:

- **Cada pulo sozinho (376 ligações):**
  - Perfeito: 100%;
  - Difícil: 98%;
  - Médio: 93%;
  - Fácil: 87%.
- **Perseguição de 300 s, jogador parado sorteado a cada pegada (2 sementes):**

| | Pulos certos | Tempo médio até pegar | Pior caso |
|---|---|---|---|
| Perfeito | 100% | 3,7–3,8 s | 7 s |
| Difícil | 97–99% | 3,8–3,9 s | 10 s |
| Médio | 92% | 4,7–5,0 s | 20 s |
| Fácil | 78–85% | 6,1–7,2 s | 35 s |

- No Médio e no Fácil, a memória chegou a evitar 1 ligação por partida.
- Jogador parado no topo de 96 u (o ponto mais alto alcançável): o Perfeito sai do canto do mapa
  e pega em 6,8 s, com 4 pulos.

## 7. Como portar para o NextBot

### Onde entra

- **Locomoção:** `CTrokBotLocomotion` (em `src/game/server/trok/bot/trok_bot_body.cpp`), filha de
  `CHL2MPBotLocomotion` → `PlayerLocomotion` → `ILocomotion`
  (`sdk/src/game/server/NextBot/NextBotLocomotionInterface.h`). Hoje ela só segura o agachar no
  `ClimbUpToLedge`.
- **Caminho:** `PathFollower` (`NextBot/Path/NextBotPathFollow.cpp`). Hoje ele marca os trechos
  `CLIMB_UP` e `JUMP_OVER_GAP` pela altura e pelo vão e chama `ClimbUpToLedge` ou `JumpAcrossGap`
  perto da borda. Não sabe de corrida, gatilho nem agachar.

### Gerar as ligações de pulo sobre o nav mesh

- Depois do `nav_generate` ou no carregamento do mapa: para cada par de `CNavArea` com subida de
  até 60 u, vão de até 230 u e sem ligação a pé, montar os candidatos (bordas que se olham, como
  no item 2).
- **Simulador:** uma função que reproduz a lei do `trok_gamemovement` (mesmas cvars `trok_mov_*`
  e `trok_pulo_*`) com `UTIL_TraceHull` do hull do jogador, tick a tick, sem entidade. Precisa dar
  o mesmo resultado que o jogo: conferir com `cl_showerror` e com o laboratório.
- Ligação que deu certo:
  - `CNavArea::ConnectTo` se ainda não existir;
  - dados do pulo numa tabela à parte, por par de áreas, salva em
    `maps/<mapa>.trokpulos` para não simular de novo (nav do Source não guarda isso).
- O que o gerador do Source não liga hoje se resolve aqui, sem `nav_connect` à mão.

### Usar no caminho e executar

- **Custo:** no functor de custo do `Path::Compute` dos bots, somar `pulo + risco × custo_risco`
  e a memória de falhas quando a ligação está na tabela.
- **Executar:** uma `Action` nova (ex.: `CTrokBotParkourPulo`), empurrada pela ação principal
  quando o próximo trecho do caminho é uma ligação da tabela.
  - Ela suspende o `PathFollower` e faz ir → alinhar → correr → ar → firmar.
  - Teclas: `PressForwardButton`, `PressBackwardButton`, `PressLeftButton`, `PressRightButton`,
    `PressJumpButton` e `PressCrouchButton` do `NextBotPlayer`. Não existe botão de sprint: falta
    um `IN_SPEED` no bot (arquivo da Valve, então com `#ifdef TROK`).
  - Olhar: `AimHeadTowards` do corpo, respeitando o giro do perfil.
  - No `ClimbUpToLedge` / `JumpAcrossGap` da locomoção: não pular sozinho quando a ligação é da
    tabela (a ação decide o tick).
- **Pouso:** `OnLandOnGround` confere a área. Área errada soma falha e replaneja.
- **Memória:** falhas por bot e por ligação; sugestão de esquecer depois de alguns minutos ou ao
  trocar de mapa.

### Cvars sugeridas (números fora do código)

| Cvar | Padrão | Para quê |
|---|---|---|
| `trok_bot_parkour` | 1 | liga o sistema |
| `trok_bot_parkour_gerar` | (comando) | gera e salva as ligações do mapa |
| `trok_bot_parkour_debug` | 0 | desenha ligações por risco, `R0`, gatilho e pouso |
| `trok_bot_parkour_subida_max` | 60 | maior subida testada (u) |
| `trok_bot_parkour_dist_max` | 230 | maior distância testada (u) |
| `trok_bot_parkour_custo_pulo` | 35 | extra de custo por pulo |
| `trok_bot_parkour_custo_risco` | 260 | multiplica o risco |
| `trok_bot_parkour_custo_falha` | 500 | extra por falha depois de N |
| `trok_bot_parkour_sigma_ref` | 35 | desvio (ms) usado para dar nota de risco |
| `trok_bot_parkour_replanejar` | 0.5 | intervalo de replanejamento (s) |
| `trok_bot_parkour_firmar_vel` | 35 | velocidade para soltar o "firmar" (u/s) |

- Os valores por nível (reação, erro do pulo, tendência, mira, correção no ar, hesitar, N) vão
  em `scripts/trok_bot_perfis.txt`, junto dos outros números do perfil.
- A reação aqui é para "o jogador mudou de lugar". A percepção de inimigo continua a do
  `CTrokBotVision`.

## Pontos em aberto

- O protótipo só tem caixas retas. Mapas de verdade têm rampas, escadas e beiradas finas: o
  simulador com `TraceHull` cobre isso, mas precisa ser testado.
- Sem colisão entre jogadores e sem tiro: no jogo, pular sob fogo deve somar erro.
- Gerar no Source pode demorar com muitas áreas. Limitar pares por distância e salvar em arquivo.

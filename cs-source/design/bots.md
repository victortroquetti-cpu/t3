# Bots do Trok: IA realista e equilibrada

Primeiro objetivo do mod: bots que parecem jogadores. Equilibrados, com percepção
realista (nem enxergam tudo, nem deixam passar tudo), mira sem cara de aimbot,
movimento fluido (inclusive pulos sobre caixas e pilastras), níveis do "burrinho"
ao "pro" e noção de lugar estratégico.

## O que se sabe dos bots do CS (pesquisa)

O código dos bots do CS:S não é público. O que dá para estudar são palestras,
documentação e o arquivo de perfis que o jogo publica.

- **Projeto do Michael Booth (Turtle Rock/Valve, GDC 2004).** O bot só reage ao que
  um humano perceberia na mesma situação: cone de visão, tempo de reação e atenção.
  Os erros (olhar o canto errado, por exemplo) surgem desse limite; não são roteirizados.
  - A cabeça é um sistema mola-amortecedor que fica mais rígido em combate.
  - O desvio da mira é sorteado de tempos em tempos e diminui com a habilidade.
  - O mapa é analisado antes (`nav_analyze`): hiding spots, sniper spots, encounter spots, áreas de aproximação.
- **Perfis (`botprofile.db`).** Por nível: Skill, Aggression, ReactionTime, AttackDelay,
  AimFocusInitial/Decay/OffsetScale/Interval e a mola da cabeça. Não há documentação
  oficial dos campos, só cópias comentadas pela comunidade, que discordam entre si.
- **Outros jogos.**
  - Percepção por acumulador: The Last of Us, Splinter Cell Blacklist, CryEngine. A distância pesa mais que tudo; a periferia é fraca de longe; agachado e parado é mais difícil de ver; a percepção sobe mais rápido em combate.
  - Avaliação de posição por soma ponderada: Killzone.
  - Percepção e caminho no NextBot: Left 4 Dead.
  - Aleatoriedade ponderada para não ficar previsível: utility AI.
- **Fatores humanos.**
  - Tempo de reação: jogador médio ~0,30 s, bom ~0,25 s, elite 0,15–0,20 s. A distribuição é ex-gaussiana (normal com cauda para a direita).
  - O erro de rastreio cresce com a velocidade angular do alvo.
- **Regra de equilíbrio comum a todos.** A dificuldade muda reação, mira, giro, atenção e
  tática. **Nunca** muda informação (ver através de parede, saber posição pelo som) nem dano.

O SDK 2013 oficial já tem boa parte da base:
- o NextBot, com cabeça com sacadas e "mousepad virtual";
- o nav mesh com luz por canto, visibilidade entre áreas, hiding spots e encounter spots;
- os bots do HL2MP.

O Trok troca as peças que deixavam esses bots robóticos.

## Como o Trok faz

Código em `src/game/server/trok/bot/`. O patch troca as peças do `CHL2MPBot` com `#ifdef TROK`.

| Pedido | Peça | Como |
|---|---|---|
| Ver de forma realista | `CTrokBotVision` | Barra de percepção por inimigo (detalhes abaixo). |
| Reação humana | `CTrokBotVision::GetMinRecognizeTime` | Ex-gaussiana por nível, mínimo de 0,15 s. |
| Mira sem cara de hack | `CTrokBotMainAction::SelectTargetPoint` | Erro inicial maior quanto maior a virada; cai pela metade a cada meia-vida, mas nunca a zero. Erro extra com alvo rápido, correndo e ao levar dano. Peito ou cabeça sorteado por engajamento (cabeça fica rara de longe). O desvio passeia devagar, sem tremer. Pausa para trocar de alvo depois de um abate. |
| Giro natural | `CTrokBotBody` | Velocidade máxima de giro e intervalo de rastreio por nível. O NextBot já suaviza a chegada e as "recentradas" do mouse. |
| Movimento fluido em combate | `CTrokBotMainAction::Dodge` | Strafe em lados de duração variável, pausas curtas para atirar parado e agachadas ocasionais. Não cai de borda nem empurra parede. |
| Parkour (caixas, pilastras) | `CTrokBotLocomotion` | Pulo agachado: segura o agachar no ar ao subir bordas acima de ~40u (o pulo em pé não alcança). O caminho já usa os links de pulo do nav. |
| Ouvir | `CTrokBotMainAction::ListenForFootsteps` | Passos de quem corre (~1100u) ou anda rápido (~600u); parede abafa. O bot vira para a direção do barulho com erro e a percepção sobe, mas sem passar do limite: ouvir não é ver. Tiro alerta na hora (já era assim no HL2MP). |
| Lugares estratégicos | `CTrokTacticalMap` | Detalhes abaixo. |
| Níveis e personalidade | `trok_bot_profile.*` + `scripts/trok_bot_perfis.txt` | Fácil, normal, difícil e expert (`hl2mp_bot_difficulty 0..3`). Cada bot sorteia até 12% de variação e a própria agressividade. |
| Segurar ou caçar | `CTrokBotBrain` + `CTrokBotHoldSpot` | O "cérebro" alterna entre segurar um lugar bom (vigiando as entradas) e caçar. Pouca agressividade com bom uso tático segura mais. |

### Percepção em números

A barra enche por segundo:

`6 × ganho_do_nível × ângulo × distância × exposição × movimento × luz × alerta × acaso`

| Fator | Valor |
|---|---|
| Ângulo | 1 no centro (±10°), caindo até 0,15 na borda do campo do nível (100° no fácil, 130° no expert) |
| Distância | 1 até 256u; ~0,6 a 1000u; ~0,3 a 2000u |
| Exposição | Traços para cinco pontos do corpo (cabeça, peito, quadril, ombros). Só a cabeça à vista atrás de uma caixa vale bem menos |
| Movimento | Correndo ×1,35; andando ×1; parado ×0,6. Agachado ×0,75; no ar ×1,2 |
| Luz | Luz do nav (`nav_analyze`): escuro ×0,35 |
| Alerta | ×1,4 depois de ouvir tiro ou passos |
| Acaso | Sorteio entre 0,7 e 1,3 a cada ~0,5 s |

- **Esvaziamento:** 0,35 por segundo fora de vista. Um inimigo que sumiu há menos de 1,5 s volta a ser visto na hora (o bot ainda o acompanhava).
- **Percepção instantânea:** clarão de tiro no campo de visão ou alguém a menos de 120u.
- **Exemplos de tempo para perceber, antes da reação:**
  - correndo de frente, a 300u: ~0,1–0,2 s;
  - andando a 45°, a 1000u, metade escondido: ~1 s;
  - agachado e parado no escuro, na periferia, a 2000u: praticamente nunca.

### Mapa tático (lugares estratégicos)

Calculado do nav mesh na primeira vez que um bot precisa, uma vez por mapa:

1. **Regiões.** Áreas ligadas a pé viram uma região (sala, corredor, topo de caixa, pilastra).
2. **Entradas.** Cada ligação de fora para dentro vira entrada a pé, por pulo para cima ou por queda.
3. **Custo de acesso.** Menor número de pulos desde a maior região do mapa. Um laboratório que só se alcança pulando três pilastras quebradas tem custo 3 e só entradas por pulo, por isso ganha nota alta de defesa.
4. **Nota de cada área.** Defesa da região 45%, pouca exposição 25%, altura sobre o entorno 12%, cobertura 8%, visão das entradas 10%. Exposição e visão das entradas usam a visibilidade do nav.
5. **Escolha.** O bot escolhe pela nota, misturada com acaso conforme o `uso_tatico` (o fácil escolhe quase ao acaso). Dois bots não ficam na mesma área.

Os mapas precisam de `.nav`. **Rode `nav_analyze`** depois de `nav_generate`: sem ele não há
visibilidade, luz nem hiding spots, e a nota fica só pela estrutura (entradas e acesso).
Mapas com parkour precisam de links de pulo entre as caixas e pilastras: confira com
`nav_edit 1` e ligue à mão (`nav_connect`) onde o gerador não ligou.

## Estilos de jogo e trabalho em equipe

Na vida real cada um joga de um jeito. Cada bot sorteia um **estilo**, com os papéis do CS
competitivo. O estilo muda **decisões**, não a habilidade: mira e reação continuam vindo do nível.

| Estilo | Peso padrão | Como joga |
|---|---|---|
| **Equilibrado** | 30 | Alterna entre segurar e caçar pela personalidade. |
| **Entry** | 20 | Agressivo (0,85–1). Rota direta, quase nunca segura, corre para a luta dos colegas, não anda devagar. |
| **Lurker** | 15 | Rotas longe das do time (custo da rota dos colegas ×3). Indo para uma pista, mira um ponto 450–750u ao lado do inimigo, para chegar pelo flanco. |
| **Âncora** | 20 | Segura lugares cobertos e pouco expostos, 2,2× mais tempo, agachado e sem largar o posto. **Emboscada**: mira e espera, e só atira quando o inimigo chega perto (700u), vira de costas, já está à vista há 2,5 s ou quando leva tiro. |
| **Suporte** | 15 | Anda 220–420u atrás de um colega (de preferência um jogador humano; no máximo 2 por líder), cobrindo o ângulo que ele não cobre. Quando o colega entra em luta, vai trocar o abate (trade). |

- Pesos: `trok_bot_estilos "equilibrado 30 entry 20 lurker 15 ancora 20 suporte 15"`.
- Para testar um estilo: `trok_bot_estilo ancora` (vazio volta ao sorteio). Os dois valem para os próximos bots.
- `trok_bot_estilos_info` lista cada bot com nível, estilo, agressividade e se está segurando posição.

### O time como time

O módulo `trok_bot_team.*` guarda o que o time sabe e o que cada um está fazendo:

- **Avisos (o "call").** Quem vê um inimigo passa a posição para o time a cada segundo. Passos e tiros ouvidos viram pista com erro (200–250u), porque o som dá a região, não o ponto. No mata-mata sem times, cada bot só usa o que ele mesmo viu ou ouviu.
- **Apoio e trade.** Colega lutando a menos de 2200u: os outros vão ajudar, mas pela rota que ninguém do time usou, o que vira flanco naturalmente. No máximo 2 indo para a mesma luta.
- **Ângulos.** Segurando uma posição, o bot vigia as entradas da região e escolhe uma que **nenhum colega** está cuidando (cada bot "reivindica" o ângulo que olha). Andando em grupo, cada um olha para onde os outros não estão olhando: frente, flancos e, com 3 ou mais, costas.
- **Espalhar.** Dois bots nunca seguram a mesma área, e o time prefere segurar regiões diferentes. Destinos perto do destino de um colega (500u) valem menos.

### Por que eles não andam mais todos iguais

O "plástico" dos bots do HL2MP tinha causa concreta:
- todos usavam a rota mais curta, sem variação por bot;
- todos iam primeiro para a arma mais próxima;
- todos andavam pelo meio do caminho, em fila.

O que mudou:

- **Rota própria.** A cada caçada, o bot sorteia uma "preferência de corredor" (células de 512u mais ou menos caras para ele). Dois bots indo ao mesmo lugar pegam caminhos diferentes. Por cima disso, ele foge da rota que colegas usaram nos últimos 20 s e, se for cauteloso, de onde gente morreu há pouco (perigo do nav, que decai sozinho).
- **Destino variado.** Pistas do time mais 30 áreas sorteadas, longe dos destinos dos colegas e dos últimos 5 lugares que o bot visitou. A distância preferida depende da agressividade.
- **Ritmo próprio.**
  - Hesita 0,1–1,5 s ao nascer, olhando em volta, então não saem em trem.
  - Ao chegar no destino, para e confere: mais tempo os cautelosos, menos os entry.
  - Os cautelosos andam sem fazer barulho na reta final até uma pista.
- **Faixa no corredor.** Cada bot anda deslocado até ~36u do meio, numa faixa que oscila devagar e muda a cada 3–8 s. Só em área larga, sem pulo e sem beirada; com parede ou buraco, volta para o meio.

## Como testar

```
exec bots
```

`bots.cfg` liga cheats e teamplay e põe 8 bots no nível difícil. Para olhar um bot por dentro, mire nele, rode `nb_select` e depois:

- `nb_debug BEHAVIOR`: a pilha de ações (`TrokCerebro` > `SegurarPosicao` ou `SeekAndDestroy`).
- `trok_bot_debug_percepcao 1`: a barra de percepção dele sobre cada inimigo.
- `trok_bot_debug_mira 1`: o ponto de mira e o erro em graus.
- `trok_bot_tatico_mostrar 2000`: pinta o nav pela nota (verde = bom para segurar). `trok_bot_tatico_info` lista as regiões mais defensáveis.
- `trok_bot_mira_humana 0`, `trok_bot_audicao 0`, `trok_bot_tatica 0`, `trok_bot_pulo_agachado 0`, `trok_bot_faixa 0`, `trok_bot_rota_variedade 0`: liga e desliga cada parte, para comparar com o bot do HL2MP.
- Depois de editar `scripts/trok_bot_perfis.txt`: `trok_bot_perfis_recarregar`, `hl2mp_bot_kick all` e adicione os bots de novo.

Testes que medem "parece gente":

1. **Tempo até o primeiro acerto.** Apareça de frente a 500u e a 1500u, 20 vezes por nível. O expert deve ficar em torno de 0,3–0,5 s; o fácil, acima de 0,8 s.
2. **Esconderijo.** Agachado e parado atrás de uma caixa, só a cabeça para fora, a 1500u. O bot de passagem não deve te ver na maior parte das vezes. Correndo ou atirando, deve ver.
3. **Strafe.** O bot não deve "tremer" (trocar de lado a cada quadro). Lados de 0,3–1 s.
4. **Mesmo caminho.** Com `trok_bot_estilo equilibrado` e 6 bots num time, eles não devem sair do spawn em fila nem chegar todos pelo mesmo corredor. Compare com `trok_bot_rota_variedade 0`.
5. **Emboscada.** Com `trok_bot_estilo ancora`, passe por uma entrada que um bot âncora vigia, de frente e de longe: ele deve esperar (com a mira em você) até você chegar perto ou virar.
6. **Parkour.** Num mapa com caixas e pilastras, `trok_bot_tatico_mostrar` deve pintar de verde os lugares de difícil acesso, e bots com `uso_tatico` alto devem subir neles.
7. **Teste cego.** Alguém assiste a demo sem saber quem é bot e tenta adivinhar.

## O que ainda não foi verificado

O código passou na checagem de sintaxe e tipos (`trok.py check`) contra o SDK, mas **ainda não
foi compilado inteiro nem testado em jogo**. Os números são pontos de partida tirados da
pesquisa; espere ajuste depois dos primeiros testes.

## Próximos passos (quando forem pedidos)

- "Checar cantos" com os encounter spots do nav: olhar, em ordem, os pontos onde um inimigo pode aparecer ao cruzar uma área.
- Busca pela última posição conhecida: ir até lá e procurar nos esconderijos próximos, em vez de esquecer.
- Som pela distância no nav, não em linha reta (parede grossa abafa de verdade).
- Controle de recuo parcial por nível, rajadas de longe e ajustes de mira por arma.
- Rotações: quando o time perde um lugar ou ouve muita luta de um lado, parte do time gira para lá.
- Modo zumbi/quarentena: estilos e cérebro próprios para infectados (perseguição, cerco, quebrar barricada) e para humanos (trancar abrigos, defender juntos), em cima desta base.

## Fontes

- [Booth, *The Official Counter-Strike Bot*, GDC 2004 (slides)](https://www.cs.rit.edu/~jdb/tmp/making_of_official.pdf) · [sessão no GDC Vault](https://www.gdcvault.com/play/1022878/The-Making-of-the-Official)
- [Entrevista com o Booth sobre o bot do CS](https://gamebanana.com/threads/23298)
- [Booth, *The AI Systems of Left 4 Dead* (2009)](https://steamcdn-a.akamaihd.net/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf)
- [CS Wiki (fandom): níveis dos bots](https://counterstrike.fandom.com/wiki/Bot) · [botprofile.db comentado (comunidade)](https://gist.github.com/so0k/7505931)
- [Valve Developer Wiki: formato NAV (luz por canto)](https://developer.valvesoftware.com/wiki/NAV)
- [McIntosh, *Human Enemy AI in The Last of Us* (Game AI Pro 2)](https://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter34_Human_Enemy_AI_in_The_Last_of_Us.pdf)
- [Walsh, *Modeling Perception and Awareness in Splinter Cell Blacklist* (Game AI Pro 2)](https://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter28_Modeling_Perception_and_Awareness_in_Tom_Clancy's_Splinter_Cell_Blacklist.pdf)
- [CRYENGINE: AI Perception](https://docs.cryengine.com/display/CEMANUAL/AI+Perception)
- [Straatman et al., *Killzone's AI: dynamic procedural combat tactics*](http://cse.unl.edu/~choueiry/Documents/straatman_remco_killzone_ai.pdf) · [*Hierarchical AI for Multiplayer Bots in Killzone 3* (Game AI Pro)](https://www.gameaipro.com/GameAIPro/GameAIPro_Chapter29_Hierarchical_AI_for_Multiplayer_Bots_in_Killzone_3.pdf)
- [Ocio Barriales, *Using Your Combat AI Accuracy to Balance Difficulty* (Game AI Pro 3)](https://www.gameaipro.com/GameAIPro3/GameAIPro3_Chapter33_Using_Your_Combat_AI_Accuracy_to_Balance_Difficulty.pdf)
- [Mark, *Modular Tactical Influence Maps* (Game AI Pro 2)](https://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter30_Modular_Tactical_Influence_Maps.pdf)
- [Claypool et al., tempo de reação em jogos (WPI)](https://web.cs.wpi.edu/~claypool/papers/reaction-time/paper.pdf) · [parâmetros ex-gaussianos de tempo de reação](https://pmc.ncbi.nlm.nih.gov/articles/PMC4915076/)
- [Boudaoud et al., mira em primeira pessoa e lei de Fitts](https://arxiv.org/abs/2203.12050)
- [van Waveren, *The Quake III Arena Bot* (tese)](http://www.kbs.twi.tudelft.nl/docs/MSc/2001/Waveren_Jean-Paul_van/thesis.pdf)
- [UT^2 / BotPrize: bots que parecem humanos](https://www.cs.utexas.edu/~ai-lab/pubs/schrum.cig11competition.pdf)

Muitos desses PDFs não abriram direto deste ambiente (rede bloqueada): os detalhes vieram de
trechos indexados. Os números exatos devem ser conferidos nos documentos.

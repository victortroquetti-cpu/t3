# Pesquisa: HUD de jogo de tiro moderno (para o HUD do Rearmed)

Pedido do dono (10/11/2026): "o HUD in-game tem que ser muito aperfeiçoado, inspirado nos CoDs modernos,
sofisticado". Aqui fica só o que se aprende olhando os jogos (posições, hierarquia, sensação). Nada de
arte, ícone ou fonte deles entra no mod (regra 2 do `CLAUDE.md`). Prévia da proposta: `design/ui.md`, seção HUD.

## 1. O que cada jogo faz

### Call of Duty: Modern Warfare (2019)
- Linguagem "militar de alta precisão": traços finos, texto pequeno em caixa alta, quase nada de caixa
  cheia. O time de UI disse que o objetivo era um HUD mínimo que deixasse a cena 3D aparecer.
- No beta tiraram o minimapa e puseram uma **bússola no topo** (que também marcava tiros); os jogadores
  reclamaram e o minimapa voltou no terceiro dia. Lição: bússola é bonita, minimapa é necessário.
- Munição embaixo à direita: número do pente grande, reserva pequena ao lado, nome da arma acima,
  ícones de letal/tático com contagem ao lado.
- Lista de abates embaixo à esquerda, acima do chat. Placar e tempo no canto de cima à esquerda, junto
  do minimapa.
- **Sem barra de vida** no multijogador (só o vermelho na tela). Modo Realismo tira o HUD todo.
- Marcador de acerto em X branco; no abate o X fica vermelho e um pouco maior.

### MWII (2022) e MWIII (2023)
- Mesma base do MW2019, mais "tático" (cantos cortados, rótulos com espaçamento entre letras).
- A interface dos menus foi muito criticada (confusa, pesada); o HUD em jogo nem tanto. Lição: no HUD,
  menos camadas e menos caixas.
- Bússola no topo, minimapa em cima à esquerda com o placar logo acima; opção de minimapa quadrado
  ou redondo.
- Pontuação "+100" pequena perto da mira, empilhando, e medalhas em texto + ícone abaixo do centro.

### Black Ops Cold War (2020) e Black Ops 6 (2024)
- **Barra de vida com número** embaixo à esquerda (volta da vida visível).
- Pontos "+100 ABATE" à direita da mira, sobem e somem rápido; medalhas no meio-baixo.
- Série (scorestreak): ícones embaixo à direita com progresso.
- BO6: menu de HUD com **predefinições** (Clássico, Batedor, Imersivo, Transmissão...) e opções de
  minimapa (forma, girar ou não), bússola, marcador de acerto, tamanho e opacidade do texto. Os
  elementos não mudam de lugar, só de visibilidade e tamanho.
- Aviso de granada: ícone ao redor da mira com uma seta apontando para onde ela está.
- Prompt de interação: tecla dentro de um quadradinho + verbo ("[F] PEGAR").

### Battlefield 2042 e Battlefield 6 (contraponto)
- Bússola fixa no topo (no lançamento do 2042 ela só aparecia mirando; depois ficou sempre visível).
- Minimapa embaixo à esquerda, vida e esquadrão juntos; munição embaixo à direita, número grande.
- Opções de escala, opacidade e cor do HUD; zoom do minimapa.
- Lição: o topo central é o lugar natural da bússola e do placar dos objetivos.

### The Finals (contraponto de personalidade)
- HUD de "programa de TV": cores fortes, tipografia grande, placar de dinheiro no topo.
- Mostra que uma identidade forte vem da **tipografia e do ritmo das animações**, não de molduras.
  O Rearmed fica no sóbrio, mas pode ter a mesma firmeza tipográfica.

## 2. Posições (consenso dos jogos)

| Elemento | Onde | Observação |
|---|---|---|
| Minimapa / radar | em cima à esquerda (CoD) | quadrado, gira com a visão; placar perto |
| Bússola | topo central | marcas a cada 15°, letras a cada 45°, número do rumo no meio |
| Placar e tempo | topo (central no BF, esquerda no CoD) | seu lado sempre à esquerda/azul |
| Lista de abates | canto de cima à direita ou embaixo à esquerda | só texto, sem caixa pesada |
| Vida / colete | embaixo à esquerda | número + barra fina; colete numa segunda barra |
| Arma / munição | embaixo à direita | pente grande, reserva pequena, nome, modo, letal/tático |
| Mira | centro | abre com a dispersão, some mirando pelo cano |
| Acerto / dano | ao redor da mira | X no acerto; arcos vermelhos da direção do dano |
| Pontos / medalhas | à direita da mira / abaixo do centro | sobem e somem em ~1 s |
| Aviso de granada | anel ao redor da mira | ícone + seta para a granada |
| Interação | abaixo da mira | [tecla] + verbo + objeto |
| Série de abates | faixa no alto do centro (CA) / ícones embaixo à direita (CoD) | o Rearmed já tem a faixa do CA |
| Objetivo / modo | topo, embaixo do placar | uma frase só, cor de destaque quando muda |

## 3. Tipografia, cor e opacidade
- Uma família condensada para números e rótulos (o Rearmed usa Barlow Condensed) e uma normal para
  nomes. Rótulos pequenos em caixa alta com espaçamento entre letras (10-15%).
- **Números com largura fixa** (algarismos "tabulares"): o pente 30 → 29 → 28 não pode "pular".
  O VGUI não liga o recurso da fonte, então o código desenha cada algarismo numa célula fixa.
- Três níveis de texto: valor (branco), rótulo (cinza 60%), apagado (cinza 40%). Uma cor de destaque
  só (âmbar) para "algo mudou"; vermelho só para perigo; azul/vermelho só para lados.
- Legibilidade sem caixa: sombra suave no texto ou um degradê escuro bem leve atrás, que some nas
  bordas. Caixa cheia escura só onde há muito texto (tela de morte).
- Opacidade de repouso 85-100%; o que não está sendo usado pode apagar até ~50% (CoD faz isso com a
  munição cheia e parada no modo imersivo).

## 4. Animação
- Entrada rápida (120-200 ms, desacelerando no fim), saída mais lenta (300-500 ms) só de transparência.
- Escala só em eventos (abate, medalha): começa 110-130% e assenta. Nada de balançar o HUD fixo.
- Pulso só para estado crítico (vida baixa, sem munição, alarme), 1-2 Hz, variando a transparência.
- Números que mudam muito (pontos) podem "contar" rápido; vida cai de uma vez e a parte perdida da
  barra fica um instante clara antes de sumir ("rastro de dano").

## 5. Escala e opções
- Margem segura de ~3-4% da altura em todos os lados; tudo escalado pela altura da tela (16:9 e 16:10
  iguais, ultrawide só afasta os cantos).
- Opções que todo jogo moderno tem: tamanho do HUD, opacidade, minimapa (gira ou não), bússola,
  marcador de acerto, mira, e uma predefinição "minimalista" (só vida, munição e mira; o resto aparece
  quando muda).

## 6. O que deixa "sofisticado"
- **Alinhamento**: tudo preso a poucas linhas (margem esquerda, direita, linha de base de baixo).
- **Respiro**: poucos elementos, espaço entre grupos maior que dentro do grupo.
- **Monoespaço nos números** e rótulos pequenos com espaçamento.
- **Traço fino** (1-2 px) em vez de moldura; cantos retos.
- **Hierarquia**: um número grande por canto (vida, pente); o resto pequeno.
- **Contexto**: mostrar o que importa agora (fôlego só quando gasta, colete só se tiver, aviso de
  recarga só com pente baixo).
- **Movimento contido**: o HUD fixo nunca se mexe; só os eventos animam.

## Referências
- Bússola no lugar do minimapa no MW2019: https://mp1st.com/news/call-of-duty-modern-warfare-compass-confirmed-will-replace-the-mini-map
- Minimapa volta no beta: https://www.gamerevolution.com/?p=596775
- Opções de minimapa quadrado/redondo no MW: https://www.thegamer.com/modern-warfare-players-choose-square-circle-minimap/
- Controles e HUD do MW2019 (Activision): https://blog.activision.com/call-of-duty/2019-10/Getting-Started-in-Modern-Warfare-Controls-and-Settings-PC
- Crítica da UI do MWII e redesenhos de fãs: https://www.gamesradar.com/modern-warfare-2s-ui-is-so-bad-players-are-fixing-it-themselves , https://charlieintel.com/modern-warfare-2-player-redesigned-games-ui-and-its-perfect/208774
- HUD do MW2 antigo sobre o MW2019: https://dexerto.com/call-of-duty/modern-warfare-with-the-hud-from-mw2-looks-absolutely-stunning-1308273
- Personalização do HUD no BO6: https://www.pcgamesn.com/call-of-duty-black-ops-6/hud , https://esports.gg/guides/call-of-duty/how-to-change-your-hud-in-black-ops-6 , https://charlieintel.com/call-of-duty/black-ops-6-best-hud-preset-337026
- BF2042 no lançamento (bússola sempre visível): https://www.pcgamesn.com/battlefield-2042/launch-content
- Acessibilidade de tela do BF2042 (EA): https://www.ea.com/able/resources/battlefield-2042/ps4/display
- Bancos de capturas de interface para consulta: https://interfaceingame.com , https://www.gameuidatabase.com

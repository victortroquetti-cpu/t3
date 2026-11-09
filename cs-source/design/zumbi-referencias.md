# Modo zumbi: referências de gênero

Linha do tempo que o dono passou (09/11/2026), de um vídeo com a história dos modos zumbi
no CS e nos jogos em volta dele. Serve para escolher que tipo de modo zumbi o Trok vai ter.
**Só referência de mecânica:** nada de nomes, mapas, modelos, sons ou código desses mods ou
jogos entra no Trok.

| Época | Mods / jogos citados |
|---|---|
| 1998–2004 | Half-Life ZNPC, CS 1.5 Biohazard |
| 2006–2014 | CS 1.6 ZM Plague (várias versões até Advance), ZM Base Builder |
| 2007–hoje | CS 1.6 ZM Escape |
| 2008–2015 | CS:S ZM Riot, ZM Extermination, ZM Plague, ZM Escape |
| 2012–2017 | CS Online NST, CS Xtreme v4–v7 |
| 2013–2023 | CS:GO ZM Survival, CS Online 2 Zombie Z |
| 2014–hoje | CS Nexon ZM Survival e ZM Escape |
| 2023–hoje | remakes em UE5, CS2 ZM Survival / Escape / Plague |

## Os subgêneros que aparecem nessa linha

| Subgênero | Como funciona (em geral) | O que pede do Trok |
|---|---|---|
| **Infecção (estilo "Plague")** | Zumbis iniciais sorteados; humano atingido vira zumbi; humanos sobrevivem até o fim do tempo. Variantes com classes de zumbi com habilidade, rodadas especiais (um zumbi muito forte contra todos, ou um humano muito forte contra todos), loja de itens. | Regras de rodada e times (gamerules), classes com habilidades, coice de arma, HUD de rodada. Bots: humanos que defendem juntos, zumbis que cercam. |
| **Escape** | Humanos correm por um mapa linear com portas, elevadores e eventos com tempo até o ponto de fuga; zumbis perseguem. Depende muito de mapa com eventos (gatilhos, portas, contagens). | Entidades de mapa com inputs/outputs e contagens (já existem no Source), mapas próprios no Hammer. Bots precisam seguir a "rota de fuga" (marcação no nav ou nos gatilhos). |
| **Base Builder** | Fase de construção: humanos arrastam blocos para montar a base. Depois os zumbis atacam. | Sistema de construção (pegar e prender props), fase de rodada, blocos com vida. |
| **Survival / Riot / Extermination** | PvE: ondas de zumbis controlados pela IA contra um time de humanos, com loja e melhorias entre as ondas. | NPCs NextBot em massa (como os zumbis do TF2 e os do Zombie Master: Reborn em `refs/`), gerador de ondas, economia. |
| **Comandante de horda** (o Zombie Z do CS Online 2, herdeiro do Zombie Master) | Um jogador comanda a horda de zumbis de cima, como num RTS, contra os humanos. | Visão de comandante, seleção e ordens para os NPCs. Referência legal e próxima: `refs/zmr-game`. |
| **Estilo CS Online / Xtreme** | Infecção com muito conteúdo: classes com habilidade e barra de energia, caixas de suprimento caindo, herói no fim da rodada, evolução do zumbi por infecções. | Tudo da infecção, mais sistema de habilidades, drops e progressão na rodada. |

## Como isso conversa com o que já existe

- **Bots:** os estilos e a memória do time já servem para os humanos (âncora segura o abrigo,
  suporte cobre, aviso de onde o zumbi está). Os zumbis pedem um cérebro próprio: perseguição,
  cerco por rotas diferentes (a variedade de rota já faz isso), quebrar barricada.
- **Mapa tático:** já acha lugares de defesa (poucas entradas, acesso por pulo). Num modo
  infecção, os humanos usam isso para escolher onde se defender, e os zumbis para saber por
  onde entrar.
- **Ideias do dono já registradas:** o relógio com alarme e a abertura de abrigos e a grade
  trancável que os zumbis quebram (estilo quarentena) encaixam em **Infecção** e em **Escape**.

## Decisão pendente (do dono)

Qual subgênero vem primeiro. Sugestão técnica: **Infecção**, porque é a base dos outros (rodada,
times, conversão, classes). Escape e Survival reaproveitam quase tudo dela.

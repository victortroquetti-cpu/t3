# Combat Arms: design, sensação e UX (lições para o Rearmed)

Pesquisa de 10/11/2026, só por busca na web: imprensa da época, wikis de fãs e Steam. Não deu para abrir as páginas
inteiras, e o Reddit ficou de fora. Número de wiki de fã é datado. **(incerto)** marca fonte fraca. As fontes estão no fim.

## Contexto
- **Origem e motor:** da Doobic Studios, comprada pela Nexon antes do lançamento. Motor LithTech Jupiter.
- **Lançamento e auge:** lançado na América do Norte em 11/07/2008. Chegou a 3 milhões de registros em set/2009 e passou de 4 milhões em jul/2010.
- **Donos depois:** a Nexon fechou a América do Sul em abr/2009, alegando latência. A operação passou para a VALOFE em nov/2017.
- **Versões novas:** o Reloaded saiu em 2017 (no Steam em 2018). O The Classic saiu em 2020 e é uma versão "de 2015". As duas continuam no ar.
- **Crítica:** "clone competente de Counter-Strike, muito arcade, mapas bem feitos" (GamesRadar). Nota 67 na PC Gamer.

## 1. Movimento e tiro
- **Ritmo:** rodadas rápidas e tempo para matar baixo. Tiro na cabeça ou rajada controlada decide a briga. O bunny hop não era punido; o jogo puxava mais para o arcade do que para o tático.
- **Dano (StrategyWiki, antigo):** 100 ou mais de dano mata com um tiro, nenhuma arma pedia mais de 5 acertos, e tiro na cabeça matava na hora.
- **Pulo, agachar e caixas:** pular muito ajudava a não morrer, e agachar escondia a cabeça atrás de cobertura baixa. Subir em caixas e contêineres era comum; o "Box Glitch" foi corrigido depois.
- **Nutshot:** tiro na virilha, de frente, só em personagem masculino. Virou marca do jogo ("27 milhões de nutshots").
- **Rede:** queixas de lag e de jogador "teleportando".
- **(incerto):** não achamos nerf de tiro no pulo, números da faca nem padrão de recuo. Medir no CA ou ver `design/decombat/`.

**Lições:**
- Calibrar pela sensação de rodada rápida, não pelo realismo.
- Pulo e agachar precisam ter valor tático.
- Testar a colisão das caixas.
- Ter um momento marcante próprio no feed de abates.
- Cuidar da predição e da compensação de lag desde cedo.

## 2. Modos e mapas
- **Quarantine (jul/2009):**
  - os zumbis são sorteados no início e infectam no corpo a corpo;
  - os humanos vencem matando todos os infectados ou sobrevivendo até o fim;
  - o infectado continua jogando;
  - zumbi ágil, com muita vida e imune a tiro na cabeça (wiki).
- **Quarantine Regen:**
  - o infectado começa com menos vida e recupera ficando parado por 3 s;
  - um mutagênico dava mais vida e regeneração.
  - Zumbi mãe e herói não foram confirmados.
- **Fireteam (mai/2009):** cooperativo de 8 jogadores contra a IA, com objetivos (intel, reféns, bomba).
- **Outros modos:**
  - **Elimination Pro:** uma vida por rodada.
  - **One Man Army:** meta de 30, 35, 40 ou 45 abates.
  - **Spy Hunt:** 5 maletas e duas fases.
  - **Last Man Standing:** rodadas de 3, 5, 7 ou 9, até 16 jogadores.
- **Mapas:** compactos, com cantos de emboscada famosos ("the corner" do Junk Flea), caixas e contêineres para subir e linhas de visão curtas ou médias. Duas saídas de base evitam o bloqueio do spawn.

**Lições:**
- O Surto já herda o que funcionou: o infectado continua jogando e a regeneração parado é legível.
- Decidir de propósito se o zumbi ignora tiro na cabeça.
- Mapas pequenos com "cantos com nome", e cada um com duas ou mais saídas de spawn.

## 3. Interface e experiência
- **Fluxo:** servidor → canal → lobby com lista de salas → sala → partida.
- **Criar sala:** até 16 jogadores, restrição de armas (ex.: sem sniper), entrar no meio da partida, dono da sala.
- **Fev/2010:** chat no lobby, loja com busca e tela de modificação de arma. Ainda não havia "partida rápida", e a interface não escalava com a resolução.
- **HUD de ago/2010:**
  - efeitos de abate;
  - munição das outras armas;
  - companheiros no mapa;
  - tecla para esconder o HUD (para quem grava vídeo).
- **Feed de abates:** "atuações especiais" como tiro na cabeça, nutshot, vingança e primeiro abate. Avisos de série de abates.
- **O que irritava:**
  - o kick pago ("Elite Moderator"), abusado e retirado em 2016;
  - votação usada contra novatos;
  - no Reloaded, não dar para expulsar ninguém.

**Lições:**
- As opções de sala que importam são restrição de armas e entrar no meio da partida.
- Usar marcadores especiais no feed de abates.
- O HUD tem que escalar com a resolução e poder ser escondido.
- Kick só com voto e regras claras; nunca vendido.

## 4. Progressão e economia (só como aviso)
- **Era Nexon:** armas alugadas por dias com GP, liberadas por patente. Vieram armas só por dinheiro e caixas aleatórias, e isso criou a fama de pay-to-win. Eram mais de 600 armas, e nem o novato que pagava sabia qual escolher.
- **Reloaded:** prometeu equilíbrio e criou o "Pure Combat Mode" (arsenal igual para todos). As resenhas continuaram "Mistas" por pay-to-win.

**Lições:**
- Tudo de graça elimina a principal causa de ódio.
- Poucas armas bem diferentes valem mais do que centenas parecidas.
- Condições iguais para todos.
- Sem nível nem XP por enquanto, por decisão do dono.

## 5. Por que decaiu, e o Reloaded e o Classic
- **Era Nexon:** hacks em massa, pay-to-win, latência e regiões fechadas, falta de polimento.
- **Reloaded:**
  - **o que mudou:** visual novo no mesmo motor (a equipe disse que a jogabilidade não mudava) e arsenal reequilibrado;
  - **reclamações:**
    - pay-to-win e hackers;
    - conta separada da Steam;
    - carregamento de mais de 15 minutos;
    - bugs que nunca foram corrigidos;
    - não dar para expulsar ninguém;
    - habilidades antigas que sumiram.
- **Classic:** voltou à sensação dos anos 2014 a 2016. Começou com uns 680 jogadores ao mesmo tempo e caiu rápido.
- **Conclusão:** não há prova de que o visual derrubou o jogo. O peso maior foi confiança (hack e dinheiro), atrito para entrar e a base pequena dividida entre duas versões.

**Lições:**
- A sensação é o produto: proteger os números de movimento e tiro.
- Abrir rápido e entrar rápido na partida.
- Ter uma versão só.
- Deixar a lista de bugs visível.

## 6. Hacks (lição de design)
- **O que houve:** mercado de cheats por assinatura e bans em massa sem efeito duradouro. Anti-cheat comprado (GameGuard, BattlEye) não resolveu.

**Lições:**
- O servidor manda em dano, acerto, munição e velocidade.
- Limites checados no servidor.
- Denúncia e demo.
- Kick e voto com registro.

## Fontes
GamesRadar (review e página do jogo); Joystiq/Engadget (hands-on de 2008); GamesBeat (estatísticas de nutshot); MMOHuts;
OnRPG (CA Europe); Wikipedia (Combat Arms); Ten Ton Hammer; Gaming Nexus; Blues News; Co-Optimus (UI); WorthPlaying
(atualizações de 2010 e Reloaded 2017); GameWatcher (Reloaded); MMOs.com (Classic); páginas do Steam 905640 e 1263550 e
discussões; SteamDB; StrategyWiki (Combat_Arms/Gameplay, Modes, Weapons, Junk_Flea); combatarms.fandom.com (Quarantine,
Calloused, The_Great_Hacker_War, NX/GP_Imbalance, Elite_Moderator, Box_Glitch, HUD, Game_Room); TV Tropes;
youtubesummary (resumo de "The Rise and Fall of Combat Arms").

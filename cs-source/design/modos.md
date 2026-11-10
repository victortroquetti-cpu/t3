# Modos de jogo do Trok

`trok_modo` vale no próximo mapa (o menu "Jogar" escolhe). Cada modo executa o seu cfg ao começar.

| Modo | `trok_modo` | cfg | Como funciona |
|---|---|---|---|
| Mata-mata em equipe | `equipe` (padrão; `tdm` também) | `cfg/equipe.cfg` | dois times; vence quem chegar na meta de abates (`mp_fraglimit`) ou tiver mais no fim do tempo (`mp_timelimit`) |
| Exército solo | `solo` (ou `ffa`) | `cfg/solo.cfg` | todos contra todos; meta de abates individual; o HUD mostra sua posição ("2º DE 8") |
| Surto | `surto` | `cfg/surto.cfg` | infecção (ver `design/modo-surto.md`) |
| Corrida Armada | `corrida` | `cfg/corrida.cfg` | todos contra todos com escada de armas: os abates trocam a sua arma; vence quem chegar na meta (seção abaixo) |
| Hordas | `hordas` | `cfg/hordas.cfg` | cooperativo contra hordas de zumbis bots por rodadas, com pontos, compras no mapa e máquina de melhoria (seção abaixo e `design/modo-hordas.md`) |

Regras comuns de equipe e solo, no estilo do Combat Arms (números MEDIR no jogo de referência):
- **Renascer sozinho** depois de `trok_renascer_tempo` s, com "RENASCENDO EM N" na tela de morte.
- **Proteção ao nascer** por `trok_protecao` s: não leva dano de jogador, aparece translúcido
  para os outros e "PROTEGIDO N" no seu HUD; acaba na hora em que você atira.
- **Nascer longe dos inimigos**: dos pontos de nascimento livres, sorteia alguns e fica com o mais
  longe do inimigo vivo mais perto, evitando ponto que algum inimigo enxerga.
- TAB com K/D; HUD de topo com o placar dos times (equipe) ou você x o líder (solo).
- Bots: no solo ninguém é colega (sem troca de informação nem divisão de entradas).

Código: `shared/trok/trok_gamerules.cpp` (`ModoThink`, `SetProtegido`, `GetPlayerSpawnSpot`,
`FPlayerCanTakeDamage`), `TrokModo()` em `shared/trok/trok_surto.cpp`. A proteção vai ao HUD por
`m_flTrokProtegidoAte` no `CHL2MP_Player` (patch).

Pendente: tela de resultado do fim da partida (hoje o HL2MP mostra o placar na intermissão),
escolha de meta e tempo no menu ("criar partida").

## Corrida Armada (`trok_modo corrida`)

O "Arms Race" do Combat Arms (modo M14 em `design/chimera/specs/combat_arms/game_modes.md`, com
M5, S5, S6, S7 e S10; a escada em `missions.md` F4.8). É o exército solo com uma escada de armas.

Regras (como no CA):
- **Todos contra todos**, uma rodada só, **sem tempo** (`mp_timelimit 0`). Renasce sozinho em 7 s
  (`trok_renascer_tempo`); proteção de 4 s ao nascer, que acaba quando você atira (`trok_protecao`).
- **Meta** = `mp_fraglimit`: 15, 30 ou 45 (padrão 15). Cada meta tem a sua escada em
  `scripts/trok_corrida.txt`. Meta sem escada vira a da escada mais perto.
- **Escada**: cada degrau começa num número de abates. Quando os seus abates de inimigo ficam
  **iguais** ao começo de um degrau, a sua arma vira a desse degrau na hora. Ao renascer, você volta
  com a arma do degrau em que está. Meta 15: 0 fuzil, 4 submetralhadora, 7 metralhadora, 10 escopeta,
  12 fuzil de precisão, 13 pistola, 14 granada de mão (a última). Abates por degrau: 4, 3, 3, 2, 1, 1, 1.
  Ainda não temos metralhadora nem fuzil de precisão: esses degraus estão com o fuzil.
- **Sorteio do CA** a cada troca (e ao nascer): número de 0 a 100; abaixo de `trok_corrida_chance` (80)
  vem a arma da escada (sorteio por peso entre as do degrau); senão vem a **sua arma da mesma classe**
  (a de `trok_armas_iniciais` no mesmo bucket; sem ela, a da escada). Ou seja, 80/101 = 79% a da escada.
  `trok_corrida_chance 101` = sempre a da escada.
- **Nada rebaixa**: abate com faca não tira degrau de ninguém; suicídio e queda só somam uma morte
  (não tiram abate, ao contrário do HL2MP).
- **Vence** o primeiro a chegar na meta (o abate da meta sai com a granada ou com a faca).
- **Último degrau**: todo mundo vê o aviso "FULANO ESTÁ NA ÚLTIMA ARMA".
- **Nenhuma arma cai** ao morrer, e nada se pega do chão (só a escada dá arma).
- **Saída**: se alguém sai e sobram 2 ou menos jogadores (contando quem saiu), a partida acaba;
  quem ficou vence se os abates da partida (de todos, até de quem saiu) passam de 10% da meta.
- Junto com a arma da escada vem a faca (`trok_corrida_extras`). **Não está nas especificações do CA**
  (elas só dizem que a arma é trocada); é escolha nossa, para não ficar sem arma depois da granada.

Tela: embaixo do placar do topo, "ARMA 3/7  SUBMETRALHADORA" e "PRÓXIMA: ESCOPETA EM 3 · 7/15"
(no último degrau, "ÚLTIMA ARMA"); faixa no meio com o aviso da última arma e, no fim, quem venceu.
TAB com a coluna ARMA (degrau de cada um). Bots: recebem a mesma arma e são mandados usá-la (a pilha
de "arma obrigatória" do bot do HL2MP), senão o bot sacaria a faca quando não vê ninguém.

Cvars (`cfg/corrida.cfg`): `mp_fraglimit`, `mp_timelimit`, `trok_renascer_tempo`, `trok_protecao`,
`trok_corrida_chance`, `trok_corrida_extras`, `trok_corrida_saida_jogadores`, `trok_corrida_saida_fracao`.
No cliente: `trok_corrida_hud`, `trok_corrida_hud_aviso`. Reler a escada: `trok_corrida_recarregar`.

Código: `shared/trok/trok_corrida.cpp/.h` (regras e estado em rede `trok_corrida_estado`),
`client/trok/hud_trok_corrida.cpp` (HUD), ganchos pequenos em `trok_gamerules.cpp` (cfg, `Think`,
`PlayerKilled`, `CanHavePlayerItem`), `ui/trok_scoreboard.cpp` (coluna ARMA) e no patch em
`hl2mp_player.cpp` (`GiveDefaultItems` e `Event_Killed`).

Diferenças do CA que ficaram: o ponto de nascimento é o do solo (longe dos inimigos), não a lista
embaralhada com cursor do CA; a proteção é uma janela só (o CA tem 3 s para tiro e 4 s para todo dano).

Testar no PC: `trok_modo corrida`, `map de_dust2`, `exec bots`. Conferir a troca no 4º, 7º, 10º...
abate; renascer com a arma do degrau; aviso da última arma; vitória no 15º; suicídio (`kill`) não
tirando abate; nada caindo ao morrer; bots usando a arma da escada e jogando a granada (se o bot não
souber usar a granada, ele fica preso no último degrau: anotar); TAB com a coluna ARMA.

## Hordas (`trok_modo hordas`)

Pedido do dono: modo de hordas de zumbis por rodadas, como os zumbis do CoD, inspirado na Equipe
Armada (Fireteam) do CA, com economia, loja física no mapa e máquina de melhoria de 5 níveis.
Desenho completo, comparação dos CoDs e decisões: **`design/modo-hordas.md`**.

- Todos os jogadores são humanos (time dos Remanescentes); os zumbis são bots do diretor no outro time,
  num grupo reaproveitado (máximo vivo ao mesmo tempo, total por rodada).
- Rodada: os zumbis da rodada nascem aos poucos nos `info_trok_hordas_spawn` das áreas abertas; acaba
  quando todos morrem; intervalo de 10 s; quem morreu volta na rodada seguinte.
- Vida 150 → +100 por rodada até a 9ª → ×1,1 por rodada; quantidade = fórmula do CoD (base 24 + 6 por
  jogador); velocidade sobe até correr na 7ª; tipos novos entram por rodada (Charger 5, Striker 8,
  Splicer 12, Crusher 15...). Matilha (corredores) a cada 4–6 rodadas; Colosso (chefe) a cada 10.
- Pontos (separados do dinheiro da economia): acerto 10, abate 50, cabeça 100, faca 130, tábua 10.
- Compra só na **LOJA** (`func_trok_loja`, num lugar do mapa: anda até ela, E abre o menu). **Sem
  arma na parede e sem portas pagas** (decisões do dono). Outras estações com E: máquina de melhoria
  (MK I a MK V; com arma de defesa na mão, nível de defesa I a III), caixa misteriosa, energia.
- **Defesas** em destaque, sem construir muros: torreta, claymore, mina e carga remota (as do CA),
  com limite de postas por jogador (`trok_hordas_limite_*`), e **armadilhas do mapa** (elétrica, fogo,
  arame; `func_trok_armadilha`) ligadas com pontos por um tempo.
- Derrubado em vez de morrer; colega segura E 3 s para reviver; todos caídos = fim de jogo.
- Bônus que caem: Reabastecimento, Abate certo, Pontos em dobro, Ataque aéreo, Reforço, Caixa barata.

Código: `shared/trok/trok_hordas.{h,cpp}` (estado em rede, níveis), `shared/trok/trok_hordas_estacao.cpp`,
`server/trok/trok_hordas_diretor.cpp`, `server/trok/trok_hordas_mapa.{h,cpp}`, `client/trok/hud_trok_hordas.cpp`.
Mapa de teste: `tools/mapas/gerar_hordas_teste.py` → `game/mod_trok/mapsrc/trok_hordas_teste.vmf`.

Testar no PC: `trok_modo hordas`, `maxplayers 32`, `map trok_hordas_teste` (depois de compilar e gerar o
nav) ou `map de_dust2` (sem estações: zumbis nascem nos pontos do CS:S). `trok_hordas_info` mostra a
tabela das rodadas; `trok_hordas_rodada 15`, `trok_hordas_pontos 50000`, `trok_hordas_dar_bonus 1`,
`trok_hordas_ligar_energia` (cheats) para pular etapas.

# Economia estilo CS (base; o dono termina)

**Estado:** feito na nuvem em 10/11/2026, **compila na checagem de sintaxe, nunca rodou**. Desligada por
padrão (`trok_economia 0` em `cfg/economia.cfg`). Liga com `trok_economia 1` e um mapa novo (ou `mp_restartgame`).

## Por que e como difere do Combat Arms

O CA não tem economia dentro da partida: as armas e os itens vêm do arsenal (comprados fora, com GP/NX) e
todo mundo nasce com o que equipou. O CS tem dinheiro por rodada: ganha abatendo e vencendo, perde morrendo
(perde o que comprou) e compra num tempo e numa zona. Esta base é a do CS:S, com números do CS:S
(`design/chimera/games/cs_source/data/economy.toml`, `rounds.toml`, `specs/cs_source/weapons.md` "Economy",
`specs/cs_source/objectives.md` "Money"), e dois acréscimos pedidos pelo dono: **coletes em níveis** que dão
armadura **e** mais fôlego **e** mais velocidade, e **capacete** que tira o crítico do tiro na cabeça. No Surto, a
loja também vende os **mutagênicos** (tipos de zumbi; no CA eram comprados fora da partida).

## Regras (o que o código faz)

| Regra | Como | Número (cfg) | CS:S |
|---|---|---|---|
| Dinheiro inicial | quem entra no mapa | `trok_economia_inicial 800` | 800 |
| Teto | nunca passa | `trok_economia_teto 16000` | 16000 |
| Abate de inimigo | pela **classe da arma** (bloco `Abate` de `scripts/trok_loja.txt`) | 300 em todas | 300 em todas (o CS:GO paga por classe) |
| Abate de aliado | | `trok_economia_aliado -3300` | −3300 |
| Suicídio | nada | | nada |
| Rodada ganha | todo mundo do time vencedor | `trok_economia_vitoria 3000` | 3000 (eliminação, mapa sem objetivo) |
| Rodada perdida | 1400 + 500 por derrota seguida, até 3400 | `_derrota`, `_derrota_passo`, `_derrota_max` | igual |
| Sobreviveu | a mais para quem termina vivo (não zumbi) | `trok_economia_sobreviver 0` | 0 |
| Empate | ninguém ganha | | igual |
| Tempo de compra | segundos depois de **nascer** | `trok_economia_tempo_compra 20` | 6 de congelamento + 90 |
| Zona de compra | dentro de um `func_buyzone` do seu time (mapas do CS:S têm); mapa sem zona: até `trok_economia_raio` de onde nasceu | `trok_economia_area 1`, `trok_economia_raio 600` | só a zona |
| Kit inicial | só pistola e faca | `trok_economia_armas_iniciais`, `trok_economia_municao_inicial` | faca + pistola do time |
| Morreu | perde tudo o que comprou (colete e capacete também) | | igual |
| Sobreviveu à rodada | renasce com o que comprou, sem pagar | `trok_economia_manter 1` | igual |
| Trocar arma do mesmo slot | a velha cai no chão | `trok_economia_largar 1` | igual |
| Arma comprada | vem com pente e reserva cheios (`municao` no script muda) | | igual |

Diferenças da base para o CS:S, para o dono decidir:
- **O bônus de rodada é pago no fim da rodada**; o CS:S paga no começo da seguinte (dá no mesmo para o jogador).
- **Sequência de derrotas é por jogador**, não por time (no Surto os times se misturam a cada rodada).
- **Surto:** quem termina no time vencedor ganha a vitória. Se os zumbis vencem, **todos** (inclusive quem foi
  infectado) estão no time zumbi e ganham a vitória; se os humanos vencem, só os humanos vivos. Sugestão a
  decidir: no Surto pagar por papel (sobreviveu, infectou, abateu zumbi) em vez de por time.
- **Equipe e Solo não têm rodadas:** o dinheiro vem só de abates; compra-se a cada renascimento (20 s, perto
  do ponto de nascimento). É o "deathmatch com dinheiro" do CS. Se o dono quiser rodadas nesses modos, é outro
  trabalho (modo de rodada).
- **Bots não compram** (ganham dinheiro, mas nascem só com o kit). Fazer bots comprarem é o próximo passo natural.

## Loja (`scripts/trok_loja.txt`)

Categorias (teclas 1..8 no menu): **Pistolas, Submetralhadoras, Fuzis, Escopetas, Especiais, Explosivos,
Equipamentos, Mutagênicos.** Cada item tem `nome`, `categoria`, `tipo`, `preco`, `descricao` e os campos do tipo:

| Tipo | Campos | O que faz no jogo |
|---|---|---|
| `arma` | `entidade`, `municao` (-1 = cheia), `max` (arremesso), `time` | dá a arma; a do mesmo slot cai; granada: compra mais uma até `max` |
| `colete` | `nivel`, `armadura`, `folego`, `velocidade` | armadura (pontos do HL2, até 255); **velocidade** multiplica a do movimento (no do CA entra em `CAVelocidadeAlvo`, no outro em `CheckParameters`); **fôlego**: a corrida gasta menos energia (devolve `gasto × (1 − 1/folego)` a cada quadro, como o fôlego do zumbi). Só compra nível maior |
| `capacete` | `protecao`, `golpes` | tiro na cabeça: o multiplicador `sk_player_head` vira `1 + (mult − 1) × (1 − protecao)`; `golpes` > 0 = quebra depois de tantos tiros na cabeça |
| `municao` | `slot` (-1 = todas), `municao` (0 = enche) | enche a reserva |
| `medico` | `vida` | devolve vida na hora (até a máxima) |
| `mutagenico` | `zumbi` (tipo de `scripts/trok_zumbis.txt`) | só no Surto: `TrokZumbis_Liberar`; o jogador escolhe o tipo na tecla N (`trok_zumbi_classe`) |

Preços de partida (CS:S onde existe; o resto **PROPOSTA**):

| Item | Preço | Origem |
|---|---|---|
| Pistola | 400 | Glock |
| Submetralhadora | 1500 | MP5 |
| Fuzil | 2500 | AK-47 (M4A1 3100) |
| Escopeta | 1700 | M3 |
| Lança-foguete | 4750 | PROPOSTA (= AWP) |
| Granada / de luz / de fumaça | 300 / 200 (até 2) / 300 | HE / flash / smoke |
| Mina / Claymore / Carga remota | 500 / 600 / 700 | PROPOSTA (armas da sessão de explosivos) |
| Colete leve (nív. 1): 30 de armadura, fôlego +10%, velocidade +2% | 400 | PROPOSTA |
| Colete (nív. 2): 60, +20%, +4% | 650 | kevlar 650 |
| Colete tático (nív. 3): 100, +30%, +6% | 1200 | PROPOSTA |
| Capacete: tira 50% do crítico, não quebra | 350 | capacete 350 |
| Capacete balístico: tira 100%, quebra em 2 tiros | 800 | PROPOSTA |
| Kit de munição (todas as armas) | 0 | munição é grátis no CS:S |
| Kit médico (+50 de vida) | 400 | PROPOSTA (o CS:S não vende vida) |
| Mutagênicos (veloz, brutamontes, investida, tóxico) | 1000 cada | PROPOSTA |

**Atenção à armadura:** a absorção é a do HL2 (o colete segura a maior parte do dano enquanto tem pontos), bem
mais forte que o kevlar do CS:S (que só corta parte do dano conforme a arma). Por isso os pontos estão baixos.
Medir e decidir: manter a regra do HL2, ou trocar pela do CS (precisa de um gancho no dano).

## Menu de compra e HUD

- **Tecla B** (`trok_loja`; `bind "b" "trok_loja"` no `config_default.cfg`, quem já tem `config.cfg` precisa dar o
  bind uma vez) abre o menu próprio (`client/trok/ui/trok_loja.cpp`, visual dos painéis do Trok): dinheiro e
  tempo de compra no alto, equipamento atual, categorias à esquerda (1..8), itens no meio (clique compra; mostra
  preço, "NA MÃO"/"EQUIPADO", vermelho sem dinheiro), detalhes do item à direita. Fecha com B, ESC, FECHAR, ao
  morrer ou quando o tempo acaba. O servidor confere tudo de novo.
- Console: `trok_comprar <id> [<id> ...]`, `trok_loja_lista`, `trok_loja_recarregar` (cliente; o servidor relê a
  loja a cada mapa). Teste: `sv_cheats 1` e `trok_dinheiro_teste 10000`.
- HUD (`client/trok/hud_trok_economia.cpp`): dinheiro acima da vida, "+$300"/"−$2500" por 2 s, "COMPRA 14 s [B]",
  colete e capacete. Só com a economia ligada (`trok_hud_dinheiro 0` esconde).
- Rede: dinheiro, prazo de compra, nível do colete, capacete e os multiplicadores de velocidade e fôlego vão na
  entidade `trok_placar_extra` (`shared/trok/trok_placar_extra.h`), que sobrevive à limpeza entre rodadas.

## O que o dono decide

1. Ligar a economia em quais modos (hoje é uma chave só, para todos).
2. Ganho por abate: plano (CS:S, 300) ou por classe (CS:GO).
3. Surto: pagar por time (hoje) ou por papel; preço dos mutagênicos; se mutagênico comprado dura o mapa.
4. Coletes: pontos, fôlego e velocidade de cada nível; regra de armadura (HL2 ou CS).
5. Capacete: proteção e se quebra.
6. Kit médico existe ou não (no CS não tem).
7. Bots comprando.
8. Tempo de compra nos modos sem rodada.

## O que testar (no PC)

1. `trok_economia 1`, `map de_dust2`: nasce só com pistola e faca, $800 no HUD, "COMPRA 20 s [B]".
2. B abre o menu; comprar o fuzil ($2500 com `trok_dinheiro_teste`), ver a pistola ficar e a reserva cheia.
3. Comprar a escopeta com o fuzil na mão: o fuzil cai no chão.
4. Sair da zona de compra do CS:S (ou andar 600 u num mapa sem zona): "Fora da zona de compra".
5. Esperar 20 s: "O tempo de compra acabou" e o menu fecha.
6. Abater bot: +$300; abater aliado (equipe): −$3300; nunca abaixo de 0 nem acima de 16000.
7. Colete tático: armadura 100, correr mais rápido (cl_showpos) e o fôlego durar ~30% mais.
8. Capacete: levar tiro na cabeça de bot e comparar o dano (balístico: 2 tiros sem crítico, depois normal).
9. Morrer: renasce só com o kit; colete e capacete somem.
10. Surto: comprar no preparo; humano vivo no fim ganha $3000 e renasce com o que comprou; zumbi não compra.
11. Mutagênico no Surto: comprar o veloz, virar zumbi e escolher com N; comprar de novo dá "já está liberado".
12. Kit de munição e kit médico; granadas até o máximo (dependem das armas da sessão de explosivos existirem).

# Mutagênicos do Quarantine (Striker e Crusher)

Fonte: wiki do Combat Arms (combatarms.fandom.com: The_Striker, The_Crusher, Striker_Mutagen, Crusher_Mutagen,
Calloused, Infected_Hide, Quarantine_Regen), lida em 10/11/2026. Nos arquivos do jogo eles aparecem como
`Infected Merc Subtype-S` (Model 56, garra 521 `Quarantine_SubtypeS_Hand`) e `Subtype-P` (Model 57, garra 522), corpos
"Assassin" e "Tanker" (`design/chimera/specs/combat_arms/characters.md`, L5).

## Como funcionava
- Item comprado (com dinheiro real, por dias) e usado só no Quarantine Regen.
- No começo de cada rodada aparece uma tela de escolha: **0** = infectado normal, **1** = Striker, **2** = Crusher.
  Sem escolher, fica o normal. A tela fica na frente até escolher.
- Dá para ter os dois; só um vale por rodada.
- **Bônus quando humano** (com qualquer um dos dois equipado): +3% de velocidade, dano e fôlego, +25% de munição em
  todas as armas. O Crusher também dá +3% de vida e pulo.

## Striker ("zumbi mulher", também chamada Assassin)
| Item | Valor |
|---|---|
| Vida | hospedeiro **275–285** (páginas divergem), infectado **185** |
| Habilidade "Virulent" | **fôlego infinito** e mais velocidade |
| Tipo | a mais rápida dos infectados; corpo fino (mais difícil de acertar); resistente a tiro na cabeça |
| Ataque | dois golpes de garra (igual ao normal) |
| Visual | mulher com macacão branco da NEMEXIS ensanguentado, capuz, óculos e máscara de oxigênio, garras longas, olhos com rastro amarelo |

## Crusher (o grandão, também Tanker / Enforcer)
| Item | Valor |
|---|---|
| Vida | hospedeiro **450**, infectado **305** |
| Habilidade "Calloused" | regeneração de **20%** por segundo (o normal é 10%) e mais vida |
| Ataque | **mais alcance**, mas golpe com uma mão só e mais lento |
| Tipo | o mais resistente; grande (alvo maior, pode bloquear a passagem dos outros zumbis) |
| Visual | homem grande com boné e uniforme azul da NEMEXIS, olhos com rastro azul |
| No modo PvE | explode numa nuvem de ácido azul ao morrer (só nos NPCs) |

## Vida do infectado normal pelo colete (para comparação)
Ultralight 200/135, Light e Recon 225/150, Tactical 275/185, Medium e Combat 300/200, Balanced e Assault 330/230,
Heavy 350/235, Tanker 375/250, Commando 400/275 (hospedeiro/infectado). O colete não muda a velocidade do infectado.

## No Rearmed
- Implementado no Surto como **tipos de zumbi** (`scripts/trok_zumbis.txt`): escolha com a tecla N ou a aba FÚRIA da loja,
  compra com **Fúria** (a moeda do infectado), paga a cada vez que vira zumbi.
- Como no CA, só o golpe de garra infecta; gás só fere.
- Números que a wiki não dá e ficaram de chute (MEDIR): o quanto a Striker é mais rápida, o alcance e o tempo do
  golpe do Crusher, o tamanho do Crusher.

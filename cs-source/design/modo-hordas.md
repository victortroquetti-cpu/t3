# Modo Hordas (`trok_modo hordas`)

Pedido do dono (10/11/2026): "um modo de hordas de zumbis por níveis, como o CoD Zombies, inspirado
na Equipe Armada (Fireteam) do CA. Pense na economia, armas auxiliares como torretas etc. Terá um
sistema de upgrade de armas e loja em um lugar físico — não é só apertar B, tem que andar pelo mapa.
O upgrade é igual àquela máquina do CoD (Pack-a-Punch), se não me engano tem 5 níveis."
E depois: estudar os Zombies de todos os CoDs e montar o **nosso**, com o melhor de cada um.
Correções do dono (10/11/2026):
- **sem portas pagas** ("só tira as portas pagas e foca nessas armas: torreta, claymore, minas etc.");
- **sem arma na parede**: armas, munição, equipamento e defesas se compram numa **LOJA num lugar
  específico** do mapa (`func_trok_loja`): anda até ela, aperta E, abre o menu (seção 8);
- o jogador continua **andando pelo mapa como nos CoDs**, pode **pôr torretas, claymores e minas**,
  **ligar armadilhas** do mapa e procurar cantos seguros, **sem construir muros**. Sacos de areia,
  arame farpado e barricadas posicionáveis ficam para o **futuro** (seção 8C). Nada de fase de construção.

Nome provisório: **Hordas**. Tudo o que é número está em `cfg/hordas.cfg`, `scripts/trok_hordas.txt`
e `scripts/trok_upgrade.txt` (regra 4 do dono). Marcas: **PROPOSTA** = número nosso, ajustar jogando;
**DECISÃO DO DONO** = não fazer/mudar sem ele.

---

## 1. Pilares (o que o modo tem que ser)

1. **Sobreviver em equipe, rodada após rodada.** A graça é a curva: começa calmo (andadores, pistola) e
   termina em pânico (corredores, tipos especiais, chefe). Não há vitória: há "até onde vocês foram".
2. **Comprar exige andar.** Uma **loja** (ou poucas) num lugar específico, a máquina de melhoria, a
   caixa e as armadilhas ficam espalhadas: ir comprar é um risco no meio da rodada ("vou à loja
   repor a torreta ou aguento mais uma rodada?").
3. **Defender com equipamento, não com muro.** Torreta, claymore, mina, carga remota e as
   armadilhas do mapa são o "plano" do time: onde pôr, quando repor, quem melhora as defesas,
   qual canto segurar. É a parte "Equipe Armada" do modo, sem virar tower defense.
4. **Pontos = desempenho.** Ganha quem atira (acerto), mata (abate, cabeça, faca) e defende
   (tábuas). Gasta-se no mapa. Uma moeda só, simples de ler.
5. **Humano não tem poderes** (regra do dono). Nada de perks mágicos, gomas ou habilidades. O que o
   CoD dá como perk, aqui é **equipamento** (colete, kit) ou fica para o dono decidir.
6. **Visual sóbrio militar.** Nomes de bônus e níveis militares ("Reabastecimento", "MK III"), sem
   neon. A melhoria é "munição e ajuste de armeiro", não magia.

---

## 2. Os Zombies de cada CoD e o que tiramos de cada um

Fontes: wikis e guias de cada jogo (links no fim). Números do CoD servem só de referência (regra 2).

| Ponto | WaW (2008) | BO1 (2010) | BO2 (2012) | BO3 (2015) | BO4 (2018) | Cold War (2020) | Vanguard (2021) | MWZ / MWIII (2023) | BO6 (2024) |
|---|---|---|---|---|---|---|---|---|---|
| **Rodadas e vida** | rodadas sem fim; vida 150 +100/rodada até a 9ª, depois ×1,1 | igual (fórmula clássica) | igual | igual | igual, com "modo" de vida ajustável (Casual/Normal/Realista) | rodadas; vida sobe mais devagar; armadura nos zumbis | objetivos em vez de rodadas puras | mundo aberto em zonas de ameaça (sem rodadas) | rodadas clássicas de volta, curva parecida com o CW |
| **Pontos / economia** | 10 por acerto, 50–100 por abate, 130 faca | igual | igual | igual | igual | pontos (essência) + **sucata** (para armadura e raridade) | essência + sucata | essência + sucata + contratos; itens guardados | essência + sucata |
| **Arma na parede / caixa** | parede (giz) + caixa 950 | igual (caixa muda de lugar, ursinho) | igual | igual | igual | parede (preço sobe com a raridade) + caixa | caixa e parede | caixa e parede pelo mapa aberto | parede + caixa |
| **Pack-a-Punch** | só no Der Riese, 1 nível (5000) | 1 nível | 1 nível + troca de acessório (2000) | 1 nível + **munição alternativa (AAT)** ao repetir (2500) | **5000 + até 4 repetições** de +25% de dano e AAT (2500 cada): ~5 "níveis" | **3 níveis** (5000/15000/30000) + **raridade** na Bancada de Arsenal (5 cores, com sucata) | 3 níveis + raridade | 3 níveis (cristais de éter) + raridade (ferramentas) | **3 níveis** + raridade (Comum → Lendária; sucata 500/1000/2500/5000) |
| **Perks / aumentos** | Juggernog, Revive, Speed, Double Tap | + PhD, Stamin-Up, Deadshot, Mule Kick | + Who's Who, Tombstone, Vulture | + Widow's Wine, Electric Cherry; **Gomas** | 4 perks escolhidos antes + Elixires e Talismãs | perks com níveis (por cristais) | **Altares** de bênção | perks em latas achadas | perks com **aumentos** (melhorias permanentes) |
| **Derrubado e reviver** | derrubado, colega revive; sangra ~30 s | igual; Quick Revive no solo | igual (+ Who's Who) | igual | igual | igual + **kit de autorreanimação** (sucata) | igual + kit | igual + kit | igual + kit |
| **Equipamento de campo / torretas** | armadilhas pagas (choque, fogo) | armadilhas | **montáveis** (turbina, torreta, armadilha) | armas especiais por mapa | armas especiais (carga) | **melhorias de campo** (habilidade com recarga) + bancada (granadas, armadura) | habilidades | habilidades + bancada | melhorias de campo + bancada |
| **Salvar e sair** | não | não | não | não | não | **exfiltração** (sair com recompensa) | exfiltração | extração é o jogo | exfiltração + salvar partida solo |
| **Mapa, portas, energia** | portas pagas, energia liga as máquinas | igual | ônibus (TranZit), energia por área | portas, energia, passos ocultos | igual | portas + energia | portais / objetivos | mundo aberto em 3 zonas | portas + energia + passos |
| **Especiais e chefes** | cães (Shi No Numa) a cada 4–8 rodadas | cães, macacos, chefes de mapa | cães, Panzer, Brutus | Margwa, Panzer, aranhas | chefes por mapa | Megaton, Mimic, Tempestade | chefes por objetivo | Mega Abominação em contratos | Amalgam, Abominação, chefes de mapa |
| **Dificuldade / jogadores** | +6 zumbis por jogador; máx. 24 vivos | igual | igual | igual | modos de dificuldade | escalada por jogador | igual | por zona (níveis 1–3) | modos de dificuldade + escala por jogador |

**O que o "5 níveis" do dono era:** no BO4 a máquina melhora a arma **uma vez e repete até 4 vezes**
(cada uma +25% de dano), e no Cold War/BO6 a arma tem **5 raridades** (Comum a Lendária) além dos 3
níveis da máquina. O nosso junta as duas ideias numa escada só, **5 níveis na mesma máquina física**
(seção 7), sem segunda moeda.

### O que adotamos (e de onde)

| Do jogo | O que entra | Por quê |
|---|---|---|
| WaW / BO1 | curva clássica de vida e quantidade; pontos por acerto/abate/cabeça/faca; tábuas nas janelas | é a curva que todo mundo conhece e funciona; tábuas dão o que fazer entre as ondas |
| BO1 | caixa misteriosa (950), energia que liga a máquina, rodada dos "cães" | rota e decisão pelo mapa; rodada especial quebra a rotina |
| BO2 / Cold War | **armas de defesa** postas no chão (torreta) | é equipamento, não poder; a torreta, a claymore, a mina e a remota do CA já existem no mod |
| WaW / BO1 | **armadilhas do mapa** pagas (elétrica, fogo) com tempo e recarga | defesa sem construir: o mapa oferece, o jogador liga na hora certa |
| MWZ / Cold War | **estação de compra** (Buy Station / bancada) num lugar fixo em vez de armas na parede | pedido do dono: uma loja num lugar específico, com menu |
| BO3 / BO4 | munição especial nos níveis altos; repetir a máquina (vários níveis) | dá motivo para guardar pontos até o fim; nível alto "muda" a arma |
| BO4 | 3 dificuldades | o CA (Equipe Armada) também tem 3 |
| Cold War / BO6 | colete, kit de autorreanimação, granadas como itens de compra; nível visível da arma (cor) | troca os perks por equipamento, respeitando "humano sem poder" |
| Equipe Armada (CA) | 3 dificuldades com fator de vida 1,0/1,2/1,5; limite de vivos por rodada; nascedouros por área; tipos Charger, Splicer, Striker, Crusher | é a nossa raiz (sucessor do CA) e os tipos já existem (`trok_zumbis.txt`) |

### O que fica de fora (por agora)

- **Armas na parede** (decisão do dono): tudo se compra na loja. `func_trok_compra_parede` e
  `func_trok_bancada` ficam no código e no FGD como opcionais, fora do desenho e do mapa de teste.
- **Construir muros / fase de construção / tower defense** (decisão do dono): o modo continua o
  loop do CoD. Sacos de areia, arame farpado e barricadas posicionáveis: futuro (seção 8C).
- **Portas pagas e áreas que se abrem por pontos** (decisão do dono). A entidade
  `func_trok_porta_paga` continua no código e no FGD como opcional, mas não entra no desenho nem no
  mapa de teste; a progressão vem da energia, da máquina e das defesas.
- **Perks, gomas, elixires, talismãs, altares, melhorias de campo** (habilidades): quebram a regra
  "humano não tem poderes". Ver seção 9 para os substitutos (DECISÃO DO DONO).
- **Segunda moeda (sucata / salvage)** e raridade separada: complica a leitura; nosso nível da máquina
  já faz o papel da raridade. Pode voltar se o dono quiser "loot".
- **Mundo aberto, contratos e extração do MWZ**: outro jogo; não combina com mapas pequenos do Source.
- **Exfiltração (sair com recompensa)** e **salvar partida**: só faz sentido com progressão (nível/XP),
  que o dono não quer agora.
- **Ônibus, passos ocultos (easter eggs), armas maravilha**: caros de fazer; ideia para o futuro.
- **Armadilhas pagas** (choque, fogo): fáceis de fazer como estação; ficam como sugestão (seção 16).

---

## 3. Loop do jogo

```
ESPERA (sem ninguém) -> PREPARO 15 s -> RODADA 1 -> INTERVALO 10 s -> RODADA 2 -> ... -> FIM DE JOGO
```

1. **Preparo:** todos nascem na sala inicial com pistola, faca e granadas, e **500 pontos**.
2. **Rodada:** o diretor faz nascer os zumbis da rodada aos poucos, nos nascedouros do mapa,
   perto dos jogadores, nunca colado em ninguém. No máximo N vivos ao mesmo tempo; morreu um, nasce
   outro, até acabar o total da rodada.
3. **Durante a rodada:** atirar dá pontos; zumbis quebram as tábuas das janelas; humanos consertam
   (E) entre um zumbi e outro; bônus caem de vez em quando.
4. **Fim da rodada:** o último zumbi morre. Intervalo de 10 s: hora de ir à loja, repor e reposicionar
   as defesas (torreta, claymores, minas), melhorar a arma, consertar tábuas. Quem morreu volta no começo da próxima rodada (com a pistola).
5. **Fim de jogo:** ninguém de pé (todos derrubados ou mortos). Mostra "FIM DE JOGO — RODADA N" e o
   recorde do mapa; 12 s depois o mapa recomeça (barricadas inteiras, defesas somem, pontos de volta a 500).

---

## 4. Curvas por rodada (fórmulas)

Código: `server/trok/trok_hordas_diretor.cpp` (`VidaDaRodada`, `QuantidadeDaRodada`, ...). No jogo:
`trok_hordas_info` imprime a tabela com os números do cfg.

- **Vida** do zumbi normal: `150 + 100 x (R - 1)` até a rodada 9; da 10ª em diante `950 x 1,1^(R - 9)`.
  Vezes o tipo (seção 5) e a dificuldade (seção 11). Teto opcional (`trok_hordas_vida_teto`).
- **Quantidade** (total da rodada, fórmula do CoD): `24 + extra`, com
  `mult = max(1, R / 5)` (e `x R x 0,15` da 10ª em diante) e `extra = 0,5 x 6 x mult` sozinho ou
  `(J - 1) x 6 x mult` com J jogadores. Rodadas 1 a 5 levam `x 0,25 / 0,3 / 0,5 / 0,7 / 0,9`.
- **Vivos ao mesmo tempo:** `10 + 4 x (J - 1)`, teto 24 e nunca mais que as vagas do servidor
  (PROPOSTA; o CoD usa 24 fixo, mas cada zumbi aqui é um bot NextBot e ocupa uma vaga).
- **Velocidade** (x a do tipo): `0,6 + 0,06 x (R - 1)`, teto 1,15, cada zumbi ±12%. Rodada 1 a 3
  andam, da 7ª em diante todos correm.
- **Intervalo entre nascimentos:** `2 s x 0,95^(R - 1)`, mínimo 0,15 s.

| Rodada | Vida | Zumbis (1 jog.) | (2) | (4) | Velocidade | Intervalo (s) |
|---|---|---|---|---|---|---|
| 1 | 150 | 6 | 7 | 10 | 0,60 | 2,00 |
| 2 | 250 | 8 | 9 | 12 | 0,66 | 1,90 |
| 3 | 350 | 13 | 15 | 21 | 0,72 | 1,80 |
| 4 | 450 | 18 | 21 | 29 | 0,78 | 1,71 |
| 5 | 550 | 24 | 27 | 37 | 0,84 | 1,63 |
| 6 | 650 | 27 | 31 | 45 | 0,90 | 1,55 |
| 7 | 750 | 28 | 32 | 49 | 0,96 | 1,47 |
| 8 | 850 | 28 | 33 | 52 | 1,02 | 1,40 |
| 9 | 950 | 29 | 34 | 56 | 1,08 | 1,33 |
| 10 | 1045 | 33 | 42 | 78 | 1,14 | 1,26 |
| 12 | 1264 | 36 | 49 | 101 | 1,15 | 1,14 |
| 15 | 1682 | 44 | 64 | 145 | 1,15 | 0,98 |
| 18 | 2240 | 53 | 82 | 198 | 1,15 | 0,84 |
| 20 | 2710 | 60 | 96 | 240 | 1,15 | 0,75 |
| 22 | 3279 | 67 | 111 | 285 | 1,15 | 0,68 |
| 25 | 4365 | 80 | 136 | 361 | 1,15 | 0,58 |
| 30 | 7030 | 105 | 186 | 510 | 1,15 | 0,45 |
| 40 | 18234 | 168 | 312 | 888 | 1,15 | 0,27 |
| 50 | 47295 | 249 | 474 | 1374 | 1,15 | 0,16 |

Leitura estratégica: até a 9ª a vida sobe em linha (a pistola e a primeira arma da loja dão conta);
da 10ª em diante sobe 10% por rodada e **só a máquina de melhoria acompanha** (×1,6 → ×8). A rodada
20 é o "muro" sem MK III; a 30, sem MK V. Isso força a economia a mirar na máquina.

Referência de dano das nossas armas (CA, `design/sistemas-jogabilidade.md`): pistola 26, MP5 29,
M4 35, cabeça x6. Rodada 1: um tiro na cabeça de pistola mata. Rodada 10 (1045): 6 tiros de M4 no
corpo ou 1 na cabeça. Rodada 30 (7030) com M4 MK V (×8): 25 no corpo ou 5 na cabeça. **MEDIR** se a
cabeça x6 do CA deixa fácil demais; se deixar, criar um multiplicador de cabeça só para zumbi.

---

## 5. Tipos de zumbi por rodada

Tipos de `scripts/trok_zumbis.txt` (os do Surto). A tabela do Hordas (`scripts/trok_hordas.txt`,
bloco `tipos`) diz quando cada um entra e quanto pesa no sorteio.

| Tipo | Entra na rodada | Peso (cresce até) | Vida x | Pontos x | Papel tático |
|---|---|---|---|---|---|
| Normal (Infectado) | 1 | 100 | 1 | 1 | a massa |
| Charger | 5 | 4 → 14 | 0,8 | 1,2 | explode ao morrer: castiga quem mata de perto |
| Striker | 8 | 6 → 18 | 0,75 | 1,2 | rápido, sem cansar: quebra a defesa parada |
| Splicer | 12 | 4 → 10 | 1,3 | 1,5 | golpe paralisa, nuvem tóxica: obriga a cobrir o colega |
| Crusher | 15 | 3 → 6 (máx. 3/rodada) | 4 | 3 | tanque, alcance maior: precisa de foco do time |
| Striker explosiva | 18 | 3 → 8 | 0,9 | 1,5 | solta bolhas que explodem: obriga a se mexer |
| Crusher tóxico | 22 | 2 → 5 (máx. 2/rodada) | 5 | 4 | aura tóxica: tira o time do canto |

**Rodada especial — Matilha** (os "cães" do CoD): na rodada 5 a 7 (sorteio) e depois a cada 4 a 6
rodadas. Só Strikers com metade da vida e 1,3x a velocidade, 6 por jogador, nascendo de 1 em 1 s.
No fim, cai **Reabastecimento** onde o último morreu (o CoD dá munição no último cão).

**Chefe — Colosso** (Crusher tóxico grande): rodadas 10, 20, 30... Nasce quando metade da rodada já
nasceu, com **20x a vida da rodada** (rodada 10: ~21 mil), escala 1,35, no `info_trok_hordas_spawn`
marcado "chefe" (ou num normal). O Ataque aéreo não o mata; o Abate certo tira 10x o tiro.
Quem dá o último tiro ganha +1000; ele sempre solta Reabastecimento.

Ideias para o local/dono (não feitas): zumbi que rasteja (perna destruída), zumbi de colete (CA:
"pele"), rodada de neblina, chefe próprio com ataque de investida (precisa de modelo e animação).

---

## 6. Economia (pontos)

**Decisão:** pontos **próprios do Hordas**, separados do dinheiro da economia estilo CS
(`trok_economia`). Motivos: (1) a economia CS tem teto, bônus de vitória/derrota e zona de compra
— nada disso existe aqui; (2) aqui o ganho é por acerto e não por abate de jogador; (3) o
`hordas.cfg` desliga a economia, e os dois sistemas não se misturam no HUD nem no código.
Os pontos vão em rede no estado do modo (`trok_hordas_estado`, um número por jogador, 24 bits).

### Ganhos

| Ação | Pontos | Obs. |
|---|---|---|
| Começo | 500 | por jogador, ao entrar no mapa |
| Acerto em zumbi | 10 | por tiro que acerta (escopeta: 10 por disparo) |
| Abate no corpo | 50 | x o "pontos" do tipo |
| Abate na cabeça | 100 | |
| Abate com faca | 130 | |
| Tábua consertada | 10 | teto 500 por rodada por jogador |
| Ataque aéreo | 400 | para cada um |
| Reforço | 200 | para cada um |
| Chefe | +1000 | último tiro |
| Pontos em dobro | x2 | ganhos positivos, 30 s |
| Cair | -5% | |
| Morrer (sangrou) | -10% | volta na próxima rodada |

### Gastos (preços padrão: `scripts/trok_hordas_loja.txt`, `trok_upgrade.txt`, keyvalues do mapa)

| Item | Preço | Onde |
|---|---|---|
| Pistola | 500 (repor 250) | loja (`func_trok_loja`) |
| Submetralhadora | 1000 (repor 500) | loja |
| Escopeta | 1200 (repor 600) | loja |
| Fuzil | 1500 (repor 750) | loja |
| Munição de todas as armas | 1000 | loja |
| Munição de arma melhorada | 4500 (cada) | loja |
| Torreta | 1500 (repor 1500) | loja (seção 8A) |
| Claymore (2) | 1000 (repor 750) | loja |
| Mina (3) | 750 (repor 500) | loja |
| Carga remota (2) | 1000 (repor 750) | loja |
| Granadas (até 4) | 400 a reposição | loja |
| Colete (100 de armadura) | 1500 | loja |
| Kit de reanimação | 2500 (com energia) | loja (DECISÃO DO DONO, seção 9) |
| Armadilha (elétrica / fogo / arame) | 1000 / 1000 / 750 por uso (25–30 s) | painel `func_trok_armadilha` (seção 8B) |
| Caixa misteriosa | 950 (10 com Caixa barata) | `func_trok_caixa_misteriosa` |
| Melhoria MK I → MK V | 5000 / 10000 / 15000 / 20000 / 30000 | `func_trok_maquina_upgrade` |
| Nível de defesa I / II / III | 4000 / 9000 / 16000 | máquina, com uma arma de defesa na mão |

**Ritmo esperado (solo, estimativa grosseira):** ~9 mil pontos até a rodada 5, ~37 mil até a 10,
~95 mil até a 15 e ~190 mil até a 20 (os acertos rendem mais quando a vida sobe). Uma arma MK V
custa 80 mil no total: dá para fechar **uma** arma no máximo por volta da rodada 15 sozinho, menos
caixa, munição e defesas. Em 4 jogadores cada um ganha menos por zumbi, mas há mais zumbis. **MEDIR**
jogando e ajustar preços antes de mexer nas curvas.

---

## 7. Máquina de melhoria (o nosso "Pack-a-Punch", 5 níveis)

`func_trok_maquina_upgrade`: um lugar físico, precisa de **energia** (padrão, como no CoD). Melhora a
**arma que está na mão**, um nível por vez. O nível fica com aquela arma (trocar de arma da mesma
classe zera). Números em `scripts/trok_upgrade.txt`.

| Nível | Preço | Dano | Pente | Reserva | Efeito (chance por acerto) | Cor provisória |
|---|---|---|---|---|---|---|
| MK I | 5000 | x1,6 | x1,25 | x1,5 | — | cinza claro |
| MK II | 10000 | x2,5 | x1,5 | x1,75 | — | azul aço |
| MK III | 15000 | x3,8 | x1,5 | x2 | **atordoante** 15%: zumbi a 40% da velocidade por 2 s | violeta |
| MK IV | 20000 | x5,5 | x1,75 | x2,25 | **incendiária** 12%: queima 12% da vida máxima por s, 4 s | laranja |
| MK V | 30000 | x8 | x2 | x2,5 | **explosiva** 10%: estilhaços de 50% do dano em 140 u (só zumbis) | ouro velho |

- Cada nível enche o pente e a reserva. Munição de arma melhorada na loja custa 4500.
- "Efeito" é munição militar (atordoante/concussiva, incendiária, HE), não magia: cabe no "sem poder".
- **Camuflagem:** hoje só tinge a arma vista de fora (`cor`). O local faz: material/skin por nível no
  modelo de mundo e de primeira pessoa (5 camuflagens próprias, sóbrias).
- Comparando: CoD clássico = 1 nível; BO4 = 1 + 4 repetições (+25% cada); CW/BO6 = 3 níveis + 5
  raridades. O nosso: 5 degraus visíveis, uma moeda, um lugar.
- **Nível de defesa (I a III):** com uma arma de defesa na mão (torreta, claymore, mina, remota) a
  mesma máquina sobe o nível de defesa **do jogador**: x1,75 / x3 / x5 no dano de tudo o que ele põe no
  chão (bloco `defesa` do `trok_upgrade.txt`). Vale para o jogador, não para a arma (as armas de defesa
  acabam e se compram de novo), e zera quando ele morre.
- Futuro (DECISÃO DO DONO): escolher a munição especial no MK III em vez de ser fixa; tempo de
  espera na máquina (o CoD leva ~3 s e a arma fica na máquina).

---

## 8. Estações físicas e a LOJA (entidades de mapa, FGD em `game/mod_trok/trok.fgd`)

Todas são **pincéis** (o mapeador desenha o balcão, a caixa, a máquina) e usam **E**, como no CA.
Olhando a até 80 u aparece o aviso no meio da tela ("[E] LOJA", "E — MELHORAR FUZIL PARA MK II:
5000"), vermelho se faltam pontos. Saídas `OnUsada` e `OnSemPontos`; entradas `Ligar`/`Desligar`.

**A loja** (`func_trok_loja`, pedido do dono): uma ou poucas por mapa, num lugar que obrigue a
andar. E abre o **menu de compra do modo** (`client/trok/ui/trok_hordas_loja.cpp`, mesma base visual
do menu da economia, com os pontos do Hordas): categorias ARMAS, DEFESAS, EQUIPAMENTO (teclas 1–9),
clique compra, mostra "REPOR" para o que você já tem e "CHEIO"/"SEM ENERGIA" quando não dá. O menu
fecha sozinho se você se afasta (`trok_hordas_loja_distancia` 160 u), cai ou morre; E ou ESC fecham.
O servidor confere a distância de novo a cada compra (`trok_hordas_comprar <id>`). Catálogo em
`scripts/trok_hordas_loja.txt`.

| Entidade | Faz | Chaves |
|---|---|---|
| `func_trok_loja` | **loja**: E abre o menu (armas, munição, defesas, colete, kit) | `precisa_energia` |
| `func_trok_maquina_upgrade` | arma de fogo na mão: MK I a V; arma de defesa na mão: nível de defesa I a III | `precisa_energia` (padrão 1) |
| `func_trok_caixa_misteriosa` | paga, gira 4 s, oferece arma sorteada por 10 s só a quem pagou | `preco` (0 = 950) |
| `func_trok_armadilha` + `trigger_trok_armadilha` | **armadilha** do mapa (seção 8B) | `item` (eletrica/fogo/arame), `zona`, `preco`, `duracao`, `recarga` |
| `func_trok_energia` | liga a energia (uma vez) | — |
| `func_trok_grade` (do Surto) | **barricada de tábuas** da janela: zumbis quebram, E conserta 1 tábua (+10) | `vida`, `tabuas` (6), `inicio_fechada 1` |
| `info_trok_hordas_spawn` | nascedouro de zumbi (padrão: sempre ativo) | `area` (0), `chefe` |
| `trok_hordas_logica` | saídas OnRodadaInicio/Fim, OnEnergia, OnChefe, OnFimDeJogo, OnArea; entradas AbrirArea, LigarEnergia | — |
| `func_trok_compra_parede`, `func_trok_bancada`, `func_trok_porta_paga` | OPCIONAIS, fora do desenho padrão (o dono não quer parede nem porta paga) | — |

Fora do código ainda (o local faz): **caixa que muda de lugar** depois de N usos (o "ursinho"), modelo
da arma girando em cima da caixa, animação/tempo da máquina, prop bonito para cada estação e para a loja.

---

## 8A. Armas de defesa (o foco pedido pelo dono)

São as armas do CA que já existem no mod (`weapon_trok_explosivo` + `trok_projetil`, frente dos
explosivos): arremessa ou planta, o objeto fica no chão e age sozinho contra os zumbis (são inimigos
do dono). No Hordas elas se compram e se repõem na **loja** (categoria DEFESAS), também saem na
caixa misteriosa, e o **Reabastecimento** enche todas.

| Arma | Faz (números do CA, `scripts/weapon_trok_*.txt`) | Preço / repor | Carrega (`trok_max_*`) | Postas ao mesmo tempo (`trok_hordas_limite_*`) | Papel |
|---|---|---|---|---|---|
| Torreta | pousa, arma em segundos e atira sozinha até acabar as balas (100); toma 3x de explosão | 1500 / 1500 | 1 | 2 (fora do Hordas: 1, regra do CA) | segura uma janela sozinha por um tempo |
| Claymore | plantada, explode em cone quando o zumbi passa (ou no detonador) | 1000 / 750 | 2 | 4 | fecha corredor; ótima contra Striker |
| Mina | arremessada, gruda no chão, zumbi encosta e explode | 750 / 500 | 3 | 6 | campo minado na frente da janela |
| Carga remota | arremessada, explode no botão 2 | 1000 / 750 | 2 | 4 | "botão do pânico" quando a janela cai; contra o Crusher |
| Granada | a M67 do CA | 400 a reposição | 4 (CoD: 4) | — | +2 por rodada de graça |

- **Limite:** passou do limite de postas, a mais velha some (o mesmo jeito da torreta do CA).
- **Nível de defesa I–III** na máquina (seção 7): x dano de tudo o que o jogador põe.
- **Contra os tipos:** o Charger explode e leva as minas junto (cuidado onde ele morre); o Striker é
  rápido demais para a torreta sozinha (claymore resolve); o Crusher aguenta muito: remota.
- **Pontos:** abate de torreta, claymore e mina conta como do dono (mesmos pontos).
- Comandos: `trok_hordas_defesas` lista as postas e o nível de cada um.
- Para o local/dono (não feito): zumbi que **ataca a torreta** que o bloqueia (hoje bate só por acaso),
  torreta com mais balas só no Hordas, mostrar no HUD as defesas postas e as balas da torreta.

---

## 8B. Armadilhas do mapa (como as do CoD)

O mapa põe um **painel** (`func_trok_armadilha`) e uma **zona** (`trigger_trok_armadilha`, pincel
invisível). E no painel liga por X pontos durante Y segundos; depois a armadilha descansa. Quem estiver
na zona leva dano: zumbis muito, humanos pouco (cuidado ao passar). Abate na armadilha não dá pontos
(como no CoD; `trok_hordas_pontos_armadilha`). Cada jogador só pode ter **1 ligada** por vez
(`trok_hordas_limite_armadilha`). O HUD mostra "E — LIGAR ARMADILHA ELÉTRICA: 1000", "LIGADA" ou
"RECARREGANDO".

| Tipo (`item`) | Zumbi | Humano | Extra | Preço / tempo / recarga (padrão do mapa de teste) |
|---|---|---|---|---|
| Elétrica | mata na hora | 60/s | faíscas | 1000 / 25 s / 30 s |
| Fogo | 400/s | 25/s | — | 1000 / 25 s / 30 s |
| Arame eletrificado | 60/s | 5/s | zumbi a 35% da velocidade | 750 / 30 s / 20 s |

Bom lugar: corredor ou janela por onde a horda passa; o time "puxa" a horda para lá e liga na hora.

---

## 8C. Futuro (não fazer agora; pedido do dono)

- **Sacos de areia** e **barricadas posicionáveis** compradas na loja (bloqueiam, têm vida, zumbi
  bate nelas).
- **Arame farpado** posicionável (deixa lento e fere, sem precisar de painel).
- Holofote, caixa de munição para o time.
- Tudo isso sem "fase de construção": se pôr, põe andando, como a torreta.

Fora do código ainda (o local faz): **caixa que muda de lugar** depois de N usos (o "ursinho"), modelo
da arma girando em cima da caixa, animação/tempo da máquina, prop bonito para cada estação.

---

## 9. No lugar dos perks (humano não tem poderes) — DECISÃO DO DONO

| Perk do CoD | Substituto proposto | Estado |
|---|---|---|
| Juggernog (mais vida) | **Colete** na loja (100 de armadura) | feito |
| Quick Revive (reviver rápido / solo) | **Kit de reanimação** (loja, 2500, com energia): ao cair, levanta sozinho em 4 s; gasta o kit | feito, dono decide se fica |
| Speed Cola (recarga rápida) | nada (seria poder) — ou "carregadores táticos" como item | só ideia |
| Double Tap (cadência) | nada; o MK cobre o dano | — |
| Stamin-Up (correr mais) | colete leve da economia já dá velocidade/fôlego | ideia |
| Mule Kick (3ª arma) | "coldre extra" (item que libera 2ª principal) | só ideia |
| PhD, Deadshot, Electric Cherry, Widow's Wine | nada (são poderes) | fora |

Decidir: o kit de reanimação conta como "poder"? (É equipamento médico; no CW/BO6 existe como item.)

---

## 10. Derrubado e reviver

- Com a vida zerada o humano **cai** (não morre): fica com 1 de vida, rasteja a 35% da velocidade, só
  com a pistola, e **sangra por 30 s**. Zumbis **ignoram** derrubados (o CoD faz igual) e eles não
  levam dano.
- Um colega de pé **segura E por 3 s** perto (110 u) para reviver; volta com 50% da vida.
- O HUD mostra: no derrubado, "CAÍDO — 23" e a barra de quem o revive; nos outros, uma cruz vermelha
  piscando sobre o derrubado com a distância e "SEGURE E — REVIVER fulano".
- Sangrou até o fim: morre, perde 10% dos pontos, o kit e as melhorias; volta na próxima rodada com a
  pistola. Todos caídos ou mortos: **fim de jogo**.
- A vida volta sozinha (40/s depois de 4 s sem dano), como no CoD.
- Da Equipe Armada do CA ficou de fora o sistema de **vidas por objetivo** (LifeCount): o reviver do
  CoD é mais cooperativo. Opção futura: "modo CA" com N vidas por partida (DECISÃO DO DONO).

---

## 11. Dificuldade e escala com jogadores

`trok_hordas_dificuldade 0/1/2` (normal, difícil, extremo — os 3 da Equipe Armada):

| | Normal | Difícil | Extremo |
|---|---|---|---|
| Vida dos zumbis | x1 | x1,2 | x1,5 (fator do CA) |
| Dano dos zumbis | x1 | x1,25 | x1,6 |
| Zumbis por rodada | x1 | x1,15 | x1,3 |

Escala com jogadores (1 a 4+): a **quantidade** sobe com a fórmula do CoD (+6 por jogador e por
fator de rodada), os **vivos ao mesmo tempo** sobem 4 por jogador. A vida não sobe com gente (CoD);
para o jeito do CA (+0,3 x log2(jogadores)) basta `trok_hordas_vida_jogadores 0.3`. Com mais de 4
jogadores a fórmula continua (6 por jogador), limitada pelas vagas do servidor.

**Limite técnico:** cada zumbi é um jogador bot. `maxplayers 32` com 4 humanos dá 27 zumbis; o
diretor deixa 1 vaga livre e nunca passa do que cabe. Recomendo `maxplayers 32` para o modo (o menu
JOGAR hoje usa `bots por time x 2 + 2`; o local ajusta para o Hordas).

---

## 12. O mapa (sem portas pagas, sem parede de arma)

- O mapa é aberto desde a rodada 1. A progressão vem de **comprar e melhorar** (loja → caixa →
  máquina) e de **montar as defesas** (torretas, claymores, minas, armadilhas).
- **Uma loja** (ou duas em mapa grande) longe do começo: ir comprar é sair do canto seguro.
- A **energia** longe do começo (perto de uma janela perigosa); liga a máquina e o que o mapa marcar
  com `precisa_energia` (o kit de reanimação, uma armadilha).
- **Armadilhas** em corredores e janelas por onde a horda passa.
- **Cantos seguros** de propósito: 1 ou 2 lugares com poucas entradas, bons para torretas e claymores,
  mas longe da loja (o time precisa sair para repor).
- **Janelas com tábuas** nas salas, nascedouros atrás delas. Medidas do CA em
  `design/medidas-mapas.md` (porta 64 x 128; corpo 100; janela de 128 x 128 para zumbi passar).
- **Mapa de teste:** `tools/mapas/gerar_hordas_teste.py` gera `game/mod_trok/mapsrc/trok_hordas_teste.vmf`
  (2 salas ligadas por passagem livre; loja na sala 2; caixa; energia; máquina; armadilha elétrica na
  passagem e arame eletrificado na janela oeste; 2 janelas com barricada; 7 nascedouros). Compilar
  com o MCP `trok` (`mapa_compilar`). **Nav:** antes de `nav_generate`, abrir as barricadas:
  `ent_fire func_trok_grade Abrir; nav_generate`.
- Mapas do CS:S também rodam (sem estações): os zumbis nascem nos pontos de nascimento do mapa,
  longe dos jogadores. Serve para testar a curva e os bots.

---

## 13. Bônus que caem (os "power-ups")

Chance de 2,5% por abate, no máximo 4 por rodada (CoD: 4). Ficam 25 s no chão girando e piscam nos
últimos 5 s. Pega quem encostar (humano de pé). Pesos em `trok_hordas.txt`.

| Bônus | Faz | CoD |
|---|---|---|
| Reabastecimento | reserva cheia de todas as armas de todos (o pente fica) | Max Ammo |
| Abate certo | 30 s: um acerto derruba qualquer zumbi (chefe leva 10x) | Insta-Kill |
| Pontos em dobro | 30 s: ganhos x2 | Double Points |
| Ataque aéreo | mata todos os zumbis vivos (menos o chefe), +400 para cada um | Nuke |
| Reforço | todas as barricadas inteiras, +200 para cada um | Carpenter |
| Caixa barata | 30 s: a caixa custa 10 | Fire Sale |

Modelo provisório: caixa de munição do HL2 tingida por bônus. O local troca por modelo próprio.

---

## 14. Fim de jogo, placar e HUD

- **HUD do modo** (`client/trok/hud_trok_hordas.cpp`, arquivo próprio): número da rodada grande em
  âmbar embaixo à esquerda ("RODADA"; vermelho em rodada especial); no alto, "RODADA 7 · 23 ZUMBIS"
  ou "RODADA 8 EM 6"; pontos de cada humano à direita (o seu em âmbar, "+10" subindo); aviso da
  estação; derrubados; bônus ativos com tempo; faixa do bônus pego; "MELHORIA MK III" da arma na mão.
- No `hud_trok.cpp` (HUD principal) só dois ganchos: o placar de times do topo some no Hordas e a
  tela de morte diz "VOCÊ VOLTA NA PRÓXIMA RODADA".
- **Fim de jogo:** faixa "FIM DE JOGO — RODADA N" e o recorde do mapa.
- **Placar (TAB):** hoje mostra abates/mortes do HL2MP. Falta (local/frente da interface): colunas
  PONTOS, ABATES, REVIVEU, CAÍDAS (os pontos e abates já estão em rede no `trok_hordas_estado`).
- **Tela de resultado** no fim (quem fez mais pontos, mais cabeças, mais reviveu): ideia já oferecida
  ao dono e não pedida.

---

## 15. Bots aliados

`trok_hordas_aliados N` (padrão 0): o diretor coloca N bots humanos quando há alguém jogando. Eles
usam o cérebro do Surto (segurar lugar difícil, recuar atirando). **Não** compram, não consertam
tábuas e não revivem ainda (o local faz: ação de reviver derrubado perto, consertar janela da área,
ir à loja repor munição). Os zumbis usam o cérebro de zumbi do Surto (caçar o humano mais perto,
quebrar barricada, cercar por rotas diferentes).

---

## 16. Como está feito (código) e como testar

| Parte | Arquivo |
|---|---|
| Estado em rede, níveis da máquina, ajudas (pente, velocidade) | `src/game/shared/trok/trok_hordas.{h,cpp}` |
| Estações (uma classe em rede para todas: loja, máquina, caixa, armadilha, energia...) | `src/game/shared/trok/trok_hordas_estacao.cpp` |
| Menu da loja | `src/game/client/trok/ui/trok_hordas_loja.cpp` (catálogo: `scripts/trok_hordas_loja.txt`) |
| Limite de defesas postas por jogador | gancho em `src/game/server/trok/trok_projetil.cpp` (`TrokHordas_LimitePostos`) |
| Diretor: rodadas, grupo de bots, pontos, derrubado, bônus, efeitos | `src/game/server/trok/trok_hordas_diretor.cpp` |
| Nascedouros e lógica do mapa | `src/game/server/trok/trok_hordas_mapa.{h,cpp}` |
| Barricada de tábuas (no `func_trok_grade`) | `src/game/server/trok/trok_surto_mapa.cpp` |
| HUD | `src/game/client/trok/hud_trok_hordas.cpp` |
| Ganchos | `trok_gamerules.cpp` (construtor, Think, PlayerSpawn, PlayerKilled, dano, renascer, armas, comandos), `trok_surto.{h,cpp}` (`TROK_MODO_HORDAS`, `TrokModoZumbi_Ativo`; `TrokSurto_IsZumbi/IsHumano` valem no Hordas e o derrubado não conta como humano), `trok_zumbis.{h,cpp}` (`TrokZumbis_DefinirClasse`, velocidade da rodada), `weapon_trok_gun.{h,cpp}` (`GetMaxClip1` com a melhoria), `hud_trok.cpp` (2 ganchos) |
| Números | `cfg/hordas.cfg`, `scripts/trok_hordas.txt`, `scripts/trok_upgrade.txt`, sons em `scripts/game_sounds_trok.txt` (`Trok.Hordas.*`, provisórios do HL2) |
| Menu | `scripts/trok_modos.txt` (cartão HORDAS) |

Como funciona por dentro:
- **Zumbis = bots NextBot** do HL2MP no time dos zumbis, criados pelo diretor com nome "Infectado NN"
  num grupo reaproveitado: zumbi morto espera 1,5 s e volta como outro (tipo, vida, velocidade, lugar
  novos). O grupo cresce até vivos máximos + 4. Na troca de mapa, os que sobram voltam ao grupo (ou
  saem do servidor se o modo mudou). Escolhi bots e não NPCs porque o cérebro de zumbi, as garras,
  os tipos e o movimento do CA já funcionam para jogador (Surto); NPC NextBot exigiria refazer tudo.
- **Dano da melhoria:** o motor aplica o dano da arma; logo depois (gancho `OnPlayerDamaged`) o
  diretor tira da vida o dano a mais (`dano x (nível - 1)`), o Abate certo e os efeitos. O motor vê a
  vida <= 0 e mata. Não mexe no código das armas.
- **Derrubado:** no mesmo gancho, humano com vida <= 0 volta a 1 e cai (o motor não mata).
- **Tipos:** guardados na entidade do Surto (`trok_surto_estado`), que o Hordas cria desligada.

Comandos (cheats, para testar): `trok_hordas_info` (tabela; sem cheat), `trok_hordas_defesas`,
`trok_hordas_loja_lista`, `trok_hordas_comprar <id>` (só perto da loja), `trok_hordas_rodada N`,
`trok_hordas_pontos N`, `trok_hordas_dar_bonus 0..5`, `trok_hordas_ligar_energia`,
`trok_hordas_recarregar`, `trok_hordas_upgrade_recarregar`.

Checado na nuvem: `trok.py check` (sintaxe e tipos, servidor e cliente). **Não** checado: link,
jogo rodando, mapa compilando, bots andando.

---

## 17. O que o Claude local termina

1. Compilar, abrir com `trok_modo hordas`, `maxplayers 32`, `map de_dust2`: rodadas, bots nascendo
   longe e reaproveitados, HUD, pontos, fim de jogo e recomeço. Ver no console avisos de som/modelo.
2. Compilar `mapsrc/trok_hordas_teste.vmf` (MCP `mapa_compilar`), gerar o nav com tudo aberto, e
   testar cada estação: loja (E abre o menu, compra, repor, "cheio", fecha ao se afastar; compra
   longe recusada), energia, máquina MK I→V (pente, cor, efeitos) e nível de defesa I→III, caixa
   (giro, oferta, só o dono pega), armadilhas (liga, mata, fere humano, recarga, limite 1 por
   jogador), barricada (zumbi quebra, E conserta, +10, teto 500).
2b. Armas de defesa: torreta matando zumbi e dando pontos ao dono, limite de 2 (a terceira some a
   mais velha), claymore/mina/remota no cone e no raio, nível de defesa multiplicando o dano,
   Reabastecimento enchendo, zumbi bot preso em torreta (se precisar, gancho no cérebro do zumbi).
3. Conferir que o `E` do motor acha as estações (`sv_debug_player_use 1`) e que o aviso do HUD bate.
4. Derrubado: zumbis ignorando, rastejar, reviver segurando E, sangrar, kit, fim de jogo com todos
   caídos. Ver se a troca de arma derrubado precisa ser travada.
5. Bots: zumbi bot respawnando pelo diretor sem "renascer sozinho"; bots presos em janela; Crusher
   grande passando na janela de 128; chefe no lugar dele.
6. Medir e ajustar preços e curvas (seções 4, 6, 7) jogando solo e em 2–4.
7. Placar (TAB) com PONTOS/ABATES/REVIVEU (falar com a frente da interface); cartão do menu com
   `maxplayers 32` e mapa padrão do Hordas.
8. Camuflagens por nível, modelo dos bônus, modelos/props das estações, sons próprios (trocar os do HL2).
9. Bots aliados: reviver, consertar tábuas, comprar munição.
10. Série de abates no Hordas: hoje conta zumbi como abate (vai ter muito "ABATE MÚLTIPLO"); decidir
    com o dono se desliga no modo.

## 18. O que o dono precisa decidir

1. **Kit de reanimação** fica (é equipamento) ou sai (parece poder)? E os outros substitutos dos perks
   (seção 9).
2. Os **5 níveis** como estão (dano + pente + munição especial no 3, 4 e 5)? Munição especial fixa por
   nível ou escolhida na máquina?
3. **Uma moeda só** (pontos) ou também sucata/raridade como o CW/BO6?
4. **Reviver** como no CoD ou **vidas** como na Equipe Armada?
5. Nomes: "Hordas", "Matilha", "Colosso", "MK I..V" e os nomes dos bônus.
6. Ter **exfiltração**/vitória (sair vivo numa rodada X) ou só sobreviver o máximo?
7. Armadilhas pagas (choque/fogo do mapa) e caixa que muda de lugar: fazer?
8. Armas de defesa e armadilhas: preços e limites das seções 8A e 8B; torreta com mais balas no
   Hordas; quando fazer os itens do futuro (seção 8C).
9. Loja: uma só por mapa ou uma por sala grande? O que vende (hoje: 4 armas, munição, 5 defesas,
   colete, kit)?

---

Fontes consultadas (números do CoD só como referência):
- Curva de vida e quantidade: [The Math of COD Zombies](https://jackhanke.github.io/posts/zombies),
  [Zombie charts (callofdutyzombies.com)](https://callofdutyzombies.com/topic/137395-zombie-charts).
- Pack-a-Punch no BO4 (até 4 repetições, +25%): [Charlie Intel](https://www.charlieintel.com/call-of-duty/pack-a-punch-your-weapon-in-zombies-will-increase-its-damage-48542/),
  [ComicBook](https://comicbook.com/gaming/news/call-of-duty-black-ops-4-zombies-how-to-make-weapons-stronger).
- Raridade e Pack-a-Punch no BO6: [Game8](https://game8.co/games/Call-of-Duty-Black-Ops-6/archives/472213),
  [Weapon Rarities (wiki BO6)](https://cod-black-ops-6-zombies.upcomer.com/wiki/Weapon_Rarities),
  [SteelSeries: Pack-a-Punch Liberty Falls](https://steelseries.com/blog/how-to-pack-a-punch-on-liberty-falls-in-black-ops-6-zombies-1210).
- Vanguard (níveis de arma): [Gfinity](https://www.gfinityesports.com/call-of-duty-vanguard/zombies-weapon-tiers/).
- Equipe Armada do CA: `design/decombat/specs/FIRETEAM_2009.md` (vidas, dificuldade, tabelas de
  nascimento com máximo vivo), `design/chimera/specs/combat_arms/game_modes.md` M19 e `bots.md` §11
  (fator 1,0/1,2/1,5 e +0,3·log2 jogadores); tipos em `design/ca-pesquisa/mutagenicos.md`.

# Combat Arms: números de jogabilidade (pesquisa de 10/11/2026)

## Conferido nas páginas inteiras da CAWiki (combatarms.fandom.com, lidas com a rede liberada)
- **Tabela de armas:** `armas-wiki.tsv` traz 609 fichas (Reloaded e Classic) com dano, mobilidade, cadência,
  precisão, controle, alcance e munição. As escalas são as do jogo: no Reloaded a mobilidade vai de 0 a 100 e no
  Classic é peso; a cadência é a escala nova (250 a 333 nos fuzis).
- **Dano por parte do corpo:** tronco = dano cheio; braço e perna = cerca de 10% a menos; cabeça = cerca de 5× (a
  página Headshot diz 6×), e normalmente mata. Capacete reduz.
- **Recuo** (estatística do jogo): quanto maior, mais rápido a dispersão cresce a cada tiro. Padrões: para cima,
  de lado, diagonal ou aleatório, conforme a arma.
- **Portabilidade:** as pistolas têm 90 (exceto Desert Eagle e afins); quase todas as escopetas têm a mesma.
- **Proteção ao nascer:** 3 s. No One Man Army acaba ao atirar (igual ao nosso `trok_protecao`).
- **Quarantine Regen:**
  - rodadas: 5, 7 ou 9; 2:40 por rodada; 20 s de preparo;
  - jogadores: de 8 a 16 (mínimo de 4 para começar);
  - hospedeiros: 2 com menos de 12 jogadores, 3 com mais;
  - regeneração: 10% da vida a cada 3 s parado (outra página diz 10% por segundo; o Crusher recupera 20%);
  - fim de rodada: zumbis eliminados (+20 para cada sobrevivente); tempo acabou (+20 para cada sobrevivente e −1
    para cada infectado); todos infectados (+3 para cada infectado restante); humano ganha 3 por infectado morto;
  - vida pela tabela de colete: hospedeiro de 200 a 300+ e infectado de 135 a 200+; o colete do infectado fica sempre
    em 100 e não esvazia.
- **One Man Army:** meta de 15, 30 ou 45, sem limite de tempo, de 2 a 16 jogadores.
- **Elimination:** meta de 30 a 140 (máximo de 100 em partida de clã), de 1x1 a 8x8.
- **Spy Hunt:** o Super Spy tem 500 de colete.


Tudo veio de **trechos de busca**. As páginas inteiras (combatarms.fandom.com, strategywiki.org) não abriram nesta rede.
Os números de física e de cone e recuo de 2009 estão em `design/decombat/`. Este arquivo complementa com dados por arma e
por modo.

**Confiança:**
- **W:** wiki de fã.
- **W-antigo:** StrategyWiki de 2008 a 2010, com dados do site oficial antigo.
- **I:** imprensa ou anúncio.
- **E:** estimativa de jogador.
- **Inferência:** dedução.

**Escalas do CA:**
- As fichas de arma mostram **barras de 0 a 100**.
- A barra de **Dano** é aproximadamente o dano por tiro contra 100 de vida: 100 ou mais mata com 1 tiro, 50 ou mais com 2. O colete costuma somar mais um tiro (M4A1, com 35, era "4 tiros"; AK, com 48, era "3 tiros").
- A barra de cadência do site antigo "não tem significado físico conhecido".
- As fichas novas (Reloaded/Classic) usam outra escala, sem documentação: cadência de 250 a 333, Peso de 8 a 14 e Alcance 2250.

## 1. Armas
| Arma | Dano | Cadência | Precisão | Recuo | Portab./Peso | Munição | Nota | Conf. |
|---|---|---|---|---|---|---|---|---|
| M4A1 | 35–36 | 285 (nova) | 66 | 23 | peso 11 | 30/90? | 4 tiros; pouca queda com distância | W |
| AK-47 | 48 | 250 (nova) | 48 | 43 | peso 14 | — | 3 tiros | W |
| G36E | 45 | 68 | — | — | 57 | — | luneta embutida, equilibrada | W |
| SCAR-L | 40 | 70 | 85 | 66 | 57 | 30/90 | recuo baixo | W |
| FAMAS | 36 | 300 (nova) | 67 | — | mobilidade 90 | — | | W |
| MP5A4 | 33 | 85 | 67 | 63 | 75 | 30/90 | 4 tiros | W |
| MP5SD6 | 35 | 333 (nova) | 75 | 25 | peso 8 | 30/90 | alcance 2250 | W |
| UMP45 | 38 | 79 | 67 | 63 | 75 | 35/105 | | W |
| P90 | 35 | 82 | 67 | 72 | 73 | 50/50 | mira reflex; recarga rápida | W |
| MG36 | 49 | 80 | 63 | 67 | 36 | 100/100 | bipé | W |
| SR-25 | 72–75 | 25–38 | — | — | 40–43 | 10/30 | semiautomática | W |
| L96A1 | 100–160 (real ≥129) | 10–25 | — | — | 40 | 5/15 | 1 tiro; braço ou pé a distância extrema pode não matar | W |
| R870 | 75 / 94 (antigo) | 17–19 | 85 | — | 65–86 | 7/21–28 | | W |
| SPAS-12 | 65–75 | 20–27 | 94 | 62 | 63–86 | 8/24 | 2 a 3 tiros conforme a distância | W |
| Saiga 20K | 67 | 23 | — | 62 | 66 | — | | W |
| Desert Eagle | 70 / 75 (antigo) | 11 | 61 | 66 | 85 | 7/7 | | W |
| M1911 MEU | 42 | 39 | — | — | 95 | 7/14 | | W-antigo |
| Glock G18 | 24 | 59 | — | — | 95 | 15/30 | rajada e semi | W-antigo |
| Faca M11 | 42 | 2º golpe mais rápido | — | — | — | — | corte e estocada | W |
| Tracker Knife | 55 (outro guia: 70–80) | — | — | — | — | — | estocada pelas costas mata com 1 golpe | W |

**Faltam:**
- **Armas:** Vector, M249, Barrett, M16A3/A4, Tavor (no CA é a MTAR-21).
- **Números:** cadência real em tiros por minuto, tempo de recarga, queda de dano por distância.

## 2. Jogador
| Item | Valor | Conf. |
|---|---|---|
| Vida | 100 | Inferência / W-antigo |
| Colete (AP) | absorve dano de bala até um limite; quase não protege de explosivo, faca, cabeça e costas | W |
| Coletes (AP / velocidade) | Light −10% / +10%; Medium 0 / 0; Heavy +15% / −10%; Commando +20% / −9%; Recon −10% / +15% | W |
| Cabeça | "qualquer dano na cabeça passa de 100" (cabeça mata); o capacete reduz o dano na cabeça em 20% | W |
| Velocidade | depende da portabilidade da arma na mão e do % do colete; portabilidade alta também dá mais fôlego | W |
| Fôlego | barra que esvazia correndo; itens aumentam o máximo, nada acelera a recarga | W |
| Proteção ao nascer | 3 s | W |
| Renascer | cerca de 3 s (o kill cam) | W-antigo |
| Jogadores | 8x8 por equipe; 16 sem equipe | W-antigo |

## 3. Modos
| Modo | Regras | Conf. |
|---|---|---|
| Elimination (mata-mata em equipe) | meta de 30 a 140 de 10 em 10, escolhida na sala; sem limite de tempo | W |
| One Man Army | meta de 30, 35, 40 ou 45 (a mais comum era 45, no Junk Flea com 16 jogadores); renasce em segundos | W / E |
| Elimination Pro | 1 vida; rodada de 3 min; 5, 7, 9 ou 11 rodadas | W / E |
| Search & Destroy | rodada de 3 min; bomba explode 40 s depois de plantada | W |
| Capture the Flag | meta de 3, 5 ou 7; desempata por capturas e depois por recuperações | W-antigo |
| Spy Hunt | 5 maletas; quem pega fica 3 s invulnerável e recupera vida, colete e munição; quem junta as 5 vira Super Spy e começa a fase 2; o transmissor cai do céu em ~10 s e esmaga quem estiver embaixo; o Super Spy vence transmitindo ou matando todos | W |
| Last Man Standing | até 16 jogadores; 3, 5, 7 ou 9 rodadas | I |
| Arms Race (como o Gun Game) | de 2 a 16 jogadores | W |
| Fireteam | até 8 humanos contra a IA; missão em 5 etapas; quem morre volta quando alguém passa o checkpoint; ondas de inimigos fracos, guardas, granadas, armadilhas e tanques | W / I |

### Quarantine / Quarantine Regen (referência do Surto)
| Item | Valor | Conf. |
|---|---|---|
| Tempos | 20 s de preparação + 2:40 de rodada | W (**já é o nosso `surto.cfg`**) |
| Zumbis iniciais (hospedeiros) | de 1 a 3: 2 com menos de 12 jogadores, 3 com mais de 12 | W |
| Infecção | um golpe de perto infecta | W |
| Vida hospedeiro / infectado (pelo colete do humano) | Ultralight 200/135, Light 225/150, Combat 300/200, Heavy 350–375/235–250, Commando 400/270; Crusher 450/305 | W (páginas divergem) |
| Regeneração | parado por 3 s, recupera 10% da vida por segundo (o Crusher, 20%) | W / I |
| Pontos | humano: 3 por infectado morto, +20 se sobreviver; infectado: 1 por infecção | W |
| Abrigos (Overdose) | as portas dos abrigos só abrem faltando 1 min; podem ser trancadas e quebradas | W (**já é o nosso desenho**) |

**Atenção, Fase 3:**
- **Vida:** o `cfg/surto.cfg` usa 2500 para o hospedeiro e 1500 para o infectado. Pela wiki, no CA eram de 200 a 450 e de 135 a 305, porque a vida de 100 do humano era a escala.
- **Regeneração:** a nossa é de 10% a cada 3 s; a da wiki é de 10% por segundo.
- **O que fazer:** decidir com o dono e medir no CA; o empurrão das armas também conta.

## 4. Mapas
| Mapa | Tipo | Destaques | Conf. |
|---|---|---|---|
| Junk Flea | o menor; ferro-velho | "the corner" (sucata embaixo do guindaste); caixa quebrável para subir; metal faz a bala ricochetear | W |
| Death Room | menor ainda; interno | combate muito próximo | W |
| Two Towers | grande, com mar | cair na água mata; muitas janelas | W |
| Snow Valley | o maior da época | 3 pontas, trincheira e instalação subterrânea | W-antigo |
| Overdose | médio, interno | primeiro mapa de Quarantine, com abrigos | W |
| Desert Fox | Fireteam | inimigos sem parar, com limite de tempo | W / I |

## 5. Série de abates e explosivos
- **Série de abates:** não havia recompensa por série no CA original nem no Reloaded, só "Accolades" (destaques do fim da partida). A nossa killstreak é original: as recompensas ficam com o dono.
- **Granada M67:** dano 75, explode 5 s depois de lançada, pode cair ao morrer.
- **Claymore:** dano 80, detonação remota, leque de 180° à frente.

## Fontes
StrategyWiki: Combat_Arms/Weapons, Assault_Rifles, Submachine_guns, Sniper_rifles, Shotguns, Pistols, Machine_guns, Support,
Bodywear, Modes, Gameplay, Junk_Flea, Snow_Valley.

combatarms.fandom.com:
- **Armas:** M4A1, AK-47, Assault_Rifle, SCAR-L, FAMAS, Guide:GP_Assault_Rifles_Overview, MP5A4, MP5SD6_Steampunk, UMP45, P90, MG36,
  Machine_Gun, L96A1, Sniper_Rifle, SR25, R870, SPAS-12, SAIGA_20K_CAMO, Desert_Eagle, M11_Tactical_Knife, Tracker_Knife,
  Guide:Comprehensive_Melee_Guide.
- **Explosivos:** M67_Frag, Claymore.
- **Jogador:** Headshot, Combat_Helmet, Armor_Points, Vest, Portability, Jogging, Stamina, Spawn_Shield.
- **Modos:** Elimination, Elimination_Pro, One_Man_Army, Search_and_Destroy, Bomb, Arms_Race, Fireteam, Accolades, Grinding,
  Quarantine, Quarantine_Regen, Infected_Hide, The_Striker, The_Crusher, Calloused.
- **Mapas e atualizações:** Warehouse, 3/20/13_Content_Update.

Outras: breezewiki (Two_Towers, Death_Room, Desert_Fox); Blues News e MMOHuts (Last Man Standing); Ten Ton Hammer
(Quarantine); Gaming Nexus e Co-Optimus (Fireteam); IMFDB (Tavor); Steam 1263550 e 905640.

# Modo Surto (sucessor da quarentena do Combat Arms)

"Surto" é o nome de trabalho (a quarentena é o nome original do Combat Arms; trocável). Pedido do
dono (09/11/2026): mesmas regras da quarentena, tiros que seguram e empurram conforme a arma, e
bots prontos e espertos para o modo. Tudo foi feito na nuvem e **nada foi testado no jogo**.

## Regras (o que se sabe da quarentena e como ficou no Trok)

| Regra | Combat Arms (fonte) | Trok (ConVar em `cfg/surto.cfg`) |
|---|---|---|
| Preparo | 20 s, todos humanos (wiki da Quarantine Regen) | `trok_surto_preparo 20` |
| Rodada | 2:40 depois do preparo (wiki) | `trok_surto_tempo 160` |
| Hospedeiros | 1 a 3, conforme o número de jogadores (wiki; a regra exata diverge) | 1 a cada `trok_surto_jogadores_por_hospedeiro` (8), até `_hospedeiros_max` (3), sempre sobra humano |
| Infecção | o infectado ataca corpo a corpo e infecta o humano (PCInvasion, 2009) | só o golpe de garra infecta: o humano levanta zumbi onde caiu, em `trok_surto_virar_tempo`; outras mortes tiram da rodada (espectador); gás só fere |
| Vencer | humanos: sobreviver até o fim ou destruir todos os infectados; infectados: infectar todos (wiki) | igual; zumbi morto fica morto (`trok_surto_zumbi_renasce 0`) |
| Recuperação | 10% da vida a cada 3 s parado, para ao se mexer ou levar dano (wiki; outra fonte diz 1 s) | `trok_surto_regen*` |
| Crítico | infectado não leva crítico (PCInvasion) | `trok_surto_zumbi_sem_critico 1` |
| Vida / fôlego | mais vida e o dobro de fôlego (PCInvasion) | vida por ConVar; `trok_surto_zumbi_folego 2` |
| Alarme / abrigos | relato do dono (mapa estilo "Overdose"): alarme faltando 1:20, abrigos abrem faltando 1:00, humanos trancam grades, zumbis quebram | `trok_surto_alarme 80`, `trok_surto_abrigos 60`, entidades abaixo |
| Tiro segura e empurra | pedido do dono | campos `empurrao` e `segurar` no bloco "Trok" de cada arma (escopeta segura mais) |

Números marcados MEDIR no `cfg/surto.cfg` devem ser conferidos no jogo de referência.

Fontes (a rede da nuvem bloqueia o wiki; resumo vindo da busca):
- https://combatarms.fandom.com/wiki/Quarantine_Regen
- https://pcinvasion.com/?p=8774

## Como funciona no código

- `shared/trok/trok_surto.*`: estado da rodada em rede (`trok_surto_estado`: fase, fim da fase,
  vencedor, alarme, abrigos, contagens). Humanos = time Rebeldes, zumbis = Combine (sem matar
  ninguém na troca; o HUD mostra cores e nomes próprios).
- `shared/trok/trok_gamerules.cpp` (servidor): diretor da rodada (preparo → sorteio → jogo →
  fim → mapa limpo e rodada nova), virar zumbi, regeneração, itens (zumbi só com garras), queda
  sem dano para zumbi, sem troca de time manual, e o empurrão/segurar das balas
  (`OnPlayerDamaged`, chamado pelo patch em `player.cpp`).
- `shared/trok/weapon_trok_garras.cpp`: as garras (arma branca), `trok_surto_garra_*`.
- `shared/trok/trok_gamemovement.cpp`: zumbi pula mais (`trok_surto_zumbi_pulo`); a velocidade
  vem das garras (`trok_surto_zumbi_velocidade`).
- `server/trok/trok_surto_mapa.cpp`: entidades de mapa (FGD em `game/mod_trok/trok.fgd`):
  - `trok_surto_logica`: saídas OnPreparo, OnInicio, OnAlarme, OnAbrigos, OnHumanosVencem,
    OnZumbisVencem (ligar portas de abrigo, luzes, sirenes);
  - `func_trok_grade`: humano fecha/abre com USAR (por padrão só depois dos abrigos), zumbi
    quebra batendo (`vida`), aberta fica translúcida;
  - `info_trok_abrigo`: centro do abrigo para os bots.
- HUD (`client/trok/hud_trok.cpp`): relógio que pisca no alarme, humanos x zumbis vivos, aviso
  de preparo/alarme/abrigos, vencedor, HUD de zumbi, "INFECTOU", tela de "infectado por".
- Menu: "Jogar" tem a escolha do modo (põe `trok_modo surto` antes do mapa).

## Bots

- **Zumbi**: persegue humano à vista (com antecipação), golpeia de perto, pula atrás de quem
  está em caixa, quebra grade fechada no caminho. Sem ninguém à vista, vai na direção aproximada
  do humano mais perto (`trok_bot_zumbi_faro`, erro de ~300u). Cada zumbi tem preferência de
  rota própria: chegam por lados diferentes (cerco). Preso: tenta outra rota.
- **Humano**: antes dos abrigos segura lugares difíceis de alcançar (mapa tático); quando os
  abrigos abrem, vai ao `info_trok_abrigo` mais perto e fecha a grade quando há zumbi vindo. Em
  luta, recua atirando, troca de lado e pula quando o zumbi encosta (`trok_bot_recuo_dist`).
- Bot que vira hospedeiro vivo recomeça o cérebro como zumbi.
- Humano trancado num abrigo: os zumbis bots esperam juntar `trok_bot_zumbi_grupo` (3) a uma
  distância da grade, até `trok_bot_zumbi_grupo_espera` (10 s), e atacam juntos.

## Placar e rodada

- TAB com K/D (abates / mortes) como no Combat Arms; no Surto, mais INFECÇÕES e ZUMBIS
  (abatidos) por jogador, no mapa todo, e os lados com nomes fixos (HUMANOS / INFECTADOS).
- Fim da rodada: destaque (quem mais infectou, se os zumbis venceram; quem mais abateu zumbis,
  se os humanos venceram). Sobreviventes da rodada contam em `m_aSobreviveu`.
- Morto que não vai levantar zumbi passa a assistir os vivos (câmera atrás de alguém do próprio
  time) depois de `trok_surto_assistir` segundos.
- Zumbi correndo gasta metade do fôlego (`trok_surto_zumbi_folego 2`).

## Como testar (local)

1. Mapa com nav mesh (`nav_generate`). Sem abrigos e grades o modo funciona, só sem essa parte.
2. Menu > Jogar > SURTO, ou no console: `trok_modo surto; map <mapa>`.
3. Conferir: sorteio aos 20 s, humano morto levanta zumbi, contagens no topo, alarme e
   abrigos nos tempos, vencedor e rodada nova com o mapa limpo, empurrão de cada arma.
4. Mapa de teste com abrigo: um `info_trok_abrigo`, uma `func_trok_grade` na entrada e um
   `trok_surto_logica` com OnAbrigos abrindo a porta do abrigo.

## Pendente

- Modelo, animações e sons de zumbi próprios (hoje: modelo do Combine com tom verde, faca do CS:S
  como garras, sons do HL2). Ver `design/conteudo-proprio.md`.
- Habilidades de infectado, pontos/XP por rodada (progressão).
- Mapa no estilo do Overdose com abrigos e grades (Hammer, `design/medidas-mapas.md`).
- Bot humano usando grade sem abrigo marcado; zumbi esperando agrupar antes de atacar abrigo.
- Depois: modo zumbis no estilo CoD (PvE em rodadas), que é outro modo.

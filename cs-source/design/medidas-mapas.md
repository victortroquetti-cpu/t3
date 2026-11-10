# Medidas para mapas (Hammer)

**Escala exata do Combat Arms** (decisão do dono, 10/11/2026), com o movimento do CA ligado
(`trok_mov_ca 1`, `cfg/movimento.cfg`). Unidades 1:1 com as do CA: o HUD do CA conta **40u = 1 m**.
Fontes: `design/chimera/specs/combat_arms/movement.md`, `design/decombat/specs/MOVEMENT_2009*.md`.
Valores com ★ vêm de conta e precisam ser medidos no jogo.

## Jogador

| Medida | Valor | De onde |
|---|---|---|
| Caixa em pé | 30 × 30 × **100** | `g_TrokCAViewVectors` × `trok_ca_corpo_escala` (100/72) |
| Caixa agachado | 30 × 30 × **80** | idem (no CA agachar baixa só 20%) |
| Olho em pé / agachado | **83 / 60** | idem |
| Boneco | os modelos de 72u do HL2/CS:S ganham escala 1,39 (= 100u) | `SetModelScale` ao nascer |
| Degrau (sobe andando) | **30** | `sv_stepsize` |
| Gravidade | **1100** u/s² | `sv_gravity` |

## Velocidades (com a M4, colete padrão)

Corrida do CA 285 × (1 − 0,1 do colete) × peso da arma (M4 0,90; MP5 0,93; R870 0,92; pistola 0,97; faca 1).

| Estado | M4 | Faca |
|---|---|---|
| Andando (C) | ~57 | ~63 |
| Correndo | ~231 | ~257 |
| Sprint (Shift, ×1,4) | ~323 | ~359 |
| Agachado | ~41 | ~45 |

## Pulo

| Medida | Valor | Observação |
|---|---|---|
| Pulo em pé | **49,5** de altura | 330² / (2 × 1100); 0,6 s no ar até voltar à mesma altura |
| Pulo agachado ★ | **~59** | no ar a caixa encolhe 20 pelo centro: os pés sobem ~10 |
| Pulos seguidos | o próximo pulo sai a 30% e recupera 1,2/s | sobe numa caixa e pula logo: ~82% |
| Escalar (mantle) | **não existe no CA** (`trok_escalar 0`) | |

Alcance horizontal de um pulo, do chão ao mesmo nível (0,6 s no ar):

| Velocidade | Alcance |
|---|---|
| Correndo com M4 (~231) | ~139 |
| Correndo com faca (~257) | ~154 |
| Sprint com M4 (~323) | ~194 |
| Sprint com faca (~359) | ~215 |

Para vão que todo mundo passa, use **~80%** disso (correndo: até ~110).

## Queda (dano do CA)

| Altura da queda | Dano |
|---|---|
| até 300 | nenhum |
| 300 → 700 | de 0 a 200, em linha reta |
| ~500 | mata quem tem 100 de vida |

## Passagens (para um corpo de 100u)

| Elemento | Medida sugerida |
|---|---|
| Porta | 64 de largura × **128** de altura (mínimo 40 × 104) |
| Passagem agachado | altura **84–96** (agachado tem 80) |
| Corredor de combate | 128–160 de largura |
| Janela para pular | parapeito até **~45** (pulo) ou ~55 (pulo agachado) |
| Meia cobertura | 60–72: cobre o corpo agachado até o peito; dá para atirar em pé por cima (olho a 83) |
| Cobertura que esconde agachado | ≥ 84 (agachado tem 80) |
| Caixa de parkour | até ~45 de diferença (pulo) ou ~55 (pulo agachado) |
| Explosão | só o mapa (paredes, pilastras, caixas do BSP, portas) protege; gente e props soltos não (`trok_explosao_ca`) |

**Mapas do CS:S provisórios:** foram feitos para um corpo de 72u. Portas (112) ainda passam, mas
dutos e passagens agachadas (~48) não passam com o corpo do CA. Vale só para testar.

## Para os bots usarem o mapa

- Os números do nav e do bot já são os do CA (patch em `nav.h`, `NextBotPlayerLocomotion.h` e
  `hl2mp_bot_locomotion`): corpo 100/80, degrau 30, pulo 47, pulo agachado 58, queda segura 300.
- **Gere o nav de novo** em cada mapa (`nav_generate`, depois `nav_analyze`): os `.nav` do CS:S
  foram gerados para o corpo de 72u.
- O gerador só liga desníveis até 58 e não liga vãos sozinho: confira com `nav_edit 1` e ligue os
  pulos à mão (`nav_connect`, marcando a área com `nav_jump`). O parkour de verdade dos bots está em
  `design/bots-parkour.md`.
- `trok_bot_tatico_mostrar` mostra se o mapa tático achou os lugares de defesa que você pensou.

## Surto (ideias registradas do dono)

- Abrigos com grade trancável (porta de 64 × 128), relógio com alarme e abertura dos abrigos
  no fim da rodada.
- Um lugar mais seguro alcançável só por parkour (ex.: pilastras quebradas até um laboratório):
  pilastras com 30–45 de diferença e vãos de até ~110 entre elas (correndo) dão parkour possível
  sem ser fácil; sprint alcança ~190.

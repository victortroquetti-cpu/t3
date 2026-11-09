# Medidas para mapas (Hammer)

Números tirados do código do SDK 2013 (HL2MP) e da movimentação do Trok
(`shared/trok/trok_gamemovement.cpp`). Unidade: u (1u ≈ 1,9 cm; o jogador tem 72u, ~1,37 m
no motor). Valores com ★ dependem de ConVar e mudam se a movimentação for ajustada
(`cfg/movimento.cfg`).

## Jogador

| Medida | Valor | De onde |
|---|---|---|
| Caixa de colisão em pé | 32 × 32 × 72 | `g_HL2MPViewVectors` |
| Agachado | 32 × 32 × 36 | idem |
| Olho em pé / agachado | 64 / 28 | idem |
| Degrau (sobe andando) | 18 | `sv_stepsize` |
| Gravidade | 600 u/s² | `sv_gravity` do HL2 |

## Pulo e escalada

| Medida | Valor | Observação |
|---|---|---|
| Pulo em pé ★ | 21 (HL2) → **40** no `movimento.cfg` | `trok_pulo_altura` |
| Pulo agachado ★ | pulo em pé + ~36 → **~76** com 40 | encolher as pernas no ar ganha 36u |
| Escalar (mantle) ★ | até **80** acima dos pés | `trok_escalar_altura_max`; segurar pular andando contra a borda, com espaço em cima (72u em pé, ou 36u se já estiver agachado) |
| Bots: maior desnível que planejam | 56 | `GetMaxJumpHeight` do bot; o nav gera ligação até 58 (`JumpCrouchHeight`) |

Alcance horizontal de um pulo (sem contar o escalar), do chão ao mesmo nível:

| Velocidade | Pulo 21u | Pulo 40u |
|---|---|---|
| Andando (140) | ~74u | ~102u |
| Correndo (210) | ~111u | ~153u |
| Sprint (300) | ~159u | ~219u |

Conta: tempo no ar = 2·√(2·g·h)/g; distância = velocidade × tempo. Para vão que todo mundo
passa, use **~80%** do valor da tabela. Do outro lado com borda, o escalar perdoa alguns
unidades de altura.

## Queda

| Medida | Valor |
|---|---|
| Sem dano até | ~230u de queda (velocidade 526,5 u/s com g = 600) |
| Morte a partir de | ~710u (922,5 u/s) |

## Passagens

| Elemento | Medida sugerida |
|---|---|
| Porta | 64 de largura × 112 de altura (mínimo 48 × 80) |
| Passagem agachado | altura 40–48 (o jogador agachado tem 36) |
| Corredor de combate | 96–128 de largura (dá para fazer strafe e peek) |
| Janela para pular | parapeito até 40 (passa pulando) ou até 80 (passa escalando) |
| Caixa de cobertura | 48–56 de altura: esconde agachado, dá para atirar em pé |
| Caixa de parkour | 40–76 (pulo agachado) ou até 80 (escalar) |

## Para os bots usarem o mapa

- Gere o nav (`nav_generate`) e analise (`nav_analyze`): luz, visibilidade e esconderijos.
- Pilastras e caixas de parkour: o gerador só liga desníveis até 58 e não liga vãos sozinho.
  Confira com `nav_edit 1` e ligue os pulos à mão (`nav_connect`, marcando a área com `nav_jump`).
- `trok_bot_tatico_mostrar` mostra se o mapa tático achou os lugares de defesa que você pensou.

## Quarentena (ideias registradas do dono)

- Abrigos com grade trancável (porta de 64 × 112), relógio com alarme e abertura dos abrigos
  no fim da rodada.
- Um lugar mais seguro alcançável só por parkour (ex.: pilastras quebradas até um laboratório):
  com pulo de 40u e escalar até 80u, pilastras de 56–80u de diferença, com vãos de até ~120u
  entre elas (correndo), dão parkour possível sem ser fácil.

# Modos de jogo do Trok

`trok_modo` vale no próximo mapa (o menu "Jogar" escolhe). Cada modo executa o seu cfg ao começar.

| Modo | `trok_modo` | cfg | Como funciona |
|---|---|---|---|
| Mata-mata em equipe | `equipe` (padrão; `tdm` também) | `cfg/equipe.cfg` | dois times; vence quem chegar na meta de abates (`mp_fraglimit`) ou tiver mais no fim do tempo (`mp_timelimit`) |
| Exército solo | `solo` (ou `ffa`) | `cfg/solo.cfg` | todos contra todos; meta de abates individual; o HUD mostra sua posição ("2º DE 8") |
| Surto | `surto` | `cfg/surto.cfg` | infecção (ver `design/modo-surto.md`) |

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

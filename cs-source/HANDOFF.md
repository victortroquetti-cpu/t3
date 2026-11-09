# Passagem para o Claude Code local

Registro do que foi feito na nuvem (sessão de 09/11/2026) e do que fica melhor no PC do
dono, com Windows, Visual Studio, Steam e o jogo. Leia isto antes de continuar.

**Combinado com o dono:** o que for difícil na nuvem e fácil no local (compilar, abrir o
jogo, gerar nav, testar em partida, ver a tela) não se força na nuvem; só se registra aqui.

## Onde está cada coisa

| O quê | Onde |
|---|---|
| Visão geral, setup, fluxo | `cs-source/README.md` |
| Skill do Claude para este mod | `.claude/skills/source-mod/` (SKILL.md + references/) |
| Bots: pesquisa, desenho, números, fontes | `cs-source/design/bots.md` |
| Código dos bots | `cs-source/src/game/server/trok/bot/` |
| Mudanças em arquivos da Valve (um patch) | `cs-source/patches/sdk.patch` |
| Perfis editáveis dos bots | `cs-source/game/mod_trok/scripts/trok_bot_perfis.txt` |
| Simulador (padrão x Trok) e página do comparativo | `cs-source/tools/botsim/` |
| Build automático no Windows (GitHub Actions) | `.github/workflows/trok-build.yml` |
| Instruções que vão no zip do mod | `cs-source/tools/LEIA-ME.txt` |

## O que foi verificado e o que não foi

| Item | Situação |
|---|---|
| `trok.py setup/gen/check/save-patch/status/refs` | rodam do zero no Linux |
| Sintaxe e tipos de todo o código do mod e dos arquivos da Valve com patch (g++ -fsyntax-only, -Wall) | passam sem aviso |
| VPC da Valve lendo nossos `.vpc` gerados (`server_trok.vpc`, `client_trok.vpc`, `launcher_main_mod_trok.vpc`) e criando `trok.sln` | **passou** no Windows do GitHub Actions |
| Compilação com MSVC (link incluso) | **passou** (GitHub Actions, build nº 2, Release win64): `client.dll` e `server.dll` em `mod_trok/bin/x64/`, pacote `trok-mod-windows` com 57 arquivos |
| Mod rodando no jogo, bots em partida | **não verificado** (precisa da Steam) |
| Simulador | roda (`python3 -I cs-source/tools/botsim/botsim.py`); é modelo das decisões, não o jogo |

## Primeiro no local (nesta ordem)

1. Já compila: o build automático no Windows passou. Para desenvolver no local:
   `python cs-source/trok.py setup`, `cs-source\sdk\src\createtrok.bat`, abrir `trok.sln`,
   **Release | win64** (só existe essa plataforma). Ou baixar o pacote pronto do último build
   em Actions > "Trok: build Windows" > Artifacts. Depois de mexer em arquivo da Valve:
   `python cs-source/trok.py save-patch`.
2. Rodar o mod (o projeto `Client (Trok)` já vem com as opções de execução, ou
   `cs-source/sdk/game/mod_trok.exe`). Precisa do Source SDK Base 2013 Multiplayer e do CS:S.
3. Teste básico: `map dm_lockdown`, `exec bots`, `trok_bot_estilos_info`, `nb_select` + `nb_debug BEHAVIOR`.
4. Mapa do CS:S: `map de_dust2`, `nav_generate`, `nav_analyze`, `exec bots`. Conferir:
   - se os jogadores nascem nos spawns de TR e CT (fallback no patch de `CHL2MP_Player::EntSelectSpawnPoint`);
   - se o `.nav` original do CS:S carrega ou precisa gerar de novo (não testado).
5. Se o `gameinfo.txt` reclamar de caminho: `|gameinfo_path|../mod_hl2mp` só existe dentro de
   `cs-source/sdk/game/`. Na pasta `sourcemods`, o pacote do build automático já copia os arquivos do HL2MP para dentro do mod.

## Riscos conhecidos (olhar no primeiro build/teste)

- **`IsVisibleEntityNoticed`** é chamado de vários lugares via `IsAbleToSee`; o acumulador de
  percepção é por tempo. Conferir com `trok_bot_debug_percepcao 1` se a barra enche e esvazia direito.
- **Ritmo de tiro** solta o gatilho com `ReleaseFireButton()` depois do `Update` do HL2MP, que aperta
  com duração de 1 s (`hl2mp_bot_fire_weapon_min_time`). Conferir se as rajadas saem curtas de verdade.
- **Peek** (`CTrokBotFight`) move o bot com `Approach()` direto, até ~150u. Pode brigar com a
  locomoção em degraus ou portas. Conferir com `nb_debug BEHAVIOR` (estados `Combate(peek)`, `Combate(cobertura)`).
- **Faixa no corredor** (`CTrokBotLocomotion::Approach`) desloca o alvo do caminho até 36u. Se os bots
  raspam em parede ou travam em batente, `trok_bot_faixa 0` isola o problema.
- **Pulo agachado** é ligado por altura da borda (> 40u). Conferir em caixas de 48–56u.
- **Encounter spots** e luz só existem depois de `nav_analyze`. Sem isso, checar cantos cai no
  plano B (esconderijos) e a luz fica "tudo claro".
- **Armas do HL2MP** não perdem precisão andando. O ganho do contra-strafe medido no simulador
  supõe armas no estilo do CS (o mod vai ter); com as do HL2MP, o efeito em jogo é menor.
- **CSO2 / Combat Arms:** o dono gosta dos modos zumbi/quarentena desses jogos. Mecânicas podem
  inspirar; nomes, mapas, modelos e sons desses jogos não entram no mod (combinado e registrado na skill).

## Movimentação e HUD (feitos na nuvem, testar no local)

- `shared/trok/trok_gamemovement.cpp` substitui o objeto de movimento do HL2 (patch em
  `hl_gamemovement.cpp`). Pulo com altura própria (`trok_pulo_altura`; o HL2 pula só **21u**),
  sem o empurrão de bunny hop do HL2, deslizar (sprint + agachar) e escalar (segurar pular
  contra borda até 80u). Tudo sem estado entre ticks, então a predição do cliente bate com o
  servidor sem variável de rede nova. Números em `cfg/movimento.cfg` (o `server.cfg` executa).
- Fluidez (pedido: "no CA pulamos entre caixas, de um lado pro outro, principalmente com faca"):
  `trok_ar_controle` (controle no ar; Source = 30, padrão do mod 60), `trok_faca_velocidade`
  (arma branca na mão corre mais; 1.12), e `sv_accelerate`/`sv_airaccelerate`/`sv_friction` no
  `cfg/movimento.cfg`. Medir no CA: quanto dá para virar no meio de um pulo entre caixas e quanto
  mais rápido se corre de faca.
- Os bots seguram pulo e agacham no ar ao subir bordas acima do pulo em pé, e usam o escalar.
- `client/trok/hud_trok_hitmarker.cpp`: marcador de acerto e de abate pelo evento `player_hurt`.
- Medidas para mapas (pulo, escalada, vãos, portas, queda, nav para bots): `design/medidas-mapas.md`.

Riscos para olhar no jogo:
- Escalar: pode "colar" em paredes altas lisas se o raio achar topo em objeto fino (ajustar
  `trok_escalar_altura_max` ou exigir profundidade maior de topo).
- Deslizar: a velocidade devolvida depois do atrito também anula parte da aceleração em rampas
  (deslizar ladeira abaixo pode ficar rápido demais).
- Velocidades do HL2 (`hl2_normspeed` etc.) são do servidor e o sprint gasta energia do traje.

### Armas, mira pelo cano, corrida e caminhada (nuvem, para testar no local)

- Sistema de armas guiado por script: `shared/trok/weapon_trok_gun.*` + bloco `"Trok"` em
  `scripts/weapon_trok_{fuzil,submetralhadora,pistola,escopeta}.txt` (modelos e sons de
  protótipo do CS:S montado). Precisão por estado, dispersão por tiro seguido, recuo em
  padrão, queda de dano, auto/semi, mira pelo cano com ajuste ao vivo (`trok_mira_ajuste`,
  `trok_mira_imprimir`), peso da arma e lentidão mirando (aplicados na movimentação).
- Correr (Shift) abaixa a arma e impede o tiro; caminhar (C) é silencioso (patch em
  `UpdateStepSound`). Teclas em `cfg/config_default.cfg`.
- Vida/colete (`trok_vida`, `trok_colete`) e armas iniciais (`trok_armas_iniciais`, patch em
  `GiveDefaultItems`).
- **Todos os números num lugar:** `cfg/balanceamento.cfg` + `cfg/movimento.cfg` + scripts das
  armas. Guia de cada campo e de como medir no jogo de referência:
  `design/sistemas-jogabilidade.md`. Assets gratuitos: `design/assets-gratuitos.md`.

Riscos para olhar no jogo:
- modelos do CS:S nas armas: sons de recarga podem ficar mudos (eventos de animação com
  soundscript do CS:S); posição de mira padrão é chute;
- se o `.wav` direto no `SoundData` não tocar, criar um soundscript do Trok com as entradas;
- `mira_fov_arma` ainda não é aplicado (precisa de patch no ClientMode);
- bots com armas do Trok: as consultas de arma do bot do HL2MP (`IsCombatWeapon`,
  `EquipBestWeaponForThreat`) foram feitas para as armas do HL2; conferir se escolhem bem.

**Tarefa do dono no local:** com o Combat Arms instalado, medir no jogo dele a velocidade de
corrida, a altura do pulo e o tempo de deslize (cronometrar uma distância conhecida) e
passar os números para o `cfg/movimento.cfg`; e das armas (cadência, tiros para matar perto
e longe, abertura da mira, recuo) para os scripts `weapon_trok_*.txt`. Guia campo a campo em
`design/sistemas-jogabilidade.md`. Só números; nada de arquivo do jogo.

## Interface e estudos dos mods do SDK (09/11, nuvem; nada visto rodando)

- **Menu próprio** no lugar do menu do GameUI: `client/trok/ui/trok_menu.cpp` (tela inicial,
  pausa, jogar contra bots), fontes Barlow (OFL) em `resource/fonts`, esquema em
  `resource/TrokScheme.res`. Patches: `vgui_int.cpp` (cria e registra) e `cdll_client_int.cpp`
  (`HandleUiToggle`: ESC volta das subtelas). Concepts, identidade e próximos passos em
  `design/ui.md`. **Testar primeiro:** se as fontes carregam (nome da família no Windows; se não,
  o VGUI cai numa fonte padrão), escala em 1080p/1440p, ESC dentro da partida, foco do teclado
  depois de fechar o diálogo de opções.
- **HUD e tela de carregamento** dos concepts: `client/trok/hud_trok.cpp` e
  `client/trok/ui/trok_loading.cpp` (detalhes em `design/ui.md`). O HUD do HL2 sai por
  `#ifndef TROK` (vida, colete, munição, energia, mortes, mira). Evento novo `trok_dano`
  (`server/player.cpp`, `resource/modevents.res`, que agora é uma cópia do HL2MP + o nosso: se o
  SDK mudar o dele, copiar de novo). **Testar:** tela de carregamento entrando por "Jogar" e por
  servidor (e um kick/servidor cheio para ver a mensagem), lista de abates, tela de morte com bot,
  mira abrindo ao andar/atirar e sumindo na mira e na corrida.
- **Escadas dos mapas do CS:S** (pincéis `CONTENTS_LADDER`): o movimento do HL2 só tratava
  `func_useableladder`. `CTrokGameMovement` agora usa as duas (`LadderMove`, `OnLadder`,
  `FullLadderMove`) e liga o ajuste de escada do CS:S (`sv_ladder_dampen`, `sv_ladder_angle`)
  com `|| defined( TROK )` em `gamemovement.cpp`. Testar em de_nuke, de_aztec, cs_office (subir,
  descer, pular da escada, bots na escada).
- **Zoom da mira** agora é chamado no cliente e no servidor (`SetFOV` é previsto): começa no
  mesmo tique, sem esperar o ping.
- Estudos completos (código com a mesma licença, pode ser reaproveitado com crédito):
  `design/estudo-neotokyo.md` e `design/estudo-da-momentum-ff.md`. Backlog que saiu deles, por
  ganho:
  1. deslize: **desligado** (`trok_deslizar 0`) por decisão do dono, que não quer o deslize "try hard"
     do CoD; se voltar algum dia, com estado em rede e recarga (hoje encadeia sem limite);
  2. "lurch" do Momentum (troca de direção logo depois do pulo, sem ganhar velocidade) + tempo de
     coyote + teto de velocidade no pulo (Fortress Forever);
  3. `GetViewModelFOV` por arma (fecha `mira_fov_arma`) e balanço da arma ao andar; o
     `CalcViewModelLag` da Valve roda depois do nosso e tira a alça do centro olhando para
     cima/baixo na mira;
  4. recuo que sobe e fica (estilo CoD) mexendo nos ângulos do usercmd no cliente;
  5. bots: correções do `PathFollower` do NT;RE (vãos, quedas, desvio), penalidade em área onde
     travou, supressão, bots usando a mira;
  6. tamanho do jogador: HL2MP 72 em pé / 36 agachado, olho agachado 28; CS:S 62 / 45, olho agachado 47
     (avaliar `GetViewVectors` com as medidas do CS:S nos mapas do CS:S (ver estudo DA/Momentum 1.4g).

## Rodada "o que der das TCs" (09/11, nuvem; nada testado no jogo)

- Bots: correções de seguir caminho do NT;RE (patch em `NextBotPathFollow`), memória de onde
  travaram (`trok_bot_travou_meia_vida`), mira pelo cano (`trok_bot_mira_cano*`), fogo de
  supressão (`trok_bot_supressao`).
- Movimento: virada logo depois do pulo, pulo atrasado na borda, pulo antecipado e teto de
  velocidade no pulo (`trok_pulo_*`, tempos previstos em rede no `CHL2MP_Player`, patch).
  **Testar:** caixa a caixa trocando de lado; sair de borda correndo e pular atrasado; erro de
  predição (`cl_showerror 1`, `cl_predictionlist`) ao pular em rede.
- Arma: FOV da arma mirando, alça centrada mirando, balanço ao andar, recuo que fica
  (`trok_recuo_fixo`). **Testar:** alinhar a alça de cada arma de novo (o atraso some na mira).
- Placar do TAB próprio.

Ficam para casa / artista (registrado a pedido do dono, que quer o nível visual do NEOTOKYO
Rebuild, estilo Combat Arms, arcade e sem fotorrealismo):
- **Armas e animações próprias**: modelos de arma em 1ª pessoa com animações feitas para elas
  (as do NT;RE e do DA são arte deles, não podem entrar). Animações de 3ª pessoa para o que o
  movimento novo faz (escalar, virada no ar) pedem modelo de jogador próprio; com os do CS:S/HL2
  só dá para reaproveitar o que já existe.
- **Visual**: iluminação dos mapas (HDR, luz de ambiente, `env_tonemap_controller`,
  `color_correction` com LUT própria, bloom moderado). É trabalho de mapa e de ajuste olhando o jogo.
- **Mapa no estilo do CA que o dono já tem**: precisa virar BSP (Hammer). Medidas de pulo,
  escalada e vãos para construir: `design/medidas-mapas.md`. Gerar nav mesh para os bots.

## O que o simulador mostrou (para não refazer)

- O bot padrão do HL2MP mira o centro exato: ~93% de acerto em alvo parado em **todos** os níveis,
  e nunca na cabeça. O Trok vai de ~36% (fácil) a ~87% (expert).
- O erro mínimo de mira precisa acompanhar o tamanho do alvo na tela (lei de Fitts), senão o bot
  nunca acerta de longe. Já corrigido (`mira_precisao_alvo`).
- Giro de cabeça baixo + a desaceleração do NextBot perto do alvo = não acompanha ADAD de perto.
  Giro subiu para 400/600/800/950 °/s.
- Rajadas curtas com muito movimento perdem duelos. Venceram rajadas de 0,6–1 s (média distância)
  e 0,3–0,5 s (longa), com pouco tempo andando: expert de ~80% para ~92% de vitórias contra um
  jogador de referência que atira andando.
- Fácil de longe ainda é fraco (a 800u, parado, ~1 em 4 duelos sem acerto em 3 s). É ajuste de perfil, se o dono quiser.

## Pendências (backlog dos bots)

Feito: percepção gradual, mira humana, giro, pulo agachado, strafe/ritmo de tiro, contra-strafe,
peek, recarregar na cobertura, procurar última posição, checar cantos, memória da morte, mapa
tático (regiões, entradas, pulos de acesso), estilos (entry, lurker, âncora, suporte,
equilibrado), memória do time (avisos, apoio, ângulos, rotas, perigo), variedade de rotas e
destinos, hesitação ao nascer, faixa no corredor.

A fazer, quando o dono pedir (melhor com o jogo aberto para testar):
- sistema de armas próprio: precisão estilo CS (parado/andando/pulando), recuo com padrão,
  dano por parte do corpo e queda de dano, mira pelo cano (ADS); scripts editáveis;
- som pela distância de caminho no nav (hoje é linha reta, com parede abafando);
- recuo por arma e controle de spray (quando o mod tiver armas próprias);
- granadas/utilitários;
- rotações do time e estratégia por mapa;
- dificuldade dinâmica (meio passo por rodada; só mira/reação);
- modo zumbi/quarentena: infectados com NextBot (cerco, quebrar barricada) e humanos que trancam
  abrigos; relógio com alarme e abertura; grade trancável (ideia registrada na conversa).
  **Referência de jogabilidade: quarentena do Combat Arms (1º) e zumbis do Call of Duty (2º).**
  Os modos zumbi do CS não são referência (o dono não gosta da jogabilidade deles). Detalhes e
  a linha do tempo que o dono passou: `design/zumbi-referencias.md` e `design/zumbi-mods-analise.md`.

## Ambiente da nuvem (por que algumas coisas não foram feitas aqui)

- Sem Visual Studio, sem podman e sem daemon do docker: não dá para compilar o mod.
- Rodar o `vpc` pré-compilado da Valve foi barrado pela permissão do ambiente.
- Rede: vários PDFs de pesquisa (gameaipro, GDC Vault) não abriram; os números vieram de trechos
  indexados (ver fontes em `design/bots.md`).
- O build no GitHub Actions contorna a compilação, mas cada ciclo de correção leva minutos. No
  local, com o Visual Studio aberto, é bem mais rápido.

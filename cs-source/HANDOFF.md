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
| Compilação com MSVC (link incluso) | **não verificado**: o 1º build automático falhou no nome da plataforma (`x64` em vez de `win64`), já corrigido; o resultado da compilação de verdade está no próximo build |
| Mod rodando no jogo, bots em partida | **não verificado** (precisa da Steam) |
| Simulador | roda (`python3 -I cs-source/tools/botsim/botsim.py`); é modelo das decisões, não o jogo |

## Primeiro no local (nesta ordem)

1. `python cs-source/trok.py setup`, depois `cs-source\sdk\src\createtrok.bat`, abrir `trok.sln`,
   compilar **Release | win64**. Corrigir os erros do MSVC que aparecerem (o g++ não pega tudo:
   avisos tratados como erro, conversões, `min`/`max` do Windows). Depois de mexer em arquivo da
   Valve: `python cs-source/trok.py save-patch`.
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
- som pela distância de caminho no nav (hoje é linha reta, com parede abafando);
- recuo por arma e controle de spray (quando o mod tiver armas próprias);
- granadas/utilitários;
- rotações do time e estratégia por mapa;
- dificuldade dinâmica (meio passo por rodada; só mira/reação);
- modo zumbi/quarentena: infectados com NextBot (cerco, quebrar barricada) e humanos que trancam
  abrigos; relógio com alarme e abertura; grade trancável (ideia registrada na conversa).

## Ambiente da nuvem (por que algumas coisas não foram feitas aqui)

- Sem Visual Studio, sem podman e sem daemon do docker: não dá para compilar o mod.
- Rodar o `vpc` pré-compilado da Valve foi barrado pela permissão do ambiente.
- Rede: vários PDFs de pesquisa (gameaipro, GDC Vault) não abriram; os números vieram de trechos
  indexados (ver fontes em `design/bots.md`).
- O build no GitHub Actions contorna a compilação, mas cada ciclo de correção leva minutos. No
  local, com o Visual Studio aberto, é bem mais rápido.

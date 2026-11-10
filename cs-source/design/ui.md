# Interface do Trok

Objetivo do dono (09/11/2026): **não parecer CS:S; parecer outro jogo.** Inspiração em CoD
moderno: limpo, sutil, texto condensado em caixa alta, fundo escuro com o mapa ao vivo atrás, uma
cor de destaque.

Concepts (8 telas, clicáveis): https://claude.ai/artifact/PgNx6FnrhYFnb4Gd9GQ4Cp
Tela inicial, Jogar contra bots, Arsenal, Configurações, Carregando, HUD, Placar e Morte.

## Identidade

| Item | Valor |
|---|---|
| Fundo | `#0B0D10`, painéis `#14171C` a ~80%, linhas brancas a 8% |
| Texto | `#ECEEF1` / `#A3A9B1` / `#80868E` |
| Destaque | `#F2B544` (âmbar), ConVar `trok_ui_destaque "R G B"` |
| Times | azul `#5AA2FF`, vermelho `#FF5A5A`; infecção/zumbi vermelho-sangue `#D9362B`; Fúria `#CC2030` sobre roxo-escuro `#220A26` (decisões do dono; humano do Surto sempre azul) |
| Fontes | Barlow Condensed (títulos, botões) e Barlow (texto), SIL OFL, em `resource/fonts/` |
| Escala | concept em 1280x720; o código escala tudo pela altura da tela (`S(px)`) |

## O que já está no código

`src/game/client/trok/ui/trok_menu.cpp` (arquitetura do NEOTOKYO Rebuild, ver
`design/estudo-neotokyo.md` 1.1 a 1.4):
- painel de tela cheia registrado com `IGameUI::SetMainMenuOverride` (patch em
  `vgui_int.cpp`, `VGui_CreateGlobalPanels`); não roda com `-tools`;
- painel 1x1 de teclado (`CTrokMenuInput`) e ESC das subtelas pelo patch em
  `CHLClient::HandleUiToggle`;
- desenho imediato: `Run(MODE_PAINT)` desenha e `Run(MODE_CLICK)` trata o clique com o mesmo código;
- telas: **inicial** (Jogar, Servidores, Criar partida, Configurações, Sair), **pausa** na partida
  (Continuar, Configurações, Desconectar, Sair) e **Jogar contra bots** (mapa com nav primeiro,
  4 dificuldades, bots por time, estilos; "Iniciar" monta `hl2mp_bot_*`, `trok_bot_estilos`,
  `maxplayers` e `map`). Últimas escolhas ficam em `trok_ui_*` (salvas no config);
- fontes em `resource/TrokScheme.res` (não mexe no ClientScheme do HL2MP).

`src/game/client/trok/ui/trok_loading.cpp`: tela de carregamento (receita do NT;RE,
`neo_loading.cpp`): `SetLoadingBackgroundDialog`, o `LoadingDialog` do GameUI sai da tela e o
painel lê a barra e o texto dele. Nome do mapa, dica sorteada, barra fina; desconexão e erro
do servidor no mesmo visual (barra escondida = desconectado).

`src/game/client/trok/hud_trok.cpp`: HUD do concept, um painel só:
- vida + fôlego (energia do sprint, só aparece quando não está cheia);
- munição: pente / reserva, uma marca por bala, nome da arma (do script) e AUTO/SEMI;
- placar e tempo no topo, cores relativas (seu time azul); `mp_fraglimit` vira "N PARA VENCER";
- lista de abates com marca CABEÇA; aviso "ABATE" perto da mira;
- mira em cruz que abre com a dispersão real da arma e some mirando ou correndo
  (`trok_hud_mira 0` desliga); faca = ponto;
- tela de morte: quem matou, arma, distância, vida dele, dano trocado nesta vida
  (evento novo `trok_dano`, com a parte do corpo, em `server/player.cpp` + `resource/modevents.res`).
- direção do dano: arco vermelho em volta da mira apontando para de onde veio (mensagem
  "Damage" do servidor; o indicador do HL2 saiu);
- vida baixa: bordas da tela avermelhando e pulsando (`trok_hud_vida_baixa`, fração da vida);
- radar no canto superior esquerdo, girando com a visão: aliados sempre; inimigos (zumbis em
  vermelho-sangue) quando estão à vista ou acabaram de acertar alguém (`trok_hud_radar`, `_alcance`).
O HUD do HL2 (vida, colete, munição, energia, mortes, mira, indicador de dano) sai por `#ifndef TROK`.
Minimapa com imagem do mapa: precisa de uma imagem de cada mapa (por enquanto é radar sem fundo).

### HUD novo (10/11/2026, `trok_hud_estilo 2`, padrão)

Pedido do dono: "muito aperfeiçoado, inspirado nos CoDs modernos, sofisticado". Pesquisa (MW2019,
MWII/III, Cold War/BO6, BF2042/BF6, The Finals): `design/hud-pesquisa.md`. Prévia interativa (4 estados:
Equipe, Surto humano, Surto zumbi, Corrida Armada, + minimalista): arquivo `hud.html` gerado na nuvem
(publicado à parte). Código: `src/game/client/trok/hud_trok_v2.cpp` (o `hud_trok.cpp` antigo
fica para `trok_hud_estilo 1` e desenha a tela de morte nos dois).

Regras do visual: tudo preso à margem (`trok_hud_margem`, 28 px a 720); um número grande por canto;
números e ícones em vez de rótulos (nada de "VIDA", "COLETE", "CRÉDITOS", "HUMANOS", "PARA VENCER"...);
algarismos de largura fixa (o código desenha letra a letra); traço fino em vez de moldura; véu escuro em
degradê no lugar de caixa; uma cor de destaque (âmbar) para "mudou", vermelho só para perigo, azul/vermelho
para lados e vermelho-sangue para a infecção. **Nada pisca em loop e não há vinheta de estado** (nem verde
do zumbi, nem âmbar do alarme, nem do lento): só eventos curtos animam (acerto, dano, abate; entrada
0,12-0,25 s, saída 0,4-0,5 s). Fica só a vinheta vermelha curta do dano e a de vida baixa, fixa.
Sem bússola (o dono preferiu; `trok_hud_bussola 1` ainda liga): a direção do dano é só a cunha em volta da mira.

| Elemento | Onde (px a 1280×720) | O quê |
|---|---|---|
| Radar | canto sup. esq., 168 (140 no minimalista) | gira com a visão, cantos marcados, anéis, cone da visão, "N" na borda; aliados triângulo, inimigos quadrado que some; no Surto humano azul e zumbi vermelho-sangue |
| Placar | topo, centro, y 20 | Equipe: nós · tempo · eles + barras até a meta, meta pequena sob o relógio; Solo: você · tempo · melhor rival, "1º/8" pequeno; Surto: ícone + humanos · relógio (âmbar fixo no alarme) · zumbis + ícone; aviso real (preparação, alarme, abrigos) só 4 s quando acontece; Corrida: "2º" e "DE 8" no meio, líder (nome e degrau) à direita |
| Lista de abates | canto sup. dir., linhas de 24 | atacante · ícone da arma (48×18) · ícone de cabeça/infecção (16) · vítima; véu à direita, traço âmbar nas suas linhas; sem ícone, o texto/losango de antes |
| Vida | canto inf. esq. | número 46 px; barra de 10 partes (220×5) com rastro vermelho do dano; fôlego em linha âmbar só quando gasto |
| Colete | traço fino (2 px) por cima da barra de vida | número menor (22 px) na cor do colete logo à direita do número da vida ("100 50"), sem rótulo |
| Acima da vida | | "$ 1.250" (economia, âmbar no tempo de compra; só se `trok_hud_dinheiro` estiver desligado, senão é o `hud_trok_economia.cpp`); Surto humano: "[N] STRIKER" e a plaquinha da Fúria "1.800 / 2.350 FÚRIA" (fundo roxo-escuro `#220A26`, filete e rótulo vermelho-sangue `#CC2030`, número `#E8414F` para ler no roxo; preço âmbar se não dá) |
| Zumbi | | tipo do zumbi ao lado do número, barra vermelho-sangue, brilho curto correndo na barra enquanto regenera (sem texto) |
| Arma | canto inf. dir. | ícone (48×18) · NOME · modo de tiro em texto pequeno; pente 46 px / reserva 22 px; uma marca por bala (pentes até 40); explosivos com ícone e número à esquerda; garras: ícone + "GARRAS" |
| Corrida | acima da arma | "14/20" + escada (degrau atual âmbar e mais alto) e "› ESCOPETA" pequeno (próxima); ao subir, "DEGRAU 15 · ESCOPETA" sob a mira |
| Centro | | mira com contorno (abre com a dispersão), X de acerto (abate vermelho e 35% maior, cabeça âmbar), cunha de dano em 3 faixas (raio 118), granada perto (anel 78, ícone, seta, metros; sem pulsar), eventos à direita (x +44, y +30: ABATE +$300 nome, NA CABEÇA, ZUMBI ABATIDO, INFECTOU +1), "[R] RECARREGAR" / "SEM MUNIÇÃO" (y +74, fixo), "PROTEGIDO" com barra, "[E] PEGAR FUZIL" (62% da altura) |

### Ícones (para o local fazer)

O HUD procura cada ícone como material `vgui/trok/icones/<nome>`. Se não existir (`IsErrorMaterial`),
usa o desenho ou o texto de antes, então dá para ir pondo um por um. A prévia mostra silhuetas
genéricas provisórias (contorno tracejado) no lugar e no tamanho de cada um.

| Grupo | Nomes | Tamanho do arquivo | Desenhado em (px a 720) |
|---|---|---|---|
| Armas e explosivos | `arma_fuzil`, `arma_submetralhadora`, `arma_escopeta`, `arma_pistola`, `arma_faca`, `arma_granada`, `arma_granada_luz`, `arma_granada_fumaca`, `arma_mina`, `arma_claymore`, `arma_remota`, `arma_torreta`, `arma_foguete`, `garras` | 128×48 (8:3) | 48×18 (lista de abates, ao lado do nome da arma); 40×15 (explosivos na arma); 26×10 (aviso de granada) |
| Abate | `abate_cabeca`, `abate_infeccao`, `abate_explosao`, `abate_queda` | 32×32 | 16×16 (lista de abates); explosão/queda entram no lugar da arma (48×18), então desenhe centrado com sobra dos lados |
| Lados (Surto) | `lado_humano`, `lado_zumbi` | 32×32 | 16×16 (placar do topo) |

- O nome da arma sai da classe: `weapon_trok_<x>` vira `arma_<x>` (arma nova = ícone novo com o mesmo nome).
  `weapon_crowbar`/`stunstick` = `arma_faca`; mundo/queda = `abate_queda`; explosão/barril = `abate_explosao`.
- Silhueta de perfil, **cano para a direita**, centrada, com ~2 px de margem; **branco puro sobre
  transparente** (o código pinta a cor: cinza, âmbar, vermelho-sangue). Traço/preenchimento cheio, sem
  sombra nem contorno (o HUD não põe sombra em textura).
- Feitos a partir dos modelos do próprio jogo (render de perfil → traçar em SVG), nunca de ícones de outros jogos.
- SVG → PNG (Inkscape: `inkscape icone.svg -w 128 -h 48 -o icone.png`) → VTF com alfa (VTFEdit ou
  `vtex`; formato BGRA8888 ou DXT5, sem mipmaps: "No Mipmap", "No LOD", "Clamp S/T").
- VMT ao lado (`game/mod_trok/materials/vgui/trok/icones/<nome>.vmt`):
  `"UnlitGeneric" { "$basetexture" "vgui/trok/icones/<nome>" "$translucent" 1 "$vertexcolor" 1 "$vertexalpha" 1 "$ignorez" 1 }`
- Arquivos em `game/mod_trok/materials/vgui/trok/icones/` (vão no git e no zip, são arte própria).
- Para ver no jogo: `mat_reloadallmaterials` não basta para o cache do HUD; reabra o mapa.

Opções (cvars, salvas): `trok_hud_estilo`, `trok_hud_escala` (0,6-1,6), `trok_hud_opacidade` (0,2-1),
`trok_hud_minimalista` (sem marcas de bala e créditos; radar menor), `trok_hud_bussola` (padrão 0),
`trok_hud_eventos`, `trok_hud_granada`, `trok_hud_granada_raio`, `trok_hud_margem`; continuam valendo
`trok_hud`, `trok_hud_mira`, `trok_hud_radar(_alcance)`, `trok_hud_vida_baixa`, `trok_hud_killfeed_tempo`,
`trok_hitmarker(_tempo)`. Ganchos para outros elementos: `TrokHudV2_Ativo()`, `TrokHudV2_Prompt()`,
`TrokHudV2_Evento()` (`hud_trok_v2.h`). A faixa da série de abates segue em `hud_trok_killstreak.cpp`.

## Telas dentro da partida e do menu (10/11/2026, nuvem; nada visto rodando)

Base comum: `client/trok/ui/trok_ui_comum.cpp/.h` (`CTrokPainel`: tela cheia, desenho imediato como o
menu, `Desenhar()` pinta e trata o clique; cores, `CorDoTime` relativa, `NomeDoTime`, `ModoComTimes`).
Os painéis de viewport são criados no `CHudViewport::CreatePanelByName` (patch em
`clientmode_hl2mpnormal.cpp`); o `PANEL_TEAM` entra por `CreateDefaultPanels`.

| Tela | Arquivo | O que faz |
|---|---|---|
| Seleção de time (`PANEL_TEAM`) | `trok_time.cpp` | Só no mata-mata em equipe. MERCENÁRIOS (1, `TEAM_COMBINE`) e REMANESCENTES (2, `TEAM_REBELS`) com quantos e quem; AUTOMÁTICO (3, time com menos gente, decidido no cliente); ASSISTIR (4). Manda `jointeam N`. Abre ao entrar (depois das boas-vindas, 1 s depois de chegar o gamerules; `trok_ui_time_ao_entrar 0` desliga) e na tecla M (`trok_menu_time`). ESC/M fecham. Fecha sozinha se o modo não tem times. |
| Boas-vindas (`PANEL_INFO`, MOTD) | `trok_motd.cpp` | Título = hostname, texto do `motd.txt` (prefere `motd_text`); HTML vira texto; link vira botão ABRIR PÁGINA (overlay da Steam). Rolagem (roda, setas, PgUp/PgDn). ENTER/ESPAÇO/ESC fecham e chamam a seleção de time. Partida aberta pelo próprio PC ou texto vazio: pula direto (`trok_ui_motd_local 1` mostra). |
| Espectador (`PANEL_SPECGUI`) | `trok_espectador.cpp` | Cartão embaixo: ASSISTINDO + time (cor relativa) + nome + vida (barra); teclas lidas dos binds (`+attack` próximo, `+attack2` anterior, `+jump` 1ª/3ª pessoa). Quem só assiste vê "M escolher time". Não desenha na câmera da morte. O menu de espectador do Source (`+duck`) foi desligado (`clientmode_shared.cpp`). |
| Chat | patch em `hl2mp_hud_chat.cpp/.h` | Re-estilizado (não substituído): histórico e campo usam o `TrokScheme` (`ChatFont` = Barlow Medium, acentos ok), fundo `#14171C`, prompt TODOS:/TIME:, cores de nome relativas (`GetClientColor`), sem botão de filtros. O histórico que some (`hud_saytext_time`) e a rolagem são os do Source. Textos `(TIME)`, `(MORTO)`, "entrou na partida" em `resource/mod_trok_english.txt` e `_brazilian.txt` (UTF-16 LE). |
| Centro e dicas | patches em `vgui_centerstringpanel.cpp`, `hud_hintdisplay.cpp` | Fonte `TrokCentro` (Barlow Condensed SemiBold); dica com texto claro e fundo dos painéis. |
| SERVIDORES | `trok_menu.cpp` (`RunServers`) + `trok_servidores.cpp/.h` | `ISteamMatchmakingServers` do cliente: INTERNET (`RequestInternetServerList` com filtro `gamedir` = pasta do mod) e REDE LOCAL / VPN (`RequestLANServerList`, só a mesma pasta). Ordem por ping, SENHA marcada, cheio em vermelho, clique duplo/ENTRAR, F5. CONECTAR POR IP (`trok_ui_servidor_ip`) e senha (`password`). VPN: Radmin/ZeroTier repassam a busca da LAN; Hamachi às vezes não, aí é por IP. Sem favoritos/histórico ainda. |
| Seleção de modo | `trok_menu.cpp` (`RunModeCards`, `RunModeRules`) + `scripts/trok_modos.txt` | JOGAR e CRIAR PARTIDA: cartões de modo no estilo da sala do CA (Equipe, Exército solo, Surto, Corrida Armada; EM BREVE desabilitados), descrição, regras, meta e tempo **lidos do cfg do modo**, mapa lembrado por modo (`trok_ui_mapas`), filtro de mapas por prefixo (`mapas`), opções de meta e tempo do modo em CRIAR PARTIDA. Novo modo = um bloco no script. |
| Fundo do menu | `trok_menu.cpp` (`TrokFundoAnimado`) | Sem mapa de fundo: grade que desliza, faixa de luz, linha de varredura e poeira, tudo fraco. `trok_ui_fundo_animado 0` volta ao liso. |

**Testar no PC** (nesta ordem):
1. Entrar num servidor de equipe (ou `map de_dust2` com `trok_modo equipe`): a seleção de time aparece
   ~1 s depois; mouse livre; 1/2/3/4 e clique funcionam; ESC fecha **sem** abrir o menu; M abre/fecha.
   Trocar de time vivo mata (regra do HL2MP: conta como suicídio). Em `trok_modo solo`/`surto` não abre.
2. Servidor dedicado com `motd.txt`: boas-vindas aparece, rola, CONTINUAR leva à seleção de time.
3. `spectate` (ou ASSISTIR): cartão do espectador, MOUSE1/MOUSE2 trocam o alvo, ESPAÇO alterna 1ª/3ª;
   CTRL não abre mais o menu do Source.
4. Chat (Y e U): fonte do mod, acento digitado (á, ç, ã), cores de time, prompt; mensagens "(TIME)"
   em português (se aparecer `HL2MP_Chat_All` cru, o motor não leu `mod_trok_<idioma>.txt`: conferir o nome
   com `con_logfile` / o idioma da Steam).
5. `centerprint`/avisos do servidor e `HintText` com a fonte nova.
6. SERVIDORES: aba INTERNET lista (ou diz que não há); abrir uma sala em REDE LOCAL num PC e ver no outro;
   CONECTAR POR IP; servidor com senha.
7. JOGAR/CRIAR PARTIDA: cartões de modo, trocar de modo troca o mapa lembrado, meta/tempo mostrados batem
   com `cfg/<modo>.cfg`, Corrida inicia com `trok_modo corrida`.
8. Fundo animado leve (sem queda de FPS) e o texto do menu legível por cima.

**Não feito / pendente:**
- **Console**: o console é do GameUI e usa o `SourceScheme.res` inteiro (cores `Console.TextColor`,
  `Console.DevTextColor`, fonte `ConsoleText`, cores de `Frame`). Não dá para mudar só um pedaço: o arquivo
  do mod substitui o do HL2 inteiro (e o `#base` não alcança o arquivo de dentro do VPK). Receita para o PC:
  extrair `resource/SourceScheme.res` do `hl2_misc_dir.vpk` do SDK Base (VPK tool / GCFScape), copiar para
  `game/mod_trok/resource/`, trocar as cores (`Frame.BgColor` `20 23 28 240`, `Console.TextColor`
  `236 238 241 255`, `Console.DevTextColor` `242 181 68 255`) e a fonte `ConsoleText` (Barlow 9 ou uma
  monoespaçada), e listar os .ttf em `CustomFontFiles`. Conferir que as outras janelas do GameUI que ainda
  aparecem (confirmações) continuam legíveis.
- **Indicador de voz**: continua o do HL2MP (`hud_voicestatus.cpp`). Próximo: um elemento do HUD do Trok com
  `GetClientVoiceMgr()->IsPlayerSpeaking(i)` e esconder o do Source.
- "Escolher time" ainda não está na lista de teclas de CONFIGURAÇÕES > Controles (a coluna já está cheia
  em 720p; precisa de rolagem). O bind padrão é M no `config_default.cfg`.
- Servidores: favoritos, histórico e filtros (cheio, senha, mapa) ainda não.

## Ainda do GameUI (com cara de Source) e próximos passos

1. **Testar no jogo** (nada foi visto rodando): fontes carregando (nome da família no Windows), escala,
   foco do teclado, ESC na partida, clique em cada item. Ajustar posições olhando a tela.
2. Configurações, criar partida e servidores já são próprios. Ainda do GameUI: caixas de confirmação e o
   console (ver acima).
3. Tela de carregamento: feita; falta a arte de cada mapa no fundo.
4. Mapa de fundo: hoje é a animação por código. Para um mapa ao vivo: fazer `maps/background01.bsp` próprio
   (câmera lenta, `point_viewcontrol`), pôr `"background01"` em `scripts/chapterbackgrounds.txt` do mod
   (o motor sorteia um mapa dessa lista) e `map_background background01` no `autoexec`; o menu já escurece o
   lado esquerdo por cima do mapa (`RunBackground`). Mapa do CS:S não serve: precisa ser compilado como fundo.
5. Arsenal e Perfil (concepts prontos): precisam de sistema de loadout no servidor (hoje é
   `trok_armas_iniciais`).
6. Placar do TAB: feito (`client/trok/ui/trok_scoreboard.cpp`, troca o do HL2MP no
   `CreatePanelByName`). Falta: estilo do bot (o cliente não sabe), assistências, minimapa.
7. Para portar mais widgets prontos (tabelas, campos de texto, rolagem), o NeoUI do NT;RE
   (`neo_ui.cpp`, ~3700 linhas) é o atalho; o menu atual não depende dele.

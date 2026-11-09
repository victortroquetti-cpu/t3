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
| Times | azul `#5AA2FF`, vermelho `#FF5A5A` |
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
O HUD do HL2 (vida, colete, munição, energia, mortes, mira) sai por `#ifndef TROK`.
Ficou de fora: minimapa (precisa de imagem de cada mapa), placar do TAB (próximo).

## Ainda do GameUI (com cara de Source) e próximos passos

1. **Testar no jogo** (nada foi visto rodando): fontes carregando (nome da família no Windows), escala,
   foco do teclado, ESC na partida, clique em cada item. Ajustar posições olhando a tela.
2. Configurações, lista de servidores e criar partida ainda abrem os diálogos do GameUI
   (`OpenOptionsDialog`, `OpenServerBrowser`, `OpenCreateMultiplayerGameDialog`). Próximo: opções
   próprias (receita do NT;RE em `neo_root_settings.cpp`: cópia das ConVars, salvar/descartar,
   teclas por `kb_act.lst`).
3. Tela de carregamento: feita; falta a arte de cada mapa no fundo.
4. Mapa de fundo: fazer um `background01.bsp` próprio (câmera lenta) e o `chapterbackgrounds.txt`.
5. Arsenal e Perfil (concepts prontos): precisam de sistema de loadout no servidor (hoje é
   `trok_armas_iniciais`).
6. Placar do TAB como `IViewPortPanel` (NT;RE `neo_scoreboard.cpp`), minimapa.
7. Para portar mais widgets prontos (tabelas, campos de texto, rolagem), o NeoUI do NT;RE
   (`neo_ui.cpp`, ~3700 linhas, mesma licença) é o atalho; o menu atual não depende dele.

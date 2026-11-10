# Estudo: NEOTOKYO Rebuild (NT;RE) para o Trok

Feito em 09/11/2026 lendo `refs/neotokyo-rebuild` (só código). Base deles: **Source SDK 2013 MP, update
2025 do TF2**, a mesma do Trok. Código reaproveitado leva **crédito** (cabeçalho do arquivo + tela de
créditos: "partes baseadas em NEOTOKYO Rebuild").

Abreviação de caminho: `NT:` = `refs/neotokyo-rebuild/src/game/`. `#ifdef NEO` marca cada mudança
deles em arquivo da Valve (o equivalente ao nosso `#ifdef TROK` + `patches/sdk.patch`).

---

## 1. Menu principal e UI no lugar do GameUI (prioridade)

### 1.1 Como substituem o menu

- **Ponto de entrada:** `NT:client/vgui_int.cpp`, `VGui_CreateGlobalPanels()`: com `#ifdef NEO` e
  fora de `-tools`, chama `OverrideUI->Create(0U)` e `OverrideGameUI()`.
- `NT:client/neo/ui/OverrideInterface.cpp`: `COverrideInterface` (Create/GetPanel/Destroy) cria `CNeoRoot`.
- `NT:client/neo/ui/neo_root.cpp`, `OverrideGameUI()`: carrega o `GameUI` com `CDllDemandLoader
  g_GameUIDLL("GameUI")`, pega `IGameUI` pela factory (`CNeoRoot::LoadGameUI`) e chama
  `SetMainMenuOverride(g_pNeoRoot->GetVPanel())`. A interface existe no nosso SDK:
  `sdk/src/common/GameUI/IGameUI.h:114`.
- **Um painel só** (`CNeoRoot : EditablePanel`, tela inteira, `SetProportional(false)`, fundo
  transparente) é o menu inteiro: raiz, opções, criar servidor, lista de servidores, créditos e
  popups. A troca de tela é uma máquina de estados (`RootState`: `STATE_ROOT`, `STATE_SETTINGS`,
  `STATE_NEWGAME`, `STATE_SERVERBROWSER`... e popups a partir de `STATE__POPUPSTART`) com uma tabela
  de ponteiros de função `MainLoopRoot/MainLoopSettings/...` em `CNeoRoot::OnMainLoop`.
- **Pausa dentro do jogo = o mesmo painel.** Com ESC no jogo o GameUI mostra o override; `MainLoopRoot`
  acrescenta "Continuar" e "Desconectar" quando `IsInGame()` (que exclui o mapa de fundo:
  `engine->IsInGame() && !engine->IsLevelMainMenuBackground()`, em `neo_utils.cpp`). Os dois botões
  chamam `GetGameUI()->SendMainMenuCommand("ResumeGame" / "Disconnect")`.
- Ações viram comandos: criar servidor = ConVars (`hostname`, `sv_password`, quota de bots...) +
  `"maxplayers N; progress_enable; map X"`; entrar = `"progress_enable; wait; connect IP"`; sair = `quit`.

### 1.2 Sistema de desenho próprio: NeoUI (GUI imediata)

- `NT:client/neo/ui/neo_ui.h/.cpp` (~700 + 3700 linhas). Comentário no topo do `.h` explica: GUI
  imediata (estilo Dear ImGui) em cima do VGUI2. **Todos** os eventos do painel (`Paint`,
  `OnMousePressed`, `OnCursorMoved`, `OnMouseWheeled`, teclas) gravam o evento no `NeoUI::Context` e
  chamam a **mesma** função com um `NeoUI::Mode` (`MODE_PAINT`, `MODE_MOUSEPRESSED`...). O código da
  tela roda igual em todos os modos; só no `MODE_PAINT` desenha.
- API: `BeginContext/EndContext`, `BeginSection/EndSection` (área com rolagem; até 7 por contexto),
  `SetPerRowLayout(n, proporções%)`, widgets `Button`, `Label`, `RingBox` (seletor < >), `Slider*`,
  `TextEdit` (com senha), `Tabs`, `Table/TableHeader` (colunas redimensionáveis e ordenáveis),
  `ColorEdit`, `Progress`, `ImageTexture`, `OpenPopup/ClosePopup`, `Bind(KEY_F4)` para atalhos,
  `HintAlt(L"Voltar (ESC)", L"Voltar (B)")` para teclado ou controle. Foco por teclado/controle,
  som de botão (`SECTIONFLAG_PLAYBUTTONSOUNDS`), rolagem por arrasto.
- **Tema** separado: `neo_theme.cpp`, `SetupNTRETheme()` só preenche `NeoUI::Colors`. O visual do
  NT vem de cores + fontes + o layout que cada `MainLoop*` monta (título desenhado à mão com
  `surface()->DrawPrintText`, faixa escura atrás dos botões).
- O mesmo NeoUI desenha o **placar** (`neo_scoreboard.cpp`), a **tela de carregamento**
  (`neo_loading.cpp`) e partes do HUD de espectador: um contexto (`NeoUI::Context`) por painel.
- Imagens: `NeoUI::TextureFromFile` lê PNG/JPG em tempo de execução com `stb_image` e cria a textura
  com `surface()`; serve para avatares, sprays e logos de time sem gerar VTF.

### 1.3 Fontes e escala

- Fontes vêm do `resource/ClientScheme.res` (o painel carrega o esquema com
  `LoadSchemeFromFileEx(enginevgui->GetPanel(PANEL_CLIENTDLL), "resource/ClientScheme.res",
  "ClientScheme")`) e são pegas por nome com `pScheme->GetFont("NeoUINormal", true)` (o `true` =
  proporcional à altura da tela). Seis papéis fixos (`EFont`: normal, título, logo, grande...).
- Escala: tudo derivado do tamanho da tela em `CNeoRoot::ApplySchemeSettings` (referência 1080p):
  altura de linha `tall/27`, margens `wide/192` e `tall/108`, largura do conteúdo = 90% da largura
  4:3 da tela (`tall*4/3`, limitada à largura real; não espicha em ultrawide). `ApplySchemeSettings`
  roda de novo quando muda a resolução; a tela de carregamento confere o tamanho a cada quadro.

### 1.4 Entrada de teclado (a maior gambiarra)

- Comentário `NEO JANK` em `neo_root.h`: o VGUI **não entrega teclas** a painel que não é popup ou
  modal; o painel raiz não pode ser popup (bagunça a ordem com outros popups) nem modal (prende a
  entrada). Solução: `CNeoRootInput`, painel 1x1 invisível, `MakePopup`, só teclado, que repassa
  `OnKeyCodeTyped/OnKeyTyped` ao `CNeoRoot`. Também faz o "segurar para repetir" do controle e a
  captura de tecla para atalhos (`engine->CheckDoneKeyTrapping`) em `OnThink`.
- **ESC nas subtelas:** patch em `NT:client/cdll_client_int.cpp`, `CHLClient::HandleUiToggle()`:
  devolve `NeoRootCaptureESC()` (true fora da raiz) para o motor não fechar o menu e a subtela tratar
  ESC como "voltar". Também manda `hideconsole`. O mesmo método existe no nosso SDK (`cdll_client_int.cpp:2551`).
- Atalhos ligados a comandos (console, MP3) são descobertos varrendo `gameuifuncs->GetBindingForButtonCode`
  no máximo 1x/s, porque com o menu aberto o `ClientModeShared::KeyInput` não vê a tecla.

### 1.5 Tela de carregamento

- `NT:client/clientmode_shared.cpp` (`ClientModeShared::Init`, `#ifdef NEO`): cria `CNeoLoading` e
  chama `IGameUI::SetLoadingBackgroundDialog(painel)`. O painel recebe "Activate"/"deactivate" em
  `OnMessage`.
- O diálogo de carregamento **continua sendo o do GameUI**, só escondido: `CNeoLoading::FetchGameUIPanels`
  procura por nome `BaseGameUIPanel` > `LoadingDialog`, guarda os filhos `Progress` (ProgressBar),
  `InfoLabel` (Label) e `CancelButton` (Button), e tira o diálogo da tela com
  `ipanel()->SetParent(loadingPanel, 0)`. A cada quadro relê a barra e o texto deles e redesenha com
  NeoUI. Cancelar = `m_pButtonCancel->DoClick()`.
- "Carregando" x "desconectado" (kick, servidor cheio, erro) é decidido pelo **token de localização
  do título** do diálogo (`TITitlePtr()->GetUnlocalizedTextSymbol()` comparado com `GameUI_Loading` e
  `GameUI_Disconnected`). É assim que mostram as mensagens de erro do motor no visual deles.
- Nome do mapa: preenchido antes do `connect`/`map` e no evento `game_newmap`.

### 1.6 Mapa de fundo

- O GameUI escolhe o mapa de fundo por `scripts/chapterbackgrounds.txt` + `sv_unlockedchapters`, o que
  não é confiável. `NeoSettingsBackgroundsInit/NeoSettingsBackgroundWrite` (`neo_root_settings.cpp`)
  leem a lista real de `scripts/neo_backgrounds.txt` e **reescrevem** `chapterbackgrounds.txt` com uma
  entrada só (a escolhida ou sorteada). Vale na próxima abertura do jogo.
- Câmera do fundo segue o mouse: `NT:client/view.cpp` (`#ifdef NEO`, `cl_neo_background_pan`), curva
  em S + lerp nos ângulos quando `IsLevelMainMenuBackground()`.
- HUD, jogador e regras checam `IsLevelMainMenuBackground()` para não rodar no fundo.

### 1.7 Opções (vídeo, áudio, teclas)

- `neo_root_settings.cpp`: struct `NeoSettings` com cópia editável de tudo + `CVR` (ConVarRefs).
  `NeoSettingsRestore` (ConVars → struct), `NeoSettingsSave` (struct → ConVars + `host_writeconfig`),
  `NeoSettingsResetToDefault`. Abas = funções `NeoSettings_General/Keys/MouseController/Audio/Video/
  Crosshair/HUD`. Popup "salvar ou descartar" ao sair com mudanças (`bModified`).
- Teclas: lista de ações de `scripts/kb_act.lst`, padrões de `scripts/kb_def.lst` (formato do GameUI),
  tecla principal + secundária; aplica com `unbind`/`bind` via `ClientCmd_Unrestricted`.
- Vídeo: `gameuifuncs->GetVideoModes` para a lista de resoluções; aplica com `mat_setvideomode L A modo`;
  modo janela/sem borda lido de `g_pMaterialSystem->GetCurrentConfigForVideoCard()`; qualidade =
  ConVars (`mat_picmip`, `r_rootlod`, `mat_antialias`, `mat_queue_mode`...).

### 1.8 Lista de servidores

- `neo_root_serverbrowser.cpp`: `ISteamMatchmakingServers` direto (Internet, LAN, amigos, favoritos,
  histórico, espectador) com filtro `{"gamedir", "neo"}`; `PlayerDetails` para jogadores e
  `PingServer` antes de entrar (servidor cheio → modo "entrar quando abrir vaga").
  Blacklist própria em `cfg/ntre_server_blacklist.txt` (nome diferente do arquivo do SDK de propósito).
- `NetAdrIsSDR`: servidor por Steam Datagram Relay (IP 169.254.x.x) **não** desconta bots da contagem.

### 1.9 Armadilhas documentadas nos comentários (UI)

| Onde | Problema | Saída deles |
|---|---|---|
| `neo_root.cpp`, `OnMainLoop` | Sair cedo da função com `DrawSetAlphaMultiplier(0)` ativo derruba o jogo (issue #1277) | sempre volta o multiplicador a 1 no fim |
| `neo_ui.cpp` topo | `surface()->DrawUnicodeChar` com defeito | desenhar caractere único como string com `DrawPrintText` |
| `neo_loading.cpp`, "Activate" | A tela de carregamento trava o cursor por software | força cursor do sistema no carregamento e restaura depois (`SwCursorHack_RestoreValue`) |
| `neo_root.cpp`, `NeoToggleconsole` | `toggleconsole` só funciona com `con_enable 1`, e opções antigas zeram | liga `con_enable` na hora de abrir; volta o estado para a raiz antes |
| `neo_root.cpp`, teclas | Console aberto com tecla diferente de ` era tratado duas vezes | filtra `KEY_BACKQUOTE` |
| `neo_root_settings.cpp` | `GetButtonCodeForBind("+duck")` devolve a tecla de `toggle_duck` | marca como sem tecla se `+duck` não estiver no mapa |
| `neo_root_settings.cpp` | Logo depois de `bind`, `GetButtonCodeForBind` ainda não reflete | pula a releitura das teclas depois de restaurar padrão |
| `neo_root_settings.cpp` | Sair do teste de microfone deixa o ícone de voz aceso | `GetClientVoiceMgr()->UpdateSpeakerStatus(-1, false)` |
| `neo_root.cpp` | Teclas multimídia chegam como `KEY_NONE` e disparam widgets sem tecla | ignora `code <= KEY_NONE` |
| `neo_loading.cpp` | Botão SELECT do controle não chega por `OnKey*` | consulta `vgui::input()->WasKeyPressed` no loop |

O que **continua do GameUI**: o console, o diálogo de carregamento (escondido, mas é ele que recebe
o progresso e o erro do motor), a imagem de fundo estática (quando não há mapa de fundo), o
`FileOpenDialog` do VGUI (escolher spray/logo) e os comandos "ResumeGame"/"Disconnect". Não vi
tratamento para outras caixas de mensagem do GameUI (ex.: aviso de VAC, senha pedida pelo motor);
conferir no jogo se alguma ainda aparece com cara de Source.

### 1.10 Recomendação para o Trok (menu + carregamento)

1. **Copiar a arquitetura inteira**, trocando nomes (`TrokUI`, `CTrokRoot`, `CTrokLoading`):
   - `COverrideInterface` + criação em `VGui_CreateGlobalPanels` (`#ifdef TROK`, pulando `-tools`);
   - o painel de repasse de teclado (`CNeoRootInput`): não tem como evitar;
   - patch em `HandleUiToggle` (ESC nas subtelas);
   - `SetLoadingBackgroundDialog` + captura dos filhos do `LoadingDialog`;
   - reescrita do `chapterbackgrounds.txt`.
2. **Portar o NeoUI quase inteiro** (`neo_ui.h/.cpp` + `neo_theme.cpp`), com crédito. Ele é genérico:
   widgets, tabelas, texto, popups, controle e rolagem já resolvidos (3700 linhas que não precisamos
   reescrever). Tirar o que é do NT (`FONT_LOGO` fixo, textos `HintAlt` em inglês, sprays). A cara
   do Trok sai do tema (cores), das fontes nossas no `ClientScheme.res`, do layout de cada tela e do
   fundo, não da biblioteca. Para não lembrar o CS:S: nada de janelas com barra de título e moldura
   (tudo é desenho livre em tela cheia), fontes próprias, transições (o NT já tem fade com
   `DrawSetAlphaMultiplier`).
3. Telas mínimas da primeira versão: raiz (Jogar contra bots, Lista de servidores, Criar servidor,
   Opções, Sair), opções (Vídeo/Áudio/Teclas/Mouse/Jogo), criar servidor (mapa, bots com
   `hl2mp_bot_difficulty`/quota, senha), lista com filtro `gamedir` = pasta do nosso mod
   (`mod_trok`), carregamento e pausa. Texto em português direto no código ou num `trok_ui_*.txt`
   de localização (eles fixaram muito texto em inglês no código; evitar).
4. Ordem de trabalho: (a) override + painel vazio que só desenha um botão "Sair" (prova o caminho
   no jogo); (b) NeoUI portado + raiz + pausa; (c) carregamento; (d) opções; (e) servidores.

---

## 2. HUD

### 2.1 Como fazem

- Cada elemento é `CHudElement` + painel (`EditablePanel`/`Panel`) **do tamanho da tela**, pintado à
  mão com `surface()` (`neo_hud_ammo.cpp`, `neo_hud_health_thermoptic_aux.cpp`...). Posição, tamanho
  e cores vêm do `scripts/HudLayout.res` por `CPanelAnimationVarAliasType` (ajuste sem recompilar).
- Base comum `CNEOHud_ChildElement` (`neo_hud_childelement.h`): separa `UpdateStateForNeoHudElementDraw()`
  (lê dados do jogo, limitado por um ConVar de frequência por elemento,
  `NEO_HUD_ELEMENT_DECLARE_FREQ_CVAR`) de `DrawNeoHudElement()` (desenha todo quadro); não desenha
  no carregamento nem no mapa de fundo; caixa arredondada com 4 texturas de canto.
- **Esconder HUD pelo servidor:** `CNetworkVar m_iHiddenHudElements` nas regras (`neo_gamerules.h`);
  cada elemento tem um bit (`m_iHideHudElementNumber`). Útil para mapas/tutorial/modos.
- **HUD do HL2 desligado na fonte:** `#ifndef NEO` em volta do `DECLARE_HUDELEMENT` em
  `hl2/hud_health.cpp`, `hud_ammo.cpp`, `hud_battery.cpp`, `hud_suitpower.cpp`, seleção de arma etc.
- Elementos: munição, vida/camuflagem/energia, bússola, estado da rodada (barra do topo com placar e
  tempo), killfeed (`neo_hud_deathnotice.cpp`, ouve `player_death`), indicador de dano, "quem te
  matou" (dano dado/recebido), marcadores de aliados (IFF), ping no mapa, dica contextual, nome do
  lugar, overlay de espectador, mensagens.
- Ícones do killfeed como **textura** (não glifo de fonte): `ClientModeHL2MPNormal::Init` registra
  `{"neo_m41", "vgui/hud/weapons/m41.vmt"}` com `gHUD.AddSearchableHudIconToList`, e `NT:client/hud.cpp`
  tem um `#ifdef NEO` que usa a textura inteira como retângulo (`rc`) desses ícones.
- **Placar:** `CNEOScoreBoard` (`neo_scoreboard.cpp`) é `IViewPortPanel` desenhado com NeoUI (clique
  em jogador abre cartão com mudo/perfil), criado em `CHudViewport::CreatePanelByName`
  (`NT:client/hl2mp/clientmode_hl2mpnormal.cpp`). Escala pelo mesmo esquema (`tall/30`).
- **Time/classe/loadout:** ainda VGUI clássico (`vgui::Frame` + `LoadControlSettings(.res)`) em
  `NT:client/neo/game_controls/`. Ficou com outra cara que o resto.

### 2.2 Para o Trok

- Criar `CTrokHudElement` no molde do `CNEOHud_ChildElement` (sem os cantos com textura deles) e
  migrar o `hud_trok_hitmarker.cpp` para ele. Bitmask de HUD escondido nas regras do Trok.
- Desligar HUD do HL2MP com `#ifndef TROK` nos `DECLARE_HUDELEMENT` (vai para o `sdk.patch`), em
  vez de esconder pelo `HudLayout.res` (que o jogador pode sobrescrever).
- Killfeed próprio com ícones-textura das nossas armas (o caminho do `AddSearchableHudIconToList`).
- Placar e menu de time **no TrokUI**, não em `.res`: uma linguagem visual só (o NT não fez e ficou misturado).
- Placar: registrar em `CHudViewport::CreatePanelByName` (existe no nosso
  `sdk/src/game/client/hl2mp/clientmode_hl2mpnormal.cpp:70`).

---

## 3. Bots

### 3.1 Arquitetura

`CNEOBot : NextBotPlayer<CNEO_Player>` (`NT:server/neo/bot/neo_bot.h`) é o **CTFBot do TF2 portado**
(ainda tem resto de sentinela/uber nos comentários), com ações em `behavior/` por modo de jogo
(`ctg_*` captura da "ghost", `jgr_*` juggernaut) e muitas correções no próprio NextBot
(`NT:server/NextBot/...`, `#ifdef NEO`). O nosso é `CHL2MPBot` com peças trocadas por `#ifdef TROK`.

### 3.2 Comparação direta

| Tema | NT;RE | Trok (`server/trok/bot/`) | Veredito |
|---|---|---|---|
| Percepção | Visão do TF2: tempo mínimo fixo por nível (1,0/0,5/0,3/0,2 s, `neo_bot_vision.cpp`) | barra de percepção por fatores + ex-gaussiana (`CTrokBotVision`) | **nosso melhor** |
| Ponto de mira | `SelectTargetPoint` usa `m_idealTargetPoint`: o 1º ponto visível dos traços (centro/olho/pés), patch em `NextBotVisionInterface.cpp` | erro humano, peito/cabeça, desvio passeando | nosso melhor; a ideia "mirar no ponto que o traço viu" é boa para alvo meio coberto |
| Custo de caminho | `CNEOBotPathCost` (`neo_bot_path_cost.cpp`) | `CTrokBotPathCost` (`trok_bot_team.cpp`) | **o deles tem mais** (abaixo) |
| Seguir caminho | várias correções no `PathFollower` | NextBot puro | **o deles** |
| Tiro sem ver | `CNEOBotSuppressiveFire` | não tem | trazer |
| Som | `NEOMemorySoundCombat`: amostra o `CSoundEnt` todo tick | passos por velocidade/linha reta | trazer o detalhe da amostragem |
| Mira pelo cano | `ShouldAim`: mira quando no alvo, ou parado com luneta | bots não usam ADS | trazer |
| Granada | arremesso balístico + fuga de granada (calcula quique) | não tem | quando houver granada |
| Escada | `ladder_approach/climb` (805 linhas) | NextBot puro | se houver escada nos mapas |
| Inclinar | auto-lean por traços também nos bots | — | se fizermos lean |
| Estilo/tática/memória do time | esquadrão, "seguir comando" do jogador | estilos, mapa tático, avisos, âncoras | nosso mais rico |
| Quota | `neo_bot_quota_mode` fill/match, `join_after_player`, `auto_vacate` | do HL2MP | conferir se o nosso tem fill/match |

### 3.3 O que trazer, concreto

1. **Correções do `PathFollower`** (`NT:server/NextBot/Path/NextBotPathFollow.cpp`, blocos `#ifdef NEO`):
   repath no meio do pulo de vão aponta para o pouso, não para a decolagem atrás; queda (`DROP_DOWN`)
   já em queda pula para o segmento seguinte; `HasFallenBelowPath` (caiu/empurrado da borda → repath
   ao pousar, com contador para não repetir o mesmo caminho); não cortar caminho em área
   `NAV_MESH_PRECISE`; contorno de props (`neo_bot_prop_detour.*`, `neo_bot_path_obstacles.*`) e
   quebrar quebráveis no caminho. O nosso parkour (pulo agachado, escalar, caixas) bate exatamente
   nesses casos. Portar como `#ifdef TROK` no `sdk.patch`.
2. **Penalidades que o time aprende na partida** (`neo_bot_path_reservation.*`): área onde bot travou
   (`OnStuck` soma 1000; vira "propensa a travar" depois de N vezes e sai até das buscas de cobertura)
   e onde bot morreu (`OnKilled` soma 10). Nós já usamos `IncreaseDanger` na morte (decai); falta o
   **travou**: barato e corrige bot repetindo o mesmo erro.
3. **Exposição pela arma** no custo: `area->GetPotentiallyVisibleAreaCount()` × peso. Pistola/SMG e
   escopeta evitam área aberta; fuzil e luneta **preferem** (`custo += base / áreas visíveis`).
   Trazer como campo no bloco `"Trok"` do script da arma (ex.: `bot_exposicao`). Precisa de `nav_analyze`.
4. Amontoamento: eles somam `n² × 3000` por colega com caminho reservado na área; nós multiplicamos
   por uso recente. Testar o quadrado se ainda houver fila em corredor.
5. **Fogo de supressão** (`neo_bot_suppressive_fire.cpp`): ao levar tiro ou ouvir tiro de quem não vê,
   atira 2–3 s (sorteado, para o time não parar junto) na posição suposta, com erro de direção de 10°
   e de distância de 20%, varrendo ±64u; rajadas de 0,1 s para reconferir a linha de tiro (colega
   entrando na frente); só se a ameaça foi vista há menos de 3 s. Combina com nosso "procurar última posição".
6. **Áreas de perigo** (fumaça, granada, explosivo) como áreas proibidas no caminho, com exceção:
   bot com pressa atravessa perigo não mortal (fumaça) com custo ×20.
7. **Bot mirando pelo cano** com as armas do Trok (`ShouldAim` + `ShouldWalk`: anda enquanto atira
   ou mira, e anda até a recarga começar para o sprint não bloquear a recarga).

### 3.4 O que evitar

- `NEO Jank Cheat` em `CNEOBotPathCost`: na rota segura o bot evita caminhos **reservados por bots
  inimigos**. Fere nossa regra (dificuldade nunca muda informação). Não trazer.
- Tempo de reconhecimento fixo por nível e `GetHeadAimTrackingInterval` de 1 s no fácil: o nosso é melhor.
- `OnStuck` deles = pular + strafe aleatório + log; útil só como último recurso.

### 3.5 Armadilhas documentadas (bots)

- Sons de combate ficam só ~0,2 s na lista do motor (`CSoundEnt`): quem lê a cada 0,25 s perde tiros.
  Amostrar todo tick (`NEOMemorySoundCombat::Update`) e guardar por 3 s, agrupando em 500u.
- Comandos de nav (`nav_load`, edição) renumeram áreas sem trocar de mapa: cache por ID de área
  precisa ser zerado quando muda o número de áreas (`s_nDuckLaneAreaCount`).
- Hull agachado mais alto que `HumanCrouchHeight` não passa em toda área "crouch" do nav: eles traçam
  "faixas" no portal e guardam o veredito (`NeoDuckLaneFits`). Vale se mudarmos o hull.
- Smoke: o traço de visão bate na entidade bloqueadora de fumaça; eles marcam a área como perigo
  para o time (patch em `NextBotVisionInterface::IsLineOfSightClearToEntity`).

---

## 4. Armas e movimento

### 4.1 Mira pelo cano (ADS)

- Estado no **jogador** (`m_bInAim`, rede + predição), não na arma. `CNEO_Player::CheckAimButtons`
  (`neo_player_shared.cpp`) aceita **segurar** (`IN_ZOOM`) e **alternar** (`IN_AIM`); correr cancela.
- `Weapon_SetZoom` chama `SetFOV(this, AimFov, NEO_ZOOM_SPEED=0.115)` **no cliente e no servidor**
  (`c_neo_player.cpp:1695`, `neo_player.cpp:1769`). O nosso `CWeaponTrokGun::SetADS` só chama no
  servidor (`#ifndef CLIENT_DLL`): o zoom chega atrasado e pode tremer na predição.
  `CBasePlayer::SetFOV` é compartilhado (`baseplayer_shared.cpp:1952`); chamar dos dois lados.
- Arma na tela: `CNEOPredictedViewModel::CalcViewModelView` interpola entre `ViewModelOffset` e
  `AimOffset` do script pelo **tempo desde a troca** (`m_flStartAimingChange`), só fora da predição
  (`!prediction->InPrediction()`). O nosso usa `frametime` acumulado em `AddViewmodelBob`; funciona,
  mas o modelo por tempo é mais previsível.
- **FOV da arma na tela por arma, normal e mirando:** `ClientModeHL2MPNormal::GetViewModelFOV()`
  (`NT:client/hl2mp/clientmode_hl2mpnormal.cpp:216`) lê `VMFov` e `ZoomOffset.fov` do script e
  interpola no mesmo tempo; soma `neo_viewmodel_fov_offset` (opção do jogador). **É exatamente a
  pendência `mira_fov_arma`** do `sistemas-jogabilidade.md`: copiar esse override para o nosso
  `ClientModeHL2MPNormal` (que hoje não sobrescreve `GetViewModelFOV`).

### 4.2 Recuo

- `CNEOBaseCombatWeapon::AddViewKick` (`weapon_neobasecombatweapon.cpp:931`), duas partes:
  1. **chute** (`ViewPunchReset` + `ViewPunch` sorteado em min/max por `SharedRandomFloat`,
     escalado pela penalidade de precisão acumulada): volta sozinho;
  2. **subida permanente**, só no cliente e só em `prediction->IsFirstTimePredicted()`:
     `engine->GetViewAngles` + soma + `engine->SetViewAngles`, com fator diferente no quadril e mirando.
     O servidor recebe pelo `usercmd`, então não precisa de rede nova.
- A parte 2 é a pendência "subida permanente como no CS:GO". Para o Trok: aplicar o nosso padrão
  (vertical crescente + lateral em seno) **metade no chute e metade permanente**, com campos novos no script
  (`recuo_permanente`, `recuo_mira`). Aviso deles no código: `ViewPunchReset` também zera o soco de
  dano (TODO não resolvido); pensar antes de resetar.
- Precisão: `m_flAccuracyPenalty` sobe por tiro e com movimento, decai por segundo; é o nosso `m_flExtraSpread`.

### 4.3 Inclinar (lean)

- `NT:shared/neo/neo_predicted_viewmodel.cpp`, `lean()`: `m_flLeanRatio` (-1..1) **na viewmodel
  prevista** (rede + interpolação), suavizado por `EaseOut` (`neo_lean_speed` 0,2 s), deslocamento
  lateral do olho (`neo_lean_peek_left/right_amount` 7,5/15u, assimétrico) girado pelo yaw, olho 2u
  mais baixo, rolagem da câmera 20° (`engine->SetViewAngles` no cliente; `v_angle` no servidor só
  para bots). Espaço livre medido com hull de 75% do jogador (`freeRoomForLean`), então não atravessa parede.
- Aplicação: `player->SetViewOffset()` + `m_vecLean` somado em `CGameMovement::GetPlayerViewOffset`
  (senão agachar/levantar apaga a inclinação). O tiro sai do olho inclinado nos dois lados.
- 3ª pessoa: `SetBoneController(0, ângulo)` no modelo do jogador. **Armadilha para nós:** os modelos
  de jogador do CS:S não têm controlador de osso para isso; o corpo não inclina e as hitboxes ficam no
  lugar, mas a cabeça visível sai: dá para atirar inclinado sem expor hitbox. Só fazer lean com modelo
  próprio (controlador ou pose param) ou limitar o deslocamento.
- `NEO HACK` em `gamemovement.cpp:5025`: o bloco "Restoring player view height" do `FinishDuck`
  dispara com o olho deslocado; desligaram.
- Auto-lean (10 traços de cada lado do olhar) e opção "inclinar só a arma" (`cl_neo_lean_viewmodel_only`).

### 4.4 Arma na tela e movimento

- **Arma recua contra parede** (`cl_neo_viewmodel_bump`): traço de hull 4u à frente do olho, empurra a
  arma para trás até 24u, no máximo 2,5×/s. Barato, tira a arma de dentro da parede. Trazer.
- Luz do clarão: `ProcessMuzzleFlashEvent` cria `dlight` de 0,05 s. Trazer.
- Correr bloqueia mirar (como o nosso). Bots administram a energia do sprint (`ShouldWalk`).
- `gamemovement.cpp` (`#ifdef NEO`): deslize com `m_slideTime` **em rede** (nosso é sem estado, mais
  simples para predição: manter o nosso); `sv_neo_jumpbuffer` (pulo apertado antes de tocar o chão vale)
  e `sv_neo_jump_cooldown` + `Friction(tempo segurando pulo)` (anti-bunny hop: segurar pular custa
  velocidade). `sv_neo_maxairspeed` = nosso `trok_ar_controle`.

---

## 5. Resumo do que fazer (ordem sugerida)

1. Menu: override + `TrokUI` (NeoUI portado) + repasse de teclado + `HandleUiToggle` + carregamento.
2. `GetViewModelFOV` por arma e `SetFOV` dos dois lados (fecha a pendência da mira).
3. Recuo permanente no cliente (`SetViewAngles` em `IsFirstTimePredicted`).
4. Bots: correções do `PathFollower`, penalidade de "travou", exposição pela arma, supressão, ADS.
5. HUD: base `CTrokHudElement`, HUD do HL2 fora por `#ifndef TROK`, killfeed e placar no TrokUI.
6. Lean só com modelo de jogador próprio.

Crédito obrigatório em todo arquivo derivado: comentário no topo citando o arquivo de origem do NT;RE
e a Source 1 SDK License; mesma nota na tela de créditos do menu.

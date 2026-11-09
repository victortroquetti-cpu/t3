# Estudo: Double Action, Momentum Mod e Fortress Forever → o que levar para o Trok

Feito em 09/11/2026 lendo só o código. Licença: os três usam a Source 1 SDK License, a mesma
do Trok. **Código pode ser reaproveitado com crédito** (cabeçalho no arquivo + linha em
`thirdpartylegalnotices`); **arte, modelos, sons, materiais e shaders deles, não** (ex.: os
sons `Player.GoSlide`, o material `shaders/slowmo`, os .res/fontes do DA).

Abreviações de caminho:
- **DA** = `refs/double-action/mp/src/game` (Double Action: Boogaloo, SDK 2013, código vivo)
- **MOM** = `refs/momentum-mod/mp/src/game` (Momentum Mod, congelado; motor modificado depois)
- **FF** = `refs/fortress-forever` (SDK 2006; só olhar ideias)
- **SDK** = `sdk/src/game` (o nosso SDK 2013 MP 64-bit) · **TROK** = `src/game`

## 0. Resumo: o que fazer primeiro (por ganho / esforço)

1. **Escadas dos mapas do CS:S não funcionam hoje** (seção 1.4-a). Correção pequena, risco alto se ignorar.
2. **Deslize com estado em rede + recarga**: hoje dá para encadear deslize→pulo→pouso agachado
   sem perder velocidade (1.4-b). Padrão do DA (variáveis em rede + `DEFINE_PRED_FIELD`).
3. **"Lurch" do Momentum** em vez de só subir `trok_ar_controle`: troca de lado no ar logo
   depois do pulo, sem ganhar velocidade. É a sensação "caixa a caixa, de um lado pro outro" (1.4-c).
4. **Mira pelo cano**: zoom só no servidor atrasa pelo ping; o balanço (sway) e o deslocamento
   por pitch do viewmodel tiram a alça do centro ao olhar para cima/baixo (2.3).
5. **Arma sem balanço ao andar**: nossa base (`CWeaponHL2MPBase`) não tem bob; só o sprint balança (2.3).
6. **FOV da arma na mira** (`mira_fov_arma` pendente): o DA resolve em `ClientModeSDKNormal::GetViewModelFOV` (2.1).
7. **Menu principal próprio**: receita que funciona no SDK 2013 atual = `IGameUI::SetMainMenuOverride`
   + sequestro do `LoadingDialog` (está no NT;RE, que também está em `refs/`). O do Momentum
   depende de um GameUI.dll próprio (3.2).

---

## 1. Movimento

### 1.1 Double Action (`DA/shared/sdk/sdk_gamemovement.cpp` + `sdk_player_shared.cpp`)

Classe `CSDKGameMovement : CGameMovement` (não herda do HL2). Reescreve `FullWalkMove`,
`WalkMove`, `Duck`, `CategorizePosition`, `CheckJumpButton`, `StepMove`, `AirAccelerate`.

- **Estado**: tudo em `CSDKPlayerShared` (`sdk_player_shared.h`): `m_bSliding`,
  `m_vecSlideDirection`, `m_flSlideStartTime`, `m_flSlideAutoEndTime`, `m_bDiving`, `m_flDiveTime`,
  `m_bRolling`, `m_flRollTime`, `m_iWallFlipCount`, `m_bIsManteling`, `m_vecMantelWallNormal`...
  Cada um tem `SendProp*` (`server/sdk/sdk_player.cpp:112`, tabela `DT_SDKSharedLocalPlayerExclusive`
  enviada só ao dono com `SendProxy_SendLocalDataTable`), `RecvProp*` e **`DEFINE_PRED_FIELD`**
  (`client/sdk/c_sdk_player.cpp:306-337`). Tempos usam `RecvPropTime` + relógio próprio do
  jogador `m_flCurrentTime` (também previsto), por causa da câmera lenta.
- **Deslize** (`StartSliding`, `GetSlideFriction`, `StandUpFromSlide`):
  direção travada na velocidade de entrada; duração fixa `sdk_slidetime` 1,5 s; atrito
  multiplicado por **0,05** até faltar 0,2 s (feito em `CategorizePosition`:
  `player->m_surfaceFriction *= GetSlideFriction()`), depois atrito normal. Em rampa descendo,
  `Duck()` soma `frametime` ao fim (desliza mais ladeira abaixo). No deslize o `WalkMove`
  zera a direção desejada (sem volante). Ao levantar sem pular: `FreezePlayer(0.4, 0.3)`
  (40% da velocidade por 0,3 s); levantando com pulo, sem penalidade (recompensa deslize→pulo).
  Olho desce em `TIME_TO_SLIDE` 0,2 s (`SetSlideEyeOffset`), sobe em 0,2 s (`SetUnSlideEyeOffset`).
  Sem espaço para levantar, vira deitado (`CanUnprone`).
- **Mergulho/rolamento**: `StartDiving` dá velocidade + gravidade 0,6; hull baixa com
  `FixPlayerDiveStuck`; no pouso escolhe deslize / rolamento / deitado pelos botões
  (`Duck()`, bloco `IsDiving`). Queda ≥ 600 u/s vira rolamento (`CheckFalling`).
- **Wallflip** (`FullWalkMove`, bloco `IN_ALT1`): traço 8u à frente; só parede de verdade
  (`|normal.z| < 0,7`, não `SURF_SKY/NODRAW`, olhando para ela `dot < -0,7`); velocidade =
  normal×180 + 420 para cima; limite de flips até pousar (`m_iWallFlipCount`).
- **Mantel** (`CheckMantel` + bloco em `FullWalkMove`): sonda para cima (teto), 4u à frente
  (espaço), desce até achar chão; aceita borda entre `da_acro_mantel_min_height` 40 e 92u;
  rejeita jogador como borda e ignora quebráveis (`ShouldHitFuncIgnoreBreakables`); exige
  parede íngreme (`plane.normal.z ≤ 0,3`). Depois **move a posição** a 144 u/s para cima
  (sem velocidade) enquanto segurar pular, e empurra para dentro da borda quando livre.
- **Câmera lenta**: `ProcessMovement` multiplica `gpGlobals->frametime` pelo
  `GetSlowMoMultiplier()` do jogador e restaura depois. Truque simples e previsível.
- Outros: `ResolveStanding` (do TF2, gruda em degrau sem "pular" a câmera), recuo de
  velocidade de costas (`sdk_clamp_back_speed` 0,9), velocidade terminal pelo pitch,
  `da_auto_unstick` (servidor: preso 3 s → volta ao spawn, em `PlayerMove`).

### 1.2 Momentum (`MOM/shared/momentum/mom_gamemovement.cpp`, 4400 linhas)

Classe `CMomentumGameMovement : CGameMovement`; modos (CS/surf/bhop/KZ/TF2/ahop/parkour) em
`mom_system_gamemode.cpp` (`SetGameModeVars`: CS = accelerate 5, airaccelerate 150, friction 4,
maxspeed 260, stopspeed 75). Hulls do CS:S em `mom_gamerules.cpp:34`: **62 em pé / 45
agachado, olho 64 / 47** (HL2MP: 72 / 36, olho 64 / 28).

- **Lurch** (`PerformLurchChecks`, chamado no `AirMove` do parkour): ao pular,
  `m_Local.m_lurchTimer = 500` ms (`CheckJumpButton`); se no ar o jogador **aperta** uma
  direção nova enquanto o timer > 0, a velocidade é girada em direção ao desejado com força
  que cai com o tempo, **sem aumentar o módulo**. Isso é o "muda de lado no meio do pulo".
- **Coyote time** (`SetGroundEntity`, ao sair do chão): `m_flCoyoteTime = curtime + sv_coyote_time` (0,2);
  `CheckJumpButton` aceita pular no ar nesse intervalo.
- **Powerslide** (`DoDuck` / `CheckPowerSlide` / `PowerSlideFriction` / `EndPowerSlide`):
  agachar acima de `PK_POWERSLIDE_MIN_SPEED` no chão → `Accelerate(dir, 400, 400)` uma vez,
  recarga `m_slideBoostCooldown` 2 s, duck completa mais rápido (`duckTimer = 200`); atrito
  próprio `0,4 × surfaceFriction` (ignora `sv_friction`); `sv_slide_lock` trava a direção;
  câmera inclina para o lado da curva via `m_Local.m_punchRollOverrideTarget = 15·sin(Δyaw)`
  (decai em `CGameMovement::DecayPunchAngle`, `MOM/shared/gamemovement.cpp:1237`).
  Também desliza ao **pousar** segurando agachar (`SetGroundEntity`, `bLanded`).
- **Borda/rampa/inclinação** (`CategorizePosition`): antes de considerar "no chão", simula o
  próximo tick (`sv_edge_fix`) e só gruda se não for escorregar em rampa
  (`vecNextVelocity.z <= NON_JUMP_VELOCITY`) — evita perder velocidade ao pousar em rampa
  (`sv_slope_fix`). `TryPlayerMove` reescrito com `sv_ramp_fix`/`sv_ramp_bumpcount` (surf
  sem travar em junções de rampas). `sv_considered_on_ground` = 1u.
- **Pulo estilo CS**: em modos CS, sobe a origem 1,5u (`sv_jump_z_offset`) para pulos
  consistentes em 64 tick; pulo agachado **não** sobrescreve `z` (só soma).
- **Stamina** (só KZ, `WalkMove`/`CheckJumpButton`/`ReduceTimers`): fórmula do CS
  (`STAMINA_MAX 100, COST_JUMP 25, RECOVER 19`): cada pulo reduz o próximo pulo e a
  velocidade no chão por um tempo. É o que deixa o CS "pesado" no pouso.
- **Recuo CS**: `CMomentumPlayer::KickBack` (`mom_player_shared.cpp:328`) acumula
  `SetPunchAngle` com teto e troca de lado aleatória (`m_iShotsFired`, `m_iDirection`
  previstos); `DecayPunchAngle` linear do CS: `len -= (10 + len*0.5)*dt`.
- **Escada estilo CS**: `LadderMove` com o ajuste do CS:S (`sv_ladder_dampen`, `sv_ladder_angle`).

### 1.3 Predição: o que eles acertam e erram

- Tudo que afeta posição/velocidade e dura mais de um tick **tem que** ser variável em rede
  do jogador + `DEFINE_PRED_FIELD` (tempo: `RecvPropTime` + `DEFINE_PRED_FIELD_TOL(...,
  TD_MSECTOLERANCE)`, como já fazemos em `weapon_trok_gun.cpp:124`). O DA faz direito.
- **Momentum erra**: `m_slideBoostCooldown`, `m_lurchTimer`, `m_punchRollOverride*` são
  `CNetworkVar` em `server/playerlocaldata.h:59` mas **sem `SendProp`** ("MOM_TODO: Network
  these??"); `m_nWallRunState`, `m_nAirJumpState`, `m_flCoyoteTime`, `m_bIsPowerSliding` são
  membros comuns. Online isso dá correção de predição (tranco). Momentum é quase solo; o Trok não.
  Se copiar lurch/coyote/powerslide, **ponha em rede**.
- Sons de movimento: `CPASFilter f(origin); f.UsePredictionRules();` (DA, `PlayStartSlideSound`),
  senão o som toca duas vezes (predição + servidor).
- Nada que use `gpGlobals->curtime` puro para estado de movimento entre ticks sem estar em
  rede (o DA escorrega nisso em `SetDuckPress`, que usa `curtime` e não o relógio previsto).
- Preferência do cliente lida no movimento compartilhado: `GetUserInfoFloat("da_slide_unclick_time")`
  com ConVar `FCVAR_USERINFO` (DA). Útil para "agachar alterna/segura deslize".

### 1.4 O que mudar no `trok_gamemovement.cpp` (concreto)

**a) Escadas (`func_ladder`) — crítico.** `CHL2GameMovement::LadderMove/OnLadder/FullLadderMove`
(`SDK/shared/hl2/hl_gamemovement.cpp:292,566,~935`) só tratam `func_useableladder` do HL2;
**não caem** no `CGameMovement::LadderMove`, que é quem lê `CONTENTS_LADDER` (as escadas dos
mapas do CS:S). Proposta (testar em de_nuke/de_aztec):
```cpp
// GetLadder() é private no CHL2GameMovement; o OnLadder dele devolve GetLadder() != NULL.
bool CTrokGameMovement::HL2Ladder()       { trace_t t; return BaseClass::OnLadder( t ); }
bool CTrokGameMovement::LadderMove()      { return BaseClass::LadderMove() || CGameMovement::LadderMove(); }
bool CTrokGameMovement::OnLadder( trace_t &tr ) { return HL2Ladder() || CGameMovement::OnLadder( tr ); }
void CTrokGameMovement::FullLadderMove()  { if ( HL2Ladder() ) BaseClass::FullLadderMove(); else CGameMovement::FullLadderMove(); }
```
E o ajuste "escada fácil" do CS:S já está no nosso SDK (`gamemovement.cpp:2842,2977`), mas
dentro de `#if defined(CSTRIKE_DLL)`: trocar por `|| defined(TROK)` no patch.

**b) Deslize com estado.** Hoje `IsSliding()` = no chão + agachado + rápido. Problemas:
(1) pular do deslize e pousar segurando agachar reativa o atrito baixo → corrente infinita
(o pulo ainda devolve a velocidade horizontal); (2) agachar andando rápido depois de qualquer
queda também "desliza"; (3) sem recarga; (4) o olho desce em 0,4 s (duck do HL2), lento para deslize.
Fazer como o DA, com 3 variáveis em rede no `CHL2MP_Player` (patch) ou num `CTrokPlayer`:
`m_flSlideEnd` (tempo), `m_flSlideNext` (recarga, ~0,75 s como `TIME_TO_RESLIDE`),
`m_vecSlideDir`. Começa só no aperto de agachar (já é assim), termina por tempo/velocidade/pulo;
atrito baixo só enquanto `curtime < m_flSlideEnd`; durante o deslize, completar o duck em
~0,2 s (`m_Local.m_flDucktime`, como o Momentum faz com `duckTimer = 200`). Opcional: estender
em descida (DA `AddSlideTime`), "deslize no pouso" (Momentum) e penalidade ao levantar sem pular (DA).
Aplicar atrito pelo `player->m_surfaceFriction` em `CategorizePosition` (DA) é mais limpo do
que devolver velocidade depois do `FullWalkMove` — e corrige o risco anotado no HANDOFF
(nossa compensação também anula aceleração/gravidade em rampas).

**c) Lurch em vez de só controle no ar alto.** `trok_ar_controle` 60 deixa curvar o pulo
inteiro (e soma com strafe → bhop). O lurch dá a troca de lado só nos primeiros ~0,3–0,5 s
e nunca acelera. Núcleo (de `PerformLurchChecks`, adaptar):
```cpp
if ( m_flLurchEnd > curtime && ( buttonsPressed & (IN_FORWARD|IN_BACK|IN_MOVELEFT|IN_MOVERIGHT) ) )
{
    Vector cur = vel2D; float speed = VectorNormalize( cur );
    float amt = MIN( ( m_flLurchEnd - curtime ) / 0.3f, 1.0f );         // força cai com o tempo
    Vector dir = Lerp( 0.7f, cur, wishdir * 1.5f ) - cur; VectorNormalize( dir );
    Vector nv = cur * speed + dir * ( trok_lurch_forca * amt );
    if ( nv.Length2D() > speed ) nv *= speed / nv.Length2D();          // gira, não acelera
    mv->m_vecVelocity.x = nv.x; mv->m_vecVelocity.y = nv.y;
}
```
`m_flLurchEnd` em rede (setado no `CheckJumpButton`). Manter `trok_ar_controle` perto de 30–40.

**d) Teto de velocidade no pulo (FF).** `CFFGameMovement::CheckJumpButton` (`FF/game_shared/ff/ff_gamemovement.cpp:250`):
acima de 1,4×maxspeed, o excedente é multiplicado por 0,65 (acima de 1,55× por 0,4); teto
duro 1,71×. Barato, previsível, e mata bhop/deslize-pulo sem tirar fluidez. Aplicar no nosso
`CheckJumpButton` antes de restaurar x/y.

**e) Coyote time** (Momentum, 0,1–0,15 s) ao sair de caixa correndo: perdoa o pulo tardio
na borda. Em rede (`m_flCoyoteEnd`). Também "jump buffer" (aceitar pular 0,1 s antes de
pousar) segue o mesmo padrão — não está nos refs, é ideia nossa.

**f) Escalar (mantle).** Do DA trazer: exigir parede íngreme à frente (`normal.z ≤ 0,3`),
**não aceitar jogador como borda** (hoje dá para "escalar" a cabeça de alguém: nosso traço
de topo bate em jogador), ignorar quebráveis, sondar teto antes. Manter o nosso por
velocidade (é sem estado e previsível) — o do DA move posição por estado e é mais "trilho".

**g) Tamanho do jogador.** Usamos hulls do HL2 (72/36, olho agachado 28) em mapas do CS:S
(62/45, olho 47). Efeitos: pulo agachado ganha +36u (CS:S ~+17) → alcança lugares que o mapa
não previa; vista agachada muito baixa. Avaliar `CTrokRules::GetViewVectors()` com os valores
do CS:S (`MOM/shared/momentum/mom_gamerules.cpp:34`). Cuidado: modelos de 72u e hitboxes; o nav
já assume altura de agachado ~55 (`HumanCrouchHeight`).

**h) Sem stamina do CS** para o Trok (Combat Arms não pune pouso). Se um dia quiser
"peso", a fórmula pronta está no `WalkMove` do Momentum (modo KZ).

**i) Câmera**: inclinação lateral no deslize (Momentum `m_punchRollOverrideTarget`, ou só no
cliente em `CalcView`); "afundada" curta ao pousar (não existe nos refs; dá para usar
`m_flFallVelocity` no cliente). Degraus já são suavizados pelo SDK (`SmoothViewOnStairs`,
`smoothstairs`); o olho do duck usa `SimpleSpline` (ok).

**j) Servidor**: `da_auto_unstick` (DA `PlayerMove`) como rede de segurança para mantle/deslize.

### 1.5 Evitar / armadilhas

- Copiar o `sdk_gamemovement.cpp` do DA inteiro: herda de `CGameMovement`, perderíamos o
  sprint/escada/física do HL2 que o Trok usa. Copiar **funções e padrões**.
- O "door hack" do DA (`FullWalkMove`, `#ifdef GAME_DLL`) empurra `func_physbox` só no servidor → erro de predição.
- `static int iState` em `Duck()` (DA): estado global compartilhado entre jogadores. Não repetir.
- `CNetworkVar` sem `SendProp` (Momentum) não vai pela rede: compila e falha em silêncio.
- Mexer em `gpGlobals->frametime` (DA slow-mo) exige restaurar sempre, inclusive em retorno antecipado.

---

## 2. Viewmodel e sensação

### 2.1 Double Action
- **`CDAViewModel::AddViewModelBob`** (`DA/shared/sdk/da_viewmodel.cpp:257`): (a) atraso por
  velocidade: `m_vecPlayerVelocityLerp` (limitado a 100 u/s, `Approach` 1000/s) × `da_weaponlag`
  × |velocidade·direita| — a arma "fica para trás" ao andar de lado; (b) bob de andar e de
  correr misturados pela rampa `m_flViewBobRamp` (0..1, sobe com a velocidade, zera no ar/deslize,
  `UpdateViewBobRamp` em `sdk_player_shared.cpp:1726`, campo previsto `FTYPEDESC_PRIVATE`);
  frequência: andar π·1,5, correr π·3; vertical no dobro da lateral; (c) "drop" ao correr;
  (d) inclinação (roll) ao mergulhar de lado; (e) animação de abaixar/levantar ao lançar granada
  com `Gain()` (curva suave). Hack: ângulos do olho às vezes vêm (0,0,0) por um frame → guarda o último.
- **Bob da câmera** em `CSDKPlayer::EyePosition()` (shared). **Armadilha**: `EyePosition` é
  a origem do tiro no servidor → o tiro também balança. Fazer bob de câmera só em `CalcView` no cliente.
- **"Aim-in" do DA não é mira pelo cano**: só FOV (`C_SDKPlayer::OverrideView`, `c_sdk_player.cpp:2066`,
  `fov -= aimIn * 40`, calculado **no cliente** a partir da fração prevista → sem atraso) +
  amortecer o mouse (`ClientModeSDKNormal::OverrideMouseInput`, `m_aimindamping` 0,5 interpolado
  pela fração) + bônus de recuo/cadência.
- **FOV do viewmodel por arma**: `ClientModeSDKNormal::GetViewModelFOV` (`DA/client/sdk/clientmode_sdk.cpp`)
  devolve `pWeapon->GetWeaponFOV()`. É exatamente o patch pendente do `mira_fov_arma`:
  sobrescrever `GetViewModelFOV` no ClientMode do HL2MP e interpolar pela fração de mira.
- **Recuo persistente** (o "sobe e fica" do CoD/CS:GO que o Trok não tem): no tiro,
  `SetPunchAngle` (visual) + `m_Shared.SetRecoil(...)`; depois `CWeaponSDKBase::CreateMove`
  (`weapon_sdkbase.cpp:1095`) tira um pedaço do acumulador por frame e soma em
  `pCmd->viewangles` (só no cliente, `IsFirstTimePredicted`). Como entra no `usercmd`, o
  servidor recebe a mira já subida: sem dessincronia.
- **Câmera lenta visual**: `CDAViewRender::PerformSlowMoEffect` (`da_view_scene.cpp:103`)
  desenha um material de pós-processo antes do HUD (`Render2DEffectsPreHUD`); mouse amortecido
  em câmera lenta. Útil para "kill cam" do último abate, se um dia quiserem.

### 2.2 Momentum
- `viewmodel_offset_x/y/z` (ARCHIVE) em `CBaseViewModel::CalcViewModelView`
  (`MOM/shared/baseviewmodel_shared.cpp:25,394`): jogador escolhe posição da arma (como CS:GO).
- `CalcViewModelLag` fora da predição (correção da VDC) — o nosso SDK já faz isso.
- Sem mira pelo cano (só lunetas do CS com `m_iLastZoomFOV` / `m_bResumeZoom`).
- Recuo CS (`KickBack` + `DecayPunchAngle` linear), ver 1.2.

### 2.3 Comparação com o nosso `CWeaponTrokGun::AddViewmodelBob`

| Ponto | Hoje no Trok | Problema | O que fazer |
|---|---|---|---|
| Bob andando | `BaseClass::AddViewmodelBob` | `CWeaponHL2MPBase` herda de `CBaseCombatWeapon`, cujo bob é **vazio** (`basecombatweapon_shared.h:324`) → arma parada ao andar | Bob tipo DA: rampa por velocidade real, andar/correr misturados, zera no ar e no deslize; reduzir a ~10% na mira |
| Sprint | `sinf(curtime*13)` | balança mesmo correndo contra a parede | fase pela distância percorrida ou rampa por velocidade (DA) |
| Sway/lag na mira | `CPredictedViewModel::CalcViewModelLag` roda depois do nosso código | o `CBaseViewModel::CalcViewModelLag` soma deslocamento por **pitch** (`-pitch*0.035/0.03/0.02`) e atraso de giro → alça sai do centro ao olhar p/ cima/baixo; ajuste do `trok_mira_ajuste` só vale num pitch | viewmodel do Trok (ou `#ifdef TROK` no `predicted_viewmodel.cpp`) que escala lag e pitch por `(1 - fraçãoMira)` |
| Zoom | `SetFOV` só `#ifndef CLIENT_DLL` | FOV é previsto (`m_iFOV`/`m_flFOVTime` têm `DEFINE_PRED_FIELD`), mas só o servidor muda → zoom chega um ping depois da arma | chamar `SetFOV` nos dois lados (como as armas do CS:S) **ou** FOV no cliente por fração (DA) |
| Sensibilidade na mira | `zoom_sensitivity_ratio` | ok, mas binário | opcional: `OverrideMouseInput` interpolado (DA) |
| Recuo | `ViewPunch` (mola do HL2) | volta rápido e "quica"; nada sobe de verdade | punch visual curto + recuo persistente por `CreateMove` (DA); padrão/teto tipo `KickBack` (MOM) |
| FOV da arma | `mira_fov_arma` lido, não usado | — | `GetViewModelFOV` no ClientMode (DA) |

`gpGlobals->frametime` em `AddViewmodelBob` é aceitável (só cliente, fora da predição).

---

## 3. Interface

### 3.1 Double Action: **não** substitui o menu
- Mantém o menu do GameUI do motor (botões vêm de `resource/GameMenu.res`) e **acrescenta**
  painéis com `SetParent(enginevgui->GetPanel(PANEL_GAMEUIDLL))`: `CButtonPanel`
  (`vgui/da_menubuttonpanel.cpp`, botões imagem news/leaderboard/options), `COptions`
  (`vgui/da_options.cpp`, um `Frame` com `.res` e `MenuScheme.res` próprio), `CNewsFrame`,
  `CLeaderboard`, `CDACredits`. Criados em `ClientModeSDKNormal::InitViewport`.
- `da_menu_background.cpp` (`CDAMainMenu`) toca vídeo `.bik` de fundo via `IVideoServices`,
  mas **ninguém instancia** (código morto). Antes de contar com vídeo no 64-bit, testar.
- HUD: `CHudElement` + `vgui::Panel`, ícones `CHudTexture` (scripts/hud_textures), fontes do
  `ClientScheme`, números ajustáveis no `.res` via `CPanelAnimationVar`. Mira própria por arma
  e por estado (`da_hud_crosshair.cpp`: `iconCrosshair`/`iconZoomedCrosshair`, mira de
  obstrução em 3ª pessoa). Tutorial contextual: `da_instructor.cpp` + `Instructor_LessonLearned("slide")`
  chamados do movimento — bom para ensinar deslize/escalar no Trok.
- Limite que bateram: pós-processo (ShaderEditor) quebra ao mudar resolução → `g_kill_shaders`
  desliga até reiniciar.

### 3.2 Momentum: substitui tudo, mas com GameUI.dll próprio
- `CBaseMenuPanel` (`MOM/client/momentum/ui/gameui/BaseMenuPanel.cpp`) filho de
  `PANEL_GAMEUIDLL`, com `MainMenu` (+ `drawer/` lateral: lobby, perfil, changelog) e
  `CLoadingScreen` + `TipManager` (dicas). Criado em `VGui_OneTimeInit` (`client/vgui_int.cpp:190`).
  Fundo: `engine->GetMainMenuBackgroundName` + textura `console/<nome>_widescreen`, fade
  entre estados (`EBackgroundState`), escurece quando há diálogo popup.
- **Não é portável como está**: recebe mensagens `GameUIActivated`, `LevelLoadStarted`,
  `ProgressFraction` que vêm do GameUI deles (projeto `$SRCDIR\gameui`, referenciado em
  `client_momentum.vpc:16`, fora da nossa cópia). O GameUI.dll do SDK não manda isso para um painel nosso.
- Configurações: `CMomentumSettingsDialog` + páginas (`SettingsPanel/Gameplay|HUD|Input|Video|Audio`),
  controles ligados a ConVar (`CvarSlider`, `CvarTextEntry`, `CvarComboBox`, `CvarToggleCheckButton`).
  **No nosso SDK só existe `public/vgui_controls/cvartogglecheckbutton.h`**; os outros e o
  `GetScaledVal()` são do vgui_controls modificado deles → escrever os nossos (pequenos) ou portar
  com crédito. Vídeo usa `gameuifuncs->GetVideoModes/GetDesktopResolution`; teclas usam
  `gameuifuncs->GetBindingForButtonCode`.
- HUD: `CHudElement` + `EditablePanel` + `SetProportional(true)` + `LoadControlSettings(.res)`
  (ex. `hud_speedometer.cpp`). Velocímetro/teclas pressionadas: ótimos para **medir** e
  ajustar o movimento contra o Combat Arms (o DA tem `cl_show_speed`).

### 3.3 Receita que funciona no SDK 2013 atual (NT;RE, também em `refs/neotokyo-rebuild`)
Fora dos três pedidos, mas é o mesmo SDK que o nosso e resolve o que o Momentum não resolve:
- Menu: `CDllDemandLoader("GameUI")` → `IGameUI` (`GAMEUI_INTERFACE_VERSION` "GameUI011",
  existe em `sdk/src/common/GameUI/IGameUI.h:114`) → **`SetMainMenuOverride(vpanel)`**;
  ações com `SendMainMenuCommand("ResumeGame"/"Disconnect")` (`neo/ui/neo_root.cpp:134,497`).
- Tela de carregamento: acha `BaseGameUIPanel` → `LoadingDialog` entre os filhos de
  `PANEL_GAMEUIDLL`, tira o pai dele (esconde) e lê a barra `Progress`/`InfoLabel`
  para desenhar a nossa (`neo/ui/neo_loading.cpp:108`, `FetchGameUIPanels`).
- Opções próprias (`neo_root_settings.cpp`) em vez do diálogo de opções do GameUI.

### 3.4 Limites do motor a planejar
- Fontes VGUI são bitmaps por tamanho (uma entrada no scheme por tamanho/peso; TTF próprio via
  `CustomFontFiles` no `ClientScheme.res`). Escala proporcional é pela **altura** (base 480):
  em 21:9 sobra lateral, ancorar com `xpos "r..."`/`"c..."`; mudar resolução refaz tudo em `ApplySchemeSettings`.
- Sem blur/sombra/cantos redondos nativos: texturas (VTF) ou `PaintBackgroundType 2`.
- Diálogos do motor (console, servidor) continuam no GameUI e aparecem por cima — o Momentum
  detecta popups para escurecer o fundo (`UpdateBackgroundState`).
- HUD do HL2MP: esconder pelo `HudLayout.res`/`ShouldDraw`, não apagar código.

### 3.5 Para o Trok
1. Menu: `SetMainMenuOverride` (3.3) com um `EditablePanel` próprio, `.res` proporcional,
   fundo com mapa de fundo ou imagem, fade (copiar a lógica de estados do Momentum).
2. Carregamento: sequestro do `LoadingDialog` + dicas (`TipManager` do Momentum é simples).
3. Opções: um diálogo nosso com 3–4 controles ligados a ConVar (escrever `CvarSlider`/`CvarComboBox`).
4. HUD: padrão `CHudElement + EditablePanel + .res` (Momentum/DA), fontes próprias, sem nada do CS:S.

---

## 4. Bots do DA

`DA/server/sdk/bots/` (~1600 linhas): bot antigo do SDK por waypoints (`CreatePath` com
`NavAreaBuildPath`, `CheckNavMeshAttrib` para agachar/pular), sem usar dive/slide/wallflip.
Nada supera nosso NextBot. Duas ideias pequenas: ao travar, `area->IncreaseDanger()` na área
(`CheckStuck`, os bots evitam aos poucos) e o histórico de botões para depurar bots presos
(`m_button_history`, `STUCK_DEBUG` em `PlayerMove`).

## 5. Fortress Forever (só ideias; SDK 2006)

- Teto de velocidade em 3 faixas no pulo (1.4-d) e "trimp" (rampa → altura, `CheckJumpButton`);
  `cl_jumpqueue` (`FCVAR_USERINFO`, lido com `engine->GetClientConVarValue`): preferência de pulo segurado.
- Mira montada com glifos de fonte, editável por arma e salva em KeyValues
  (`cl_dll/ff/vgui/ff_crosshairoptions.cpp`) — boa base para mira configurável do Trok.
- Menu radial com o mouse (`cl_dll/ff/ff_hud_menu.cpp`, `CHudContextMenu::MouseMove/KeyEvent`):
  serve para comandos de rádio/compra rápida.
- `DECLARE_GAMEUI` (`ff_gameui.h`): macro para criar painéis presos ao GameUI (mesma ideia do DA).

## 6. Crédito ao reaproveitar

No topo do arquivo: `// Adaptado de <projeto> (<arquivo original>), Source 1 SDK License.`
e uma linha por projeto em `thirdpartylegalnotices`/LEIA-ME. Reescrever nomes de ConVar
para `trok_*`. Nunca trazer `.res`, fontes, sons, materiais ou shaders deles.

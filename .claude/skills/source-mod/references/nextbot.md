# IA de bots com NextBot

O SDK 2013 traz o **NextBot** (a IA dos bots do TF2 e dos chefes de Halloween) em
`server/NextBot/`, e o HL2MP já tem bots prontos em cima dele em `server/hl2mp/bot/`.
Os bots do CS:S (`CCSBot`) **não** estão no SDK. Tudo é só servidor.

## Dois jeitos de fazer um bot

| | Bot jogador | NPC NextBot |
|---|---|---|
| Classe | `NextBotPlayer< CHL2MP_Player >` (ex.: `CHL2MPBot`) | `NextBotCombatCharacter` (ex.: `CZombie` em `server/tf/halloween/zombie/`) |
| É jogador? | Sim: ocupa vaga, aparece no placar, usa armas e `usercmd` | Não: entidade do mapa, sem vaga |
| Movimento | o do jogador (`gamemovement`) via `NextBotPlayerLocomotion` | `NextBotGroundLocomotion` (próprio, mais barato) |
| Bom para | bots que jogam como gente (times, armas, objetivos) | hordas, criaturas, chefes, PvE com muitos inimigos |
| Exemplo | `server/hl2mp/bot/hl2mp_bot.*`, `server/tf/bot/` | `server/tf/halloween/zombie/*`, `server/NextBot/simple_bot.*` |

## Peças de um NextBot

- `INextBot` com quatro componentes, cada um com interface trocável:
  - `ILocomotion` (`GetLocomotionInterface()`): `Approach( pos )`, `FaceTowards( pos )`, `Run()`, `Walk()`, `Jump()`, `IsStuck()`.
  - `IBody` (`GetBodyInterface()`): `AimHeadTowards( alvo, prioridade, duração, ... )`, postura, animação.
  - `IVision` (`GetVisionInterface()`): `GetPrimaryKnownThreat( bool soVisiveis )`, `IsAbleToSee( ent, checkFOV )`, entidades conhecidas (`CKnownEntity`).
  - `IIntention` (`GetIntentionInterface()`): guarda o `Behavior< T >`, que roda a pilha de `Action< T >`.
- Exemplo completo e pequeno de cada interface: `CZombieLocomotion`, `CZombieIntention`, `CZombieBody`.

## Action< T >: a máquina de estados

```cpp
class CTrokBotPerseguir : public Action< CHL2MPBot >
{
public:
	virtual ActionResult< CHL2MPBot > OnStart( CHL2MPBot *me, Action< CHL2MPBot > *priorAction );
	virtual ActionResult< CHL2MPBot > Update( CHL2MPBot *me, float interval );
	virtual void OnEnd( CHL2MPBot *me, Action< CHL2MPBot > *nextAction );
	virtual EventDesiredResult< CHL2MPBot > OnStuck( CHL2MPBot *me );
	virtual const char *GetName( void ) const { return "Perseguir"; }   // aparece no nb_debug
private:
	ChasePath m_path;
};

ActionResult< CHL2MPBot > CTrokBotPerseguir::Update( CHL2MPBot *me, float interval )
{
	const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
	if ( !threat )
		return Done( "Sem alvo" );

	CHL2MPBotPathCost cost( me, FASTEST_ROUTE );
	m_path.Update( me, threat->GetEntity(), cost );
	return Continue();
}
```

- Retornos de `OnStart/Update/OnResume`: `Continue()`, `ChangeTo( new X, "motivo" )` (troca),
  `SuspendFor( new X, "motivo" )` (empilha; volta com `OnResume`), `Done( "motivo" )` (desempilha).
- Eventos (`OnInjured`, `OnKilled`, `OnSight`, `OnLostSight`, `OnSound`, `OnContact`, `OnStuck`,
  `OnMoveToSuccess/Failure`, `OnOtherKilled`, `OnWeaponFired`...) retornam `TryContinue()`,
  `TryChangeTo(...)`, `TrySuspendFor(...)`, `TryDone(...)`. O evento sobe pela pilha até alguém responder.
- `InitialContainedAction()` cria uma ação filha que roda junto (ex.: `MainAction` contém o
  `TacticalMonitor`, que contém `ScenarioMonitor`, que contém o comportamento do modo).
- Perguntas (`IContextualQuery`): `ShouldAttack`, `ShouldRetreat`, `ShouldHurry`,
  `SelectMoreDangerousThreat`, `SelectTargetPoint`, `IsPositionAllowed`. Respondem
  `ANSWER_YES`, `ANSWER_NO` ou `ANSWER_UNDEFINED` (deixa a ação de baixo decidir).
- Ações são criadas com `new` e apagadas pelo `Behavior`; nunca guarde ponteiro para elas.

Árvore do bot do HL2MP (`server/hl2mp/bot/behavior/`): `CHL2MPBotMainAction` (mira, tiro, esquiva)
-> `TacticalMonitor` (vida, munição, recuar) -> `ScenarioMonitor` (objetivo do modo) ->
`SeekAndDestroy` / `Attack` / `MeleeAttack` / `RetreatToCover` / `GetHealth` / `GetAmmo`.
Para um modo novo, o ponto natural é o `ScenarioMonitor`: ele escolhe a ação de acordo com o modo.

## Caminhos

- `PathFollower`: `m_path.Compute( me, objetivo, cost )` (posição ou `CBaseCombatCharacter`) e
  `m_path.Update( me )` todo quadro. Recalcule com `CountdownTimer` (não todo quadro).
- `ChasePath`: persegue um alvo que se move (`Update( me, alvo, cost )`), pode prever a posição.
- `RetreatPath` (`Path/NextBotRetreatPath.h`): foge de uma ameaça.
- Custo: `CHL2MPBotPathCost( me, FASTEST_ROUTE )` ou `SAFEST_ROUTE` (enum `RouteType` em
  `server/nav_pathfind.h`; o custo está em `hl2mp_bot.h`), ou um
  `IPathCost` próprio (ex.: evitar áreas vistas pelos inimigos, preferir cobertura).

## Nav mesh

- Bots e NPCs só andam com `.nav`. Gere no mapa aberto: `sv_cheats 1`, `nav_generate`
  (salva `maps/<mapa>.nav`); ajuste com `nav_edit 1`, `nav_mark_walkable`, `nav_save`.
- Mapas do CS:S já vêm com `.nav` (dos bots do CS). Se o formato não carregar no SDK 2013,
  gere de novo com `nav_generate`. Ainda não testado neste mod.
- Áreas têm atributos (`nav_mark_attribute`) e lugares (`nav_use_place`) que o bot pode ler.

## Depurar

| Comando | Para quê |
|---|---|
| `hl2mp_bot_add [n]`, `hl2mp_bot_kick all`, `hl2mp_bot_quota N` | pôr e tirar bots do HL2MP |
| `hl2mp_bot_difficulty 0..3`, `hl2mp_bot_melee_only 1` | dificuldade e só arma branca |
| `nb_select` (mirando no bot), `nb_debug BEHAVIOR PATH VISION` | mostra a pilha de ações, o caminho e o que ele vê |
| `nb_debug_filter <id ou nome>`, `nb_debug_history 1` | limitar a um bot, guardar o histórico |
| `nb_stop 1`, `nb_blind 1`, `nb_move_to_cursor` | congelar, cegar, mandar andar |
| `nav_edit 1`, `nav_show_danger` | ver a malha e os pesos |

Todos pedem `sv_cheats 1`. Para inspecionar um padrão: veja como `server/tf/bot/behavior/` faz
missões e esquadrões (`tf_bot_squad`), e como `refs/zmr-game` faz IA de zumbi num mod MP.

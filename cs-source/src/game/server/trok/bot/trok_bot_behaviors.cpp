//=============================================================================
// Trok: comportamentos táticos dos bots.
//
// CTrokBotBrain (ação do cenário): ao nascer hesita um instante (não saem
// todos em fila); depois decide entre apoiar um colega que está lutando
// (indo por outro caminho = flanco), segurar um lugar bom ou caçar,
// sorteando pela personalidade.
//
// CTrokBotHunt: caçada com destino e rota variados. O destino sai de pistas
// do time (onde inimigos foram vistos/ouvidos) e de áreas sorteadas, longe
// dos destinos dos colegas e dos lugares que o bot acabou de visitar; a
// rota usa a preferência de corredor do bot e evita a rota dos colegas.
// Ao chegar, para e confere antes de seguir.
//
// CTrokBotHoldSpot: vai até uma área com nota tática alta (mapa tático),
// fica lá um tempo vigiando as entradas da região e volta para o cérebro.
// Mira e tiro continuam na ação raiz (CTrokBotMainAction), então o bot
// atira em quem aparecer enquanto anda ou segura a posição.
//=============================================================================

#include "cbase.h"
#include "trok_bot.h"
#include "trok_bot_tactical.h"
#include "bot/hl2mp_bot.h"
#include "bot/behavior/hl2mp_bot_seek_and_destroy.h"
#include "bot/behavior/hl2mp_bot_attack.h"
#include "trok_bot_team.h"
#include "nav_area.h"
#include "NextBot/Path/NextBotPathFollow.h"
#include "NextBot/Path/NextBotChasePath.h"
#include "trok/trok_surto.h"
#include "trok/trok_surto_mapa.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar trok_bot_tatica( "trok_bot_tatica", "1", FCVAR_GAMEDLL,
	"Bots usam o mapa tático (segurar lugares estratégicos). 0 = só caçar, como o HL2MP." );
static ConVar trok_bot_tatica_raio( "trok_bot_tatica_raio", "2500", FCVAR_GAMEDLL,
	"Distância máxima (u) até o lugar estratégico que o bot vai segurar." );

//-----------------------------------------------------------------------------
// Procurar: o inimigo sumiu. Vai até onde ele foi visto por último e confere
// os esconderijos em volta, em vez de esquecer (como o bot padrão faz).
class CTrokBotSearch : public Action< CHL2MPBot >
{
public:
	CTrokBotSearch( const Vector &pos ) : m_pos( pos ), m_bArrived( false ) {}

	virtual ActionResult< CHL2MPBot > OnStart( CHL2MPBot *me, Action< CHL2MPBot > *priorAction )
	{
		m_path.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );
		m_giveUpTimer.Start( 15.0f );
		return Continue();
	}

	virtual ActionResult< CHL2MPBot > Update( CHL2MPBot *me, float interval )
	{
		if ( me->GetVisionInterface()->GetPrimaryKnownThreat( true ) )
			return Done( "Achou" );
		if ( m_giveUpTimer.IsElapsed() )
			return Done( "Desistiu de procurar" );

		const TrokBotProfile_t &profile = TrokBot_GetProfile( me );

		if ( !m_bArrived )
		{
			if ( ( me->GetAbsOrigin() - m_pos ).Length2D() < 100.0f )
			{
				m_bArrived = true;
				m_searchTimer.Start( RandomFloat( 4.0f, 7.0f ) );
				CollectSpots();
				return Continue();
			}
			if ( !m_path.IsValid() && m_repathTimer.IsElapsed() )
			{
				m_repathTimer.Start( 1.0f );
				CTrokBotPathCost cost( me, (unsigned int)me->entindex() * 2246822519u );
				if ( !m_path.Compute( me, m_pos, cost ) )
					return Done( "Sem caminho até onde ele sumiu" );
			}
			m_path.Update( me );
			if ( profile.flAgressividade < 0.5f && me->IsRangeLessThan( m_pos, 600.0f ) )
				me->PressWalkButton( 0.3f );		// chega quieto
			return Continue();
		}

		if ( m_searchTimer.IsElapsed() )
			return Done( "Ninguém aqui" );

		if ( m_lookTimer.IsElapsed() )
		{
			m_lookTimer.Start( RandomFloat( 0.6f, 1.0f ) );
			Vector lookAt;
			if ( m_spots.Count() )
			{
				lookAt = m_spots[ RandomInt( 0, m_spots.Count() - 1 ) ] + Vector( 0, 0, HumanEyeHeight - 12.0f );
			}
			else
			{
				QAngle angles( RandomFloat( -8.0f, 8.0f ), me->EyeAngles().y + RandomFloat( -160.0f, 160.0f ), 0.0f );
				Vector forward;
				AngleVectors( angles, &forward );
				lookAt = me->EyePosition() + 600.0f * forward;
			}
			me->GetBodyInterface()->AimHeadTowards( lookAt, IBody::INTERESTING, 0.9f, NULL, "Procurando" );
		}
		return Continue();
	}

	virtual const char *GetName( void ) const { return "Procurar"; }

private:
	void CollectSpots( void )
	{
		m_spots.RemoveAll();
		CNavArea *area = TheNavMesh->GetNearestNavArea( m_pos, false, 300.0f );
		if ( !area )
			return;

		struct Collect
		{
			Vector center; CUtlVector< Vector > *out;
			bool operator()( CNavArea *other )
			{
				if ( ( other->GetCenter() - center ).LengthSqr() > 700.0f * 700.0f )
					return true;
				const HidingSpotVector *spots = other->GetHidingSpots();
				FOR_EACH_VEC( *spots, i )
				{
					if ( out->Count() >= 12 )
						return false;
					out->AddToTail( ( *spots )[ i ]->GetPosition() );
				}
				return true;
			}
		} collect = { m_pos, &m_spots };

		collect( area );
		area->ForAllPotentiallyVisibleAreas( collect );
	}

	Vector m_pos;
	bool m_bArrived;
	CUtlVector< Vector > m_spots;
	PathFollower m_path;
	CountdownTimer m_repathTimer;
	CountdownTimer m_giveUpTimer;
	CountdownTimer m_searchTimer;
	CountdownTimer m_lookTimer;
};

//-----------------------------------------------------------------------------
// Combate: substitui o CHL2MPBotAttack do HL2MP (que só corria para cima).
//   - peek: com cobertura a até ~150u (lado ou trás), alterna esconder e sair
//     para atirar; o ritmo de rajada da ação raiz acontece na saída;
//   - recarrega atrás da cobertura, não no aberto;
//   - sem cobertura: luta no lugar com strafe e avança se estiver longe demais;
//   - inimigo sumiu: vai procurar onde ele foi visto (CTrokBotSearch);
//   - segurando posição (âncora): não avança e respeita a emboscada.
static bool FindCoverNear( CHL2MPBot *me, CBaseEntity *threat, const Vector &threatEye, Vector *pCover )
{
	Vector origin = me->GetAbsOrigin();
	Vector toThreat = threatEye - origin;
	toThreat.z = 0.0f;
	toThreat.NormalizeInPlace();
	Vector side( -toThreat.y, toThreat.x, 0.0f );

	const Vector dirs[] =
	{
		side, -side,
		( side - toThreat ).Normalized(), ( -side - toThreat ).Normalized(),
		-toThreat,
	};
	const float radii[] = { 56.0f, 96.0f, 144.0f };

	CTraceFilterSkipTwoEntities filter( me, threat, COLLISION_GROUP_NONE );
	CTraceFilterSimple moveFilter( me, COLLISION_GROUP_PLAYER_MOVEMENT );

	for ( int r = 0; r < ARRAYSIZE( radii ); ++r )
	{
		for ( int d = 0; d < ARRAYSIZE( dirs ); ++d )
		{
			Vector candidate = origin + radii[ r ] * dirs[ d ];
			CNavArea *area = TheNavMesh->GetNavArea( candidate + Vector( 0, 0, 32 ), 120.0f );
			if ( !area )
				continue;
			candidate.z = area->GetZ( candidate );

			trace_t tr;
			UTIL_TraceHull( origin + Vector( 0, 0, StepHeight ), candidate + Vector( 0, 0, StepHeight ),
				Vector( -16, -16, 0 ), Vector( 16, 16, 36 ), MASK_PLAYERSOLID, &moveFilter, &tr );
			if ( tr.DidHit() || me->GetLocomotionInterface()->HasPotentialGap( origin, candidate ) )
				continue;

			// Cobertura de verdade: em pé, ali, não dá para ver o olho do inimigo.
			UTIL_TraceLine( candidate + Vector( 0, 0, 64 ), threatEye, MASK_SHOT, &filter, &tr );
			if ( !tr.DidHit() )
				continue;

			*pCover = candidate;
			return true;
		}
	}
	return false;
}

class CTrokBotFight : public Action< CHL2MPBot >
{
public:
	CTrokBotFight( bool bHold = false ) : m_bHold( bHold ), m_state( OPEN ), m_bHasCover( false ), m_bOpened( false ), m_chasePath( ChasePath::LEAD_SUBJECT ) {}

	virtual ActionResult< CHL2MPBot > OnStart( CHL2MPBot *me, Action< CHL2MPBot > *priorAction )
	{
		m_path.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );
		m_peekPos = me->GetAbsOrigin();
		return Continue();
	}

	virtual ActionResult< CHL2MPBot > Update( CHL2MPBot *me, float interval )
	{
		const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
		if ( !threat || !threat->GetEntity() || threat->IsObsolete() )
			return Done( "Sem inimigo" );

		me->EquipBestWeaponForThreat( threat );
		const TrokBotProfile_t &profile = TrokBot_GetProfile( me );
		CBaseEntity *pThreat = threat->GetEntity();
		Vector threatEye = pThreat->EyePosition();
		float flRange = me->GetRangeTo( threat->GetLastKnownPosition() );

		// Sumiu de vista.
		if ( !threat->IsVisibleRecently() )
		{
			if ( m_bHold )
			{
				if ( threat->GetTimeSinceLastSeen() > 3.0f )
					return Done( "Inimigo sumiu; volta a segurar" );
			}
			else if ( threat->GetTimeSinceLastSeen() > 1.5f )
			{
				return ChangeTo( new CTrokBotSearch( threat->GetLastKnownPosition() ), "Procurar onde ele sumiu" );
			}
		}

		CBaseCombatWeapon *pWeapon = me->GetActiveWeapon();
		CBaseHL2MPCombatWeapon *pHL2MPWeapon = dynamic_cast< CBaseHL2MPCombatWeapon * >( pWeapon );

		// Arma branca / curta distância: vai para cima.
		if ( !m_bHold && ( CHL2MPBot::IsBludgeon( pWeapon ) || ( pHL2MPWeapon && me->IsCloseRange( pHL2MPWeapon ) ) ) )
		{
			Own( me, false );
			CTrokBotPathCost cost( me, (unsigned int)me->entindex() );
			m_chasePath.Update( me, pThreat, cost );
			return Continue();
		}

		// Âncora em emboscada: parado e quieto até a ação raiz liberar o tiro.
		if ( m_bHold && profile.bEmboscada && !m_bOpened )
		{
			Own( me, true );
			if ( me->GetIntentionInterface()->ShouldAttack( me, threat ) != ANSWER_NO )
				m_bOpened = true;
			return Continue();
		}

		// Procura cobertura perto de vez em quando (o lugar muda).
		if ( m_coverTimer.IsElapsed() && m_state == OPEN )
		{
			m_coverTimer.Start( 1.0f );
			m_bHasCover = FindCoverNear( me, pThreat, threatEye, &m_cover );
			if ( m_bHasCover )
				m_peekPos = me->GetAbsOrigin();
		}

		// Recarregar atrás da cobertura.
		bool bLowClip = pWeapon && pWeapon->UsesClipsForAmmo1() && pWeapon->Clip1() < MAX( 1, pWeapon->GetMaxClip1() / 4 ) &&
			me->GetAmmoCount( pWeapon->GetPrimaryAmmoType() ) > 0;
		if ( bLowClip && m_bHasCover && m_state != RELOAD && profile.flUsoTatico >= 0.3f )
		{
			m_state = RELOAD;
			m_stateTimer.Start( 3.5f );
		}

		bool bPeekStyle = m_bHasCover && flRange >= 400.0f && profile.flUsoTatico >= 0.35f && profile.iEstilo != TROK_ESTILO_ENTRY;
		if ( m_state == OPEN && bPeekStyle )
		{
			m_state = PEEK_OUT;
			m_stateTimer.Start( RandomFloat( 0.7f, 1.4f ) );
		}

		switch ( m_state )
		{
		case RELOAD:
			Own( me, true );
			if ( MoveTo( me, m_cover ) )
				me->PressReloadButton( 0.2f );
			if ( ( pWeapon && pWeapon->Clip1() >= pWeapon->GetMaxClip1() ) || m_stateTimer.IsElapsed() )
			{
				m_state = PEEK_OUT;
				m_stateTimer.Start( RandomFloat( 0.7f, 1.4f ) );
			}
			return Continue();

		case PEEK_OUT:
			// Fora da cobertura: o ritmo de rajada da ação raiz cuida do tiro.
			Own( me, true );
			MoveTo( me, m_peekPos );
			if ( m_stateTimer.IsElapsed() )
			{
				m_state = HIDE;
				m_stateTimer.Start( RandomFloat( 0.35f, 0.9f ) * ( 1.5f - profile.flAgressividade ) );
			}
			return Continue();

		case HIDE:
			Own( me, true );
			MoveTo( me, m_cover );
			if ( m_stateTimer.IsElapsed() )
			{
				m_state = PEEK_OUT;
				m_stateTimer.Start( RandomFloat( 0.7f, 1.4f ) );
			}
			return Continue();

		case OPEN:
		default:
			break;
		}

		// Sem cobertura: luta no lugar (strafe e rajadas da ação raiz) e avança se precisar.
		Own( me, false );
		if ( m_bHold )
			return Continue();

		bool bTooFar = flRange > 1.3f * MAX( 400.0f, me->GetDesiredAttackRange() );
		bool bNoShot = !me->IsLineOfFireClear( threat->GetLastKnownPosition() );
		bool bClose = profile.iEstilo == TROK_ESTILO_ENTRY || ( profile.flDisciplina <= 0.0f && flRange > 900.0f );	// entry empurra; fácil chega perto
		if ( bTooFar || bNoShot || bClose )
		{
			if ( m_repathTimer.IsElapsed() )
			{
				m_repathTimer.Start( RandomFloat( 0.4f, 0.7f ) );
				CTrokBotPathCost cost( me, (unsigned int)me->entindex() * 3266489917u );
				m_path.Compute( me, threat->GetLastKnownPosition(), cost );
			}
			m_path.Update( me );
		}
		return Continue();
	}

	virtual ActionResult< CHL2MPBot > OnSuspend( CHL2MPBot *me, Action< CHL2MPBot > *interruptingAction )
	{
		Own( me, false );
		return Continue();
	}

	virtual void OnEnd( CHL2MPBot *me, Action< CHL2MPBot > *nextAction )
	{
		Own( me, false );
	}

	virtual const char *GetName( void ) const
	{
		switch ( m_state )
		{
		case PEEK_OUT: return "Combate(peek)";
		case HIDE: return "Combate(cobertura)";
		case RELOAD: return "Combate(recarregando)";
		default: return m_bHold ? "Combate(segurando)" : "Combate";
		}
	}

private:
	enum State_t { OPEN, PEEK_OUT, HIDE, RELOAD };

	void Own( CHL2MPBot *me, bool bOwned ) { TrokBot_SetCombatMoveOwned( me, bOwned ); }

	// Passos curtos (até ~150u) direto, sem caminho. true = chegou.
	bool MoveTo( CHL2MPBot *me, const Vector &pos )
	{
		if ( ( me->GetAbsOrigin() - pos ).Length2D() < 14.0f )
			return true;
		me->GetLocomotionInterface()->Approach( pos );
		return false;
	}

	bool m_bHold;
	State_t m_state;
	bool m_bHasCover;
	bool m_bOpened;
	Vector m_cover;
	Vector m_peekPos;
	ChasePath m_chasePath;
	PathFollower m_path;
	CountdownTimer m_coverTimer;
	CountdownTimer m_stateTimer;
	CountdownTimer m_repathTimer;
};

//-----------------------------------------------------------------------------
class CTrokBotHoldSpot : public Action< CHL2MPBot >
{
public:
	CTrokBotHoldSpot( float flHoldTime ) : m_flHoldTime( flHoldTime ), m_pArea( NULL ), m_bArrived( false ) {}

	virtual ActionResult< CHL2MPBot > OnStart( CHL2MPBot *me, Action< CHL2MPBot > *priorAction )
	{
		TrokTacticalMap().Update();
		m_pArea = TrokTacticalMap().FindHoldArea( me, trok_bot_tatica_raio.GetFloat() );
		if ( !m_pArea )
			return Done( "Nenhum lugar estratégico perto" );

		TrokBot_ClaimHold( me, m_pArea );
		m_goal = m_pArea->GetCenter();
		m_path.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );
		m_path.Invalidate();
		m_giveUpTimer.Start( 30.0f );	// se não chegar em 30 s, desiste
		return Continue();
	}

	virtual ActionResult< CHL2MPBot > Update( CHL2MPBot *me, float interval )
	{
		if ( !m_bArrived )
		{
			if ( ( me->GetAbsOrigin() - m_goal ).Length2D() < 40.0f )
			{
				m_bArrived = true;
				TrokBot_SetHoldArrived( me, true );
				m_holdTimer.Start( m_flHoldTime * TrokBot_GetProfile( me ).flTempoSegurar );
				m_lookTimer.Invalidate();
			}
			else
			{
				if ( m_giveUpTimer.IsElapsed() )
					return Done( "Não conseguiu chegar" );

				if ( !m_path.IsValid() && m_repathTimer.IsElapsed() )
				{
					m_repathTimer.Start( 1.0f );
					CTrokBotPathCost cost( me, (unsigned int)me->entindex() * 2654435761u );
					if ( !m_path.Compute( me, m_goal, cost ) )
						return Done( "Sem caminho" );
					TrokBot_NotePath( me, m_path );
				}
				m_path.Update( me );
				return Continue();
			}
		}

		if ( m_holdTimer.IsElapsed() )
			return Done( "Tempo de segurar acabou" );

		// Colega lutando perto: os mais agressivos largam o lugar para ajudar.
		if ( m_supportCheckTimer.IsElapsed() )
		{
			m_supportCheckTimer.Start( 1.0f );
			const TrokBotProfile_t &profile = TrokBot_GetProfile( me );
			if ( profile.iEstilo != TROK_ESTILO_ANCORA && profile.flAgressividade > 0.45f )
			{
				CUtlVector< TrokBotIntel_t > intel;
				TrokBot_CollectIntel( me, 3.0f, &intel );
				FOR_EACH_VEC( intel, i )
				{
					if ( intel[ i ].iObserver != me->entindex() && me->IsRangeLessThan( intel[ i ].pos, 1500.0f ) )
						return Done( "Colega precisa de apoio" );
				}
			}
		}

		// Inimigo à vista: luta daqui (peek da posição, sem avançar).
		{
			const CKnownEntity *visible = me->GetVisionInterface()->GetPrimaryKnownThreat( true );
			if ( visible && visible->GetEntity() && me->IsRangeLessThan( visible->GetEntity(), 2500.0f ) )
				return SuspendFor( new CTrokBotFight( true ), "Combate segurando posição" );
		}

		// Vigia as entradas da região (por onde o inimigo vai aparecer),
		// de preferência uma que nenhum colega esteja cuidando.
		const TrokBotProfile_t &profile = TrokBot_GetProfile( me );
		const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat( true );
		const TrokTacticalArea_t *pTact = TrokTacticalMap().GetArea( m_pArea );

		if ( !threat && m_lookTimer.IsElapsed() )
		{
			// Âncora fica mais tempo no mesmo ângulo (espera o inimigo cruzar a mira).
			float flLook = RandomFloat( 1.5f, 3.5f ) * ( profile.bEmboscada ? 2.0f : 1.0f );
			m_lookTimer.Start( flLook );

			const TrokTacticalRegion_t *pRegion = pTact ? TrokTacticalMap().GetRegion( pTact->iRegion ) : NULL;
			if ( pRegion && pRegion->entranceAreas.Count() )
			{
				CNavArea *pChosen = NULL;
				int iCount = pRegion->entranceAreas.Count();
				int iStart = RandomInt( 0, iCount - 1 );
				for ( int k = 0; k < iCount; ++k )
				{
					CNavArea *pEntrance = pRegion->entranceAreas[ ( iStart + k ) % iCount ];
					if ( pEntrance == m_pArea )
						continue;
					if ( !pChosen )
						pChosen = pEntrance;	// se todas estiverem cobertas, fica com a primeira
					if ( !TrokBot_IsWatchedByTeammate( me, pEntrance->GetCenter(), 250.0f ) )
					{
						pChosen = pEntrance;
						break;
					}
				}

				if ( pChosen )
				{
					Vector target = pChosen->GetCenter() + Vector( 0, 0, HumanEyeHeight );
					me->GetBodyInterface()->AimHeadTowards( target, IBody::INTERESTING, flLook, NULL, "Vigiando entrada" );
					TrokBot_ClaimWatch( me, pChosen->GetCenter(), flLook + 0.5f );
				}
			}
		}

		// Com cobertura: âncora fica agachado sempre; os outros às vezes.
		if ( pTact && pTact->flCover > 0.0f && m_crouchTimer.IsElapsed() )
		{
			m_crouchTimer.Start( 1.0f );
			if ( profile.bEmboscada || RandomFloat() < profile.flUsoTatico * 0.3f )
				me->PressCrouchButton( 1.1f );
		}

		return Continue();
	}

	virtual void OnEnd( CHL2MPBot *me, Action< CHL2MPBot > *nextAction )
	{
		TrokBot_ReleaseHold( me );
	}

	virtual EventDesiredResult< CHL2MPBot > OnStuck( CHL2MPBot *me )
	{
		return TryDone( RESULT_CRITICAL, "Preso no caminho" );
	}

	virtual EventDesiredResult< CHL2MPBot > OnMoveToFailure( CHL2MPBot *me, const Path *path, MoveToFailureType reason )
	{
		return TryDone( RESULT_CRITICAL, "Caminho falhou" );
	}

	virtual QueryResultType ShouldHurry( const INextBot *me ) const
	{
		// A caminho do lugar não para para strafe; parado no lugar, luta normal.
		return m_bArrived ? ANSWER_UNDEFINED : ANSWER_YES;
	}

	virtual const char *GetName( void ) const { return "SegurarPosicao"; }

private:
	float m_flHoldTime;
	CNavArea *m_pArea;
	Vector m_goal;
	bool m_bArrived;
	PathFollower m_path;
	CountdownTimer m_repathTimer;
	CountdownTimer m_giveUpTimer;
	CountdownTimer m_holdTimer;
	CountdownTimer m_lookTimer;
	CountdownTimer m_supportCheckTimer;
	CountdownTimer m_crouchTimer;
};

//-----------------------------------------------------------------------------
// Hesitação ao nascer: olha em volta um instante antes de sair.
class CTrokBotHesitate : public Action< CHL2MPBot >
{
public:
	CTrokBotHesitate( float flTime ) : m_flTime( flTime ) {}

	virtual ActionResult< CHL2MPBot > OnStart( CHL2MPBot *me, Action< CHL2MPBot > *priorAction )
	{
		m_timer.Start( m_flTime );
		return Continue();
	}

	virtual ActionResult< CHL2MPBot > Update( CHL2MPBot *me, float interval )
	{
		if ( m_timer.IsElapsed() || me->GetVisionInterface()->GetPrimaryKnownThreat( true ) )
			return Done( "Pronto" );

		if ( m_lookTimer.IsElapsed() )
		{
			m_lookTimer.Start( RandomFloat( 0.3f, 0.7f ) );
			QAngle angles = me->EyeAngles();
			angles.y += RandomFloat( -100.0f, 100.0f );
			angles.x = RandomFloat( -10.0f, 10.0f );
			Vector forward;
			AngleVectors( angles, &forward );
			me->GetBodyInterface()->AimHeadTowards( me->EyePosition() + 500.0f * forward, IBody::BORING, 0.6f, NULL, "Olhando em volta" );
		}
		return Continue();
	}

	virtual const char *GetName( void ) const { return "Hesitar"; }

private:
	float m_flTime;
	CountdownTimer m_timer;
	CountdownTimer m_lookTimer;
};

//-----------------------------------------------------------------------------
// Lugares que cada bot visitou por último (para não repetir o mesmo trajeto).
#define VISITED_COUNT 5
static Vector s_visited[ MAX_PLAYERS + 1 ][ VISITED_COUNT ];
static int s_visitedNext[ MAX_PLAYERS + 1 ];

static void RememberVisited( CHL2MPBot *me, const Vector &pos )
{
	int i = me->entindex();
	if ( i <= 0 || i > MAX_PLAYERS )
		return;
	s_visited[ i ][ s_visitedNext[ i ] ] = pos;
	s_visitedNext[ i ] = ( s_visitedNext[ i ] + 1 ) % VISITED_COUNT;
}

static bool WasVisitedRecently( CHL2MPBot *me, const Vector &pos )
{
	int i = me->entindex();
	if ( i <= 0 || i > MAX_PLAYERS )
		return false;
	for ( int k = 0; k < VISITED_COUNT; ++k )
	{
		if ( s_visited[ i ][ k ] != vec3_origin && ( s_visited[ i ][ k ] - pos ).LengthSqr() < 700.0f * 700.0f )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
class CTrokBotHunt : public Action< CHL2MPBot >
{
public:
	CTrokBotHunt( float flDuration, const Vector *pAssist = NULL )
	{
		m_flDuration = flDuration;
		m_bAssist = pAssist != NULL;
		m_assistPos = pAssist ? *pAssist : vec3_origin;
		m_bFromIntel = false;
		m_bChecking = false;
		m_nSeed = 0;
	}

	virtual ActionResult< CHL2MPBot > OnStart( CHL2MPBot *me, Action< CHL2MPBot > *priorAction )
	{
		m_path.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );
		m_giveUpTimer.Start( m_flDuration );
		m_nSeed = (unsigned int)RandomInt( 1, 0x7FFFFFFF );	// rota "do jeito dele" nesta caçada

		if ( !PickGoal( me ) )
			return Done( "Sem destino" );
		return Continue();
	}

	virtual ActionResult< CHL2MPBot > Update( CHL2MPBot *me, float interval )
	{
		if ( m_giveUpTimer.IsElapsed() )
			return Done( "Tempo de caçada acabou" );

		const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
		if ( threat && ( threat->IsVisibleRecently() || me->IsRangeLessThan( threat->GetLastKnownPosition(), 1000.0f ) ) )
			return SuspendFor( new CTrokBotFight, "Combate" );

		const TrokBotProfile_t &profile = TrokBot_GetProfile( me );

		// Chegou: para e confere o lugar antes de seguir.
		if ( m_bChecking )
		{
			if ( m_checkTimer.IsElapsed() )
			{
				m_bChecking = false;
				if ( m_bAssist )
					return Done( "Chegou para apoiar" );
				if ( !PickGoal( me ) )
					return Done( "Sem destino" );
			}
			else if ( m_lookTimer.IsElapsed() )
			{
				m_lookTimer.Start( RandomFloat( 0.5f, 1.0f ) );
				LookAround( me );
			}
			return Continue();
		}

		if ( ( me->GetAbsOrigin() - m_goal ).Length2D() < 96.0f )
		{
			RememberVisited( me, m_goal );
			m_bChecking = true;
			m_checkTimer.Start( RandomFloat( 0.6f, 1.2f ) * ( 0.8f + 2.0f * ( 1.0f - profile.flAgressividade ) ) );
			m_lookTimer.Invalidate();
			return Continue();
		}

		if ( !m_path.IsValid() && m_repathTimer.IsElapsed() )
		{
			m_repathTimer.Start( 1.0f );
			if ( !ComputePath( me ) && !PickGoal( me ) )
				return Done( "Sem caminho" );
		}

		m_path.Update( me );

		// Indo para onde o time viu alguém: os cautelosos andam (sem barulho) na reta final.
		if ( m_bFromIntel && profile.iEstilo != TROK_ESTILO_ENTRY && profile.flAgressividade < 0.5f && me->IsRangeLessThan( m_goal, 700.0f ) )
			me->PressWalkButton( 0.3f );

		// Pré-mira: olha para onde o time viu inimigos, não só para a frente.
		if ( m_preAimTimer.IsElapsed() && !me->GetVisionInterface()->GetPrimaryKnownThreat( true ) )
		{
			m_preAimTimer.Start( RandomFloat( 1.2f, 2.5f ) );

			CUtlVector< TrokBotIntel_t > intel;
			TrokBot_CollectIntel( me, 15.0f, &intel );
			FOR_EACH_VEC( intel, i )
			{
				if ( me->IsRangeLessThan( intel[ i ].pos, 1500.0f ) && RandomFloat() < 0.5f + 0.5f * profile.flUsoTatico )
				{
					me->GetBodyInterface()->AimHeadTowards( intel[ i ].pos + Vector( 0, 0, HumanEyeHeight ), IBody::INTERESTING, 1.5f, NULL, "Pré-mirando pista do time" );
					return Continue();
				}
			}

			// Andando com colegas: cobre o ângulo que ninguém está olhando.
			Vector lookAt;
			if ( RandomFloat() < 0.4f + 0.6f * profile.flUsoTatico && TrokBot_PickUncoveredLook( me, 500.0f, &lookAt ) )
				me->GetBodyInterface()->AimHeadTowards( lookAt, IBody::INTERESTING, 1.5f, NULL, "Cobrindo ângulo do grupo" );
		}

		return Continue();
	}

	virtual ActionResult< CHL2MPBot > OnResume( CHL2MPBot *me, Action< CHL2MPBot > *interruptingAction )
	{
		m_path.Invalidate();
		m_repathTimer.Invalidate();
		return Continue();
	}

	virtual void OnEnd( CHL2MPBot *me, Action< CHL2MPBot > *nextAction )
	{
		TrokBot_ReleaseGoal( me );
	}

	virtual EventDesiredResult< CHL2MPBot > OnStuck( CHL2MPBot *me )
	{
		PickGoal( me );
		return TryContinue();
	}

	virtual EventDesiredResult< CHL2MPBot > OnMoveToFailure( CHL2MPBot *me, const Path *path, MoveToFailureType reason )
	{
		PickGoal( me );
		return TryContinue();
	}

	virtual const char *GetName( void ) const { return m_bAssist ? "Apoiar" : "Cacar"; }

private:
	bool ComputePath( CHL2MPBot *me )
	{
		CTrokBotPathCost cost( me, m_nSeed );
		if ( !m_path.Compute( me, m_goal, cost ) )
			return false;
		TrokBot_NotePath( me, m_path );
		return true;
	}

	void LookAround( CHL2MPBot *me )
	{
		// Olha para um esconderijo da área ou para um lado qualquer.
		CNavArea *area = me->GetLastKnownArea();
		const HidingSpotVector *spots = area ? area->GetHidingSpots() : NULL;
		if ( spots && spots->Count() && RandomFloat() < 0.6f )
		{
			const Vector &spot = ( *spots )[ RandomInt( 0, spots->Count() - 1 ) ]->GetPosition();
			me->GetBodyInterface()->AimHeadTowards( spot + Vector( 0, 0, HumanEyeHeight - 20.0f ), IBody::INTERESTING, 1.0f, NULL, "Conferindo esconderijo" );
			return;
		}

		QAngle angles = me->EyeAngles();
		angles.y += RandomFloat( -150.0f, 150.0f );
		angles.x = RandomFloat( -8.0f, 8.0f );
		Vector forward;
		AngleVectors( angles, &forward );
		me->GetBodyInterface()->AimHeadTowards( me->EyePosition() + 600.0f * forward, IBody::INTERESTING, 1.0f, NULL, "Conferindo" );
	}

	bool PickGoal( CHL2MPBot *me )
	{
		m_path.Invalidate();

		if ( m_bAssist )
		{
			// Apoio: vai para perto da luta, por outro caminho (a rota dos colegas pesa).
			m_goal = m_assistPos;
			m_bFromIntel = true;
			TrokBot_ClaimGoal( me, m_goal );
			return ComputePath( me );
		}

		const TrokBotProfile_t &profile = TrokBot_GetProfile( me );

		struct Candidate_t { Vector pos; float value; bool intel; };
		CUtlVector< Candidate_t > candidates;

		// 1) Pistas do time: quanto mais recente, melhor.
		CUtlVector< TrokBotIntel_t > intel;
		TrokBot_CollectIntel( me, 25.0f, &intel );
		FOR_EACH_VEC( intel, i )
		{
			Candidate_t c;
			c.pos = intel[ i ].pos;
			c.value = 1.2f * ( 1.0f - ( gpGlobals->curtime - intel[ i ].flTime ) / 25.0f );
			c.intel = true;

			// Lurker não vai direto: mira um ponto ao lado do inimigo, para chegar pelo flanco.
			if ( profile.iEstilo == TROK_ESTILO_LURKER )
			{
				Vector toIntel = c.pos - me->GetAbsOrigin();
				toIntel.z = 0.0f;
				toIntel.NormalizeInPlace();
				Vector side( -toIntel.y, toIntel.x, 0.0f );
				Vector flank = c.pos + side * ( RandomInt( 0, 1 ) ? 1.0f : -1.0f ) * RandomFloat( 450.0f, 750.0f );
				CNavArea *pFlankArea = TheNavMesh->GetNearestNavArea( flank, false, 400.0f );
				if ( pFlankArea )
					c.pos = pFlankArea->GetCenter();
			}
			candidates.AddToTail( c );
		}

		// 2) Áreas sorteadas do mapa.
		TrokTacticalMap().Update();
		for ( int k = 0; k < 30 && TheNavAreas.Count(); ++k )
		{
			CNavArea *area = TheNavAreas[ RandomInt( 0, TheNavAreas.Count() - 1 ) ];
			if ( area->GetSizeX() < 40.0f || area->GetSizeY() < 40.0f || area->IsUnderwater() ||
				 ( area->GetAttributes() & NAV_MESH_AVOID ) )
				continue;

			const TrokTacticalArea_t *pTact = TrokTacticalMap().GetArea( area );
			if ( pTact )
			{
				const TrokTacticalRegion_t *pRegion = TrokTacticalMap().GetRegion( pTact->iRegion );
				if ( pRegion && pRegion->iAccessCost >= 99 )
					continue;	// lugar que não se alcança
			}

			Candidate_t c;
			c.pos = area->GetCenter();
			c.value = 0.0f;
			c.intel = false;
			candidates.AddToTail( c );
		}

		// Agressivo prefere ir longe (atravessar o mapa); cauteloso, perto.
		float flPreferred = 900.0f + 1500.0f * profile.flAgressividade;

		FOR_EACH_VEC( candidates, i )
		{
			Candidate_t &c = candidates[ i ];
			float flDist = ( c.pos - me->GetAbsOrigin() ).Length();
			if ( flDist < 300.0f )
			{
				c.value = -100.0f;
				continue;
			}
			c.value += 0.6f * MAX( 0.0f, 1.0f - fabsf( flDist - flPreferred ) / 2500.0f );
			c.value -= 0.8f * TrokBot_CountClaimsNear( me, c.pos, 500.0f );
			if ( WasVisitedRecently( me, c.pos ) )
				c.value -= 0.7f;
			c.value += RandomFloat( 0.0f, 0.5f );
		}

		candidates.SortPredicate( []( const Candidate_t &a, const Candidate_t &b ) { return a.value > b.value; } );

		// Tenta os melhores até achar um com caminho.
		for ( int i = 0; i < candidates.Count() && i < 4; ++i )
		{
			if ( candidates[ i ].value <= -50.0f )
				break;
			m_goal = candidates[ i ].pos;
			m_bFromIntel = candidates[ i ].intel;
			if ( ComputePath( me ) )
			{
				TrokBot_ClaimGoal( me, m_goal );
				return true;
			}
		}
		return false;
	}

	float m_flDuration;
	bool m_bAssist;
	Vector m_assistPos;
	Vector m_goal;
	bool m_bFromIntel;
	bool m_bChecking;
	unsigned int m_nSeed;
	PathFollower m_path;
	CountdownTimer m_giveUpTimer;
	CountdownTimer m_repathTimer;
	CountdownTimer m_checkTimer;
	CountdownTimer m_lookTimer;
	CountdownTimer m_preAimTimer;
};

//-----------------------------------------------------------------------------
// Suporte: anda alguns metros atrás de um colega, cobrindo o ângulo que ele
// não cobre. Quando o colega entra em luta, o cérebro manda apoiar (trade).
class CTrokBotFollow : public Action< CHL2MPBot >
{
public:
	CTrokBotFollow( CBasePlayer *pLeader, float flDuration ) : m_hLeader( pLeader ), m_flDuration( flDuration ) {}

	virtual ActionResult< CHL2MPBot > OnStart( CHL2MPBot *me, Action< CHL2MPBot > *priorAction )
	{
		m_path.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );
		m_timer.Start( m_flDuration );
		m_flDistance = RandomFloat( 220.0f, 420.0f );
		TrokBot_SetLeader( me, m_hLeader.Get() );
		return Continue();
	}

	virtual ActionResult< CHL2MPBot > Update( CHL2MPBot *me, float interval )
	{
		CBasePlayer *pLeader = m_hLeader.Get();
		if ( !pLeader || !pLeader->IsAlive() || pLeader->GetTeamNumber() != me->GetTeamNumber() )
			return Done( "Líder sumiu" );
		if ( m_timer.IsElapsed() )
			return Done( "Tempo acompanhando acabou" );

		const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
		if ( threat && ( threat->IsVisibleRecently() || me->IsRangeLessThan( threat->GetLastKnownPosition(), 1000.0f ) ) )
			return SuspendFor( new CTrokBotFight, "Combate" );

		// Colega lutando: volta para o cérebro, que manda apoiar.
		CUtlVector< TrokBotIntel_t > intel;
		TrokBot_CollectIntel( me, 2.0f, &intel );
		FOR_EACH_VEC( intel, i )
		{
			if ( intel[ i ].iObserver == pLeader->entindex() )
				return Done( "Líder em combate" );
		}

		if ( me->IsRangeGreaterThan( pLeader, m_flDistance ) )
		{
			if ( m_repathTimer.IsElapsed() )
			{
				m_repathTimer.Start( RandomFloat( 0.8f, 1.2f ) );
				CTrokBotPathCost cost( me, (unsigned int)me->entindex() * 40503u );
				m_path.Compute( me, pLeader->GetAbsOrigin(), cost );
			}
			m_path.Update( me );
		}

		if ( m_lookTimer.IsElapsed() && !me->GetVisionInterface()->GetPrimaryKnownThreat( true ) )
		{
			m_lookTimer.Start( RandomFloat( 1.0f, 2.0f ) );
			Vector lookAt;
			if ( TrokBot_PickUncoveredLook( me, 800.0f, &lookAt ) )
				me->GetBodyInterface()->AimHeadTowards( lookAt, IBody::INTERESTING, 1.5f, NULL, "Cobrindo o líder" );
		}

		return Continue();
	}

	virtual void OnEnd( CHL2MPBot *me, Action< CHL2MPBot > *nextAction )
	{
		TrokBot_SetLeader( me, NULL );
	}

	virtual ActionResult< CHL2MPBot > OnResume( CHL2MPBot *me, Action< CHL2MPBot > *interruptingAction )
	{
		m_repathTimer.Invalidate();
		return Continue();
	}

	virtual const char *GetName( void ) const { return "Acompanhar"; }

private:
	CHandle< CBasePlayer > m_hLeader;
	float m_flDuration;
	float m_flDistance;
	PathFollower m_path;
	CountdownTimer m_timer;
	CountdownTimer m_repathTimer;
	CountdownTimer m_lookTimer;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Modo Surto (design/modo-surto.md)
//-----------------------------------------------------------------------------
static ConVar trok_bot_zumbi_faro( "trok_bot_zumbi_faro", "1", FCVAR_CHEAT,
	"Zumbis sem ninguém à vista vão na direção aproximada do humano mais perto (erro de ~300u, atualizado a cada 4 s)." );

static CBasePlayer *NearestLivingHuman( CHL2MPBot *me )
{
	CBasePlayer *pBest = NULL;
	float flBest = FLT_MAX;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *p = UTIL_PlayerByIndex( i );
		if ( !p || !p->IsAlive() || !TrokSurto_IsHumano( p ) )
			continue;
		const float d = me->GetRangeSquaredTo( p );
		if ( d < flBest )
		{
			flBest = d;
			pBest = p;
		}
	}
	return pBest;
}

// Zumbi: caça o humano mais perto. Cada zumbi tem sua preferência de rota (custo com
// variedade e penalidade de rota do time), então eles chegam por lados diferentes (cerco).
// Grade fechada no caminho: bate até quebrar.
class CTrokBotZumbi : public Action< CHL2MPBot >
{
public:
	CTrokBotZumbi() : m_chase( ChasePath::LEAD_SUBJECT ), m_vecGoal( vec3_invalid ) {}

	virtual ActionResult< CHL2MPBot > OnStart( CHL2MPBot *me, Action< CHL2MPBot > *priorAction )
	{
		m_path.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );
		m_nSeed = (unsigned int)me->entindex() * 2654435761u + (unsigned int)( gpGlobals->curtime * 10.0f );
		return Continue();
	}

	virtual ActionResult< CHL2MPBot > Update( CHL2MPBot *me, float interval )
	{
		if ( !TrokSurto_IsZumbi( me ) )
			return Done( "Não é mais zumbi" );

		CTrokBotPathCost cost( me, m_nSeed );

		// Grade fechada encostada: quebra.
		CBaseEntity *pGrade = TrokSurto_GradeMaisPerto( me->WorldSpaceCenter(), 110.0f, true );
		if ( pGrade )
		{
			me->GetBodyInterface()->AimHeadTowards( pGrade->WorldSpaceCenter(), IBody::IMPORTANT, 0.3f, NULL, "Quebrar a grade" );
			me->PressFireButton( 0.1f );
			return Continue();
		}

		// Humano à vista: persegue e golpeia.
		const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat( true );
		CBaseEntity *pTarget = ( threat && threat->GetEntity() && threat->GetEntity()->IsAlive() ) ? threat->GetEntity() : NULL;
		if ( pTarget && TrokSurto_IsHumano( pTarget ) )
		{
			m_chase.Update( me, pTarget, cost );
			if ( me->IsRangeLessThan( pTarget, 90.0f ) )
			{
				me->GetBodyInterface()->AimHeadTowards( pTarget->WorldSpaceCenter(), IBody::IMPORTANT, 0.2f, NULL, "Golpear" );
				me->PressFireButton( 0.1f );
			}
			// Humano em cima de caixa, perto: pula atrás (zumbi pula mais alto).
			const float flUp = pTarget->GetAbsOrigin().z - me->GetAbsOrigin().z;
			if ( flUp > 30.0f && me->IsRangeLessThan( pTarget, 160.0f ) && me->GetLocomotionInterface()->IsOnGround() )
				me->PressJumpButton( 0.1f );
			return Continue();
		}

		// Ninguém à vista: vai para onde o faro manda (direção aproximada do humano mais perto).
		if ( m_goalTimer.IsElapsed() || m_vecGoal == vec3_invalid )
		{
			m_goalTimer.Start( RandomFloat( 3.0f, 5.0f ) );
			CBasePlayer *pHuman = trok_bot_zumbi_faro.GetBool() ? NearestLivingHuman( me ) : NULL;
			if ( pHuman )
			{
				Vector vecGoal = pHuman->GetAbsOrigin() + Vector( RandomFloat( -300.0f, 300.0f ), RandomFloat( -300.0f, 300.0f ), 0.0f );
				CNavArea *pArea = TheNavMesh->GetNearestNavArea( vecGoal, true, 600.0f );
				m_vecGoal = pArea ? pArea->GetCenter() : pHuman->GetAbsOrigin();
			}
			else
			{
				CNavArea *pArea = TheNavMesh->GetNavAreaCount() ? TheNavMesh->GetNavAreaByID( RandomInt( 1, TheNavMesh->GetNavAreaCount() ) ) : NULL;
				m_vecGoal = pArea ? pArea->GetCenter() : me->GetAbsOrigin();
			}
			m_path.Compute( me, m_vecGoal, cost );
		}
		if ( m_path.IsValid() )
			m_path.Update( me );
		else
			m_goalTimer.Invalidate();
		return Continue();
	}

	virtual EventDesiredResult< CHL2MPBot > OnStuck( CHL2MPBot *me )
	{
		me->PressJumpButton( 0.1f );
		m_goalTimer.Invalidate();
		m_nSeed = m_nSeed * 1664525u + 1013904223u; // tenta outra rota
		return TryContinue();
	}

	virtual EventDesiredResult< CHL2MPBot > OnMoveToFailure( CHL2MPBot *me, const Path *path, MoveToFailureType reason )
	{
		m_goalTimer.Invalidate();
		return TryContinue();
	}

	virtual QueryResultType ShouldRetreat( const INextBot *me ) const { return ANSWER_NO; }
	virtual QueryResultType ShouldHurry( const INextBot *me ) const { return ANSWER_YES; }
	virtual const char *GetName( void ) const { return "SurtoZumbi"; }

private:
	ChasePath m_chase;
	PathFollower m_path;
	Vector m_vecGoal;
	CountdownTimer m_goalTimer;
	unsigned int m_nSeed;
};

// Humano: no preparo e na rodada segura lugares difíceis de alcançar (mapa tático);
// quando os abrigos abrem, vai para o abrigo mais perto e fecha a grade quando há zumbi
// por perto. O recuo atirando ("dar olé") fica no Dodge da ação principal.
class CTrokBotSobreviver : public Action< CHL2MPBot >
{
public:
	CTrokBotSobreviver() : m_hAbrigo( NULL ), m_bNoAbrigo( false ) {}

	virtual ActionResult< CHL2MPBot > OnStart( CHL2MPBot *me, Action< CHL2MPBot > *priorAction )
	{
		m_path.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );
		return Continue();
	}

	virtual ActionResult< CHL2MPBot > Update( CHL2MPBot *me, float interval )
	{
		if ( !TrokSurto_IsHumano( me ) )
			return Done( "Não é mais humano" );

		CTrokSurtoEstado *pEstado = TrokSurto_Estado();
		if ( pEstado && pEstado->IsAbrigos() )
			return UpdateAbrigo( me );

		// Antes dos abrigos: um lugar alto/difícil, por um tempo; depois troca.
		TrokTacticalMap().Update();
		if ( TrokTacticalMap().IsReady() && m_holdTimer.IsElapsed() )
		{
			m_holdTimer.Start( 6.0f );
			return SuspendFor( new CTrokBotHoldSpot( RandomFloat( 25.0f, 45.0f ) ), "Segurar lugar difícil de alcançar" );
		}
		return Continue();
	}

	ActionResult< CHL2MPBot > UpdateAbrigo( CHL2MPBot *me )
	{
		if ( !m_hAbrigo.Get() )
		{
			CUtlVector< CBaseEntity * > abrigos;
			TrokSurto_Abrigos( &abrigos );
			float flBest = FLT_MAX;
			FOR_EACH_VEC( abrigos, i )
			{
				const float d = me->GetRangeSquaredTo( abrigos[ i ] );
				if ( d < flBest )
				{
					flBest = d;
					m_hAbrigo = abrigos[ i ];
				}
			}
			if ( !m_hAbrigo.Get() )
				return Continue(); // mapa sem abrigo marcado: segue segurando lugar
		}

		CBaseEntity *pAbrigo = m_hAbrigo.Get();
		const float flRaio = TrokSurto_AbrigoRaio( pAbrigo );
		m_bNoAbrigo = me->IsRangeLessThan( pAbrigo, flRaio );

		if ( !m_bNoAbrigo )
		{
			if ( m_repathTimer.IsElapsed() )
			{
				m_repathTimer.Start( 1.0f );
				CTrokBotPathCost cost( me, (unsigned int)me->entindex() * 2246822519u );
				m_path.Compute( me, pAbrigo->GetAbsOrigin(), cost );
			}
			m_path.Update( me );
			return Continue();
		}

		// Dentro: fecha a grade aberta mais perto se tem zumbi vindo (ou logo ao chegar).
		CBaseEntity *pGrade = TrokSurto_GradeMaisPerto( me->WorldSpaceCenter(), 220.0f, false );
		if ( pGrade && m_useTimer.IsElapsed() )
		{
			const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat( false );
			const bool bZumbiPerto = threat && threat->GetEntity() && me->IsRangeLessThan( threat->GetLastKnownPosition(), 1400.0f );
			if ( bZumbiPerto || !m_bFechouAoChegar )
			{
				me->GetBodyInterface()->AimHeadTowards( pGrade->WorldSpaceCenter(), IBody::CRITICAL, 0.5f, NULL, "Fechar a grade" );
				if ( me->IsRangeLessThan( pGrade->WorldSpaceCenter(), 110.0f ) )
				{
					me->PressUseButton( 0.1f );
					m_useTimer.Start( 1.0f );
					m_bFechouAoChegar = true;
				}
				else if ( m_repathTimer.IsElapsed() )
				{
					m_repathTimer.Start( 0.5f );
					CTrokBotPathCost cost( me, (unsigned int)me->entindex() );
					m_path.Compute( me, pGrade->WorldSpaceCenter(), cost );
				}
				if ( m_path.IsValid() && !me->IsRangeLessThan( pGrade->WorldSpaceCenter(), 110.0f ) )
					m_path.Update( me );
			}
		}
		return Continue();
	}

	virtual const char *GetName( void ) const { return "SurtoSobreviver"; }

private:
	CHandle< CBaseEntity > m_hAbrigo;
	bool m_bNoAbrigo;
	bool m_bFechouAoChegar = false;
	PathFollower m_path;
	CountdownTimer m_holdTimer;
	CountdownTimer m_repathTimer;
	CountdownTimer m_useTimer;
};

class CTrokBotBrain : public Action< CHL2MPBot >
{
public:
	CTrokBotBrain() : m_bFirst( true ) {}

	virtual ActionResult< CHL2MPBot > Update( CHL2MPBot *me, float interval )
	{
		const TrokBotProfile_t &profile = TrokBot_GetProfile( me );

		// Modo Surto: zumbi caça; humano sobrevive.
		if ( TrokSurto_IsZumbi( me ) )
			return SuspendFor( new CTrokBotZumbi, "Zumbi" );
		if ( TrokSurto_IsHumano( me ) )
			return SuspendFor( new CTrokBotSobreviver, "Sobreviver" );

		// Ao nascer: um instante olhando em volta, cada um com seu tempo.
		if ( m_bFirst )
		{
			m_bFirst = false;
			return SuspendFor( new CTrokBotHesitate( RandomFloat( 0.1f, 1.0f ) * ( 1.5f - profile.flAgressividade ) ), "Hesitar ao nascer" );
		}

		// Colega lutando por perto: vai ajudar e troca o abate, chegando por outra
		// rota. Âncora não larga o lugar; entry e suporte quase sempre vão.
		float flSupportChance = 0.4f + 0.5f * profile.flUsoTatico;
		if ( profile.iEstilo == TROK_ESTILO_ENTRY || profile.iEstilo == TROK_ESTILO_SUPORTE )
			flSupportChance = 0.95f;
		else if ( profile.iEstilo == TROK_ESTILO_ANCORA )
			flSupportChance = 0.0f;

		if ( m_supportTimer.IsElapsed() && RandomFloat() < flSupportChance )
		{
			m_supportTimer.Start( 6.0f );

			CUtlVector< TrokBotIntel_t > intel;
			TrokBot_CollectIntel( me, 5.0f, &intel );
			FOR_EACH_VEC( intel, i )
			{
				if ( intel[ i ].iObserver == me->entindex() || !me->IsRangeLessThan( intel[ i ].pos, 2200.0f ) )
					continue;
				if ( TrokBot_CountClaimsNear( me, intel[ i ].pos, 600.0f ) >= 2 )
					continue;	// já tem gente indo
				return SuspendFor( new CTrokBotHunt( 25.0f, &intel[ i ].pos ), "Apoiar colega" );
			}
		}

		// Suporte: acompanha alguém (de preferência um jogador humano).
		if ( profile.iEstilo == TROK_ESTILO_SUPORTE )
		{
			CBasePlayer *pLeader = TrokBot_PickLeader( me, 2500.0f );
			if ( pLeader )
				return SuspendFor( new CTrokBotFollow( pLeader, RandomFloat( 30.0f, 60.0f ) ), "Acompanhar colega" );
		}

		// Segurar ou caçar, pelo estilo e pela personalidade.
		float flHoldChance = ( 1.0f - profile.flAgressividade ) * ( 0.3f + 0.7f * profile.flUsoTatico );
		if ( profile.iEstilo == TROK_ESTILO_ANCORA )
			flHoldChance = 0.9f;
		else if ( profile.iEstilo == TROK_ESTILO_ENTRY )
			flHoldChance = 0.05f;

		TrokTacticalMap().Update();
		if ( TrokTacticalMap().IsReady() && m_noHoldTimer.IsElapsed() && RandomFloat() < flHoldChance )
		{
			m_noHoldTimer.Start( 8.0f );	// se não achar lugar, não tenta de novo logo
			float flHold = RandomFloat( 20.0f, 45.0f ) * ( 1.3f - profile.flAgressividade );
			return SuspendFor( new CTrokBotHoldSpot( flHold ), "Segurar um lugar estratégico" );
		}

		float flHunt = RandomFloat( 15.0f, 35.0f ) * ( 0.6f + profile.flAgressividade );
		return SuspendFor( new CTrokBotHunt( flHunt ), "Caçar" );
	}

	virtual const char *GetName( void ) const { return "TrokCerebro"; }

private:
	bool m_bFirst;
	CountdownTimer m_noHoldTimer;
	CountdownTimer m_supportTimer;
};

//-----------------------------------------------------------------------------

Action< CHL2MPBot > *TrokBot_ScenarioAction( CHL2MPBot *me )
{
	if ( !trok_bot_tatica.GetBool() )
		return NULL;

	return new CTrokBotBrain;
}

//-----------------------------------------------------------------------------

CON_COMMAND_F( trok_bot_estilos_info, "Lista os bots com nível, estilo, agressividade e se estão segurando posição.", FCVAR_GAMEDLL )
{
	static const char *s_pszLevels[] = { "facil", "normal", "dificil", "expert" };

	Msg( "  bot                      time  nível    estilo        agress  tático  segurando\n" );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CHL2MPBot *pBot = dynamic_cast< CHL2MPBot * >( UTIL_PlayerByIndex( i ) );
		if ( !pBot )
			continue;

		const TrokBotProfile_t &profile = TrokBot_GetProfile( pBot );
		int iLevel = clamp( (int)pBot->GetDifficulty(), 0, 3 );
		Msg( "  %-24s %4d  %-8s %-12s  %.2f    %.2f    %s\n", pBot->GetPlayerName(), pBot->GetTeamNumber(),
			s_pszLevels[ iLevel ], TrokBot_StyleName( profile.iEstilo ), profile.flAgressividade, profile.flUsoTatico,
			TrokBot_IsHolding( pBot ) ? "sim" : "-" );
	}
}

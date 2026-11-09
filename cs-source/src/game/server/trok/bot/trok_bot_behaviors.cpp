//=============================================================================
// Trok: comportamentos táticos dos bots.
//
// CTrokBotBrain (ação do cenário): alterna entre "segurar um lugar bom" e
// "caçar" (o SeekAndDestroy do HL2MP), sorteando pela personalidade:
// bots pouco agressivos e bem táticos seguram mais; agressivos caçam mais.
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
#include "nav_area.h"
#include "NextBot/Path/NextBotPathFollow.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar trok_bot_tatica( "trok_bot_tatica", "1", FCVAR_GAMEDLL,
	"Bots usam o mapa tático (segurar lugares estratégicos). 0 = só caçar, como o HL2MP." );
static ConVar trok_bot_tatica_raio( "trok_bot_tatica_raio", "2500", FCVAR_GAMEDLL,
	"Distância máxima (u) até o lugar estratégico que o bot vai segurar." );

// Áreas já ocupadas por algum bot (para não empilharem no mesmo lugar).
static CUtlVector< CNavArea * > s_claimedAreas;

//-----------------------------------------------------------------------------
class CTrokBotHoldSpot : public Action< CHL2MPBot >
{
public:
	CTrokBotHoldSpot( float flHoldTime ) : m_flHoldTime( flHoldTime ), m_pArea( NULL ), m_bArrived( false ) {}

	virtual ActionResult< CHL2MPBot > OnStart( CHL2MPBot *me, Action< CHL2MPBot > *priorAction )
	{
		TrokTacticalMap().Update();
		m_pArea = TrokTacticalMap().FindHoldArea( me, trok_bot_tatica_raio.GetFloat(), &s_claimedAreas );
		if ( !m_pArea )
			return Done( "Nenhum lugar estratégico perto" );

		s_claimedAreas.AddToTail( m_pArea );
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
				m_holdTimer.Start( m_flHoldTime );
				m_lookTimer.Invalidate();
			}
			else
			{
				if ( m_giveUpTimer.IsElapsed() )
					return Done( "Não conseguiu chegar" );

				if ( !m_path.IsValid() && m_repathTimer.IsElapsed() )
				{
					m_repathTimer.Start( 1.0f );
					CHL2MPBotPathCost cost( me, SAFEST_ROUTE );
					if ( !m_path.Compute( me, m_goal, cost ) )
						return Done( "Sem caminho" );
				}
				m_path.Update( me );
				return Continue();
			}
		}

		if ( m_holdTimer.IsElapsed() )
			return Done( "Tempo de segurar acabou" );

		// Vigia as entradas da região (por onde o inimigo vai aparecer).
		const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat( true );
		if ( !threat && m_lookTimer.IsElapsed() )
		{
			m_lookTimer.Start( RandomFloat( 1.5f, 3.5f ) );

			const TrokTacticalArea_t *pTact = TrokTacticalMap().GetArea( m_pArea );
			const TrokTacticalRegion_t *pRegion = pTact ? TrokTacticalMap().GetRegion( pTact->iRegion ) : NULL;
			if ( pRegion && pRegion->entranceAreas.Count() )
			{
				CNavArea *pEntrance = pRegion->entranceAreas[ RandomInt( 0, pRegion->entranceAreas.Count() - 1 ) ];
				if ( pEntrance != m_pArea )
				{
					me->GetBodyInterface()->AimHeadTowards( pEntrance->GetCenter() + Vector( 0, 0, HumanEyeHeight ),
						IBody::INTERESTING, 2.0f, NULL, "Vigiando entrada" );
				}
			}

			// Com cobertura no lugar, às vezes fica agachado.
			if ( pTact && pTact->flCover > 0.0f && RandomFloat() < TrokBot_GetProfile( me ).flUsoTatico * 0.5f )
				me->PressCrouchButton( RandomFloat( 1.0f, 2.5f ) );
		}

		return Continue();
	}

	virtual void OnEnd( CHL2MPBot *me, Action< CHL2MPBot > *nextAction )
	{
		if ( m_pArea )
			s_claimedAreas.FindAndRemove( m_pArea );
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
};

//-----------------------------------------------------------------------------
class CTrokBotBrain : public Action< CHL2MPBot >
{
public:
	virtual ActionResult< CHL2MPBot > Update( CHL2MPBot *me, float interval )
	{
		const TrokBotProfile_t &profile = TrokBot_GetProfile( me );

		// Chance de segurar: baixa agressividade e bom uso tático.
		float flHoldChance = ( 1.0f - profile.flAgressividade ) * ( 0.3f + 0.7f * profile.flUsoTatico );

		TrokTacticalMap().Update();
		if ( TrokTacticalMap().IsReady() && m_noHoldTimer.IsElapsed() && RandomFloat() < flHoldChance )
		{
			m_noHoldTimer.Start( 8.0f );	// se não achar lugar, não tenta de novo logo
			float flHold = RandomFloat( 20.0f, 45.0f ) * ( 1.3f - profile.flAgressividade );
			return SuspendFor( new CTrokBotHoldSpot( flHold ), "Segurar um lugar estratégico" );
		}

		float flHunt = RandomFloat( 15.0f, 35.0f ) * ( 0.6f + profile.flAgressividade );
		return SuspendFor( new CHL2MPBotSeekAndDestroy( flHunt ), "Caçar" );
	}

	virtual const char *GetName( void ) const { return "TrokCerebro"; }

private:
	CountdownTimer m_noHoldTimer;
};

//-----------------------------------------------------------------------------

Action< CHL2MPBot > *TrokBot_ScenarioAction( CHL2MPBot *me )
{
	if ( !trok_bot_tatica.GetBool() )
		return NULL;

	return new CTrokBotBrain;
}

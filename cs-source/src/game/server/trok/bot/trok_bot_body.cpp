//=============================================================================
// Trok: corpo e locomoção dos bots.
//
// Corpo: a cabeça gira no máximo na velocidade do perfil (ninguém vira 180
// graus num quadro) e o bot só reavalia o movimento do alvo de tempos em
// tempos, o que dá o "atraso" natural de quem acompanha alguém correndo.
//
// Locomoção: para subir em bordas altas (caixas, pilastras) o jogador do
// HL2/CS:S precisa pular e agachar no ar (pulo agachado). O HL2MP só pulava;
// aqui o bot agacha enquanto sobe, como um jogador faz.
//=============================================================================

#include "cbase.h"
#include "trok_bot.h"
#include "bot/hl2mp_bot.h"
#include "bot/hl2mp_bot_body.h"
#include "bot/hl2mp_bot_locomotion.h"
#include "nav_area.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar trok_bot_pulo_agachado( "trok_bot_pulo_agachado", "1", FCVAR_CHEAT,
	"Bots agacham no ar ao subir em bordas altas (pulo agachado)." );

static ConVar trok_bot_faixa( "trok_bot_faixa", "1", FCVAR_CHEAT,
	"Bots andam numa faixa própria do corredor (oscilando devagar), não todos na linha do meio." );

// Altura (u) a partir da qual vale agachar no pulo: o pulo em pé do HL2 sobe ~45u.
#define TROK_PLAIN_JUMP_HEIGHT 40.0f

//-----------------------------------------------------------------------------
class CTrokBotBody : public CHL2MPBotBody
{
public:
	CTrokBotBody( INextBot *bot ) : CHL2MPBotBody( bot ) {}

	virtual float GetHeadAimTrackingInterval( void ) const
	{
		return TrokBot_GetProfile( static_cast< CHL2MPBot * >( GetBot() ) ).flIntervaloRastreio;
	}

	virtual float GetMaxHeadAngularVelocity( void ) const
	{
		return TrokBot_GetProfile( static_cast< CHL2MPBot * >( GetBot() ) ).flGiroMax;
	}
};

//-----------------------------------------------------------------------------
class CTrokBotLocomotion : public CHL2MPBotLocomotion
{
public:
	DECLARE_CLASS( CTrokBotLocomotion, CHL2MPBotLocomotion );

	CTrokBotLocomotion( INextBot *bot ) : CHL2MPBotLocomotion( bot )
	{
		m_flLedgeHeight = 0.0f;
		m_flLane = 0.0f;
		m_flWeaveFreq = RandomFloat( 0.25f, 0.6f );
		m_flWeavePhase = RandomFloat( 0.0f, 2.0f * M_PI );
	}

	// Desvia o ponto para onde o caminho manda andar para a "faixa" do bot,
	// só onde há espaço (área larga, sem pulo, sem beirada, sem parede).
	virtual void Approach( const Vector &pos, float goalWeight = 1.0f )
	{
		Vector goal = pos;

		CHL2MPBot *me = static_cast< CHL2MPBot * >( GetBot() );
		CNavArea *area = me->GetLastKnownArea();

		if ( trok_bot_faixa.GetBool() && area && IsOnGround() && !IsClimbingOrJumping() &&
			 area->GetSizeX() >= 100.0f && area->GetSizeY() >= 100.0f &&
			 !( area->GetAttributes() & ( NAV_MESH_PRECISE | NAV_MESH_JUMP | NAV_MESH_CROUCH | NAV_MESH_STAIRS ) ) )
		{
			Vector to = pos - GetFeet();
			to.z = 0.0f;
			if ( to.NormalizeInPlace() > 48.0f )
			{
				float flOffset = clamp( m_flLane + 14.0f * sinf( gpGlobals->curtime * m_flWeaveFreq * 2.0f * M_PI + m_flWeavePhase ), -36.0f, 36.0f );
				Vector side( -to.y, to.x, 0.0f );
				Vector candidate = pos + flOffset * side;

				trace_t tr;
				CTraceFilterSimple filter( me, COLLISION_GROUP_PLAYER_MOVEMENT );
				UTIL_TraceHull( GetFeet() + Vector( 0, 0, StepHeight ), candidate + Vector( 0, 0, StepHeight ),
					Vector( -16, -16, 0 ), Vector( 16, 16, 36 ), MASK_PLAYERSOLID, &filter, &tr );

				if ( !tr.DidHit() && !HasPotentialGap( GetFeet(), candidate ) )
					goal = candidate;
			}
		}

		BaseClass::Approach( goal, goalWeight );
	}

	virtual bool ClimbUpToLedge( const Vector &landingGoal, const Vector &landingForward, const CBaseEntity *obstacle )
	{
		m_flLedgeHeight = landingGoal.z - GetFeet().z;
		return BaseClass::ClimbUpToLedge( landingGoal, landingForward, obstacle );
	}

	virtual void Update( void )
	{
		BaseClass::Update();

		// Faixa muda de tempos em tempos (como gente que troca de lado no corredor).
		if ( m_laneTimer.IsElapsed() )
		{
			m_laneTimer.Start( RandomFloat( 3.0f, 8.0f ) );
			m_flLane = RandomFloat( -26.0f, 26.0f );
		}

		if ( !trok_bot_pulo_agachado.GetBool() )
			return;

		// Pulo agachado: no ar, subindo para uma borda alta, segura o agachar.
		if ( IsClimbingUpToLedge() && !IsOnGround() && m_flLedgeHeight > TROK_PLAIN_JUMP_HEIGHT )
		{
			CHL2MPBot *me = static_cast< CHL2MPBot * >( GetBot() );
			me->PressCrouchButton( 0.3f );
		}
	}

private:
	float m_flLedgeHeight;
	float m_flLane;
	float m_flWeaveFreq;
	float m_flWeavePhase;
	CountdownTimer m_laneTimer;
};

//-----------------------------------------------------------------------------

CHL2MPBotBody *TrokBot_CreateBody( INextBot *bot )
{
	return new CTrokBotBody( bot );
}

CHL2MPBotLocomotion *TrokBot_CreateLocomotion( INextBot *bot )
{
	return new CTrokBotLocomotion( bot );
}

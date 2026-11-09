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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar trok_bot_pulo_agachado( "trok_bot_pulo_agachado", "1", FCVAR_CHEAT,
	"Bots agacham no ar ao subir em bordas altas (pulo agachado)." );

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
	}

	virtual bool ClimbUpToLedge( const Vector &landingGoal, const Vector &landingForward, const CBaseEntity *obstacle )
	{
		m_flLedgeHeight = landingGoal.z - GetFeet().z;
		return BaseClass::ClimbUpToLedge( landingGoal, landingForward, obstacle );
	}

	virtual void Update( void )
	{
		BaseClass::Update();

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

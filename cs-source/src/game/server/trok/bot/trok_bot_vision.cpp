//=============================================================================
// Trok: percepção dos bots. Ver trok_bot_vision.h.
//=============================================================================

#include "cbase.h"
#include "trok_bot.h"
#include "trok_bot_vision.h"
#include "bot/hl2mp_bot.h"
#include "nav_area.h"
#include "NextBotUtil.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar trok_bot_percepcao_base( "trok_bot_percepcao_base", "6.0", FCVAR_CHEAT,
	"Quanto a barra de percepção enche por segundo no caso ideal (inimigo no centro, perto, exposto, correndo)." );
static ConVar trok_bot_percepcao_queda( "trok_bot_percepcao_queda", "0.35", FCVAR_CHEAT,
	"Quanto a barra de percepção esvazia por segundo com o inimigo fora de vista." );
static ConVar trok_bot_percepcao_memoria( "trok_bot_percepcao_memoria", "1.5", FCVAR_CHEAT,
	"Segundos em que um inimigo que sumiu de vista volta a ser visto na hora (o bot ainda está olhando para ele)." );
static ConVar trok_bot_debug_percepcao( "trok_bot_debug_percepcao", "0", FCVAR_CHEAT,
	"Mostra a barra de percepção do bot escolhido com nb_select sobre cada inimigo." );

// Distância (unidades) até a qual a distância não atrapalha a percepção.
#define PERCEP_CLOSE_RANGE		256.0f
// Escala da queda com a distância: a 1000u ~0.6, a 2000u ~0.3.
#define PERCEP_RANGE_SCALE		900.0f
// Ângulo (graus) do centro da visão em que a percepção é máxima.
#define PERCEP_FOVEA			10.0f
// Contato físico ou muito perto: percebe na hora.
#define PERCEP_INSTANT_RANGE	120.0f
// Um passo ouvido nunca leva a barra além disto.
#define PERCEP_NOISE_CAP		0.9f

CTrokBotVision::CTrokBotVision( INextBot *bot ) : CHL2MPBotVision( bot )
{
	m_iProfileDifficulty = -1;
	m_flReactionTime = 0.3f;
	m_flReactionRollTime = 0.0f;
	m_flAlertUntil = 0.0f;
	memset( m_state, 0, sizeof( m_state ) );
}

void CTrokBotVision::Reset( void )
{
	BaseClass::Reset();
	memset( m_state, 0, sizeof( m_state ) );
	m_flAlertUntil = 0.0f;
}

const TrokBotProfile_t &CTrokBotVision::GetProfile( void ) const
{
	CHL2MPBot *me = static_cast< CHL2MPBot * >( GetBot() );
	int iDifficulty = (int)me->GetDifficulty();

	// A dificuldade é escolhida depois de o bot ser criado (e pode mudar).
	if ( iDifficulty != m_iProfileDifficulty )
	{
		m_iProfileDifficulty = iDifficulty;
		TrokBot_BuildProfile( iDifficulty, &m_profile );
	}

	return m_profile;
}

void CTrokBotVision::SetFieldOfView( float horizAngle )
{
	// O HL2MP passa o FOV da câmera (90) toda hora; o bot "sente" movimento
	// na periferia como gente, até o campo do perfil.
	BaseClass::SetFieldOfView( MAX( horizAngle, GetProfile().flCampoVisao ) );
}

float CTrokBotVision::GetMinRecognizeTime( void ) const
{
	// Sorteia um tempo de reação novo a cada poucos segundos (não a cada consulta).
	// Distribuição ex-gaussiana, a que melhor descreve tempo de reação humano:
	// normal em volta de reacao_min com uma cauda para a direita (às vezes demora
	// bem mais), com média em torno de reacao_max. Nunca abaixo de 0.15 s.
	if ( gpGlobals->curtime >= m_flReactionRollTime )
	{
		const TrokBotProfile_t &profile = GetProfile();
		float flMu = profile.flReacaoMin;
		float flTau = MAX( 0.01f, profile.flReacaoMax - profile.flReacaoMin );
		float flExp = -flTau * logf( MAX( 0.0001f, RandomFloat( 0.0f, 1.0f ) ) );
		m_flReactionTime = MAX( 0.15f, RandomGaussianFloat( flMu, 0.1f * flMu ) + flExp );
		m_flReactionRollTime = gpGlobals->curtime + RandomFloat( 1.5f, 3.0f );
	}

	return m_flReactionTime;
}

CTrokBotVision::Awareness_t *CTrokBotVision::GetState( CBaseEntity *subject ) const
{
	int i = subject ? subject->entindex() : 0;
	if ( i <= 0 || i > MAX_PLAYERS )
		return NULL;

	return &m_state[ i ];
}

float CTrokBotVision::GetAwareness( CBaseEntity *subject ) const
{
	Awareness_t *pState = GetState( subject );
	return pState ? clamp( pState->flValue, 0.0f, 1.0f ) : 1.0f;
}

void CTrokBotVision::NoteGunfire( CBaseEntity *who )
{
	Awareness_t *pState = GetState( who );
	if ( pState )
		pState->flLastGunfire = gpGlobals->curtime;

	m_flAlertUntil = gpGlobals->curtime + 6.0f;
}

void CTrokBotVision::NoteNoise( CBaseEntity *who, float flAmount )
{
	Awareness_t *pState = GetState( who );
	if ( pState && pState->flValue < PERCEP_NOISE_CAP )
		pState->flValue = MIN( PERCEP_NOISE_CAP, pState->flValue + flAmount );

	m_flAlertUntil = gpGlobals->curtime + 4.0f;
}

// Fração do corpo do alvo à vista: cabeça, peito, quadril, dois ombros.
float CTrokBotVision::ComputeExposure( CBaseEntity *subject ) const
{
	CBaseEntity *me = GetBot()->GetEntity();
	const Vector &eye = GetBot()->GetBodyInterface()->GetEyePosition();

	Vector center = subject->WorldSpaceCenter();
	Vector toSubject = center - eye;
	toSubject.z = 0.0f;
	toSubject.NormalizeInPlace();
	Vector side( -toSubject.y, toSubject.x, 0.0f );

	const Vector points[] =
	{
		subject->EyePosition(),
		center + Vector( 0, 0, 8 ),
		subject->GetAbsOrigin() + Vector( 0, 0, 14 ),
		center + Vector( 0, 0, 14 ) + 11.0f * side,
		center + Vector( 0, 0, 14 ) - 11.0f * side,
	};

	CTraceFilterSkipTwoEntities filter( me, subject, COLLISION_GROUP_NONE );
	int iVisible = 0;

	for ( int i = 0; i < ARRAYSIZE( points ); ++i )
	{
		trace_t tr;
		UTIL_TraceLine( eye, points[ i ], MASK_BLOCKLOS_AND_NPCS | CONTENTS_IGNORE_NODRAW_OPAQUE, &filter, &tr );
		if ( !tr.DidHit() )
			++iVisible;
	}

	return (float)iVisible / (float)ARRAYSIZE( points );
}

// Quanto a barra enche por segundo agora (0 = não enche).
float CTrokBotVision::ComputeGain( CBaseEntity *subject, Awareness_t &state ) const
{
	const TrokBotProfile_t &profile = GetProfile();
	const Vector &eye = GetBot()->GetBodyInterface()->GetEyePosition();
	const Vector &forward = GetBot()->GetBodyInterface()->GetViewVector();

	Vector toSubject = subject->WorldSpaceCenter() - eye;
	float flRange = toSubject.NormalizeInPlace();

	// 1) Onde está na visão: centro conta inteiro, periferia bem menos.
	float flHalfFOV = 0.5f * profile.flCampoVisao;
	float flAngle = RAD2DEG( acos( clamp( DotProduct( forward, toSubject ), -1.0f, 1.0f ) ) );
	if ( flAngle > flHalfFOV )
		return 0.0f;

	float flAngleFactor = 1.0f;
	if ( flAngle > PERCEP_FOVEA )
	{
		float t = ( flAngle - PERCEP_FOVEA ) / MAX( 1.0f, flHalfFOV - PERCEP_FOVEA );
		flAngleFactor = 1.0f - 0.85f * t * ( 2.0f - t );	// cai rápido saindo do centro, 0.15 na borda
	}

	// 2) Distância.
	float flRangeFactor = 1.0f;
	if ( flRange > PERCEP_CLOSE_RANGE )
	{
		float x = ( flRange - PERCEP_CLOSE_RANGE ) / PERCEP_RANGE_SCALE;
		flRangeFactor = 1.0f / ( 1.0f + x * sqrtf( x ) );
	}

	// 3) Quanto do corpo aparece (atrás de caixa só a cabeça = difícil de ver).
	if ( gpGlobals->curtime - state.flExposureTime > 0.2f )
	{
		state.flExposure = ComputeExposure( subject );
		state.flExposureTime = gpGlobals->curtime;
	}
	float flExposureFactor = 0.2f + 0.8f * state.flExposure;

	// 4) Movimento e postura: parado e agachado chama pouca atenção.
	float flSpeed = subject->GetAbsVelocity().Length2D();
	float flMoveFactor = ( flSpeed > 200.0f ) ? 1.35f : ( flSpeed > 80.0f ) ? 1.0f : 0.6f;
	if ( subject->GetFlags() & FL_DUCKING )
		flMoveFactor *= 0.75f;
	if ( !( subject->GetFlags() & FL_ONGROUND ) )
		flMoveFactor *= 1.2f;

	// 5) Luz do lugar (precisa de nav_analyze; sem análise o nav diz 1 = claro).
	float flLightFactor = 1.0f;
	CBaseCombatCharacter *pCombat = subject->MyCombatCharacterPointer();
	CNavArea *pArea = pCombat ? pCombat->GetLastKnownArea() : NULL;
	if ( pArea )
		flLightFactor = 0.35f + 0.65f * clamp( pArea->GetLightIntensity( subject->GetAbsOrigin() ), 0.0f, 1.0f );

	// 6) Estado do bot: em alerta (ouviu tiro/passos, está lutando) presta mais atenção.
	float flAlertFactor = IsAlert() ? 1.4f : 1.0f;

	// 7) Variação do momento: às vezes a gente bate o olho, às vezes não.
	if ( gpGlobals->curtime >= state.flNoiseTime )
	{
		state.flNoise = RandomFloat( 0.7f, 1.3f );
		state.flNoiseTime = gpGlobals->curtime + RandomFloat( 0.3f, 0.7f );
	}

	return trok_bot_percepcao_base.GetFloat() * profile.flGanhoPercepcao * flAngleFactor * flRangeFactor *
		flExposureFactor * flMoveFactor * flLightFactor * flAlertFactor * state.flNoise;
}

bool CTrokBotVision::IsVisibleEntityNoticed( CBaseEntity *subject ) const
{
	CHL2MPBot *me = static_cast< CHL2MPBot * >( GetBot() );

	// Aliados e não-jogadores: sem simulação.
	if ( !me->IsEnemy( subject ) )
		return true;

	Awareness_t *pState = GetState( subject );
	if ( !pState )
		return true;

	Awareness_t &state = *pState;
	float flNow = gpGlobals->curtime;
	float flElapsed = flNow - state.flLastUpdate;
	state.flLastUpdate = flNow;

	// Esta função só é chamada com o alvo em linha de visão; um intervalo
	// grande quer dizer que ele estava escondido: a barra esvazia nesse tempo.
	const float flContinuous = 0.3f;
	if ( flElapsed > flContinuous )
	{
		state.flValue -= trok_bot_percepcao_queda.GetFloat() * ( flElapsed - flContinuous );
		state.flValue = MAX( 0.0f, state.flValue );
		flElapsed = flContinuous;
	}

	// Já estava acompanhando: some atrás de algo e volta, continua visto.
	const CKnownEntity *known = GetKnown( subject );
	if ( known && known->WasEverVisible() && known->GetTimeSinceLastSeen() < trok_bot_percepcao_memoria.GetFloat() )
	{
		state.flValue = 1.0f;
		return true;
	}

	// Clarão do tiro ou alguém colado: percebe na hora (se estiver no campo de visão).
	bool bInstant = ( flNow - state.flLastGunfire < 0.5f ) || me->IsRangeLessThan( subject, PERCEP_INSTANT_RANGE );

	float flGain = ComputeGain( subject, state );
	if ( bInstant && flGain > 0.0f )
		state.flValue = 1.0f;
	else
		state.flValue += flGain * flElapsed;

	state.flValue = MIN( state.flValue, 1.5f );

	if ( trok_bot_debug_percepcao.GetBool() && TheNextBots().GetSelected() == GetBot() )
	{
		int r = state.flValue >= 1.0f ? 255 : 255;
		int g = state.flValue >= 1.0f ? 60 : 220;
		NDebugOverlay::EntityTextAtPosition( subject->EyePosition() + Vector( 0, 0, 12 ), 0,
			CFmtStr( "percepção %3.0f%% (ganho %.2f/s)", 100.0f * MIN( state.flValue, 1.0f ), flGain ), 0.1f, r, g, 0, 255 );
	}

	return state.flValue >= 1.0f;
}

//-----------------------------------------------------------------------------
// Fábrica e acesso ao perfil
//-----------------------------------------------------------------------------

CHL2MPBotVision *TrokBot_CreateVision( INextBot *bot )
{
	return new CTrokBotVision( bot );
}

const TrokBotProfile_t &TrokBot_GetProfile( const CHL2MPBot *me )
{
	return static_cast< CTrokBotVision * >( me->GetVisionInterface() )->GetProfile();
}

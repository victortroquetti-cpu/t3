//=============================================================================
// Trok: animação de corpo do jogador. Ver trok_playeranimstate.h.
// Adaptado do zmr_playeranimstate.cpp do Zombie Master: Reborn (Source 1 SDK License).
//=============================================================================

#include "cbase.h"
#include "tier0/vprof.h"
#include "animation.h"
#include "studio.h"
#include "datacache/imdlcache.h"
#include "trok_playeranimstate.h"

#ifndef MOVING_MINIMUM_SPEED
#define MOVING_MINIMUM_SPEED 0.5f
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar trok_anim_nova( "trok_anim_nova", "1", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Animação de corpo nova (andar/correr/sprint, pulo em fases, 9 direções). 0 = a antiga do HL2MP. Vale ao renascer." );
ConVar trok_anim_andar_max( "trok_anim_andar_max", "110", FCVAR_REPLICATED,
	"Abaixo desta velocidade (u/s) toca a animação de andar (se o modelo tiver)." );
ConVar trok_anim_sprint_min( "trok_anim_sprint_min", "300", FCVAR_REPLICATED,
	"A partir desta velocidade (u/s) toca a animação de sprint (se o modelo tiver)." );
ConVar trok_anim_pes_max( "trok_anim_pes_max", "45", FCVAR_REPLICATED,
	"Graus que o tronco gira parado antes de os pés acompanharem (CA: 45)." );
ConVar trok_anim_pes_velocidade( "trok_anim_pes_velocidade", "720", FCVAR_REPLICATED,
	"Graus por segundo com que os pés acompanham a mira." );

bool TrokAnimNova( void )
{
	return trok_anim_nova.GetBool();
}

CTrokPlayerAnimState *CreateTrokPlayerAnimState( CBasePlayer *pPlayer )
{
	MDLCACHE_CRITICAL_SECTION();

	MultiPlayerMovementData_t movementData;
	movementData.m_flBodyYawRate = 720.0f;
	movementData.m_flWalkSpeed = 70.0f;
	movementData.m_flRunSpeed = 256.0f;
	movementData.m_flSprintSpeed = 359.0f;

	return new CTrokPlayerAnimState( pPlayer, movementData );
}

CTrokPlayerAnimState::CTrokPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData )
	: CMultiPlayerAnimState( pPlayer, movementData )
{
	m_bIs9Way = false;
	m_bPuloNovo = false;
	m_flCurrentAimYaw = 0.0f;
}

void CTrokPlayerAnimState::OnNewModel( void )
{
	m_bPoseParameterInit = false;
	m_bIs9Way = false;
	m_bPuloNovo = false;
	ClearAnimationState();
}

// Velocidade local (a de plataformas que andam não conta para as pernas).
void CTrokPlayerAnimState::GetOuterAbsVelocity( Vector &vel )
{
	vel = GetBasePlayer()->GetLocalVelocity();
}

bool CTrokPlayerAnimState::ShouldUpdateAnimState()
{
	CBasePlayer *pPlayer = GetBasePlayer();
	if ( pPlayer->IsEffectActive( EF_NODRAW ) || pPlayer->IsObserver() )
		return false;
#ifdef CLIENT_DLL
	if ( pPlayer->IsDormant() )
		return false;
#endif
	return pPlayer->IsAlive() || m_bDying;
}

//-----------------------------------------------------------------------------
// Atividades. Ordem de tentativa:
//   1. a atividade nova (ACT_MP_*) traduzida pela arma (modelo novo com tipos de arma);
//   2. a atividade nova sem arma (modelo novo genérico);
//   3. a equivalente do HL2MP (ACT_HL2MP_*) traduzida pela arma (modelos atuais);
//   4. a do HL2MP sem arma.
//-----------------------------------------------------------------------------
static Activity TrokParaHL2MP( Activity act )
{
	switch ( act )
	{
	case ACT_MP_STAND_IDLE:					return ACT_HL2MP_IDLE;
	case ACT_MP_WALK:
	case ACT_MP_RUN:
	case ACT_MP_SPRINT:						return ACT_HL2MP_RUN;
	case ACT_MP_CROUCH_IDLE:				return ACT_HL2MP_IDLE_CROUCH;
	case ACT_MP_CROUCHWALK:					return ACT_HL2MP_WALK_CROUCH;
	case ACT_MP_JUMP:
	case ACT_MP_JUMP_START:
	case ACT_MP_JUMP_FLOAT:
	case ACT_MP_SWIM:						return ACT_HL2MP_JUMP;
	case ACT_MP_ATTACK_STAND_PRIMARYFIRE:
	case ACT_MP_ATTACK_CROUCH_PRIMARYFIRE:
	case ACT_MP_ATTACK_STAND_SECONDARYFIRE:
	case ACT_MP_ATTACK_CROUCH_SECONDARYFIRE:
	case ACT_MP_ATTACK_STAND_GRENADE:
	case ACT_MP_ATTACK_CROUCH_GRENADE:		return ACT_HL2MP_GESTURE_RANGE_ATTACK;
	case ACT_MP_RELOAD_STAND:
	case ACT_MP_RELOAD_CROUCH:				return ACT_HL2MP_GESTURE_RELOAD;
	default:								return ACT_INVALID;
	}
}

Activity CTrokPlayerAnimState::PelaArma( Activity act )
{
	CBaseCombatWeapon *pWeapon = GetBasePlayer()->GetActiveWeapon();
	if ( !pWeapon )
		return act;
	bool bRequired = false;
	return pWeapon->ActivityOverride( act, &bRequired );
}

bool CTrokPlayerAnimState::TemSequencia( Activity act )
{
	return act != ACT_INVALID && GetBasePlayer()->SelectWeightedSequence( act ) >= 0;
}

Activity CTrokPlayerAnimState::TranslateActivity( Activity actDesired )
{
	Activity act = PelaArma( actDesired );
	if ( TemSequencia( act ) )
		return act;
	if ( TemSequencia( actDesired ) )
		return actDesired;

	const Activity actHL2MP = TrokParaHL2MP( actDesired );
	if ( actHL2MP == ACT_INVALID )
		return act;
	const Activity actHL2MPArma = PelaArma( actHL2MP );
	if ( TemSequencia( actHL2MPArma ) )
		return actHL2MPArma;
	return actHL2MP;
}

//-----------------------------------------------------------------------------
void CTrokPlayerAnimState::Update( float eyeYaw, float eyePitch )
{
	VPROF( "CTrokPlayerAnimState::Update" );

	CStudioHdr *pStudioHdr = GetBasePlayer()->GetModelPtr();
	if ( !pStudioHdr )
		return;

	if ( !ShouldUpdateAnimState() )
	{
		ClearAnimationState();
		return;
	}

	m_flEyeYaw = AngleNormalize( eyeYaw );
	m_flEyePitch = AngleNormalize( eyePitch );

	ComputeSequences( pStudioHdr );

	if ( SetupPoseParameters( pStudioHdr ) )
	{
		ComputePoseParam_MoveYaw( pStudioHdr );
		ComputePoseParam_AimPitch( pStudioHdr );
		ComputePoseParam_AimYaw( pStudioHdr );
	}
}

void CTrokPlayerAnimState::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	const bool bAgachado = ( GetBasePlayer()->GetFlags() & FL_DUCKING ) != 0;
	switch ( event )
	{
	case PLAYERANIMEVENT_ATTACK_PRIMARY:
		RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, bAgachado ? ACT_MP_ATTACK_CROUCH_PRIMARYFIRE : ACT_MP_ATTACK_STAND_PRIMARYFIRE );
		break;
	case PLAYERANIMEVENT_ATTACK_SECONDARY:
		RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, bAgachado ? ACT_MP_ATTACK_CROUCH_SECONDARYFIRE : ACT_MP_ATTACK_STAND_SECONDARYFIRE );
		break;
	case PLAYERANIMEVENT_ATTACK_GRENADE:
		RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, bAgachado ? ACT_MP_ATTACK_CROUCH_GRENADE : ACT_MP_ATTACK_STAND_GRENADE );
		break;
	case PLAYERANIMEVENT_RELOAD:
		RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, bAgachado ? ACT_MP_RELOAD_CROUCH : ACT_MP_RELOAD_STAND );
		break;
	case PLAYERANIMEVENT_RELOAD_LOOP:
		RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, bAgachado ? ACT_MP_RELOAD_CROUCH_LOOP : ACT_MP_RELOAD_STAND_LOOP );
		break;
	case PLAYERANIMEVENT_RELOAD_END:
		RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, bAgachado ? ACT_MP_RELOAD_CROUCH_END : ACT_MP_RELOAD_STAND_END );
		break;
	default:
		BaseClass::DoAnimationEvent( event, nData );
		break;
	}
}

//-----------------------------------------------------------------------------
// Estados principais
//-----------------------------------------------------------------------------
bool CTrokPlayerAnimState::HandleSwimming( Activity &idealActivity )
{
	// Sem animação de nado: na água funda fica no "no ar".
	if ( GetBasePlayer()->GetWaterLevel() >= WL_Waist )
	{
		idealActivity = ACT_MP_JUMP;
		return true;
	}
	return false;
}

bool CTrokPlayerAnimState::HandleMoving( Activity &idealActivity )
{
	const float flSpeed = GetOuterXYSpeed();
	if ( flSpeed <= MOVING_MINIMUM_SPEED )
		return false;

	// Andar e sprint só se o modelo tiver (senão TranslateActivity cai na corrida do HL2MP,
	// mas a corrida própria do modelo é melhor que um andar "emprestado" de outra arma).
	if ( flSpeed < trok_anim_andar_max.GetFloat() && TemSequencia( TranslateActivity( ACT_MP_WALK ) ) &&
		TranslateActivity( ACT_MP_WALK ) != TranslateActivity( ACT_MP_RUN ) )
		idealActivity = ACT_MP_WALK;
	else if ( flSpeed >= trok_anim_sprint_min.GetFloat() && TranslateActivity( ACT_MP_SPRINT ) != TranslateActivity( ACT_MP_RUN ) )
		idealActivity = ACT_MP_SPRINT;
	else
		idealActivity = ACT_MP_RUN;
	return true;
}

bool CTrokPlayerAnimState::HandleDucking( Activity &idealActivity )
{
	if ( !( GetBasePlayer()->GetFlags() & FL_DUCKING ) )
		return false;
	idealActivity = ( GetOuterXYSpeed() < MOVING_MINIMUM_SPEED ) ? ACT_MP_CROUCH_IDLE : ACT_MP_CROUCHWALK;
	return true;
}

// Pulo: com modelo novo, sair do chão (ACT_MP_JUMP_START) -> no ar (ACT_MP_JUMP_FLOAT) ->
// pousar (gesto ACT_MP_JUMP_LAND). Com os modelos do HL2MP, só o "hop" (ACT_MP_JUMP).
bool CTrokPlayerAnimState::HandleJumping( Activity &idealActivity )
{
	if ( !m_bJumping )
		return false;

	CBasePlayer *pPlayer = GetBasePlayer();
	if ( m_bFirstJumpFrame )
	{
		m_bFirstJumpFrame = false;
		m_bPuloNovo = TranslateActivity( ACT_MP_JUMP_START ) != TranslateActivity( ACT_MP_JUMP );
		RestartMainSequence();
	}

	if ( pPlayer->GetWaterLevel() >= WL_Waist )
	{
		m_bJumping = false;
		RestartMainSequence();
	}
	// O cliente às vezes ainda está no chão no tique em que o pulo chega: espera um pouco.
	else if ( gpGlobals->curtime - m_flJumpStartTime > 0.2f && ( pPlayer->GetFlags() & FL_ONGROUND ) )
	{
		m_bJumping = false;
		RestartMainSequence();
		if ( m_bPuloNovo )
			RestartGesture( GESTURE_SLOT_JUMP, ACT_MP_JUMP_LAND );
	}

	if ( !m_bJumping )
		return false;

	if ( m_bPuloNovo )
		idealActivity = ( gpGlobals->curtime - m_flJumpStartTime > 0.3f ) ? ACT_MP_JUMP_FLOAT : ACT_MP_JUMP_START;
	else
		idealActivity = ACT_MP_JUMP;
	return true;
}

//-----------------------------------------------------------------------------
// Pose parameters: 9 direções (move_x/move_y, body_pitch/body_yaw) ou 8 (move_yaw,
// aim_pitch/aim_yaw, os modelos do HL2MP).
//-----------------------------------------------------------------------------
bool CTrokPlayerAnimState::SetupPoseParameters( CStudioHdr *pStudioHdr )
{
	if ( m_bPoseParameterInit )
		return true;
	if ( !pStudioHdr )
		return false;

	CBasePlayer *pPlayer = GetBasePlayer();
	m_bIs9Way = false;
	m_PoseParameterData.m_iMoveX = pPlayer->LookupPoseParameter( pStudioHdr, "move_x" );
	m_PoseParameterData.m_iMoveY = pPlayer->LookupPoseParameter( pStudioHdr, "move_y" );
	if ( m_PoseParameterData.m_iMoveX >= 0 && m_PoseParameterData.m_iMoveY >= 0 )
	{
		m_bIs9Way = true;
		m_PoseParameterData.m_iAimPitch = pPlayer->LookupPoseParameter( pStudioHdr, "body_pitch" );
		m_PoseParameterData.m_iAimYaw = pPlayer->LookupPoseParameter( pStudioHdr, "body_yaw" );
		if ( m_PoseParameterData.m_iAimPitch < 0 )
			m_PoseParameterData.m_iAimPitch = pPlayer->LookupPoseParameter( pStudioHdr, "aim_pitch" );
		if ( m_PoseParameterData.m_iAimYaw < 0 )
			m_PoseParameterData.m_iAimYaw = pPlayer->LookupPoseParameter( pStudioHdr, "aim_yaw" );
	}
	else
	{
		m_PoseParameterData.m_iMoveX = m_PoseParameterData.m_iMoveY = pPlayer->LookupPoseParameter( pStudioHdr, "move_yaw" );
		if ( m_PoseParameterData.m_iMoveY < 0 )
			return false;
		m_PoseParameterData.m_iAimPitch = pPlayer->LookupPoseParameter( pStudioHdr, "aim_pitch" );
		m_PoseParameterData.m_iAimYaw = pPlayer->LookupPoseParameter( pStudioHdr, "aim_yaw" );
		if ( m_PoseParameterData.m_iAimPitch < 0 || m_PoseParameterData.m_iAimYaw < 0 )
			return false;
	}

	m_bPoseParameterInit = true;
	return true;
}

void CTrokPlayerAnimState::EstimateYaw( void )
{
	const float flDeltaTime = gpGlobals->frametime;
	if ( flDeltaTime == 0.0f )
		return;

	Vector vecEstVelocity;
	GetOuterAbsVelocity( vecEstVelocity );

	if ( vecEstVelocity.x == 0.0f && vecEstVelocity.y == 0.0f )
	{
		// Parado: a direção estimada vai aos poucos para onde os olhos apontam.
		const float flBase = m_bIs9Way ? GetBasePlayer()->GetLocalAngles()[ YAW ] : m_flEyeYaw;
		float flYawDelta = AngleNormalize( flBase - m_PoseParameterData.m_flEstimateYaw );
		flYawDelta *= ( flDeltaTime < 0.25f ) ? flDeltaTime * 4.0f : flDeltaTime;
		m_PoseParameterData.m_flEstimateYaw = AngleNormalize( m_PoseParameterData.m_flEstimateYaw + flYawDelta );
	}
	else
	{
		m_PoseParameterData.m_flEstimateYaw = clamp( RAD2DEG( atan2f( vecEstVelocity.y, vecEstVelocity.x ) ), -180.0f, 180.0f );
	}
}

void CTrokPlayerAnimState::ComputePoseParam_MoveYaw( CStudioHdr *pStudioHdr )
{
	EstimateYaw();

	CBasePlayer *pPlayer = GetBasePlayer();
	// Direção do movimento em relação à mira (os olhos, não o corpo: o corpo atrasa).
	const float flYaw = AngleNormalize( -( AngleNormalize( m_flEyeYaw ) - m_PoseParameterData.m_flEstimateYaw ) );

	if ( m_bIs9Way )
	{
		bool bIsMoving;
		const float flPlaybackRate = CalcMovementPlaybackRate( &bIsMoving );
		Vector2D vecMove( 0.0f, 0.0f );
		if ( bIsMoving )
		{
			vecMove.x = cosf( DEG2RAD( flYaw ) ) * flPlaybackRate;
			vecMove.y = sinf( DEG2RAD( flYaw ) ) * flPlaybackRate;
		}
		pPlayer->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveX, vecMove.x );
		pPlayer->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveY, vecMove.y );
		m_DebugAnimData.m_vecMoveYaw = vecMove;
	}
	else
	{
		pPlayer->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveY, flYaw );
	}
}

void CTrokPlayerAnimState::ComputePoseParam_AimPitch( CStudioHdr *pStudioHdr )
{
	if ( m_PoseParameterData.m_iAimPitch >= 0 )
		GetBasePlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iAimPitch, m_flEyePitch );
	m_DebugAnimData.m_flAimPitch = m_flEyePitch;
}

// Andando, os pés seguem a mira (a mistura de direções faz o resto). Parado, o tronco gira
// sozinho até trok_anim_pes_max graus e então os pés giram para acompanhar.
void CTrokPlayerAnimState::ComputePoseParam_AimYaw( CStudioHdr *pStudioHdr )
{
	Vector vecVelocity;
	GetOuterAbsVelocity( vecVelocity );
	const bool bMoving = vecVelocity.Length() > 1.0f;

	if ( bMoving || m_bForceAimYaw )
	{
		m_flGoalFeetYaw = m_flEyeYaw;
	}
	else if ( m_PoseParameterData.m_flLastAimTurnTime <= 0.0f )
	{
		m_flGoalFeetYaw = m_flEyeYaw;
		m_flCurrentFeetYaw = m_flEyeYaw;
		m_PoseParameterData.m_flLastAimTurnTime = gpGlobals->curtime;
	}
	else if ( fabsf( AngleNormalize( m_flGoalFeetYaw - m_flEyeYaw ) ) > trok_anim_pes_max.GetFloat() )
	{
		m_flGoalFeetYaw = m_flEyeYaw;
	}

	m_flGoalFeetYaw = AngleNormalize( m_flGoalFeetYaw );
	if ( m_flGoalFeetYaw != m_flCurrentFeetYaw )
	{
		if ( m_bForceAimYaw )
			m_flCurrentFeetYaw = m_flGoalFeetYaw;
		else
		{
			ConvergeYawAngles( m_flGoalFeetYaw, trok_anim_pes_velocidade.GetFloat(), gpGlobals->frametime, m_flCurrentFeetYaw );
			m_flLastAimTurnTime = gpGlobals->curtime;
		}
	}

	m_angRender[ YAW ] = m_flCurrentFeetYaw;

	const float flAimYaw = AngleNormalize( m_flEyeYaw - m_flCurrentFeetYaw );
	if ( m_PoseParameterData.m_iAimYaw >= 0 )
	{
		GetBasePlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iAimYaw, flAimYaw );
		m_flCurrentAimYaw = flAimYaw;
	}
	else
		m_flCurrentAimYaw = 0.0f;
	m_DebugAnimData.m_flAimYaw = flAimYaw;
	m_bForceAimYaw = false;

#ifndef CLIENT_DLL
	// No servidor o corpo gira de verdade (caixas de acerto e compensação de lag).
	QAngle angle = GetBasePlayer()->GetAbsAngles();
	angle[ YAW ] = m_flCurrentFeetYaw;
	GetBasePlayer()->SetAbsAngles( angle );
#endif
}

// Modelos de 8 direções não têm move_x: a velocidade de chão sai da sequência com
// move_yaw normalizado (receita do ZMR).
float CTrokPlayerAnimState::GetCurrentMaxGroundSpeed()
{
	if ( m_bIs9Way )
		return BaseClass::GetCurrentMaxGroundSpeed();

	CBasePlayer *pPlayer = GetBasePlayer();
	CStudioHdr *pStudioHdr = pPlayer->GetModelPtr();
	if ( !pStudioHdr || m_PoseParameterData.m_iMoveY < 0 )
		return 1.0f;

	const float flPrev = pPlayer->GetPoseParameter( m_PoseParameterData.m_iMoveY );
	const float d = fabsf( flPrev );
	pPlayer->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveY, d == 0.0f ? 0.0f : flPrev / d );
	const float flSpeed = pPlayer->GetSequenceGroundSpeed( pPlayer->GetSequence() );
	pPlayer->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveY, flPrev );
	return flSpeed;
}

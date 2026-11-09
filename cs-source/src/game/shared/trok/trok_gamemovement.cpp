//=============================================================================
// Trok: movimentação (roda igual no servidor e no cliente: predição).
//
// Substitui o CHL2GameMovement do HL2MP (patch em hl_gamemovement.cpp troca o
// objeto global pelo nosso com #ifdef TROK). Tudo aqui é sem estado guardado
// entre ticks: depende só da posição, velocidade, botões e traços, então o
// cliente prevê exatamente o que o servidor faz, sem variável de rede nova.
//
//   Pulo:     altura ajustável (trok_pulo_altura; HL2 = 21u, CS:S ~45u) e
//             sem o empurrão para frente do HL2 (o "bunny hop" acelerado).
//   Deslizar: correndo rápido, apertar agachar dá um impulso e quase não se
//             perde velocidade enquanto ela for alta; acaba sozinho quando cai.
//             Só reinicia correndo de novo (recarga natural, sem cronômetro).
//   Escalar:  segurando pular e andando contra uma borda (até
//             trok_escalar_altura_max acima dos pés) com espaço em cima, o
//             jogador sobe e passa por cima (mantle). Funciona do chão e no ar.
//
// Todos os números são ConVars replicadas: dá para ajustar no console com o
// jogo aberto (sv_cheats não é preciso; são do servidor).
//=============================================================================

#include "cbase.h"
#include "hl_gamemovement.h"
#include "in_buttons.h"
#include "movevars_shared.h"
#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#include "c_world.h"
#else
#include "hl2mp_player.h"
#include "world.h"
#endif
// Peso da arma / lentidão da mira / faca mais leve (shared/trok/weapon_trok_gun.cpp).
float TrokWeapon_MoveSpeedScale( CBasePlayer *pPlayer );

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool g_bMovementOptimizations;

ConVar trok_pulo_altura( "trok_pulo_altura", "21", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Altura do pulo em pé (u). HL2 = 21, CS:S ~45. O pulo agachado alcança ~36u a mais." );
ConVar trok_pulo_impulso_frente( "trok_pulo_impulso_frente", "0", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"1 = mantém o empurrão para frente do HL2 ao pular (permite ganhar velocidade pulando)." );

ConVar trok_deslizar( "trok_deslizar", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Deslizar ao agachar correndo." );
ConVar trok_deslizar_vel_min( "trok_deslizar_vel_min", "240", FCVAR_REPLICATED, "Velocidade (u/s) mínima para começar a deslizar (só correndo)." );
ConVar trok_deslizar_impulso( "trok_deslizar_impulso", "1.25", FCVAR_REPLICATED, "Multiplica a velocidade ao começar a deslizar." );
ConVar trok_deslizar_vel_max( "trok_deslizar_vel_max", "400", FCVAR_REPLICATED, "Velocidade máxima no começo do deslize." );
ConVar trok_deslizar_atrito( "trok_deslizar_atrito", "0.15", FCVAR_REPLICATED, "Fração do atrito normal durante o deslize." );
ConVar trok_deslizar_vel_fim( "trok_deslizar_vel_fim", "160", FCVAR_REPLICATED, "Abaixo desta velocidade o deslize acaba (atrito volta ao normal)." );

ConVar trok_ar_controle( "trok_ar_controle", "30", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Controle no ar: velocidade (u/s) que o jogador consegue impor no ar a cada direção. Source/CS = 30 "
	"(quase nada sem técnica de strafe); mais alto = muda de direção no meio do pulo, como no Combat Arms." );

// Fluidez de pulo (ideias do Momentum Mod e do Fortress Forever, ver design/estudo-da-momentum-ff.md):
ConVar trok_pulo_virada( "trok_pulo_virada", "0.6", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Logo depois do pulo, apertar uma direção nova gira a velocidade para ela (0 = desliga, 1 = forte). Nunca acelera." );
ConVar trok_pulo_virada_tempo( "trok_pulo_virada_tempo", "0.5", FCVAR_REPLICATED, "Janela (s) depois do pulo em que a virada funciona; enfraquece até o fim." );
ConVar trok_pulo_coyote( "trok_pulo_coyote", "0.12", FCVAR_REPLICATED, "Ainda dá para pular este tempo (s) depois de sair da borda andando." );
ConVar trok_pulo_antecipado( "trok_pulo_antecipado", "0.12", FCVAR_REPLICATED, "Apertar pular até este tempo (s) antes de pousar já pula ao pousar." );
ConVar trok_pulo_teto( "trok_pulo_teto", "1.4", FCVAR_REPLICATED,
	"Ao pular, velocidade acima de N x a máxima perde parte do excesso (contra bunny hop). 0 = sem teto." );
ConVar trok_pulo_teto_duro( "trok_pulo_teto_duro", "1.7", FCVAR_REPLICATED, "Teto absoluto (x velocidade máxima) ao pular." );

ConVar trok_escalar( "trok_escalar", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Escalar bordas segurando pular contra elas." );
ConVar trok_escalar_altura_max( "trok_escalar_altura_max", "80", FCVAR_REPLICATED, "Altura máxima (u, acima dos pés) de uma borda escalável." );
ConVar trok_escalar_vel_frente( "trok_escalar_vel_frente", "140", FCVAR_REPLICATED, "Velocidade para frente ao passar por cima da borda." );

// Distância à frente em que procuramos a parede e a borda.
#define MANTLE_PROBE		24.0f
#define MANTLE_LEDGE_DEPTH	20.0f
#define MANTLE_EXTRA		6.0f	// sobe um pouco acima da borda para passar limpo

class CTrokGameMovement : public CHL2GameMovement
{
	typedef CHL2GameMovement BaseClass;

public:
	virtual void CheckParameters( void );
	virtual float GetAirSpeedCap( void ) { return MAX( 0.0f, trok_ar_controle.GetFloat() ); }
	virtual void FullWalkMove( void );
	virtual bool CheckJumpButton( void );
	virtual void AirMove( void );

	// Escadas: o HL2 só conhece func_useableladder; as escadas dos mapas do CS:S são
	// pincéis com CONTENTS_LADDER, tratadas pelo CGameMovement. Usa as duas.
	virtual bool LadderMove( void );
	virtual bool OnLadder( trace_t &trace );
	virtual void FullLadderMove( void );

private:
	bool OnHL2Ladder( void );
	CHL2MP_Player *TrokPlayer( void ) const { return dynamic_cast< CHL2MP_Player * >( player ); }
	void ApplyJumpTurn( void );
	void CapJumpSpeed( void );
	bool IsDucked( void ) const;
	bool IsSliding( void ) const;
	void TryStartSlide( void );
	bool TryMantle( void );
};

//-----------------------------------------------------------------------------
// Escadas (achado do estudo em design/estudo-da-momentum-ff.md, 1.4-a)
//-----------------------------------------------------------------------------
bool CTrokGameMovement::OnHL2Ladder( void )
{
	trace_t tr; // ignorado pelo CHL2GameMovement::OnLadder (só olha o func_useableladder atual)
	return BaseClass::OnLadder( tr );
}

bool CTrokGameMovement::LadderMove( void )
{
	if ( BaseClass::LadderMove() )
		return true;
	return CGameMovement::LadderMove();
}

// Mesma regra do CGameMovement::OnLadder (que é inline em gamemovement.cpp): pincel de
// escada ou superfície marcada como escalável.
bool CTrokGameMovement::OnLadder( trace_t &trace )
{
	if ( OnHL2Ladder() )
		return true;
	if ( trace.contents & CONTENTS_LADDER )
		return true;
	IPhysicsSurfaceProps *pPhysProps = MoveHelper()->GetSurfaceProps();
	if ( pPhysProps )
	{
		const surfacedata_t *pSurfaceData = pPhysProps->GetSurfaceData( trace.surface.surfaceProps );
		if ( pSurfaceData && pSurfaceData->game.climbable != 0 )
			return true;
	}
	return false;
}

void CTrokGameMovement::FullLadderMove( void )
{
	if ( OnHL2Ladder() )
		BaseClass::FullLadderMove();
	else
		CGameMovement::FullLadderMove();
}

bool CTrokGameMovement::IsDucked( void ) const
{
	return ( player->GetFlags() & FL_DUCKING ) || player->m_Local.m_bDucking;
}

bool CTrokGameMovement::IsSliding( void ) const
{
	return trok_deslizar.GetBool() && player->GetGroundEntity() != NULL && IsDucked() &&
		mv->m_vecVelocity.Length2D() > trok_deslizar_vel_fim.GetFloat();
}

//-----------------------------------------------------------------------------
// Deslizar
//-----------------------------------------------------------------------------
void CTrokGameMovement::TryStartSlide( void )
{
	if ( !trok_deslizar.GetBool() || player->GetGroundEntity() == NULL || IsDucked() )
		return;

	bool bDuckPressed = ( mv->m_nButtons & IN_DUCK ) && !( mv->m_nOldButtons & IN_DUCK );
	if ( !bDuckPressed )
		return;

	float flSpeed = mv->m_vecVelocity.Length2D();
	if ( flSpeed < trok_deslizar_vel_min.GetFloat() )
		return;

	float flNew = MIN( flSpeed * trok_deslizar_impulso.GetFloat(), trok_deslizar_vel_max.GetFloat() );
	if ( flNew <= flSpeed )
		return;

	float flScale = flNew / flSpeed;
	mv->m_vecVelocity.x *= flScale;
	mv->m_vecVelocity.y *= flScale;
}

//-----------------------------------------------------------------------------
// Escalar (mantle)
//-----------------------------------------------------------------------------
bool CTrokGameMovement::TryMantle( void )
{
	if ( !trok_escalar.GetBool() || !( mv->m_nButtons & IN_JUMP ) || mv->m_flForwardMove <= 0.0f )
		return false;

	if ( player->GetMoveType() != MOVETYPE_WALK || player->GetWaterLevel() >= WL_Waist )
		return false;

	QAngle yaw( 0.0f, mv->m_vecViewAngles.y, 0.0f );
	Vector forward;
	AngleVectors( yaw, &forward );

	const Vector &origin = mv->GetAbsOrigin();
	bool bDucked = IsDucked();
	trace_t tr;

	// 1) Tem parede logo à frente?
	TracePlayerBBox( origin, origin + MANTLE_PROBE * forward, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, tr );
	if ( tr.fraction >= 1.0f || tr.startsolid )
		return false;

	// 2) Onde está o topo da borda? Desce um raio de cima, um pouco além da parede.
	float flMaxHeight = trok_escalar_altura_max.GetFloat();
	Vector top = origin + ( MANTLE_PROBE + MANTLE_LEDGE_DEPTH ) * forward;
	Vector start = top + Vector( 0, 0, flMaxHeight + 2.0f );
	float flStep = sv_stepsize.GetFloat();
	Vector end = top + Vector( 0, 0, flStep );
	UTIL_TraceLine( start, end, PlayerSolidMask(), mv->m_nPlayerHandle.Get(), COLLISION_GROUP_PLAYER_MOVEMENT, &tr );
	if ( tr.startsolid || tr.fraction >= 1.0f || tr.plane.normal.z < 0.7f )
		return false;

	float flHeight = tr.endpos.z - origin.z;
	if ( flHeight <= flStep || flHeight > flMaxHeight )
		return false;

	// Do chão, bordas que um pulo normal já alcança ficam com o pulo normal.
	if ( player->GetGroundEntity() != NULL && flHeight <= trok_pulo_altura.GetFloat() )
		return false;

	// 3) Cabe o jogador em cima? (em pé, ou agachado se já estiver agachado)
	Vector landing = tr.endpos + Vector( 0, 0, 1.0f );
	trace_t room;
	TracePlayerBBox( landing, landing, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, room );
	if ( room.startsolid || room.allsolid )
	{
		if ( !bDucked )
			return false;
		Vector duckMins = GetPlayerMins( true ), duckMaxs = GetPlayerMaxs( true );
		Ray_t ray;
		ray.Init( landing, landing, duckMins, duckMaxs );
		UTIL_TraceRay( ray, PlayerSolidMask(), mv->m_nPlayerHandle.Get(), COLLISION_GROUP_PLAYER_MOVEMENT, &room );
		if ( room.startsolid || room.allsolid )
			return false;
	}

	// 4) Sobe o necessário para passar a borda (a cada tick, recalculado:
	//    conforme sobe, a velocidade pedida cai e a subida fica suave).
	float flRise = flHeight + MANTLE_EXTRA;
	float flNeeded = sqrtf( 2.0f * GetCurrentGravity() * flRise );
	if ( mv->m_vecVelocity.z < flNeeded )
		mv->m_vecVelocity.z = flNeeded;

	float flForward = DotProduct( mv->m_vecVelocity, forward );
	if ( flForward < trok_escalar_vel_frente.GetFloat() )
	{
		Vector side = mv->m_vecVelocity - flForward * forward;
		side.z = 0.0f;
		Vector newVel = trok_escalar_vel_frente.GetFloat() * forward + 0.3f * side;
		mv->m_vecVelocity.x = newVel.x;
		mv->m_vecVelocity.y = newVel.y;
	}

	if ( player->GetGroundEntity() != NULL )
		SetGroundEntity( NULL );

	return true;
}

//-----------------------------------------------------------------------------
// Peso da arma na mão e lentidão de mirar pelo cano (script da arma).
//-----------------------------------------------------------------------------
void CTrokGameMovement::CheckParameters( void )
{
	BaseClass::CheckParameters();

	float flScale = TrokWeapon_MoveSpeedScale( player );
	if ( flScale > 0.0f && fabsf( flScale - 1.0f ) > 0.001f )
	{
		mv->m_flMaxSpeed *= flScale;
		mv->m_flForwardMove *= flScale;
		mv->m_flSideMove *= flScale;
		mv->m_flUpMove *= flScale;
	}
}

//-----------------------------------------------------------------------------
void CTrokGameMovement::FullWalkMove( void )
{
	CHL2MP_Player *pTrok = TrokPlayer();
	if ( pTrok )
	{
		if ( player->GetGroundEntity() != NULL )
			pTrok->m_flTrokGroundTime = gpGlobals->curtime;
		else if ( ( mv->m_nButtons & IN_JUMP ) && !( mv->m_nOldButtons & IN_JUMP ) )
			pTrok->m_flTrokJumpPressTime = gpGlobals->curtime;
	}

	TryStartSlide();
	TryMantle();

	// Deslize: depois do movimento normal, devolve a parte da velocidade que o
	// atrito tirou (fica só trok_deslizar_atrito dela). Determinístico: o
	// cliente prevê igual ao servidor.
	bool bSliding = IsSliding();
	float flSpeedBefore = mv->m_vecVelocity.Length2D();

	BaseClass::FullWalkMove();

	if ( bSliding && player->GetGroundEntity() != NULL )
	{
		float flSpeedAfter = mv->m_vecVelocity.Length2D();
		if ( flSpeedAfter > 1.0f && flSpeedAfter < flSpeedBefore )
		{
			float flKept = flSpeedBefore - ( flSpeedBefore - flSpeedAfter ) * trok_deslizar_atrito.GetFloat();
			float flScale = flKept / flSpeedAfter;
			mv->m_vecVelocity.x *= flScale;
			mv->m_vecVelocity.y *= flScale;
		}
	}
}

//-----------------------------------------------------------------------------
// Pulo: altura própria e sem o empurrão para frente do HL2.
//-----------------------------------------------------------------------------
bool CTrokGameMovement::CheckJumpButton( void )
{
	CHL2MP_Player *pTrok = TrokPlayer();
	const float flNow = gpGlobals->curtime;
	const bool bFreshPress = ( mv->m_nButtons & IN_JUMP ) && !( mv->m_nOldButtons & IN_JUMP );

	// Pulo antecipado: apertou no ar pouco antes de pousar e ainda segura.
	if ( pTrok && player->GetGroundEntity() != NULL && ( mv->m_nButtons & IN_JUMP ) && ( mv->m_nOldButtons & IN_JUMP ) &&
		 flNow - pTrok->m_flTrokJumpPressTime <= trok_pulo_antecipado.GetFloat() &&
		 pTrok->m_flTrokJumpPressTime > pTrok->m_flTrokJumpTime )
	{
		mv->m_nOldButtons &= ~IN_JUMP;
	}

	// Pulo atrasado ("coyote"): saiu da borda andando há um instante, sem ter pulado.
	bool bCoyote = false;
	if ( pTrok && player->GetGroundEntity() == NULL && bFreshPress && player->GetWaterLevel() < WL_Waist &&
		 player->GetMoveType() == MOVETYPE_WALK && mv->m_vecVelocity.z <= 0.0f &&
		 flNow - pTrok->m_flTrokGroundTime <= trok_pulo_coyote.GetFloat() &&
		 pTrok->m_flTrokJumpTime < pTrok->m_flTrokGroundTime )
	{
		bCoyote = true;
		mv->m_vecVelocity.z = 0.0f;
#ifdef CLIENT_DLL
		player->SetGroundEntity( GetClientWorldEntity() );
#else
		player->SetGroundEntity( GetWorldEntity() );
#endif
	}

	Vector before = mv->m_vecVelocity;

	if ( !BaseClass::CheckJumpButton() )
	{
		if ( bCoyote )
			player->SetGroundEntity( NULL );
		return false;
	}

	float flGravity = GetCurrentGravity();
	float flBaseMul = g_bMovementOptimizations ? 160.0f : sqrtf( 2.0f * flGravity * GAMEMOVEMENT_JUMP_HEIGHT );
	float flWantMul = sqrtf( 2.0f * flGravity * MAX( 1.0f, trok_pulo_altura.GetFloat() ) );
	surfacedata_t *pSurface = player->GetSurfaceData();
	float flGroundFactor = pSurface ? pSurface->game.jumpFactor : 1.0f;

	float flDelta = flGroundFactor * ( flWantMul - flBaseMul );
	mv->m_vecVelocity.z += flDelta;
	mv->m_outJumpVel.z += flDelta;

	if ( !trok_pulo_impulso_frente.GetBool() )
	{
		mv->m_vecVelocity.x = before.x;
		mv->m_vecVelocity.y = before.y;
	}

	CapJumpSpeed();

	if ( pTrok )
		pTrok->m_flTrokJumpTime = flNow;

	return true;
}

// Teto de velocidade no pulo (Fortress Forever, CFFGameMovement::CheckJumpButton):
// acima de N x a máxima, só parte do excesso sobra; e há um teto duro.
void CTrokGameMovement::CapJumpSpeed( void )
{
	const float flSoft = trok_pulo_teto.GetFloat();
	if ( flSoft <= 0.0f )
		return;
	const float flMax = MAX( 1.0f, player->MaxSpeed() );
	const float flSpeed = mv->m_vecVelocity.Length2D();
	const float flSoftCap = flSoft * flMax;
	if ( flSpeed <= flSoftCap )
		return;
	float flNew = flSoftCap + ( flSpeed - flSoftCap ) * 0.65f;
	flNew = MIN( flNew, MAX( flSoft, trok_pulo_teto_duro.GetFloat() ) * flMax );
	const float flScale = flNew / flSpeed;
	mv->m_vecVelocity.x *= flScale;
	mv->m_vecVelocity.y *= flScale;
}

//-----------------------------------------------------------------------------
// Virada logo depois do pulo ("lurch" do Momentum Mod, PerformLurchChecks): ao
// apertar uma direção nova até trok_pulo_virada_tempo depois do pulo, a velocidade
// gira para ela, mais forte no começo. Nunca aumenta a velocidade. É o "caixa a
// caixa, de um lado pro outro". Vale para bots também (eles apertam as mesmas teclas).
//-----------------------------------------------------------------------------
void CTrokGameMovement::AirMove( void )
{
	ApplyJumpTurn();
	BaseClass::AirMove();
}

void CTrokGameMovement::ApplyJumpTurn( void )
{
	CHL2MP_Player *pTrok = TrokPlayer();
	const float flStrength = trok_pulo_virada.GetFloat();
	const float flWindow = trok_pulo_virada_tempo.GetFloat();
	if ( !pTrok || flStrength <= 0.0f || flWindow <= 0.0f )
		return;

	const float flElapsed = gpGlobals->curtime - pTrok->m_flTrokJumpTime;
	if ( flElapsed < 0.0f || flElapsed > flWindow )
		return;

	const int iDirs = IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT;
	const int iPressed = ( mv->m_nOldButtons ^ mv->m_nButtons ) & mv->m_nButtons;
	if ( !( iPressed & iDirs ) )
		return;

	Vector forward, right;
	AngleVectors( mv->m_vecViewAngles, &forward, &right, NULL );
	forward.z = right.z = 0.0f;
	VectorNormalize( forward );
	VectorNormalize( right );
	Vector wishdir = forward * mv->m_flForwardMove + right * mv->m_flSideMove;
	wishdir.z = 0.0f;
	if ( wishdir.Length2D() < 0.1f )
		return;
	VectorNormalize( wishdir );

	const float flSpeed = mv->m_vecVelocity.Length2D();
	if ( flSpeed < 1.0f )
		return;
	Vector cur( mv->m_vecVelocity.x, mv->m_vecVelocity.y, 0.0f );
	cur /= flSpeed;

	// Mais forte no começo da janela.
	const float flAmt = clamp( ( flWindow - flElapsed ) / ( flWindow * 0.6f ), 0.0f, 1.0f );
	const float flPush = flStrength * player->MaxSpeed() * flAmt;

	Vector turn = ( cur + ( wishdir * 1.5f - cur ) * 0.7f ) - cur;
	turn.z = 0.0f;
	if ( turn.Length2D() < 0.001f )
		return;
	VectorNormalize( turn );

	Vector vNew = cur * flSpeed + turn * flPush;
	vNew.z = 0.0f;
	const float flNew = vNew.Length2D();
	if ( flNew > flSpeed )
		vNew *= flSpeed / flNew;

	mv->m_vecVelocity.x = vNew.x;
	mv->m_vecVelocity.y = vNew.y;
}

//-----------------------------------------------------------------------------
// O objeto global de movimento do jogo (o patch desliga o do HL2 com #ifdef TROK).
//-----------------------------------------------------------------------------
static CTrokGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameMovement, IGameMovement, INTERFACENAME_GAMEMOVEMENT, g_GameMovement );

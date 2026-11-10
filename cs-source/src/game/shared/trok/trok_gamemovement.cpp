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
//   Modo CA (trok_mov_ca 1): velocidade, aceleração, parada, pulo e agachar como no
//             Combat Arms de 2009 (design/decombat/specs/MOVEMENT_2009.md e as specs de
//             movimento do Chimera, que concordam). Ver "Movimento do Combat Arms" abaixo.
//
// Todos os números são ConVars replicadas: dá para ajustar no console com o
// jogo aberto (sv_cheats não é preciso; são do servidor).
//=============================================================================

#include "cbase.h"
#include "hl_gamemovement.h"
#include "in_buttons.h"
#include "movevars_shared.h"
#include "trok_surto.h"
#include "trok_placar_extra.h"	// TrokEquip_Velocidade (colete da loja)
#include "trok_zumbis.h"
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

// --- Movimento do Combat Arms (números do CA em unidades do CA; trok_ca_escala converte, padrão 1) ---
ConVar trok_mov_ca( "trok_mov_ca", "1", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"1 = movimento do Combat Arms 2009 (aceleração em rampa, parada seca, pulo fixo, agachar curto). 0 = Source/HL2 com os ajustes trok_*." );
ConVar trok_ca_escala( "trok_ca_escala", "1.0", FCVAR_REPLICATED,
	"Unidades do CA para o Source. O mundo do CA usa 40u = 1 m, quase o mesmo do Source (~39u = 1 m), então 1 = mesma escala "
	"dos mapas. 0.72 converteria pela altura do corpo (o boneco do CA tem 100u, o do Source 72u)." );
ConVar trok_ca_corrida( "trok_ca_corrida", "285", FCVAR_REPLICATED, "CA: velocidade correndo (u/s do CA), igual para frente, trás e lados." );
ConVar trok_ca_andar( "trok_ca_andar", "70", FCVAR_REPLICATED, "CA: velocidade andando (+walk) e mirando com luneta." );
ConVar trok_ca_agachado( "trok_ca_agachado", "50", FCVAR_REPLICATED, "CA: velocidade agachado." );
ConVar trok_ca_sprint( "trok_ca_sprint", "1.4", FCVAR_REPLICATED, "CA: sprint = corrida x este valor (personagem padrão: 1 + 0.4 do colete)." );
ConVar trok_ca_colete( "trok_ca_colete", "0.1", FCVAR_REPLICATED, "CA: perda de velocidade do colete padrão (somada à da arma)." );
ConVar trok_ca_pulo( "trok_ca_pulo", "330", FCVAR_REPLICATED, "CA: velocidade vertical do pulo (u/s do CA). Com gravidade 1100: ~50u de altura." );
ConVar trok_ca_acel( "trok_ca_acel", "3000", FCVAR_REPLICATED, "CA: aceleração base por eixo (u/s²)." );
ConVar trok_ca_acel_inicio( "trok_ca_acel_inicio", "500", FCVAR_REPLICATED, "CA: aceleração no primeiro instante de um movimento." );
ConVar trok_ca_acel_max( "trok_ca_acel_max", "3000", FCVAR_REPLICATED, "CA: teto da rampa e da aceleração somada." );
ConVar trok_ca_acel_rampa( "trok_ca_acel_rampa", "6000", FCVAR_REPLICATED, "CA: quanto a aceleração sobe por segundo segurando a direção (500 -> 3000 em ~0,4 s)." );
ConVar trok_ca_atrito( "trok_ca_atrito", "5", FCVAR_REPLICATED, "CA: atrito no chão (por segundo; a velocidade cai como exp(-5t))." );
ConVar trok_ca_parada( "trok_ca_parada", "0.1", FCVAR_REPLICATED, "CA: tempo (s) para frear da corrida até parar ao soltar as teclas." );
ConVar trok_ca_ar( "trok_ca_ar", "0.1", FCVAR_REPLICATED, "CA: fator da aceleração no ar depois de um pulo quando a rampa não vale (agachado no ar)." );
ConVar trok_ca_ar_rampa( "trok_ca_ar_rampa", "1", FCVAR_REPLICATED, "CA: 1 = no ar vale a mesma rampa do chão (controle forte, como no CA). 0 = só trok_ca_ar." );
ConVar trok_ca_pulo_min( "trok_ca_pulo_min", "0.3", FCVAR_REPLICATED, "CA: depois de cada pulo, o próximo pulo e a aceleração ficam com esta fração..." );
ConVar trok_ca_pulo_recupera( "trok_ca_pulo_recupera", "1.2", FCVAR_REPLICATED, "...e recuperam este tanto por segundo (de volta a 1 em ~0,6 s, perto de pousar)." );
ConVar trok_ca_agachar_tempo( "trok_ca_agachar_tempo", "0.15", FCVAR_REPLICATED, "CA: tempo (s) para agachar por completo no chão (Source: 0,4)." );
ConVar trok_ca_agachar_ar( "trok_ca_agachar_ar", "0.5", FCVAR_REPLICATED,
	"CA: no ar, a caixa encolhe pelo centro: os pés sobem esta fração da diferença de altura e a cabeça desce o resto (Source: 1 = só os pés)." );

static inline float CAEsc( float f ) { return f * MAX( 0.01f, trok_ca_escala.GetFloat() ); }

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

	// Movimento do Combat Arms
	virtual void WalkMove( void );
	virtual void Duck( void );
	virtual void FinishDuck( void );
	virtual void FinishUnDuck( void );

private:
	bool CAAtivo( void ) const;
	bool CAAgachado( void ) const;
	bool CASprint( void ) const;
	bool CANoPulo( void ) const;
	bool CAEmpurrao( float *pT = NULL ) const;
	float CAHop( void ) const;
	float CAVelocidadeAlvo( void ) const;
	void CAVelocidade( bool bChao );
	void CAMoverNoChao( void );
	Vector m_vecCAAntesAtrito;		// velocidade antes do atrito do Source (o CA usa o atrito dele)
	bool m_bCAAntesAtritoValido;

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
	if ( !trok_mov_ca.GetBool() )
		flScale *= TrokEquip_Velocidade( player );	// colete da loja (no movimento do CA entra em CAVelocidadeAlvo)
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

	// Movimento do CA: o atrito do Source (CGameMovement::Friction, que não é virtual) roda antes
	// do WalkMove; guardamos a velocidade de antes para o WalkMove do CA aplicar o atrito dele.
	m_vecCAAntesAtrito = mv->m_vecVelocity;
	m_bCAAntesAtritoValido = CAAtivo();

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

	if ( CAAtivo() )
	{
		// CA: só pula apertando (segurar não repete), só do chão, nunca agachado nem enquanto o
		// agachar/levantar não terminou. Sem pulo atrasado nem antecipado.
		if ( CAAgachado() || player->m_Local.m_bDucking || player->m_Local.m_flDucktime > 0.0f )
		{
			mv->m_nOldButtons |= IN_JUMP;
			return false;
		}
		const float flHop = CAHop();
		const Vector before = mv->m_vecVelocity;
		if ( !BaseClass::CheckJumpButton() )
			return false;
		float flJump = CAEsc( trok_ca_pulo.GetFloat() ) * flHop;
		if ( TrokSurto_IsZumbi( player ) )
			flJump *= sqrtf( MAX( 0.1f, trok_surto_zumbi_pulo.GetFloat() ) ); // o cvar multiplica a altura
		mv->m_vecVelocity.x = before.x;
		mv->m_vecVelocity.y = before.y;
		mv->m_vecVelocity.z = flJump;	// define, não soma (CA)
		mv->m_outJumpVel.z = flJump;
		if ( pTrok )
			pTrok->m_flTrokJumpTime = flNow;
		return true;
	}
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
	float flHeight = MAX( 1.0f, trok_pulo_altura.GetFloat() );
	if ( TrokSurto_IsZumbi( player ) )
		flHeight *= MAX( 0.1f, trok_surto_zumbi_pulo.GetFloat() );
	float flWantMul = sqrtf( 2.0f * flGravity * flHeight );
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
	if ( CAAtivo() )
	{
		CAVelocidade( false );
		VectorAdd( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		TryPlayerMove();
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}
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

//=============================================================================
// Movimento do Combat Arms (trok_mov_ca 1)
//
// Fontes: design/decombat/specs/MOVEMENT_2009.md (DeCombat, MIT/Apache) e as specs de movimento
// do Chimera (r0pi0x/Chimera, MIT). Código nosso; só os números e regras vêm de lá.
// Unidades do CA vezes trok_ca_escala (1 = mesma escala de mundo: o CA usa 40u por metro, o Source ~39).
//
//   Velocidade-alvo: corrida 285 (frente, trás e lados iguais), andando 70, agachado 50,
//   sprint 1,4x; menos a perda do colete e da arma na mão. Acima dela a velocidade é cortada.
//   Aceleração por eixo em rampa: 500 no primeiro instante, sobe 6000/s até 3000; trocar de
//   sentido (frente<->trás, esquerda<->direita) volta para 500. Agachado: metade, sem rampa.
//   Chão: atrito exato exp(-5t) e, sem tecla, freio que para a corrida em 0,1 s.
//   Ar: sem atrito e sem freio; mesma rampa (controle forte, limitado à velocidade-alvo);
//   agachado no ar depois de pular, quase nenhum controle (x0,1).
//   Pulo: velocidade vertical fixa (330); depois de cada pulo o fator cai a 0,3 e recupera
//   1,2/s, reduzindo o próximo pulo e a aceleração (antibunny hop natural).
//   Agachar: 0,15 s; não pula agachado; no ar a caixa encolhe pelo centro.
//=============================================================================
bool CTrokGameMovement::CAAtivo( void ) const
{
	return trok_mov_ca.GetBool() && player->GetMoveType() == MOVETYPE_WALK && player->GetWaterLevel() < WL_Waist;
}

bool CTrokGameMovement::CAAgachado( void ) const
{
	return ( player->GetFlags() & FL_DUCKING ) != 0 || ( ( mv->m_nButtons & IN_DUCK ) && player->m_Local.m_bDucking );
}

// Sprint: o HL2 já decide (Shift + fôlego) e põe a velocidade máxima no hl2_sprintspeed.
extern ConVar hl2_normspeed;
bool CTrokGameMovement::CASprint( void ) const
{
	return ( mv->m_nButtons & IN_SPEED ) && mv->m_flForwardMove > 0.1f && !( mv->m_nButtons & IN_BACK ) &&
		player->MaxSpeed() > hl2_normspeed.GetFloat() + 1.0f;
}

// "Em um pulo": pulou e ainda não pousou (cair de uma borda não conta).
bool CTrokGameMovement::CANoPulo( void ) const
{
	const CHL2MP_Player *pTrok = TrokPlayer();
	return pTrok && player->GetGroundEntity() == NULL && pTrok->m_flTrokJumpTime > pTrok->m_flTrokGroundTime;
}

// Empurrão do CA no infectado (só no Surto; o servidor liga o tempo em OnPlayerDamaged).
// pT recebe os segundos desde o acerto.
bool CTrokGameMovement::CAEmpurrao( float *pT ) const
{
	const CHL2MP_Player *pTrok = TrokPlayer();
	if ( !pTrok || pTrok->m_flTrokEmpForca <= 0.0f )
		return false;
	const float flNow = gpGlobals->curtime;
	if ( flNow < pTrok->m_flTrokEmpInicio || flNow >= pTrok->m_flTrokEmpFim )
		return false;
	if ( pT )
		*pT = flNow - pTrok->m_flTrokEmpInicio;
	return true;
}

// Fator do pulo: cai a trok_ca_pulo_min a cada pulo e recupera trok_ca_pulo_recupera por segundo.
float CTrokGameMovement::CAHop( void ) const
{
	const CHL2MP_Player *pTrok = TrokPlayer();
	if ( !pTrok || pTrok->m_flTrokJumpTime < 0.0f )
		return 1.0f;
	const float t = gpGlobals->curtime - pTrok->m_flTrokJumpTime;
	if ( t < 0.0f )
		return 1.0f;
	return clamp( trok_ca_pulo_min.GetFloat() + trok_ca_pulo_recupera.GetFloat() * t, 0.0f, 1.0f );
}

float CTrokGameMovement::CAVelocidadeAlvo( void ) const
{
	float s;
	if ( CAAgachado() )
		s = trok_ca_agachado.GetFloat();
	else if ( ( mv->m_nButtons & IN_WALK ) || CAEmpurrao() )
		s = trok_ca_andar.GetFloat();	// levando empurrão: sem correr nem sprint
	else if ( CASprint() )
		s = trok_ca_corrida.GetFloat() * trok_ca_sprint.GetFloat();
	else
		s = trok_ca_corrida.GetFloat();

	// Colete e arma na mão (o script da arma traz a velocidade: M4 0,90; MP5 0,93; R870 0,92;
	// pistola 0,97; faca 1). Também inclui mirar pelo cano e a velocidade do zumbi.
	float flArma = TrokWeapon_MoveSpeedScale( player );
	if ( flArma <= 0.0f )
		flArma = 1.0f;
	s *= MAX( 0.0f, 1.0f - trok_ca_colete.GetFloat() ) * flArma;
	s *= TrokEquip_Velocidade( player );	// colete comprado na loja (trok_placar_extra.cpp)
	s *= TrokZumbis_VelocidadeDe( player );	// tipo de zumbi / humano lento ou com mutagênico (trok_zumbis.cpp)
	return CAEsc( s );
}

void CTrokGameMovement::CAVelocidade( bool bChao )
{
	CHL2MP_Player *pTrok = TrokPlayer();
	const float dt = gpGlobals->frametime;
	if ( dt <= 0.0f )
		return;

	const float flAlvo = CAVelocidadeAlvo();
	const bool bAgachado = CAAgachado();
	const bool bNoPulo = CANoPulo();

	// Direções apertadas agora e no tick anterior (bots também apertam os botões).
	const bool bFrente = mv->m_flForwardMove > 0.1f, bTras = mv->m_flForwardMove < -0.1f;
	const bool bDir = mv->m_flSideMove > 0.1f, bEsq = mv->m_flSideMove < -0.1f;
	const int iOld = mv->m_nOldButtons;
	const bool oFrente = ( iOld & IN_FORWARD ) != 0, oTras = ( iOld & IN_BACK ) != 0;
	const bool oDir = ( iOld & IN_MOVERIGHT ) != 0, oEsq = ( iOld & IN_MOVELEFT ) != 0;
	const bool bMove = bFrente || bTras || bDir || bEsq;
	const bool oMove = oFrente || oTras || oDir || oEsq;

	// 1. Velocidade acima do alvo é cortada (no chão e no ar, só na horizontal).
	{
		const float h = mv->m_vecVelocity.Length2D();
		if ( h > flAlvo && h > 0.001f )
		{
			const float k = flAlvo / h;
			mv->m_vecVelocity.x *= k;
			mv->m_vecVelocity.y *= k;
		}
	}

	// 2. Direções pela mira (só o giro horizontal).
	Vector vecF, vecR;
	QAngle ang( 0.0f, mv->m_vecViewAngles.y, 0.0f );
	AngleVectors( ang, &vecF, &vecR, NULL );
	vecF.z = vecR.z = 0.0f;
	VectorNormalize( vecF );
	VectorNormalize( vecR );

	// 3. Aceleração por eixo, com a rampa do CA.
	const float flInicio = CAEsc( trok_ca_acel_inicio.GetFloat() );
	const float flMax = CAEsc( trok_ca_acel_max.GetFloat() );
	const float flRampa = CAEsc( trok_ca_acel_rampa.GetFloat() );
	float a0 = CAEsc( trok_ca_acel.GetFloat() );
	if ( bAgachado )
		a0 *= 0.5f;
	if ( !bChao && bNoPulo )
		a0 *= trok_ca_ar.GetFloat();

	float fa = pTrok ? MAX( (float)pTrok->m_flTrokRampaFrente, flInicio ) : flInicio;
	float sa = pTrok ? MAX( (float)pTrok->m_flTrokRampaLado, flInicio ) : flInicio;
	float flAF = a0, flAR = a0, flImpulso = 0.0f;
	const bool bRampa = !bAgachado && ( bChao || trok_ca_ar_rampa.GetBool() );
	if ( bRampa )
	{
		bool bReinicio = false;
		if ( bMove && !oMove )
		{
			flImpulso = flInicio;
			fa = sa = flInicio;
			bReinicio = true;
		}
		if ( ( bFrente && oTras ) || ( bTras && oFrente ) )
		{
			flAF = flInicio;
			fa = flInicio;
			bReinicio = true;
		}
		if ( ( bEsq && oDir ) || ( bDir && oEsq ) )
		{
			flAR = flInicio;
			sa = flInicio;
			bReinicio = true;
		}
		if ( !bReinicio )
		{
			if ( bFrente || bTras )
			{
				fa = MIN( fa + dt * flRampa, flMax );
				flAF = fa;
			}
			if ( bEsq || bDir )
			{
				sa = MIN( sa + dt * flRampa, flMax );
				flAR = sa;
			}
		}
		if ( !bFrente && !bTras )
			fa = flInicio;
		if ( !bEsq && !bDir )
			sa = flInicio;
	}
	if ( pTrok )
	{
		pTrok->m_flTrokRampaFrente = fa;
		pTrok->m_flTrokRampaLado = sa;
	}

	Vector vecA( 0.0f, 0.0f, 0.0f );
	if ( bFrente )
		vecA += vecF * flAF;
	if ( bTras )
		vecA -= vecF * flAF;
	if ( bEsq )
	{
		vecA -= vecR * flAR;
		if ( vecA.Length() > flMax )
			vecA *= flMax / vecA.Length();
	}
	if ( bDir )
	{
		vecA += vecR * flAR;
		if ( vecA.Length() > flMax )
			vecA *= flMax / vecA.Length();
	}
	if ( flImpulso > 0.0f && vecA.LengthSqr() > 0.0f )
		vecA *= flImpulso / vecA.Length();	// primeiro instante de qualquer movimento: exatamente o início

	const float flHop = CAHop();
	if ( flHop < 1.0f )
		vecA *= flHop;

	// Empurrão do CA (infectado acertado por inimigo): a aceleração vira a ↑ K²(1 - t) na
	// direção horizontal do dano. Agachado andando: x0,25; agachado parado: para na hora.
	float flEmpT = 0.0f;
	const bool bEmpurrao = CAEmpurrao( &flEmpT );
	if ( bEmpurrao && pTrok )
	{
		const float K = pTrok->m_flTrokEmpForca;
		Vector vecDir( pTrok->m_flTrokEmpDirX, pTrok->m_flTrokEmpDirY, 0.0f );
		vecA = vecDir * CAEsc( K * K * MAX( 0.0f, 1.0f - flEmpT ) );
		if ( bAgachado )
		{
			if ( bMove || ( mv->m_nButtons & IN_JUMP ) )
				vecA *= 0.25f;
			else
			{
				vecA.Init();
				mv->m_vecVelocity.x = mv->m_vecVelocity.y = 0.0f;
			}
		}
	}

	// 4. Integração.
	if ( bChao )
	{
		// Atrito exato do CA: v = a/k + (v - a/k) e^(-k dt).
		const float k = trok_ca_atrito.GetFloat();
		Vector v( mv->m_vecVelocity.x, mv->m_vecVelocity.y, 0.0f );
		if ( k > 0.0f )
		{
			const float E = expf( -k * dt );
			const Vector ak = vecA / k;
			v = ak + ( v - ak ) * E;
		}
		else
			v += vecA * dt;

		// Freio ao soltar as teclas: para a corrida em trok_ca_parada segundos (não freia no empurrão).
		if ( !bMove && !bEmpurrao )
		{
			const float h = v.Length2D();
			if ( h <= CAEsc( 5.0f ) )
				v.Init();
			else
			{
				const float d = ( CAEsc( trok_ca_corrida.GetFloat() ) / MAX( 0.01f, trok_ca_parada.GetFloat() ) ) * dt;
				if ( d >= h )
					v.Init();
				else
					v -= v * ( d / h );
			}
		}
		mv->m_vecVelocity.x = v.x;
		mv->m_vecVelocity.y = v.y;
	}
	else
	{
		// No ar: sem atrito nem freio.
		mv->m_vecVelocity.x += vecA.x * dt;
		mv->m_vecVelocity.y += vecA.y * dt;
	}
}

// WalkMove do CA: a velocidade pela lei do CA e a colisão igual à do CGameMovement::WalkMove.
void CTrokGameMovement::WalkMove( void )
{
	if ( !CAAtivo() )
	{
		BaseClass::WalkMove();
		return;
	}
	CAMoverNoChao();
}

void CTrokGameMovement::CAMoverNoChao( void )
{
	// Desfaz o atrito do Source (aplicamos o do CA).
	if ( m_bCAAntesAtritoValido )
	{
		mv->m_vecVelocity.x = m_vecCAAntesAtrito.x;
		mv->m_vecVelocity.y = m_vecCAAntesAtrito.y;
	}

	CBaseEntity *pOldGround = player->GetGroundEntity();
	CAVelocidade( true );
	mv->m_vecVelocity.z = 0.0f;

	VectorAdd( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
	if ( mv->m_vecVelocity.Length() < 1.0f )
	{
		mv->m_vecVelocity.Init();
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	Vector dest;
	dest.x = mv->GetAbsOrigin().x + mv->m_vecVelocity.x * gpGlobals->frametime;
	dest.y = mv->GetAbsOrigin().y + mv->m_vecVelocity.y * gpGlobals->frametime;
	dest.z = mv->GetAbsOrigin().z;

	trace_t pm;
	TracePlayerBBox( mv->GetAbsOrigin(), dest, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm );
	if ( pm.fraction == 1.0f )
	{
		mv->SetAbsOrigin( pm.endpos );
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		StayOnGround();
		return;
	}
	if ( pOldGround == NULL && player->GetWaterLevel() == 0 )
	{
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}
	// (Pulo para fora da água não chega aqui: o movimento do CA só vale fora da água.)
	StepMove( dest, pm );
	VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
	StayOnGround();
}

// Agachar do CA: completo em trok_ca_agachar_tempo no chão (o Source leva 0,4 s).
void CTrokGameMovement::Duck( void )
{
	BaseClass::Duck();
	if ( !CAAtivo() )
		return;
	if ( ( mv->m_nButtons & IN_DUCK ) && player->m_Local.m_bDucking && !( player->GetFlags() & FL_DUCKING ) &&
		 player->GetGroundEntity() != NULL )
	{
		const float flPassou = ( GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_flDucktime ) / 1000.0f;
		if ( flPassou >= trok_ca_agachar_tempo.GetFloat() )
			FinishDuck();
	}
}

// No ar o Source encolhe a caixa por cima e sobe os pés a diferença toda; no CA a caixa encolhe
// pelo centro: os pés sobem só trok_ca_agachar_ar dela e a cabeça desce o resto.
void CTrokGameMovement::FinishDuck( void )
{
	const bool bAr = player->GetGroundEntity() == NULL;
	const bool bJa = ( player->GetFlags() & FL_DUCKING ) != 0;
	const Vector antes = mv->GetAbsOrigin();
	BaseClass::FinishDuck();
	if ( !CAAtivo() || !bAr || bJa )
		return;
	const Vector subiu = mv->GetAbsOrigin() - antes;
	const float flVolta = subiu.z * ( 1.0f - clamp( trok_ca_agachar_ar.GetFloat(), 0.0f, 1.0f ) );
	if ( flVolta <= 0.0f )
		return;
	trace_t tr;
	TracePlayerBBox( mv->GetAbsOrigin(), mv->GetAbsOrigin() - Vector( 0, 0, flVolta ), PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, tr );
	if ( !tr.startsolid )
		mv->SetAbsOrigin( tr.endpos );
}

void CTrokGameMovement::FinishUnDuck( void )
{
	const bool bAr = player->GetGroundEntity() == NULL;
	const Vector antes = mv->GetAbsOrigin();
	BaseClass::FinishUnDuck();
	if ( !CAAtivo() || !bAr )
		return;
	// O Source desceu os pés a diferença toda; no CA a caixa cresce pelo centro (pés descem a
	// fração e a cabeça sobe o resto). Sobe de volta o que der, sem entrar no teto.
	const Vector desceu = antes - mv->GetAbsOrigin();
	const float flVolta = desceu.z * ( 1.0f - clamp( trok_ca_agachar_ar.GetFloat(), 0.0f, 1.0f ) );
	if ( flVolta <= 0.0f )
		return;
	trace_t tr;
	TracePlayerBBox( mv->GetAbsOrigin(), mv->GetAbsOrigin() + Vector( 0, 0, flVolta ), PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, tr );
	if ( !tr.startsolid )
		mv->SetAbsOrigin( tr.endpos );
}

//-----------------------------------------------------------------------------
// O objeto global de movimento do jogo (o patch desliga o do HL2 com #ifdef TROK).
//-----------------------------------------------------------------------------
static CTrokGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameMovement, IGameMovement, INTERFACENAME_GAMEMOVEMENT, g_GameMovement );

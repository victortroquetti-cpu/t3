//=============================================================================
// Trok: animação de corpo do jogador (3ª pessoa) sobre o CMultiPlayerAnimState
// do SDK, no lugar do CPlayerAnimState simples do HL2MP. Adaptado do
// zmr_playeranimstate do Zombie Master: Reborn (Source 1 SDK License; crédito em
// game/mod_trok/CREDITOS.txt).
//
//   - Andar, correr e sprint separados (cada um cai para a corrida se o modelo não tiver).
//   - Pulo em 3 fases (sair do chão, no ar, pousar) quando o modelo tiver; senão o "hop" do HL2MP.
//   - Mistura de 8 direções (move_yaw, modelos do HL2MP) ou de 9 (move_x/move_y, modelos novos).
//   - Tiro e recarga como gestos por cima das pernas, com evento de rede para os outros clientes.
//   - Funciona com os modelos atuais (ACT_HL2MP_*) e com modelos novos que tenham ACT_MP_*.
// Ligado por trok_anim_nova (0 = volta ao sistema antigo do HL2MP).
//=============================================================================

#ifndef TROK_PLAYERANIMSTATE_H
#define TROK_PLAYERANIMSTATE_H
#ifdef _WIN32
#pragma once
#endif

#include "Multiplayer/multiplayer_animstate.h"

class CTrokPlayerAnimState : public CMultiPlayerAnimState
{
public:
	DECLARE_CLASS( CTrokPlayerAnimState, CMultiPlayerAnimState );

	CTrokPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData );

	virtual void GetOuterAbsVelocity( Vector &vel );
	virtual bool ShouldUpdateAnimState();
	virtual Activity TranslateActivity( Activity actDesired );
	virtual void Update( float eyeYaw, float eyePitch );
	virtual void DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );

	virtual bool HandleMoving( Activity &idealActivity );
	virtual bool HandleJumping( Activity &idealActivity );
	virtual bool HandleDucking( Activity &idealActivity );
	virtual bool HandleSwimming( Activity &idealActivity );

	virtual float GetCurrentMaxGroundSpeed();

	// Troca de modelo: os índices dos pose parameters mudam.
	void OnNewModel( void );
	bool Uses9WayAnim( void ) const { return m_bIs9Way; }

private:
	bool SetupPoseParameters( CStudioHdr *pStudioHdr );
	virtual void EstimateYaw( void );
	virtual void ComputePoseParam_MoveYaw( CStudioHdr *pStudioHdr );
	virtual void ComputePoseParam_AimPitch( CStudioHdr *pStudioHdr );
	virtual void ComputePoseParam_AimYaw( CStudioHdr *pStudioHdr );

	Activity PelaArma( Activity act );
	bool TemSequencia( Activity act );

	bool m_bIs9Way;
	bool m_bPuloNovo;			// o modelo tem pulo em 3 fases (ACT_MP_JUMP_START)
	float m_flCurrentAimYaw;
};

CTrokPlayerAnimState *CreateTrokPlayerAnimState( CBasePlayer *pPlayer );
bool TrokAnimNova( void );

#endif // TROK_PLAYERANIMSTATE_H

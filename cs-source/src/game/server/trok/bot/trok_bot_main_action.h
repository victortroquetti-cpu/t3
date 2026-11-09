//=============================================================================
// Trok: ação raiz dos bots (substitui CHL2MPBotMainAction pelo patch).
//
// Mantém tudo do HL2MP (escolha de alvo, tiro, troca de arma, a árvore
// tática embaixo) e troca:
//   - SelectTargetPoint: mira humana (erro inicial que cai com o tempo,
//     erro extra com alvo rápido, tremor ao levar dano, peito ou cabeça);
//   - Dodge: strafe em "lados" de duração variável, pausas para atirar
//     parado e agachadas, em vez do sorteio a cada quadro;
//   - audição de passos e aviso de tiros para a percepção.
//=============================================================================

#ifndef TROK_BOT_MAIN_ACTION_H
#define TROK_BOT_MAIN_ACTION_H
#ifdef _WIN32
#pragma once
#endif

#include "bot/hl2mp_bot.h"
#include "bot/behavior/hl2mp_bot_behavior.h"

class CTrokBotMainAction : public CHL2MPBotMainAction
{
public:
	DECLARE_CLASS( CTrokBotMainAction, CHL2MPBotMainAction );

	CTrokBotMainAction();

	virtual ActionResult< CHL2MPBot > Update( CHL2MPBot *me, float interval );

	virtual EventDesiredResult< CHL2MPBot > OnInjured( CHL2MPBot *me, const CTakeDamageInfo &info );
	virtual EventDesiredResult< CHL2MPBot > OnWeaponFired( CHL2MPBot *me, CBaseCombatCharacter *whoFired, CBaseCombatWeapon *weapon );
	virtual EventDesiredResult< CHL2MPBot > OnOtherKilled( CHL2MPBot *me, CBaseCombatCharacter *victim, const CTakeDamageInfo &info );
	virtual EventDesiredResult< CHL2MPBot > OnKilled( CHL2MPBot *me, const CTakeDamageInfo &info );

	virtual QueryResultType ShouldAttack( const INextBot *me, const CKnownEntity *them ) const;

	virtual Vector SelectTargetPoint( const INextBot *me, const CBaseCombatCharacter *subject ) const;

	virtual const char *GetName( void ) const { return "TrokMainAction"; }

protected:
	virtual void Dodge( CHL2MPBot *me );

private:
	void ListenForFootsteps( CHL2MPBot *me );
	void CheckCorners( CHL2MPBot *me );
	void UpdateFireWindow( CHL2MPBot *me );
	void UpdateADS( CHL2MPBot *me );
	void UpdateSuppression( CHL2MPBot *me );

	// Mira (estado de um "engajamento" com o alvo atual).
	mutable CHandle< CBaseEntity > m_hAimSubject;
	mutable float m_flAimStart;
	mutable float m_flAimLastQuery;
	mutable float m_flAimInitialError;		// graus
	mutable float m_flAimPhase;				// direção do erro no plano do alvo
	mutable float m_flAimWobble;
	mutable bool m_bAimHead;
	float m_flFlinchError;					// graus extras ao levar dano (some rápido)
	float m_flFlinchTime;

	// Strafe.
	float m_flRetargetUntil;				// depois de derrubar o alvo, um instante até atirar no próximo

	int m_iStrafeDir;						// -1 esquerda, 0 parado, 1 direita
	int m_iLastStrafeDir;
	CountdownTimer m_strafeTimer;

	// Ritmo de tiro: anda (sem atirar) -> contra-strafe -> rajada parado -> recupera.
	enum CombatPhase_t { PHASE_MOVE, PHASE_STOP, PHASE_BURST, PHASE_RECOVER };
	CombatPhase_t m_combatPhase;
	CountdownTimer m_phaseTimer;
	bool m_bDisciplinedCycle;				// este ciclo segue o ritmo (senão: spray andando)
	bool m_bFireWindowOpen;

	// Checar cantos.
	CNavArea *m_pCornerArea;
	CNavArea *m_pCornerPrevArea;
	int m_iCornerSpot;
	CountdownTimer m_cornerTimer;

	CountdownTimer m_listenTimer;
	CountdownTimer m_intelTimer;

	// Mira pelo cano e fogo de supressão (ideias do NEOTOKYO Rebuild: ShouldAim e
	// CNEOBotSuppressiveFire)
	CountdownTimer m_adsTimer;
	float m_flLastVisibleThreat;
	bool m_bSuppressArmed;					// viu o inimigo; pode suprimir quando ele sumir
	float m_flSuppressUntil;
	Vector m_vecSuppress;
	float m_flLastInjured;
	float m_flThreatSeenSince;				// emboscada: há quanto tempo o alvo atual está à vista
	CHandle< CBaseEntity > m_hThreatSeen;
};

#endif // TROK_BOT_MAIN_ACTION_H

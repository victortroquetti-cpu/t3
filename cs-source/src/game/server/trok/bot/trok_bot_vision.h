//=============================================================================
// Trok: percepção dos bots.
//
// Cada inimigo tem uma "barra de percepção" (0..1) por bot. Ela enche com o
// inimigo à vista, mais rápido quando ele está no centro da visão, perto,
// bem exposto, iluminado e se mexendo; e esvazia devagar quando some. Só ao
// passar de 1 o bot "percebe" o inimigo; depois disso vem o tempo de reação.
// Tiro (clarão do cano) e encostar no bot fazem perceber na hora.
// Passos ouvidos enchem a barra só até perto do limite e fazem o bot olhar
// para o barulho: ouvir não é ver.
//=============================================================================

#ifndef TROK_BOT_VISION_H
#define TROK_BOT_VISION_H
#ifdef _WIN32
#pragma once
#endif

#include "bot/hl2mp_bot_vision.h"
#include "trok_bot_profile.h"

class CHL2MPBot;

class CTrokBotVision : public CHL2MPBotVision
{
	typedef CHL2MPBotVision BaseClass;

public:
	CTrokBotVision( INextBot *bot );
	virtual ~CTrokBotVision() {}

	virtual void Reset( void );
	virtual bool IsVisibleEntityNoticed( CBaseEntity *subject ) const;
	virtual float GetMinRecognizeTime( void ) const;
	virtual void SetFieldOfView( float horizAngle );

	const TrokBotProfile_t &GetProfile( void ) const;

	// Eventos vindos da ação principal.
	void NoteGunfire( CBaseEntity *who );						// alguém atirou (clarão)
	void NoteNoise( CBaseEntity *who, float flAmount );			// ouviu passos dessa pessoa
	bool IsAlert( void ) const { return gpGlobals->curtime < m_flAlertUntil; }

	// 0..1, para depuração (trok_bot_debug_percepcao).
	float GetAwareness( CBaseEntity *subject ) const;

private:
	struct Awareness_t
	{
		float flValue;				// 0..1 (>= 1 = percebido)
		float flLastUpdate;
		float flExposure;			// 0..1, fração do corpo à vista (cache)
		float flExposureTime;
		float flNoise;				// fator aleatório do momento (0.7..1.3)
		float flNoiseTime;
		float flLastGunfire;
	};

	Awareness_t *GetState( CBaseEntity *subject ) const;
	float ComputeExposure( CBaseEntity *subject ) const;
	float ComputeGain( CBaseEntity *subject, Awareness_t &state ) const;

	mutable Awareness_t m_state[ MAX_PLAYERS + 1 ];
	mutable TrokBotProfile_t m_profile;
	mutable int m_iProfileDifficulty;
	mutable float m_flReactionTime;
	mutable float m_flReactionRollTime;
	float m_flAlertUntil;
};

#endif // TROK_BOT_VISION_H

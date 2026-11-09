//=============================================================================
// Trok: memória compartilhada dos bots (o que o time sabe e para onde vai).
//
// Serve para os bots não andarem todos iguais:
//   - pistas: onde inimigos foram vistos, ouvidos ou atiraram há pouco
//     (do time; no mata-mata sem time, cada bot só usa as próprias);
//   - destinos: para onde cada bot está indo, para os outros escolherem outro;
//   - caminhos: áreas do nav que colegas acabaram de usar como rota, para
//     os próximos preferirem outro corredor (flanco natural);
//   - perigo: áreas onde alguém morreu ficam "perigosas" por um tempo
//     (CNavArea::IncreaseDanger, que decai sozinho).
//=============================================================================

#ifndef TROK_BOT_TEAM_H
#define TROK_BOT_TEAM_H
#ifdef _WIN32
#pragma once
#endif

#include "nav_mesh.h"
#include "nav_pathfind.h"
#include "NextBot/Path/NextBotPath.h"

class CHL2MPBot;
class CNavArea;

struct TrokBotIntel_t
{
	Vector pos;
	float flTime;
	int iTeam;			// time de quem viu/ouviu
	int iObserver;		// entindex de quem viu/ouviu
};

// Pistas
void TrokBot_AddIntel( CHL2MPBot *observer, const Vector &pos, float flErrorRadius );
void TrokBot_CollectIntel( CHL2MPBot *me, float flMaxAge, CUtlVector< TrokBotIntel_t > *pOut );

// Destinos
void TrokBot_ClaimGoal( CHL2MPBot *me, const Vector &goal );
void TrokBot_ReleaseGoal( CHL2MPBot *me );
int TrokBot_CountClaimsNear( CHL2MPBot *me, const Vector &pos, float flRadius );

// Caminhos
void TrokBot_NotePath( CHL2MPBot *me, const Path &path );
bool TrokBot_IsAreaUsedByTeam( CHL2MPBot *me, const CNavArea *area );

// Vigia: "eu cuido deste ângulo". Os colegas olham para outro.
void TrokBot_ClaimWatch( CHL2MPBot *me, const Vector &target, float flDuration );
bool TrokBot_IsWatchedByTeammate( CHL2MPBot *me, const Vector &target, float flRadius );

// Posições seguradas (âncoras): ninguém empilha na mesma área e o time
// prefere segurar regiões diferentes.
void TrokBot_ClaimHold( CHL2MPBot *me, CNavArea *area );
void TrokBot_ReleaseHold( CHL2MPBot *me );
bool TrokBot_IsHoldTaken( CHL2MPBot *me, const CNavArea *area );
int TrokBot_CountTeamHoldsInRegion( CHL2MPBot *me, int iRegion );
bool TrokBot_IsHolding( const CHL2MPBot *me );			// chegou e está segurando
void TrokBot_SetHoldArrived( CHL2MPBot *me, bool bArrived );

// Andando com colegas por perto: direção que ninguém está olhando
// (frente, flancos, costas). Devolve false se não há colega perto.
bool TrokBot_PickUncoveredLook( CHL2MPBot *me, float flRadius, Vector *pLookAt );

// Suporte: escolhe um colega para acompanhar (no máximo 2 por líder).
CBasePlayer *TrokBot_PickLeader( CHL2MPBot *me, float flMaxRange );
void TrokBot_SetLeader( CHL2MPBot *me, CBasePlayer *pLeader );

// Perigo (chamado quando alguém morre)
void TrokBot_NoteDeath( CBaseCombatCharacter *victim );

// Área onde um bot travou: o time aprende e evita (ideia do NEOTOKYO Rebuild,
// neo_bot_path_reservation). Decai com o tempo; zera ao trocar de mapa.
void TrokBot_NoteStuck( CHL2MPBot *me );
float TrokBot_StuckLevel( const CNavArea *area );

// Custo de caminho com variedade: cada bot tem uma "preferência de rota"
// (corredores que ele gosta mais), evita a rota dos colegas e, se for
// cauteloso, áreas onde gente morreu há pouco.
class CTrokBotPathCost : public IPathCost
{
public:
	CTrokBotPathCost( CHL2MPBot *me, unsigned int nRouteSeed );

	virtual float operator()( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder, const CFuncElevator *elevator, float length ) const;

private:
	CHL2MPBot *m_me;
	unsigned int m_nSeed;
	float m_flVariety;
	float m_flTeamPenalty;
	float m_flCaution;
	float m_stepHeight;
	float m_maxJumpHeight;
	float m_maxDropHeight;
};

#endif // TROK_BOT_TEAM_H

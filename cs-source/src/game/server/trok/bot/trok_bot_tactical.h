//=============================================================================
// Trok: mapa tático dos bots (lugares estratégicos).
//
// Calculado uma vez por mapa a partir do nav mesh:
//   1. Regiões: áreas ligadas por passagem a pé (sem pulo) viram uma região
//      (uma sala, um corredor, o topo de uma caixa, uma pilastra).
//   2. Entradas de cada região: a pé (fácil), por pulo para cima (difícil),
//      por queda de cima (fácil para quem ataca).
//   3. Custo de acesso: quantos pulos, no mínimo, separam a região da maior
//      região do mapa. Um lugar que só se alcança pulando três pilastras
//      quebradas tem custo 3 e poucas entradas: bom para se defender.
//   4. Nota de cada área: defensabilidade da região, pouca exposição (de
//      quantos lugares dá para vê-la), altura sobre o entorno, cobertura
//      (hiding spots do nav) e visão das entradas.
//
// Precisa do .nav do mapa; exposição e cobertura ficam melhores depois de
// nav_analyze (que calcula visibilidade e hiding spots).
//=============================================================================

#ifndef TROK_BOT_TACTICAL_H
#define TROK_BOT_TACTICAL_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "utlmap.h"

class CNavArea;
class CHL2MPBot;
struct TrokBotProfile_t;

struct TrokTacticalArea_t
{
	CNavArea *pArea;
	int iRegion;
	float flExposure;		// 0..1 (1 = vista de quase todo o mapa)
	float flHeight;			// 0..1 (altura sobre o que ela enxerga)
	float flCover;			// 0..1 (hiding spots com cobertura)
	float flScore;			// 0..1 (nota final para segurar posição)
};

struct TrokTacticalRegion_t
{
	int iAreaCount;
	float flSize;			// soma das áreas (u²)
	int iWalkEntrances;		// entradas a pé
	int iJumpEntrances;		// entradas só pulando para cima
	int iDropEntrances;		// entradas caindo de cima
	int iAccessCost;		// pulos mínimos desde a região principal
	float flDefense;		// 0..1
	CUtlVector< CNavArea * > entranceAreas;	// áreas da região onde alguém chega (para vigiar)
};

class CTrokTacticalMap
{
public:
	CTrokTacticalMap();

	// Reconstrói se o mapa ou o nav mudaram. Barato quando já está pronto.
	void Update( void );
	bool IsReady( void ) const { return m_bReady; }

	const TrokTacticalArea_t *GetArea( const CNavArea *area ) const;
	const TrokTacticalRegion_t *GetRegion( int iRegion ) const;
	int GetRegionCount( void ) const { return m_regions.Count(); }

	// Melhor área para segurar posição a até flMaxRange de distância (em linha reta)
	// do bot, com a escolha "imperfeita" de bots de pouco uso tático. NULL se não há.
	CNavArea *FindHoldArea( CHL2MPBot *me, float flMaxRange, const CUtlVector< CNavArea * > *pAvoid = NULL ) const;

	void DebugDraw( const Vector &center, float flRadius ) const;
	void PrintSummary( void ) const;

private:
	void Build( void );
	void BuildRegions( void );
	void BuildEntrances( void );
	void BuildAccessCost( void );
	void BuildAreaScores( void );

	bool m_bReady;
	char m_szMap[ 64 ];
	int m_iNavAreaCount;

	CUtlVector< TrokTacticalArea_t > m_areas;
	CUtlMap< const CNavArea *, int > m_areaIndex;
	CUtlVector< TrokTacticalRegion_t > m_regions;
	int m_iMainRegion;
};

CTrokTacticalMap &TrokTacticalMap( void );

#endif // TROK_BOT_TACTICAL_H

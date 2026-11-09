//=============================================================================
// Trok: mapa tático dos bots. Ver trok_bot_tactical.h.
//=============================================================================

#include "cbase.h"
#include "trok_bot.h"
#include "trok_bot_tactical.h"
#include "bot/hl2mp_bot.h"
#include "nav_mesh.h"
#include "nav_area.h"
#include "fmtstr.h"
#include "trok_bot_team.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Diferença de altura (u) até a qual a passagem é "a pé" (degrau).
#define TACT_WALK_HEIGHT	( StepHeight + 2.0f )
// Custo de acesso dado às regiões que não se alcançam a partir da principal.
#define TACT_UNREACHABLE	99

enum LinkType_t
{
	LINK_WALK = 0,
	LINK_JUMP,		// subir pulando
	LINK_DROP,		// descer caindo
};

static LinkType_t ClassifyLink( const CNavArea *from, const CNavArea *to )
{
	if ( ( from->GetAttributes() | to->GetAttributes() ) & NAV_MESH_JUMP )
		return LINK_JUMP;

	float flDelta = from->ComputeAdjacentConnectionHeightChange( to );
	if ( flDelta == FLT_MAX )
		return LINK_WALK;
	if ( flDelta > TACT_WALK_HEIGHT )
		return LINK_JUMP;
	if ( flDelta < -TACT_WALK_HEIGHT )
		return LINK_DROP;
	return LINK_WALK;
}

// Percorre todas as ligações de saída de uma área.
template < typename Func >
static void ForEachLink( CNavArea *area, Func &func )
{
	for ( int dir = 0; dir < NUM_DIRECTIONS; ++dir )
	{
		int n = area->GetAdjacentCount( (NavDirType)dir );
		for ( int i = 0; i < n; ++i )
		{
			CNavArea *other = area->GetAdjacentArea( (NavDirType)dir, i );
			if ( other )
				func( area, other );
		}
	}
}

//-----------------------------------------------------------------------------

CTrokTacticalMap::CTrokTacticalMap() : m_areaIndex( DefLessFunc( const CNavArea * ) )
{
	m_bReady = false;
	m_szMap[ 0 ] = '\0';
	m_iNavAreaCount = -1;
	m_iMainRegion = -1;
}

CTrokTacticalMap &TrokTacticalMap( void )
{
	static CTrokTacticalMap s_map;
	return s_map;
}

void CTrokTacticalMap::Update( void )
{
	if ( !TheNavMesh || !TheNavMesh->IsLoaded() || TheNavAreas.Count() == 0 )
	{
		m_bReady = false;
		return;
	}

	const char *pszMap = STRING( gpGlobals->mapname );
	if ( m_bReady && m_iNavAreaCount == TheNavAreas.Count() && !Q_stricmp( m_szMap, pszMap ) )
		return;

	Q_strncpy( m_szMap, pszMap, sizeof( m_szMap ) );
	m_iNavAreaCount = TheNavAreas.Count();
	Build();
	m_bReady = true;
}

const TrokTacticalArea_t *CTrokTacticalMap::GetArea( const CNavArea *area ) const
{
	unsigned short i = m_areaIndex.Find( area );
	return ( i == m_areaIndex.InvalidIndex() ) ? NULL : &m_areas[ m_areaIndex[ i ] ];
}

const TrokTacticalRegion_t *CTrokTacticalMap::GetRegion( int iRegion ) const
{
	return ( iRegion >= 0 && iRegion < m_regions.Count() ) ? &m_regions[ iRegion ] : NULL;
}

void CTrokTacticalMap::Build( void )
{
	double flStart = Plat_FloatTime();

	m_areas.RemoveAll();
	m_areaIndex.RemoveAll();
	m_regions.RemoveAll();
	m_iMainRegion = -1;

	m_areas.EnsureCapacity( TheNavAreas.Count() );
	FOR_EACH_VEC( TheNavAreas, it )
	{
		TrokTacticalArea_t entry;
		entry.pArea = TheNavAreas[ it ];
		entry.iRegion = -1;
		entry.flExposure = 0.5f;
		entry.flHeight = 0.5f;
		entry.flCover = 0.0f;
		entry.flScore = 0.0f;
		m_areaIndex.Insert( entry.pArea, m_areas.AddToTail( entry ) );
	}

	BuildRegions();
	BuildEntrances();
	BuildAccessCost();
	BuildAreaScores();

	DevMsg( "[Trok] mapa tático: %d áreas, %d regiões (%.0f ms)\n",
		m_areas.Count(), m_regions.Count(), 1000.0 * ( Plat_FloatTime() - flStart ) );
}

// Regiões = componentes ligados por passagem a pé (flood fill).
void CTrokTacticalMap::BuildRegions( void )
{
	CUtlVector< int > stack;

	for ( int start = 0; start < m_areas.Count(); ++start )
	{
		if ( m_areas[ start ].iRegion >= 0 )
			continue;

		int iRegion = m_regions.AddToTail();
		TrokTacticalRegion_t &region = m_regions[ iRegion ];
		region.iAreaCount = 0;
		region.flSize = 0.0f;
		region.iWalkEntrances = region.iJumpEntrances = region.iDropEntrances = 0;
		region.iAccessCost = TACT_UNREACHABLE;
		region.flDefense = 0.0f;

		m_areas[ start ].iRegion = iRegion;
		stack.AddToTail( start );

		while ( stack.Count() )
		{
			int cur = stack.Tail();
			stack.RemoveMultipleFromTail( 1 );

			CNavArea *area = m_areas[ cur ].pArea;
			region.iAreaCount++;
			region.flSize += area->GetSizeX() * area->GetSizeY();

			struct Visit
			{
				CTrokTacticalMap *map; CUtlVector< int > *stack; int iRegion;
				void operator()( CNavArea *from, CNavArea *to )
				{
					if ( ClassifyLink( from, to ) != LINK_WALK )
						return;
					// a pé nos dois sentidos (ou só neste, se a volta não existe)
					if ( to->IsConnected( from, NUM_DIRECTIONS ) && ClassifyLink( to, from ) != LINK_WALK )
						return;
					const TrokTacticalArea_t *pTo = map->GetArea( to );
					if ( !pTo || pTo->iRegion >= 0 )
						return;
					int idx = pTo - map->m_areas.Base();
					map->m_areas[ idx ].iRegion = iRegion;
					stack->AddToTail( idx );
				}
			} visit = { this, &stack, iRegion };

			ForEachLink( area, visit );
		}
	}

	// Região principal = a maior em área de chão.
	float flBiggest = -1.0f;
	FOR_EACH_VEC( m_regions, r )
	{
		if ( m_regions[ r ].flSize > flBiggest )
		{
			flBiggest = m_regions[ r ].flSize;
			m_iMainRegion = r;
		}
	}
}

// Entradas: ligações de fora da região para dentro dela.
void CTrokTacticalMap::BuildEntrances( void )
{
	FOR_EACH_VEC( m_areas, a )
	{
		struct Visit
		{
			CTrokTacticalMap *map;
			void operator()( CNavArea *from, CNavArea *to )
			{
				const TrokTacticalArea_t *pFrom = map->GetArea( from );
				const TrokTacticalArea_t *pTo = map->GetArea( to );
				if ( !pFrom || !pTo || pFrom->iRegion == pTo->iRegion )
					return;

				TrokTacticalRegion_t &region = map->m_regions[ pTo->iRegion ];
				if ( region.entranceAreas.Find( to ) == region.entranceAreas.InvalidIndex() )
					region.entranceAreas.AddToTail( to );

				switch ( ClassifyLink( from, to ) )
				{
				case LINK_WALK:	region.iWalkEntrances++; break;
				case LINK_JUMP:	region.iJumpEntrances++; break;
				case LINK_DROP:	region.iDropEntrances++; break;
				}
			}
		} visit = { this };

		ForEachLink( m_areas[ a ].pArea, visit );
	}
}

// Custo de acesso = menor número de pulos para chegar desde a região principal
// (Dijkstra pequeno no grafo de regiões; a pé e queda custam 0, pulo custa 1).
void CTrokTacticalMap::BuildAccessCost( void )
{
	if ( m_iMainRegion < 0 )
		return;

	m_regions[ m_iMainRegion ].iAccessCost = 0;

	bool bChanged = true;
	int iGuard = 0;
	while ( bChanged && iGuard++ < 64 )
	{
		bChanged = false;
		FOR_EACH_VEC( m_areas, a )
		{
			struct Relax
			{
				CTrokTacticalMap *map; bool *changed;
				void operator()( CNavArea *from, CNavArea *to )
				{
					const TrokTacticalArea_t *pFrom = map->GetArea( from );
					const TrokTacticalArea_t *pTo = map->GetArea( to );
					if ( !pFrom || !pTo || pFrom->iRegion == pTo->iRegion )
						return;
					int iFromCost = map->m_regions[ pFrom->iRegion ].iAccessCost;
					if ( iFromCost >= TACT_UNREACHABLE )
						return;
					int iCost = iFromCost + ( ClassifyLink( from, to ) == LINK_JUMP ? 1 : 0 );
					int &iToCost = map->m_regions[ pTo->iRegion ].iAccessCost;
					if ( iCost < iToCost )
					{
						iToCost = iCost;
						*changed = true;
					}
				}
			} relax = { this, &bChanged };

			ForEachLink( m_areas[ a ].pArea, relax );
		}
	}

	// Defensabilidade de cada região.
	FOR_EACH_VEC( m_regions, r )
	{
		TrokTacticalRegion_t &region = m_regions[ r ];

		float flEntrances = region.iWalkEntrances * 1.0f + region.iDropEntrances * 0.7f + region.iJumpEntrances * 0.3f;
		float flFewEntrances = 1.0f / ( 1.0f + 0.5f * flEntrances );
		float flHardToReach = MIN( region.iAccessCost, 4 ) / 4.0f;

		// Região enorme e aberta não se defende; região mínima só cabe um.
		float flSizeFactor = RemapValClamped( region.flSize, 150000.0f, 600000.0f, 1.0f, 0.5f );

		region.flDefense = ( r == m_iMainRegion || region.iAccessCost >= TACT_UNREACHABLE ) ? 0.0f :
			clamp( ( 0.6f * flFewEntrances + 0.4f * flHardToReach ) * flSizeFactor, 0.0f, 1.0f );
	}
}

// Nota de cada área.
void CTrokTacticalMap::BuildAreaScores( void )
{
	// Exposição e altura vêm da visibilidade pré-calculada do nav (nav_analyze).
	int iMaxVisible = 0;
	CUtlVector< int > visibleCount;
	CUtlVector< float > avgVisibleZ;
	visibleCount.SetCount( m_areas.Count() );
	avgVisibleZ.SetCount( m_areas.Count() );

	FOR_EACH_VEC( m_areas, a )
	{
		struct Count
		{
			int n; float zSum;
			bool operator()( CNavArea *other ) { ++n; zSum += other->GetCenter().z; return true; }
		} count = { 0, 0.0f };

		m_areas[ a ].pArea->ForAllPotentiallyVisibleAreas( count );
		visibleCount[ a ] = count.n;
		avgVisibleZ[ a ] = count.n ? count.zSum / count.n : m_areas[ a ].pArea->GetCenter().z;
		iMaxVisible = MAX( iMaxVisible, count.n );
	}

	bool bHasVisibility = iMaxVisible > 0;

	FOR_EACH_VEC( m_areas, a )
	{
		TrokTacticalArea_t &entry = m_areas[ a ];
		CNavArea *area = entry.pArea;
		const TrokTacticalRegion_t &region = m_regions[ entry.iRegion ];

		if ( bHasVisibility )
		{
			entry.flExposure = (float)visibleCount[ a ] / (float)iMaxVisible;
			float flAbove = area->GetCenter().z - avgVisibleZ[ a ];
			entry.flHeight = clamp( 0.5f + flAbove / 256.0f, 0.0f, 1.0f );
		}

		int iCover = 0;
		const HidingSpotVector *spots = area->GetHidingSpots();
		FOR_EACH_VEC( *spots, s )
		{
			if ( (*spots)[ s ]->HasGoodCover() )
				++iCover;
		}
		entry.flCover = MIN( iCover, 2 ) / 2.0f;

		// Consegue vigiar por onde o inimigo chega?
		float flWatch = 0.5f;
		if ( bHasVisibility && region.entranceAreas.Count() )
		{
			int iSeen = 0;
			FOR_EACH_VEC( region.entranceAreas, e )
			{
				if ( region.entranceAreas[ e ] == area || area->IsPotentiallyVisible( region.entranceAreas[ e ] ) )
					++iSeen;
			}
			flWatch = (float)iSeen / (float)region.entranceAreas.Count();
		}

		bool bUsable = !( area->GetAttributes() & NAV_MESH_AVOID ) && !area->IsUnderwater() &&
			area->GetSizeX() >= 16.0f && area->GetSizeY() >= 16.0f && region.iAccessCost < TACT_UNREACHABLE;

		entry.flScore = bUsable ? clamp(
			0.45f * region.flDefense +
			0.25f * ( 1.0f - entry.flExposure ) +
			0.12f * entry.flHeight +
			0.08f * entry.flCover +
			0.10f * flWatch, 0.0f, 1.0f ) : 0.0f;
	}
}

//-----------------------------------------------------------------------------

CNavArea *CTrokTacticalMap::FindHoldArea( CHL2MPBot *me, float flMaxRange, const CUtlVector< CNavArea * > *pAvoid ) const
{
	if ( !m_bReady )
		return NULL;

	const TrokBotProfile_t &profile = TrokBot_GetProfile( me );
	float flTactics = profile.flUsoTatico;

	CNavArea *pBest = NULL;
	float flBest = -1.0f;
	Vector origin = me->GetAbsOrigin();

	FOR_EACH_VEC( m_areas, a )
	{
		const TrokTacticalArea_t &entry = m_areas[ a ];
		if ( entry.flScore <= 0.0f )
			continue;

		float flDist = ( entry.pArea->GetCenter() - origin ).Length();
		if ( flDist > flMaxRange )
			continue;

		if ( pAvoid && pAvoid->Find( entry.pArea ) != pAvoid->InvalidIndex() )
			continue;

		// Ninguém empilha na mesma área.
		if ( TrokBot_IsHoldTaken( me, entry.pArea ) )
			continue;

		// Bot tático escolhe pela nota; bot "burrinho" escolhe meio ao acaso.
		float flValue = flTactics * entry.flScore + ( 1.0f - flTactics ) * RandomFloat( 0.0f, 0.6f ) - 0.15f * flDist / flMaxRange;

		// O time cobre lugares diferentes: região que um colega já segura vale menos.
		flValue -= 0.25f * TrokBot_CountTeamHoldsInRegion( me, entry.iRegion );

		// Âncora quer esconderijo: coberto e pouco exposto, para surpreender.
		if ( profile.iEstilo == TROK_ESTILO_ANCORA )
			flValue += 0.15f * entry.flCover + 0.1f * ( 1.0f - entry.flExposure );
		if ( flValue > flBest )
		{
			flBest = flValue;
			pBest = entry.pArea;
		}
	}

	return pBest;
}

void CTrokTacticalMap::DebugDraw( const Vector &center, float flRadius ) const
{
	const float flDuration = 15.0f;

	FOR_EACH_VEC( m_areas, a )
	{
		const TrokTacticalArea_t &entry = m_areas[ a ];
		if ( ( entry.pArea->GetCenter() - center ).Length() > flRadius )
			continue;

		int r = (int)( 255 * ( 1.0f - entry.flScore ) );
		int g = (int)( 255 * entry.flScore );
		entry.pArea->DrawFilled( r, g, 0, 70, flDuration, true, 3.0f );

		const TrokTacticalRegion_t &region = m_regions[ entry.iRegion ];
		NDebugOverlay::Text( entry.pArea->GetCenter() + Vector( 0, 0, 10 ),
			CFmtStr( "%.2f  R%d  acesso %d  entradas %d/%d/%d", entry.flScore, entry.iRegion, region.iAccessCost,
				region.iWalkEntrances, region.iJumpEntrances, region.iDropEntrances ),
			true, flDuration );
	}
}

void CTrokTacticalMap::PrintSummary( void ) const
{
	Msg( "[Trok] mapa tático de %s: %d áreas, %d regiões (principal R%d)\n", m_szMap, m_areas.Count(), m_regions.Count(), m_iMainRegion );
	Msg( "  região  áreas  acesso  entradas(pé/pulo/queda)  defesa\n" );

	// As 15 regiões mais defensáveis.
	CUtlVector< int > order;
	FOR_EACH_VEC( m_regions, r ) order.AddToTail( r );
	for ( int i = 0; i < order.Count() && i < 15; ++i )
	{
		for ( int j = i + 1; j < order.Count(); ++j )
		{
			if ( m_regions[ order[ j ] ].flDefense > m_regions[ order[ i ] ].flDefense )
				V_swap( order[ i ], order[ j ] );
		}
		const TrokTacticalRegion_t &region = m_regions[ order[ i ] ];
		Msg( "  R%-5d %5d  %6d  %4d/%d/%d                %.2f\n", order[ i ], region.iAreaCount, region.iAccessCost,
			region.iWalkEntrances, region.iJumpEntrances, region.iDropEntrances, region.flDefense );
	}
}

//-----------------------------------------------------------------------------
// Comandos de depuração
//-----------------------------------------------------------------------------

CON_COMMAND_F( trok_bot_tatico_mostrar, "Pinta as áreas do nav pela nota tática (verde = bom para segurar) por 15 s. Uso: trok_bot_tatico_mostrar [raio]", FCVAR_CHEAT )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer )
		pPlayer = UTIL_GetListenServerHost();
	if ( !pPlayer )
		return;

	TrokTacticalMap().Update();
	if ( !TrokTacticalMap().IsReady() )
	{
		Msg( "[Trok] mapa sem nav mesh: rode nav_generate (e nav_analyze para visibilidade e cobertura).\n" );
		return;
	}

	float flRadius = args.ArgC() > 1 ? atof( args[ 1 ] ) : 1500.0f;
	TrokTacticalMap().DebugDraw( pPlayer->GetAbsOrigin(), flRadius );
}

CON_COMMAND_F( trok_bot_tatico_info, "Lista as regiões mais defensáveis do mapa tático.", FCVAR_GAMEDLL )
{
	TrokTacticalMap().Update();
	if ( !TrokTacticalMap().IsReady() )
	{
		Msg( "[Trok] mapa sem nav mesh.\n" );
		return;
	}
	TrokTacticalMap().PrintSummary();
}

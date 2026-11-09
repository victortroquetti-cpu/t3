//=============================================================================
// Trok: memória compartilhada dos bots. Ver trok_bot_team.h.
//=============================================================================

#include "cbase.h"
#include "trok_bot.h"
#include "trok_bot_team.h"
#include "bot/hl2mp_bot.h"
#include "nav_area.h"
#include "trok_bot_tactical.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar trok_bot_rota_variedade( "trok_bot_rota_variedade", "1.0", FCVAR_CHEAT,
	"Quanto cada bot prefere 'os seus' corredores (0 = todos pegam a mesma rota mais curta)." );
static ConVar trok_bot_rota_colegas( "trok_bot_rota_colegas", "0.6", FCVAR_CHEAT,
	"Custo extra (fração) de passar por onde um colega acabou de passar. Faz o time se espalhar e flanquear." );

#define INTEL_MAX			64
#define PATH_USE_TIME		20.0f	// s que uma rota de colega "pesa"
#define PATH_USE_AREAS		40		// quantas áreas do começo da rota contam
#define ROUTE_CELL			512.0f	// tamanho (u) da célula da preferência de rota

// Mata-mata sem time: ninguém é colega.
static bool HasTeammates( int iTeam )
{
	return iTeam == TEAM_COMBINE || iTeam == TEAM_REBELS;
}

//-----------------------------------------------------------------------------
// Pistas
//-----------------------------------------------------------------------------

static TrokBotIntel_t s_intel[ INTEL_MAX ];
static int s_iIntelNext = 0;

void TrokBot_AddIntel( CHL2MPBot *observer, const Vector &pos, float flErrorRadius )
{
	// A mesma pista repetida (o mesmo bot vendo o mesmo lugar) só atualiza a hora.
	for ( int i = 0; i < INTEL_MAX; ++i )
	{
		TrokBotIntel_t &old = s_intel[ i ];
		if ( old.iObserver == observer->entindex() && ( old.pos - pos ).LengthSqr() < 200.0f * 200.0f &&
			 gpGlobals->curtime - old.flTime < 5.0f )
		{
			old.flTime = gpGlobals->curtime;
			return;
		}
	}

	TrokBotIntel_t &intel = s_intel[ s_iIntelNext ];
	s_iIntelNext = ( s_iIntelNext + 1 ) % INTEL_MAX;

	intel.pos = pos + Vector( RandomFloat( -flErrorRadius, flErrorRadius ), RandomFloat( -flErrorRadius, flErrorRadius ), 0.0f );
	intel.flTime = gpGlobals->curtime;
	intel.iTeam = observer->GetTeamNumber();
	intel.iObserver = observer->entindex();
}

void TrokBot_CollectIntel( CHL2MPBot *me, float flMaxAge, CUtlVector< TrokBotIntel_t > *pOut )
{
	int iTeam = me->GetTeamNumber();
	bool bShared = HasTeammates( iTeam );

	for ( int i = 0; i < INTEL_MAX; ++i )
	{
		const TrokBotIntel_t &intel = s_intel[ i ];
		if ( intel.flTime <= 0.0f || gpGlobals->curtime - intel.flTime > flMaxAge )
			continue;

		bool bMine = intel.iObserver == me->entindex();
		if ( bMine || ( bShared && intel.iTeam == iTeam ) )
			pOut->AddToTail( intel );
	}
}

//-----------------------------------------------------------------------------
// Destinos
//-----------------------------------------------------------------------------

struct GoalClaim_t
{
	int iBot;
	int iTeam;
	Vector pos;
};
static CUtlVector< GoalClaim_t > s_claims;

void TrokBot_ReleaseGoal( CHL2MPBot *me )
{
	for ( int i = s_claims.Count() - 1; i >= 0; --i )
	{
		if ( s_claims[ i ].iBot == me->entindex() )
			s_claims.FastRemove( i );
	}
}

void TrokBot_ClaimGoal( CHL2MPBot *me, const Vector &goal )
{
	TrokBot_ReleaseGoal( me );

	GoalClaim_t claim;
	claim.iBot = me->entindex();
	claim.iTeam = me->GetTeamNumber();
	claim.pos = goal;
	s_claims.AddToTail( claim );
}

int TrokBot_CountClaimsNear( CHL2MPBot *me, const Vector &pos, float flRadius )
{
	int iCount = 0;
	FOR_EACH_VEC( s_claims, i )
	{
		const GoalClaim_t &claim = s_claims[ i ];
		if ( claim.iBot == me->entindex() )
			continue;

		// No mata-mata todo mundo se espalha; em time, só os colegas contam.
		if ( HasTeammates( me->GetTeamNumber() ) && claim.iTeam != me->GetTeamNumber() )
			continue;

		CBasePlayer *pOther = UTIL_PlayerByIndex( claim.iBot );
		if ( !pOther || !pOther->IsAlive() )
			continue;

		if ( ( claim.pos - pos ).LengthSqr() < flRadius * flRadius )
			++iCount;
	}
	return iCount;
}

//-----------------------------------------------------------------------------
// Caminhos dos colegas
//-----------------------------------------------------------------------------

static CUtlMap< unsigned int, float > s_pathUse[ MAX_TEAMS ];
static bool s_bPathUseInit = false;

static CUtlMap< unsigned int, float > &PathUse( int iTeam )
{
	if ( !s_bPathUseInit )
	{
		for ( int i = 0; i < MAX_TEAMS; ++i )
			s_pathUse[ i ].SetLessFunc( DefLessFunc( unsigned int ) );
		s_bPathUseInit = true;
	}
	return s_pathUse[ clamp( iTeam, 0, MAX_TEAMS - 1 ) ];
}

void TrokBot_NotePath( CHL2MPBot *me, const Path &path )
{
	CUtlMap< unsigned int, float > &use = PathUse( me->GetTeamNumber() );
	float flExpire = gpGlobals->curtime + PATH_USE_TIME;

	// Limpa entradas vencidas de vez em quando.
	if ( use.Count() > 2000 )
	{
		for ( int i = use.FirstInorder(); i != use.InvalidIndex(); )
		{
			int next = use.NextInorder( i );
			if ( use[ i ] < gpGlobals->curtime )
				use.RemoveAt( i );
			i = next;
		}
	}

	int n = 0;
	for ( const Path::Segment *seg = path.FirstSegment(); seg && n < PATH_USE_AREAS; seg = path.NextSegment( seg ), ++n )
	{
		if ( !seg->area )
			continue;

		unsigned int id = seg->area->GetID();
		int i = use.Find( id );
		if ( i == use.InvalidIndex() )
			use.Insert( id, flExpire );
		else
			use[ i ] = flExpire;
	}
}

bool TrokBot_IsAreaUsedByTeam( CHL2MPBot *me, const CNavArea *area )
{
	CUtlMap< unsigned int, float > &use = PathUse( me->GetTeamNumber() );
	int i = use.Find( area->GetID() );
	return i != use.InvalidIndex() && use[ i ] > gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Vigia
//-----------------------------------------------------------------------------

struct WatchClaim_t
{
	float flExpire;
	int iTeam;
	Vector target;
};
static WatchClaim_t s_watch[ MAX_PLAYERS + 1 ];

void TrokBot_ClaimWatch( CHL2MPBot *me, const Vector &target, float flDuration )
{
	int i = me->entindex();
	if ( i <= 0 || i > MAX_PLAYERS )
		return;
	s_watch[ i ].flExpire = gpGlobals->curtime + flDuration;
	s_watch[ i ].iTeam = me->GetTeamNumber();
	s_watch[ i ].target = target;
}

bool TrokBot_IsWatchedByTeammate( CHL2MPBot *me, const Vector &target, float flRadius )
{
	if ( !HasTeammates( me->GetTeamNumber() ) )
		return false;

	for ( int i = 1; i <= MAX_PLAYERS; ++i )
	{
		const WatchClaim_t &w = s_watch[ i ];
		if ( i == me->entindex() || w.flExpire < gpGlobals->curtime || w.iTeam != me->GetTeamNumber() )
			continue;
		CBasePlayer *pOther = UTIL_PlayerByIndex( i );
		if ( !pOther || !pOther->IsAlive() )
			continue;
		if ( ( w.target - target ).LengthSqr() < flRadius * flRadius )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Posições seguradas
//-----------------------------------------------------------------------------

struct HoldClaim_t
{
	CNavArea *pArea;
	int iTeam;
	bool bArrived;
};
static HoldClaim_t s_hold[ MAX_PLAYERS + 1 ];

static bool IsHoldValid( int i )
{
	if ( !s_hold[ i ].pArea )
		return false;
	CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
	if ( !pPlayer || !pPlayer->IsAlive() )
	{
		s_hold[ i ].pArea = NULL;
		return false;
	}
	return true;
}

void TrokBot_ClaimHold( CHL2MPBot *me, CNavArea *area )
{
	int i = me->entindex();
	if ( i <= 0 || i > MAX_PLAYERS )
		return;
	s_hold[ i ].pArea = area;
	s_hold[ i ].iTeam = me->GetTeamNumber();
	s_hold[ i ].bArrived = false;
}

void TrokBot_ReleaseHold( CHL2MPBot *me )
{
	int i = me->entindex();
	if ( i > 0 && i <= MAX_PLAYERS )
		s_hold[ i ].pArea = NULL;
}

void TrokBot_SetHoldArrived( CHL2MPBot *me, bool bArrived )
{
	int i = me->entindex();
	if ( i > 0 && i <= MAX_PLAYERS )
		s_hold[ i ].bArrived = bArrived;
}

bool TrokBot_IsHolding( const CHL2MPBot *me )
{
	int i = me->entindex();
	return i > 0 && i <= MAX_PLAYERS && s_hold[ i ].pArea && s_hold[ i ].bArrived;
}

bool TrokBot_IsHoldTaken( CHL2MPBot *me, const CNavArea *area )
{
	for ( int i = 1; i <= MAX_PLAYERS; ++i )
	{
		if ( i != me->entindex() && IsHoldValid( i ) && s_hold[ i ].pArea == area )
			return true;
	}
	return false;
}

int TrokBot_CountTeamHoldsInRegion( CHL2MPBot *me, int iRegion )
{
	if ( !HasTeammates( me->GetTeamNumber() ) )
		return 0;

	int iCount = 0;
	for ( int i = 1; i <= MAX_PLAYERS; ++i )
	{
		if ( i == me->entindex() || !IsHoldValid( i ) || s_hold[ i ].iTeam != me->GetTeamNumber() )
			continue;
		const TrokTacticalArea_t *pTact = TrokTacticalMap().GetArea( s_hold[ i ].pArea );
		if ( pTact && pTact->iRegion == iRegion )
			++iCount;
	}
	return iCount;
}

//-----------------------------------------------------------------------------
// Andar em grupo: cobrir o ângulo que ninguém cobre
//-----------------------------------------------------------------------------

bool TrokBot_PickUncoveredLook( CHL2MPBot *me, float flRadius, Vector *pLookAt )
{
	if ( !HasTeammates( me->GetTeamNumber() ) )
		return false;

	CUtlVector< float > teammateYaws;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pOther = UTIL_PlayerByIndex( i );
		if ( !pOther || pOther == me || !pOther->IsAlive() || pOther->GetTeamNumber() != me->GetTeamNumber() )
			continue;
		if ( !me->IsRangeLessThan( pOther, flRadius ) )
			continue;
		teammateYaws.AddToTail( pOther->EyeAngles().y );
	}

	if ( teammateYaws.Count() == 0 )
		return false;

	// 8 direções; cada uma "custa" o quanto os colegas já olham para ela.
	// A frente de quem anda conta um pouco a favor (alguém tem que olhar para onde vai).
	Vector velocity = me->GetAbsVelocity();
	float flMoveYaw = velocity.Length2D() > 50.0f ? RAD2DEG( atan2f( velocity.y, velocity.x ) ) : me->EyeAngles().y;

	float flBestYaw = flMoveYaw;
	float flBestCost = FLT_MAX;
	for ( int d = 0; d < 8; ++d )
	{
		float flYaw = flMoveYaw + d * 45.0f;
		float flCost = 0.0f;
		FOR_EACH_VEC( teammateYaws, t )
		{
			float c = cosf( DEG2RAD( AngleDiff( flYaw, teammateYaws[ t ] ) ) );
			if ( c > 0.0f )
				flCost += c * c * c * c;
		}
		// costas (180°) só se o grupo for grande
		if ( d == 4 && teammateYaws.Count() < 2 )
			flCost += 0.8f;
		flCost += RandomFloat( 0.0f, 0.15f );

		if ( flCost < flBestCost )
		{
			flBestCost = flCost;
			flBestYaw = flYaw;
		}
	}

	Vector forward;
	AngleVectors( QAngle( 0, flBestYaw, 0 ), &forward );
	*pLookAt = me->EyePosition() + 500.0f * forward;
	return true;
}

//-----------------------------------------------------------------------------
// Suporte
//-----------------------------------------------------------------------------

static int s_leaderOf[ MAX_PLAYERS + 1 ];	// entindex do líder de cada suporte (0 = nenhum)

void TrokBot_SetLeader( CHL2MPBot *me, CBasePlayer *pLeader )
{
	int i = me->entindex();
	if ( i > 0 && i <= MAX_PLAYERS )
		s_leaderOf[ i ] = pLeader ? pLeader->entindex() : 0;
}

CBasePlayer *TrokBot_PickLeader( CHL2MPBot *me, float flMaxRange )
{
	if ( !HasTeammates( me->GetTeamNumber() ) )
		return NULL;

	CBasePlayer *pBest = NULL;
	float flBest = FLT_MAX;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pOther = UTIL_PlayerByIndex( i );
		if ( !pOther || pOther == me || !pOther->IsAlive() || pOther->GetTeamNumber() != me->GetTeamNumber() )
			continue;

		// quem segura posição não é líder de grupo
		if ( i <= MAX_PLAYERS && IsHoldValid( i ) )
			continue;

		// no máximo 2 suportes por líder; suporte não segue suporte
		int iFollowers = 0;
		for ( int k = 1; k <= MAX_PLAYERS; ++k )
		{
			if ( k != me->entindex() && s_leaderOf[ k ] == i )
			{
				CBasePlayer *pFollower = UTIL_PlayerByIndex( k );
				if ( pFollower && pFollower->IsAlive() )
					++iFollowers;
			}
		}
		if ( iFollowers >= 2 || ( i <= MAX_PLAYERS && s_leaderOf[ i ] != 0 ) )
			continue;

		float flDist = me->GetRangeTo( pOther );
		if ( flDist > flMaxRange )
			continue;

		// humanos são ótimos líderes: o bot acompanha o jogador
		if ( !pOther->IsBot() )
			flDist *= 0.5f;

		if ( flDist < flBest )
		{
			flBest = flDist;
			pBest = pOther;
		}
	}
	return pBest;
}

//-----------------------------------------------------------------------------
// Perigo
//-----------------------------------------------------------------------------

void TrokBot_NoteDeath( CBaseCombatCharacter *victim )
{
	// Vários bots recebem o mesmo evento de morte: só o primeiro marca.
	static float s_flLastMark[ MAX_PLAYERS + 1 ];
	int i = victim->entindex();
	if ( i <= 0 || i > MAX_PLAYERS || s_flLastMark[ i ] == gpGlobals->curtime )
		return;
	s_flLastMark[ i ] = gpGlobals->curtime;

	CNavArea *area = victim->GetLastKnownArea();
	if ( area )
		area->IncreaseDanger( victim->GetTeamNumber(), 1.0f );
}

//-----------------------------------------------------------------------------
// Custo de caminho com variedade
//-----------------------------------------------------------------------------

CTrokBotPathCost::CTrokBotPathCost( CHL2MPBot *me, unsigned int nRouteSeed )
{
	m_me = me;
	m_nSeed = nRouteSeed;
	// Cauteloso (pouco agressivo) evita mais onde gente morreu; o estilo
	// muda o quanto o bot tem "rotas próprias" e o quanto foge da rota do time.
	const TrokBotProfile_t &profile = TrokBot_GetProfile( me );
	m_flVariety = trok_bot_rota_variedade.GetFloat() * profile.flRotaVariedade;
	m_flTeamPenalty = trok_bot_rota_colegas.GetFloat() * profile.flRotaColegas;
	m_flCaution = 1.0f - profile.flAgressividade;

	m_stepHeight = me->GetLocomotionInterface()->GetStepHeight();
	m_maxJumpHeight = me->GetLocomotionInterface()->GetMaxJumpHeight();
	m_maxDropHeight = me->GetLocomotionInterface()->GetDeathDropHeight();
}

float CTrokBotPathCost::operator()( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder, const CFuncElevator *elevator, float length ) const
{
	if ( !fromArea )
		return 0.0f;

	if ( !m_me->GetLocomotionInterface()->IsAreaTraversable( area ) )
		return -1.0f;

	float dist;
	if ( ladder )
		dist = ladder->m_length;
	else if ( length > 0.0f )
		dist = length;
	else
		dist = ( area->GetCenter() - fromArea->GetCenter() ).Length();

	// Altura: pulo custa o dobro; alto demais ou queda mortal, não passa.
	float deltaZ = fromArea->ComputeAdjacentConnectionHeightChange( area );
	if ( deltaZ >= m_stepHeight )
	{
		if ( deltaZ >= m_maxJumpHeight )
			return -1.0f;
		dist *= 2.0f;
	}
	else if ( deltaZ < -m_maxDropHeight )
	{
		return -1.0f;
	}

	float flMult = 1.0f;

	// Preferência de rota do bot: por célula de 512u (corredores inteiros,
	// não área por área), sorteada pela semente da caçada atual.
	if ( m_flVariety > 0.0f )
	{
		const Vector &c = area->GetCenter();
		unsigned int cx = (unsigned int)(int)floorf( c.x / ROUTE_CELL );
		unsigned int cy = (unsigned int)(int)floorf( c.y / ROUTE_CELL );
		unsigned int h = m_nSeed ^ ( cx * 73856093u ) ^ ( cy * 19349663u );
		h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
		float r = ( h & 0xFFFF ) / 65535.0f;
		flMult *= 1.0f + m_flVariety * r;
	}

	// Rota que um colega acabou de usar: prefere outro corredor.
	if ( TrokBot_IsAreaUsedByTeam( m_me, area ) )
		flMult *= 1.0f + m_flTeamPenalty;

	// Gente morreu aqui há pouco.
	float flDanger = MIN( area->GetDanger( m_me->GetTeamNumber() ), 3.0f );
	if ( flDanger > 0.0f )
		flMult *= 1.0f + 0.8f * m_flCaution * flDanger;

	float cost = dist * flMult;

	if ( area->HasAttributes( NAV_MESH_FUNC_COST ) )
		cost *= area->ComputeFuncNavCost( m_me );

	return cost + fromArea->GetCostSoFar();
}

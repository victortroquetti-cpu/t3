//=============================================================================
// Trok: série de abates como no Combat Arms (servidor). Ver trok_killstreak.h.
// O cliente (faixa, linha lateral e voz) está em client/trok/hud_trok_killstreak.cpp.
//=============================================================================

#include "cbase.h"
#include "trok_killstreak.h"
#include "trok_placar_extra.h"

#ifndef CLIENT_DLL
	#include "igameevents.h"
	#include "gamerules.h"
	#include "trok/trok_economia.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar trok_serie_voz( "trok_serie_voz", "1", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Voz do anunciador da série de abates (sons provisórios do HL2/CS:S, em scripts/trok_anunciador.txt). 0 = só a faixa." );

#ifndef CLIENT_DLL

ConVar trok_serie( "trok_serie", "1", FCVAR_NOTIFY, "Série de abates como no CA (contagem e anúncios). 0 = desliga." );
ConVar trok_serie_janela( "trok_serie_janela", "5", FCVAR_NOTIFY,
	"Segundos: um abate a MENOS que isso do anterior sobe o multi-abate (CA: 5)." );
ConVar trok_serie_vinganca( "trok_serie_vinganca", "2", FCVAR_NOTIFY,
	"Vingança: abater quem te abateu esta quantidade de vezes sem resposta (0 = desliga)." );
ConVar trok_serie_primeiro( "trok_serie_primeiro", "2", FCVAR_NOTIFY,
	"Primeiro abate: 0 = desliga, 1 = só o primeiro do mapa, 2 = o primeiro de cada rodada nos modos de rodada (no resto, do mapa)." );
ConVar trok_serie_sorte( "trok_serie_sorte", "0", FCVAR_NOTIFY,
	"Chance (0 a 1) de um abate sair como 'tiro de sorte' (no CA vem do lobby, valor desconhecido). 0 = desliga." );

struct TrokSerieJogador_t
{
	int iUserId;
	float flUltimo;			// curtime do último abate de inimigo (< 0 = nenhum)
	int iMulti;				// multi-abate atual (abates a menos de trok_serie_janela um do outro)
	int iAbates;			// abates de inimigo no mapa (número do "N ABATES")
	int iPassos[ 8 ];		// estatística: quantas vezes chegou a cada passo (2 a 7+)
	int iPrimeiros;
	int iCabeca;
};

static TrokSerieJogador_t s_Jog[ MAX_PLAYERS + 1 ];
// s_iSemResposta[a][b]: quantas vezes a abateu b desde a última vez que b abateu a.
static int s_iSemResposta[ MAX_PLAYERS + 1 ][ MAX_PLAYERS + 1 ];
static bool s_bPrimeiroFeito = false;

static void ZerarSlot( int i )
{
	V_memset( &s_Jog[ i ], 0, sizeof( s_Jog[ i ] ) );
	s_Jog[ i ].flUltimo = -1.0f;
	for ( int j = 0; j <= MAX_PLAYERS; ++j )
	{
		s_iSemResposta[ i ][ j ] = 0;
		s_iSemResposta[ j ][ i ] = 0;
	}
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	if ( pPlacar )
	{
		pPlacar->m_aSerie.Set( i, 0 );
		pPlacar->m_aMelhorSerie.Set( i, 0 );
	}
}

static void ZerarTudo( void )
{
	for ( int i = 0; i <= MAX_PLAYERS; ++i )
	{
		V_memset( &s_Jog[ i ], 0, sizeof( s_Jog[ i ] ) );
		s_Jog[ i ].flUltimo = -1.0f;
	}
	V_memset( s_iSemResposta, 0, sizeof( s_iSemResposta ) );
	s_bPrimeiroFeito = false;
}

// Índice do jogador; se outra pessoa ocupou o lugar, começa do zero.
static int Slot( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return 0;
	const int i = pPlayer->entindex();
	if ( !CTrokPlacarExtra::Valido( i ) )
		return 0;
	if ( s_Jog[ i ].iUserId != pPlayer->GetUserID() )
	{
		ZerarSlot( i );
		s_Jog[ i ].iUserId = pPlayer->GetUserID();
	}
	return i;
}

class CTrokSerieSistema : public CAutoGameSystem
{
public:
	CTrokSerieSistema() : CAutoGameSystem( "CTrokSerieSistema" ) {}
	virtual void LevelInitPreEntity( void ) { ZerarTudo(); }
};
static CTrokSerieSistema s_TrokSerieSistema;

void TrokSerie_PlayerSpawn( CBasePlayer *pPlayer )
{
	Slot( pPlayer );
	TrokEconomia_SerieNovaVida( pPlayer );	// teto de bônus de série por vida
}

void TrokSerie_RodadaNova( void )
{
	if ( trok_serie_primeiro.GetInt() == 2 )
		s_bPrimeiroFeito = false;
}

// Só dano de bala e corpo a corpo vale cabeça (CA: explosão, dano contínuo e arremesso descem
// para tronco). O último grupo atingido fica no jogador mesmo depois de dano sem traço, por
// isso o tipo de dano decide.
static bool AbateNaCabeca( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	if ( pVictim->LastHitGroup() != HITGROUP_HEAD )
		return false;
	const int iTipo = info.GetDamageType();
	if ( iTipo & ( DMG_BLAST | DMG_BURN | DMG_POISON | DMG_ACID | DMG_RADIATION | DMG_FALL | DMG_CRUSH ) )
		return false;
	return ( iTipo & ( DMG_BULLET | DMG_CLUB | DMG_SLASH ) ) != 0;
}

void TrokSerie_PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	if ( !pVictim )
		return;
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	const int v = Slot( pVictim );

	// Morreu: acaba a série desta vida (o multi-abate é por tempo e não zera aqui; CA, pergunta 2).
	if ( pPlacar && v )
		pPlacar->m_aSerie.Set( v, 0 );

	if ( !trok_serie.GetBool() )
		return;

	CBasePlayer *pKiller = ToBasePlayer( info.GetAttacker() );
	if ( !pKiller || pKiller == pVictim )
		return;
	const int a = Slot( pKiller );
	if ( !a || !v )
		return;
	// Abate de aliado nem sobe nem zera nada.
	if ( g_pGameRules && g_pGameRules->PlayerRelationship( pKiller, pVictim ) == GR_TEAMMATE )
		return;

	TrokSerieJogador_t &k = s_Jog[ a ];
	const float flNow = gpGlobals->curtime;
	const float flJanela = MAX( 0.0f, trok_serie_janela.GetFloat() );
	k.iMulti = ( k.flUltimo >= 0.0f && flNow - k.flUltimo < flJanela ) ? k.iMulti + 1 : 1;
	k.flUltimo = flNow;
	k.iAbates++;
	if ( k.iMulti >= 2 )
		k.iPassos[ MIN( k.iMulti, 7 ) ]++;

	// Série desta vida (sem limite de tempo) e a maior do mapa.
	int iSerie = 0;
	if ( pPlacar )
	{
		iSerie = pPlacar->m_aSerie[ a ] + 1;
		pPlacar->m_aSerie.Set( a, iSerie );
		if ( iSerie > pPlacar->m_aMelhorSerie[ a ] )
			pPlacar->m_aMelhorSerie.Set( a, iSerie );
	}

	const bool bCabeca = AbateNaCabeca( pVictim, info );
	if ( bCabeca )
		k.iCabeca++;

	// Vingança: a vítima tinha abatido o matador N vezes sem resposta.
	const int iN = trok_serie_vinganca.GetInt();
	const bool bVinganca = iN > 0 && s_iSemResposta[ v ][ a ] >= iN;
	s_iSemResposta[ v ][ a ] = 0;
	s_iSemResposta[ a ][ v ] = MIN( s_iSemResposta[ a ][ v ] + 1, 1000 );

	bool bPrimeiro = false;
	if ( trok_serie_primeiro.GetInt() > 0 && !s_bPrimeiroFeito )
	{
		s_bPrimeiroFeito = true;
		bPrimeiro = true;
		k.iPrimeiros++;
	}

	const float flSorte = trok_serie_sorte.GetFloat();
	const bool bSorte = flSorte > 0.0f && RandomFloat( 0.0f, 1.0f ) < flSorte;

	// Bônus em dinheiro (humano) ou Fúria (zumbi), equilibrado e com teto por vida (trok_economia.cpp).
	TrokEconomia_BonusSerie( pKiller, k.iMulti, iSerie, bVinganca, bPrimeiro );

	IGameEvent *pEvent = gameeventmanager->CreateEvent( "trok_serie" );
	if ( pEvent )
	{
		pEvent->SetInt( "atacante", pKiller->GetUserID() );
		pEvent->SetInt( "vitima", pVictim->GetUserID() );
		pEvent->SetInt( "multi", MIN( k.iMulti, 255 ) );
		pEvent->SetInt( "serie", iSerie );
		pEvent->SetInt( "abates", k.iAbates );
		pEvent->SetBool( "cabeca", bCabeca );
		pEvent->SetBool( "primeiro", bPrimeiro );
		pEvent->SetBool( "vinganca", bVinganca );
		pEvent->SetBool( "sorte", bSorte );
		gameeventmanager->FireEvent( pEvent );
	}
}

// Estatísticas do mapa (no CA vão para a conta no fim da partida; aqui só no console).
CON_COMMAND( trok_serie_info, "Mostra no console a série de abates de cada jogador neste mapa." )
{
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	Msg( "%-24s %6s %6s %6s %5s %5s %5s %5s %5s %5s %6s %6s\n", "jogador", "abates", "serie", "melhor",
		"duplo", "multi", "ultra", "fant", "inacr", "inac+", "1o", "cabeca" );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer || !pPlayer->IsConnected() || s_Jog[ i ].iUserId != pPlayer->GetUserID() )
			continue;
		const TrokSerieJogador_t &k = s_Jog[ i ];
		Msg( "%-24s %6d %6d %6d %5d %5d %5d %5d %5d %5d %6d %6d\n", pPlayer->GetPlayerName(), k.iAbates,
			pPlacar ? pPlacar->GetSerie( i ) : 0, pPlacar ? pPlacar->GetMelhorSerie( i ) : 0,
			k.iPassos[ 2 ], k.iPassos[ 3 ], k.iPassos[ 4 ], k.iPassos[ 5 ], k.iPassos[ 6 ], k.iPassos[ 7 ],
			k.iPrimeiros, k.iCabeca );
	}
}

#endif // !CLIENT_DLL

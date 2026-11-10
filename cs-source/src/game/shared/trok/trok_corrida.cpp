//=============================================================================
// Trok: modo Corrida Armada (o "Arms Race" do Combat Arms). Ver trok_corrida.h.
//
// Regras do CA (design/chimera/specs/combat_arms/game_modes.md M14, M5, S5-S7, S10, e
// missions.md F4.8), como ficaram aqui:
// - todos contra todos, uma rodada, sem tempo; renasce sozinho; vence o primeiro a chegar
//   em G abates de inimigo (G = mp_fraglimit; o CA oferece 15, 30 e 45, padrão 15);
// - escada por meta: degrau i começa em Kill<i> abates; quando os abates do jogador ficam
//   IGUAIS ao início de um degrau, a arma dele é sorteada para esse degrau; ao nascer,
//   também (com os abates que ele já tem). Nada rebaixa: faca não tira degrau de ninguém,
//   suicídio e queda só somam morte (não tiram abate);
// - sorteio do CA: r inteiro em 0..100; r < trok_corrida_chance (80) = arma da escada
//   (sorteio por peso entre as do degrau); senão = a arma própria do jogador da mesma classe
//   (aqui: a de trok_armas_iniciais no mesmo bucket); sem arma própria, a da escada;
// - a última arma da escada do CA é a granada de mão (G=15: no 14º abate); o abate que leva a
//   G vence. Chegar no último degrau avisa todo mundo ("última arma");
// - nenhuma arma cai ao morrer; a proteção ao nascer acaba ao atirar (como no solo);
// - saída: se sobram 2 ou menos jogadores (contando quem saiu), a partida acaba; quem ficou
//   vence se os abates da partida (de todos, até de quem saiu) passam de 10% de G.
//=============================================================================

#include "cbase.h"
#include "trok_corrida.h"
#include "filesystem.h"
#include "KeyValues.h"

#ifndef CLIENT_DLL
	#include "trok_gamerules.h"
	#include "ammodef.h"
	#include "weapon_parse.h"
	#include "weapon_hl2mpbasehlmpcombatweapon.h"
	#include "bot/hl2mp_bot.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define TROK_CORRIDA_ARQUIVO "scripts/trok_corrida.txt"

//-----------------------------------------------------------------------------
// Escada (dos dois lados)
//-----------------------------------------------------------------------------
static TrokCorridaEscada_t s_Escada;
static int s_iEscadaPedida = -1;

static bool LerEscada( KeyValues *pKV, TrokCorridaEscada_t &e )
{
	V_memset( &e, 0, sizeof( e ) );
	e.iMeta = pKV->GetInt( "meta", 0 );
	if ( e.iMeta <= 0 )
		return false;
	int iAnterior = -1;
	for ( KeyValues *pD = pKV->GetFirstTrueSubKey(); pD && e.nDegraus < TROK_CORRIDA_MAX_DEGRAUS; pD = pD->GetNextTrueSubKey() )
	{
		if ( V_stricmp( pD->GetName(), "degrau" ) )
			continue;
		TrokCorridaDegrau_t &d = e.degraus[ e.nDegraus ];
		d.iAbates = pD->GetInt( "abates", -1 );
		// CA: os degraus acabam no primeiro inválido; aqui também exigimos ordem crescente.
		if ( d.iAbates < 0 || d.iAbates <= iAnterior || ( e.nDegraus == 0 && d.iAbates != 0 ) )
		{
			Warning( "%s: escada da meta %d: degrau %d com \"abates\" inválido (o primeiro é 0, depois crescente).\n", TROK_CORRIDA_ARQUIVO, e.iMeta, e.nDegraus + 1 );
			break;
		}
		V_strncpy( d.szNome, pD->GetString( "nome", "" ), sizeof( d.szNome ) );
		d.iMunicao = pD->GetInt( "municao", -1 );
		KeyValues *pArmas = pD->FindKey( "armas" );
		for ( KeyValues *pA = pArmas ? pArmas->GetFirstValue() : NULL; pA && d.nArmas < TROK_CORRIDA_MAX_ARMAS; pA = pA->GetNextValue() )
		{
			const int iPeso = pA->GetInt();
			if ( iPeso <= 0 )
				continue; // CA: peso 0 tira a arma do sorteio
			V_strncpy( d.armas[ d.nArmas ].szClasse, pA->GetName(), sizeof( d.armas[ 0 ].szClasse ) );
			d.armas[ d.nArmas ].iPeso = iPeso;
			++d.nArmas;
		}
		if ( d.nArmas == 0 )
		{
			Warning( "%s: escada da meta %d: degrau %d sem arma.\n", TROK_CORRIDA_ARQUIVO, e.iMeta, e.nDegraus + 1 );
			break;
		}
		if ( !d.szNome[ 0 ] )
		{
			// Sem nome: o da classe sem o prefixo ("weapon_trok_fuzil" -> "FUZIL").
			const char *psz = d.armas[ 0 ].szClasse;
			if ( !V_strnicmp( psz, "weapon_trok_", 12 ) )
				psz += 12;
			else if ( !V_strnicmp( psz, "weapon_", 7 ) )
				psz += 7;
			V_strncpy( d.szNome, psz, sizeof( d.szNome ) );
			V_strupr( d.szNome );
		}
		iAnterior = d.iAbates;
		++e.nDegraus;
	}
	return e.nDegraus > 0;
}

const TrokCorridaEscada_t *TrokCorrida_Escada( int iMeta, bool bRecarregar )
{
	if ( !bRecarregar && iMeta == s_iEscadaPedida )
		return s_Escada.nDegraus > 0 ? &s_Escada : NULL;
	s_iEscadaPedida = iMeta;
	V_memset( &s_Escada, 0, sizeof( s_Escada ) );

	KeyValues *pRoot = new KeyValues( "TrokCorrida" );
	if ( !pRoot->LoadFromFile( filesystem, TROK_CORRIDA_ARQUIVO, "MOD" ) )
	{
		Warning( "Corrida Armada: não achei %s.\n", TROK_CORRIDA_ARQUIVO );
		pRoot->deleteThis();
		return NULL;
	}

	// A escada de meta igual; senão a de meta mais perto (empate: a maior).
	TrokCorridaEscada_t e;
	int iMelhorDif = INT_MAX;
	for ( KeyValues *pE = pRoot->GetFirstTrueSubKey(); pE; pE = pE->GetNextTrueSubKey() )
	{
		if ( V_stricmp( pE->GetName(), "escada" ) || !LerEscada( pE, e ) )
			continue;
		const int iDif = abs( e.iMeta - iMeta );
		if ( iDif < iMelhorDif || ( iDif == iMelhorDif && e.iMeta > s_Escada.iMeta ) )
		{
			iMelhorDif = iDif;
			s_Escada = e;
		}
	}
	pRoot->deleteThis();
	return s_Escada.nDegraus > 0 ? &s_Escada : NULL;
}

int TrokCorrida_DegrauDosAbates( const TrokCorridaEscada_t *pEscada, int iAbates )
{
	if ( !pEscada )
		return 0;
	int s = 0;
	for ( int k = 0; k < pEscada->nDegraus; ++k )
	{
		if ( pEscada->degraus[ k ].iAbates <= iAbates )
			s = k;
	}
	return s;
}

//-----------------------------------------------------------------------------
// Estado em rede
//-----------------------------------------------------------------------------
static CHandle< CTrokCorridaEstado > s_hEstado;

IMPLEMENT_NETWORKCLASS_ALIASED( TrokCorridaEstado, DT_TrokCorridaEstado )

BEGIN_NETWORK_TABLE_NOBASE( CTrokCorridaEstado, DT_TrokCorridaEstado )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_iMeta ) ),
	RecvPropInt( RECVINFO( m_iDegraus ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aDegrau ), RecvPropInt( RECVINFO( m_aDegrau[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aAbates ), RecvPropInt( RECVINFO( m_aAbates[ 0 ] ) ) ),
	RecvPropInt( RECVINFO( m_iUltimo ) ),
	RecvPropFloat( RECVINFO( m_flUltimoHora ) ),
	RecvPropInt( RECVINFO( m_iVencedor ) ),
	RecvPropBool( RECVINFO( m_bFim ) ),
#else
	SendPropInt( SENDINFO( m_iMeta ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDegraus ), 6, SPROP_UNSIGNED ),
	SendPropArray3( SENDINFO_ARRAY3( m_aDegrau ), SendPropInt( SENDINFO_ARRAY( m_aDegrau ), 6, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aAbates ), SendPropInt( SENDINFO_ARRAY( m_aAbates ), 10, SPROP_UNSIGNED ) ),
	SendPropInt( SENDINFO( m_iUltimo ), 8, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flUltimoHora ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_iVencedor ), 8, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bFim ) ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( trok_corrida_estado, CTrokCorridaEstado );

CTrokCorridaEstado::CTrokCorridaEstado()
{
	m_iMeta = 0;
	m_iDegraus = 0;
	for ( int i = 0; i <= MAX_PLAYERS; ++i )
	{
		m_aDegrau.Set( i, 0 );
		m_aAbates.Set( i, 0 );
	}
	m_iUltimo = 0;
	m_flUltimoHora = 0.0f;
	m_iVencedor = 0;
	m_bFim = false;
	s_hEstado = this;
}

CTrokCorridaEstado::~CTrokCorridaEstado()
{
	if ( s_hEstado.Get() == this )
		s_hEstado = NULL;
}

CTrokCorridaEstado *TrokCorrida_Estado( void )
{
	return s_hEstado.Get();
}

bool TrokCorrida_Ativo( void )
{
	return s_hEstado.Get() != NULL;
}

#ifndef CLIENT_DLL
//=============================================================================
// Servidor
//=============================================================================
ConVar trok_corrida_chance( "trok_corrida_chance", "80", FCVAR_NOTIFY,
	"Corrida Armada: sorteio de 0 a 100 ao trocar de arma; abaixo disto vem a arma da escada, senão a sua da mesma classe (CA: 80, ou seja 80/101). 101 = sempre a da escada." );
ConVar trok_corrida_extras( "trok_corrida_extras", "weapon_trok_faca", FCVAR_NOTIFY,
	"Corrida Armada: armas que o jogador tem junto com a da escada (separadas por espaço; vazio = só a da escada)." );
ConVar trok_corrida_saida_jogadores( "trok_corrida_saida_jogadores", "2", FCVAR_NOTIFY,
	"Corrida Armada: alguém sai e sobram (contando quem saiu) estes jogadores ou menos: a partida acaba (CA: 2)." );
ConVar trok_corrida_saida_fracao( "trok_corrida_saida_fracao", "0.1", FCVAR_NOTIFY,
	"Corrida Armada: na saída, quem ficou vence se os abates da partida passam desta fração da meta, arredondada (CA: 0.1)." );

static ConVarRef s_fraglimit( "mp_fraglimit" );
static ConVarRef s_armasIniciais( "trok_armas_iniciais" );
static ConVarRef s_municaoInicial( "trok_municao_inicial" );

static bool s_bAtivo = false;
static int s_iUserId[ MAX_PLAYERS + 1 ];
static int s_iTrocar[ MAX_PLAYERS + 1 ];					// degrau a dar no próximo frame (-1 = nada)
static bool s_bBotArma[ MAX_PLAYERS + 1 ];				// falta mandar o bot usar a arma da escada
static CHandle< CBaseCombatWeapon > s_hArma[ MAX_PLAYERS + 1 ];
static int s_iAbatesPartida = 0;							// todos os abates, até de quem saiu (regra da saída)
static int s_iJogadoresAntes = 0;
static bool s_bPedirFim = false;
static int s_iMetaAplicada = -1;

static void ZerarJogador( int i )
{
	s_iUserId[ i ] = 0;
	s_iTrocar[ i ] = -1;
	s_bBotArma[ i ] = false;
	s_hArma[ i ] = NULL;
	CTrokCorridaEstado *pEstado = s_hEstado.Get();
	if ( pEstado )
	{
		pEstado->m_aDegrau.Set( i, 0 );
		pEstado->m_aAbates.Set( i, 0 );
	}
}

void TrokCorrida_Iniciar( bool bAtivo )
{
	s_bAtivo = bAtivo;
	for ( int i = 0; i <= MAX_PLAYERS; ++i )
		ZerarJogador( i );
	s_iAbatesPartida = 0;
	s_iJogadoresAntes = 0;
	s_bPedirFim = false;
	s_iMetaAplicada = -1;
}

static const TrokCorridaEscada_t *EscadaAtual( void )
{
	const int iMeta = s_fraglimit.IsValid() && s_fraglimit.GetInt() > 0 ? s_fraglimit.GetInt() : 15;
	return TrokCorrida_Escada( iMeta );
}

// Cria o estado se falta e confere a meta: sem escada para a meta pedida, a meta vira a da
// escada mais perto (o CA só tem 15, 30 e 45).
static CTrokCorridaEstado *GarantirEstado( void )
{
	CTrokCorridaEstado *pEstado = s_hEstado.Get();
	if ( !pEstado )
	{
		pEstado = static_cast< CTrokCorridaEstado * >( CreateEntityByName( "trok_corrida_estado" ) );
		if ( !pEstado )
			return NULL;
		DispatchSpawn( pEstado );
	}

	const TrokCorridaEscada_t *pEscada = EscadaAtual();
	if ( pEscada && s_fraglimit.IsValid() && s_fraglimit.GetInt() != pEscada->iMeta )
	{
		Msg( "Corrida Armada: não há escada para a meta %d em %s; a meta fica %d.\n", s_fraglimit.GetInt(), TROK_CORRIDA_ARQUIVO, pEscada->iMeta );
		s_fraglimit.SetValue( pEscada->iMeta );
	}
	const int iMeta = pEscada ? pEscada->iMeta : 0;
	if ( iMeta != s_iMetaAplicada )
	{
		s_iMetaAplicada = iMeta;
		pEstado->m_iMeta = iMeta;
		pEstado->m_iDegraus = pEscada ? pEscada->nDegraus : 0;
		// Meta trocada no meio (rcon): o degrau de cada um segue os abates na escada nova.
		for ( int i = 1; i <= MAX_PLAYERS; ++i )
			pEstado->m_aDegrau.Set( i, TrokCorrida_DegrauDosAbates( pEscada, pEstado->m_aAbates[ i ] ) );
	}
	return pEstado;
}

// Arma do jogador da mesma classe (bucket) que a da escada, do equipamento inicial.
static const char *ArmaPropria( const char *pszEscada, char *pszOut, int nOut )
{
	WEAPON_FILE_INFO_HANDLE h = LookupWeaponInfoSlot( pszEscada );
	if ( h == GetInvalidWeaponInfoHandle() || !s_armasIniciais.IsValid() )
		return NULL;
	const int iSlot = GetFileWeaponInfoFromHandle( h )->iSlot;

	CUtlVector< CUtlString > armas;
	V_SplitString( s_armasIniciais.GetString(), " ", armas );
	FOR_EACH_VEC( armas, k )
	{
		WEAPON_FILE_INFO_HANDLE hk = LookupWeaponInfoSlot( armas[ k ] );
		if ( hk != GetInvalidWeaponInfoHandle() && GetFileWeaponInfoFromHandle( hk )->iSlot == iSlot )
		{
			V_strncpy( pszOut, armas[ k ], nOut );
			return pszOut;
		}
	}
	return NULL;
}

// Sorteio por peso do CA: q inteiro em 0..W (W = soma dos pesos), a primeira cuja soma
// corrida chega em q.
static int SortearArma( const TrokCorridaDegrau_t &d )
{
	int iTotal = 0;
	for ( int k = 0; k < d.nArmas; ++k )
		iTotal += d.armas[ k ].iPeso;
	const int q = RandomInt( 0, iTotal );
	int iSoma = 0;
	for ( int k = 0; k < d.nArmas; ++k )
	{
		iSoma += d.armas[ k ].iPeso;
		if ( iSoma >= q )
			return k;
	}
	return d.nArmas - 1;
}

// Dá a arma direto (sem passar pelas regras de pegar do chão, que a Corrida bloqueia).
static CBaseCombatWeapon *DarArma( CBasePlayer *pPlayer, const char *pszClasse )
{
	if ( !pszClasse || !pszClasse[ 0 ] )
		return NULL;
	CBaseCombatWeapon *pJa = pPlayer->Weapon_OwnsThisType( pszClasse );
	if ( pJa )
		return pJa;
	CBaseEntity *pEnt = CreateEntityByName( pszClasse );
	CBaseCombatWeapon *pArma = dynamic_cast< CBaseCombatWeapon * >( pEnt );
	if ( !pArma )
	{
		if ( pEnt )
			UTIL_Remove( pEnt );
		DevWarning( "Corrida Armada: arma \"%s\" não existe.\n", pszClasse );
		return NULL;
	}
	pArma->SetLocalOrigin( pPlayer->GetLocalOrigin() );
	pArma->AddSpawnFlags( SF_NORESPAWN );
	DispatchSpawn( pArma );
	if ( pArma->IsMarkedForDeletion() )
		return NULL;
	pArma->AddSolidFlags( FSOLID_NOT_SOLID );
	pArma->AddEffects( EF_NODRAW );
	pPlayer->Weapon_Equip( pArma );
	return pArma;
}

// Reserva da munição da arma: a do degrau ou a de trok_municao_inicial para esse tipo.
static void DarMunicao( CBasePlayer *pPlayer, CBaseCombatWeapon *pArma, int iMunicao )
{
	const int iTipo = pArma->GetPrimaryAmmoType();
	if ( iTipo < 0 )
		return;
	if ( iMunicao < 0 && s_municaoInicial.IsValid() )
	{
		const Ammo_t *pAmmo = GetAmmoDef()->GetAmmoOfIndex( iTipo );
		CUtlVector< CUtlString > lista;
		V_SplitString( s_municaoInicial.GetString(), " ", lista );
		for ( int k = 0; pAmmo && k + 1 < lista.Count(); k += 2 )
		{
			if ( !V_stricmp( lista[ k ], pAmmo->pName ) )
			{
				iMunicao = atoi( lista[ k + 1 ] );
				break;
			}
		}
	}
	if ( iMunicao > 0 )
		pPlayer->GiveAmmo( iMunicao, iTipo, true );
}

// Tira tudo e dá a arma do degrau (sorteada como no CA) e as extras (faca).
static void DarArmaDoDegrau( CBasePlayer *pPlayer, int iDegrau )
{
	const TrokCorridaEscada_t *pEscada = EscadaAtual();
	const int i = pPlayer->entindex();
	if ( !pEscada || i <= 0 || i > MAX_PLAYERS )
		return;
	iDegrau = clamp( iDegrau, 0, pEscada->nDegraus - 1 );
	const TrokCorridaDegrau_t &d = pEscada->degraus[ iDegrau ];

	const int k = SortearArma( d );
	const char *pszClasse = d.armas[ k ].szClasse;
	char szPropria[ 64 ];
	if ( RandomInt( 0, 100 ) >= trok_corrida_chance.GetInt() )
	{
		const char *pszPropria = ArmaPropria( pszClasse, szPropria, sizeof( szPropria ) );
		if ( pszPropria )
			pszClasse = pszPropria;
	}

	pPlayer->RemoveAllItems( false );

	// A da escada primeiro (fica no primeiro lugar da lista de armas, que é a que o bot do
	// HL2MP usa quando não reconhece nenhuma); se não existir, as outras do degrau.
	CBaseCombatWeapon *pArma = DarArma( pPlayer, pszClasse );
	for ( int n = 0; !pArma && n < d.nArmas; ++n )
		pArma = DarArma( pPlayer, d.armas[ n ].szClasse );
	if ( pArma )
		DarMunicao( pPlayer, pArma, d.iMunicao );

	CBaseCombatWeapon *pPrimeiraExtra = NULL;
	CUtlVector< CUtlString > extras;
	V_SplitString( trok_corrida_extras.GetString(), " ", extras );
	FOR_EACH_VEC( extras, n )
	{
		CBaseCombatWeapon *pExtra = DarArma( pPlayer, extras[ n ] );
		if ( pExtra && !pPrimeiraExtra )
			pPrimeiraExtra = pExtra;
	}

	CBaseCombatWeapon *pSacar = pArma ? pArma : pPrimeiraExtra;
	if ( pSacar )
		pPlayer->Weapon_Switch( pSacar );

	s_hArma[ i ] = pArma;
	s_bBotArma[ i ] = pArma && pPlayer->IsBot();
	s_iTrocar[ i ] = -1;

	CTrokCorridaEstado *pEstado = s_hEstado.Get();
	if ( pEstado )
		pEstado->m_aDegrau.Set( i, iDegrau );
}

bool TrokCorrida_DarArmas( CBasePlayer *pPlayer )
{
	if ( !s_bAtivo || !pPlayer )
		return false;
	CTrokCorridaEstado *pEstado = GarantirEstado();
	const int i = pPlayer->entindex();
	if ( !pEstado || i <= 0 || i > MAX_PLAYERS )
		return false;
	if ( s_iUserId[ i ] != pPlayer->GetUserID() )
	{
		ZerarJogador( i );
		s_iUserId[ i ] = pPlayer->GetUserID();
	}
	// Ao nascer, a arma do degrau em que ele está (CA: o sorteio também vale ao nascer).
	DarArmaDoDegrau( pPlayer, TrokCorrida_DegrauDosAbates( EscadaAtual(), pEstado->m_aAbates[ i ] ) );
	return true;
}

void TrokCorrida_AntesDeMorrer( CBasePlayer *pPlayer )
{
	// CA: na Corrida nada cai ao morrer. Sem arma, nem o inventário nem o motor largam nada.
	if ( s_bAtivo && pPlayer )
		pPlayer->RemoveAllItems( false );
}

bool TrokCorrida_PodeTerArma( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem )
{
	// Só a Corrida dá arma (DarArma não passa por aqui); nada se pega do chão.
	return !s_bAtivo;
}

void TrokCorrida_Abate( CBasePlayer *pVictim, const CTakeDamageInfo &info, int iFragsAntes )
{
	if ( !s_bAtivo || !pVictim || g_fGameOver )
		return;
	CTrokCorridaEstado *pEstado = GarantirEstado();
	if ( !pEstado || pEstado->m_bFim )
		return;

	// CA (exército solo e Corrida): suicídio e queda somam uma morte e não tiram abate.
	if ( pVictim->FragCount() < iFragsAntes )
		pVictim->IncrementFragCount( iFragsAntes - pVictim->FragCount() );

	CBasePlayer *pKiller = ToBasePlayer( info.GetAttacker() );
	if ( !pKiller || pKiller == pVictim )
		return;
	const int a = pKiller->entindex();
	if ( a <= 0 || a > MAX_PLAYERS )
		return;
	if ( s_iUserId[ a ] != pKiller->GetUserID() )
	{
		ZerarJogador( a );
		s_iUserId[ a ] = pKiller->GetUserID();
	}

	const int iAbates = pEstado->m_aAbates[ a ] + 1;
	pEstado->m_aAbates.Set( a, iAbates );
	++s_iAbatesPartida;

	// Chegou exatamente no começo de um degrau: troca a arma (no próximo frame, fora do
	// código da arma que matou). Morto (granada depois de morrer), troca ao nascer.
	const TrokCorridaEscada_t *pEscada = EscadaAtual();
	for ( int s = 0; pEscada && s < pEscada->nDegraus; ++s )
	{
		if ( pEscada->degraus[ s ].iAbates != iAbates )
			continue;
		pEstado->m_aDegrau.Set( a, s );
		if ( pKiller->IsAlive() )
			s_iTrocar[ a ] = s;
		if ( s == pEscada->nDegraus - 1 )
		{
			// CA: chegar no último degrau avisa todo mundo.
			pEstado->m_iUltimo = a;
			pEstado->m_flUltimoHora = gpGlobals->curtime;
		}
		break;
	}

	// Vitória: o abate que leva à meta (CA: cada abate que chega na meta escolhe o vencedor;
	// dois no mesmo frame, vale o último).
	if ( pEstado->m_iMeta > 0 && iAbates >= pEstado->m_iMeta )
	{
		pEstado->m_iVencedor = a;
		s_bPedirFim = true;
	}
}

void TrokCorrida_Think( void )
{
	if ( !s_bAtivo )
		return;
	CTrokCorridaEstado *pEstado = GarantirEstado();
	if ( !pEstado )
		return;

	if ( s_bPedirFim && !pEstado->m_bFim )
	{
		pEstado->m_bFim = true;
		if ( !g_fGameOver && TrokRules() )
			TrokRules()->GoToIntermission();
		return;
	}
	if ( pEstado->m_bFim )
		return;

	int iJogadores = 0;
	int iUltimoVivo = 0;
	for ( int i = 1; i <= MIN( gpGlobals->maxClients, MAX_PLAYERS ); ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer || !pPlayer->IsConnected() )
		{
			if ( s_iUserId[ i ] )
				ZerarJogador( i );
			continue;
		}
		if ( s_iUserId[ i ] != pPlayer->GetUserID() )
		{
			ZerarJogador( i );
			s_iUserId[ i ] = pPlayer->GetUserID();
		}
		if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
			continue;
		++iJogadores;
		iUltimoVivo = i;

		if ( !pPlayer->IsAlive() )
			continue;
		if ( s_iTrocar[ i ] >= 0 )
			DarArmaDoDegrau( pPlayer, s_iTrocar[ i ] );

		// Bot: manda usar a arma da escada (o bot do HL2MP só reconhece as armas do HL2 e,
		// sem ameaça, sacaria a faca). A pilha de arma obrigatória zera quando ele nasce.
		if ( s_bBotArma[ i ] )
		{
			s_bBotArma[ i ] = false;
			CHL2MPBot *pBot = dynamic_cast< CHL2MPBot * >( pPlayer );
			CBaseHL2MPCombatWeapon *pArma = dynamic_cast< CBaseHL2MPCombatWeapon * >( s_hArma[ i ].Get() );
			if ( pBot && pArma )
				pBot->PushRequiredWeapon( pArma );
		}
	}

	// CA: alguém saiu e sobram 2 ou menos (contando quem saiu): a partida acaba.
	if ( iJogadores < s_iJogadoresAntes && s_iJogadoresAntes <= trok_corrida_saida_jogadores.GetInt() )
	{
		const int iMinimo = (int)( trok_corrida_saida_fracao.GetFloat() * pEstado->m_iMeta + 0.5f );
		pEstado->m_iVencedor = ( iJogadores == 1 && s_iAbatesPartida > iMinimo ) ? iUltimoVivo : 0;
		s_bPedirFim = true;
	}
	s_iJogadoresAntes = iJogadores;
}

CON_COMMAND( trok_corrida_recarregar, "Corrida Armada: relê scripts/trok_corrida.txt (vale na próxima troca de arma)." )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;
	const TrokCorridaEscada_t *pEscada = TrokCorrida_Escada( s_fraglimit.IsValid() ? s_fraglimit.GetInt() : 15, true );
	s_iMetaAplicada = -1;
	Msg( "Corrida Armada: %s relido (%d degraus na meta %d).\n", TROK_CORRIDA_ARQUIVO, pEscada ? pEscada->nDegraus : 0, pEscada ? pEscada->iMeta : 0 );
}
#endif // !CLIENT_DLL

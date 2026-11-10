//=============================================================================
// Trok: modo Hordas, estado em rede, máquina de melhoria e ajudas dos dois lados.
// Ver trok_hordas.h e design/modo-hordas.md. O diretor de rodadas fica em
// server/trok/trok_hordas_diretor.cpp; as estações em trok_hordas_estacao.cpp.
//=============================================================================

#include "cbase.h"
#include "filesystem.h"
#include "trok_hordas.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar trok_hordas_upgrade_arquivo( "trok_hordas_upgrade_arquivo", "scripts/trok_upgrade.txt", FCVAR_REPLICATED,
	"Hordas: arquivo com os níveis da máquina de melhoria." );

//-----------------------------------------------------------------------------
// Máquina de melhoria (scripts/trok_upgrade.txt)
//-----------------------------------------------------------------------------
static TrokUpgrade_t s_Upgrade;
static bool s_bUpgradeCarregado = false;

static void TrokUpgrade_Padrao( TrokUpgradeNivel_t &n )
{
	memset( &n, 0, sizeof( n ) );
	n.flDano = n.flPente = n.flReserva = 1.0f;
	n.flEfeitoForca = 1.0f;
	n.iCor[ 0 ] = n.iCor[ 1 ] = n.iCor[ 2 ] = -1;
}

static void TrokUpgrade_Carregar( void )
{
	s_bUpgradeCarregado = true;
	memset( &s_Upgrade, 0, sizeof( s_Upgrade ) );
	for ( int i = 0; i <= TROK_UPGRADE_MAX; ++i )
		TrokUpgrade_Padrao( s_Upgrade.niveis[ i ] );
	Q_strncpy( s_Upgrade.niveis[ 0 ].szNome, "", sizeof( s_Upgrade.niveis[ 0 ].szNome ) );
	s_Upgrade.iMunicaoPreco = 4500;

	KeyValues *pKV = new KeyValues( "TrokUpgrade" );
	if ( pKV->LoadFromFile( filesystem, trok_hordas_upgrade_arquivo.GetString(), "MOD" ) )
	{
		s_Upgrade.iMunicaoPreco = pKV->GetInt( "municao_preco", 4500 );
		for ( int i = 1; i <= TROK_UPGRADE_MAX; ++i )
		{
			char szChave[ 8 ];
			V_snprintf( szChave, sizeof( szChave ), "%d", i );
			KeyValues *pSub = pKV->FindKey( szChave );
			if ( !pSub )
				break;
			TrokUpgradeNivel_t &n = s_Upgrade.niveis[ i ];
			Q_strncpy( n.szNome, pSub->GetString( "nome", szChave ), sizeof( n.szNome ) );
			n.iPreco = pSub->GetInt( "preco", 5000 * i );
			n.flDano = pSub->GetFloat( "dano", 1.0f );
			n.flPente = pSub->GetFloat( "pente", 1.0f );
			n.flReserva = pSub->GetFloat( "reserva", 1.0f );
			Q_strncpy( n.szEfeito, pSub->GetString( "efeito", "" ), sizeof( n.szEfeito ) );
			n.flEfeitoChance = pSub->GetFloat( "efeito_chance", 0.0f );
			n.flEfeitoForca = pSub->GetFloat( "efeito_forca", 1.0f );
			n.flEfeitoTempo = pSub->GetFloat( "efeito_tempo", 0.0f );
			n.flEfeitoRaio = pSub->GetFloat( "efeito_raio", 0.0f );
			const char *pszCor = pSub->GetString( "cor", NULL );
			if ( pszCor )
				sscanf( pszCor, "%d %d %d", &n.iCor[ 0 ], &n.iCor[ 1 ], &n.iCor[ 2 ] );
			s_Upgrade.nNiveis = i;
		}
	}
	else
	{
		Warning( "Trok: não achei %s; máquina de melhoria sem níveis.\n", trok_hordas_upgrade_arquivo.GetString() );
	}
	pKV->deleteThis();
}

const TrokUpgrade_t &TrokUpgrade( bool bRecarregar )
{
	if ( !s_bUpgradeCarregado || bRecarregar )
		TrokUpgrade_Carregar();
	return s_Upgrade;
}

CON_COMMAND_F( trok_hordas_upgrade_recarregar, "Hordas: lê de novo scripts/trok_upgrade.txt.", FCVAR_CHEAT )
{
	TrokUpgrade( true );
	Msg( "Trok: %d níveis de melhoria.\n", s_Upgrade.nNiveis );
}

//-----------------------------------------------------------------------------
// Estado em rede
//-----------------------------------------------------------------------------
static CHandle< CTrokHordasEstado > s_hEstado;

IMPLEMENT_NETWORKCLASS_ALIASED( TrokHordasEstado, DT_TrokHordasEstado )

BEGIN_NETWORK_TABLE_NOBASE( CTrokHordasEstado, DT_TrokHordasEstado )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_iFase ) ),
	RecvPropFloat( RECVINFO( m_flFimFase ) ),
	RecvPropInt( RECVINFO( m_iRodada ) ),
	RecvPropInt( RECVINFO( m_iRodadaTipo ) ),
	RecvPropInt( RECVINFO( m_iRestantes ) ),
	RecvPropInt( RECVINFO( m_iRecorde ) ),
	RecvPropBool( RECVINFO( m_bEnergia ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aPontos ), RecvPropInt( RECVINFO( m_aPontos[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aAbates ), RecvPropInt( RECVINFO( m_aAbates[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aCaidoAte ), RecvPropFloat( RECVINFO( m_aCaidoAte[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aReviver ), RecvPropFloat( RECVINFO( m_aReviver[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aKit ), RecvPropInt( RECVINFO( m_aKit[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aVelZumbi ), RecvPropFloat( RECVINFO( m_aVelZumbi[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aNivel ), RecvPropInt( RECVINFO( m_aNivel[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aBonusAte ), RecvPropFloat( RECVINFO( m_aBonusAte[ 0 ] ) ) ),
	RecvPropInt( RECVINFO( m_iUltimoBonus ) ),
	RecvPropFloat( RECVINFO( m_flUltimoBonus ) ),
#else
	SendPropInt( SENDINFO( m_iFase ), 3, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flFimFase ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_iRodada ), 12, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iRodadaTipo ), 2, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iRestantes ), 14, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iRecorde ), 12, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bEnergia ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aPontos ), SendPropInt( SENDINFO_ARRAY( m_aPontos ), 24, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aAbates ), SendPropInt( SENDINFO_ARRAY( m_aAbates ), 16, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aCaidoAte ), SendPropFloat( SENDINFO_ARRAY( m_aCaidoAte ), 0, SPROP_NOSCALE ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aReviver ), SendPropFloat( SENDINFO_ARRAY( m_aReviver ), 8, 0, 0.0f, 1.0f ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aKit ), SendPropInt( SENDINFO_ARRAY( m_aKit ), 1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aVelZumbi ), SendPropFloat( SENDINFO_ARRAY( m_aVelZumbi ), 10, 0, 0.0f, 4.0f ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aNivel ), SendPropInt( SENDINFO_ARRAY( m_aNivel ), 3, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aBonusAte ), SendPropFloat( SENDINFO_ARRAY( m_aBonusAte ), 0, SPROP_NOSCALE ) ),
	SendPropInt( SENDINFO( m_iUltimoBonus ), 4, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flUltimoBonus ), 0, SPROP_NOSCALE ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( trok_hordas_estado, CTrokHordasEstado );

CTrokHordasEstado::CTrokHordasEstado()
{
	m_iFase = HORDAS_ESPERA;
	m_flFimFase = 0.0f;
	m_iRodada = 0;
	m_iRodadaTipo = HORDAS_RODADA_NORMAL;
	m_iRestantes = 0;
	m_iRecorde = 0;
	m_bEnergia = false;
	for ( int i = 0; i <= MAX_PLAYERS; ++i )
	{
		m_aPontos.Set( i, 0 );
		m_aAbates.Set( i, 0 );
		m_aCaidoAte.Set( i, 0.0f );
		m_aReviver.Set( i, 0.0f );
		m_aKit.Set( i, 0 );
		m_aVelZumbi.Set( i, 1.0f );
	}
	for ( int i = 0; i < ( MAX_PLAYERS + 1 ) * TROK_HORDAS_SLOTS; ++i )
		m_aNivel.Set( i, 0 );
	for ( int b = 0; b < HORDAS_BONUS_NUM; ++b )
		m_aBonusAte.Set( b, 0.0f );
	m_iUltimoBonus = 0;
	m_flUltimoBonus = -100.0f;
	s_hEstado = this;
}

CTrokHordasEstado::~CTrokHordasEstado()
{
	if ( s_hEstado.Get() == this )
		s_hEstado = NULL;
}

CTrokHordasEstado *TrokHordas_Estado( void )
{
	return s_hEstado.Get();
}

bool TrokHordas_Ativo( void )
{
	return s_hEstado.Get() != NULL;
}

bool TrokHordas_Caido( const CBaseEntity *pEnt )
{
	CTrokHordasEstado *p = s_hEstado.Get();
	if ( !p || !pEnt || !pEnt->IsPlayer() )
		return false;
	return p->GetCaidoAte( pEnt->entindex() ) > 0.0f;
}

int TrokHordas_NivelArma( const CBaseCombatWeapon *pArma )
{
	CTrokHordasEstado *p = s_hEstado.Get();
	if ( !p || !pArma )
		return 0;
	CBaseEntity *pDono = pArma->GetOwner();
	if ( !pDono || !pDono->IsPlayer() )
		return 0;
	return clamp( p->GetNivel( pDono->entindex(), pArma->GetSlot() ), 0, TrokUpgrade().nNiveis );
}

int TrokHordas_Pente( const CBaseCombatWeapon *pArma, int iBase )
{
	if ( iBase <= 0 )
		return iBase;
	const int iNivel = TrokHordas_NivelArma( pArma );
	if ( iNivel <= 0 )
		return iBase;
	return MAX( 1, (int)( iBase * TrokUpgrade().niveis[ iNivel ].flPente + 0.5f ) );
}

float TrokHordas_VelocidadeZumbi( const CBaseEntity *pPlayer )
{
	CTrokHordasEstado *p = s_hEstado.Get();
	if ( !p || !pPlayer )
		return 1.0f;
	return clamp( p->GetVelZumbi( pPlayer->entindex() ), 0.05f, 4.0f );
}

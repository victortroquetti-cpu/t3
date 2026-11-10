//=============================================================================
// Trok: arma de explosivos do Combat Arms (granadas, minas, claymore, remota, torreta,
// lança-foguetes). Ver trok_explosivos.h.
//
// Arremesso (grenades.md seção 3), previsto no cliente como o weapon_frag do HL2MP:
//   1. botão 1 (não correndo, arma já sacada) puxa o pino: flPino (M67 0,7 s);
//   2. depois vem o laço do arremesso (0,067 s), que se repete enquanto o botão está
//      apertado: segurar NÃO encurta o pavio (o CA não tem "cozinhar");
//   3. soltou o botão: a animação de depois do arremesso (0,366 s) e, 0,133 s dentro dela,
//      o objeto sai da câmera;
//   4. acabou a animação: pega a próxima (0,4 s) ou, sem mais nenhuma, troca de arma.
// Lançador (special_weapons.md seção 2): um tiro por ciclo; pente vazio recarrega sozinho.
// Claymore e remota: botão 2 detona todas as do dono (o "controle" do CA).
//=============================================================================

#include "cbase.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
	#include "c_hl2mp_player.h"
	#define CWeaponTrokExplosivo C_WeaponTrokExplosivo
#else
	#include "hl2mp_player.h"
#endif

#include "weapon_hl2mpbase.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "utldict.h"
#include "trok_explosivos.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar trok_ca_escala;

ConVar trok_explosivo_correndo_vel( "trok_explosivo_correndo_vel", "230", FCVAR_REPLICATED,
	"Explosivos: acima desta velocidade (u/s) segurando Shift conta como correndo e o arremesso não começa (CA)." );

// Quantos de cada um o jogador carrega (CA MaxAmount). Usadas pelos tipos de munição
// Trok* (patch em shared/hl2mp/hl2mp_gamerules.cpp, GetAmmoDef).
ConVar trok_max_granada( "trok_max_granada", "1", FCVAR_REPLICATED, "Máximo de granadas de mão (CA M67: 1)." );
ConVar trok_max_granada_luz( "trok_max_granada_luz", "2", FCVAR_REPLICATED, "Máximo de granadas de luz (CA XM84: 2)." );
ConVar trok_max_granada_fumaca( "trok_max_granada_fumaca", "2", FCVAR_REPLICATED, "Máximo de granadas de fumaça (CA M18: 2)." );
ConVar trok_max_mina( "trok_max_mina", "3", FCVAR_REPLICATED, "Máximo de minas (CA M16A1: 3)." );
ConVar trok_max_claymore( "trok_max_claymore", "2", FCVAR_REPLICATED, "Máximo de claymores (CA: 2)." );
ConVar trok_max_remota( "trok_max_remota", "2", FCVAR_REPLICATED, "Máximo de granadas remotas (CA Mobile_Grenade: 2)." );
ConVar trok_max_torreta( "trok_max_torreta", "1", FCVAR_REPLICATED, "Máximo de torretas (CA Auto-Turret: 1)." );

float TrokExplosivo_Escala( void )
{
	return MAX( 0.01f, trok_ca_escala.GetFloat() );
}

//-----------------------------------------------------------------------------
// Leitura do bloco "TrokExplosivo" dos scripts
//-----------------------------------------------------------------------------
static CUtlDict< TrokExplosivoInfo_t *, unsigned short > s_ExplosivoInfos;

static void TrokExplosivo_Padrao( TrokExplosivoInfo_t &e )
{
	Q_memset( &e, 0, sizeof( e ) );
	Q_strncpy( e.szEntidade, "trok_granada", sizeof( e.szEntidade ) );
	e.iTipo = TROK_EXP_GRANADA;
	e.iEfeito = TROK_EFEITO_FRAG;
	// M67 (grenades.md, "Data" e seção 3).
	e.flSaque = 0.4f;
	e.flPino = 0.7f;
	e.flPinoSom = 0.513f;
	e.flLaco = 0.067f;
	e.flSoltar = 0.133f;
	e.flPos = 0.366f;
	e.flRecarga = 0.4f;
	e.flCiclo = 1.0f;
	e.flDetonarAtraso = 0.05f;
	e.flVelocidade = 1550.0f;
	e.bGravidade = true;
	e.flDispersao = 0.001f;
	e.flPavio = 2.5f;
	e.flVida = 300.0f;
	e.vecCaixa.Init( 8.0f, 6.0f, 6.0f );
	e.flMeiaAltura = 6.0f;
	e.flVelocidadeMorte = 500.0f;
	e.flLuzTempo = 25.0f;
	e.flLuzAtraso = 0.25f;
	e.flLuzCostas = 0.25f;
	e.flLuzAtrasoCostas = 0.25f / 3.0f;
	e.flLuzMinimo = 0.1f;
	e.flFumacaSome = 8.0f;
	e.flFumacaFim = 10.0f;
	e.flMinaAtraso = 0.267f;
	e.flTorretaPreparo = 2.833f;
	e.flTorretaIntervalo = 0.13f;
	e.flTorretaAlcance = 800.0f;
	e.iTorretaBalas = 100;
	e.flTorretaBalaDano = 12.0f;
	e.flTorretaErro = 50.0f;
	e.flTorretaAngCima = 60.0f;
	e.flTorretaAngBaixo = 45.0f;
	e.flTorretaSensor = 10.0f;
	Q_strncpy( e.szModelo, "models/weapons/w_eq_fraggrenade_thrown.mdl", sizeof( e.szModelo ) );
}

static int TrokExplosivo_TipoPorNome( const char *psz )
{
	if ( !Q_stricmp( psz, "impacto" ) ) return TROK_EXP_IMPACTO;
	if ( !Q_stricmp( psz, "mina" ) ) return TROK_EXP_MINA;
	if ( !Q_stricmp( psz, "claymore" ) ) return TROK_EXP_CLAYMORE;
	if ( !Q_stricmp( psz, "remota" ) ) return TROK_EXP_REMOTA;
	if ( !Q_stricmp( psz, "torreta" ) ) return TROK_EXP_TORRETA;
	return TROK_EXP_GRANADA;
}

static int TrokExplosivo_EfeitoPorNome( const char *psz )
{
	if ( !Q_stricmp( psz, "luz" ) ) return TROK_EFEITO_LUZ;
	if ( !Q_stricmp( psz, "fumaca" ) ) return TROK_EFEITO_FUMACA;
	return TROK_EFEITO_FRAG;
}

#define TROK_LE_FLOAT( campo, chave )	e.campo = p->GetFloat( chave, e.campo )
#define TROK_LE_TEXTO( campo, chave )	Q_strncpy( e.campo, p->GetString( chave, e.campo ), sizeof( e.campo ) )

const TrokExplosivoInfo_t &TrokExplosivo_Info( const char *pszArma )
{
	if ( !pszArma )
		pszArma = "";

	unsigned short idx = s_ExplosivoInfos.Find( pszArma );
	if ( idx != s_ExplosivoInfos.InvalidIndex() )
		return *s_ExplosivoInfos[ idx ];

	TrokExplosivoInfo_t *pInfo = new TrokExplosivoInfo_t;
	TrokExplosivoInfo_t &e = *pInfo;
	TrokExplosivo_Padrao( e );
	Q_strncpy( e.szArma, pszArma, sizeof( e.szArma ) );

	char szArquivo[ MAX_PATH ];
	Q_snprintf( szArquivo, sizeof( szArquivo ), "scripts/%s.txt", pszArma );
	KeyValues *pKV = new KeyValues( "WeaponData" );
	if ( pKV->LoadFromFile( filesystem, szArquivo, "GAME" ) )
	{
		KeyValues *p = pKV->FindKey( "TrokExplosivo" );
		if ( p )
		{
			e.iTipo = TrokExplosivo_TipoPorNome( p->GetString( "tipo", "granada" ) );
			e.iEfeito = TrokExplosivo_EfeitoPorNome( p->GetString( "efeito", "frag" ) );
			e.bLancador = p->GetInt( "lancador", e.iTipo == TROK_EXP_IMPACTO ? 1 : 0 ) != 0;
			TROK_LE_TEXTO( szEntidade, "entidade" );
			TROK_LE_FLOAT( flSaque, "saque" );
			TROK_LE_FLOAT( flPino, "pino" );
			TROK_LE_FLOAT( flPinoSom, "pino_som" );
			TROK_LE_FLOAT( flLaco, "laco" );
			TROK_LE_FLOAT( flSoltar, "soltar" );
			TROK_LE_FLOAT( flPos, "pos" );
			TROK_LE_FLOAT( flRecarga, "recarga" );
			TROK_LE_FLOAT( flCiclo, "ciclo" );
			TROK_LE_FLOAT( flCoice, "coice" );
			TROK_LE_FLOAT( flDetonarAtraso, "detonar_atraso" );
			TROK_LE_FLOAT( flVelocidade, "velocidade" );
			e.bGravidade = p->GetInt( "gravidade", e.bGravidade ? 1 : 0 ) != 0;
			TROK_LE_FLOAT( flDispersao, "dispersao" );
			TROK_LE_FLOAT( flPavio, "pavio" );
			TROK_LE_FLOAT( flVida, "vida" );
			const char *pszCaixa = p->GetString( "caixa", NULL );
			if ( pszCaixa && pszCaixa[ 0 ] )
			{
				float x = e.vecCaixa.x, y = e.vecCaixa.y, z = e.vecCaixa.z;
				sscanf( pszCaixa, "%f %f %f", &x, &y, &z );
				e.vecCaixa.Init( x, y, z );
			}
			TROK_LE_FLOAT( flMeiaAltura, "meia_altura" );
			TROK_LE_FLOAT( flVelocidadeMorte, "velocidade_morte" );
			TROK_LE_FLOAT( flDano, "dano" );
			TROK_LE_FLOAT( flRaio, "raio" );
			TROK_LE_FLOAT( flConeDano, "cone_dano" );
			TROK_LE_FLOAT( flConeRaio, "cone_raio" );
			TROK_LE_FLOAT( flConeAngulo, "cone_angulo" );
			TROK_LE_FLOAT( flMinaAtraso, "mina_atraso" );
			TROK_LE_FLOAT( flLuzTempo, "luz_tempo" );
			TROK_LE_FLOAT( flLuzAtraso, "luz_atraso" );
			TROK_LE_FLOAT( flLuzCostas, "luz_costas" );
			TROK_LE_FLOAT( flLuzAtrasoCostas, "luz_atraso_costas" );
			TROK_LE_FLOAT( flLuzMinimo, "luz_minimo" );
			TROK_LE_FLOAT( flFumacaSome, "fumaca_some" );
			TROK_LE_FLOAT( flFumacaFim, "fumaca_fim" );
			TROK_LE_FLOAT( flTorretaPreparo, "torreta_preparo" );
			TROK_LE_FLOAT( flTorretaIntervalo, "torreta_intervalo" );
			TROK_LE_FLOAT( flTorretaAlcance, "torreta_alcance" );
			e.iTorretaBalas = p->GetInt( "torreta_balas", e.iTorretaBalas );
			TROK_LE_FLOAT( flTorretaBalaDano, "torreta_bala_dano" );
			TROK_LE_FLOAT( flTorretaErro, "torreta_erro" );
			TROK_LE_FLOAT( flTorretaAngCima, "torreta_ang_cima" );
			TROK_LE_FLOAT( flTorretaAngBaixo, "torreta_ang_baixo" );
			TROK_LE_FLOAT( flTorretaSensor, "torreta_sensor" );
			e.bUmPorVez = p->GetInt( "um_por_vez", 0 ) != 0;
			TROK_LE_TEXTO( szModelo, "modelo" );
			TROK_LE_TEXTO( szSomQuique, "som_quique" );
			TROK_LE_TEXTO( szSomExplosao, "som_explosao" );
			TROK_LE_TEXTO( szSomAviso, "som_aviso" );
			TROK_LE_TEXTO( szSomAtivo, "som_ativo" );
			TROK_LE_TEXTO( szSomTiro, "som_tiro" );
		}
		else
		{
			Warning( "Trok: %s não tem o bloco TrokExplosivo; usando os números da M67.\n", szArquivo );
		}
	}
	else
	{
		Warning( "Trok: não achei %s.\n", szArquivo );
	}
	pKV->deleteThis();

	s_ExplosivoInfos.Insert( pszArma, pInfo );
	return *pInfo;
}

//-----------------------------------------------------------------------------
// A arma
//-----------------------------------------------------------------------------
enum
{
	EXP_PRONTA = 0,		// na mão, esperando o botão
	EXP_PINO,			// puxando o pino
	EXP_SEGURA,			// laço do arremesso (botão apertado)
	EXP_SOLTA,			// soltou: esperando o objeto sair da mão
	EXP_POS,			// resto da animação depois de soltar
	EXP_RECARGA,		// pegando a próxima / recarregando o pente
};

static acttable_t s_actGranada[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_GRENADE,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_GRENADE,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_GRENADE,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_GRENADE,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_GRENADE,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_GRENADE,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_GRENADE,					false },
};
static acttable_t s_actSlam[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_SLAM,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_SLAM,						false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_SLAM,				false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_SLAM,				false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_SLAM,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_SLAM,			false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_SLAM,					false },
};
static acttable_t s_actRPG[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_RPG,						false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_RPG,						false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_RPG,				false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_RPG,				false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_RPG,		false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_RPG,			false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_RPG,						false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_RPG,					false },
};

class CWeaponTrokExplosivo : public CWeaponHL2MPBase
{
public:
	DECLARE_CLASS( CWeaponTrokExplosivo, CWeaponHL2MPBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponTrokExplosivo();

	virtual void Precache( void );
	virtual void ItemPostFrame( void );
	virtual void PrimaryAttack( void ) {}		// tudo em ItemPostFrame
	virtual void SecondaryAttack( void ) {}
	virtual bool Reload( void );
	virtual bool Deploy( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void Drop( const Vector &vecVelocity );
	virtual acttable_t *ActivityList( int &iActivityCount );
	virtual bool HasAnyAmmo( void );
	virtual bool CanBeSelected( void );

	const TrokExplosivoInfo_t &Exp( void ) const { return TrokExplosivo_Info( GetWpnData().szClassName ); }
	bool PinoPuxado( void ) const { return m_iEstado == EXP_PINO || m_iEstado == EXP_SEGURA || m_iEstado == EXP_SOLTA; }

private:
	CBasePlayer *Dono( void ) const { return ToBasePlayer( GetOwner() ); }
	// Arremesso: quantos ainda tem (munição do tipo da arma, como o weapon_frag).
	int Contagem( void ) const { return ( GetOwner() && m_iPrimaryAmmoType >= 0 ) ? GetOwner()->GetAmmoCount( m_iPrimaryAmmoType ) : 0; }
	bool DonoCorrendo( CBasePlayer *pPlayer ) const;
	bool Detonador( void ) const { return Exp().iTipo == TROK_EXP_CLAYMORE || Exp().iTipo == TROK_EXP_REMOTA; }
	void Anim( int iAct, int iActReserva, float flDuracao );
	void FrameArremesso( CBasePlayer *pPlayer );
	void FrameLancador( CBasePlayer *pPlayer );
	void Soltar( CBasePlayer *pPlayer );
	void Lancar( CBasePlayer *pPlayer );
	void Acabou( CBasePlayer *pPlayer );
	void Detonar( CBasePlayer *pPlayer );
#ifndef CLIENT_DLL
	void CriarObjeto( CBasePlayer *pPlayer, bool bMorte );
#endif

	CNetworkVar( int, m_iEstado );
	CNetworkVar( float, m_flEstadoFim );
	CNetworkVar( float, m_flSoltarEm );
	CNetworkVar( float, m_flPinoInicio );
	CNetworkVar( bool, m_bSomPino );

	CWeaponTrokExplosivo( const CWeaponTrokExplosivo & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponTrokExplosivo, DT_WeaponTrokExplosivo )

BEGIN_NETWORK_TABLE( CWeaponTrokExplosivo, DT_WeaponTrokExplosivo )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_iEstado ) ),
	RecvPropTime( RECVINFO( m_flEstadoFim ) ),
	RecvPropTime( RECVINFO( m_flSoltarEm ) ),
	RecvPropTime( RECVINFO( m_flPinoInicio ) ),
	RecvPropBool( RECVINFO( m_bSomPino ) ),
#else
	SendPropInt( SENDINFO( m_iEstado ), 4, SPROP_UNSIGNED ),
	SendPropTime( SENDINFO( m_flEstadoFim ) ),
	SendPropTime( SENDINFO( m_flSoltarEm ) ),
	SendPropTime( SENDINFO( m_flPinoInicio ) ),
	SendPropBool( SENDINFO( m_bSomPino ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponTrokExplosivo )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_iEstado, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flEstadoFim, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD_TOL( m_flSoltarEm, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD_TOL( m_flPinoInicio, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD( m_bSomPino, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

// Uma subclasse por arma (o cliente pede uma classe por nome de entidade); cada uma lê
// scripts/<nome>.txt. Explosivo novo = mais uma linha destas + o script.
#define TROK_EXPLOSIVO_CLASS( ClassName, NetName, TableName, entityname )		\
	class ClassName : public CWeaponTrokExplosivo								\
	{																			\
	public:																		\
		DECLARE_CLASS( ClassName, CWeaponTrokExplosivo );						\
		DECLARE_NETWORKCLASS();													\
		DECLARE_PREDICTABLE();													\
	};																			\
	IMPLEMENT_NETWORKCLASS_ALIASED( NetName, TableName )						\
	BEGIN_NETWORK_TABLE( ClassName, TableName )									\
	END_NETWORK_TABLE()															\
	BEGIN_PREDICTION_DATA( ClassName )											\
	END_PREDICTION_DATA()														\
	LINK_ENTITY_TO_CLASS( entityname, ClassName );								\
	PRECACHE_WEAPON_REGISTER( entityname );

#ifdef CLIENT_DLL
	#define CWeaponTrokGranada C_WeaponTrokGranada
	#define CWeaponTrokGranadaLuz C_WeaponTrokGranadaLuz
	#define CWeaponTrokGranadaFumaca C_WeaponTrokGranadaFumaca
	#define CWeaponTrokMina C_WeaponTrokMina
	#define CWeaponTrokClaymore C_WeaponTrokClaymore
	#define CWeaponTrokRemota C_WeaponTrokRemota
	#define CWeaponTrokTorreta C_WeaponTrokTorreta
	#define CWeaponTrokFoguete C_WeaponTrokFoguete
#endif

TROK_EXPLOSIVO_CLASS( CWeaponTrokGranada, WeaponTrokGranada, DT_WeaponTrokGranada, weapon_trok_granada )
TROK_EXPLOSIVO_CLASS( CWeaponTrokGranadaLuz, WeaponTrokGranadaLuz, DT_WeaponTrokGranadaLuz, weapon_trok_granada_luz )
TROK_EXPLOSIVO_CLASS( CWeaponTrokGranadaFumaca, WeaponTrokGranadaFumaca, DT_WeaponTrokGranadaFumaca, weapon_trok_granada_fumaca )
TROK_EXPLOSIVO_CLASS( CWeaponTrokMina, WeaponTrokMina, DT_WeaponTrokMina, weapon_trok_mina )
TROK_EXPLOSIVO_CLASS( CWeaponTrokClaymore, WeaponTrokClaymore, DT_WeaponTrokClaymore, weapon_trok_claymore )
TROK_EXPLOSIVO_CLASS( CWeaponTrokRemota, WeaponTrokRemota, DT_WeaponTrokRemota, weapon_trok_remota )
TROK_EXPLOSIVO_CLASS( CWeaponTrokTorreta, WeaponTrokTorreta, DT_WeaponTrokTorreta, weapon_trok_torreta )
TROK_EXPLOSIVO_CLASS( CWeaponTrokFoguete, WeaponTrokFoguete, DT_WeaponTrokFoguete, weapon_trok_foguete )

CWeaponTrokExplosivo::CWeaponTrokExplosivo()
{
	m_iEstado = EXP_PRONTA;
	m_flEstadoFim = 0.0f;
	m_flSoltarEm = 0.0f;
	m_flPinoInicio = 0.0f;
	m_bSomPino = false;
	m_bFiresUnderwater = true;
}

void CWeaponTrokExplosivo::Precache( void )
{
	BaseClass::Precache();

	const TrokExplosivoInfo_t &e = Exp();
	if ( e.szModelo[ 0 ] )
		PrecacheModel( e.szModelo );
	const char *sons[] = { e.szSomQuique, e.szSomExplosao, e.szSomAviso, e.szSomAtivo, e.szSomTiro };
	for ( int i = 0; i < ARRAYSIZE( sons ); ++i )
	{
		if ( sons[ i ][ 0 ] )
			PrecacheScriptSound( sons[ i ] );
	}
#ifndef CLIENT_DLL
	if ( e.iEfeito == TROK_EFEITO_FUMACA )
		UTIL_PrecacheOther( "env_particlesmokegrenade" );
#endif
}

acttable_t *CWeaponTrokExplosivo::ActivityList( int &iActivityCount )
{
	const TrokExplosivoInfo_t &e = Exp();
	if ( e.bLancador )
	{
		iActivityCount = ARRAYSIZE( s_actRPG );
		return s_actRPG;
	}
	if ( e.iTipo == TROK_EXP_CLAYMORE || e.iTipo == TROK_EXP_MINA || e.iTipo == TROK_EXP_REMOTA )
	{
		iActivityCount = ARRAYSIZE( s_actSlam );
		return s_actSlam;
	}
	iActivityCount = ARRAYSIZE( s_actGranada );
	return s_actGranada;
}

// Granadas e minas: sem nenhuma, a arma some (CA "HideWhenEmpty"). Claymore e remota ficam
// para o botão 2 ainda detonar as que estão no chão. Lançador: pente ou reserva.
bool CWeaponTrokExplosivo::HasAnyAmmo( void )
{
	if ( Exp().bLancador )
		return m_iClip1 > 0 || Contagem() > 0;
	return Contagem() > 0 || Detonador();
}

bool CWeaponTrokExplosivo::CanBeSelected( void )
{
	return HasAnyAmmo() && BaseClass::CanBeSelected();
}

bool CWeaponTrokExplosivo::DonoCorrendo( CBasePlayer *pPlayer ) const
{
	if ( !pPlayer || !( pPlayer->m_nButtons & IN_SPEED ) || !( pPlayer->GetFlags() & FL_ONGROUND ) )
		return false;
	return pPlayer->GetAbsVelocity().Length2D() > trok_explosivo_correndo_vel.GetFloat();
}

// Toca a animação da arma e acerta a velocidade para durar o tempo do CA.
void CWeaponTrokExplosivo::Anim( int iAct, int iActReserva, float flDuracao )
{
	if ( !SendWeaponAnim( iAct ) )
	{
		if ( iActReserva == ACT_INVALID || !SendWeaponAnim( iActReserva ) )
			return;
	}
	CBasePlayer *pPlayer = Dono();
	CBaseViewModel *vm = pPlayer ? pPlayer->GetViewModel( m_nViewModelIndex ) : NULL;
	if ( vm && flDuracao > 0.01f )
	{
		const float flSeq = SequenceDuration();
		if ( flSeq > 0.01f )
			vm->SetPlaybackRate( clamp( flSeq / flDuracao, 0.25f, 4.0f ) );
	}
}

bool CWeaponTrokExplosivo::Deploy( void )
{
	m_iEstado = EXP_PRONTA;
	m_bSomPino = false;
	const bool bOk = BaseClass::Deploy();
	const TrokExplosivoInfo_t &e = Exp();
	Anim( ACT_VM_DRAW, ACT_INVALID, e.flSaque );
	m_flNextPrimaryAttack = gpGlobals->curtime + e.flSaque;
	m_flNextSecondaryAttack = gpGlobals->curtime + e.flSaque;
	m_flTimeWeaponIdle = gpGlobals->curtime + e.flSaque;
	return bOk;
}

// Trocar de arma com o pino puxado: a granada volta para o bolso (não é gasta).
bool CWeaponTrokExplosivo::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( m_iEstado != EXP_POS )
		m_iEstado = EXP_PRONTA;
	return BaseClass::Holster( pSwitchingTo );
}

bool CWeaponTrokExplosivo::Reload( void )
{
	const TrokExplosivoInfo_t &e = Exp();
	CBasePlayer *pPlayer = Dono();
	if ( !e.bLancador || !pPlayer || m_iEstado != EXP_PRONTA )
		return false;
	if ( m_iClip1 >= GetMaxClip1() || pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
		return false;

	m_iEstado = EXP_RECARGA;
	m_flEstadoFim = gpGlobals->curtime + e.flRecarga;
	Anim( ACT_VM_RELOAD, ACT_INVALID, e.flRecarga );
	pPlayer->SetAnimation( PLAYER_RELOAD );
	WeaponSound( RELOAD );
	return true;
}

void CWeaponTrokExplosivo::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = Dono();
	if ( !pPlayer )
		return;

	if ( Exp().bLancador )
		FrameLancador( pPlayer );
	else
		FrameArremesso( pPlayer );
}

void CWeaponTrokExplosivo::FrameArremesso( CBasePlayer *pPlayer )
{
	const TrokExplosivoInfo_t &e = Exp();
	const float flAgora = gpGlobals->curtime;

	switch ( m_iEstado )
	{
	case EXP_PRONTA:
		if ( ( pPlayer->m_nButtons & IN_ATTACK ) && flAgora >= m_flNextPrimaryAttack && Contagem() > 0 && !DonoCorrendo( pPlayer ) )
		{
			m_iEstado = EXP_PINO;
			m_flPinoInicio = flAgora;
			m_flEstadoFim = flAgora + e.flPino;
			m_bSomPino = false;
			m_flTimeWeaponIdle = FLT_MAX;
			if ( e.flPino > 0.0f )
				Anim( ACT_VM_PULLPIN, ACT_VM_PULLBACK_HIGH, e.flPino );
		}
		else if ( ( pPlayer->m_nButtons & IN_ATTACK2 ) && Detonador() && flAgora >= m_flNextSecondaryAttack )
		{
			Detonar( pPlayer );
		}
		else if ( flAgora >= m_flTimeWeaponIdle && Contagem() > 0 )
		{
			SendWeaponAnim( ACT_VM_IDLE );
			m_flTimeWeaponIdle = flAgora + 5.0f;
		}
		break;

	case EXP_PINO:
		if ( !m_bSomPino && e.flPino > 0.0f && flAgora >= m_flPinoInicio + e.flPinoSom )
		{
			m_bSomPino = true;
			WeaponSound( SPECIAL1 );
		}
		if ( flAgora >= m_flEstadoFim )
		{
			if ( e.flLaco > 0.0f )
			{
				m_iEstado = EXP_SEGURA;
				m_flEstadoFim = flAgora + e.flLaco;
			}
			else
			{
				Soltar( pPlayer );
			}
		}
		break;

	case EXP_SEGURA:
		// Segurar o botão só segura a granada na mão: o pavio começa quando ela sai.
		if ( flAgora >= m_flEstadoFim )
		{
			if ( pPlayer->m_nButtons & IN_ATTACK )
				m_flEstadoFim = flAgora + e.flLaco;
			else
				Soltar( pPlayer );
		}
		break;

	case EXP_SOLTA:
		if ( flAgora >= m_flSoltarEm )
		{
			Lancar( pPlayer );
			m_iEstado = EXP_POS;
		}
		break;

	case EXP_POS:
		if ( flAgora >= m_flEstadoFim )
		{
			if ( Contagem() <= 0 )
			{
				Acabou( pPlayer );
			}
			else
			{
				m_iEstado = EXP_RECARGA;
				m_flEstadoFim = flAgora + e.flRecarga;
				Anim( ACT_VM_DRAW, ACT_INVALID, e.flRecarga );
			}
		}
		break;

	case EXP_RECARGA:
		if ( flAgora >= m_flEstadoFim )
		{
			m_iEstado = EXP_PRONTA;
			m_flNextPrimaryAttack = flAgora;
			m_flTimeWeaponIdle = flAgora;
		}
		break;
	}
}

void CWeaponTrokExplosivo::Soltar( CBasePlayer *pPlayer )
{
	const TrokExplosivoInfo_t &e = Exp();
	m_iEstado = EXP_SOLTA;
	m_flSoltarEm = gpGlobals->curtime + e.flSoltar;
	m_flEstadoFim = gpGlobals->curtime + MAX( e.flPos, e.flSoltar );
	Anim( ACT_VM_THROW, ACT_VM_PRIMARYATTACK, e.flPos );
	if ( e.flSoltar <= 0.0f )
	{
		Lancar( pPlayer );
		m_iEstado = EXP_POS;
	}
}

// O objeto sai da mão (arremesso) ou do cano (lançador).
void CWeaponTrokExplosivo::Lancar( CBasePlayer *pPlayer )
{
	if ( Exp().bLancador )
	{
		if ( m_iClip1 <= 0 )
			return;
		m_iClip1 -= 1;
		WeaponSound( SINGLE );
		// CA: cada tiro levanta a câmera (BaseCamRecoilPitch).
		if ( Exp().flCoice > 0.0f )
			pPlayer->ViewPunch( QAngle( -Exp().flCoice, 0, 0 ) );
	}
	else
	{
		if ( Contagem() <= 0 )
			return;
		pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
	}
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
#ifndef CLIENT_DLL
	CriarObjeto( pPlayer, false );
	pPlayer->OnMyWeaponFired( this );
#endif
}

// Sem nenhuma na mão: troca de arma e a arma some (claymore e remota ficam para detonar).
void CWeaponTrokExplosivo::Acabou( CBasePlayer *pPlayer )
{
	m_iEstado = EXP_PRONTA;
	m_flNextPrimaryAttack = gpGlobals->curtime;
	if ( Detonador() )
		return;

	pPlayer->SwitchToNextBestWeapon( this );
#ifndef CLIENT_DLL
	if ( pPlayer->GetActiveWeapon() != this )
	{
		pPlayer->Weapon_Detach( this );
		UTIL_Remove( this );
	}
#endif
}

void CWeaponTrokExplosivo::Detonar( CBasePlayer *pPlayer )
{
	const TrokExplosivoInfo_t &e = Exp();
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
	SendWeaponAnim( ACT_VM_SECONDARYATTACK );
#ifndef CLIENT_DLL
	TrokExplosivo_Detonar( pPlayer, e.szArma, e.flDetonarAtraso );
	if ( e.szSomAtivo[ 0 ] )
		pPlayer->EmitSound( e.szSomAtivo );
#endif
}

void CWeaponTrokExplosivo::FrameLancador( CBasePlayer *pPlayer )
{
	const TrokExplosivoInfo_t &e = Exp();
	const float flAgora = gpGlobals->curtime;

	if ( m_iEstado == EXP_RECARGA )
	{
		if ( flAgora < m_flEstadoFim )
			return;
		const int iFalta = GetMaxClip1() - m_iClip1;
		const int iTem = pPlayer->GetAmmoCount( m_iPrimaryAmmoType );
		const int iPoe = MIN( iFalta, iTem );
		m_iClip1 += iPoe;
		pPlayer->RemoveAmmo( iPoe, m_iPrimaryAmmoType );
		m_iEstado = EXP_PRONTA;
		m_flNextPrimaryAttack = flAgora;
		return;
	}

	if ( ( pPlayer->m_nButtons & IN_RELOAD ) && flAgora >= m_flNextPrimaryAttack )
	{
		Reload();
		return;
	}

	if ( flAgora < m_flNextPrimaryAttack )
		return;

	// O CA recarrega sozinho quando o disparo acaba com o pente vazio.
	if ( m_iClip1 <= 0 )
	{
		if ( !Reload() && ( pPlayer->m_nButtons & IN_ATTACK ) && !m_bFireOnEmpty )
		{
			WeaponSound( EMPTY );
			m_bFireOnEmpty = true;
		}
		if ( !( pPlayer->m_nButtons & IN_ATTACK ) )
			m_bFireOnEmpty = false;
		return;
	}

	if ( ( pPlayer->m_nButtons & IN_ATTACK ) && !DonoCorrendo( pPlayer ) )
	{
		Anim( ACT_VM_PRIMARYATTACK, ACT_INVALID, e.flCiclo );
		Lancar( pPlayer );
		m_flNextPrimaryAttack = flAgora + e.flCiclo;
		m_flTimeWeaponIdle = flAgora + e.flCiclo;
	}
	else if ( flAgora >= m_flTimeWeaponIdle )
	{
		SendWeaponAnim( ACT_VM_IDLE );
		m_flTimeWeaponIdle = flAgora + 5.0f;
	}
}

#ifndef CLIENT_DLL
// Morreu com o pino puxado: a granada cai do corpo, com o pavio inteiro (grenades.md 14).
// Explosivo nunca fica no chão para outro pegar (CA: não cai ao morrer).
void CWeaponTrokExplosivo::Drop( const Vector &vecVelocity )
{
	CBasePlayer *pPlayer = Dono();
	if ( pPlayer && !pPlayer->IsAlive() && PinoPuxado() && Exp().iTipo == TROK_EXP_GRANADA )
		CriarObjeto( pPlayer, true );
	m_iEstado = EXP_PRONTA;
	UTIL_Remove( this );
}

void CWeaponTrokExplosivo::CriarObjeto( CBasePlayer *pPlayer, bool bMorte )
{
	const TrokExplosivoInfo_t &e = Exp();
	const float flEsc = TrokExplosivo_Escala();
	Vector vecOrigem, vecVel;
	QAngle angInicial( 0, pPlayer->EyeAngles().y, 0 );

	if ( bMorte )
	{
		// Do meio do corpo, para onde o corpo olhava (só o giro).
		vecOrigem = pPlayer->WorldSpaceCenter();
		Vector vecFrente;
		AngleVectors( angInicial, &vecFrente );
		vecVel = vecFrente * e.flVelocidadeMorte * flEsc;
	}
	else if ( e.iTipo == TROK_EXP_CLAYMORE )
	{
		// Plantada: à frente do pé, na metade da largura do corpo, e descida até o chão.
		Vector vecFrente;
		AngleVectors( angInicial, &vecFrente );
		const float flMeio = pPlayer->CollisionProp()->OBBMaxs().x;
		Vector vecAlto = pPlayer->GetAbsOrigin() + vecFrente * flMeio + Vector( 0, 0, 16 );
		trace_t tr;
		UTIL_TraceLine( vecAlto, vecAlto - Vector( 0, 0, 512 ), MASK_SOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &tr );
		vecOrigem = ( tr.fraction < 1.0f && !tr.startsolid ) ? tr.endpos : pPlayer->GetAbsOrigin();
		vecVel.Init();
	}
	else
	{
		// Da câmera, ao longo da mira, com o desvio do CA; o movimento de quem joga não soma.
		vecOrigem = pPlayer->EyePosition();
		Vector vecFrente, vecDireita, vecCima;
		pPlayer->EyeVectors( &vecFrente, &vecDireita, &vecCima );
		if ( e.flDispersao > 0.0f )
		{
			vecFrente += vecDireita * random->RandomFloat( -e.flDispersao, e.flDispersao );
			vecFrente += vecCima * random->RandomFloat( -e.flDispersao, e.flDispersao );
			VectorNormalize( vecFrente );
		}
		vecVel = vecFrente * e.flVelocidade * flEsc;
		if ( e.iTipo == TROK_EXP_IMPACTO )
			VectorAngles( vecFrente, angInicial );
	}

	CBaseEntity *pObj = TrokExplosivo_Criar( e, pPlayer, vecOrigem, vecVel, angInicial );

	// "Granada!": o time de quem jogou ouve (grenades.md, Corrections C1).
	if ( pObj && !bMorte && e.szSomAviso[ 0 ] )
	{
		CRecipientFilter filter;
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *pOutro = UTIL_PlayerByIndex( i );
			if ( !pOutro || pOutro->IsBot() )
				continue;
			if ( pOutro == pPlayer || ( g_pGameRules->IsTeamplay() && g_pGameRules->PlayerRelationship( pPlayer, pOutro ) == GR_TEAMMATE ) )
				filter.AddRecipient( pOutro );
		}
		if ( filter.GetRecipientCount() > 0 )
			pPlayer->EmitSound( filter, pPlayer->entindex(), e.szSomAviso );
	}
}
#else
void CWeaponTrokExplosivo::Drop( const Vector &vecVelocity )
{
	BaseClass::Drop( vecVelocity );
}
#endif

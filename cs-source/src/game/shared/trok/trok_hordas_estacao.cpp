//=============================================================================
// Trok: estações do modo Hordas (lugares físicos do mapa, usados com E, como no CA).
//
//   func_trok_compra_parede      arma na parede: compra a arma; tendo ela, compra munição
//   func_trok_porta_paga         porta/entulho pago: some e abre uma área (nascedouros de zumbi)
//   func_trok_maquina_upgrade    máquina de melhoria: sobe a arma da mão um nível (1 a 5)
//   func_trok_caixa_misteriosa   caixa: gira e oferece uma arma sorteada (scripts/trok_hordas.txt)
//   func_trok_bancada            bancada: colete, kit de reanimação, torreta, granadas...
//   func_trok_energia            chave de energia: liga as máquinas que precisam de energia
//
// Todas são pincéis (o mapa desenha a parede, a máquina, a porta) e usam uma classe só em
// rede (CTrokHordasEstacao): o cliente lê tipo, preço e rótulo para o aviso
// "E — COMPRAR SUBMETRALHADORA: 1000" (client/trok/hud_trok_hordas.cpp). Os gastos e as
// regras ficam no diretor (server/trok/trok_hordas_diretor.cpp). Entradas e saídas no FGD.
//=============================================================================

#include "cbase.h"
#include "trok_hordas.h"
#include "trok_surto.h"
#include "weapon_parse.h"
#include "collisionutils.h"

#ifndef CLIENT_DLL
	#include "entityoutput.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar trok_hordas_usar_distancia( "trok_hordas_usar_distancia", "80", FCVAR_REPLICATED,
	"Hordas: distância (u) do olho até a estação para o aviso aparecer (o E do motor alcança 80 u na horizontal, PLAYER_USE_RADIUS)." );
ConVar trok_hordas_caixa_giro( "trok_hordas_caixa_giro", "4", FCVAR_REPLICATED, "Hordas: segundos que a caixa misteriosa gira." );
ConVar trok_hordas_caixa_oferta( "trok_hordas_caixa_oferta", "10", FCVAR_REPLICATED, "Hordas: segundos que a arma sorteada fica esperando o dono pegar." );
ConVar trok_hordas_caixa_preco( "trok_hordas_caixa_preco", "950", FCVAR_REPLICATED, "Hordas: preço da caixa misteriosa (quando o mapa não diz)." );
ConVar trok_hordas_caixa_barata_preco( "trok_hordas_caixa_barata_preco", "10", FCVAR_REPLICATED, "Hordas: preço da caixa com o bônus Caixa barata." );
ConVar trok_hordas_colete( "trok_hordas_colete", "100", FCVAR_REPLICATED, "Hordas: armadura que o colete da bancada dá." );

static CUtlVector< CTrokHordasEstacao * > s_Estacoes;

int TrokHordas_PrecoCaixa( int iBase )
{
	CTrokHordasEstado *pEstado = TrokHordas_Estado();
	if ( pEstado && pEstado->BonusAtivo( HORDAS_BONUS_CAIXA_BARATA ) )
		return MAX( 0, trok_hordas_caixa_barata_preco.GetInt() );
	return iBase > 0 ? iBase : trok_hordas_caixa_preco.GetInt();
}

IMPLEMENT_NETWORKCLASS_ALIASED( TrokHordasEstacao, DT_TrokHordasEstacao )

BEGIN_NETWORK_TABLE( CTrokHordasEstacao, DT_TrokHordasEstacao )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_iTipo ) ),
	RecvPropInt( RECVINFO( m_iPreco ) ),
	RecvPropInt( RECVINFO( m_iPreco2 ) ),
	RecvPropInt( RECVINFO( m_iEstado ) ),
	RecvPropInt( RECVINFO( m_iDono ) ),
	RecvPropBool( RECVINFO( m_bPrecisaEnergia ) ),
	RecvPropBool( RECVINFO( m_bUsada ) ),
	RecvPropString( RECVINFO( m_szRotulo ) ),
	RecvPropString( RECVINFO( m_szArma ) ),
#else
	SendPropInt( SENDINFO( m_iTipo ), 4, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iPreco ), 20, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iPreco2 ), 20, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iEstado ), 2, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDono ), 8, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bPrecisaEnergia ) ),
	SendPropBool( SENDINFO( m_bUsada ) ),
	SendPropString( SENDINFO( m_szRotulo ) ),
	SendPropString( SENDINFO( m_szArma ) ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( func_trok_compra_parede, CTrokHordasEstacao );
#ifndef CLIENT_DLL	// no cliente a fábrica é uma só (o servidor manda a classe em rede, não o nome)
LINK_ENTITY_TO_CLASS( func_trok_porta_paga, CTrokHordasEstacao );
LINK_ENTITY_TO_CLASS( func_trok_maquina_upgrade, CTrokHordasEstacao );
LINK_ENTITY_TO_CLASS( func_trok_caixa_misteriosa, CTrokHordasEstacao );
LINK_ENTITY_TO_CLASS( func_trok_bancada, CTrokHordasEstacao );
LINK_ENTITY_TO_CLASS( func_trok_energia, CTrokHordasEstacao );
#endif

CTrokHordasEstacao::CTrokHordasEstacao()
{
	m_iTipo = ESTACAO_PAREDE;
	m_iPreco = 0;
	m_iPreco2 = 0;
	m_iEstado = 0;
	m_iDono = 0;
	m_bPrecisaEnergia = false;
	m_bUsada = false;
#ifndef CLIENT_DLL
	m_iszArmaMapa = NULL_STRING;
	m_iszRotuloMapa = NULL_STRING;
	m_iArea = 0;
	m_iUsos = 0;
	m_flProxUso = 0.0f;
	m_flFimEstado = 0.0f;
	m_bDesligada = false;
	m_iEnergiaMapa = -1;
#endif
	s_Estacoes.AddToTail( this );
}

CTrokHordasEstacao::~CTrokHordasEstacao()
{
	s_Estacoes.FindAndRemove( this );
}

int CTrokHordasEstacao::Num( void )
{
	return s_Estacoes.Count();
}

CTrokHordasEstacao *CTrokHordasEstacao::Get( int i )
{
	return s_Estacoes.IsValidIndex( i ) ? s_Estacoes[ i ] : NULL;
}

// Nome que o jogador vê da arma (printname do script) em maiúsculas.
static void NomeDaArma( const char *pszClasse, char *pszOut, int iTam )
{
	pszOut[ 0 ] = 0;
	if ( !pszClasse || !pszClasse[ 0 ] )
		return;
	WEAPON_FILE_INFO_HANDLE h = LookupWeaponInfoSlot( pszClasse );
	const FileWeaponInfo_t *pInfo = ( h != GetInvalidWeaponInfoHandle() ) ? GetFileWeaponInfoFromHandle( h ) : NULL;
	V_strncpy( pszOut, ( pInfo && pInfo->szPrintName[ 0 ] && pInfo->szPrintName[ 0 ] != '#' ) ? pInfo->szPrintName : pszClasse, iTam );
	V_strupr( pszOut );
}

bool CTrokHordasEstacao::Aviso( CBasePlayer *pPlayer, char *pszTexto, int iTam, bool *pbPode ) const
{
	pszTexto[ 0 ] = 0;
	*pbPode = true;
	CTrokHordasEstado *pEstado = TrokHordas_Estado();
	if ( !pPlayer || !pEstado )
		return false;
	const int iPontos = pEstado->GetPontos( pPlayer->entindex() );
	char szRotulo[ 64 ];
	V_strncpy( szRotulo, GetRotulo(), sizeof( szRotulo ) );
	V_strupr( szRotulo );

	if ( PrecisaEnergia() && !pEstado->IsEnergia() )
	{
		V_snprintf( pszTexto, iTam, "%s  —  SEM ENERGIA", szRotulo );
		*pbPode = false;
		return true;
	}

	int iPreco = GetPreco();
	switch ( GetTipo() )
	{
	case ESTACAO_PAREDE:
	{
		CBaseCombatWeapon *pTem = pPlayer->Weapon_OwnsThisType( GetArma() );
		if ( pTem )
		{
			iPreco = TrokHordas_NivelArma( pTem ) > 0 ? TrokUpgrade().iMunicaoPreco : GetPreco2();
			V_snprintf( pszTexto, iTam, "E  —  MUNIÇÃO %s: %d", szRotulo, iPreco );
		}
		else
		{
			V_snprintf( pszTexto, iTam, "E  —  COMPRAR %s: %d", szRotulo, iPreco );
		}
		break;
	}
	case ESTACAO_PORTA:
		if ( IsUsada() )
			return false;
		V_snprintf( pszTexto, iTam, "E  —  ABRIR %s: %d", szRotulo[ 0 ] ? szRotulo : "PASSAGEM", iPreco );
		break;
	case ESTACAO_UPGRADE:
	{
		CBaseCombatWeapon *pArma = pPlayer->GetActiveWeapon();
		const TrokUpgrade_t &up = TrokUpgrade();
		if ( !pArma || !pArma->UsesClipsForAmmo1() )
		{
			V_snprintf( pszTexto, iTam, "%s  —  PEGUE UMA ARMA DE FOGO", szRotulo[ 0 ] ? szRotulo : "MELHORIA" );
			*pbPode = false;
			return true;
		}
		const int iNivel = TrokHordas_NivelArma( pArma );
		char szArma[ 48 ];
		NomeDaArma( pArma->GetClassname(), szArma, sizeof( szArma ) );
		if ( iNivel >= up.nNiveis )
		{
			V_snprintf( pszTexto, iTam, "%s  —  NÍVEL MÁXIMO", szArma );
			*pbPode = false;
			return true;
		}
		iPreco = up.niveis[ iNivel + 1 ].iPreco;
		V_snprintf( pszTexto, iTam, "E  —  MELHORAR %s PARA %s: %d", szArma, up.niveis[ iNivel + 1 ].szNome, iPreco );
		break;
	}
	case ESTACAO_CAIXA:
		if ( GetEstadoCaixa() == 1 )
		{
			V_snprintf( pszTexto, iTam, "SORTEANDO..." );
			*pbPode = false;
			return true;
		}
		if ( GetEstadoCaixa() == 2 )
		{
			if ( GetDono() != pPlayer->entindex() )
				return false;
			char szArma[ 48 ];
			NomeDaArma( GetArma(), szArma, sizeof( szArma ) );
			V_snprintf( pszTexto, iTam, "E  —  PEGAR %s", szArma );
			return true;
		}
		iPreco = TrokHordas_PrecoCaixa( GetPreco() );
		V_snprintf( pszTexto, iTam, "E  —  %s: %d", szRotulo[ 0 ] ? szRotulo : "CAIXA MISTERIOSA", iPreco );
		break;
	case ESTACAO_BANCADA:
		V_snprintf( pszTexto, iTam, "E  —  %s: %d", szRotulo, iPreco );
		break;
	case ESTACAO_ENERGIA:
		if ( IsUsada() || pEstado->IsEnergia() )
			return false;
		V_snprintf( pszTexto, iTam, "E  —  LIGAR A ENERGIA" );
		return true;
	default:
		return false;
	}
	*pbPode = iPontos >= iPreco;
	return true;
}

CTrokHordasEstacao *CTrokHordasEstacao::Mirada( CBasePlayer *pPlayer )
{
	if ( !pPlayer || !pPlayer->IsAlive() || !TrokHordas_Ativo() )
		return NULL;
	const Vector vecOlho = pPlayer->EyePosition();
	Vector vecFrente;
	pPlayer->EyeVectors( &vecFrente );
	const float flDist = trok_hordas_usar_distancia.GetFloat();

	CTrokHordasEstacao *pMelhor = NULL;
	float flMelhor = FLT_MAX;
	FOR_EACH_VEC( s_Estacoes, i )
	{
		CTrokHordasEstacao *p = s_Estacoes[ i ];
		if ( !p || p->IsEffectActive( EF_NODRAW ) || p->IsDormant() )
			continue;
		Vector vecMin, vecMax;
		p->CollisionProp()->WorldSpaceAABB( &vecMin, &vecMax );
		const float d = CalcDistanceToAABB( vecMin, vecMax, vecOlho );
		if ( d > flDist || d >= flMelhor )
			continue;
		if ( !IsBoxIntersectingRay( vecMin, vecMax, vecOlho, vecFrente * ( flDist + 64.0f ), 2.0f ) )
			continue;
		flMelhor = d;
		pMelhor = p;
	}
	return pMelhor;
}

#ifndef CLIENT_DLL
//=============================================================================
// Servidor
//=============================================================================
BEGIN_DATADESC( CTrokHordasEstacao )
	DEFINE_KEYFIELD( m_iArea, FIELD_INTEGER, "area" ),
	DEFINE_KEYFIELD( m_bDesligada, FIELD_BOOLEAN, "comeca_desligada" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Ligar", InputLigar ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Desligar", InputDesligar ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Abrir", InputAbrir ),
	DEFINE_THINKFUNC( CaixaPensar ),
	DEFINE_OUTPUT( m_OnUsada, "OnUsada" ),
	DEFINE_OUTPUT( m_OnSemPontos, "OnSemPontos" ),
END_DATADESC()

bool CTrokHordasEstacao::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "arma" ) || FStrEq( szKeyName, "item" ) )
		m_iszArmaMapa = AllocPooledString( szValue );
	else if ( FStrEq( szKeyName, "rotulo" ) )
		m_iszRotuloMapa = AllocPooledString( szValue );
	else if ( FStrEq( szKeyName, "preco" ) )
		m_iPreco = MAX( 0, atoi( szValue ) );
	else if ( FStrEq( szKeyName, "preco_municao" ) )
		m_iPreco2 = MAX( 0, atoi( szValue ) );
	else if ( FStrEq( szKeyName, "precisa_energia" ) )
		m_iEnergiaMapa = atoi( szValue ) != 0 ? 1 : 0;
	else
		return BaseClass::KeyValue( szKeyName, szValue );
	return true;
}

void CTrokHordasEstacao::Precache( void )
{
	PrecacheScriptSound( "Trok.Hordas.Compra" );
	PrecacheScriptSound( "Trok.Hordas.SemPontos" );
	PrecacheScriptSound( "Trok.Hordas.Porta" );
	PrecacheScriptSound( "Trok.Hordas.Melhoria" );
	PrecacheScriptSound( "Trok.Hordas.Caixa" );
	PrecacheScriptSound( "Trok.Hordas.Energia" );
	const char *pszArma = STRING( m_iszArmaMapa );
	if ( pszArma && !Q_strncmp( pszArma, "weapon_", 7 ) )
		UTIL_PrecacheOther( pszArma );
}

void CTrokHordasEstacao::Spawn( void )
{
	const char *pszClasse = GetClassname();
	if ( FStrEq( pszClasse, "func_trok_porta_paga" ) )
		m_iTipo = ESTACAO_PORTA;
	else if ( FStrEq( pszClasse, "func_trok_maquina_upgrade" ) )
		m_iTipo = ESTACAO_UPGRADE;
	else if ( FStrEq( pszClasse, "func_trok_caixa_misteriosa" ) )
		m_iTipo = ESTACAO_CAIXA;
	else if ( FStrEq( pszClasse, "func_trok_bancada" ) )
		m_iTipo = ESTACAO_BANCADA;
	else if ( FStrEq( pszClasse, "func_trok_energia" ) )
		m_iTipo = ESTACAO_ENERGIA;
	else
		m_iTipo = ESTACAO_PAREDE;

	Precache();
	SetMoveType( MOVETYPE_PUSH );
	SetModel( STRING( GetModelName() ) );
	SetSolid( SOLID_BSP );

	// Preços padrão (design/modo-hordas.md) quando o mapa não diz.
	if ( m_iPreco <= 0 )
	{
		switch ( m_iTipo )
		{
		case ESTACAO_PAREDE: m_iPreco = 1000; break;
		case ESTACAO_PORTA: m_iPreco = 1000; break;
		case ESTACAO_CAIXA: m_iPreco = trok_hordas_caixa_preco.GetInt(); break;
		case ESTACAO_BANCADA: m_iPreco = 1500; break;
		default: break;
		}
	}
	if ( m_iTipo == ESTACAO_PAREDE && m_iPreco2 <= 0 )
		m_iPreco2 = m_iPreco / 2;			// CoD: munição da parede = metade do preço
	// A máquina de melhoria precisa de energia por padrão (como no CoD); o mapa pode tirar
	// (precisa_energia 0). As outras não precisam, a não ser que o mapa peça.
	m_bPrecisaEnergia = ( m_iEnergiaMapa >= 0 ) ? ( m_iEnergiaMapa != 0 ) : ( m_iTipo == ESTACAO_UPGRADE );

	m_szArma.GetForModify()[ 0 ] = 0;
	if ( m_iTipo == ESTACAO_PAREDE || m_iTipo == ESTACAO_BANCADA )
		V_strncpy( m_szArma.GetForModify(), STRING( m_iszArmaMapa ), 48 );

	char szRotulo[ 64 ];
	if ( m_iszRotuloMapa != NULL_STRING && STRING( m_iszRotuloMapa )[ 0 ] )
		V_strncpy( szRotulo, STRING( m_iszRotuloMapa ), sizeof( szRotulo ) );
	else if ( m_iTipo == ESTACAO_PAREDE || ( m_iTipo == ESTACAO_BANCADA && !Q_strncmp( STRING( m_iszArmaMapa ), "weapon_", 7 ) ) )
		NomeDaArma( STRING( m_iszArmaMapa ), szRotulo, sizeof( szRotulo ) );
	else if ( m_iTipo == ESTACAO_BANCADA && FStrEq( STRING( m_iszArmaMapa ), "colete" ) )
		V_strncpy( szRotulo, "COLETE", sizeof( szRotulo ) );
	else if ( m_iTipo == ESTACAO_BANCADA && FStrEq( STRING( m_iszArmaMapa ), "kit_reanimacao" ) )
		V_strncpy( szRotulo, "KIT DE REANIMAÇÃO", sizeof( szRotulo ) );
	else if ( m_iTipo == ESTACAO_UPGRADE )
		V_strncpy( szRotulo, "MÁQUINA DE MELHORIA", sizeof( szRotulo ) );
	else
		szRotulo[ 0 ] = 0;
	V_strncpy( m_szRotulo.GetForModify(), szRotulo, 48 );

	m_iEstado = 0;
	m_iDono = 0;
	m_bUsada = false;
	if ( m_bDesligada )
		AddEffects( EF_NODRAW );
}

void CTrokHordasEstacao::InputLigar( inputdata_t &inputdata )
{
	m_bDesligada = false;
	if ( !( m_iTipo == ESTACAO_PORTA && m_bUsada ) )
	{
		RemoveEffects( EF_NODRAW );
		RemoveSolidFlags( FSOLID_NOT_SOLID );
	}
}

void CTrokHordasEstacao::InputDesligar( inputdata_t &inputdata )
{
	m_bDesligada = true;
	AddEffects( EF_NODRAW );
	AddSolidFlags( FSOLID_NOT_SOLID );
}

void CTrokHordasEstacao::InputAbrir( inputdata_t &inputdata )
{
	if ( m_iTipo == ESTACAO_PORTA )
		AbrirPorta( ToBasePlayer( inputdata.pActivator ) );
	else if ( m_iTipo == ESTACAO_ENERGIA )
		LigarEnergia( ToBasePlayer( inputdata.pActivator ) );
}

void CTrokHordasEstacao::AbrirPorta( CBasePlayer *pPlayer )
{
	if ( m_bUsada )
		return;
	m_bUsada = true;
	EmitSound( "Trok.Hordas.Porta" );
	AddEffects( EF_NODRAW );
	AddSolidFlags( FSOLID_NOT_SOLID );
	TrokHordas_AbrirArea( m_iArea );
	m_OnUsada.FireOutput( pPlayer ? (CBaseEntity *)pPlayer : (CBaseEntity *)this, this );

	// Outras portas da mesma área (a mesma passagem com dois lados) abrem juntas.
	FOR_EACH_VEC( s_Estacoes, i )
	{
		CTrokHordasEstacao *p = s_Estacoes[ i ];
		if ( p != this && p->m_iTipo == ESTACAO_PORTA && !p->m_bUsada && m_iArea > 0 && p->m_iArea == m_iArea )
			p->AbrirPorta( pPlayer );
	}
}

void CTrokHordasEstacao::LigarEnergia( CBasePlayer *pPlayer )
{
	if ( m_bUsada )
		return;
	m_bUsada = true;
	EmitSound( "Trok.Hordas.Energia" );
	TrokHordas_LigarEnergia();
	m_OnUsada.FireOutput( pPlayer ? (CBaseEntity *)pPlayer : (CBaseEntity *)this, this );
}

void CTrokHordasEstacao::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	CTrokHordasEstado *pEstado = TrokHordas_Estado();
	if ( !pPlayer || !pEstado || !pPlayer->IsAlive() || m_bDesligada || gpGlobals->curtime < m_flProxUso )
		return;
	if ( pPlayer->GetTeamNumber() != TEAM_TROK_HUMANOS || TrokHordas_Caido( pPlayer ) )
		return;	// zumbis e derrubados não compram
	m_flProxUso = gpGlobals->curtime + 0.4f;

	if ( m_bPrecisaEnergia && !pEstado->IsEnergia() )
	{
		ClientPrint( pPlayer, HUD_PRINTCENTER, "SEM ENERGIA" );
		EmitSound( "Trok.Hordas.SemPontos" );
		return;
	}

	switch ( m_iTipo )
	{
	case ESTACAO_PAREDE: UsarParede( pPlayer ); break;
	case ESTACAO_PORTA:
		if ( m_bUsada )
			return;
		if ( !TrokHordas_Pagar( pPlayer, m_iPreco ) )
		{
			m_OnSemPontos.FireOutput( pPlayer, this );
			return;
		}
		AbrirPorta( pPlayer );
		break;
	case ESTACAO_UPGRADE: UsarUpgrade( pPlayer ); break;
	case ESTACAO_CAIXA: UsarCaixa( pPlayer ); break;
	case ESTACAO_BANCADA: UsarBancada( pPlayer ); break;
	case ESTACAO_ENERGIA: LigarEnergia( pPlayer ); break;
	}
}

void CTrokHordasEstacao::UsarParede( CBasePlayer *pPlayer )
{
	const char *pszArma = m_szArma.Get();
	if ( !pszArma[ 0 ] )
		return;
	CBaseCombatWeapon *pTem = pPlayer->Weapon_OwnsThisType( pszArma );
	if ( pTem )
	{
		// Já tem: munição (arma melhorada paga o preço da munição melhorada, como no CoD).
		const int iTipo = pTem->GetPrimaryAmmoType();
		if ( iTipo >= 0 && pTem->m_iClip1 >= pTem->GetMaxClip1() && pPlayer->GetAmmoCount( iTipo ) >= pTem->GetMaxClip1() * 4 )
		{
			ClientPrint( pPlayer, HUD_PRINTCENTER, "MUNIÇÃO CHEIA" );
			return;
		}
		const int iPreco = TrokHordas_NivelArma( pTem ) > 0 ? TrokUpgrade().iMunicaoPreco : m_iPreco2.Get();
		if ( !TrokHordas_Pagar( pPlayer, iPreco ) )
		{
			m_OnSemPontos.FireOutput( pPlayer, this );
			return;
		}
		TrokHordas_EncherMunicao( pPlayer, pTem );
	}
	else
	{
		if ( !TrokHordas_Pagar( pPlayer, m_iPreco ) )
		{
			m_OnSemPontos.FireOutput( pPlayer, this );
			return;
		}
		TrokHordas_DarArma( pPlayer, pszArma );
	}
	EmitSound( "Trok.Hordas.Compra" );
	m_OnUsada.FireOutput( pPlayer, this );
}

void CTrokHordasEstacao::UsarUpgrade( CBasePlayer *pPlayer )
{
	CBaseCombatWeapon *pArma = pPlayer->GetActiveWeapon();
	const TrokUpgrade_t &up = TrokUpgrade();
	if ( !pArma || !pArma->UsesClipsForAmmo1() )
		return;
	const int iNivel = TrokHordas_NivelArma( pArma );
	if ( iNivel >= up.nNiveis )
	{
		ClientPrint( pPlayer, HUD_PRINTCENTER, "NÍVEL MÁXIMO" );
		return;
	}
	if ( !TrokHordas_Pagar( pPlayer, up.niveis[ iNivel + 1 ].iPreco ) )
	{
		m_OnSemPontos.FireOutput( pPlayer, this );
		return;
	}
	if ( TrokHordas_Melhorar( pPlayer, pArma ) )
	{
		EmitSound( "Trok.Hordas.Melhoria" );
		m_OnUsada.FireOutput( pPlayer, this );
	}
}

void CTrokHordasEstacao::UsarCaixa( CBasePlayer *pPlayer )
{
	if ( m_iEstado == 1 )
		return;
	if ( m_iEstado == 2 )
	{
		if ( m_iDono != pPlayer->entindex() )
			return;
		TrokHordas_DarArma( pPlayer, m_szArma.Get() );
		EmitSound( "Trok.Hordas.Compra" );
		m_iEstado = 0;
		m_iDono = 0;
		m_szArma.GetForModify()[ 0 ] = 0;
		SetThink( NULL );
		return;
	}
	if ( !TrokHordas_Pagar( pPlayer, TrokHordas_PrecoCaixa( m_iPreco ) ) )
	{
		m_OnSemPontos.FireOutput( pPlayer, this );
		return;
	}
	++m_iUsos;
	m_iEstado = 1;
	m_iDono = pPlayer->entindex();
	m_flFimEstado = gpGlobals->curtime + MAX( 0.1f, trok_hordas_caixa_giro.GetFloat() );
	EmitSound( "Trok.Hordas.Caixa" );
	m_OnUsada.FireOutput( pPlayer, this );
	SetThink( &CTrokHordasEstacao::CaixaPensar );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CTrokHordasEstacao::CaixaPensar( void )
{
	// Preço mostrado (o bônus Caixa barata muda) e fim do giro / da oferta.
	if ( gpGlobals->curtime >= m_flFimEstado )
	{
		if ( m_iEstado == 1 )
		{
			const char *pszArma = TrokHordas_SortearArmaCaixa( UTIL_PlayerByIndex( m_iDono ) );
			if ( pszArma && pszArma[ 0 ] )
			{
				V_strncpy( m_szArma.GetForModify(), pszArma, 48 );
				m_iEstado = 2;
				m_flFimEstado = gpGlobals->curtime + MAX( 1.0f, trok_hordas_caixa_oferta.GetFloat() );
			}
			else
			{
				m_iEstado = 0;
				m_iDono = 0;
			}
		}
		else
		{
			m_iEstado = 0;
			m_iDono = 0;
			m_szArma.GetForModify()[ 0 ] = 0;
		}
	}
	if ( m_iEstado == 0 )
	{
		SetThink( NULL );
		return;
	}
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CTrokHordasEstacao::UsarBancada( CBasePlayer *pPlayer )
{
	const char *pszItem = STRING( m_iszArmaMapa );
	if ( !pszItem || !pszItem[ 0 ] )
		return;
	CTrokHordasEstado *pEstado = TrokHordas_Estado();

	if ( FStrEq( pszItem, "colete" ) )
	{
		if ( pPlayer->ArmorValue() >= trok_hordas_colete.GetInt() )
		{
			ClientPrint( pPlayer, HUD_PRINTCENTER, "COLETE CHEIO" );
			return;
		}
		if ( !TrokHordas_Pagar( pPlayer, m_iPreco ) )
			return;
		pPlayer->SetArmorValue( trok_hordas_colete.GetInt() );
	}
	else if ( FStrEq( pszItem, "kit_reanimacao" ) )
	{
		if ( pEstado && pEstado->TemKit( pPlayer->entindex() ) )
		{
			ClientPrint( pPlayer, HUD_PRINTCENTER, "VOCÊ JÁ TEM UM KIT" );
			return;
		}
		if ( !TrokHordas_Pagar( pPlayer, m_iPreco ) )
			return;
		TrokHordas_DarKit( pPlayer );
	}
	else if ( !Q_strncmp( pszItem, "weapon_", 7 ) )
	{
		CBaseCombatWeapon *pTem = pPlayer->Weapon_OwnsThisType( pszItem );
		if ( !TrokHordas_Pagar( pPlayer, m_iPreco ) )
			return;
		if ( pTem )
			TrokHordas_EncherMunicao( pPlayer, pTem );
		else
			TrokHordas_DarArma( pPlayer, pszItem );
	}
	else
	{
		Warning( "Trok: func_trok_bancada com item desconhecido '%s'.\n", pszItem );
		return;
	}
	EmitSound( "Trok.Hordas.Compra" );
	m_OnUsada.FireOutput( pPlayer, this );
}
#endif // !CLIENT_DLL

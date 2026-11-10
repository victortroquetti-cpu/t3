//=============================================================================
// Trok: tipos de zumbi. Ver trok_zumbis.h e scripts/trok_zumbis.txt.
//=============================================================================

#include "cbase.h"
#include "filesystem.h"
#include "trok_zumbis.h"
#include "trok_surto.h"
#include "trok_hordas.h"

#ifndef CLIENT_DLL
#include "explode.h"
#include "IEffects.h"
#include "hl2_player.h"
#include "trok_gamerules.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar trok_zumbis_tipos( "trok_zumbis_tipos", "1", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Surto: jogadores escolhem o tipo de zumbi (trok_zumbi_classe). 0 = todos normais." );
// Fúria (a moeda do infectado; decisão do dono). Conta de equilíbrio (design/modo-surto.md, "Tipos de zumbi"): numa partida
// de 7 rodadas, um infectado médio junta ~3000–4000 e um bom passa de 6000; o tipo mais caro
// (Crusher, 3500) sai uma ou duas vezes por partida para quem joga bem.
ConVar trok_surto_inf_infectar( "trok_surto_inf_infectar", "250", FCVAR_NOTIFY, "Fúria (moeda do infectado) por humano infectado." );
ConVar trok_surto_inf_dano( "trok_surto_inf_dano", "0.5", FCVAR_NOTIFY, "Fúria por ponto de dano em humano que não infecta (explosão, nuvem, aura)." );
ConVar trok_surto_inf_vitoria( "trok_surto_inf_vitoria", "400", FCVAR_NOTIFY, "Fúria para cada zumbi quando os zumbis vencem a rodada." );
ConVar trok_surto_inf_derrota( "trok_surto_inf_derrota", "150", FCVAR_NOTIFY, "Fúria para cada zumbi quando os humanos vencem a rodada." );
ConVar trok_surto_inf_inicial( "trok_surto_inf_inicial", "0", FCVAR_NOTIFY, "Fúria no começo do mapa." );
ConVar trok_surto_inf_max( "trok_surto_inf_max", "10000", FCVAR_NOTIFY, "Teto da Fúria." );
ConVar trok_zumbis_gas_letal( "trok_zumbis_gas_letal", "0", FCVAR_NOTIFY,
	"0 = gás (nuvem tóxica e aura) só fere e nunca mata, como no CA; 1 = pode matar (quem morre assim sai da rodada)." );
ConVar trok_zumbis_arquivo( "trok_zumbis_arquivo", "scripts/trok_zumbis.txt", FCVAR_REPLICATED,
	"Arquivo com os tipos de zumbi." );

static TrokZumbiClasse_t s_aClasses[ TROK_ZUMBIS_MAX ];
static int s_iNumClasses = 0;
static bool s_bCarregado = false;

static void TrokZumbis_Padrao( TrokZumbiClasse_t &c, const char *pszNome )
{
	memset( &c, 0, sizeof( c ) );
	Q_strncpy( c.szNome, pszNome, sizeof( c.szNome ) );
	Q_strncpy( c.szTitulo, pszNome, sizeof( c.szTitulo ) );
	c.bJogavel = true;
	c.iPreco = 0;
	c.flVidaHospedeiro = -1.0f;
	c.flVida = -1.0f;
	c.flVelocidade = 1.0f;
	c.flRegen = -1.0f;
	c.flGarraDano = c.flGarraAlcance = c.flGarraCadencia = 1.0f;
	c.flEscala = 1.0f;
	c.iCor[ 0 ] = c.iCor[ 1 ] = c.iCor[ 2 ] = -1;
	c.flBonusHumanoVelocidade = 1.0f;
}

static void TrokZumbis_Carregar( void )
{
	s_bCarregado = true;
	s_iNumClasses = 0;

	KeyValues *pKV = new KeyValues( "TrokZumbis" );
	if ( pKV->LoadFromFile( filesystem, trok_zumbis_arquivo.GetString(), "MOD" ) )
	{
		for ( KeyValues *pSub = pKV->GetFirstTrueSubKey(); pSub && s_iNumClasses < TROK_ZUMBIS_MAX; pSub = pSub->GetNextTrueSubKey() )
		{
			TrokZumbiClasse_t &c = s_aClasses[ s_iNumClasses++ ];
			TrokZumbis_Padrao( c, pSub->GetName() );
			Q_strncpy( c.szTitulo, pSub->GetString( "titulo", pSub->GetName() ), sizeof( c.szTitulo ) );
			c.bJogavel = pSub->GetInt( "jogavel", 1 ) != 0;
			c.iPreco = MAX( 0, pSub->GetInt( "preco", 0 ) );
			c.flVidaHospedeiro = pSub->GetFloat( "vida_hospedeiro", -1.0f );
			c.flVida = pSub->GetFloat( "vida", -1.0f );
			c.flVelocidade = pSub->GetFloat( "velocidade", 1.0f );
			c.bFolegoInfinito = pSub->GetInt( "folego_infinito", 0 ) != 0;
			c.flRegen = pSub->GetFloat( "regen", -1.0f );
			c.flGarraDano = pSub->GetFloat( "garra_dano", 1.0f );
			c.flGarraAlcance = pSub->GetFloat( "garra_alcance", 1.0f );
			c.flGarraCadencia = pSub->GetFloat( "garra_cadencia", 1.0f );
			c.flEscala = pSub->GetFloat( "escala", 1.0f );
			const char *pszCor = pSub->GetString( "cor", NULL );
			if ( pszCor )
				sscanf( pszCor, "%d %d %d", &c.iCor[ 0 ], &c.iCor[ 1 ], &c.iCor[ 2 ] );
			c.flExplodeDano = pSub->GetFloat( "explode_dano", 0.0f );
			c.flExplodeRaio = pSub->GetFloat( "explode_raio", 0.0f );
			c.flNuvemDps = pSub->GetFloat( "nuvem_dps", 0.0f );
			c.flNuvemRaio = pSub->GetFloat( "nuvem_raio", 0.0f );
			c.flNuvemTempo = pSub->GetFloat( "nuvem_tempo", 0.0f );
			c.flNuvemLentidao = pSub->GetFloat( "nuvem_lentidao", 1.0f );
			c.flAuraDps = pSub->GetFloat( "aura_dps", 0.0f );
			c.flAuraRaio = pSub->GetFloat( "aura_raio", 0.0f );
			c.flBombaIntervalo = pSub->GetFloat( "bomba_intervalo", 0.0f );
			c.flBombaDano = pSub->GetFloat( "bomba_dano", 0.0f );
			c.flBombaRaio = pSub->GetFloat( "bomba_raio", 0.0f );
			c.flBombaPavio = pSub->GetFloat( "bomba_pavio", 1.5f );
			c.flGolpeLentidao = pSub->GetFloat( "golpe_lentidao", 1.0f );
			c.flGolpeLentidaoTempo = pSub->GetFloat( "golpe_lentidao_tempo", 0.0f );
			c.flBonusHumanoVelocidade = pSub->GetFloat( "bonus_humano_velocidade", 1.0f );
		}
	}
	else
	{
		Warning( "Trok: não achei %s; só o zumbi normal.\n", trok_zumbis_arquivo.GetString() );
	}
	pKV->deleteThis();

	if ( s_iNumClasses == 0 )
		TrokZumbis_Padrao( s_aClasses[ s_iNumClasses++ ], "normal" );
}

CON_COMMAND_F( trok_zumbis_recarregar, "Lê de novo o arquivo dos tipos de zumbi.", FCVAR_CHEAT )
{
	TrokZumbis_Carregar();
	Msg( "Trok: %d tipos de zumbi.\n", s_iNumClasses );
}

int TrokZumbis_Num( void )
{
	if ( !s_bCarregado )
		TrokZumbis_Carregar();
	return s_iNumClasses;
}

const TrokZumbiClasse_t &TrokZumbis_Classe( int i )
{
	const int n = TrokZumbis_Num();
	return s_aClasses[ clamp( i, 0, n - 1 ) ];
}

int TrokZumbis_Achar( const char *pszNome )
{
	if ( !pszNome || !pszNome[ 0 ] )
		return -1;
	const int n = TrokZumbis_Num();
	if ( pszNome[ 0 ] >= '0' && pszNome[ 0 ] <= '9' )
	{
		const int i = atoi( pszNome );
		return ( i >= 0 && i < n ) ? i : -1;
	}
	for ( int i = 0; i < n; ++i )
		if ( !Q_stricmp( s_aClasses[ i ].szNome, pszNome ) )
			return i;
	return -1;
}

//-----------------------------------------------------------------------------
// Leitura do estado (cliente e servidor: as garras e a velocidade são previstas)
//-----------------------------------------------------------------------------
int TrokZumbis_ClasseDe( const CBaseEntity *pPlayer )
{
	CTrokSurtoEstado *pEstado = TrokSurto_Estado();
	if ( !pEstado || !TrokSurto_IsZumbi( pPlayer ) )
		return 0;
	return clamp( pEstado->GetClasseZumbi( pPlayer->entindex() ), 0, TrokZumbis_Num() - 1 );
}

int TrokZumbis_EscolhaDe( const CBaseEntity *pPlayer )
{
	CTrokSurtoEstado *pEstado = TrokSurto_Estado();
	if ( !pEstado || !pPlayer )
		return 0;
	return clamp( pEstado->GetEscolhaZumbi( pPlayer->entindex() ), 0, TrokZumbis_Num() - 1 );
}

float TrokZumbis_VelocidadeDe( const CBaseEntity *pPlayer )
{
	CTrokSurtoEstado *pEstado = TrokSurto_Estado();
	if ( !pEstado || !pPlayer || !TrokModoZumbi_Ativo() )
		return 1.0f;
	if ( TrokSurto_IsZumbi( pPlayer ) )	// no Hordas, x a velocidade da rodada (trok_hordas.cpp)
		return TrokZumbis_Classe( TrokZumbis_ClasseDe( pPlayer ) ).flVelocidade * TrokHordas_VelocidadeZumbi( pPlayer );

	// Humano: bônus do mutagênico escolhido e lentidão (golpe que paralisa, nuvem tóxica).
	float f = 1.0f;
	if ( trok_zumbis_tipos.GetBool() )
		f *= TrokZumbis_Classe( TrokZumbis_EscolhaDe( pPlayer ) ).flBonusHumanoVelocidade;
	const int i = pPlayer->entindex();
	if ( gpGlobals->curtime < pEstado->GetLentoAte( i ) )
		f *= clamp( pEstado->GetLentoFator( i ), 0.05f, 1.0f );
	return f;
}

int TrokZumbis_Dinheiro( const CBaseEntity *pPlayer )
{
	CTrokSurtoEstado *pEstado = TrokSurto_Estado();
	return ( pEstado && pPlayer ) ? pEstado->GetDinheiroInfectado( pPlayer->entindex() ) : 0;
}

float TrokZumbis_GarraDano( const CBaseEntity *pOwner ) { return TrokZumbis_Classe( TrokZumbis_ClasseDe( pOwner ) ).flGarraDano; }
float TrokZumbis_GarraAlcance( const CBaseEntity *pOwner ) { return TrokZumbis_Classe( TrokZumbis_ClasseDe( pOwner ) ).flGarraAlcance; }
float TrokZumbis_GarraCadencia( const CBaseEntity *pOwner ) { return TrokZumbis_Classe( TrokZumbis_ClasseDe( pOwner ) ).flGarraCadencia; }

#ifdef CLIENT_DLL
// Escolha rápida pela tecla (padrão N em cfg/config_default.cfg): passa para o próximo tipo jogável.
CON_COMMAND( trok_zumbi_classe_prox, "Surto: passa para o próximo tipo de zumbi (vale quando você virar zumbi)." )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	const int n = TrokZumbis_Num();
	if ( !pLocal || n <= 1 )
		return;
	int i = TrokZumbis_EscolhaDe( pLocal );
	for ( int k = 0; k < n; ++k )
	{
		i = ( i + 1 ) % n;
		if ( TrokZumbis_Classe( i ).bJogavel )
			break;
	}
	char szCmd[ 64 ];
	V_snprintf( szCmd, sizeof( szCmd ), "trok_zumbi_classe %s\n", TrokZumbis_Classe( i ).szNome );
	engine->ClientCmd( szCmd );
}
#endif

#ifndef CLIENT_DLL
//=============================================================================
// Servidor
//=============================================================================
static unsigned int s_aLiberado[ MAX_PLAYERS + 1 ];
static float s_aProxBomba[ MAX_PLAYERS + 1 ];
static float s_flProxAura = 0.0f;

// Dano de gás: no CA só fere (não mata na hora); morte por gás não infectaria mesmo.
static void TrokZumbis_DanoGas( CBasePlayer *pVitima, CBaseEntity *pInflictor, CBaseEntity *pAtacante, float flDano )
{
	if ( !pVitima || flDano <= 0.0f )
		return;
	if ( !trok_zumbis_gas_letal.GetBool() )
		flDano = MIN( flDano, (float)( pVitima->GetHealth() - 1 ) );
	if ( flDano < 1.0f )
		return;
	CTakeDamageInfo info( pInflictor, pAtacante, flDano, DMG_NERVEGAS );
	pVitima->TakeDamage( info );
}

static void TrokZumbis_Lento( CBasePlayer *pPlayer, float flFator, float flTempo )
{
	CTrokSurtoEstado *pEstado = TrokSurto_Estado();
	if ( !pEstado || !pPlayer || flTempo <= 0.0f || flFator >= 1.0f )
		return;
	const int i = pPlayer->entindex();
	const bool bJaLento = gpGlobals->curtime < pEstado->m_aLentoAte[ i ];
	pEstado->m_aLentoFator.Set( i, bJaLento ? MIN( flFator, (float)pEstado->m_aLentoFator[ i ] ) : flFator );
	const float flAte = gpGlobals->curtime + flTempo;
	if ( flAte > pEstado->m_aLentoAte[ i ] )
		pEstado->m_aLentoAte.Set( i, flAte );
}

bool TrokZumbis_Permitido( CBasePlayer *pPlayer, int iClasse )
{
	if ( !pPlayer || iClasse < 0 || iClasse >= TrokZumbis_Num() )
		return false;
	const TrokZumbiClasse_t &c = TrokZumbis_Classe( iClasse );
	if ( iClasse == 0 )
		return true;
	return c.bJogavel && trok_zumbis_tipos.GetBool();
}

static bool TrokZumbis_Gratis( CBasePlayer *pPlayer, int iClasse )
{
	return iClasse >= 0 && iClasse < 32 && ( s_aLiberado[ pPlayer->entindex() ] & ( 1u << iClasse ) ) != 0;
}

void TrokZumbis_Ganhar( CBasePlayer *pPlayer, int iValor )
{
	CTrokSurtoEstado *pEstado = TrokSurto_Estado();
	if ( !pEstado || !pPlayer || iValor == 0 )
		return;
	const int i = pPlayer->entindex();
	pEstado->m_aDinheiroInf.Set( i, clamp( pEstado->m_aDinheiroInf[ i ] + iValor, 0, MAX( 0, trok_surto_inf_max.GetInt() ) ) );
}

void TrokZumbis_Infectou( CBasePlayer *pZumbi )
{
	TrokZumbis_Ganhar( pZumbi, trok_surto_inf_infectar.GetInt() );
}

void TrokZumbis_FimRodada( bool bZumbisVenceram )
{
	const int iValor = bZumbisVenceram ? trok_surto_inf_vitoria.GetInt() : trok_surto_inf_derrota.GetInt();
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *p = UTIL_PlayerByIndex( i );
		if ( p && p->GetTeamNumber() == TEAM_TROK_ZUMBIS )
			TrokZumbis_Ganhar( p, iValor );
	}
}

void TrokZumbis_Liberar( CBasePlayer *pPlayer, int iClasse )
{
	if ( pPlayer && iClasse >= 0 && iClasse < 32 )
		s_aLiberado[ pPlayer->entindex() ] |= ( 1u << iClasse );
}

bool TrokZumbis_ClientCommand( CBasePlayer *pPlayer, const CCommand &args )
{
	if ( !pPlayer || !FStrEq( args[ 0 ], "trok_zumbi_classe" ) )
		return false;
	CTrokSurtoEstado *pEstado = TrokSurto_Estado();
	if ( !pEstado )
		return true;
	if ( args.ArgC() < 2 )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Tipos de zumbi:\n" );
		for ( int i = 0; i < TrokZumbis_Num(); ++i )
		{
			const TrokZumbiClasse_t &c = TrokZumbis_Classe( i );
			if ( c.bJogavel || i == 0 )
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "  %s (%s) - %d%s\n", c.szNome, c.szTitulo, c.iPreco, TrokZumbis_Permitido( pPlayer, i ) ? "" : " - bloqueado" ) );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "Sua Fúria: %d\n", TrokZumbis_Dinheiro( pPlayer ) ) );
		}
		return true;
	}
	const int i = TrokZumbis_Achar( args[ 1 ] );
	if ( i < 0 || !TrokZumbis_Permitido( pPlayer, i ) )
	{
		ClientPrint( pPlayer, HUD_PRINTCENTER, "Tipo de zumbi bloqueado." );
		return true;
	}
	pEstado->m_aEscolhaZumbi.Set( pPlayer->entindex(), i );
	const TrokZumbiClasse_t &c = TrokZumbis_Classe( i );
	if ( c.iPreco > 0 && !TrokZumbis_Gratis( pPlayer, i ) )
		ClientPrint( pPlayer, HUD_PRINTCENTER, UTIL_VarArgs( "Quando virar zumbi: %s (%d de %d de Fúria)", c.szTitulo, c.iPreco, TrokZumbis_Dinheiro( pPlayer ) ) );
	else
		ClientPrint( pPlayer, HUD_PRINTCENTER, UTIL_VarArgs( "Quando virar zumbi: %s", c.szTitulo ) );
	return true;
}

int TrokZumbis_AoVirar( CBasePlayer *pPlayer )
{
	CTrokSurtoEstado *pEstado = TrokSurto_Estado();
	if ( !pEstado || !pPlayer )
		return 0;
	const int i = pPlayer->entindex();
	int iClasse = pEstado->GetEscolhaZumbi( i );
	if ( !TrokZumbis_Permitido( pPlayer, iClasse ) )
		iClasse = 0;
	// Compra: paga o preço com o dinheiro de infectado; sem dinheiro, vira o normal.
	const int iPreco = TrokZumbis_Classe( iClasse ).iPreco;
	if ( iClasse != 0 && iPreco > 0 && !TrokZumbis_Gratis( pPlayer, iClasse ) )
	{
		if ( pEstado->m_aDinheiroInf[ i ] >= iPreco )
			pEstado->m_aDinheiroInf.Set( i, pEstado->m_aDinheiroInf[ i ] - iPreco );
		else
		{
			ClientPrint( pPlayer, HUD_PRINTCENTER, UTIL_VarArgs( "Fúria insuficiente para %s (%d de %d)",
				TrokZumbis_Classe( iClasse ).szTitulo, (int)pEstado->m_aDinheiroInf[ i ], iPreco ) );
			iClasse = 0;
		}
	}
	pEstado->m_aClasseZumbi.Set( i, iClasse );
	s_aProxBomba[ i ] = gpGlobals->curtime + TrokZumbis_Classe( iClasse ).flBombaIntervalo;
	return iClasse;
}

// Modo Hordas: o diretor escolhe o tipo (sem escolha nem compra do jogador).
void TrokZumbis_DefinirClasse( CBasePlayer *pPlayer, int iClasse )
{
	CTrokSurtoEstado *pEstado = TrokSurto_Estado();
	if ( !pEstado || !pPlayer )
		return;
	iClasse = clamp( iClasse, 0, TrokZumbis_Num() - 1 );
	pEstado->m_aClasseZumbi.Set( pPlayer->entindex(), iClasse );
	s_aProxBomba[ pPlayer->entindex() ] = gpGlobals->curtime + TrokZumbis_Classe( iClasse ).flBombaIntervalo;
}

void TrokZumbis_AoVirarHumano( CBasePlayer *pPlayer )
{
	CTrokSurtoEstado *pEstado = TrokSurto_Estado();
	if ( pEstado && pPlayer )
	{
		pEstado->m_aClasseZumbi.Set( pPlayer->entindex(), 0 );
		pEstado->m_aLentoAte.Set( pPlayer->entindex(), 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Bolha que explode (Striker no cooperativo) e explosão de morte (Charger)
//-----------------------------------------------------------------------------
static void TrokZumbis_Explodir( CBaseEntity *pDono, const Vector &vecPos, float flDano, float flRaio )
{
	if ( flDano <= 0.0f || flRaio <= 0.0f )
		return;
	ExplosionCreate( vecPos, vec3_angle, pDono, (int)flDano, (int)flRaio, false );
	CTakeDamageInfo info( pDono, pDono, flDano, DMG_BLAST );
	RadiusDamage( info, vecPos, flRaio, CLASS_NONE, NULL );	// cobertura do CA (CTrokRules::RadiusDamage)
}

class CTrokBolhaZumbi : public CBaseAnimating
{
public:
	DECLARE_CLASS( CTrokBolhaZumbi, CBaseAnimating );
	DECLARE_DATADESC();

	float m_flDano, m_flRaio;

	virtual void Precache( void )
	{
		PrecacheModel( "models/weapons/w_grenade.mdl" );
	}
	virtual void Spawn( void )
	{
		Precache();
		SetModel( "models/weapons/w_grenade.mdl" );
		SetMoveType( MOVETYPE_FLYGRAVITY );
		SetSolid( SOLID_NONE );
		SetRenderColor( 200, 255, 120 );
	}
	void Explodir( void )
	{
		TrokZumbis_Explodir( GetOwnerEntity(), GetAbsOrigin() + Vector( 0, 0, 4 ), m_flDano, m_flRaio );
		UTIL_Remove( this );
	}
};

BEGIN_DATADESC( CTrokBolhaZumbi )
	DEFINE_THINKFUNC( Explodir ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( trok_bolha_zumbi, CTrokBolhaZumbi );

//-----------------------------------------------------------------------------
// Nuvem tóxica (Splicer e Crusher ao morrer): dano por segundo e lentidão em quem estiver dentro
//-----------------------------------------------------------------------------
class CTrokNuvemToxica : public CBaseEntity
{
public:
	DECLARE_CLASS( CTrokNuvemToxica, CBaseEntity );
	DECLARE_DATADESC();

	float m_flDps, m_flRaio, m_flFim, m_flLentidao, m_flProxFumaca;
	int m_iTimeDono;

	virtual void Spawn( void )
	{
		SetSolid( SOLID_NONE );
		SetMoveType( MOVETYPE_NONE );
		m_flProxFumaca = 0.0f;
		SetThink( &CTrokNuvemToxica::Pensar );
		SetNextThink( gpGlobals->curtime + 0.05f );
	}
	void Pensar( void )
	{
		const float flPasso = 0.25f;
		if ( gpGlobals->curtime >= m_flFim )
		{
			UTIL_Remove( this );
			return;
		}
		if ( gpGlobals->curtime >= m_flProxFumaca )
		{
			g_pEffects->Smoke( GetAbsOrigin(), g_sModelIndexSmoke, m_flRaio * 0.08f, 6.0f );
			m_flProxFumaca = gpGlobals->curtime + 0.6f;
		}
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *p = UTIL_PlayerByIndex( i );
			if ( !p || !p->IsAlive() || p->GetTeamNumber() == m_iTimeDono )
				continue;
			if ( ( p->WorldSpaceCenter() - GetAbsOrigin() ).LengthSqr() > m_flRaio * m_flRaio )
				continue;
			TrokZumbis_DanoGas( p, this, GetOwnerEntity() ? GetOwnerEntity() : this, m_flDps * flPasso );
			TrokZumbis_Lento( p, m_flLentidao, 0.5f );
		}
		SetNextThink( gpGlobals->curtime + flPasso );
	}
};

BEGIN_DATADESC( CTrokNuvemToxica )
	DEFINE_THINKFUNC( Pensar ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( trok_nuvem_toxica, CTrokNuvemToxica );

void TrokZumbis_AoMorrer( CBasePlayer *pVictim )
{
	if ( !pVictim || !TrokSurto_IsZumbi( pVictim ) )
		return;
	const TrokZumbiClasse_t &c = TrokZumbis_Classe( TrokZumbis_ClasseDe( pVictim ) );
	const Vector vecPos = pVictim->WorldSpaceCenter();

	if ( c.flExplodeDano > 0.0f )
		TrokZumbis_Explodir( pVictim, vecPos, c.flExplodeDano, c.flExplodeRaio );

	if ( c.flNuvemDps > 0.0f && c.flNuvemRaio > 0.0f && c.flNuvemTempo > 0.0f )
	{
		CTrokNuvemToxica *pNuvem = (CTrokNuvemToxica *)CreateEntityByName( "trok_nuvem_toxica" );
		if ( pNuvem )
		{
			pNuvem->m_flDps = c.flNuvemDps;
			pNuvem->m_flRaio = c.flNuvemRaio;
			pNuvem->m_flFim = gpGlobals->curtime + c.flNuvemTempo;
			pNuvem->m_flLentidao = c.flNuvemLentidao;
			pNuvem->m_iTimeDono = pVictim->GetTeamNumber();
			pNuvem->SetAbsOrigin( vecPos );
			pNuvem->SetOwnerEntity( pVictim );
			DispatchSpawn( pNuvem );
		}
	}
}

void TrokZumbis_AoAcertar( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	CBasePlayer *pAttacker = ToBasePlayer( info.GetAttacker() );
	if ( !pVictim || !pAttacker || !TrokSurto_IsZumbi( pAttacker ) || TrokSurto_IsZumbi( pVictim ) )
		return;
	// Dano em humano que não infectou na hora (explosão, nuvem, aura, golpe fraco) rende um pouco.
	if ( pVictim->GetHealth() > 0 )
		TrokZumbis_Ganhar( pAttacker, (int)( info.GetDamage() * trok_surto_inf_dano.GetFloat() ) );
	const TrokZumbiClasse_t &c = TrokZumbis_Classe( TrokZumbis_ClasseDe( pAttacker ) );
	TrokZumbis_Lento( pVictim, c.flGolpeLentidao, c.flGolpeLentidaoTempo );
}

void TrokZumbis_Think( void )
{
	const float flNow = gpGlobals->curtime;
	const bool bAura = flNow >= s_flProxAura;
	if ( bAura )
		s_flProxAura = flNow + 0.5f;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer || !pPlayer->IsAlive() || !TrokSurto_IsZumbi( pPlayer ) )
			continue;
		const TrokZumbiClasse_t &c = TrokZumbis_Classe( TrokZumbis_ClasseDe( pPlayer ) );

		// Fôlego infinito (Striker).
		if ( c.bFolegoInfinito )
		{
			CHL2_Player *pHL2 = dynamic_cast< CHL2_Player * >( pPlayer );
			if ( pHL2 )
				pHL2->SuitPower_Charge( 100.0f );
		}

		// Aura tóxica em volta (Crusher no cooperativo).
		if ( bAura && c.flAuraDps > 0.0f && c.flAuraRaio > 0.0f )
		{
			for ( int j = 1; j <= gpGlobals->maxClients; ++j )
			{
				CBasePlayer *p = UTIL_PlayerByIndex( j );
				if ( !p || !p->IsAlive() || !TrokSurto_IsHumano( p ) )
					continue;
				if ( ( p->WorldSpaceCenter() - pPlayer->WorldSpaceCenter() ).LengthSqr() > c.flAuraRaio * c.flAuraRaio )
					continue;
				TrokZumbis_DanoGas( p, pPlayer, pPlayer, c.flAuraDps * 0.5f );
			}
		}

		// Bolhas que explodem (Striker no cooperativo).
		if ( c.flBombaIntervalo > 0.0f && c.flBombaDano > 0.0f && flNow >= s_aProxBomba[ i ] )
		{
			s_aProxBomba[ i ] = flNow + c.flBombaIntervalo;
			CTrokBolhaZumbi *pBolha = (CTrokBolhaZumbi *)CreateEntityByName( "trok_bolha_zumbi" );
			if ( pBolha )
			{
				pBolha->m_flDano = c.flBombaDano;
				pBolha->m_flRaio = c.flBombaRaio;
				pBolha->SetAbsOrigin( pPlayer->GetAbsOrigin() + Vector( 0, 0, 40 ) );
				pBolha->SetOwnerEntity( pPlayer );
				DispatchSpawn( pBolha );
				pBolha->SetThink( &CTrokBolhaZumbi::Explodir );
				pBolha->SetNextThink( flNow + MAX( 0.1f, c.flBombaPavio ) );
			}
		}
	}
}
#endif // !CLIENT_DLL

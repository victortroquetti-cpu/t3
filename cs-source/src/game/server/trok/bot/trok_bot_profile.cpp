//=============================================================================
// Trok: perfis dos bots. Ver trok_bot_profile.h.
//=============================================================================

#include "cbase.h"
#include "trok_bot_profile.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "utlstring.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar trok_bot_estilos( "trok_bot_estilos", "equilibrado 30 entry 20 lurker 15 ancora 20 suporte 15", FCVAR_GAMEDLL,
	"Pesos do sorteio de estilo dos bots (vale para os próximos bots)." );
static ConVar trok_bot_estilo( "trok_bot_estilo", "", FCVAR_GAMEDLL,
	"Força um estilo para todos os próximos bots (equilibrado, entry, lurker, ancora, suporte). Vazio = sorteio." );

static const char *s_pszStyleNames[ TROK_ESTILO_COUNT ] = { "equilibrado", "entry", "lurker", "ancora", "suporte" };

const char *TrokBot_StyleName( int iStyle )
{
	return ( iStyle >= 0 && iStyle < TROK_ESTILO_COUNT ) ? s_pszStyleNames[ iStyle ] : "?";
}

static int StyleFromName( const char *pszName )
{
	for ( int i = 0; i < TROK_ESTILO_COUNT; ++i )
	{
		if ( !Q_stricmp( pszName, s_pszStyleNames[ i ] ) )
			return i;
	}
	return -1;
}

// Sorteio ponderado a partir de "nome peso nome peso ...".
static int PickStyle( void )
{
	int iForced = StyleFromName( trok_bot_estilo.GetString() );
	if ( iForced >= 0 )
		return iForced;

	float flWeights[ TROK_ESTILO_COUNT ] = {};
	float flTotal = 0.0f;

	CUtlVector< CUtlString > parts;
	V_SplitString( trok_bot_estilos.GetString(), " ", parts );
	for ( int i = 0; i + 1 < parts.Count(); i += 2 )
	{
		int iStyle = StyleFromName( parts[ i ] );
		float flWeight = atof( parts[ i + 1 ] );
		if ( iStyle >= 0 && flWeight > 0.0f )
		{
			flWeights[ iStyle ] += flWeight;
			flTotal += flWeight;
		}
	}

	if ( flTotal <= 0.0f )
		return TROK_ESTILO_EQUILIBRADO;

	float flRoll = RandomFloat( 0.0f, flTotal );
	for ( int i = 0; i < TROK_ESTILO_COUNT; ++i )
	{
		flRoll -= flWeights[ i ];
		if ( flRoll <= 0.0f && flWeights[ i ] > 0.0f )
			return i;
	}
	return TROK_ESTILO_EQUILIBRADO;
}

static ConVar trok_bot_personalidade( "trok_bot_personalidade", "0.12", FCVAR_GAMEDLL,
	"Variação sorteada por bot em cima do nível (0.12 = até 12% para mais ou para menos).", true, 0, true, 0.5 );

#define TROK_BOT_LEVELS 4

// Valores padrão (usados se scripts/trok_bot_perfis.txt não existir ou não tiver o campo).
// Base: tempo de reação visual humano ~0.2-0.3 s; jogadores fracos passam de 0.4 s.
static const TrokBotProfile_t s_DefaultProfiles[ TROK_BOT_LEVELS ] =
{
	//  nível      reação         perc  campo  aud   erro0  erroMin meiaV  rastr  ferido cabeça giro  interv  strafe      agach  agress tático
	{ "facil",    0.45f, 0.70f,   0.60f, 100,  0.6f,  9.0f,  2.5f,   0.60f, 4.0f,  6.0f,  0.05f, 180,  0.45f,  0.0f, 0.0f, 0.00f, 0.60f, 0.10f },
	{ "normal",   0.30f, 0.45f,   0.85f, 110,  0.8f,  6.0f,  1.5f,   0.40f, 3.0f,  4.0f,  0.15f, 300,  0.25f,  0.5f, 1.1f, 0.05f, 0.50f, 0.40f },
	{ "dificil",  0.22f, 0.32f,   1.00f, 120,  1.0f,  4.0f,  0.9f,   0.28f, 2.0f,  3.0f,  0.30f, 450,  0.15f,  0.35f, 0.8f, 0.12f, 0.50f, 0.70f },
	{ "expert",   0.17f, 0.25f,   1.15f, 130,  1.1f,  3.0f,  0.6f,   0.20f, 1.5f,  2.5f,  0.45f, 600,  0.10f,  0.25f, 0.6f, 0.18f, 0.50f, 0.90f },
};

static TrokBotProfile_t s_Profiles[ TROK_BOT_LEVELS ];
static bool s_bLoaded = false;

static void ReadFloat( KeyValues *pKV, const char *pszKey, float *pValue )
{
	if ( pKV->FindKey( pszKey ) )
		*pValue = pKV->GetFloat( pszKey, *pValue );
}

void TrokBot_ReloadProfiles( void )
{
	memcpy( s_Profiles, s_DefaultProfiles, sizeof( s_Profiles ) );
	s_bLoaded = true;

	KeyValues *pFile = new KeyValues( "TrokBotPerfis" );
	if ( !pFile->LoadFromFile( filesystem, "scripts/trok_bot_perfis.txt", "GAME" ) )
	{
		DevMsg( "[Trok] scripts/trok_bot_perfis.txt não encontrado; usando os perfis padrão.\n" );
		pFile->deleteThis();
		return;
	}

	for ( int i = 0; i < TROK_BOT_LEVELS; ++i )
	{
		KeyValues *pLevel = pFile->FindKey( s_Profiles[ i ].szNivel );
		if ( !pLevel )
			continue;

		TrokBotProfile_t &p = s_Profiles[ i ];
		ReadFloat( pLevel, "reacao_min", &p.flReacaoMin );
		ReadFloat( pLevel, "reacao_max", &p.flReacaoMax );
		ReadFloat( pLevel, "ganho_percepcao", &p.flGanhoPercepcao );
		ReadFloat( pLevel, "campo_visao", &p.flCampoVisao );
		ReadFloat( pLevel, "audicao", &p.flAudicao );
		ReadFloat( pLevel, "mira_erro_inicial", &p.flMiraErroInicial );
		ReadFloat( pLevel, "mira_erro_minimo", &p.flMiraErroMinimo );
		ReadFloat( pLevel, "mira_meia_vida", &p.flMiraMeiaVida );
		ReadFloat( pLevel, "mira_rastreio", &p.flMiraRastreio );
		ReadFloat( pLevel, "mira_tremor_ferido", &p.flMiraTremorFerido );
		ReadFloat( pLevel, "chance_cabeca", &p.flChanceCabeca );
		ReadFloat( pLevel, "giro_max", &p.flGiroMax );
		ReadFloat( pLevel, "intervalo_rastreio", &p.flIntervaloRastreio );
		ReadFloat( pLevel, "strafe_min", &p.flStrafeMin );
		ReadFloat( pLevel, "strafe_max", &p.flStrafeMax );
		ReadFloat( pLevel, "chance_agachar", &p.flChanceAgachar );
		ReadFloat( pLevel, "agressividade", &p.flAgressividade );
		ReadFloat( pLevel, "uso_tatico", &p.flUsoTatico );
	}

	pFile->deleteThis();
	DevMsg( "[Trok] perfis de bot carregados de scripts/trok_bot_perfis.txt\n" );
}

CON_COMMAND_F( trok_bot_perfis_recarregar, "Relê scripts/trok_bot_perfis.txt. Vale para bots que nascerem depois.", FCVAR_GAMEDLL )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	TrokBot_ReloadProfiles();
}

// Multiplica por (1 ± v), v sorteado.
static float Jitter( float flValue, float flAmount )
{
	return flValue * RandomFloat( 1.0f - flAmount, 1.0f + flAmount );
}

void TrokBot_BuildProfile( int iDificuldade, TrokBotProfile_t *pOut )
{
	if ( !s_bLoaded )
		TrokBot_ReloadProfiles();

	iDificuldade = clamp( iDificuldade, 0, TROK_BOT_LEVELS - 1 );
	*pOut = s_Profiles[ iDificuldade ];

	// Personalidade: cada bot erra, reage e se arrisca um pouco diferente.
	float v = trok_bot_personalidade.GetFloat();
	TrokBotProfile_t &p = *pOut;

	p.flReacaoMin			= Jitter( p.flReacaoMin, v );
	p.flReacaoMax			= MAX( p.flReacaoMin, Jitter( p.flReacaoMax, v ) );
	p.flGanhoPercepcao		= Jitter( p.flGanhoPercepcao, v );
	p.flAudicao				= Jitter( p.flAudicao, v );
	p.flMiraErroInicial		= Jitter( p.flMiraErroInicial, v );
	p.flMiraErroMinimo		= Jitter( p.flMiraErroMinimo, v );
	p.flMiraMeiaVida		= Jitter( p.flMiraMeiaVida, v );
	p.flMiraRastreio		= Jitter( p.flMiraRastreio, v );
	p.flChanceCabeca		= clamp( Jitter( p.flChanceCabeca, v * 2.0f ), 0.0f, 1.0f );
	p.flGiroMax				= Jitter( p.flGiroMax, v );
	p.flAgressividade		= clamp( p.flAgressividade + RandomFloat( -2.0f * v, 2.0f * v ) * 1.5f, 0.0f, 1.0f );
	p.flUsoTatico			= clamp( Jitter( p.flUsoTatico, v ), 0.0f, 1.0f );

	// Estilo: muda decisões (o que fazer), não a habilidade (mira, reação).
	p.iEstilo = PickStyle();
	p.flRotaVariedade = 1.0f;
	p.flRotaColegas = 1.0f;
	p.flTempoSegurar = 1.0f;
	p.bEmboscada = false;

	switch ( p.iEstilo )
	{
	case TROK_ESTILO_ENTRY:
		p.flAgressividade = RandomFloat( 0.85f, 1.0f );
		p.flRotaVariedade = 0.6f;		// vai pelo caminho direto
		p.flRotaColegas = 0.5f;
		p.flTempoSegurar = 0.5f;
		break;

	case TROK_ESTILO_LURKER:
		p.flAgressividade = RandomFloat( 0.4f, 0.6f );
		p.flRotaVariedade = 1.6f;
		p.flRotaColegas = 3.0f;			// foge da rota do time
		p.flTempoSegurar = 0.8f;
		break;

	case TROK_ESTILO_ANCORA:
		p.flAgressividade = RandomFloat( 0.0f, 0.2f );
		p.flUsoTatico = MAX( p.flUsoTatico, 0.7f );
		p.flTempoSegurar = 2.2f;
		p.bEmboscada = true;
		break;

	case TROK_ESTILO_SUPORTE:
		p.flAgressividade = RandomFloat( 0.4f, 0.6f );
		p.flRotaColegas = 0.2f;			// anda junto
		break;

	default:
		break;
	}
}

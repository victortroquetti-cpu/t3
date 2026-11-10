//=============================================================================
// Trok: leitura dos números de arma do Combat Arms (ver trok_arma_ca.h).
//=============================================================================

#include "cbase.h"
#include "KeyValues.h"
#include "trok_arma_ca.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Padrões do CA para campos que o registro não traz (attribute_data.md).
void TrokArmaCA_Padrao( TrokArmaCA_t &ca )
{
	V_memset( &ca, 0, sizeof( ca ) );
	ca.bAtivo = false;
	ca.flDano = 30.0f;
	ca.flDanoVariacao = 0.1f;
	ca.flAlcance = 3000.0f;
	ca.flFaixa[ 0 ] = 1500.0f; ca.flFaixa[ 1 ] = 2500.0f; ca.flFaixa[ 2 ] = 3000.0f;
	ca.flFaixaDano[ 0 ] = 1.0f; ca.flFaixaDano[ 1 ] = 0.7f; ca.flFaixaDano[ 2 ] = 0.35f;
	ca.iConeMin = 3; ca.iConeMax = 100;
	ca.flConeSobe = 1.0f; ca.flConeDesce = 0.3f;
	ca.flConeFator = 1.0f;
	ca.flTeto[ 0 ] = ca.flTeto[ 1 ] = ca.flTeto[ 2 ] = ca.flTeto[ 3 ] = 0.3f;
	ca.flAlvoAndar = 0.05f; ca.flAlvoCorrer = 0.25f; ca.flAlvoPulo = 0.7f;
	ca.flEsperaRecuperar = 0.150f;
	ca.flRecuo = 1.0f;
	ca.flRecuoAcel = 0.0f;
	ca.flRecuoMax = 5.0f;
	ca.flRecuoLadoInicio = 0.032f;
	ca.flRecuoLadoDir = -1.0f; ca.flRecuoLadoEsq = 1.0f;
	ca.flRecuoLadoMax = 10.0f;
	ca.flRecuoLadoTroca = 0.015f;
	ca.flRecuoVoltaEspera = 0.600f;
	ca.flRecuoVoltaLenta = 0.1f; ca.flRecuoVoltaRapida = 0.3f;
	ca.flSaidaCorrida = -1.0f;
	ca.flFacaAlcance = 105.0f;
	ca.flFacaGolpe[ 0 ] = 0.65f; ca.flFacaGolpe[ 1 ] = 45.0f; ca.flFacaGolpe[ 2 ] = 0.155f; ca.flFacaGolpe[ 3 ] = 0.402f;
	ca.flFacaEstocada[ 0 ] = 1.1f; ca.flFacaEstocada[ 1 ] = 75.0f; ca.flFacaEstocada[ 2 ] = 0.367f;
}

// "a b c" -> até n floats (os que faltarem ficam como estão).
static void LerVarios( KeyValues *pKV, const char *pszChave, float *pOut, int n )
{
	const char *psz = pKV->GetString( pszChave, NULL );
	if ( !psz || !psz[ 0 ] )
		return;
	for ( int i = 0; i < n && *psz; ++i )
	{
		while ( *psz == ' ' || *psz == '\t' )
			++psz;
		if ( !*psz )
			break;
		char *pEnd = NULL;
		const float f = strtof( psz, &pEnd );
		if ( pEnd == psz )
			break;
		pOut[ i ] = f;
		psz = pEnd;
	}
}

static void LerPar( KeyValues *pKV, const char *pszChave, int &a, int &b )
{
	float f[ 2 ] = { (float)a, (float)b };
	LerVarios( pKV, pszChave, f, 2 );
	a = (int)f[ 0 ];
	b = (int)f[ 1 ];
}

static void LerPar( KeyValues *pKV, const char *pszChave, float &a, float &b )
{
	float f[ 2 ] = { a, b };
	LerVarios( pKV, pszChave, f, 2 );
	a = f[ 0 ];
	b = f[ 1 ];
}

void TrokArmaCA_Ler( KeyValues *pTrok, TrokArmaCA_t &ca )
{
	TrokArmaCA_Padrao( ca );
	if ( !pTrok )
		return;

	KeyValues *pCA = pTrok->FindKey( "CA" );
	ca.bAtivo = pCA && pTrok->GetInt( "modelo_ca", 0 ) != 0;
	if ( !pCA )
		return;

	ca.flDano = pCA->GetFloat( "dano", ca.flDano );
	ca.flDanoVariacao = pCA->GetFloat( "dano_variacao", ca.flDanoVariacao );
	if ( ca.flDanoVariacao < 0.0f || ca.flDanoVariacao > 1.0f )
		ca.flDanoVariacao = 0.2f;	// como o CA faz com valor fora de [0, 1]
	ca.flAlcance = pCA->GetFloat( "alcance", ca.flAlcance );
	LerVarios( pCA, "faixas", ca.flFaixa, 3 );
	LerVarios( pCA, "faixas_dano", ca.flFaixaDano, 3 );

	LerPar( pCA, "cone", ca.iConeMin, ca.iConeMax );
	LerPar( pCA, "cone_mira", ca.iConeMiraMin, ca.iConeMiraMax );
	LerPar( pCA, "cone_velocidade", ca.flConeSobe, ca.flConeDesce );
	LerPar( pCA, "cone_velocidade_mira", ca.flConeMiraSobe, ca.flConeMiraDesce );
	ca.flConeFator = pCA->GetFloat( "cone_fator", ca.flConeFator );
	LerVarios( pCA, "teto_tiro", ca.flTeto, 4 );
	float alvo[ 3 ] = { ca.flAlvoAndar, ca.flAlvoCorrer, ca.flAlvoPulo };
	LerVarios( pCA, "alvo_movimento", alvo, 3 );
	ca.flAlvoAndar = alvo[ 0 ]; ca.flAlvoCorrer = alvo[ 1 ]; ca.flAlvoPulo = alvo[ 2 ];
	ca.flEsperaRecuperar = pCA->GetFloat( "espera_recuperar", ca.flEsperaRecuperar );

	ca.flRecuo = pCA->GetFloat( "recuo", ca.flRecuo );
	ca.flRecuoAcel = pCA->GetFloat( "recuo_acel", ca.flRecuoAcel );
	ca.flRecuoMax = pCA->GetFloat( "recuo_max", ca.flRecuoMax );
	ca.flRecuoLadoInicio = pCA->GetFloat( "recuo_lado_inicio", ca.flRecuoLadoInicio );
	LerPar( pCA, "recuo_lado", ca.flRecuoLadoDir, ca.flRecuoLadoEsq );
	ca.flRecuoLadoMax = pCA->GetFloat( "recuo_lado_max", ca.flRecuoLadoMax );
	ca.flRecuoLadoTroca = pCA->GetFloat( "recuo_lado_troca", ca.flRecuoLadoTroca );
	ca.flRecuoVoltaEspera = pCA->GetFloat( "recuo_volta_espera", ca.flRecuoVoltaEspera );
	LerPar( pCA, "recuo_volta", ca.flRecuoVoltaLenta, ca.flRecuoVoltaRapida );

	ca.flTempoRecarga = pCA->GetFloat( "tempo_recarga", ca.flTempoRecarga );
	ca.flTempoSacar = pCA->GetFloat( "tempo_sacar", ca.flTempoSacar );
	LerVarios( pCA, "recarga_por_bala", ca.flRecargaBala, 3 );
	ca.flSaidaCorrida = pCA->GetFloat( "saida_corrida", ca.flSaidaCorrida );

	ca.flFacaAlcance = pCA->GetFloat( "faca_alcance", ca.flFacaAlcance );
	LerVarios( pCA, "faca_golpe", ca.flFacaGolpe, 4 );
	LerVarios( pCA, "faca_estocada", ca.flFacaEstocada, 3 );
}

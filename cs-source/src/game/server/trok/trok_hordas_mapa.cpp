//=============================================================================
// Trok: entidades de mapa do modo Hordas (as estações ficam em shared/trok/trok_hordas_estacao.cpp).
//
// info_trok_hordas_spawn   (ponto) nascedouro de zumbi. "area": só vale depois que uma porta
//                          paga abre essa área (0 e 1 = desde o começo). "chefe" 1: lugar do
//                          chefe. Entradas Ligar/Desligar.
// trok_hordas_logica       (ponto) saídas para o mapa: OnRodadaInicio, OnRodadaFim (número da
//                          rodada), OnEnergia, OnChefe, OnFimDeJogo, OnArea. Entradas:
//                          AbrirArea <n>, LigarEnergia.
// As barricadas do Hordas são as func_trok_grade do Surto (trok_surto_mapa.cpp), que no Hordas
// começam fechadas, os zumbis quebram tábua a tábua e os humanos consertam com E.
//=============================================================================

#include "cbase.h"
#include "trok_hordas_mapa.h"
#include "trok/trok_hordas.h"
#include "entityoutput.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// info_trok_hordas_spawn
//-----------------------------------------------------------------------------
class CTrokHordasSpawn : public CPointEntity
{
public:
	DECLARE_CLASS( CTrokHordasSpawn, CPointEntity );
	DECLARE_DATADESC();

	CTrokHordasSpawn() { m_iArea = 0; m_bDesligado = false; m_bChefe = false; }

	bool Ativo( void ) const { return !m_bDesligado && TrokHordas_AreaAberta( m_iArea ); }
	bool IsChefe( void ) const { return m_bChefe; }

	void InputLigar( inputdata_t &inputdata ) { m_bDesligado = false; }
	void InputDesligar( inputdata_t &inputdata ) { m_bDesligado = true; }

private:
	int m_iArea;
	bool m_bDesligado;
	bool m_bChefe;
};

LINK_ENTITY_TO_CLASS( info_trok_hordas_spawn, CTrokHordasSpawn );

BEGIN_DATADESC( CTrokHordasSpawn )
	DEFINE_KEYFIELD( m_iArea, FIELD_INTEGER, "area" ),
	DEFINE_KEYFIELD( m_bDesligado, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_KEYFIELD( m_bChefe, FIELD_BOOLEAN, "chefe" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Ligar", InputLigar ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Desligar", InputDesligar ),
END_DATADESC()

void TrokHordas_Spawns( CUtlVector< CBaseEntity * > *pOut, bool bChefe )
{
	// Primeira volta: só os do tipo pedido (chefe ou normal). Segunda, se não achou nada: todos
	// os ativos (chefe sem lugar próprio, ou mapa só com lugares de chefe).
	pOut->RemoveAll();
	for ( int k = 0; k < 2 && pOut->Count() == 0; ++k )
	{
		CBaseEntity *pEnt = NULL;
		while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, "info_trok_hordas_spawn" ) ) != NULL )
		{
			CTrokHordasSpawn *p = static_cast< CTrokHordasSpawn * >( pEnt );
			if ( p->Ativo() && ( k == 1 || p->IsChefe() == bChefe ) )
				pOut->AddToTail( p );
		}
	}
}

//-----------------------------------------------------------------------------
// trok_hordas_logica
//-----------------------------------------------------------------------------
class CTrokHordasLogica : public CLogicalEntity
{
public:
	DECLARE_CLASS( CTrokHordasLogica, CLogicalEntity );
	DECLARE_DATADESC();

	void Disparar( int iEvento, int iValor )
	{
		switch ( iEvento )
		{
		case HORDAS_EV_RODADA_INICIO: m_OnRodadaInicio.Set( iValor, this, this ); break;
		case HORDAS_EV_RODADA_FIM: m_OnRodadaFim.Set( iValor, this, this ); break;
		case HORDAS_EV_ENERGIA: m_OnEnergia.FireOutput( this, this ); break;
		case HORDAS_EV_CHEFE: m_OnChefe.Set( iValor, this, this ); break;
		case HORDAS_EV_FIM_DE_JOGO: m_OnFimDeJogo.Set( iValor, this, this ); break;
		case HORDAS_EV_AREA: m_OnArea.Set( iValor, this, this ); break;
		}
	}

	void InputAbrirArea( inputdata_t &inputdata ) { TrokHordas_AbrirArea( inputdata.value.Int() ); }
	void InputLigarEnergia( inputdata_t &inputdata ) { TrokHordas_LigarEnergia(); }

private:
	COutputInt m_OnRodadaInicio;
	COutputInt m_OnRodadaFim;
	COutputEvent m_OnEnergia;
	COutputInt m_OnChefe;
	COutputInt m_OnFimDeJogo;
	COutputInt m_OnArea;
};

LINK_ENTITY_TO_CLASS( trok_hordas_logica, CTrokHordasLogica );

BEGIN_DATADESC( CTrokHordasLogica )
	DEFINE_INPUTFUNC( FIELD_INTEGER, "AbrirArea", InputAbrirArea ),
	DEFINE_INPUTFUNC( FIELD_VOID, "LigarEnergia", InputLigarEnergia ),
	DEFINE_OUTPUT( m_OnRodadaInicio, "OnRodadaInicio" ),
	DEFINE_OUTPUT( m_OnRodadaFim, "OnRodadaFim" ),
	DEFINE_OUTPUT( m_OnEnergia, "OnEnergia" ),
	DEFINE_OUTPUT( m_OnChefe, "OnChefe" ),
	DEFINE_OUTPUT( m_OnFimDeJogo, "OnFimDeJogo" ),
	DEFINE_OUTPUT( m_OnArea, "OnArea" ),
END_DATADESC()

void TrokHordas_DispararMapa( int iEvento, int iValor )
{
	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, "trok_hordas_logica" ) ) != NULL )
		static_cast< CTrokHordasLogica * >( pEnt )->Disparar( iEvento, iValor );
}

//-----------------------------------------------------------------------------
// trigger_trok_armadilha: zona (pincel invisível, não sólido) de uma func_trok_armadilha, que a
// encontra pelo nome (keyvalue "zona"). Só marca o volume; o dano é do painel.
//-----------------------------------------------------------------------------
class CTrokArmadilhaZona : public CBaseEntity
{
public:
	DECLARE_CLASS( CTrokArmadilhaZona, CBaseEntity );

	virtual void Spawn( void )
	{
		SetMoveType( MOVETYPE_NONE );
		SetModel( STRING( GetModelName() ) );
		SetSolid( SOLID_BSP );
		AddSolidFlags( FSOLID_NOT_SOLID );
		AddEffects( EF_NODRAW );
	}
};

LINK_ENTITY_TO_CLASS( trigger_trok_armadilha, CTrokArmadilhaZona );

//=============================================================================
// Trok: entidades de mapa do modo Surto.
//
// trok_surto_logica  (ponto) saídas da rodada para o mapa: OnPreparo, OnInicio, OnAlarme,
//                    OnAbrigos, OnHumanosVencem, OnZumbisVencem. O mapa liga isso a portas,
//                    luzes, sons (ex.: abrir os abrigos no OnAbrigos).
// func_trok_grade    (pincel) grade de abrigo: humano aperta USAR para fechar/abrir; fechada
//                    é sólida e os zumbis a quebram batendo (vida). Por padrão só pode ser
//                    usada depois que os abrigos abrem. Entradas: Liberar, Fechar, Abrir.
// info_trok_abrigo   (ponto) centro de um abrigo, para os bots humanos irem para lá.
// O CleanUpMap do HL2MP recria tudo a cada rodada (grades voltam inteiras).
//=============================================================================

#include "cbase.h"
#include "trok_surto_mapa.h"
#include "trok/trok_surto.h"
#include "entityoutput.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// trok_surto_logica
//-----------------------------------------------------------------------------
class CTrokSurtoLogica : public CLogicalEntity
{
public:
	DECLARE_CLASS( CTrokSurtoLogica, CLogicalEntity );
	DECLARE_DATADESC();

	void Disparar( int iEvento )
	{
		switch ( iEvento )
		{
		case SURTO_EV_PREPARO: m_OnPreparo.FireOutput( this, this ); break;
		case SURTO_EV_INICIO: m_OnInicio.FireOutput( this, this ); break;
		case SURTO_EV_ALARME: m_OnAlarme.FireOutput( this, this ); break;
		case SURTO_EV_ABRIGOS: m_OnAbrigos.FireOutput( this, this ); break;
		case SURTO_EV_HUMANOS_VENCEM: m_OnHumanosVencem.FireOutput( this, this ); break;
		case SURTO_EV_ZUMBIS_VENCEM: m_OnZumbisVencem.FireOutput( this, this ); break;
		}
	}

private:
	COutputEvent m_OnPreparo;
	COutputEvent m_OnInicio;
	COutputEvent m_OnAlarme;
	COutputEvent m_OnAbrigos;
	COutputEvent m_OnHumanosVencem;
	COutputEvent m_OnZumbisVencem;
};

LINK_ENTITY_TO_CLASS( trok_surto_logica, CTrokSurtoLogica );

BEGIN_DATADESC( CTrokSurtoLogica )
	DEFINE_OUTPUT( m_OnPreparo, "OnPreparo" ),
	DEFINE_OUTPUT( m_OnInicio, "OnInicio" ),
	DEFINE_OUTPUT( m_OnAlarme, "OnAlarme" ),
	DEFINE_OUTPUT( m_OnAbrigos, "OnAbrigos" ),
	DEFINE_OUTPUT( m_OnHumanosVencem, "OnHumanosVencem" ),
	DEFINE_OUTPUT( m_OnZumbisVencem, "OnZumbisVencem" ),
END_DATADESC()

//-----------------------------------------------------------------------------
// info_trok_abrigo
//-----------------------------------------------------------------------------
class CTrokAbrigo : public CPointEntity
{
public:
	DECLARE_CLASS( CTrokAbrigo, CPointEntity );
	DECLARE_DATADESC();
	CTrokAbrigo() { m_flRaio = 300.0f; }
	float m_flRaio;
};

LINK_ENTITY_TO_CLASS( info_trok_abrigo, CTrokAbrigo );

BEGIN_DATADESC( CTrokAbrigo )
	DEFINE_KEYFIELD( m_flRaio, FIELD_FLOAT, "raio" ),
END_DATADESC()

//-----------------------------------------------------------------------------
// func_trok_grade
//-----------------------------------------------------------------------------
class CTrokGrade : public CBaseEntity
{
public:
	DECLARE_CLASS( CTrokGrade, CBaseEntity );
	DECLARE_DATADESC();

	CTrokGrade()
	{
		m_iVidaMax = 1500;
		m_bInicioFechada = false;
		m_bLiberarNosAbrigos = true;
		m_bFechada = false;
		m_bQuebrada = false;
		m_bLiberada = false;
		m_flProxUso = 0.0f;
	}

	virtual void Precache( void )
	{
		PrecacheScriptSound( "Trok.Grade.Fecha" );
		PrecacheScriptSound( "Trok.Grade.Abre" );
		PrecacheScriptSound( "Trok.Grade.Golpe" );
		PrecacheScriptSound( "Trok.Grade.Quebra" );
	}

	virtual void Spawn( void )
	{
		Precache();
		SetMoveType( MOVETYPE_PUSH );
		SetModel( STRING( GetModelName() ) );
		SetSolid( SOLID_BSP );
		m_takedamage = DAMAGE_YES;
		m_iVida = MAX( 1, m_iVidaMax );
		m_bFechada = m_bInicioFechada;
		m_bQuebrada = false;
		m_bLiberada = !m_bLiberarNosAbrigos;
		SetRenderMode( kRenderTransAlpha );
		Aplicar();
	}

	virtual int ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE; }

	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pActivator );
		if ( !pPlayer || TrokSurto_IsZumbi( pPlayer ) || !m_bLiberada || m_bQuebrada || gpGlobals->curtime < m_flProxUso )
			return;
		m_flProxUso = gpGlobals->curtime + 0.8f;
		SetFechada( !m_bFechada );
	}

	virtual int OnTakeDamage( const CTakeDamageInfo &info )
	{
		// Só zumbi estraga, e só a grade fechada.
		if ( !m_bFechada || m_bQuebrada || !TrokSurto_IsZumbi( info.GetAttacker() ) )
			return 0;
		m_iVida -= MAX( 1, (int)info.GetDamage() );
		EmitSound( "Trok.Grade.Golpe" );
		m_OnGolpe.FireOutput( info.GetAttacker(), this );
		if ( m_iVida <= 0 )
		{
			m_bQuebrada = true;
			m_bFechada = false;
			EmitSound( "Trok.Grade.Quebra" );
			Aplicar();
			m_OnQuebrada.FireOutput( info.GetAttacker(), this );
		}
		return 1;
	}

	void SetFechada( bool bFechada )
	{
		if ( m_bQuebrada || bFechada == m_bFechada )
			return;
		m_bFechada = bFechada;
		EmitSound( bFechada ? "Trok.Grade.Fecha" : "Trok.Grade.Abre" );
		Aplicar();
		if ( bFechada )
			m_OnFechada.FireOutput( this, this );
		else
			m_OnAberta.FireOutput( this, this );
	}

	// Fechada: sólida e visível. Aberta: atravessa e fica bem clara (dá para ver onde fecha).
	// Quebrada: some.
	void Aplicar( void )
	{
		if ( m_bQuebrada )
		{
			AddSolidFlags( FSOLID_NOT_SOLID );
			AddEffects( EF_NODRAW );
			return;
		}
		RemoveEffects( EF_NODRAW );
		if ( m_bFechada )
		{
			RemoveSolidFlags( FSOLID_NOT_SOLID );
			SetRenderColorA( 255 );
		}
		else
		{
			AddSolidFlags( FSOLID_NOT_SOLID );
			SetRenderColorA( 50 );
		}
	}

	void InputLiberar( inputdata_t &inputdata ) { m_bLiberada = true; }
	void InputFechar( inputdata_t &inputdata ) { m_bLiberada = true; SetFechada( true ); }
	void InputAbrir( inputdata_t &inputdata ) { SetFechada( false ); }

	bool IsFechada( void ) const { return m_bFechada && !m_bQuebrada; }
	bool IsAbertaUsavel( void ) const { return !m_bFechada && !m_bQuebrada && m_bLiberada; }
	void Liberar( void ) { m_bLiberada = true; }
	bool LiberaNosAbrigos( void ) const { return m_bLiberarNosAbrigos; }

private:
	int m_iVidaMax;
	int m_iVida;
	bool m_bInicioFechada;
	bool m_bLiberarNosAbrigos;
	bool m_bFechada;
	bool m_bQuebrada;
	bool m_bLiberada;
	float m_flProxUso;

	COutputEvent m_OnFechada;
	COutputEvent m_OnAberta;
	COutputEvent m_OnGolpe;
	COutputEvent m_OnQuebrada;
};

LINK_ENTITY_TO_CLASS( func_trok_grade, CTrokGrade );

BEGIN_DATADESC( CTrokGrade )
	DEFINE_KEYFIELD( m_iVidaMax, FIELD_INTEGER, "vida" ),
	DEFINE_KEYFIELD( m_bInicioFechada, FIELD_BOOLEAN, "inicio_fechada" ),
	DEFINE_KEYFIELD( m_bLiberarNosAbrigos, FIELD_BOOLEAN, "liberar_nos_abrigos" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Liberar", InputLiberar ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Fechar", InputFechar ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Abrir", InputAbrir ),
	DEFINE_OUTPUT( m_OnFechada, "OnFechada" ),
	DEFINE_OUTPUT( m_OnAberta, "OnAberta" ),
	DEFINE_OUTPUT( m_OnGolpe, "OnGolpe" ),
	DEFINE_OUTPUT( m_OnQuebrada, "OnQuebrada" ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Ligação com a rodada e com os bots
//-----------------------------------------------------------------------------
void TrokSurto_DispararMapa( int iEvento )
{
	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, "trok_surto_logica" ) ) != NULL )
		static_cast< CTrokSurtoLogica * >( pEnt )->Disparar( iEvento );

	// Abrigos abertos: as grades marcadas passam a poder ser fechadas.
	if ( iEvento == SURTO_EV_ABRIGOS )
	{
		pEnt = NULL;
		while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, "func_trok_grade" ) ) != NULL )
		{
			CTrokGrade *pGrade = static_cast< CTrokGrade * >( pEnt );
			if ( pGrade->LiberaNosAbrigos() )
				pGrade->Liberar();
		}
	}
}

bool TrokSurto_GradeFechada( CBaseEntity *pEnt )
{
	return pEnt && FClassnameIs( pEnt, "func_trok_grade" ) && static_cast< CTrokGrade * >( pEnt )->IsFechada();
}

bool TrokSurto_GradeAberta( CBaseEntity *pEnt )
{
	return pEnt && FClassnameIs( pEnt, "func_trok_grade" ) && static_cast< CTrokGrade * >( pEnt )->IsAbertaUsavel();
}

CBaseEntity *TrokSurto_GradeMaisPerto( const Vector &pos, float flRaio, bool bFechada )
{
	CBaseEntity *pBest = NULL;
	float flBest = flRaio * flRaio;
	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, "func_trok_grade" ) ) != NULL )
	{
		if ( bFechada ? !TrokSurto_GradeFechada( pEnt ) : !TrokSurto_GradeAberta( pEnt ) )
			continue;
		const float d = ( pEnt->WorldSpaceCenter() - pos ).LengthSqr();
		if ( d < flBest )
		{
			flBest = d;
			pBest = pEnt;
		}
	}
	return pBest;
}

void TrokSurto_Abrigos( CUtlVector< CBaseEntity * > *pOut )
{
	pOut->RemoveAll();
	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, "info_trok_abrigo" ) ) != NULL )
		pOut->AddToTail( pEnt );
}

float TrokSurto_AbrigoRaio( CBaseEntity *pAbrigo )
{
	CTrokAbrigo *p = dynamic_cast< CTrokAbrigo * >( pAbrigo );
	return p ? MAX( 64.0f, p->m_flRaio ) : 300.0f;
}

//=============================================================================
// Trok: entidades de mapa do modo Surto (ver trok_surto_mapa.cpp e game/mod_trok/trok.fgd).
//=============================================================================
#ifndef TROK_SURTO_MAPA_H
#define TROK_SURTO_MAPA_H
#ifdef _WIN32
#pragma once
#endif

// Eventos da rodada que viram saídas do trok_surto_logica.
enum TrokSurtoEvento_t
{
	SURTO_EV_PREPARO = 0,
	SURTO_EV_INICIO,
	SURTO_EV_ALARME,
	SURTO_EV_ABRIGOS,
	SURTO_EV_HUMANOS_VENCEM,
	SURTO_EV_ZUMBIS_VENCEM,
};

void TrokSurto_DispararMapa( int iEvento );

// Para os bots
bool TrokSurto_GradeFechada( CBaseEntity *pEnt );		// é uma grade fechada (inteira)
bool TrokSurto_GradeAberta( CBaseEntity *pEnt );		// é uma grade aberta, liberada, que dá para fechar
CBaseEntity *TrokSurto_GradeMaisPerto( const Vector &pos, float flRaio, bool bFechada );
void TrokSurto_Abrigos( CUtlVector< CBaseEntity * > *pOut );	// marcadores info_trok_abrigo
float TrokSurto_AbrigoRaio( CBaseEntity *pAbrigo );

#endif // TROK_SURTO_MAPA_H

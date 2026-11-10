//=============================================================================
// Trok: entidades de mapa do modo Hordas que não são estações (ver trok_hordas_mapa.cpp,
// shared/trok/trok_hordas_estacao.cpp e game/mod_trok/trok.fgd).
//=============================================================================
#ifndef TROK_HORDAS_MAPA_H
#define TROK_HORDAS_MAPA_H
#ifdef _WIN32
#pragma once
#endif

// Eventos que viram saídas do trok_hordas_logica.
enum TrokHordasEvento_t
{
	HORDAS_EV_RODADA_INICIO = 0,	// valor = número da rodada
	HORDAS_EV_RODADA_FIM,			// valor = número da rodada
	HORDAS_EV_ENERGIA,
	HORDAS_EV_CHEFE,				// valor = número da rodada
	HORDAS_EV_FIM_DE_JOGO,			// valor = rodada alcançada
	HORDAS_EV_AREA,					// valor = área aberta
};

void TrokHordas_DispararMapa( int iEvento, int iValor );

// Nascedouros de zumbi ativos (info_trok_hordas_spawn ligados e de área aberta).
// bChefe: só os marcados para o chefe (se não houver, volta os normais).
void TrokHordas_Spawns( CUtlVector< CBaseEntity * > *pOut, bool bChefe );

// Barricadas (func_trok_grade no Hordas; server/trok/trok_surto_mapa.cpp).
void TrokHordas_GradesReparar( void );		// bônus Reforço: todas inteiras
int TrokHordas_NumGrades( void );

#endif // TROK_HORDAS_MAPA_H

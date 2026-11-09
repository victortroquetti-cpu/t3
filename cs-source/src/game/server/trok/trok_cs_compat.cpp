//=============================================================================
// Trok: compatibilidade com mapas do Counter-Strike: Source.
//
// Entidades dos mapas do CS:S que o código do HL2MP não conhece são
// ignoradas ao carregar o mapa. Aqui ficam as que o mod já usa:
//   - spawns de TR e CT (o patch em CHL2MP_Player::EntSelectSpawnPoint
//     usa os dois quando o mapa não tem spawns do HL2MP).
// Zona de compra, bomba e reféns entram quando esses sistemas forem feitos.
//=============================================================================

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( info_player_terrorist, CPointEntity );
LINK_ENTITY_TO_CLASS( info_player_counterterrorist, CPointEntity );

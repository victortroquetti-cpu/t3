//=============================================================================
// Trok: bots melhorados (em cima dos bots NextBot do HL2MP).
//
// O patch (patches/sdk.patch) troca, com #ifdef TROK, as peças do CHL2MPBot:
//   visão      -> CTrokBotVision      (percepção gradual, sem onisciência)
//   corpo      -> CTrokBotBody        (velocidade de giro e rastreio por perfil)
//   locomoção  -> CTrokBotLocomotion  (pulo agachado para alcançar bordas)
//   ação raiz  -> CTrokBotMainAction  (mira humana, strafe, audição)
//   cenário    -> TrokBot_ScenarioAction (usa o mapa tático: segurar lugares bons ou caçar)
//
// Tudo é servidor. Ver docs em cs-source/design/bots.md.
//=============================================================================

#ifndef TROK_BOT_H
#define TROK_BOT_H
#ifdef _WIN32
#pragma once
#endif

#include "trok_bot_profile.h"

class CHL2MPBot;
class CHL2MPBotBody;
class CHL2MPBotLocomotion;
class CHL2MPBotVision;
class INextBot;
template < typename Actor > class Action;

// Fábricas usadas pelo construtor do CHL2MPBot (patch).
CHL2MPBotBody		*TrokBot_CreateBody( INextBot *bot );
CHL2MPBotLocomotion	*TrokBot_CreateLocomotion( INextBot *bot );
CHL2MPBotVision		*TrokBot_CreateVision( INextBot *bot );

// Ação do cenário (patch em CHL2MPBotScenarioMonitor). NULL = comportamento do HL2MP.
Action< CHL2MPBot > *TrokBot_ScenarioAction( CHL2MPBot *me );

// Uma ação de combate (peek) está movendo o bot: o strafe da ação raiz para.
void TrokBot_SetCombatMoveOwned( CHL2MPBot *me, bool bOwned );

// Perfil do bot (nível + personalidade), sempre válido.
const TrokBotProfile_t &TrokBot_GetProfile( const CHL2MPBot *me );

#endif // TROK_BOT_H

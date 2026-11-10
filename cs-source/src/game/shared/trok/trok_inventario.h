//=============================================================================
// Trok: inventário como no Combat Arms (design/chimera/specs/combat_arms/inventory.md).
//
//   Classes = bucket do script + 1: 1 principal, 2 secundária, 3 branca, 4 arremesso
//   (granadas e minas), 5 especial, 6 objetivo. Teclas 1-6 e Q: weapon_selection
//   (hud_fastswitch 1) + gancho em hud_weaponselection.cpp (volta para a última da classe).
//   Pegar: sem pegar andando por cima; olhar para a arma no chão a até 100 u e apertar E.
//   Principal e secundária trocam pela da mesma classe (a sua cai no lugar, com a
//   munição); das outras classes, a mesma arma soma munição.
//   Largar: sem tecla. Ao morrer caem a principal e a secundária selecionadas por
//   último, cada uma com o pente e a reserva. Armas no chão somem em 20 s.
//=============================================================================

#ifndef TROK_INVENTARIO_H
#define TROK_INVENTARIO_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
class C_BasePlayer;
class C_BaseCombatWeapon;
// Arma no chão que o jogador está olhando, a até trok_pegar_distancia (NULL = nenhuma).
// Mesma conta no cliente (aviso no HUD) e no servidor (o E).
C_BaseCombatWeapon *TrokInventario_ArmaMirada( C_BasePlayer *pPlayer );
// Última arma usada em cada classe (para a tecla da classe voltar nela, como no CA).
C_BaseCombatWeapon *TrokInventario_UltimaDaClasse( int iSlot );
void TrokInventario_ClienteFrame( void );
#else
class CBasePlayer;
class CBaseCombatWeapon;
CBaseCombatWeapon *TrokInventario_ArmaMirada( CBasePlayer *pPlayer );
bool TrokInventario_BloqueiaEncostar( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );
void TrokInventario_PreThink( CBasePlayer *pPlayer );
bool TrokInventario_BloqueiaLargar( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );
void TrokInventario_AoLargar( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );
void TrokInventario_AoMorrer( CBasePlayer *pPlayer );
#endif

#endif // TROK_INVENTARIO_H

# Entidades de mapa e regras de jogo

## Entidade de mapa (servidor)

```cpp
class CTrokPonto : public CPointEntity
{
public:
	DECLARE_CLASS( CTrokPonto, CPointEntity );
	DECLARE_DATADESC();

	void InputAtivar( inputdata_t &inputdata );

private:
	int m_iTime;
	bool m_bAtivo;
	COutputEvent m_OnAtivado;
};

LINK_ENTITY_TO_CLASS( trok_ponto, CTrokPonto );   // nome usado no Hammer e no CreateEntityByName

BEGIN_DATADESC( CTrokPonto )
	DEFINE_KEYFIELD( m_iTime, FIELD_INTEGER, "time" ),        // keyvalue do mapa
	DEFINE_FIELD( m_bAtivo, FIELD_BOOLEAN ),                  // estado interno (save/restore)
	DEFINE_INPUTFUNC( FIELD_VOID, "Ativar", InputAtivar ),     // input do Hammer
	DEFINE_OUTPUT( m_OnAtivado, "OnAtivado" ),                 // output do Hammer
END_DATADESC()

void CTrokPonto::InputAtivar( inputdata_t &inputdata )
{
	m_bAtivo = true;
	m_OnAtivado.FireOutput( inputdata.pActivator, this );
}
```

- Exemplo real com keyfields: `server/hl2mp/bot/map_entities/hl2mp_bot_generator.cpp`.
- Precisa de `Precache()` para modelos e sons (`PrecacheModel`, `PrecacheScriptSound`) e de
  `Spawn()` chamando `Precache()`.
- Think: `SetThink( &CTrokPonto::MeuThink ); SetNextThink( gpGlobals->curtime + 0.1f );`
  (e `DEFINE_THINKFUNC( MeuThink )` no datadesc).
- Para o Hammer ver a entidade: um `trok.fgd` em `game/mod_trok/` com `@include "hl2mp.fgd"` e a
  classe (`@PointClass base(Targetname) = trok_ponto : "..." [ time(integer) : "Time" : 2 ]`).
  Os FGDs base (`base.fgd`, `halflife2.fgd`, `hl2mp.fgd`) vêm com o Source SDK Base 2013 MP (pasta `bin`).
- Entidades de mapas do CS:S (`func_buyzone`, `func_bomb_target`, `info_player_counterterrorist`,
  `hostage_entity`...) **não existem** no código do SDK: mapas do CS:S carregam, mas elas são
  ignoradas até o mod criar classes com esses nomes. Spawns: o HL2MP usa
  `info_player_combine` / `info_player_rebel` / `info_player_deathmatch`.

## Regras de jogo (gamerules)

- Hierarquia: `CGameRules` -> `CMultiplayRules` -> `CTeamplayRules` -> `CHL2MPRules` -> `CTrokRules`.
  Os quatro primeiros estão em `shared/` e `server/`.
- Instalação: `InstallGameRules()` em `server/hl2mp/hl2mp_client.cpp` (no patch do Trok, cria
  `CTrokRules` com `#ifdef TROK`). Cliente cria a mesma classe pelo nome
  (`REGISTER_GAMERULES_CLASS`).
- Ganchos mais úteis (virtuais, quase todos só servidor):
  `Think`, `PlayerSpawn` (chamado **dentro** de `CBasePlayer::Spawn`, antes do HL2MP dar as armas
  em `CHL2MP_Player::Spawn` -> `GiveDefaultItems`), `PlayerKilled`, `DeathNotice`,
  `FPlayerCanRespawn`, `FlPlayerSpawnTime`, `GetPlayerSpawnSpot`, `FPlayerCanTakeDamage`,
  `PlayerRelationship` (aliado/inimigo para dano e bots), `CanHavePlayerItem`,
  `ClientCommand`, `ClientDisconnected`, `CreateStandardEntities`, `GetGameDescription`.
- `CHL2MPRules::CleanUpMap()` recria as entidades do mapa (bom para reiniciar rodada).
- Times: `GetGlobalTeam( TEAM_X )` (`server/team.h`), `TEAM_COMBINE = 2`, `TEAM_REBELS = 3`
  (`hl2mp_gamerules.h`). Times só valem com `mp_teamplay 1` (lido na troca de mapa).
- Troca de time: `CHL2MP_Player::ChangeTeam( t )` **mata** o jogador se ele já estava num time.
  Para trocar sem morte: `pPlayer->CBasePlayer::ChangeTeam( t, false, true )` e depois
  `SetPlayerTeamModel()`.
- Renascer na hora: `pPlayer->ForceRespawn()`. Tempo de morte: `GetDeathTime()`.
- Rodadas prontas e testadas: `shared/teamplayroundbased_gamerules.*` (máquina de estados
  `GR_STATE_PREROUND`, `GR_STATE_RND_RUNNING`, `GR_STATE_TEAM_WIN`..., `SetWinningTeam`, timers
  `team_round_timer`). É a base do TF2; o HL2MP não usa. Adotar exige mudar a herança das regras,
  que é decisão grande: discuta antes.

## Mensagens rápidas

- Para todos: `UTIL_ClientPrintAll( HUD_PRINTCENTER, "texto" )` (ou `HUD_PRINTTALK`, `HUD_PRINTCONSOLE`).
- Para um: `ClientPrint( pPlayer, HUD_PRINTCENTER, "texto" )`.
- Para HUD de verdade, use user message ou variável de rede (ver `rede-e-predicao.md`).

//=============================================================================
// Trok: regras de jogo do mod. Ver trok_gamerules.h e design/modo-surto.md.
//=============================================================================

#include "cbase.h"
#include "trok_gamerules.h"

#ifndef CLIENT_DLL
	#include "hl2mp_player.h"
	#include "team.h"
	#include "weapon_hl2mpbase.h"
	#include "hl2mp_weapon_parse.h"
	#include "trok/weapon_trok_gun.h"
	#include "NextBot/NextBotInterface.h"
	#include "NextBot/NextBotIntentionInterface.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// O servidor cria as regras por nome em InstallGameRules() (ver
// patches/sdk.patch, hl2mp_client.cpp) e o cliente cria a classe com o
// mesmo nome quando a string chega.
REGISTER_GAMERULES_CLASS( CTrokRules );

#ifndef CLIENT_DLL
ConVar trok_vida( "trok_vida", "100", FCVAR_NOTIFY, "Vida (e vida máxima) ao nascer." );
ConVar trok_colete( "trok_colete", "0", FCVAR_NOTIFY, "Colete (armadura do HL2) ao nascer." );

// --- Modo Surto (números de partida: medir na quarentena do Combat Arms e ajustar em cfg/surto.cfg)
ConVar trok_surto_preparo( "trok_surto_preparo", "20", FCVAR_NOTIFY, "Segundos de preparo: todos humanos, antes de sortear os zumbis." );
ConVar trok_surto_tempo( "trok_surto_tempo", "160", FCVAR_NOTIFY, "Segundos de rodada depois do sorteio (2:40)." );
ConVar trok_surto_alarme( "trok_surto_alarme", "80", FCVAR_NOTIFY, "Faltando estes segundos toca o alarme (1:20)." );
ConVar trok_surto_abrigos( "trok_surto_abrigos", "60", FCVAR_NOTIFY, "Faltando estes segundos os abrigos abrem (1:00)." );
ConVar trok_surto_fim( "trok_surto_fim", "8", FCVAR_NOTIFY, "Segundos mostrando o vencedor antes da próxima rodada." );
ConVar trok_surto_min_jogadores( "trok_surto_min_jogadores", "2", FCVAR_NOTIFY, "Mínimo de jogadores (com bots) para começar uma rodada." );
ConVar trok_surto_jogadores_por_hospedeiro( "trok_surto_jogadores_por_hospedeiro", "8", FCVAR_NOTIFY, "Um zumbi hospedeiro a cada N jogadores." );
ConVar trok_surto_hospedeiros_max( "trok_surto_hospedeiros_max", "3", FCVAR_NOTIFY, "Máximo de hospedeiros sorteados." );
ConVar trok_surto_vida_hospedeiro( "trok_surto_vida_hospedeiro", "2500", FCVAR_NOTIFY, "Vida do zumbi hospedeiro (os sorteados)." );
ConVar trok_surto_vida_zumbi( "trok_surto_vida_zumbi", "1500", FCVAR_NOTIFY, "Vida do zumbi infectado (humano que virou)." );
ConVar trok_surto_virar_tempo( "trok_surto_virar_tempo", "2", FCVAR_NOTIFY, "Segundos entre o humano morrer e levantar como zumbi." );
ConVar trok_surto_virar_no_lugar( "trok_surto_virar_no_lugar", "1", FCVAR_NOTIFY, "O infectado levanta onde morreu (0 = nasce num ponto de nascimento)." );
ConVar trok_surto_zumbi_renasce( "trok_surto_zumbi_renasce", "0", FCVAR_NOTIFY, "Zumbi morto volta (0 = fica morto até a próxima rodada; humanos vencem matando todos)." );
ConVar trok_surto_regen( "trok_surto_regen", "0.10", FCVAR_NOTIFY, "Fração da vida máxima que o zumbi parado recupera a cada intervalo." );
ConVar trok_surto_regen_intervalo( "trok_surto_regen_intervalo", "3", FCVAR_NOTIFY, "Segundos entre as recuperações do zumbi parado." );
ConVar trok_surto_regen_espera( "trok_surto_regen_espera", "3", FCVAR_NOTIFY, "Segundos sem levar dano antes de começar a recuperar." );
ConVar trok_surto_zumbi_sem_critico( "trok_surto_zumbi_sem_critico", "1", FCVAR_NOTIFY, "Zumbi não leva o multiplicador de tiro na cabeça." );
ConVar trok_surto_empurrao( "trok_surto_empurrao", "1", FCVAR_NOTIFY, "Multiplica o empurrão e o 'segurar' das armas nos zumbis (0 = desliga)." );
ConVar trok_surto_empurrao_max( "trok_surto_empurrao_max", "450", FCVAR_NOTIFY, "Velocidade horizontal máxima (u/s) que o empurrão pode dar." );
ConVar trok_surto_cor_zumbi( "trok_surto_cor_zumbi", "150 225 140", FCVAR_NOTIFY, "Tom do zumbi enquanto não há modelo próprio (R G B)." );
#endif

CTrokRules::CTrokRules()
{
#ifndef CLIENT_DLL
	m_bSurto = !V_stricmp( trok_modo.GetString(), "surto" );
	m_flSurtoTick = 0.0f;
	for ( int i = 0; i <= MAX_PLAYERS; ++i )
	{
		m_flVirar[ i ] = 0.0f;
		m_vecMorte[ i ] = vec3_invalid;
		m_bHospedeiro[ i ] = false;
		m_bNaRodada[ i ] = false;
		m_flUltimoDano[ i ] = -100.0f;
		m_flProxRegen[ i ] = 0.0f;
	}
	if ( m_bSurto )
	{
		// Surto é sempre em times (humanos x zumbis) e lê os números do cfg do modo.
		m_bTeamPlayEnabled = true;
		engine->ServerCommand( "exec surto.cfg\n" );
	}
#endif
}

CTrokRules::~CTrokRules()
{
}

const char *CTrokRules::GetGameDescription( void )
{
	return "Trok";
}

#ifndef CLIENT_DLL

extern void respawn( CBaseEntity *pEdict, bool fCopyCorpse );
void TrokSurto_DispararMapa( int iEvento );	// server/trok/trok_surto_mapa.cpp

static bool IsPlaying( CBasePlayer *pPlayer )
{
	return pPlayer && pPlayer->IsConnected() && pPlayer->GetTeamNumber() != TEAM_SPECTATOR;
}

//-----------------------------------------------------------------------------
// Nascer
//-----------------------------------------------------------------------------
// Chamado dentro de CBasePlayer::Spawn, depois da vida padrão e antes das armas.
void CTrokRules::PlayerSpawn( CBasePlayer *pPlayer )
{
	BaseClass::PlayerSpawn( pPlayer );

	int iHealth = MAX( 1, trok_vida.GetInt() );
	pPlayer->SetMaxHealth( iHealth );
	pPlayer->SetHealth( iHealth );
	pPlayer->SetArmorValue( MAX( 0, trok_colete.GetInt() ) );

	if ( !m_bSurto || pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		return;

	const int i = pPlayer->entindex();
	CTrokSurtoEstado *pEstado = m_hEstado.Get();
	const int iFase = pEstado ? pEstado->GetFase() : SURTO_DESLIGADO;

	// No preparo todo mundo é humano. Durante a rodada, quem chegou atrasado entra zumbi.
	if ( iFase == SURTO_JOGO && !m_bNaRodada[ i ] && m_flVirar[ i ] == 0.0f )
	{
		m_bNaRodada[ i ] = true;
		SurtoTrocarTime( pPlayer, TEAM_TROK_ZUMBIS );
	}
	else if ( iFase != SURTO_JOGO && pPlayer->GetTeamNumber() != TEAM_TROK_HUMANOS )
	{
		SurtoTrocarTime( pPlayer, TEAM_TROK_HUMANOS );
	}

	if ( pPlayer->GetTeamNumber() == TEAM_TROK_ZUMBIS )
		SurtoAplicarZumbi( pPlayer );
	else
		SurtoAplicarHumano( pPlayer );
}

void CTrokRules::SurtoTrocarTime( CBasePlayer *pPlayer, int iTeam )
{
	if ( pPlayer->GetTeamNumber() == iTeam )
		return;
	// A troca do HL2MP mata o jogador; a do CBasePlayer só muda o time (silenciosa).
	pPlayer->CBasePlayer::ChangeTeam( iTeam, false, true );
	CHL2MP_Player *pHL2MP = dynamic_cast< CHL2MP_Player * >( pPlayer );
	if ( pHL2MP )
		pHL2MP->SetPlayerTeamModel();

	// Bot que muda de lado (hospedeiro sorteado vivo) recomeça o cérebro como zumbi/humano.
	INextBot *pBot = pPlayer->MyNextBotPointer();
	if ( pBot && pBot->GetIntentionInterface() )
		pBot->GetIntentionInterface()->Reset();
}

void CTrokRules::SurtoAplicarHumano( CBasePlayer *pPlayer )
{
	pPlayer->SetRenderColor( 255, 255, 255 );
}

// Zumbi: só garras, vida de zumbi, tom esverdeado (até haver modelo próprio), levanta onde morreu.
void CTrokRules::SurtoAplicarZumbi( CBasePlayer *pPlayer )
{
	const int i = pPlayer->entindex();

	pPlayer->RemoveAllItems( false );
	pPlayer->GiveNamedItem( "weapon_trok_garras" );
	CBaseCombatWeapon *pGarras = pPlayer->Weapon_OwnsThisType( "weapon_trok_garras" );
	if ( pGarras )
		pPlayer->Weapon_Switch( pGarras );

	const int iVida = MAX( 1, m_bHospedeiro[ i ] ? trok_surto_vida_hospedeiro.GetInt() : trok_surto_vida_zumbi.GetInt() );
	pPlayer->SetMaxHealth( iVida );
	pPlayer->SetHealth( iVida );
	pPlayer->SetArmorValue( 0 );

	int r = 150, g = 225, b = 140;
	sscanf( trok_surto_cor_zumbi.GetString(), "%d %d %d", &r, &g, &b );
	pPlayer->SetRenderColor( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ) );

	if ( trok_surto_virar_no_lugar.GetBool() && m_vecMorte[ i ] != vec3_invalid )
	{
		pPlayer->Teleport( &m_vecMorte[ i ], NULL, &vec3_origin );
		m_vecMorte[ i ] = vec3_invalid;
	}

	m_flUltimoDano[ i ] = gpGlobals->curtime;
	m_flProxRegen[ i ] = 0.0f;
}

//-----------------------------------------------------------------------------
// Rodada
//-----------------------------------------------------------------------------
void CTrokRules::Think( void )
{
	BaseClass::Think();

	if ( m_bSurto && !g_fGameOver )
		SurtoThink();
}

void CTrokRules::SurtoContar( int &iHumanos, int &iZumbis )
{
	iHumanos = iZumbis = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !IsPlaying( pPlayer ) )
			continue;
		if ( m_flVirar[ i ] > 0.0f )
			++iZumbis; // morto, vai levantar zumbi
		else if ( pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_TROK_HUMANOS )
			++iHumanos;
		else if ( pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_TROK_ZUMBIS )
			++iZumbis;
	}
}

void CTrokRules::SurtoThink( void )
{
	CTrokSurtoEstado *pEstado = m_hEstado.Get();
	if ( !pEstado )
	{
		pEstado = static_cast< CTrokSurtoEstado * >( CreateEntityByName( "trok_surto_estado" ) );
		if ( !pEstado )
			return;
		DispatchSpawn( pEstado );
		m_hEstado = pEstado;
		SurtoPreparo();
		return;
	}

	const float flNow = gpGlobals->curtime;

	// Quem morreu levanta zumbi na hora marcada.
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		if ( m_flVirar[ i ] <= 0.0f || flNow < m_flVirar[ i ] )
			continue;
		m_flVirar[ i ] = 0.0f;
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( IsPlaying( pPlayer ) && pEstado->GetFase() == SURTO_JOGO )
			SurtoVirarZumbi( pPlayer, false );
	}

	int iHumanos, iZumbis;
	SurtoContar( iHumanos, iZumbis );
	pEstado->m_iHumanos = iHumanos;
	pEstado->m_iZumbis = iZumbis;

	switch ( pEstado->GetFase() )
	{
	case SURTO_PREPARO:
		if ( flNow >= pEstado->GetFimFase() )
		{
			if ( iHumanos >= MAX( 2, trok_surto_min_jogadores.GetInt() ) )
				SurtoComecar();
			else
				pEstado->m_flFimFase = flNow + 5.0f; // esperando gente
		}
		break;

	case SURTO_JOGO:
	{
		const float flLeft = pEstado->GetFimFase() - flNow;
		if ( !pEstado->IsAlarme() && flLeft <= trok_surto_alarme.GetFloat() )
		{
			pEstado->m_bAlarme = true;
			CBroadcastRecipientFilter filter;
			CBaseEntity::EmitSound( filter, 0, "Trok.Surto.Alarme" );
			TrokSurto_DispararMapa( 2 );
		}
		if ( !pEstado->IsAbrigos() && flLeft <= trok_surto_abrigos.GetFloat() )
		{
			pEstado->m_bAbrigos = true;
			TrokSurto_DispararMapa( 3 );
		}

		SurtoRegenerar();

		if ( iHumanos == 0 )
			SurtoFim( SURTO_ZUMBIS );
		else if ( iZumbis == 0 )
			SurtoFim( SURTO_HUMANOS );
		else if ( flLeft <= 0.0f )
			SurtoFim( SURTO_HUMANOS );
		break;
	}

	case SURTO_FIM:
		if ( flNow >= pEstado->GetFimFase() )
			SurtoPreparo();
		break;

	default:
		SurtoPreparo();
		break;
	}
}

// Rodada nova: mapa limpo (grades e portas voltam), todos humanos, preparo.
void CTrokRules::SurtoPreparo( void )
{
	CTrokSurtoEstado *pEstado = m_hEstado.Get();
	if ( !pEstado )
		return;

	// O CleanUpMap recria as entidades do mapa; o estado é nosso e fica (está na lista de
	// classes preservadas do HL2MP? não: por isso ele é recriado se sumir, no SurtoThink).
	CleanUpMap();
	pEstado = m_hEstado.Get();
	if ( !pEstado )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		m_flVirar[ i ] = 0.0f;
		m_vecMorte[ i ] = vec3_invalid;
		m_bHospedeiro[ i ] = false;
		m_bNaRodada[ i ] = false;

		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !IsPlaying( pPlayer ) )
			continue;
		SurtoTrocarTime( pPlayer, TEAM_TROK_HUMANOS );
		if ( pPlayer->GetActiveWeapon() )
			pPlayer->GetActiveWeapon()->Holster();
		pPlayer->RemoveAllItems( true );
		respawn( pPlayer, false );
	}

	pEstado->m_iFase = SURTO_PREPARO;
	pEstado->m_flFimFase = gpGlobals->curtime + MAX( 1.0f, trok_surto_preparo.GetFloat() );
	pEstado->m_iVencedor = SURTO_NINGUEM;
	pEstado->m_bAlarme = false;
	pEstado->m_bAbrigos = false;
	TrokSurto_DispararMapa( 0 );
}

// Sorteio dos hospedeiros (1 a cada N jogadores, até o máximo, sempre sobra humano).
void CTrokRules::SurtoComecar( void )
{
	CTrokSurtoEstado *pEstado = m_hEstado.Get();
	if ( !pEstado )
		return;

	CUtlVector< CBasePlayer * > humanos;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !IsPlaying( pPlayer ) )
			continue;
		m_bNaRodada[ i ] = true;
		if ( pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_TROK_HUMANOS )
			humanos.AddToTail( pPlayer );
	}

	const int iPor = MAX( 1, trok_surto_jogadores_por_hospedeiro.GetInt() );
	int iHosts = ( humanos.Count() + iPor - 1 ) / iPor;
	iHosts = clamp( iHosts, 1, MAX( 1, trok_surto_hospedeiros_max.GetInt() ) );
	iHosts = MIN( iHosts, humanos.Count() - 1 );

	for ( int k = 0; k < iHosts && humanos.Count() > 0; ++k )
	{
		int j = RandomInt( 0, humanos.Count() - 1 );
		CBasePlayer *pHost = humanos[ j ];
		humanos.FastRemove( j );
		m_bHospedeiro[ pHost->entindex() ] = true;
		m_vecMorte[ pHost->entindex() ] = pHost->GetAbsOrigin();
		SurtoVirarZumbi( pHost, true );
	}

	pEstado->m_iFase = SURTO_JOGO;
	pEstado->m_flFimFase = gpGlobals->curtime + MAX( 10.0f, trok_surto_tempo.GetFloat() );
	TrokSurto_DispararMapa( 1 );
}

void CTrokRules::SurtoVirarZumbi( CBasePlayer *pPlayer, bool bHospedeiro )
{
	const int i = pPlayer->entindex();
	if ( bHospedeiro )
		m_bHospedeiro[ i ] = true;
	SurtoTrocarTime( pPlayer, TEAM_TROK_ZUMBIS );

	if ( pPlayer->IsAlive() )
	{
		// Hospedeiro sorteado: vira na hora, no lugar.
		m_vecMorte[ i ] = vec3_invalid;
		SurtoAplicarZumbi( pPlayer );
	}
	else
	{
		if ( pPlayer->GetActiveWeapon() )
			pPlayer->GetActiveWeapon()->Holster();
		pPlayer->RemoveAllItems( true );
		respawn( pPlayer, false ); // Spawn -> PlayerSpawn -> SurtoAplicarZumbi
	}

	pPlayer->EmitSound( "Trok.Surto.Virou" );
}

void CTrokRules::SurtoFim( int iVencedor )
{
	CTrokSurtoEstado *pEstado = m_hEstado.Get();
	if ( !pEstado )
		return;
	pEstado->m_iFase = SURTO_FIM;
	pEstado->m_iVencedor = iVencedor;
	pEstado->m_flFimFase = gpGlobals->curtime + MAX( 2.0f, trok_surto_fim.GetFloat() );

	CTeam *pTeam = GetGlobalTeam( iVencedor == SURTO_HUMANOS ? TEAM_TROK_HUMANOS : TEAM_TROK_ZUMBIS );
	if ( pTeam )
		pTeam->AddScore( 1 );

	TrokSurto_DispararMapa( iVencedor == SURTO_HUMANOS ? 4 : 5 );
}

// Zumbi parado e sem levar dano recupera vida aos poucos.
void CTrokRules::SurtoRegenerar( void )
{
	const float flNow = gpGlobals->curtime;
	const float flFrac = trok_surto_regen.GetFloat();
	if ( flFrac <= 0.0f )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !IsPlaying( pPlayer ) || !pPlayer->IsAlive() || pPlayer->GetTeamNumber() != TEAM_TROK_ZUMBIS )
			continue;

		const bool bParado = pPlayer->GetAbsVelocity().Length2D() < 10.0f;
		if ( !bParado || flNow - m_flUltimoDano[ i ] < trok_surto_regen_espera.GetFloat() )
		{
			m_flProxRegen[ i ] = flNow + trok_surto_regen_intervalo.GetFloat();
			continue;
		}
		if ( flNow < m_flProxRegen[ i ] )
			continue;
		m_flProxRegen[ i ] = flNow + trok_surto_regen_intervalo.GetFloat();
		const int iMax = pPlayer->GetMaxHealth();
		pPlayer->SetHealth( MIN( iMax, pPlayer->GetHealth() + MAX( 1, (int)( iMax * flFrac ) ) ) );
	}
}

//-----------------------------------------------------------------------------
// Morte, dano, itens
//-----------------------------------------------------------------------------
void CTrokRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::PlayerKilled( pVictim, info );

	CTrokSurtoEstado *pEstado = m_hEstado.Get();
	if ( !m_bSurto || !pEstado || pEstado->GetFase() != SURTO_JOGO )
		return;

	const int i = pVictim->entindex();
	if ( pVictim->GetTeamNumber() == TEAM_TROK_HUMANOS )
	{
		// Humano que morre na rodada (garra, queda, o que for) levanta zumbi.
		m_vecMorte[ i ] = pVictim->GetAbsOrigin();
		m_flVirar[ i ] = gpGlobals->curtime + MAX( 0.1f, trok_surto_virar_tempo.GetFloat() );
	}
	else if ( pVictim->GetTeamNumber() == TEAM_TROK_ZUMBIS && trok_surto_zumbi_renasce.GetBool() )
	{
		m_vecMorte[ i ] = vec3_invalid;
		m_flVirar[ i ] = gpGlobals->curtime + 5.0f;
	}
}

bool CTrokRules::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	if ( m_bSurto )
	{
		CTrokSurtoEstado *pEstado = m_hEstado.Get();
		// Na rodada e no fim, quem renasce é o diretor (virar zumbi); no preparo, normal.
		if ( pEstado && ( pEstado->GetFase() == SURTO_JOGO || pEstado->GetFase() == SURTO_FIM ) )
			return false;
	}
	return BaseClass::FPlayerCanRespawn( pPlayer );
}

float CTrokRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	if ( TrokSurto_IsZumbi( pPlayer ) )
		return 0.0f;
	return BaseClass::FlPlayerFallDamage( pPlayer );
}

bool CTrokRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem )
{
	if ( m_bSurto && pItem )
	{
		const bool bGarras = FClassnameIs( pItem, "weapon_trok_garras" );
		if ( pPlayer->GetTeamNumber() == TEAM_TROK_ZUMBIS )
			return bGarras;
		if ( bGarras )
			return false;
	}
	return BaseClass::CanHavePlayerItem( pPlayer, pItem );
}

bool CTrokRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
{
	// No Surto o time é decidido pela rodada.
	if ( m_bSurto && ( FStrEq( args[ 0 ], "jointeam" ) || FStrEq( args[ 0 ], "changeteam" ) ) )
		return true;
	return BaseClass::ClientCommand( pEdict, args );
}

bool CTrokRules::IgnoresHeadshot( CBasePlayer *pVictim ) const
{
	return m_bSurto && trok_surto_zumbi_sem_critico.GetBool() && TrokSurto_IsZumbi( pVictim );
}

// Depois de cada dano em jogador (patch em CBasePlayer::OnTakeDamage_Alive).
// No Surto, bala de humano em zumbi "segura" (tira velocidade) e empurra, conforme a arma.
void CTrokRules::OnPlayerDamaged( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	if ( !m_bSurto || !pVictim )
		return;
	m_flUltimoDano[ pVictim->entindex() ] = gpGlobals->curtime;

	const float flScale = trok_surto_empurrao.GetFloat();
	if ( flScale <= 0.0f || !TrokSurto_IsZumbi( pVictim ) || !( info.GetDamageType() & DMG_BULLET ) )
		return;

	CBasePlayer *pAttacker = ToBasePlayer( info.GetAttacker() );
	if ( !pAttacker || !TrokSurto_IsHumano( pAttacker ) )
		return;
	CWeaponTrokGun *pGun = dynamic_cast< CWeaponTrokGun * >( pAttacker->GetActiveWeapon() );
	if ( !pGun )
		return;
	const CHL2MPSWeaponInfo &wi = pGun->Info();

	Vector vecVel = pVictim->GetAbsVelocity();
	const float flStagger = clamp( wi.m_flTrokStagger * flScale, 0.0f, 0.9f );
	vecVel.x *= 1.0f - flStagger;
	vecVel.y *= 1.0f - flStagger;

	Vector vecDir = pVictim->WorldSpaceCenter() - pAttacker->EyePosition();
	vecDir.z = 0.0f;
	VectorNormalize( vecDir );
	vecVel += vecDir * ( wi.m_flTrokKnockback * flScale );

	const float flMax = trok_surto_empurrao_max.GetFloat();
	const float flSpeed = vecVel.Length2D();
	if ( flMax > 0.0f && flSpeed > flMax )
	{
		vecVel.x *= flMax / flSpeed;
		vecVel.y *= flMax / flSpeed;
	}
	pVictim->SetAbsVelocity( vecVel );
}

// Pontes para o código da Valve (patches).
void TrokRules_OnPlayerDamaged( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	if ( TrokRules() )
		TrokRules()->OnPlayerDamaged( pVictim, info );
}

bool TrokRules_IgnoresHeadshot( CBasePlayer *pVictim )
{
	return TrokRules() && TrokRules()->IgnoresHeadshot( pVictim );
}

#endif // !CLIENT_DLL
